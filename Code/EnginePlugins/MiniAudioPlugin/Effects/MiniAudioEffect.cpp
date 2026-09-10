#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <Foundation/IO/Stream.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffect.h>

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_ENUM(ezMiniAudioEffectType, 3)
  EZ_ENUM_CONSTANTS(ezMiniAudioEffectType::Reverb, ezMiniAudioEffectType::Speed, ezMiniAudioEffectType::Delay,
    ezMiniAudioEffectType::ParametricEqualizer, ezMiniAudioEffectType::Chorus, ezMiniAudioEffectType::Flanger,
    ezMiniAudioEffectType::HighPass, ezMiniAudioEffectType::LowPass)
  EZ_ENUM_CONSTANTS(ezMiniAudioEffectType::Volume, ezMiniAudioEffectType::Pitch,
    ezMiniAudioEffectType::TimeStretch, ezMiniAudioEffectType::Compressor, ezMiniAudioEffectType::Limiter, ezMiniAudioEffectType::Distortion,
    ezMiniAudioEffectType::Bitcrusher, ezMiniAudioEffectType::BandPass, ezMiniAudioEffectType::Panner, ezMiniAudioEffectType::Muffling)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_STATIC_REFLECTED_TYPE(ezMiniAudioEffect, ezNoBase, 2, ezRTTIDefaultAllocator<ezMiniAudioEffect>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ENUM_MEMBER_PROPERTY("Type", ezMiniAudioEffectType, m_Type),
    EZ_MEMBER_PROPERTY("Enabled", m_bEnabled)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_MEMBER_PROPERTY("Mix", m_fMix)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("Delay", m_fDelay)->AddAttributes(new ezDefaultValueAttribute(500.0f), new ezClampValueAttribute(10.0f, 5000.0f), new ezSuffixAttribute(" ms")),
    EZ_MEMBER_PROPERTY("Feedback", m_fFeedback)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 0.98f)),
    EZ_MEMBER_PROPERTY("Decay", m_fDecay)->AddAttributes(new ezDefaultValueAttribute(2.0f), new ezClampValueAttribute(0.1f, 20.0f), new ezSuffixAttribute(" s")),
    EZ_MEMBER_PROPERTY("RoomSize", m_fRoomSize)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("Damping", m_fDamping)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("Rate", m_fRate)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.01f, 20.0f), new ezSuffixAttribute(" Hz")),
    EZ_MEMBER_PROPERTY("Depth", m_fDepth)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("Frequency", m_fFrequency)->AddAttributes(new ezDefaultValueAttribute(1000.0f), new ezClampValueAttribute(20.0f, 20000.0f), new ezSuffixAttribute(" Hz")),
    EZ_MEMBER_PROPERTY("Q", m_fQ)->AddAttributes(new ezDefaultValueAttribute(0.707f), new ezClampValueAttribute(0.1f, 20.0f)),
    EZ_MEMBER_PROPERTY("Gain", m_fGain)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(-24.0f, 24.0f), new ezSuffixAttribute(" dB")),
    EZ_MEMBER_PROPERTY("Volume", m_fVolume)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 4.0f)),
    EZ_MEMBER_PROPERTY("Pitch", m_fPitch)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(-24.0f, 24.0f), new ezSuffixAttribute(" semitones")),
    EZ_MEMBER_PROPERTY("Threshold", m_fThreshold)->AddAttributes(new ezDefaultValueAttribute(-18.0f), new ezClampValueAttribute(-60.0f, 0.0f), new ezSuffixAttribute(" dBFS")),
    EZ_MEMBER_PROPERTY("Ratio", m_fRatio)->AddAttributes(new ezDefaultValueAttribute(4.0f), new ezClampValueAttribute(1.0f, 20.0f)),
    EZ_MEMBER_PROPERTY("AttackTime", m_fAttack)->AddAttributes(new ezDefaultValueAttribute(10.0f), new ezClampValueAttribute(0.0f, 2000.0f), new ezSuffixAttribute(" ms")),
    EZ_MEMBER_PROPERTY("ReleaseTime", m_fRelease)->AddAttributes(new ezDefaultValueAttribute(100.0f), new ezClampValueAttribute(0.0f, 10000.0f), new ezSuffixAttribute(" ms")),
    EZ_MEMBER_PROPERTY("Drive", m_fDrive)->AddAttributes(new ezDefaultValueAttribute(2.0f), new ezClampValueAttribute(1.0f, 50.0f)),
    EZ_MEMBER_PROPERTY("Bits", m_uiBits)->AddAttributes(new ezDefaultValueAttribute(8), new ezClampValueAttribute(1, 16)),
    EZ_MEMBER_PROPERTY("Downsample", m_uiDownsample)->AddAttributes(new ezDefaultValueAttribute(1), new ezClampValueAttribute(1, 64)),
    EZ_MEMBER_PROPERTY("Pan", m_fPan)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(-1.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("Speed", m_fSpeed)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.1f, 4.0f)),
  }
  EZ_END_PROPERTIES;
}
EZ_END_STATIC_REFLECTED_TYPE;
// clang-format on

void ezMiniAudioEffect::Sanitize()
{
  auto clamp = [](float& value, float min, float max, float fallback)
  { value = ezMath::IsFinite(value) ? ezMath::Clamp(value, min, max) : fallback; };
  if (m_Type.GetValue() > ezMiniAudioEffectType::Muffling)
    m_Type = ezMiniAudioEffectType::Default;
  clamp(m_fMix, 0, 1, 0.5f);
  clamp(m_fDelay, 10, 5000, 500);
  clamp(m_fFeedback, 0, 0.98f, 0.5f);
  clamp(m_fDecay, 0.1f, 20, 2);
  clamp(m_fRoomSize, 0, 1, 0.5f);
  clamp(m_fDamping, 0, 1, 0.5f);
  clamp(m_fRate, 0.01f, 20, 0.5f);
  clamp(m_fDepth, 0, 1, 0.5f);
  clamp(m_fFrequency, 20, 20000, 1000);
  clamp(m_fQ, 0.1f, 20, 0.707f);
  clamp(m_fGain, -24, 24, 0);
  clamp(m_fSpeed, 0.1f, 4, 1);
  clamp(m_fVolume, 0, 4, 1);
  clamp(m_fPitch, -24, 24, 0);
  clamp(m_fThreshold, -60, 0, -18);
  clamp(m_fRatio, 1, 20, 4);
  clamp(m_fAttack, 0, 2000, 10);
  clamp(m_fRelease, 0, 10000, 100);
  clamp(m_fDrive, 1, 50, 2);
  clamp(m_fPan, -1, 1, 0);
  m_uiBits = ezMath::Clamp(m_uiBits, 1u, 16u);
  m_uiDownsample = ezMath::Clamp(m_uiDownsample, 1u, 64u);
}

bool ezMiniAudioEffect::operator==(const ezMiniAudioEffect& o) const
{
  return m_Type == o.m_Type && m_bEnabled == o.m_bEnabled && m_fMix == o.m_fMix && m_fDelay == o.m_fDelay &&
         m_fFeedback == o.m_fFeedback && m_fDecay == o.m_fDecay && m_fRoomSize == o.m_fRoomSize &&
         m_fDamping == o.m_fDamping && m_fRate == o.m_fRate && m_fDepth == o.m_fDepth &&
         m_fFrequency == o.m_fFrequency && m_fQ == o.m_fQ && m_fGain == o.m_fGain && m_fSpeed == o.m_fSpeed &&
         m_fVolume == o.m_fVolume && m_fPitch == o.m_fPitch && m_fThreshold == o.m_fThreshold && m_fRatio == o.m_fRatio && m_fAttack == o.m_fAttack && m_fRelease == o.m_fRelease && m_fDrive == o.m_fDrive && m_uiBits == o.m_uiBits && m_uiDownsample == o.m_uiDownsample && m_fPan == o.m_fPan;
}

ezResult ezMiniAudioEffect::Serialize(ezStreamWriter& stream) const
{
  stream.WriteVersion(2);
  stream << m_Type << m_bEnabled << m_fMix << m_fDelay << m_fFeedback << m_fDecay << m_fRoomSize << m_fDamping;
  stream << m_fRate << m_fDepth << m_fFrequency << m_fQ << m_fGain << m_fSpeed;
  stream << m_fVolume << m_fPitch << m_fThreshold << m_fRatio << m_fAttack << m_fRelease << m_fDrive << m_uiBits << m_uiDownsample << m_fPan;
  return EZ_SUCCESS;
}

ezResult ezMiniAudioEffect::Deserialize(ezStreamReader& stream)
{
  const auto version = stream.ReadVersion(2);
  stream >> m_Type >> m_bEnabled >> m_fMix >> m_fDelay >> m_fFeedback >> m_fDecay >> m_fRoomSize >> m_fDamping;
  stream >> m_fRate >> m_fDepth >> m_fFrequency >> m_fQ >> m_fGain >> m_fSpeed;
  if (version >= 2)
    stream >> m_fVolume >> m_fPitch >> m_fThreshold >> m_fRatio >> m_fAttack >> m_fRelease >> m_fDrive >> m_uiBits >> m_uiDownsample >> m_fPan;
  Sanitize();
  return EZ_SUCCESS;
}

EZ_STATICLINK_FILE(MiniAudioPlugin, MiniAudioPlugin_Effects_MiniAudioEffect);

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_TYPE(ezMiniAudioGroupEffect, ezMiniAudioEffect, 1, ezRTTIDefaultAllocator<ezMiniAudioGroupEffect>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Group", m_sGroup)->AddAttributes(new ezDynamicStringEnumAttribute("MiniAudioSoundGroups")),
  }
  EZ_END_PROPERTIES;
}
EZ_END_STATIC_REFLECTED_TYPE;
EZ_BEGIN_STATIC_REFLECTED_TYPE(ezMiniAudioDucker, ezNoBase, 1, ezRTTIDefaultAllocator<ezMiniAudioDucker>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Enabled", m_bEnabled)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_ARRAY_MEMBER_PROPERTY("SourceGroups", m_SourceGroups)->AddAttributes(new ezDynamicStringEnumAttribute("MiniAudioSoundGroups")),
    EZ_ARRAY_MEMBER_PROPERTY("TargetGroups", m_TargetGroups)->AddAttributes(new ezDynamicStringEnumAttribute("MiniAudioSoundGroups")),
    EZ_MEMBER_PROPERTY("Threshold", m_fThreshold)->AddAttributes(new ezDefaultValueAttribute(10.0f), new ezClampValueAttribute(0.0f, 100.0f), new ezSuffixAttribute(" %")),
    EZ_MEMBER_PROPERTY("Reduction", m_fReduction)->AddAttributes(new ezDefaultValueAttribute(80.0f), new ezClampValueAttribute(0.0f, 100.0f), new ezSuffixAttribute(" %")),
    EZ_MEMBER_PROPERTY("AttackTime", m_fAttack)->AddAttributes(new ezDefaultValueAttribute(10.0f), new ezClampValueAttribute(0.0f, 2000.0f), new ezSuffixAttribute(" ms")),
    EZ_MEMBER_PROPERTY("ReleaseTime", m_fRelease)->AddAttributes(new ezDefaultValueAttribute(250.0f), new ezClampValueAttribute(0.0f, 10000.0f), new ezSuffixAttribute(" ms")),
  }
  EZ_END_PROPERTIES;
}
EZ_END_STATIC_REFLECTED_TYPE;
// clang-format on

ezResult ezMiniAudioGroupEffect::Serialize(ezStreamWriter& stream) const
{
  EZ_SUCCEED_OR_RETURN(ezMiniAudioEffect::Serialize(stream));
  stream << m_sGroup;
  return EZ_SUCCESS;
}
ezResult ezMiniAudioGroupEffect::Deserialize(ezStreamReader& stream)
{
  EZ_SUCCEED_OR_RETURN(ezMiniAudioEffect::Deserialize(stream));
  stream >> m_sGroup;
  return EZ_SUCCESS;
}
ezResult ezMiniAudioDucker::Serialize(ezStreamWriter& stream) const
{
  stream << m_bEnabled;
  EZ_SUCCEED_OR_RETURN(stream.WriteArray(m_SourceGroups));
  EZ_SUCCEED_OR_RETURN(stream.WriteArray(m_TargetGroups));
  stream << m_fThreshold << m_fReduction << m_fAttack << m_fRelease;
  return EZ_SUCCESS;
}
ezResult ezMiniAudioDucker::Deserialize(ezStreamReader& stream)
{
  stream >> m_bEnabled;
  EZ_SUCCEED_OR_RETURN(stream.ReadArray(m_SourceGroups));
  EZ_SUCCEED_OR_RETURN(stream.ReadArray(m_TargetGroups));
  stream >> m_fThreshold >> m_fReduction >> m_fAttack >> m_fRelease;
  return EZ_SUCCESS;
}
