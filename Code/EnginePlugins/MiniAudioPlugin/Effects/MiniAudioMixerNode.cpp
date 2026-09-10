#include <MiniAudioPlugin/Effects/MiniAudioMixerNode.h>
#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

ezResult ezMiniAudioMixerNode::Initialize(ma_engine* engine)
{
  m_uiChannels = ma_engine_get_channels(engine);
  m_uiRate = ma_engine_get_sample_rate(engine);
  ezUInt32 channels[MaxGroups];
  for (ezUInt32 i = 0; i < MaxGroups; ++i)
  {
    channels[i] = m_uiChannels;
    m_Gains[i] = 1;
  }
  static const ma_node_vtable vtable = {Process, nullptr, MaxGroups, 1, MA_NODE_FLAG_CONTINUOUS_PROCESSING};
  auto config = ma_node_config_init();
  config.vtable = &vtable;
  config.pInputChannels = channels;
  config.pOutputChannels = &m_uiChannels;
  if (ma_node_init(ma_engine_get_node_graph(engine), &config, nullptr, &m_Node) != MA_SUCCESS)
    return EZ_FAILURE;
  m_bInitialized = true;
  return ma_node_attach_output_bus(&m_Node, 0, ma_engine_get_endpoint(engine), 0) == MA_SUCCESS ? EZ_SUCCESS : EZ_FAILURE;
}
ezMiniAudioMixerNode::~ezMiniAudioMixerNode()
{
  if (m_bInitialized)
    ma_node_uninit(&m_Node, nullptr);
}
void ezMiniAudioMixerNode::Configure(ezArrayPtr<const ezString> groups, ezArrayPtr<const ezMiniAudioDucker> duckers)
{
  ezDynamicArray<Rule> rules;
  auto clamp = [](float v, float lo, float hi, float fallback)
  { return ezMath::IsFinite(v) ? ezMath::Clamp(v, lo, hi) : fallback; };
  for (const auto& ducker : duckers)
  {
    Rule rule;
    if (ducker.m_bEnabled)
    {
      for (ezUInt32 i = 0; i < groups.GetCount() && i < MaxGroups; ++i)
      {
        if (ducker.m_SourceGroups.Contains(groups[i]))
          rule.m_Sources |= ezUInt64(1) << i;
        if (ducker.m_TargetGroups.Contains(groups[i]))
          rule.m_Targets |= ezUInt64(1) << i;
      }
    }
    rule.m_fThreshold = clamp(ducker.m_fThreshold, 0, 100, 10) * 0.01f;
    rule.m_fReduction = clamp(ducker.m_fReduction, 0, 100, 80) * 0.01f;
    rule.m_fAttackStep = 1.0f / ezMath::Max(1.0f, clamp(ducker.m_fAttack, 0, 2000, 10) * 0.001f * m_uiRate);
    rule.m_fReleaseStep = 1.0f / ezMath::Max(1.0f, clamp(ducker.m_fRelease, 0, 10000, 250) * 0.001f * m_uiRate);
    rules.PushBack(rule);
  }
  EZ_LOCK(m_Mutex);
  for (ezUInt32 i = 0; i < ezMath::Min(rules.GetCount(), m_Rules.GetCount()); ++i)
  {
    if (rules[i].m_Sources == m_Rules[i].m_Sources && rules[i].m_Targets == m_Rules[i].m_Targets)
      rules[i].m_fEnvelope = m_Rules[i].m_fEnvelope;
  }
  m_Rules.Swap(rules);
}
void ezMiniAudioMixerNode::Process(ma_node* node, const float** inputs, ma_uint32* inputFrames, float** outputs, ma_uint32* outputFrames)
{
  auto& self = *reinterpret_cast<ezMiniAudioMixerNode*>(node);
  const ezUInt32 count = ezMath::Min(*inputFrames, *outputFrames);
  float peaks[MaxGroups] = {};
  for (ezUInt32 g = 0; g < MaxGroups; ++g)
  {
    if (inputs && inputs[g])
      for (ezUInt32 i = 0; i < count * self.m_uiChannels; ++i)
        peaks[g] = ezMath::Max(peaks[g], ezMath::Abs(inputs[g][i]));
  }
  const bool locked = self.m_Mutex.TryLock().Succeeded();
  for (ezUInt32 f = 0; f < *outputFrames; ++f)
  {
    if (locked)
    {
      for (auto& gain : self.m_Gains)
        gain = 1;
      for (auto& rule : self.m_Rules)
      {
        float peak = 0;
        for (ezUInt32 g = 0; g < MaxGroups; ++g)
          if (rule.m_Sources & (ezUInt64(1) << g))
            peak = ezMath::Max(peak, peaks[g]);
        const bool active = peak > rule.m_fThreshold;
        rule.m_fEnvelope = ezMath::Saturate(rule.m_fEnvelope + (active ? rule.m_fAttackStep : -rule.m_fReleaseStep));
        const float gain = 1 - rule.m_fReduction * rule.m_fEnvelope;
        for (ezUInt32 g = 0; g < MaxGroups; ++g)
          if (rule.m_Targets & (ezUInt64(1) << g))
            self.m_Gains[g] = ezMath::Min(self.m_Gains[g], gain);
      }
    }
    for (ezUInt32 c = 0; c < self.m_uiChannels; ++c)
    {
      float value = 0;
      if (f < count && inputs)
        for (ezUInt32 g = 0; g < MaxGroups; ++g)
          if (inputs[g])
            value += inputs[g][f * self.m_uiChannels + c] * self.m_Gains[g];
      outputs[0][f * self.m_uiChannels + c] = value;
    }
  }
  if (locked)
    self.m_Mutex.Unlock();
  *inputFrames = count;
}
