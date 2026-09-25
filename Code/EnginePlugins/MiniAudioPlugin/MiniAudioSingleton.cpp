#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <Core/GameApplication/GameApplicationBase.h>
#include <Core/ResourceManager/ResourceManager.h>
#include <Core/World/World.h>
#include <Foundation/Configuration/CVar.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <MiniAudioPlugin/Components/MiniAudioListenerComponent.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundComponent.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffectNode.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffects.h>
#include <MiniAudioPlugin/Effects/MiniAudioMixerNode.h>
#include <MiniAudioPlugin/Effects/MiniAudioTimeStretch.h>
#include <MiniAudioPlugin/MiniAudioSingleton.h>
#include <MiniAudioPlugin/Resources/MiniAudioSoundResource.h>

ezCVarFloat cvar_MiniAudioMasterVolume("MiniAudio.Volume", 1.0f, ezCVarFlags::Save, "Overall volume for all MiniAudio output");
ezCVarBool cvar_MiniAudioMute("MiniAudio.Mute", false, ezCVarFlags::Default, "Whether MiniAudio output is muted");
ezCVarBool cvar_MiniAudioPause("MiniAudio.Pause", false, ezCVarFlags::Default, "Whether MiniAudio output is paused");

EZ_IMPLEMENT_SINGLETON(ezMiniAudioSingleton);

static ezMiniAudioSingleton g_MiniAudioSingleton;

ezMiniAudioSingleton::ezMiniAudioSingleton()
  : m_SingletonRegistrar(this)
{
  m_bInitialized = false;
  m_vListenerPosition.SetZero();
}

ezMiniAudioSingleton::~ezMiniAudioSingleton() = default;

void ezMiniAudioSingleton::Startup()
{
  if (m_bInitialized)
    return;

  m_pData = EZ_DEFAULT_NEW(Data);

  EZ_LOCK(m_pData->m_Mutex);

  ma_engine_config cfg = ma_engine_config_init();
  // cfg.pLog
  cfg.listenerCount = 1;
  cfg.channels = 0; // native channel count
  // cfg.allocationCallbacks
  // pContext

  auto result = ma_engine_init(&cfg, &m_pData->m_Engine);
  if (result != MA_SUCCESS)
    return;

  m_pData->m_pMixer = EZ_DEFAULT_NEW(ezMiniAudioMixerNode);
  if (m_pData->m_pMixer->Initialize(GetEngine()).Failed())
  {
    EZ_DEFAULT_DELETE(m_pData->m_pMixer);
    ma_engine_uninit(GetEngine());
    return;
  }
  m_bInitialized = true;
  UpdateSound();
}

void ezMiniAudioSingleton::Shutdown()
{
  // the lock must be released before m_pData (and thus m_pData->m_Mutex) is destroyed below,
  // otherwise the lock guard's destructor unlocks an already-freed mutex
  {
    EZ_LOCK(m_pData->m_Mutex);

    if (m_bInitialized)
    {
      m_bInitialized = false;

      for (ezUInt32 i = 0; i < m_pData->m_SoundInstancesStorage.GetCount(); ++i)
      {
        auto* pInst = &m_pData->m_SoundInstancesStorage[i];

        if (pInst->m_bInUse)
        {
          FreeSoundInstance(pInst);
        }
      }

      for (auto& group : m_pData->m_SoundGroups)
      {
        ma_sound_group_uninit(group.m_pGroup.Borrow());
        EZ_DEFAULT_DELETE(group.m_pEffects);
      }

      m_pData->m_SoundGroups.Clear();

      EZ_DEFAULT_DELETE(m_pData->m_pMixer);
      ma_engine_uninit(&m_pData->m_Engine);
    }
  }

  // finally delete all data
  m_pData.Clear();
}

void ezMiniAudioSingleton::SetNumListeners(ezUInt8 uiNumListeners)
{
}

ezUInt8 ezMiniAudioSingleton::GetNumListeners()
{
  return 1;
}

void ezMiniAudioSingleton::LoadConfiguration(ezStringView sFile)
{
}

void ezMiniAudioSingleton::SetOverridePlatform(ezStringView sPlatform)
{
}

void ezMiniAudioSingleton::UpdateSound()
{
  if (!m_bInitialized)
    return;

  EZ_LOCK(m_pData->m_Mutex);

  if (cvar_MiniAudioPause)
  {
    const ma_result res = ma_engine_stop(&m_pData->m_Engine);

    if (res == MA_UNAVAILABLE)
      return;

    EZ_MA_CHECK(res);
  }
  else
  {
    const ma_result res = ma_engine_start(&m_pData->m_Engine);

    if (res == MA_UNAVAILABLE)
      return;

    EZ_MA_CHECK(res);

    if (cvar_MiniAudioMute)
    {
      EZ_MA_CHECK(ma_engine_set_volume(&m_pData->m_Engine, 0.0f));
    }
    else
    {
      EZ_MA_CHECK(ma_engine_set_volume(&m_pData->m_Engine, cvar_MiniAudioMasterVolume));
    }
  }

  // Poll completion on the game thread. An end callback must never take the backend
  // mutex: ma_sound_uninit can wait for the audio graph while holding that mutex.
  const ezUInt32 uiCount = m_pData->m_SoundInstancesStorage.GetCount();
  for (ezUInt32 i = 0; i < uiCount; ++i)
  {
    auto* pInstance = &m_pData->m_SoundInstancesStorage[i];
    if (!pInstance->m_bInUse || !pInstance->m_bSoundInitialized || !ma_sound_at_end(&pInstance->m_Sound))
      continue;

    if (!pInstance->m_bFinishedNotified)
    {
      pInstance->m_bFinishedNotified = true;
      const float fTail = pInstance->m_pEffectNode ? pInstance->m_pEffectNode->GetTailSeconds() : 0.0f;
      pInstance->m_uiTailEnd = ma_engine_get_time_in_pcm_frames(GetEngine()) + static_cast<ma_uint64>(fTail * ma_engine_get_sample_rate(GetEngine()));
      const auto hComponent = pInstance->m_hComponent;
      pInstance->m_hComponent.Invalidate();
      if (pInstance->pWorld && !hComponent.IsInvalidated())
      {
        EZ_LOCK(pInstance->pWorld->GetWriteMarker());
        ezMiniAudioSoundComponent* pComponent = nullptr;
        if (pInstance->pWorld->TryGetComponent(hComponent, pComponent) && pComponent->IsActiveAndSimulating())
          pComponent->SoundFinished();
      }
    }
    if (ma_engine_get_time_in_pcm_frames(GetEngine()) >= pInstance->m_uiTailEnd)
      FreeSoundInstance(pInstance);
  }

  if (!m_pData->m_FadingInstances.IsEmpty())
  {
    for (ezUInt32 i = 0; i < m_pData->m_FadingInstances.GetCount(); ++i)
    {
      const ezUInt32 idx = m_pData->m_FadingInstances[i];
      ezMiniAudioSoundInstance* pInstance = &m_pData->m_SoundInstancesStorage[idx];

      if (!ma_sound_is_playing(&pInstance->m_Sound) || ma_sound_get_current_fade_volume(&pInstance->m_Sound) <= 0.0f)
      {
        FreeSoundInstance(pInstance);

        break;
      }
    }
  }
}

void ezMiniAudioSingleton::SetMasterChannelVolume(float fVolume)
{
  cvar_MiniAudioMasterVolume = ezMath::Clamp<float>(fVolume, 0.0f, 1.0f);
}

float ezMiniAudioSingleton::GetMasterChannelVolume() const
{
  return cvar_MiniAudioMasterVolume;
}

void ezMiniAudioSingleton::SetMasterChannelMute(bool bMute)
{
  cvar_MiniAudioMute = bMute;
}

bool ezMiniAudioSingleton::GetMasterChannelMute() const
{
  return cvar_MiniAudioMute;
}

void ezMiniAudioSingleton::SetMasterChannelPaused(bool bPaused)
{
  cvar_MiniAudioPause = bPaused;
}

bool ezMiniAudioSingleton::GetMasterChannelPaused() const
{
  return cvar_MiniAudioPause;
}

void ezMiniAudioSingleton::SetSoundGroupVolume(ezStringView sGroupName, float fVolume)
{
  auto& group = GetSoundGroup(sGroupName);
  group.m_fVolume = fVolume;

  ma_sound_group_set_volume(group.m_pGroup.Borrow(), fVolume);
}

float ezMiniAudioSingleton::GetSoundGroupVolume(ezStringView sGroupName) const
{
  if (sGroupName.IsEmpty())
    sGroupName = "Default";
  for (ezUInt32 i = 0; i < m_pData->m_SoundGroups.GetCount(); ++i)
  {
    if (m_pData->m_SoundGroups[i].m_sName == sGroupName)
      return m_pData->m_SoundGroups[i].m_fVolume;
  }

  return 1.0f;
}

ezMiniAudioSingleton::SoundGroup& ezMiniAudioSingleton::GetSoundGroup(ezStringView sGroupName)
{
  EZ_LOCK(m_pData->m_Mutex);
  if (sGroupName.IsEmpty())
    sGroupName = "Default";
  for (ezUInt32 i = 0; i < m_pData->m_SoundGroups.GetCount(); ++i)
  {
    if (m_pData->m_SoundGroups[i].m_sName == sGroupName)
      return m_pData->m_SoundGroups[i];
  }

  if (m_pData->m_SoundGroups.GetCount() >= ezMiniAudioMixerNode::MaxGroups)
  {
    ezLog::Error("MiniAudio supports at most {} active groups.", ezMiniAudioMixerNode::MaxGroups);
    return m_pData->m_SoundGroups[0];
  }
  auto& group = m_pData->m_SoundGroups.ExpandAndGetRef();
  group.m_sName = sGroupName;
  group.m_pGroup = EZ_DEFAULT_NEW(ma_sound_group);

  EZ_MA_CHECK(ma_sound_group_init(GetEngine(), MA_SOUND_FLAG_NO_PITCH, nullptr, group.m_pGroup.Borrow()));
  group.m_pEffects = EZ_DEFAULT_NEW(ezMiniAudioEffectNode);
  group.m_pEffects->Initialize(GetEngine(), &m_pData->m_pMixer->m_Node).AssertSuccess();
  EZ_MA_CHECK(ma_node_attach_output_bus(&group.m_pEffects->m_Node, 0, &m_pData->m_pMixer->m_Node, m_pData->m_SoundGroups.GetCount() - 1));
  EZ_MA_CHECK(ma_node_attach_output_bus(group.m_pGroup.Borrow(), 0, &group.m_pEffects->m_Node, 0));

  ezDynamicArray<ezMiniAudioEffectInstance> effects;
  for (const auto& effect : m_pData->m_ListenerEffects)
  {
    if (!effect.IsSourceEffect() && (effect.m_sGroup.IsEmpty() || effect.m_sGroup == group.m_sName))
    {
      ezMiniAudioEffect sanitized = effect;
      sanitized.Sanitize();
      effects.PushBack({sanitized, 1.0f});
    }
  }
  group.m_pEffects->Configure(effects);
  ezDynamicArray<ezString> names;
  for (const auto& bus : m_pData->m_SoundGroups)
    names.PushBack(bus.m_sName);
  m_pData->m_pMixer->Configure(names, m_pData->m_Duckers);
  return group;
}

void ezMiniAudioSingleton::GameApplicationEventHandler(const ezGameApplicationExecutionEvent& e)
{
  if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeforeUpdatePlugins)
  {
    ezMiniAudioSingleton::GetSingleton()->UpdateSound();
  }
  else if (e.m_Type == ezGameApplicationExecutionEvent::Type::AfterWorldUpdates)
  {
    ezMiniAudioSingleton::GetSingleton()->UpdateEffects();
  }
}

void ezMiniAudioSingleton::SetListenerOverrideMode(bool bEnabled)
{
  m_bListenerOverrideMode = bEnabled;
}

void ezMiniAudioSingleton::SetListener(ezInt32 iIndex, const ezVec3& vPosition, const ezVec3& vForward, const ezVec3& vUp, const ezVec3& vVelocity)
{
  if (m_bListenerOverrideMode)
  {
    if (iIndex != -1)
      return;

    iIndex = 0;
  }

  if (iIndex < 0 || iIndex >= /*FMOD_MAX_LISTENERS*/ 1)
    return;

  if (iIndex == 0)
  {
    m_vListenerPosition = vPosition;
  }

  const ezVec3 vMaUp = -vUp; // EZ and MiniAudio use different coordinate systems

  ma_engine_listener_set_position(&m_pData->m_Engine, iIndex, vPosition.x, vPosition.y, vPosition.z);
  ma_engine_listener_set_direction(&m_pData->m_Engine, iIndex, vForward.x, vForward.y, vForward.z);
  ma_engine_listener_set_world_up(&m_pData->m_Engine, iIndex, vMaUp.x, vMaUp.y, vMaUp.z);
  ma_engine_listener_set_velocity(&m_pData->m_Engine, iIndex, vVelocity.x, vVelocity.y, vVelocity.z);
}

ezResult ezMiniAudioSingleton::OneShotSound(ezWorld* pWorld, ezStringView sResourceID, const ezTransform& globalPosition, float fPitch /*= 1.0f*/, float fVolume /*= 1.0f*/, bool bBlockIfNotLoaded /*= true*/)
{
  ezMiniAudioSoundResourceHandle hSound = ezResourceManager::LoadResource<ezMiniAudioSoundResource>(sResourceID);

  if (!hSound.IsValid())
    return EZ_FAILURE;

  ezResourceLock<ezMiniAudioSoundResource> pResource(hSound, bBlockIfNotLoaded ? ezResourceAcquireMode::BlockTillLoaded_NeverFail : ezResourceAcquireMode::AllowLoadingFallback_NeverFail);

  if (pResource.GetAcquireResult() != ezResourceAcquireResult::Final)
    return EZ_FAILURE;

  if (pResource->GetLoop())
    return EZ_FAILURE; // never play looping sounds

  ezRandom* pRng = nullptr;

  if (pWorld)
  {
    pRng = &pWorld->GetRandomNumberGenerator();

    fVolume *= pResource->GetVolume(*pRng);
    fPitch *= pResource->GetPitch(*pRng);
  }

  auto pInstance = pResource->InstantiateSound(pRng, pWorld, {});
  if (pInstance == nullptr)
    return EZ_FAILURE;

  const ezVec3 pos = globalPosition.m_vPosition;
  ma_sound_set_position(&pInstance->m_Sound, pos.x, pos.y, pos.z);
  pInstance->m_fBasePitch = fPitch;
  ma_sound_set_pitch(&pInstance->m_Sound, fPitch * pInstance->m_fEffectPitch);
  ma_sound_set_volume(&pInstance->m_Sound, fVolume);

  // the sound will play until it ends and then get cleaned up automatically
  EZ_MA_CHECK(ma_sound_start(&pInstance->m_Sound));

  return EZ_SUCCESS;
}

ezMiniAudioSoundInstance* ezMiniAudioSingleton::AllocateSoundInstance(const ezDataBuffer& audioData, ezWorld* pWorld, ezComponentHandle hComponent, ma_sound_group* pGroup)
{
  if (!m_bInitialized)
    return nullptr;

  EZ_LOCK(m_pData->m_Mutex);
  if (pGroup == nullptr)
    pGroup = GetSoundGroup("Default").m_pGroup.Borrow();

  ezMiniAudioSoundInstance* pInstance = nullptr;

  if (!m_pData->m_SoundInstanceFreeList.IsEmpty())
  {
    const ezUInt32 idx = m_pData->m_SoundInstanceFreeList.PeekBack();
    m_pData->m_SoundInstanceFreeList.PopBack();
    pInstance = &m_pData->m_SoundInstancesStorage[idx];
  }
  else
  {
    const ezUInt32 idx = m_pData->m_SoundInstancesStorage.GetCount();
    pInstance = &m_pData->m_SoundInstancesStorage.ExpandAndGetRef();
    pInstance->m_uiOwnIndex = idx;
  }

  pInstance->m_bVolumeSource = pInstance->m_bUseOcclusion = false;
  pInstance->m_fVolumeWeight = 1;
  pInstance->m_fOcclusion = 0;
  pInstance->m_bInUse = true;
  pInstance->pWorld = pWorld;
  pInstance->m_hComponent = hComponent;
  pInstance->m_pGroup = pGroup;
  pInstance->m_hSourceObject.Invalidate();
  pInstance->m_SourceTags.Clear();
  pInstance->m_bSoundInitialized = false;
  pInstance->m_bDecoderInitialized = false;
  pInstance->m_bFinishedNotified = false;
  pInstance->m_uiTailEnd = 0;
  pInstance->m_fBasePitch = 1.0f;
  pInstance->m_fEffectPitch = 1.0f;
  pInstance->m_Effects.Clear();
  pInstance->m_hSourceComponent = hComponent;
  pInstance->m_AssetEffects.Clear();
  pInstance->m_sAssetGroup.Clear();
  pInstance->m_sGroup = "Default";
  for (const auto& group : m_pData->m_SoundGroups)
    if (group.m_pGroup.Borrow() == pGroup)
      pInstance->m_sGroup = group.m_sName;
  const ezComponent* pSource = nullptr;
  if (pWorld != nullptr && pWorld->TryGetComponent(hComponent, pSource))
  {
    pInstance->m_hSourceObject = pSource->GetOwner()->GetHandle();
    pInstance->m_SourceTags = pSource->GetOwner()->GetTags();
    if (const auto* pSound = ezDynamicCast<const ezMiniAudioSoundComponent*>(pSource))
    {
      pInstance->m_Effects = pSound->m_Effects;
    }
  }

  const ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
  const ma_result decoderResult = ma_decoder_init_memory(audioData.GetData(), audioData.GetCount(), &decoderConfig, &pInstance->m_Decoder);
  if (decoderResult != MA_SUCCESS)
  {
    ezLog::Error("MiniAudio could not decode a sound: {} ({}).", ma_result_description(decoderResult), decoderResult);
    FreeSoundInstance(pInstance);
    return nullptr;
  }

  pInstance->m_bDecoderInitialized = true;
  pInstance->m_pTimeStretch = EZ_DEFAULT_NEW(ezMiniAudioTimeStretch);
  if (pInstance->m_pTimeStretch->Initialize(&pInstance->m_Decoder).Failed())
  {
    FreeSoundInstance(pInstance);
    return nullptr;
  }
  if (ma_sound_init_from_data_source(GetEngine(), &pInstance->m_pTimeStretch->m_Source, MA_SOUND_FLAG_DECODE, pGroup, &pInstance->m_Sound) != MA_SUCCESS)
  {
    FreeSoundInstance(pInstance);
    return nullptr;
  }

  pInstance->m_bSoundInitialized = true;

  m_SoundInstanceEvents.Broadcast({ezMiniAudioSoundInstanceEvent::Type::Created, pInstance});
  ezMiniAudioConfigureEffects(*pInstance, GetEngine(), m_vListenerPosition);

  return pInstance;
}

void ezMiniAudioSingleton::FreeSoundInstance(ezMiniAudioSoundInstance*& ref_pInstance)
{
  if (ref_pInstance == nullptr)
    return;

  const ezUInt32 uiIndex = ref_pInstance->m_uiOwnIndex;

  if (!ref_pInstance->m_bInUse) // already freed
  {
    EZ_ASSERT_DEBUG(m_pData->m_SoundInstanceFreeList.Contains(uiIndex), "Sound is not in the free list");
    return;
  }

  EZ_LOCK(m_pData->m_Mutex);

  EZ_ASSERT_DEBUG(!m_pData->m_SoundInstanceFreeList.Contains(uiIndex), "Sound is already freed");

  ref_pInstance->m_bInUse = false;

  if (ref_pInstance->m_bSoundInitialized)
    m_SoundInstanceEvents.Broadcast({ezMiniAudioSoundInstanceEvent::Type::Destroying, ref_pInstance});

  auto& inst = m_pData->m_SoundInstancesStorage[uiIndex];
  inst.m_hComponent.Invalidate();
  inst.pWorld = nullptr;

  // The graph must stop reading the data source before the decoder is destroyed.
  if (inst.m_bSoundInitialized)
  {
    ma_sound_uninit(&inst.m_Sound);
    inst.m_bSoundInitialized = false;
  }
  EZ_DEFAULT_DELETE(inst.m_pTimeStretch);
  if (inst.m_bDecoderInitialized)
  {
    EZ_MA_CHECK(ma_decoder_uninit(&inst.m_Decoder));
    inst.m_bDecoderInitialized = false;
  }

  inst.m_hResource.Invalidate();
  EZ_DEFAULT_DELETE(inst.m_pEffectNode);
  inst.m_Effects.Clear();
  m_pData->m_FadingInstances.RemoveAndSwap(uiIndex);
  m_pData->m_SoundInstanceFreeList.PushBack(uiIndex);

  ref_pInstance = nullptr;
}

void ezMiniAudioSingleton::DetachSoundInstance(ezMiniAudioSoundInstance*& ref_pInstance)
{
  if (ref_pInstance == nullptr)
    return;

  // deactivate looping
  ma_sound_set_looping(&ref_pInstance->m_Sound, false);

  ref_pInstance->m_hComponent.Invalidate(); // owner doesn't want to be notified anymore
  // pInstance->pWorld = nullptr; // but keep the world reference for shutdown behavior

  // owner doesn't point to it any longer, but we'll continue playing it
  ref_pInstance = nullptr;
}


void ezMiniAudioSingleton::DetachAndFadeOutSoundInstance(ezMiniAudioSoundInstance*& ref_pInstance, ezTime fadeDuration)
{
  if (ref_pInstance == nullptr)
    return;

  EZ_LOCK(m_pData->m_Mutex);
  m_pData->m_FadingInstances.PushBack(ref_pInstance->m_uiOwnIndex);

  ref_pInstance->m_hComponent.Invalidate(); // owner doesn't want to be notified anymore
  // pInstance->pWorld = nullptr; // but keep the world reference for shutdown behavior

  ma_sound_stop_with_fade_in_milliseconds(&ref_pInstance->m_Sound, static_cast<ma_uint64>(fadeDuration.GetMilliseconds()));

  // owner doesn't point to it any longer, but we'll continue playing it
  ref_pInstance = nullptr;
}

void ezMiniAudioSingleton::SetListenerComponent(ezComponentHandle hComponent)
{
  if (m_pData)
    m_pData->m_hListener = hComponent;
}

void ezMiniAudioSingleton::ClearListenerComponent(ezComponentHandle hComponent)
{
  if (!m_bInitialized)
    return;
  EZ_LOCK(m_pData->m_Mutex);
  if (m_pData->m_hListener == hComponent)
  {
    m_pData->m_hListener.Invalidate();
    m_pData->m_ListenerEffects.Clear();
    m_pData->m_Duckers.Clear();
    for (auto& group : m_pData->m_SoundGroups)
      group.m_pEffects->Configure({});
    m_pData->m_pMixer->Configure({}, {});
  }
}

ezArrayPtr<const ezMiniAudioGroupEffect> ezMiniAudioSingleton::GetListenerEffects() const
{
  return m_pData->m_ListenerEffects;
}

void ezMiniAudioSingleton::UpdateEffects()
{
  if (!m_bInitialized)
    return;
  EZ_LOCK(m_pData->m_Mutex);
  m_pData->m_ListenerEffects.Clear();
  ezDynamicArray<ezMiniAudioDucker> duckers;
  if (!m_pData->m_hListener.IsInvalidated())
  {
    auto* pWorld = ezWorld::GetWorld(m_pData->m_hListener);
    EZ_LOCK(pWorld->GetReadMarker());
    const ezMiniAudioListenerComponent* pListener = nullptr;
    if (pWorld->TryGetComponent(m_pData->m_hListener, pListener) && pListener->IsActiveAndSimulating())
    {
      m_pData->m_ListenerEffects = pListener->m_Effects;
      if (pListener->m_bDucker)
        duckers = pListener->m_Duckers;
    }
  }
  m_pData->m_Duckers = duckers;
  ezMiniAudioUpdateEffectVolumes(m_vListenerPosition);
  for (auto& instance : m_pData->m_SoundInstancesStorage)
  {
    if (instance.m_bInUse && instance.m_bSoundInitialized)
      ezMiniAudioConfigureEffects(instance, GetEngine(), m_vListenerPosition);
  }
  ezDynamicArray<ezString> names;
  for (auto& group : m_pData->m_SoundGroups)
  {
    names.PushBack(group.m_sName);
    ezDynamicArray<ezMiniAudioEffectInstance> effects;
    for (const auto& effect : m_pData->m_ListenerEffects)
    {
      if (!effect.IsSourceEffect() && (effect.m_sGroup.IsEmpty() || effect.m_sGroup == group.m_sName))
      {
        ezMiniAudioEffect sanitized = effect;
        sanitized.Sanitize();
        effects.PushBack({sanitized, 1.0f});
      }
    }
    group.m_pEffects->Configure(effects);
  }
  m_pData->m_pMixer->Configure(names, duckers);
}

void ezMiniAudioSingleton::StopWorldSounds(ezWorld* pWorld)
{
  if (m_pData == nullptr)
    return;

  EZ_LOCK(m_pData->m_Mutex);
  if (!m_pData->m_hListener.IsInvalidated() && ezWorld::GetWorld(m_pData->m_hListener) == pWorld)
  {
    m_pData->m_hListener.Invalidate();
    m_pData->m_ListenerEffects.Clear();
    m_pData->m_Duckers.Clear();
    for (auto& group : m_pData->m_SoundGroups)
      group.m_pEffects->Configure({});
    m_pData->m_pMixer->Configure({}, {});
  }

  for (ezUInt32 i = 0; i < m_pData->m_FadingInstances.GetCount();)
  {
    const ezUInt32 idx = m_pData->m_FadingInstances[i];

    if (m_pData->m_SoundInstancesStorage[idx].pWorld == pWorld)
    {
      m_pData->m_FadingInstances.RemoveAtAndSwap(i);
    }
    else
    {
      ++i;
    }
  }

  for (ezUInt32 i = 0; i < m_pData->m_SoundInstancesStorage.GetCount(); ++i)
  {
    auto* pInst = &m_pData->m_SoundInstancesStorage[i];

    if (pInst->pWorld == pWorld)
    {
      // since the world pointer is set, that means the sound is not freed
      FreeSoundInstance(pInst);
    }
  }
}

void ezMiniAudioSingleton::GetSoundInstances(ezDynamicArray<ezMiniAudioSoundInstance*>& out_instances)
{
  out_instances.Clear();
  if (!m_bInitialized)
    return;

  EZ_LOCK(m_pData->m_Mutex);
  for (auto& instance : m_pData->m_SoundInstancesStorage)
  {
    if (instance.m_bInUse && instance.m_bSoundInitialized)
      out_instances.PushBack(&instance);
  }
}
