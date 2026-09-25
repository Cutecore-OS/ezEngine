#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <MiniAudioPlugin/Effects/MiniAudioEffectNode.h>
#include <cmath>

struct ezMiniAudioEffectNode::Stage
{
  ezMiniAudioEffect m_Settings;
  float m_fWeight = 0;
  float m_fTargetWeight = 0;
  float m_fPhase = 0;
  float m_fPhaseStep = 0;
  float m_fWeightStep = 0;
  float m_fSampleRate = 0;
  float m_fB0 = 1, m_fB1 = 0, m_fB2 = 0, m_fA1 = 0, m_fA2 = 0;
  ezDynamicArray<float> m_Z1, m_Z2, m_Held;
  float m_fEnvelope = 0;
  ezUInt32 m_uiHold = 0;

  struct DelayLine
  {
    ezDynamicArray<float> m_Buffer;
    ezDynamicArray<float> m_Damped;
    ezUInt32 m_uiFrames = 0;
    ezUInt32 m_uiCursor = 0;
    float m_fFeedback = 0;

    void Initialize(ezUInt32 uiFrames, ezUInt32 uiChannels)
    {
      m_uiFrames = ezMath::Max(2u, uiFrames);
      m_Buffer.SetCount(m_uiFrames * uiChannels, 0.0f);
      m_Damped.SetCount(uiChannels, 0.0f);
    }

    float Read(float fDelay, ezUInt32 uiChannel, ezUInt32 uiChannels) const
    {
      float fPosition = static_cast<float>(m_uiCursor) - fDelay;
      if (fPosition < 0)
        fPosition += static_cast<float>(m_uiFrames);
      const ezUInt32 uiFirst = static_cast<ezUInt32>(fPosition) % m_uiFrames;
      const ezUInt32 uiSecond = (uiFirst + 1) % m_uiFrames;
      return ezMath::Lerp(m_Buffer[uiFirst * uiChannels + uiChannel], m_Buffer[uiSecond * uiChannels + uiChannel],
        fPosition - std::floor(fPosition));
    }
  };

  DelayLine m_Lines[6];

  void Initialize(const ezMiniAudioEffectInstance& effect, ezUInt32 uiSampleRate, ezUInt32 uiChannels)
  {
    m_Settings = effect.m_Effect;
    m_fWeight = m_fTargetWeight = m_Settings.m_bEnabled ? ezMath::Saturate(effect.m_fWeight) : 0.0f;
    m_fSampleRate = static_cast<float>(uiSampleRate);
    m_fWeightStep = 1.0f / (0.05f * m_fSampleRate);
    m_fPhaseStep = 2.0f * ezMath::Pi<float>() * m_Settings.m_fRate / m_fSampleRate;
    m_Z1.SetCount(uiChannels, 0.0f);
    m_Z2.SetCount(uiChannels, 0.0f);
    m_Held.SetCount(uiChannels, 0.0f);

    const auto type = m_Settings.m_Type;
    if (type == ezMiniAudioEffectType::Delay)
    {
      m_Lines[0].Initialize(static_cast<ezUInt32>(m_Settings.m_fDelay * 0.001f * m_fSampleRate) + 2, uiChannels);
    }
    else if (type == ezMiniAudioEffectType::Chorus || type == ezMiniAudioEffectType::Flanger)
    {
      m_Lines[0].Initialize(static_cast<ezUInt32>(0.05f * m_fSampleRate) + 2, uiChannels);
    }
    else if (type == ezMiniAudioEffectType::Reverb)
    {
      // Parallel damped combs followed by two all-pass diffusers (Schroeder topology).
      const float delays[] = {0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.005f, 0.0017f};
      for (ezUInt32 i = 0; i < 6; ++i)
      {
        const float fSeconds = delays[i] * (0.5f + m_Settings.m_fRoomSize);
        m_Lines[i].Initialize(static_cast<ezUInt32>(fSeconds * m_fSampleRate), uiChannels);
        m_Lines[i].m_fFeedback = std::pow(0.001f, fSeconds / m_Settings.m_fDecay);
      }
    }
    else if (type == ezMiniAudioEffectType::Muffling || type == ezMiniAudioEffectType::HighPass || type == ezMiniAudioEffectType::LowPass || type == ezMiniAudioEffectType::BandPass || type == ezMiniAudioEffectType::ParametricEqualizer)
    {
      // Normalized biquad coefficients; cutoff stays below Nyquist at any device rate.
      const float w = 2.0f * ezMath::Pi<float>() * ezMath::Min(m_Settings.m_fFrequency, 0.45f * m_fSampleRate) / m_fSampleRate;
      const float c = std::cos(w);
      const float alpha = std::sin(w) / (2.0f * m_Settings.m_fQ);
      float a0 = 1 + alpha;
      m_fA1 = -2 * c;
      m_fA2 = 1 - alpha;
      if (type == ezMiniAudioEffectType::LowPass || type == ezMiniAudioEffectType::Muffling)
      {
        m_fB0 = m_fB2 = (1 - c) * 0.5f;
        m_fB1 = 1 - c;
      }
      else if (type == ezMiniAudioEffectType::HighPass)
      {
        m_fB0 = m_fB2 = (1 + c) * 0.5f;
        m_fB1 = -(1 + c);
      }
      else if (type == ezMiniAudioEffectType::BandPass)
      {
        m_fB0 = alpha;
        m_fB1 = 0;
        m_fB2 = -alpha;
      }
      else
      {
        const float a = std::pow(10.0f, m_Settings.m_fGain / 40.0f);
        a0 = 1 + alpha / a;
        m_fA2 = 1 - alpha / a;
        m_fB0 = 1 + alpha * a;
        m_fB1 = -2 * c;
        m_fB2 = 1 - alpha * a;
      }
      m_fB0 /= a0;
      m_fB1 /= a0;
      m_fB2 /= a0;
      m_fA1 /= a0;
      m_fA2 /= a0;
    }
  }

  float GetTailSeconds() const
  {
    if (!m_Settings.m_bEnabled || m_Settings.m_fMix == 0)
      return 0;
    switch (m_Settings.m_Type)
    {
      case ezMiniAudioEffectType::Reverb:
        return m_Settings.m_fDecay + 0.2f;
      case ezMiniAudioEffectType::Delay:
        return m_Settings.m_fDelay * 0.001f * (1.0f + (m_Settings.m_fFeedback > 0 ? std::log(0.001f) / std::log(m_Settings.m_fFeedback) : 0));
      case ezMiniAudioEffectType::Chorus:
      case ezMiniAudioEffectType::Flanger:
        return 0.05f * (1.0f + (m_Settings.m_fFeedback > 0 ? std::log(0.001f) / std::log(m_Settings.m_fFeedback) : 0));
      case ezMiniAudioEffectType::Speed:
      case ezMiniAudioEffectType::Pitch:
      case ezMiniAudioEffectType::TimeStretch:
      case ezMiniAudioEffectType::Volume:
      case ezMiniAudioEffectType::Panner:
      case ezMiniAudioEffectType::Distortion:
      case ezMiniAudioEffectType::Bitcrusher:
      case ezMiniAudioEffectType::Compressor:
      case ezMiniAudioEffectType::Limiter:
        return 0;
      default:
        return 10.0f * m_Settings.m_fQ / m_Settings.m_fFrequency * std::pow(10.0f, ezMath::Abs(m_Settings.m_fGain) / 40.0f) + 0.05f;
    }
  }

  void Process(float* pSamples, ezUInt32 uiFrames, ezUInt32 uiChannels)
  {
    if (m_Settings.IsSourceEffect())
      return; // Applied to the source resampler, before spatialization.

    for (ezUInt32 f = 0; f < uiFrames; ++f)
    {
      m_fWeight += ezMath::Clamp(m_fTargetWeight - m_fWeight, -m_fWeightStep, m_fWeightStep);
      const bool bDirect = m_Settings.m_Type == ezMiniAudioEffectType::Volume || m_Settings.m_Type == ezMiniAudioEffectType::Panner || m_Settings.m_Type == ezMiniAudioEffectType::Compressor || m_Settings.m_Type == ezMiniAudioEffectType::Limiter;
      const float mix = (bDirect ? 1.0f : m_Settings.m_fMix) * m_fWeight;
      float gain = 1.0f;
      if (m_Settings.m_Type == ezMiniAudioEffectType::Compressor || m_Settings.m_Type == ezMiniAudioEffectType::Limiter)
      {
        float peak = 0;
        for (ezUInt32 c = 0; c < uiChannels; ++c)
          peak = ezMath::Max(peak, ezMath::Abs(pSamples[f * uiChannels + c]));
        const bool limiter = m_Settings.m_Type == ezMiniAudioEffectType::Limiter;
        const float ms = peak > m_fEnvelope ? (limiter ? 0.0f : m_Settings.m_fAttack) : m_Settings.m_fRelease;
        const float coefficient = ms > 0 ? std::exp(-1.0f / (ms * 0.001f * m_fSampleRate)) : 0.0f;
        m_fEnvelope = peak + coefficient * (m_fEnvelope - peak);
        const float level = 20.0f * std::log10(ezMath::Max(m_fEnvelope, 0.000001f));
        if (level > m_Settings.m_fThreshold)
          gain = std::pow(10.0f, (m_Settings.m_fThreshold - level) * (limiter ? 1.0f : 1.0f - 1.0f / m_Settings.m_fRatio) / 20.0f);
      }
      for (ezUInt32 c = 0; c < uiChannels; ++c)
      {
        float& sample = pSamples[f * uiChannels + c];
        const float input = sample;
        float wet = input;
        switch (m_Settings.m_Type)
        {
          case ezMiniAudioEffectType::Volume:
            wet = input * m_Settings.m_fVolume;
            break;
          case ezMiniAudioEffectType::Panner:
            if (uiChannels >= 2 && c < 2)
              wet = input * (1.0f - ezMath::Max(0.0f, c == 0 ? m_Settings.m_fPan : -m_Settings.m_fPan));
            break;
          case ezMiniAudioEffectType::Compressor:
          case ezMiniAudioEffectType::Limiter:
            wet = input * gain;
            break;
          case ezMiniAudioEffectType::Distortion:
            wet = std::tanh(input * m_Settings.m_fDrive) / std::tanh(m_Settings.m_fDrive);
            break;
          case ezMiniAudioEffectType::Bitcrusher:
          {
            const float steps = std::pow(2.0f, static_cast<float>(m_Settings.m_uiBits) - 1.0f);
            if (m_uiHold == 0)
              m_Held[c] = std::round(ezMath::Clamp(input, -1.0f, 1.0f) * steps) / steps;
            wet = m_Held[c];
            break;
          }
          case ezMiniAudioEffectType::Delay:
          case ezMiniAudioEffectType::Chorus:
          case ezMiniAudioEffectType::Flanger:
          {
            auto& line = m_Lines[0];
            float delay = m_Settings.m_fDelay * 0.001f * m_fSampleRate;
            if (m_Settings.m_Type != ezMiniAudioEffectType::Delay)
            {
              const bool chorus = m_Settings.m_Type == ezMiniAudioEffectType::Chorus;
              const float modulation = std::sin(m_fPhase + static_cast<float>(c) * 0.7f);
              delay = m_fSampleRate * ((chorus ? 0.02f : 0.003f) + modulation * m_Settings.m_fDepth * (chorus ? 0.01f : 0.0025f));
            }
            wet = line.Read(delay, c, uiChannels);
            // A bypassed zone does not accumulate audio that would echo on subsequent entry.
            line.m_Buffer[line.m_uiCursor * uiChannels + c] = (m_fWeight > 0 ? input : 0) + wet * m_Settings.m_fFeedback;
            break;
          }
          case ezMiniAudioEffectType::Reverb:
          {
            wet = 0;
            for (ezUInt32 i = 0; i < 4; ++i)
            {
              auto& line = m_Lines[i];
              float& delayed = line.m_Buffer[line.m_uiCursor * uiChannels + c];
              const float value = delayed;
              const float damping = m_Settings.m_fDamping * 0.95f;
              line.m_Damped[c] = value * (1 - damping) + line.m_Damped[c] * damping;
              delayed = (m_fWeight > 0 ? input : 0) + line.m_Damped[c] * line.m_fFeedback;
              wet += value * 0.25f;
            }
            for (ezUInt32 i = 4; i < 6; ++i)
            {
              auto& line = m_Lines[i];
              float& delayed = line.m_Buffer[line.m_uiCursor * uiChannels + c];
              const float value = delayed;
              delayed = wet + value * 0.5f;
              wet = value - delayed * 0.5f;
            }
            break;
          }
          default:
            // Transposed direct form II, with independent state for every output channel.
            wet = m_fB0 * input + m_Z1[c];
            m_Z1[c] = m_fB1 * input - m_fA1 * wet + m_Z2[c];
            m_Z2[c] = m_fB2 * input - m_fA2 * wet;
            break;
        }
        if (m_Settings.m_Type == ezMiniAudioEffectType::Muffling)
          wet *= m_Settings.m_fVolume;
        sample = ezMath::Lerp(input, wet, mix);
      }
      for (auto& line : m_Lines)
      {
        if (line.m_uiFrames > 0)
          line.m_uiCursor = (line.m_uiCursor + 1) % line.m_uiFrames;
      }
      m_uiHold = (m_uiHold + 1) % m_Settings.m_uiDownsample;
      m_fPhase += m_fPhaseStep;
      if (m_fPhase >= 2.0f * ezMath::Pi<float>())
        m_fPhase -= 2.0f * ezMath::Pi<float>();
    }
  }
};

ezMiniAudioEffectNode::ezMiniAudioEffectNode() = default;

ezMiniAudioEffectNode::~ezMiniAudioEffectNode()
{
  if (m_bInitialized)
    ma_node_uninit(&m_Node, nullptr);
}

ezResult ezMiniAudioEffectNode::Initialize(ma_engine* pEngine, ma_node* pDestination)
{
  m_uiChannels = ma_engine_get_channels(pEngine);
  m_uiSampleRate = ma_engine_get_sample_rate(pEngine);
  static const ma_node_vtable vtable = {Process, nullptr, 1, 1, MA_NODE_FLAG_CONTINUOUS_PROCESSING};
  ma_node_config config = ma_node_config_init();
  config.vtable = &vtable;
  config.pInputChannels = &m_uiChannels;
  config.pOutputChannels = &m_uiChannels;
  if (ma_node_init(ma_engine_get_node_graph(pEngine), &config, nullptr, &m_Node) != MA_SUCCESS)
    return EZ_FAILURE;
  m_bInitialized = true;
  return ma_node_attach_output_bus(&m_Node, 0, pDestination, 0) == MA_SUCCESS ? EZ_SUCCESS : EZ_FAILURE;
}

void ezMiniAudioEffectNode::Configure(ezArrayPtr<const ezMiniAudioEffectInstance> effects)
{
  bool bRebuild = effects.GetCount() != m_Stages.GetCount();
  for (ezUInt32 i = 0; !bRebuild && i < effects.GetCount(); ++i)
    bRebuild = !(m_Stages[i]->m_Settings == effects[i].m_Effect);

  if (bRebuild)
  {
    ezDynamicArray<ezUniquePtr<Stage>> stages;
    float tail = 0;
    for (const auto& effect : effects)
    {
      auto stage = EZ_DEFAULT_NEW(Stage);
      stage->Initialize(effect, m_uiSampleRate, m_uiChannels);
      tail += stage->GetTailSeconds();
      stages.PushBack(std::move(stage));
    }
    {
      EZ_LOCK(m_Mutex);
      m_Stages.Swap(stages);
      m_fTailSeconds = tail;
    }
    // Retire the old buffers here, never in the audio callback.
  }
  else
  {
    EZ_LOCK(m_Mutex);
    for (ezUInt32 i = 0; i < effects.GetCount(); ++i)
      m_Stages[i]->m_fTargetWeight = effects[i].m_Effect.m_bEnabled ? ezMath::Saturate(effects[i].m_fWeight) : 0.0f;
  }
}

void ezMiniAudioEffectNode::Process(ma_node* pNode, const float** pInputs, ma_uint32* pInputFrames, float** pOutputs, ma_uint32* pOutputFrames)
{
  auto* pThis = reinterpret_cast<ezMiniAudioEffectNode*>(pNode);
  const ezUInt32 uiFrames = *pOutputFrames;
  const ezUInt32 uiInputFrames = pInputs && pInputs[0] ? ezMath::Min(*pInputFrames, uiFrames) : 0;
  float* pOutput = pOutputs[0];
  if (uiInputFrames > 0)
    ezMemoryUtils::Copy(pOutput, pInputs[0], uiInputFrames * pThis->m_uiChannels);
  ezMemoryUtils::ZeroFill(pOutput + uiInputFrames * pThis->m_uiChannels, (uiFrames - uiInputFrames) * pThis->m_uiChannels);
  *pInputFrames = uiInputFrames;
  if (pThis->m_Mutex.TryLock().Succeeded())
  {
    for (auto& stage : pThis->m_Stages)
      stage->Process(pOutput, uiFrames, pThis->m_uiChannels);
    pThis->m_Mutex.Unlock();
  }
}
