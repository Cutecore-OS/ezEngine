#pragma once

#include <Foundation/Containers/DynamicArray.h>
#include <MiniAudio/miniaudio.h>
#include <MiniAudioPlugin/MiniAudioPluginDLL.h>
#include <atomic>

/// Streaming WSOLA: linked channels, 20 ms windows, 10 ms synthesis hops.
/// Buffers are allocated before playback. A tempo of one uses the decoder directly.
struct EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioTimeStretch
{
  ma_data_source_base m_Source;
  ezResult Initialize(ma_decoder* pDecoder);
  ~ezMiniAudioTimeStretch();
  void SetTempo(float fTempo);

private:
  static ma_result Read(ma_data_source*, void*, ma_uint64, ma_uint64*);
  static ma_result Seek(ma_data_source*, ma_uint64);
  static ma_result Format(ma_data_source*, ma_format*, ma_uint32*, ma_uint32*, ma_channel*, size_t);
  static ma_result Cursor(ma_data_source*, ma_uint64*);
  static ma_result Length(ma_data_source*, ma_uint64*);
  bool Generate(float fTempo);

  ma_decoder* m_pDecoder = nullptr;
  std::atomic<float> m_fTempo{1.0f};
  ezUInt32 m_uiChannels = 0, m_uiRate = 0, m_uiHop = 0, m_uiSearch = 0;
  ma_uint64 m_uiLength = 0;
  double m_fPosition = 0;
  ezUInt32 m_uiQueued = 0, m_uiRead = 0;
  bool m_bTail = false, m_bInitialized = false;
  ezDynamicArray<float> m_Input, m_Tail, m_Output;
};
