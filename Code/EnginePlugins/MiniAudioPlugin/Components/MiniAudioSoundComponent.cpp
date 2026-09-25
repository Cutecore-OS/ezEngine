#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/Messages/DeleteObjectMessage.h>
#include <Core/ResourceManager/Implementation/ResourceHandleReflection.h>
#include <Core/World/GameObject.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundComponent.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffectNode.h>
#include <MiniAudioPlugin/MiniAudioSingleton.h>
#include <MiniAudioPlugin/Resources/MiniAudioSoundResource.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezMiniAudioOcclusionManipulatorAttribute, 1, ezRTTIDefaultAllocator<ezMiniAudioOcclusionManipulatorAttribute>)
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezMiniAudioSoundComponentManager::ezMiniAudioSoundComponentManager(ezWorld* pWorld)
  : ezComponentManager(pWorld)
{
}

void ezMiniAudioSoundComponentManager::Initialize()
{
  SUPER::Initialize();

  {
    auto desc = EZ_CREATE_MODULE_UPDATE_FUNCTION_DESC(ezMiniAudioSoundComponentManager::UpdateEvents, this);
    desc.m_Phase = ezWorldUpdatePhase::PostTransform;
    desc.m_bOnlyUpdateWhenSimulating = true;

    RegisterUpdateFunction(desc);
  }
}

void ezMiniAudioSoundComponentManager::Deinitialize()
{
  SUPER::Deinitialize();

  ezMiniAudioSingleton::GetSingleton()->StopWorldSounds(GetWorld());
}

void ezMiniAudioSoundComponentManager::UpdateEvents(const ezWorldModule::UpdateContext& context)
{
  constexpr ezUInt32 uiUpdatesPerSec = 20;
  constexpr ezTime tUpdateRate = ezTime::Milliseconds(1000 / uiUpdatesPerSec);

  const float fUpdateFraction = (GetWorld()->GetClock().GetTimeDiff() / tUpdateRate).AsFloatInSeconds();

  const ezUInt32 uiNumComps = m_ComponentStorage.GetCount();

  if (m_uiFirstComponentIndex >= uiNumComps)
  {
    m_uiFirstComponentIndex = 0;
  }

  const ezUInt32 uiNumUpdate = static_cast<ezUInt32>(uiNumComps * fUpdateFraction) + 1;
  const ezUInt32 uiLastCompP1 = ezMath::Min(m_uiFirstComponentIndex + uiNumUpdate, uiNumComps);

  for (auto it = m_ComponentStorage.GetIterator(m_uiFirstComponentIndex, uiNumComps); it.IsValid(); ++it)
  {
    ComponentType* pComponent = it;

    // a lot of components will actually be inactive (waiting to be reused)
    if (pComponent->IsActiveAndInitialized())
    {
      pComponent->Update();
    }
  }

  m_uiFirstComponentIndex = uiLastCompP1;
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezMiniAudioSoundComponent, 6, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_MEMBER_PROPERTY("Sound", m_hSound)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_MiniAudio_Sound", ezDependencyFlags::Package), new ezRequiredAttribute()),
    EZ_ACCESSOR_PROPERTY("Paused", GetPaused, SetPaused),
    EZ_ACCESSOR_PROPERTY("Volume", GetVolume, SetVolume)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_ACCESSOR_PROPERTY("Pitch", GetPitch, SetPitch)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.1f, 10.0f)),
    EZ_ACCESSOR_PROPERTY("NoGlobalPitch", GetNoGlobalPitch, SetNoGlobalPitch),
    EZ_MEMBER_PROPERTY("Group", m_sGroup)->AddAttributes(new ezDynamicStringEnumAttribute("MiniAudioSoundGroups"), new ezDefaultValueAttribute("")),
    EZ_ARRAY_MEMBER_PROPERTY("Effects", m_Effects),
    EZ_MEMBER_PROPERTY("UseOcclusion", m_bUseOcclusion),
    EZ_MEMBER_PROPERTY("OcclusionRadius", m_fOcclusionRadius)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, ezVariant()), new ezSuffixAttribute(" m")),
    EZ_MEMBER_PROPERTY("OcclusionRange", m_fOcclusionRange)->AddAttributes(new ezDefaultValueAttribute(50.0f), new ezClampValueAttribute(0.0f, ezVariant()), new ezSuffixAttribute(" m")),
    EZ_MEMBER_PROPERTY("OcclusionThreshold", m_fOcclusionThreshold)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("OcclusionCollisionLayer", m_uiOcclusionCollisionLayer)->AddAttributes(new ezDynamicEnumAttribute("PhysicsCollisionLayer")),
    EZ_ENUM_MEMBER_PROPERTY("OnFinishedAction", ezOnComponentFinishedAction2, m_OnFinishedAction),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgDeleteGameObject, OnMsgDeleteGameObject),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_FUNCTIONS
  {
    EZ_SCRIPT_FUNCTION_PROPERTY(Play),
    EZ_SCRIPT_FUNCTION_PROPERTY(Pause),
    EZ_SCRIPT_FUNCTION_PROPERTY(Stop),
    EZ_SCRIPT_FUNCTION_PROPERTY(FadeOut, In, "Delay"),
    EZ_SCRIPT_FUNCTION_PROPERTY(StartOneShot),
  }
  EZ_END_FUNCTIONS;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Sound/MiniAudio"),
    new ezMiniAudioOcclusionManipulatorAttribute(),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

enum
{
  NoGlobalPitch = 0,
};

ezMiniAudioSoundComponent::ezMiniAudioSoundComponent() = default;
ezMiniAudioSoundComponent::~ezMiniAudioSoundComponent() = default;

void ezMiniAudioSoundComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);

  auto& s = inout_stream.GetStream();

  s << m_hSound;
  s << m_bPaused;
  s << m_fPitch;
  s << m_fComponentVolume;

  ezOnComponentFinishedAction2::StorageType type = m_OnFinishedAction;
  s << type;
  s.WriteArray(m_Effects).IgnoreResult();
  s << m_sGroup;
  s << m_bUseOcclusion << m_fOcclusionThreshold << m_uiOcclusionCollisionLayer;
  s << GetNoGlobalPitch();
  s << m_fOcclusionRadius << m_fOcclusionRange;
}

void ezMiniAudioSoundComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  const ezUInt32 uiVersion = inout_stream.GetComponentTypeVersion(GetStaticRTTI());

  auto& s = inout_stream.GetStream();

  s >> m_hSound;
  s >> m_bPaused;
  s >> m_fPitch;
  s >> m_fComponentVolume;

  ezOnComponentFinishedAction2::StorageType type;
  s >> type;
  m_OnFinishedAction = (ezOnComponentFinishedAction2::Enum)type;
  if (uiVersion >= 2)
    s.ReadArray(m_Effects).IgnoreResult();
  if (uiVersion == 3)
  {
    ezTagSet legacyTags;
    legacyTags.Load(s, ezTagRegistry::GetGlobalRegistry());
  }
  if (uiVersion >= 4)
    s >> m_sGroup;
  if (uiVersion >= 5)
  {
    s >> m_bUseOcclusion >> m_fOcclusionThreshold >> m_uiOcclusionCollisionLayer;
    bool bNoGlobalPitch;
    s >> bNoGlobalPitch;
    SetNoGlobalPitch(bNoGlobalPitch);
  }
  if (uiVersion >= 6)
    s >> m_fOcclusionRadius >> m_fOcclusionRange;
}

void ezMiniAudioSoundComponent::SetPaused(bool b)
{
  if (b == m_bPaused)
    return;

  m_bPaused = b;

  if (m_bPaused)
  {
    Pause();
  }
  else
  {
    Play();
  }
}

void ezMiniAudioSoundComponent::SetPitch(float f)
{
  if (f == m_fPitch)
    return;

  m_fPitch = f;
}

void ezMiniAudioSoundComponent::SetVolume(float f)
{
  if (f == m_fComponentVolume)
    return;

  m_fComponentVolume = f;
}

void ezMiniAudioSoundComponent::SetNoGlobalPitch(bool bEnable)
{
  SetUserFlag(NoGlobalPitch, bEnable);
}

bool ezMiniAudioSoundComponent::GetNoGlobalPitch() const
{
  return GetUserFlag(NoGlobalPitch);
}

void ezMiniAudioSoundComponent::OnSimulationStarted()
{
  if (!m_bPaused)
  {
    Play();
  }
}

void ezMiniAudioSoundComponent::OnDeactivated()
{
  if (m_pInstance)
  {
    ezMiniAudioSingleton* pMA = ezMiniAudioSingleton::GetSingleton();

    // fade it out over a short period
    pMA->DetachAndFadeOutSoundInstance(m_pInstance, ezTime::Milliseconds(500));
  }
}

void ezMiniAudioSoundComponent::Play()
{
  if (!m_hSound.IsValid())
    return;

  if (m_pInstance == nullptr)
  {
    ezResourceLock<ezMiniAudioSoundResource> pResource(m_hSound, ezResourceAcquireMode::BlockTillLoaded_NeverFail);

    if (pResource.GetAcquireResult() != ezResourceAcquireResult::Final)
      return;

    ezRandom& rng = GetWorld()->GetRandomNumberGenerator();
    m_pInstance = pResource->InstantiateSound(&rng, GetWorld(), GetHandle());
    if (m_pInstance == nullptr)
      return;

    m_fResourceVolume = pResource->GetVolume(rng);
    m_fResourcePitch = pResource->GetPitch(rng);

    Update();
  }

  if (m_pInstance->m_pEffectNode)
    ma_node_set_state(&m_pInstance->m_pEffectNode->m_Node, ma_node_state_started);
  EZ_MA_CHECK(ma_sound_start(&m_pInstance->m_Sound));
  m_bPaused = false;
}

void ezMiniAudioSoundComponent::Pause()
{
  if (m_pInstance)
  {
    EZ_MA_CHECK(ma_sound_stop(&m_pInstance->m_Sound));
    if (m_pInstance->m_pEffectNode)
      ma_node_set_state(&m_pInstance->m_pEffectNode->m_Node, ma_node_state_stopped);
  }
}

void ezMiniAudioSoundComponent::Stop()
{
  if (m_pInstance == nullptr)
    return;

  // just free the sound, this will stop it right away
  ezMiniAudioSingleton* pMA = ezMiniAudioSingleton::GetSingleton();
  pMA->FreeSoundInstance(m_pInstance);
}

void ezMiniAudioSoundComponent::FadeOut(ezTime fadeDuration)
{
  if (m_pInstance == nullptr)
    return;

  ezMiniAudioSingleton* pMA = ezMiniAudioSingleton::GetSingleton();
  pMA->DetachAndFadeOutSoundInstance(m_pInstance, fadeDuration);
}

void ezMiniAudioSoundComponent::StartOneShot()
{
  if (!m_hSound.IsValid())
    return;

  ezResourceLock<ezMiniAudioSoundResource> pResource(m_hSound, ezResourceAcquireMode::BlockTillLoaded_NeverFail);

  if (pResource.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  ezRandom& rng = GetWorld()->GetRandomNumberGenerator();
  auto pInstance = pResource->InstantiateSound(&rng, GetWorld(), GetHandle());
  if (pInstance == nullptr)
    return;
  // Retain source identity for effects, but do not notify this component when a detached sound ends.
  pInstance->m_hComponent.Invalidate();

  const float fResourceVolume = pResource->GetVolume(rng);
  const float fResourcePitch = pResource->GetPitch(rng);

  UpdateParameters(pInstance, m_fComponentVolume * fResourceVolume, m_fPitch * fResourcePitch);

  // the sound will play until it ends and then get cleaned up automatically
  EZ_MA_CHECK(ma_sound_start(&pInstance->m_Sound));
}

void ezMiniAudioSoundComponent::OnMsgDeleteGameObject(ezMsgDeleteGameObject& msg)
{
  ezOnComponentFinishedAction2::HandleDeleteObjectMsg(msg, m_OnFinishedAction);
}

void ezMiniAudioSoundComponent::Update()
{
  if (m_pInstance)
  {
    UpdateParameters(m_pInstance, m_fComponentVolume * m_fResourceVolume, m_fPitch * m_fResourcePitch);
  }
}

void ezMiniAudioSoundComponent::UpdateParameters(ezMiniAudioSoundInstance* pInstance, float fVolume, float fPitch) const
{
  const ezVec3 pos = GetSourcePosition();

  ma_sound_set_position(&pInstance->m_Sound, pos.x, pos.y, pos.z);

  pInstance->m_fBasePitch = GetNoGlobalPitch() ? fPitch : fPitch * static_cast<float>(GetWorld()->GetClock().GetSpeed());
  ma_sound_set_pitch(&pInstance->m_Sound, ezMath::Clamp(pInstance->m_fBasePitch * pInstance->m_fEffectPitch, 0.01f, 100.0f));
  pInstance->m_Effects = m_Effects;

  ma_sound_set_volume(&pInstance->m_Sound, fVolume);

  // no need to set the direction, we currently don't support directional sounds
  // const ezVec3 dir = GetOwner()->GetGlobalDirForwards();
  // ma_sound_set_direction(&m_pInstance->m_Sound, dir.x, dir.y, dir.z);
}

void ezMiniAudioSoundComponent::SoundFinished()
{
  // reset used sound
  m_pInstance = nullptr;

  // TODO MiniAudio: send event
  // ezMsgFmodSoundFinished msg;
  // m_SoundFinishedEventSender.SendEventMessage(msg, this, GetOwner());

  ezOnComponentFinishedAction2::HandleFinishedAction(this, m_OnFinishedAction);

  if (m_OnFinishedAction == ezOnComponentFinishedAction2::Restart)
  {
    Play();
  }
}

ezVec3 ezMiniAudioSoundComponent::GetSourcePosition() const
{
  return GetOwner()->GetGlobalPosition();
}

float ezMiniAudioSoundComponent::GetOcclusion(const ezVec3& vListener) const
{
  return GetOcclusion(vListener, GetSourcePosition());
}

float ezMiniAudioSoundComponent::GetOcclusion(const ezVec3& vListener, const ezVec3& vSource) const
{
  const float fRange = ezMath::IsFinite(m_fOcclusionRange) ? ezMath::Max(0.0f, m_fOcclusionRange) : 50.0f;
  const float fRadius = ezMath::IsFinite(m_fOcclusionRadius) ? ezMath::Max(0.0f, m_fOcclusionRadius) : 1.0f;
  const float fDistance = (vSource - vListener).GetLength();
  const auto* pPhysics = GetWorld()->GetModuleReadOnly<ezPhysicsWorldModuleInterface>();
  if (!m_bUseOcclusion || m_fOcclusionThreshold >= 1.0f || pPhysics == nullptr || !vSource.IsValid() || !vListener.IsValid() ||
      !ezMath::IsFinite(fDistance) || fDistance <= ezMath::Max(0.02f, fRadius) || fDistance >= fRange)
  {
    m_fOcclusion = 0;
    m_fSmoothedOcclusion = 0;
    m_LastOcclusionSmoothing = ezTime::MakeFromSeconds(-1);
    m_LastOcclusionQuery = ezTime::MakeFromSeconds(-1);
    return 0;
  }

  const ezTime now = GetWorld()->GetClock().GetAccumulatedTime();
  const float fMovementTolerance = ezMath::Max(0.05f, ezMath::Min(fRadius * 0.25f, 0.25f));
  const bool bMoved = !vSource.IsEqual(m_vLastOcclusionSource, fMovementTolerance) || !vListener.IsEqual(m_vLastOcclusionListener, fMovementTolerance);
  const bool bResetSmoothing = m_LastOcclusionSmoothing.IsNegative() || now < m_LastOcclusionSmoothing ||
                               m_uiLastOcclusionLayer != m_uiOcclusionCollisionLayer || m_fLastOcclusionRadius != fRadius || m_fLastOcclusionRange != fRange ||
                               (vListener - m_vLastOcclusionListener).GetLength() > ezMath::Max(5.0f, fRadius * 2.0f) ||
                               (vSource - m_vLastOcclusionSource).GetLength() > ezMath::Max(5.0f, fRadius * 2.0f);
  if (bMoved || now < m_LastOcclusionQuery || now - m_LastOcclusionQuery >= ezTime::MakeFromMilliseconds(50) ||
      m_uiLastOcclusionLayer != m_uiOcclusionCollisionLayer || m_fLastOcclusionRadius != fRadius || m_fLastOcclusionRange != fRange)
  {
    m_LastOcclusionQuery = now;
    m_uiLastOcclusionLayer = m_uiOcclusionCollisionLayer;
    m_fLastOcclusionRadius = fRadius;
    m_fLastOcclusionRange = fRange;
    m_vLastOcclusionSource = vSource;
    m_vLastOcclusionListener = vListener;
    const ezVec3 vToListener = (vListener - vSource) / fDistance;
    // World-anchored Fibonacci sphere: camera rotation cannot rotate a regular ray
    // pattern over a grille. Only the facing hemisphere contributes, with weights
    // tending to zero at its silhouette rather than abruptly adding/removing rays.
    const ezUInt32 uiSampleCount = fRadius > 0 ? 32 : 1;
    ezPhysicsQueryParameters query(m_uiOcclusionCollisionLayer);
    query.m_bIgnoreInitialOverlap = true;
    query.m_ShapeTypes = ezPhysicsShapeType::Static | ezPhysicsShapeType::Dynamic;
    float fBlockedWeight = 0, fTotalWeight = 0;
    for (ezUInt32 i = 0; i < uiSampleCount; ++i)
    {
      ezVec3 vSample = ezVec3::MakeZero();
      float fWeight = 1;
      if (fRadius > 0)
      {
        const float z = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / uiSampleCount;
        const float r = ezMath::Sqrt(ezMath::Max(0.0f, 1.0f - z * z));
        const ezAngle angle = ezAngle::MakeFromRadian(static_cast<float>(i) * 2.39996323f);
        vSample = ezVec3(r * ezMath::Cos(angle), r * ezMath::Sin(angle), z);
        fWeight = ezMath::Max(0.0f, vSample.Dot(vToListener));
        if (fWeight <= 0)
          continue;
      }
      fTotalWeight += fWeight;
      const ezVec3 vEnd = vSource + vSample * fRadius;
      // A small listener aperture also avoids concentrating all rays on one pole
      // next to the listener. It is independent of the authored source radius.
      const ezVec3 vLateral = vSample - vToListener * vSample.Dot(vToListener);
      const ezVec3 vStart = vListener + vLateral * ezMath::Min(0.2f, fRadius);
      ezVec3 vDirection = vEnd - vStart;
      const float fRayLength = vDirection.GetLengthAndNormalize();
      ezPhysicsCastResultArray hits;
      if (!pPhysics->RaycastAll(hits, vStart, vDirection, fRayLength, query))
        continue;
      for (const auto& hit : hits.m_Results)
      {
        // Only finite hits strictly between the sampled endpoints can block sound.
        if (!ezMath::IsFinite(hit.m_fDistance) || hit.m_fDistance <= 0.001f || hit.m_fDistance >= fRayLength - 0.001f)
          continue;
        bool bOwnCollider = false;
        for (const ezGameObject* pObject = GetOwner(); pObject != nullptr; pObject = pObject->GetParent())
        {
          if (hit.m_hActorObject == pObject->GetHandle() || hit.m_hShapeObject == pObject->GetHandle())
          {
            bOwnCollider = true;
            break;
          }
        }
        if (!bOwnCollider)
        {
          fBlockedWeight += fWeight;
          break;
        }
      }
    }
    const float fMeasuredOcclusion = fTotalWeight > 0 ? fBlockedWeight / fTotalWeight : 0;
    // A small dead band suppresses near-identical coverage changes at grille edges.
    if (bResetSmoothing || fMeasuredOcclusion == 0 || fMeasuredOcclusion == 1 || ezMath::Abs(fMeasuredOcclusion - m_fOcclusion) >= 0.04f)
      m_fOcclusion = fMeasuredOcclusion;
  }
  const float fThreshold = ezMath::IsFinite(m_fOcclusionThreshold) ? ezMath::Saturate(m_fOcclusionThreshold) : 0.5f;
  const float fOcclusion = fThreshold < 1 ? ezMath::Saturate((m_fOcclusion - fThreshold) / (1 - fThreshold)) : 0.0f;
  if (bResetSmoothing)
    m_fSmoothedOcclusion = fOcclusion;
  else
  {
    const double fDelta = ezMath::Max(0.0, (now - m_LastOcclusionSmoothing).GetSeconds());
    // 90% attack in 300 ms, release in 150 ms. Updating multiple voices in one
    // world tick does not advance the smoothing repeatedly.
    const double fDuration = fOcclusion > m_fSmoothedOcclusion ? 0.3 : 0.15;
    const float fFactor = static_cast<float>(1.0 - ezMath::Pow(10.0, -fDelta / fDuration));
    m_fSmoothedOcclusion = ezMath::Lerp(m_fSmoothedOcclusion, fOcclusion, fFactor);
  }
  m_LastOcclusionSmoothing = now;
  // The entire source sphere is guaranteed clear. Outside it, blend occlusion in
  // over one radius so crossing the surface cannot suddenly muffle the source.
  const float t = fRadius > 0 ? ezMath::Saturate((fDistance - fRadius) / fRadius) : 1;
  const float fNearWeight = t * t * (3 - 2 * t);
  return m_fSmoothedOcclusion * fNearWeight * ezMath::Saturate((fRange - fDistance) / (fRange * 0.2f));
}
