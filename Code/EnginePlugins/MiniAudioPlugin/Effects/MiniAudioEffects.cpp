#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <MiniAudioPlugin/Components/MiniAudioEffectVolumeComponent.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundComponent.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundVolumeComponent.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffectNode.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffects.h>
#include <MiniAudioPlugin/Effects/MiniAudioTimeStretch.h>
#include <MiniAudioPlugin/MiniAudioSingleton.h>
#include <cmath>

namespace
{
  ezMutex s_VolumesMutex;
  ezDynamicArray<ezComponentHandle> s_Volumes;
  ezDynamicArray<ezComponentHandle> s_SoundVolumes;
} // namespace

void ezMiniAudioRegisterEffectVolume(ezComponentHandle hComponent)
{
  EZ_LOCK(s_VolumesMutex);
  s_Volumes.PushBack(hComponent);
}

void ezMiniAudioUnregisterEffectVolume(ezComponentHandle hComponent)
{
  EZ_LOCK(s_VolumesMutex);
  s_Volumes.RemoveAndCopy(hComponent);
}

void ezMiniAudioRegisterSoundVolume(ezComponentHandle hComponent)
{
  EZ_LOCK(s_VolumesMutex);
  s_SoundVolumes.PushBack(hComponent);
}

void ezMiniAudioUnregisterSoundVolume(ezComponentHandle hComponent)
{
  EZ_LOCK(s_VolumesMutex);
  s_SoundVolumes.RemoveAndCopy(hComponent);
}

void ezMiniAudioUpdateEffectVolumes(const ezVec3& vListener)
{
  ezDynamicArray<ezComponentHandle> handles;
  {
    EZ_LOCK(s_VolumesMutex);
    handles = s_Volumes;
  }
  for (const auto hVolume : handles)
  {
    auto* pWorld = ezWorld::GetWorld(hVolume);
    EZ_LOCK(pWorld->GetWriteMarker());
    ezMiniAudioEffectVolumeComponent* pVolume = nullptr;
    if (pWorld->TryGetComponent(hVolume, pVolume) && pVolume->IsActiveAndSimulating())
      pVolume->UpdateTemporalWeight(vListener);
  }
  {
    EZ_LOCK(s_VolumesMutex);
    handles = s_SoundVolumes;
  }
  for (const auto hVolume : handles)
  {
    auto* pWorld = ezWorld::GetWorld(hVolume);
    EZ_LOCK(pWorld->GetWriteMarker());
    ezMiniAudioSoundVolumeComponent* pVolume = nullptr;
    if (pWorld->TryGetComponent(hVolume, pVolume) && pVolume->IsActiveAndSimulating())
      pVolume->UpdateTemporalWeight(vListener);
  }
}

void ezMiniAudioConfigureEffects(ezMiniAudioSoundInstance& instance, ma_engine* pEngine, const ezVec3& vListener)
{
  ezDynamicArray<ezMiniAudioEffectInstance> effects;
  for (const auto& effect : instance.m_AssetEffects)
    effects.PushBack({effect, 1.0f});
  for (const auto& effect : instance.m_Effects)
    effects.PushBack({effect, 1.0f});

  if (instance.pWorld != nullptr)
  {
    ezHybridArray<ezComponentHandle, 16> handles;
    {
      EZ_LOCK(s_VolumesMutex);
      handles = s_Volumes;
    }
    EZ_LOCK(instance.pWorld->GetReadMarker());
    const ezMiniAudioSoundComponent* pSound = nullptr;
    if (instance.pWorld->TryGetComponent(instance.m_hSourceComponent, pSound))
    {
      instance.m_bVolumeSource = ezDynamicCast<const ezMiniAudioSoundVolumeComponent*>(pSound) != nullptr;
      instance.m_fVolumeWeight = pSound->GetVolumeWeight(vListener);
      instance.m_bUseOcclusion |= pSound->m_bUseOcclusion;
      const auto vPlaybackPosition = ma_sound_get_position(&instance.m_Sound);
      const ezVec3 vSource = instance.m_hComponent.IsInvalidated() ? ezVec3(vPlaybackPosition.x, vPlaybackPosition.y, vPlaybackPosition.z) : pSound->GetSourcePosition();
      instance.m_fOcclusion = pSound->GetOcclusion(vListener, vSource);
      if (!instance.m_hComponent.IsInvalidated())
        ma_sound_set_position(&instance.m_Sound, vSource.x, vSource.y, vSource.z);
      ezString groupName = pSound->m_sGroup.IsEmpty() ? instance.m_sAssetGroup : pSound->m_sGroup;
      if (groupName.IsEmpty())
        groupName = "Default";
      if (groupName != instance.m_sGroup)
      {
        auto& group = ezMiniAudioSingleton::GetSingleton()->GetSoundGroup(groupName);
        instance.m_sGroup = group.m_sName;
        instance.m_pGroup = group.m_pGroup.Borrow();
        ma_node* pOutput = instance.m_pEffectNode ? static_cast<ma_node*>(&instance.m_pEffectNode->m_Node) : static_cast<ma_node*>(&instance.m_Sound);
        EZ_MA_CHECK(ma_node_attach_output_bus(pOutput, 0, instance.m_pGroup, 0));
      }
    }

    if (pSound == nullptr)
    {
      instance.m_fOcclusion = 0;
      if (instance.m_bVolumeSource)
        instance.m_fVolumeWeight = 0;
    }

    ezHybridArray<const ezMiniAudioEffectVolumeComponent*, 16> volumes;
    for (const auto hVolume : handles)
    {
      if (ezWorld::GetWorld(hVolume) != instance.pWorld)
        continue;
      const ezMiniAudioEffectVolumeComponent* pVolume = nullptr;
      if (instance.pWorld->TryGetComponent(hVolume, pVolume) && pVolume->IsActiveAndSimulating())
        volumes.PushBack(pVolume);
    }
    volumes.Sort([](const ezMiniAudioEffectVolumeComponent* a, const ezMiniAudioEffectVolumeComponent* b)
      { return a->m_iPriority != b->m_iPriority ? a->m_iPriority < b->m_iPriority : a->GetHandle() < b->GetHandle(); });
    for (const auto* pVolume : volumes)
    {
      const bool bIncluded = pVolume->m_IncludeGroups.IsEmpty() || pVolume->m_IncludeGroups.Contains(instance.m_sGroup);
      const bool bExcluded = pVolume->m_ExcludeGroups.Contains(instance.m_sGroup);
      const float fWeight = bIncluded && !bExcluded ? pVolume->GetEffectWeight(vListener) : 0.0f;
      // Keep the chain topology stable at the boundary; only the weight changes.
      for (const auto& effect : pVolume->m_Effects)
        effects.PushBack({effect, fWeight});
    }
  }

  for (const auto& effect : ezMiniAudioSingleton::GetSingleton()->GetListenerEffects())
    if (effect.IsSourceEffect() && (effect.m_sGroup.IsEmpty() || effect.m_sGroup == instance.m_sGroup))
      effects.PushBack({effect, 1.0f});

  if (instance.m_bUseOcclusion)
  {
    ezMiniAudioEffect muffling;
    muffling.m_Type = ezMiniAudioEffectType::Muffling;
    muffling.m_fFrequency = 800;
    muffling.m_fMix = 1;
    muffling.m_fVolume = 0.25f;
    effects.PushBack({muffling, instance.m_fOcclusion});
  }
  if (instance.m_bVolumeSource)
  {
    // Gate after the voice's delay/reverb so their tails cannot bypass the volume.
    ezMiniAudioEffect gate;
    gate.m_Type = ezMiniAudioEffectType::Volume;
    gate.m_fVolume = 0;
    effects.PushBack({gate, 1.0f - instance.m_fVolumeWeight});
  }

  float fSpeed = 1.0f, fPitch = 1.0f, fTempo = 1.0f;
  bool bNeedsNode = false;
  for (auto& entry : effects)
  {
    auto& effect = entry.m_Effect;
    effect.Sanitize();
    if (effect.IsSourceEffect())
    {
      if (!effect.m_bEnabled)
        continue;
      if (effect.m_Type == ezMiniAudioEffectType::Speed)
        fSpeed *= ezMath::Lerp(1.0f, effect.m_fSpeed, entry.m_fWeight);
      else if (effect.m_Type == ezMiniAudioEffectType::Pitch)
        fPitch *= std::pow(2.0f, effect.m_fPitch * entry.m_fWeight / 12.0f);
      else
        fTempo *= ezMath::Lerp(1.0f, effect.m_fSpeed, entry.m_fWeight);
    }
    else
      bNeedsNode = true;
  }
  instance.m_fEffectPitch = ezMath::Clamp(fSpeed * fPitch, 0.01f, 100.0f);
  if (instance.m_pTimeStretch)
    instance.m_pTimeStretch->SetTempo(fTempo / fPitch);
  ma_sound_set_pitch(&instance.m_Sound, ezMath::Clamp(instance.m_fBasePitch * instance.m_fEffectPitch, 0.01f, 100.0f));

  if (instance.m_pEffectNode == nullptr && bNeedsNode)
  {
    ezMiniAudioEffectNode* pNode = EZ_DEFAULT_NEW(ezMiniAudioEffectNode);
    ma_node* pDestination = instance.m_pGroup ? static_cast<ma_node*>(instance.m_pGroup) : ma_engine_get_endpoint(pEngine);
    if (pNode->Initialize(pEngine, pDestination).Failed())
    {
      EZ_DEFAULT_DELETE(pNode);
      ezLog::Error("Could not initialize MiniAudio effects.");
      return;
    }
    pNode->Configure(effects);
    if (ma_node_attach_output_bus(&instance.m_Sound, 0, &pNode->m_Node, 0) != MA_SUCCESS)
    {
      EZ_DEFAULT_DELETE(pNode);
      ezLog::Error("Could not attach MiniAudio effects to the sound.");
      return;
    }
    instance.m_pEffectNode = pNode;
  }
  else if (instance.m_pEffectNode)
  {
    instance.m_pEffectNode->Configure(effects);
  }
}
