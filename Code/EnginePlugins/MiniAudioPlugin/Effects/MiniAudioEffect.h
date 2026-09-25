#pragma once

#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Reflection/Reflection.h>
#include <MiniAudioPlugin/MiniAudioPluginDLL.h>

struct ezMiniAudioEffectType
{
  using StorageType = ezUInt8;
  enum Enum
  {
    Reverb,
    Speed,
    Delay,
    ParametricEqualizer,
    Chorus,
    Flanger,
    HighPass,
    LowPass,
    Volume,
    Pitch,
    TimeStretch,
    Compressor,
    Limiter,
    Distortion,
    Bitcrusher,
    BandPass,
    Panner,
    Muffling,
    Default = Reverb
  };
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_MINIAUDIOPLUGIN_DLL, ezMiniAudioEffectType);

/// One entry in an ordered audio effect chain. Mix and feedback are normalized (0..1).
/// Speed changes both playback speed and pitch, like a tape speed control.
struct EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioEffect
{
  ezEnum<ezMiniAudioEffectType> m_Type;
  bool m_bEnabled = true;
  float m_fMix = 0.5f;
  float m_fDelay = 500.0f;      ///< Milliseconds.
  float m_fFeedback = 0.5f;
  float m_fDecay = 2.0f;        ///< Reverb RT60 in seconds.
  float m_fRoomSize = 0.5f;
  float m_fDamping = 0.5f;
  float m_fRate = 0.5f;         ///< Modulation frequency in Hz.
  float m_fDepth = 0.5f;
  float m_fFrequency = 1000.0f; ///< Filter center / cutoff in Hz.
  float m_fQ = 0.707f;
  float m_fGain = 0.0f;         ///< Equalizer gain in dB.
  float m_fSpeed = 1.0f;

  float m_fVolume = 1.0f;
  float m_fPitch = 0.0f;       ///< Semitones, preserving duration.
  float m_fThreshold = -18.0f; ///< dBFS.
  float m_fRatio = 4.0f;
  float m_fAttack = 10.0f;     ///< Milliseconds.
  float m_fRelease = 100.0f;
  float m_fDrive = 2.0f;
  ezUInt32 m_uiBits = 8;
  ezUInt32 m_uiDownsample = 1;
  float m_fPan = 0.0f;

  bool IsSourceEffect() const { return m_Type == ezMiniAudioEffectType::Speed || m_Type == ezMiniAudioEffectType::Pitch || m_Type == ezMiniAudioEffectType::TimeStretch; }
  void Sanitize();
  bool operator==(const ezMiniAudioEffect& other) const;
  ezResult Serialize(ezStreamWriter& inout_stream) const;
  ezResult Deserialize(ezStreamReader& inout_stream);
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_MINIAUDIOPLUGIN_DLL, ezMiniAudioEffect);

/// Runtime contribution of an effect. Weight is independent of the authored settings.
struct ezMiniAudioEffectInstance
{
  ezMiniAudioEffect m_Effect;
  float m_fWeight = 1.0f;
};

/// Listener effect on one bus. Empty Group applies to every group.
struct EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioGroupEffect : public ezMiniAudioEffect
{
  ezString m_sGroup;
  ezResult Serialize(ezStreamWriter& stream) const;
  ezResult Deserialize(ezStreamReader& stream);
};
EZ_DECLARE_REFLECTABLE_TYPE(EZ_MINIAUDIOPLUGIN_DLL, ezMiniAudioGroupEffect);

struct EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioDucker
{
  bool m_bEnabled = true;
  ezDynamicArray<ezString> m_SourceGroups;
  ezDynamicArray<ezString> m_TargetGroups;
  float m_fThreshold = 10.0f; ///< Percent of full scale (peak, before ducking).
  float m_fReduction = 80.0f; ///< Percent reduction.
  float m_fAttack = 10.0f;
  float m_fRelease = 250.0f;
  ezResult Serialize(ezStreamWriter& stream) const;
  ezResult Deserialize(ezStreamReader& stream);
};
EZ_DECLARE_REFLECTABLE_TYPE(EZ_MINIAUDIOPLUGIN_DLL, ezMiniAudioDucker);
