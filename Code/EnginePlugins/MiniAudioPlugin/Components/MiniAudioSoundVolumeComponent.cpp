#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <Core/Messages/UpdateLocalBoundsMessage.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundVolumeComponent.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffects.h>

// clang-format off
EZ_BEGIN_ABSTRACT_COMPONENT_TYPE(ezMiniAudioSoundVolumeComponent, 1)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("InterpolateByTime", m_bInterpolateByTime)->AddAttributes(new ezDefaultValueAttribute(false)),
    EZ_MEMBER_PROPERTY("InterpolationDuration", m_InterpolationDuration)->AddAttributes(new ezDefaultValueAttribute(ezTime::MakeFromSeconds(1.0)), new ezClampValueAttribute(ezTime::MakeZero(), ezVariant())),
    EZ_MEMBER_PROPERTY("Falloff", m_fFalloff)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, ezVariant())),
    EZ_MEMBER_PROPERTY("CaptureOffset", m_vCaptureOffset),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Sound/MiniAudio"),
    new ezTransformManipulatorAttribute("CaptureOffset"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_ABSTRACT_COMPONENT_TYPE;

EZ_BEGIN_COMPONENT_TYPE(ezMiniAudioSoundBoxComponent, 1, ezComponentMode::Static)
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

EZ_BEGIN_COMPONENT_TYPE(ezMiniAudioSoundSphereComponent, 1, ezComponentMode::Static)
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

void ezMiniAudioSoundVolumeComponent::OnActivated()
{
  SUPER::OnActivated();
  m_fTemporalWeight = 0.0f;
  m_uiLastWeightUpdate = ezInvalidIndex;
  GetWorld()->GetOrCreateComponentManager<ezMiniAudioSoundComponentManager>();
  ezMiniAudioRegisterSoundVolume(GetHandle());
  GetOwner()->UpdateLocalBounds();
}

void ezMiniAudioSoundVolumeComponent::OnDeactivated()
{
  ezMiniAudioUnregisterSoundVolume(GetHandle());
  SUPER::OnDeactivated();
  GetOwner()->UpdateLocalBounds();
}

float ezMiniAudioSoundVolumeComponent::CalculateWeight(float fDistanceInside) const
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

void ezMiniAudioSoundVolumeComponent::UpdateTemporalWeight(const ezVec3& vListener)
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

float ezMiniAudioSoundVolumeComponent::GetVolumeWeight(const ezVec3& vListener) const
{
  return m_bInterpolateByTime ? m_fTemporalWeight : GetWeight(vListener);
}

bool ezMiniAudioSoundVolumeComponent::GetLocalPosition(const ezVec3& vGlobalPosition, ezVec3& out_vLocalPosition) const
{
  const ezTransform transform = GetOwner()->GetGlobalTransform();
  const ezVec3 vScale = transform.m_vScale.Abs();
  if (!vScale.IsValid() || ezMath::Min(vScale.x, vScale.y, vScale.z) < 0.000001f)
    return false;

  out_vLocalPosition = (transform.m_qRotation.GetInverse() * (vGlobalPosition - transform.m_vPosition)).CompDiv(transform.m_vScale);
  return out_vLocalPosition.IsValid();
}

void ezMiniAudioSoundVolumeComponent::SerializeComponent(ezWorldWriter& stream) const
{
  SUPER::SerializeComponent(stream);
  stream.GetStream() << m_fFalloff << m_bInterpolateByTime << m_InterpolationDuration;
  stream.GetStream() << m_vCaptureOffset;
}

void ezMiniAudioSoundVolumeComponent::DeserializeComponent(ezWorldReader& stream)
{
  SUPER::DeserializeComponent(stream);
  stream.GetStream() >> m_fFalloff >> m_bInterpolateByTime >> m_InterpolationDuration;
  stream.GetStream() >> m_vCaptureOffset;
}

ezVec3 ezMiniAudioSoundVolumeComponent::GetSourcePosition() const
{
  return GetOwner()->GetGlobalTransform().TransformPosition(m_vCaptureOffset.IsValid() ? m_vCaptureOffset : ezVec3::MakeZero());
}

void ezMiniAudioSoundVolumeComponent::Update()
{
  SUPER::Update();
}

void ezMiniAudioSoundBoxComponent::SetExtents(const ezVec3& vExtents)
{
  m_vExtents = vExtents.IsValid() ? vExtents.CompMax(ezVec3::MakeZero()) : ezVec3(10.0f);
  if (IsActiveAndInitialized())
    GetOwner()->UpdateLocalBounds();
}

float ezMiniAudioSoundBoxComponent::GetWeight(const ezVec3& vGlobalPosition) const
{
  ezVec3 vLocal;
  if (ezMath::Min(m_vExtents.x, m_vExtents.y, m_vExtents.z) <= 0.0f || !GetLocalPosition(vGlobalPosition, vLocal))
    return 0.0f;
  const ezVec3 vDistance = m_vExtents * 0.5f - vLocal.Abs();
  return CalculateWeight(ezMath::Min(vDistance.x, vDistance.y, vDistance.z));
}

void ezMiniAudioSoundBoxComponent::OnUpdateLocalBounds(ezMsgUpdateLocalBounds& ref_msg) const
{
  ref_msg.AddBounds(ezBoundingBoxSphere::MakeFromBox(ezBoundingBox::MakeFromCenterAndHalfExtents(ezVec3::MakeZero(), m_vExtents * 0.5f)), ezInvalidSpatialDataCategory);
}

void ezMiniAudioSoundBoxComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  inout_stream.GetStream() << m_vExtents;
}

void ezMiniAudioSoundBoxComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  inout_stream.GetStream() >> m_vExtents;
  SetExtents(m_vExtents);
}

void ezMiniAudioSoundSphereComponent::SetRadius(float fRadius)
{
  m_fRadius = ezMath::IsFinite(fRadius) ? ezMath::Max(fRadius, 0.0f) : 5.0f;
  if (IsActiveAndInitialized())
    GetOwner()->UpdateLocalBounds();
}

float ezMiniAudioSoundSphereComponent::GetWeight(const ezVec3& vGlobalPosition) const
{
  ezVec3 vLocal;
  if (m_fRadius <= 0.0f || !GetLocalPosition(vGlobalPosition, vLocal))
    return 0.0f;
  return CalculateWeight(m_fRadius - vLocal.GetLength());
}

void ezMiniAudioSoundSphereComponent::OnUpdateLocalBounds(ezMsgUpdateLocalBounds& ref_msg) const
{
  ref_msg.AddBounds(ezBoundingSphere::MakeFromCenterAndRadius(ezVec3::MakeZero(), m_fRadius), ezInvalidSpatialDataCategory);
}

void ezMiniAudioSoundSphereComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  inout_stream.GetStream() << m_fRadius;
}

void ezMiniAudioSoundSphereComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  inout_stream.GetStream() >> m_fRadius;
  SetRadius(m_fRadius);
}

EZ_STATICLINK_FILE(MiniAudioPlugin, MiniAudioPlugin_Components_MiniAudioSoundVolumeComponent);
