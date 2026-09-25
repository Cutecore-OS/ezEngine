#pragma once
#include <Foundation/Basics.h>
#include <Foundation/Strings/String.h>
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct SwsContext;

/// Streaming encoder. Memory consumption is independent of movie duration.
class ezMovieEncoder
{
public:
  ezMovieEncoder() = default;
  EZ_DISALLOW_COPY_AND_ASSIGN(ezMovieEncoder);
  ~ezMovieEncoder();
  ezResult Open(const char* szPath, const char* szFormat, const char* szEncoder, ezUInt32 uiWidth, ezUInt32 uiHeight,
    ezUInt32 uiFps, ezUInt32 uiBitrate, ezUInt32 uiSampleRate, ezUInt32 uiChannels);
  ezResult WriteVideo(const ezUInt8* pRgba, ezUInt32 uiRowPitch);
  ezResult WriteAudio(const float* pSamples, ezUInt32 uiFrames);
  ezResult Finish();
  ezStringView GetCodecName() const { return m_sCodecName; }

private:
  ezResult Send(AVCodecContext* pCodec, AVStream* pStream, AVFrame* pFrame);
  AVFormatContext* m_pFormat = nullptr;
  AVCodecContext* m_pVideo = nullptr;
  AVCodecContext* m_pAudio = nullptr;
  AVStream* m_pVideoStream = nullptr;
  AVStream* m_pAudioStream = nullptr;
  AVFrame* m_pVideoFrame = nullptr;
  AVFrame* m_pAudioFrame = nullptr;
  SwsContext* m_pScale = nullptr;
  ezString m_sCodecName;
  ezInt64 m_iVideoPts = 0;
  ezInt64 m_iAudioPts = 0;
  ezUInt32 m_uiAudioFill = 0;
  ezUInt32 m_uiAudioFrameSize = 0;
  bool m_bFinished = false;
};
