#include <MiniAudioPlugin/Effects/MiniAudioTimeStretch.h>
#include <MiniAudioPlugin/MiniAudioPluginPCH.h>
#include <cmath>

ezResult ezMiniAudioTimeStretch::Initialize(ma_decoder* pDecoder)
{
  m_pDecoder = pDecoder;
  ma_format format;
  if (ma_decoder_get_data_format(pDecoder, &format, &m_uiChannels, &m_uiRate, nullptr, 0) != MA_SUCCESS || format != ma_format_f32 ||
      ma_decoder_get_length_in_pcm_frames(pDecoder, &m_uiLength) != MA_SUCCESS || m_uiChannels == 0 || m_uiRate == 0)
    return EZ_FAILURE;
  m_uiHop = ezMath::Max(16u, m_uiRate / 100);
  m_uiSearch = m_uiHop / 2;
  m_Input.SetCount((2 * m_uiHop + 2 * m_uiSearch) * m_uiChannels, 0.0f);
  m_Tail.SetCount(m_uiHop * m_uiChannels, 0.0f);
  m_Output.SetCount(m_uiHop * m_uiChannels, 0.0f);
  static const ma_data_source_vtable vtable = {Read, Seek, Format, Cursor, Length, nullptr, 0};
  auto config = ma_data_source_config_init();
  config.vtable = &vtable;
  m_bInitialized = ma_data_source_init(&config, &m_Source) == MA_SUCCESS;
  return m_bInitialized ? EZ_SUCCESS : EZ_FAILURE;
}

ezMiniAudioTimeStretch::~ezMiniAudioTimeStretch()
{
  if (m_bInitialized)
    ma_data_source_uninit(&m_Source);
}

void ezMiniAudioTimeStretch::SetTempo(float fTempo)
{
  m_fTempo.store(ezMath::IsFinite(fTempo) ? ezMath::Clamp(fTempo, 0.025f, 16.0f) : 1.0f, std::memory_order_relaxed);
}

bool ezMiniAudioTimeStretch::Generate(float fTempo)
{
  if (m_fPosition >= static_cast<double>(m_uiLength))
    return false;
  const ma_uint64 expected = static_cast<ma_uint64>(m_fPosition);
  const ma_uint64 start = expected > m_uiSearch ? expected - m_uiSearch : 0;
  const ezUInt32 center = static_cast<ezUInt32>(expected - start);
  const ezUInt32 frames = 2 * m_uiHop + 2 * m_uiSearch;
  ezMemoryUtils::ZeroFill(m_Input.GetData(), m_Input.GetCount());
  if (ma_decoder_seek_to_pcm_frame(m_pDecoder, start) != MA_SUCCESS)
    return false;
  ma_uint64 read = 0;
  ma_decoder_read_pcm_frames(m_pDecoder, m_Input.GetData(), frames, &read);
  if (read == 0)
    return false;
  ezUInt32 best = center;
  if (m_bTail)
  {
    double bestScore = -2.0;
    const ezUInt32 maxOffset = static_cast<ezUInt32>(ezMath::Min<ma_uint64>(2 * m_uiSearch, read > 2 * m_uiHop ? read - 2 * m_uiHop : 0));
    for (ezUInt32 offset = 0; offset <= maxOffset; offset += 4)
    {
      double dot = 0, a2 = 1e-20, b2 = 1e-20;
      for (ezUInt32 f = 0; f < m_uiHop; f += 8)
      {
        for (ezUInt32 c = 0; c < m_uiChannels; ++c)
        {
          const float a = m_Tail[f * m_uiChannels + c];
          const float b = m_Input[(f + offset) * m_uiChannels + c];
          dot += a * b;
          a2 += a * a;
          b2 += b * b;
        }
      }
      const double score = dot / std::sqrt(a2 * b2) - std::abs(static_cast<int>(offset) - static_cast<int>(center)) * 1e-7;
      if (score > bestScore)
      {
        bestScore = score;
        best = offset;
      }
    }
  }
  for (ezUInt32 f = 0; f < m_uiHop; ++f)
  {
    const float t = static_cast<float>(f) / m_uiHop;
    for (ezUInt32 c = 0; c < m_uiChannels; ++c)
    {
      const auto i = f * m_uiChannels + c;
      const float sample = m_Input[(best + f) * m_uiChannels + c];
      m_Output[i] = m_bTail ? ezMath::Lerp(m_Tail[i], sample, t) : sample;
      m_Tail[i] = m_Input[(best + m_uiHop + f) * m_uiChannels + c];
    }
  }
  m_uiQueued = static_cast<ezUInt32>(ezMath::Min(static_cast<double>(m_uiHop), std::ceil((m_uiLength - m_fPosition) / fTempo)));
  m_uiRead = 0;
  m_fPosition += static_cast<double>(m_uiQueued) * fTempo;
  m_bTail = true;
  return m_uiQueued > 0;
}

ma_result ezMiniAudioTimeStretch::Read(ma_data_source* source, void* output, ma_uint64 count, ma_uint64* read)
{
  auto& self = *reinterpret_cast<ezMiniAudioTimeStretch*>(source);
  *read = 0;
  float* dst = static_cast<float*>(output);
  while (*read < count)
  {
    const float tempo = self.m_fTempo.load(std::memory_order_relaxed);
    if (self.m_uiRead == self.m_uiQueued)
    {
      if (tempo == 1.0f)
      {
        if (self.m_bTail)
          ma_decoder_seek_to_pcm_frame(self.m_pDecoder, static_cast<ma_uint64>(self.m_fPosition));
        self.m_bTail = false;
        ma_uint64 frames = 0;
        const auto result = ma_decoder_read_pcm_frames(self.m_pDecoder, dst ? dst + *read * self.m_uiChannels : nullptr, count - *read, &frames);
        *read += frames;
        self.m_fPosition += frames;
        return *read > 0 ? MA_SUCCESS : result;
      }
      if (!self.Generate(tempo))
        break;
    }
    const ezUInt32 frames = static_cast<ezUInt32>(ezMath::Min<ma_uint64>(count - *read, self.m_uiQueued - self.m_uiRead));
    if (dst)
      ezMemoryUtils::Copy(dst + *read * self.m_uiChannels, self.m_Output.GetData() + self.m_uiRead * self.m_uiChannels, frames * self.m_uiChannels);
    self.m_uiRead += frames;
    *read += frames;
  }
  return *read > 0 ? MA_SUCCESS : MA_AT_END;
}

ma_result ezMiniAudioTimeStretch::Seek(ma_data_source* source, ma_uint64 frame)
{
  auto& self = *reinterpret_cast<ezMiniAudioTimeStretch*>(source);
  self.m_fPosition = static_cast<double>(frame);
  self.m_uiQueued = self.m_uiRead = 0;
  self.m_bTail = false;
  return ma_decoder_seek_to_pcm_frame(self.m_pDecoder, frame);
}
ma_result ezMiniAudioTimeStretch::Format(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* rate, ma_channel* map, size_t capacity)
{
  return ma_decoder_get_data_format(reinterpret_cast<ezMiniAudioTimeStretch*>(source)->m_pDecoder, format, channels, rate, map, capacity);
}
ma_result ezMiniAudioTimeStretch::Cursor(ma_data_source* source, ma_uint64* cursor)
{
  auto& self = *reinterpret_cast<ezMiniAudioTimeStretch*>(source);
  *cursor = ezMath::Min(self.m_uiLength, static_cast<ma_uint64>(self.m_fPosition));
  return MA_SUCCESS;
}
ma_result ezMiniAudioTimeStretch::Length(ma_data_source* source, ma_uint64* length)
{
  *length = reinterpret_cast<ezMiniAudioTimeStretch*>(source)->m_uiLength;
  return MA_SUCCESS;
}
