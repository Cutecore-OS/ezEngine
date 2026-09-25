#include <RmlUiVideoPlugin/RmlUiVideoPluginPCH.h>

#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/OSFile.h>
#include <RmlUiVideoPlugin/VideoDecoder.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

static_assert(LIBAVCODEC_VERSION_MAJOR >= 60, "RmlUiVideoPlugin requires FFmpeg 6 or newer.");

namespace
{
  // Loose files use random access. Archive data directories use the existing stream API;
  // backwards seeks reopen the stream rather than retaining an entire movie in memory.
  struct VideoInput
  {
    ezOSFile m_File;
    ezFileReader m_Stream;
    ezString m_sPath;
    ezUInt64 m_uiPosition = 0;

    ezResult Open(ezStringView sPath)
    {
      m_sPath = sPath;
      ezStringBuilder sAbsolutePath;
      if (ezFileSystem::ResolvePath(sPath, &sAbsolutePath, nullptr).Succeeded() &&
          ezOSFile::ExistsFile(sAbsolutePath))
        return m_File.Open(sAbsolutePath, ezFileOpenMode::Read);
      if (m_sPath.IsAbsolutePath() && ezOSFile::ExistsFile(m_sPath))
        return m_File.Open(m_sPath, ezFileOpenMode::Read);
      return m_Stream.Open(sPath);
    }

    ezUInt64 GetSize() const
    {
      return m_File.IsOpen() ? m_File.GetFileSize() : m_Stream.GetFileSize();
    }

    static int Read(void* pOpaque, uint8_t* pBuffer, int iSize)
    {
      auto& input = *static_cast<VideoInput*>(pOpaque);
      const auto uiRead = input.m_File.IsOpen() ? input.m_File.Read(pBuffer, iSize) : input.m_Stream.ReadBytes(pBuffer, iSize);
      input.m_uiPosition += uiRead;
      return uiRead > 0 ? static_cast<int>(uiRead) : AVERROR_EOF;
    }

    static int64_t Seek(void* pOpaque, int64_t iOffset, int iWhence)
    {
      auto& input = *static_cast<VideoInput*>(pOpaque);
      if (iWhence == AVSEEK_SIZE)
        return static_cast<int64_t>(input.GetSize());

      iWhence &= ~AVSEEK_FORCE;
      ezInt64 iBase = 0;
      if (iWhence == SEEK_CUR)
        iBase = static_cast<ezInt64>(input.m_uiPosition);
      else if (iWhence == SEEK_END)
        iBase = static_cast<ezInt64>(input.GetSize());
      else if (iWhence != SEEK_SET)
        return AVERROR(EINVAL);

      if (iOffset < -iBase || iOffset > static_cast<ezInt64>(input.GetSize()) - iBase)
        return AVERROR(EINVAL);
      const ezUInt64 uiPosition = static_cast<ezUInt64>(iBase + iOffset);

      if (input.m_File.IsOpen())
        input.m_File.SetFilePosition(uiPosition, ezFileSeekMode::FromStart);
      else
      {
        if (uiPosition < input.m_uiPosition)
        {
          input.m_Stream.Close();
          if (input.m_Stream.Open(input.m_sPath).Failed())
            return AVERROR(EIO);
          input.m_uiPosition = 0;
        }
        const ezUInt64 uiSkip = uiPosition - input.m_uiPosition;
        if (input.m_Stream.SkipBytes(uiSkip) != uiSkip)
          return AVERROR(EIO);
      }
      input.m_uiPosition = uiPosition;
      return static_cast<int64_t>(uiPosition);
    }
  };
} // namespace

ezRmlUiVideoDecoder::ezRmlUiVideoDecoder(ezStringView sPath)
  : ezThread("RmlUi Video", 1024 * 1024)
  , m_sPath(sPath)
{
  Start();
  m_Signal.RaiseSignal();
}

ezRmlUiVideoDecoder::~ezRmlUiVideoDecoder()
{
  {
    EZ_LOCK(m_Mutex);
    m_bStop = true;
  }
  m_Signal.RaiseSignal();
  Join();
}

void ezRmlUiVideoDecoder::RequestFrame(ezTime position, bool bSeek)
{
  {
    EZ_LOCK(m_Mutex);
    if (!ezMath::IsFinite(position.GetSeconds()))
      return;
    m_Position = ezMath::Max(position, ezTime::MakeZero());
    if (bSeek)
    {
      ++m_uiSeekSerial;
      m_bFrameAvailable = false;
      m_bEnded = false;
    }
  }
  m_Signal.RaiseSignal();
}

bool ezRmlUiVideoDecoder::TakeFrame(Frame& out_frame, bool& out_bEnded, bool& out_bFailed)
{
  EZ_LOCK(m_Mutex);
  out_bEnded = m_bEnded;
  out_bFailed = m_bFailed;
  if (!m_bFrameAvailable)
    return false;

  out_frame = std::move(m_Frame);
  m_bFrameAvailable = false;
  return true;
}

ezUInt32 ezRmlUiVideoDecoder::Run()
{
  if (Decode().Failed())
  {
    EZ_LOCK(m_Mutex);
    m_bFailed = true;
    return 1;
  }
  return 0;
}

ezResult ezRmlUiVideoDecoder::Decode()
{
  VideoInput input;
  EZ_SUCCEED_OR_RETURN(input.Open(m_sPath));

  auto* pBuffer = static_cast<unsigned char*>(av_malloc(32768));
  if (!pBuffer)
    return EZ_FAILURE;
  AVIOContext* pIO = avio_alloc_context(pBuffer, 32768, 0, &input, VideoInput::Read, nullptr, VideoInput::Seek);
  if (!pIO)
  {
    av_free(pBuffer);
    return EZ_FAILURE;
  }
  EZ_SCOPE_EXIT(av_freep(&pIO->buffer); avio_context_free(&pIO));

  AVFormatContext* pFormat = avformat_alloc_context();
  if (!pFormat)
    return EZ_FAILURE;
  EZ_SCOPE_EXIT(avformat_close_input(&pFormat));
  pFormat->pb = pIO;
  pFormat->flags |= AVFMT_FLAG_CUSTOM_IO;
  pFormat->interrupt_callback.opaque = this;
  pFormat->interrupt_callback.callback = [](void* pOpaque) -> int
  {
    auto& decoder = *static_cast<ezRmlUiVideoDecoder*>(pOpaque);
    EZ_LOCK(decoder.m_Mutex);
    return decoder.m_bStop ? 1 : 0;
  };

  // Use the engine file system for both loose files and archive data directories.
  if (avformat_open_input(&pFormat, nullptr, nullptr, nullptr) < 0 || avformat_find_stream_info(pFormat, nullptr) < 0)
    return EZ_FAILURE;

  const AVCodec* pCodec = nullptr;
  const int iStream = av_find_best_stream(pFormat, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
  if (iStream < 0)
    return EZ_FAILURE;

  AVCodecContext* pDecoder = avcodec_alloc_context3(pCodec);
  if (!pDecoder)
    return EZ_FAILURE;
  EZ_SCOPE_EXIT(avcodec_free_context(&pDecoder));
  pDecoder->thread_count = 2;
  pDecoder->max_pixels = 8192 * 8192;
  if (avcodec_parameters_to_context(pDecoder, pFormat->streams[iStream]->codecpar) < 0 ||
      avcodec_open2(pDecoder, pCodec, nullptr) < 0)
    return EZ_FAILURE;

  AVPacket* pPacket = av_packet_alloc();
  AVFrame* pFrame = av_frame_alloc();
  AVFrame* pDueFrame = av_frame_alloc();
  SwsContext* pScaler = nullptr;
  EZ_SCOPE_EXIT(av_packet_free(&pPacket); av_frame_free(&pFrame); av_frame_free(&pDueFrame); sws_freeContext(pScaler));
  if (!pPacket || !pFrame || !pDueFrame)
    return EZ_FAILURE;

  const AVStream* pStream = pFormat->streams[iStream];
  const double fTimeBase = av_q2d(pStream->time_base);
  if (!ezMath::IsFinite(fTimeBase) || fTimeBase <= 0)
    return EZ_FAILURE;
  const int64_t iStart = pStream->start_time == AV_NOPTS_VALUE ? 0 : pStream->start_time;
  const AVRational frameRate = av_guess_frame_rate(pFormat, pFormat->streams[iStream], nullptr);
  const double fFrameDuration = frameRate.num > 0 && frameRate.den > 0 ? av_q2d(av_inv_q(frameRate)) : 1.0 / 30.0;
  double fFrameTime = 0;
  bool bDecodedFrame = false;
  double fEndTime = 0;
  bool bPending = false;
  bool bDraining = false;
  ezUInt64 uiSeekSerial = 0;

  while (true)
  {
    m_Signal.WaitForSignal();
    double fTarget;
    ezUInt64 uiRequestedSeekSerial;
    {
      EZ_LOCK(m_Mutex);
      if (m_bStop)
        return EZ_SUCCESS;
      fTarget = m_Position.GetSeconds();
      uiRequestedSeekSerial = m_uiSeekSerial;
    }
    if (uiSeekSerial != uiRequestedSeekSerial)
    {
      // Never hold the mailbox mutex during file IO: an archive seek may scan a stream.
      uiSeekSerial = uiRequestedSeekSerial;
      const double fTargetTicks = fTarget / fTimeBase;
      if (fTargetTicks >= static_cast<double>(ezMath::MaxValue<ezInt64>() - ezMath::Max<int64_t>(iStart, 0)))
        return EZ_FAILURE;
      const int64_t iTarget = iStart + static_cast<int64_t>(fTargetTicks);
      if (av_seek_frame(pFormat, iStream, iTarget, AVSEEK_FLAG_BACKWARD) < 0)
        return EZ_FAILURE;
      avcodec_flush_buffers(pDecoder);
      av_frame_unref(pFrame);
      av_frame_unref(pDueFrame);
      av_packet_unref(pPacket);
      bPending = false;
      bDraining = false;
      fFrameTime = fTarget;
      fEndTime = fTarget;
    }

    bool bPublish = false;
    bool bEnded = false;
    // Bound work per request so shutdown and a new seek cannot be starved by a long file.
    for (ezUInt32 uiWork = 0; uiWork < 64; ++uiWork)
    {
      {
        EZ_LOCK(m_Mutex);
        if (m_bStop)
          return EZ_SUCCESS;
        if (uiSeekSerial != m_uiSeekSerial)
          break;
      }

      if (!bPending)
      {
        const int iResult = avcodec_receive_frame(pDecoder, pFrame);
        if (iResult == AVERROR_EOF)
        {
          if (!bDecodedFrame)
            return EZ_FAILURE;
          bEnded = fTarget >= fEndTime;
          bPublish = true;
          break;
        }
        if (iResult == AVERROR(EAGAIN))
        {
          if (bDraining)
            return EZ_FAILURE;
          int iRead = av_read_frame(pFormat, pPacket);
          if (iRead == AVERROR_EOF)
          {
            if (avcodec_send_packet(pDecoder, nullptr) < 0)
              return EZ_FAILURE;
            bDraining = true;
          }
          else if (iRead < 0)
            return EZ_FAILURE;
          else
          {
            const int iSend = pPacket->stream_index == iStream ? avcodec_send_packet(pDecoder, pPacket) : 0;
            av_packet_unref(pPacket);
            if (iSend < 0)
              return EZ_FAILURE;
          }
          // Continue decoding even when no additional request has arrived.
          m_Signal.RaiseSignal();
          continue;
        }
        if (iResult < 0)
          return EZ_FAILURE;
        if (pFrame->best_effort_timestamp != AV_NOPTS_VALUE)
          fFrameTime = (pFrame->best_effort_timestamp - iStart) * fTimeBase;
        bPending = true;
        bDecodedFrame = true;
      }

      if (fFrameTime > fTarget + 0.0001)
      {
        bPublish = true;
        break;
      }

      // Retain only the latest due frame. In particular, never publish keyframe preroll
      // during a seek, and do not convert frames that will immediately be skipped.
      fEndTime = fFrameTime + (pFrame->duration > 0 ? pFrame->duration * fTimeBase : fFrameDuration);
      fFrameTime = fEndTime;
      av_frame_unref(pDueFrame);
      av_frame_move_ref(pDueFrame, pFrame);
      bPending = false;
      m_Signal.RaiseSignal();
    }

    if (!bPublish)
      continue;

    if (pDueFrame->data[0] != nullptr)
    {
      // Reject unreasonable dimensions before allocating the output buffer.
      if (pDueFrame->width <= 0 || pDueFrame->height <= 0 || pDueFrame->width > 8192 || pDueFrame->height > 8192)
        return EZ_FAILURE;
      pScaler = sws_getCachedContext(pScaler, pDueFrame->width, pDueFrame->height, static_cast<AVPixelFormat>(pDueFrame->format),
        pDueFrame->width, pDueFrame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
      if (!pScaler)
        return EZ_FAILURE;

      const int iColorSpace = pDueFrame->colorspace == AVCOL_SPC_UNSPECIFIED ? SWS_CS_DEFAULT : static_cast<int>(pDueFrame->colorspace);
      const int* pCoefficients = sws_getCoefficients(iColorSpace);
      if (sws_setColorspaceDetails(pScaler, pCoefficients, pDueFrame->color_range == AVCOL_RANGE_JPEG,
            pCoefficients, 1, 0, 1 << 16, 1 << 16) < 0)
        return EZ_FAILURE;

      Frame frame;
      frame.m_uiWidth = pDueFrame->width;
      frame.m_uiHeight = pDueFrame->height;
      frame.m_Pixels.SetCountUninitialized(frame.m_uiWidth * frame.m_uiHeight * 4);
      uint8_t* pPlanes[4] = {frame.m_Pixels.GetData(), nullptr, nullptr, nullptr};
      int strides[4] = {static_cast<int>(frame.m_uiWidth * 4), 0, 0, 0};
      if (sws_scale(pScaler, pDueFrame->data, pDueFrame->linesize, 0, pDueFrame->height, pPlanes, strides) != pDueFrame->height)
        return EZ_FAILURE;

      {
        EZ_LOCK(m_Mutex);
        if (uiSeekSerial == m_uiSeekSerial)
        {
          m_Frame = std::move(frame);
          m_bFrameAvailable = true;
          m_bEnded = bEnded;
        }
      }
      av_frame_unref(pDueFrame);
    }
    else
    {
      EZ_LOCK(m_Mutex);
      if (uiSeekSerial == m_uiSeekSerial)
        m_bEnded = bEnded;
    }
  }
}
