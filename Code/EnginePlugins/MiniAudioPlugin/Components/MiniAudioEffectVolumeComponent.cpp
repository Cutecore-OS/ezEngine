#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <Core/Messages/UpdateLocalBoundsMessage.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <MiniAudioPlugin/Components/MiniAudioEffectVolumeComponent.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffects.h>

// clang-format off
EZ_BEGIN_ABSTRACT_COMPONENT_TYPE(ezMiniAudioEffectVolumeComponent, 4)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("InterpolateByTime", m_bInterpolateByTime)->AddAttributes(new ezDefaultValueAttribute(false)),
    EZ_MEMBER_PROPERTY("InterpolationDuration", m_InterpolationDuration)->AddAttributes(new ezDefaultValueAttribute(ezTime::MakeFromSeconds(1.0)), new ezClampValueAttribute(ezTime::MakeZero(), ezVariant())),
    EZ_MEMBER_PROPERTY("Falloff", m_fFalloff)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, ezVariant())),
    EZ_MEMBER_PROPERTY("Priority", m_iPriority),
    EZ_ARRAY_MEMBER_PROPERTY("IncludeGroups", m_IncludeGroups)->AddAttributes(new ezDynamicStringEnumAttribute("MiniAudioSoundGroups")),
    EZ_ARRAY_MEMBER_PROPERTY("ExcludeGroups", m_ExcludeGroups)->AddAttributes(new ezDynamicStringEnumAttribute("MiniAudioSoundGroups")),
    EZ_ARRAY_MEMBER_PROPERTY("Effects", m_Effects),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Sound/MiniAudio"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_ABSTRACT_COMPONENT_TYPE;

EZ_BEGIN_COMPONENT_TYPE(ezMiniAudioEffectBoxComponent, 1, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("Extents", GetExtents, SetExtents)->AddAttributes(new ezDefaultValueAttribute(ezVec3(10.0f)), new ezClampValueAttribute(ezVec3(0.0f), ezVariant())),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgUpdateLocalBounds, OnUpdateLocalBounds),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezBoxManipulatorAttribute("Extents", 1.0f, true),
    new ezBoxVisualizerAttribute("Extents", 1.0f, ezColor::CornflowerBlue),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE;

EZ_BEGIN_COMPONENT_TYPE(ezMiniAudioEffectSphereComponent, 1, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("Radius", GetRadius, SetRadius)->AddAttributes(new ezDefaultValueAttribute(5.0f), new ezClampValueAttribute(0.0f, ezVariant())),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgUpdateLocalBounds, OnUpdateLocalBounds),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezSphereManipulatorAttribute("Radius"),
    new ezSphereVisualizerAttribute("Radius", ezColor::CornflowerBlue),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE;
// clang-format on

void ezMiniAudioEffectVolumeComponent::OnActivated()
{
  SUPER::OnActivated();
  m_fTemporalWeight = 0.0f;
  m_uiLastWeightUpdate = ezInvalidIndex;
  ezMiniAudioRegisterEffectVolume(GetHandle());
  GetOwner()->UpdateLocalBounds();
}

void ezMiniAudioEffectVolumeComponent::OnDeactivated()
{
  ezMiniAudioUnregisterEffectVolume(GetHandle());
  SUPER::OnDeactivated();
  GetOwner()->UpdateLocalBounds();
}

float ezMiniAudioEffectVolumeComponent::CalculateWeight(float fDistanceInside) const
{
  if (fDistanceInside < 0.0f)
    return 0.0f;
  if (m_bInterpolateByTime)
    return 1.0f;
  if (!ezMath::IsFinite(m_fFalloff) || m_fFalloff <= 0.0f)
    return 1.0f;
  const float t = ezMath::Saturate(fDistanceInside / m_fFalloff);
  return t * t * (3.0f - 2.0f * t);
}

void ezMiniAudioEffectVolumeComponent::UpdateTemporalWeight(const ezVec3& vListener)
{
  const ezUInt32 uiUpdate = GetWorld()->GetUpdateCounter();
  if (m_uiLastWeightUpdate == uiUpdate)
    return;
  m_uiLastWeightUpdate = uiUpdate;

  const float fTarget = GetWeight(vListener);
  const double fDuration = m_InterpolationDuration.GetSeconds();
  if (!m_bInterpolateByTime || !ezMath::IsFinite(fDuration) || fDuration <= 0.0)
  {
    m_fTemporalWeight = fTarget;
    return;
  }

  // Same exponential interpolation as ezVolumeSampler: 90% after one duration.
  const double fDelta = ezMath::Max(0.0, GetWorld()->GetClock().GetTimeDiff().GetSeconds());
  const float fFactor = static_cast<float>(1.0 - ezMath::Pow(10.0, -fDelta / fDuration));
  m_fTemporalWeight = ezMath::Lerp(m_fTemporalWeight, fTarget, fFactor);
}

float ezMiniAudioEffectVolumeComponent::GetEffectWeight(const ezVec3& vListener) const
{
  return m_bInterpolateByTime ? m_fTemporalWeight : GetWeight(vListener);
}

bool ezMiniAudioEffectVolumeComponent::GetLocalPosition(const ezVec3& vGlobalPosition, ezVec3& out_vLocalPosition) const
{
  const ezTransform transform = GetOwner()->GetGlobalTransform();
  const ezVec3 vScale = transform.m_vScale.Abs();
  if (!vScale.IsValid() || ezMath::Min(vScale.x, vScale.y, vScale.z) < 0.000001f)
    return false;

  out_vLocalPosition = (transform.m_qRotation.GetInverse() * (vGlobalPosition - transform.m_vPosition)).CompDiv(transform.m_vScale);
  return out_vLocalPosition.IsValid();
}

void ezMiniAudioEffectVolumeComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s << m_fFalloff << m_iPriority;
  s.WriteArray(m_ExcludeGroups).IgnoreResult();
  s.WriteArray(m_Effects).IgnoreResult();
  s.WriteArray(m_IncludeGroups).IgnoreResult();
  s << m_bInterpolateByTime << m_InterpolationDuration;
}

void ezMiniAudioEffectVolumeComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s >> m_fFalloff >> m_iPriority;
  const auto version = inout_stream.GetComponentTypeVersion(ezMiniAudioEffectVolumeComponent::GetStaticRTTI());
  if (version >= 3)
    s.ReadArray(m_ExcludeGroups).IgnoreResult();
  else
  {
    ezTagSet legacyTags;
    legacyTags.Load(s, ezTagRegistry::GetGlobalRegistry());
  }
  s.ReadArray(m_Effects).IgnoreResult();
  if (version >= 3)
    s.ReadArray(m_IncludeGroups).IgnoreResult();
  else if (version == 2)
  {
    ezTagSet legacyTags;
    legacyTags.Load(s, ezTagRegistry::GetGlobalRegistry());
  }
  if (version >= 4)
    s >> m_bInterpolateByTime >> m_InterpolationDuration;
}

void ezMiniAudioEffectBoxComponent::SetExtents(const ezVec3& vExtents)
{
  m_vExtents = vExtents.IsValid() ? vExtents.CompMax(ezVec3::MakeZero()) : ezVec3(10.0f);
  if (IsActiveAndInitialized())
    GetOwner()->UpdateLocalBounds();
}

float ezMiniAudioEffectBoxComponent::GetWeight(const ezVec3& vGlobalPosition) const
{
  ezVec3 vLocal;
  if (ezMath::Min(m_vExtents.x, m_vExtents.y, m_vExtents.z) <= 0.0f || !GetLocalPosition(vGlobalPosition, vLocal))
    return 0.0f;
  const ezVec3 vDistance = m_vExtents * 0.5f - vLocal.Abs();
  return CalculateWeight(ezMath::Min(vDistance.x, vDistance.y, vDistance.z));
}

void ezMiniAudioEffectBoxComponent::OnUpdateLocalBounds(ezMsgUpdateLocalBounds& ref_msg) const
{
  ref_msg.AddBounds(ezBoundingBoxSphere::MakeFromBox(ezBoundingBox::MakeFromCenterAndHalfExtents(ezVec3::MakeZero(), m_vExtents * 0.5f)), ezInvalidSpatialDataCategory);
}

void ezMiniAudioEffectBoxComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  inout_stream.GetStream() << m_vExtents;
}

void ezMiniAudioEffectBoxComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  inout_stream.GetStream() >> m_vExtents;
  SetExtents(m_vExtents);
}

void ezMiniAudioEffectSphereComponent::SetRadius(float fRadius)
{
  m_fRadius = ezMath::IsFinite(fRadius) ? ezMath::Max(fRadius, 0.0f) : 5.0f;
  if (IsActiveAndInitialized())
    GetOwner()->UpdateLocalBounds();
}

float ezMiniAudioEffectSphereComponent::GetWeight(const ezVec3& vGlobalPosition) const
{
  ezVec3 vLocal;
  if (m_fRadius <= 0.0f || !GetLocalPosition(vGlobalPosition, vLocal))
    return 0.0f;
  return CalculateWeight(m_fRadius - vLocal.GetLength());
}

void ezMiniAudioEffectSphereComponent::OnUpdateLocalBounds(ezMsgUpdateLocalBounds& ref_msg) const
{
  ref_msg.AddBounds(ezBoundingSphere::MakeFromCenterAndRadius(ezVec3::MakeZero(), m_fRadius), ezInvalidSpatialDataCategory);
}

void ezMiniAudioEffectSphereComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  inout_stream.GetStream() << m_fRadius;
}

void ezMiniAudioEffectSphereComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  inout_stream.GetStream() >> m_fRadius;
  SetRadius(m_fRadius);
}

EZ_STATICLINK_FILE(MiniAudioPlugin, MiniAudioPlugin_Components_MiniAudioEffectVolumeComponent);
