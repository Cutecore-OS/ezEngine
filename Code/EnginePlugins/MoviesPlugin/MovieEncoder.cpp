#include <MoviesPlugin/MovieEncoder.h>
#include <MoviesPlugin/MoviesPluginPCH.h>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

ezMovieEncoder::~ezMovieEncoder()
{
  sws_freeContext(m_pScale);
  av_frame_free(&m_pVideoFrame);
  av_frame_free(&m_pAudioFrame);
  avcodec_free_context(&m_pVideo);
  avcodec_free_context(&m_pAudio);
  if (m_pFormat)
  {
    avio_closep(&m_pFormat->pb);
    avformat_free_context(m_pFormat);
  }
}

ezResult ezMovieEncoder::Open(const char* szPath, const char* szFormat, const char* szEncoder, ezUInt32 uiWidth,
  ezUInt32 uiHeight, ezUInt32 uiFps, ezUInt32 uiBitrate, ezUInt32 uiSampleRate, ezUInt32 uiChannels)
{
  if (avformat_alloc_output_context2(&m_pFormat, nullptr, szFormat, szPath) < 0 || !m_pFormat)
    return EZ_FAILURE;

  const bool bAuto = ezStringView(szEncoder) == "auto";
  const char* candidates[] = {"h264_nvenc", "h264_amf", "h264_qsv", "libx264", "mpeg4"};
  for (const char* szCandidate : candidates)
  {
    if (!bAuto && ezStringView(szEncoder) != szCandidate)
      continue;
    const AVCodec* pCodec = avcodec_find_encoder_by_name(szCandidate);
    if (!pCodec)
      continue;
    m_pVideo = avcodec_alloc_context3(pCodec);
    if (!m_pVideo)
      return EZ_FAILURE;
    m_pVideo->width = uiWidth;
    m_pVideo->height = uiHeight;
    m_pVideo->time_base = AVRational{1, static_cast<int>(uiFps)};
    m_pVideo->framerate = AVRational{static_cast<int>(uiFps), 1};
    m_pVideo->bit_rate = uiBitrate;
    m_pVideo->gop_size = uiFps * 2;
    m_pVideo->max_b_frames = 0;
    m_pVideo->pix_fmt = (ezStringView(szCandidate) == "libx264" || ezStringView(szCandidate) == "mpeg4") ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NV12;
    m_pVideo->color_range = AVCOL_RANGE_MPEG;
    m_pVideo->colorspace = AVCOL_SPC_BT709;
    m_pVideo->color_primaries = AVCOL_PRI_BT709;
    m_pVideo->color_trc = AVCOL_TRC_BT709;
    if (m_pFormat->oformat->flags & AVFMT_GLOBALHEADER)
      m_pVideo->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(m_pVideo, pCodec, nullptr) >= 0)
    {
      m_sCodecName = szCandidate;
      break;
    }
    avcodec_free_context(&m_pVideo);
  }
  if (!m_pVideo)
    return EZ_FAILURE;

  m_pVideoStream = avformat_new_stream(m_pFormat, nullptr);
  if (!m_pVideoStream)
    return EZ_FAILURE;
  m_pVideoStream->time_base = m_pVideo->time_base;
  m_pVideoStream->avg_frame_rate = m_pVideo->framerate;
  if (avcodec_parameters_from_context(m_pVideoStream->codecpar, m_pVideo) < 0)
    return EZ_FAILURE;
  m_pVideoFrame = av_frame_alloc();
  if (!m_pVideoFrame)
    return EZ_FAILURE;
  m_pVideoFrame->format = m_pVideo->pix_fmt;
  m_pVideoFrame->width = uiWidth;
  m_pVideoFrame->height = uiHeight;
  if (av_frame_get_buffer(m_pVideoFrame, 32) < 0)
    return EZ_FAILURE;
  m_pScale = sws_getContext(uiWidth, uiHeight, AV_PIX_FMT_RGBA, uiWidth, uiHeight, m_pVideo->pix_fmt, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (!m_pScale)
    return EZ_FAILURE;
  const int* pCoefficients = sws_getCoefficients(SWS_CS_ITU709);
  if (sws_setColorspaceDetails(m_pScale, pCoefficients, 1, pCoefficients, 0, 0, 1 << 16, 1 << 16) < 0)
    return EZ_FAILURE;

  // Matroska stores PCM without AAC priming delay; MP4 uses AAC with an edit list.
  const bool bPcm = ezStringView(szFormat) == "matroska";
  const AVCodec* pAudioCodec = avcodec_find_encoder(bPcm ? AV_CODEC_ID_PCM_F32LE : AV_CODEC_ID_AAC);
  if (!pAudioCodec)
    return EZ_FAILURE;
  m_pAudio = avcodec_alloc_context3(pAudioCodec);
  if (!m_pAudio)
    return EZ_FAILURE;
  m_pAudio->sample_rate = uiSampleRate;
  av_channel_layout_default(&m_pAudio->ch_layout, uiChannels);
  m_pAudio->sample_fmt = bPcm ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_FLTP;
  m_pAudio->bit_rate = bPcm ? uiSampleRate * uiChannels * 32 : 192000;
  m_pAudio->time_base = AVRational{1, static_cast<int>(uiSampleRate)};
  if (m_pFormat->oformat->flags & AVFMT_GLOBALHEADER)
    m_pAudio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  if (avcodec_open2(m_pAudio, pAudioCodec, nullptr) < 0)
    return EZ_FAILURE;
  m_pAudioStream = avformat_new_stream(m_pFormat, nullptr);
  if (!m_pAudioStream)
    return EZ_FAILURE;
  m_pAudioStream->time_base = m_pAudio->time_base;
  if (avcodec_parameters_from_context(m_pAudioStream->codecpar, m_pAudio) < 0)
    return EZ_FAILURE;
  m_pAudioFrame = av_frame_alloc();
  if (!m_pAudioFrame)
    return EZ_FAILURE;
  m_pAudioFrame->format = m_pAudio->sample_fmt;
  m_pAudioFrame->sample_rate = m_pAudio->sample_rate;
  m_uiAudioFrameSize = m_pAudio->frame_size > 0 ? m_pAudio->frame_size : 1024;
  m_pAudioFrame->nb_samples = m_uiAudioFrameSize;
  if (av_channel_layout_copy(&m_pAudioFrame->ch_layout, &m_pAudio->ch_layout) < 0 || av_frame_get_buffer(m_pAudioFrame, 0) < 0)
    return EZ_FAILURE;
  if (ezStringView(szFormat) == "mp4" || ezStringView(szFormat) == "mov")
    av_opt_set_int(m_pFormat->priv_data, "movie_timescale", uiSampleRate, 0);
  if (avio_open(&m_pFormat->pb, szPath, AVIO_FLAG_WRITE) < 0 || avformat_write_header(m_pFormat, nullptr) < 0)
    return EZ_FAILURE;
  return EZ_SUCCESS;
}

ezResult ezMovieEncoder::Send(AVCodecContext* pCodec, AVStream* pStream, AVFrame* pFrame)
{
  if (avcodec_send_frame(pCodec, pFrame) < 0)
    return EZ_FAILURE;
  AVPacket* pPacket = av_packet_alloc();
  if (!pPacket)
    return EZ_FAILURE;
  int iResult = 0;
  while ((iResult = avcodec_receive_packet(pCodec, pPacket)) >= 0)
  {
    av_packet_rescale_ts(pPacket, pCodec->time_base, pStream->time_base);
    pPacket->stream_index = pStream->index;
    iResult = av_interleaved_write_frame(m_pFormat, pPacket);
    av_packet_unref(pPacket);
    if (iResult < 0)
      break;
  }
  av_packet_free(&pPacket);
  return (iResult == AVERROR(EAGAIN) || iResult == AVERROR_EOF) ? EZ_SUCCESS : EZ_FAILURE;
}

ezResult ezMovieEncoder::WriteVideo(const ezUInt8* pRgba, ezUInt32 uiRowPitch)
{
  if (av_frame_make_writable(m_pVideoFrame) < 0)
    return EZ_FAILURE;
  const ezUInt8* data[] = {pRgba, nullptr, nullptr, nullptr};
  const int strides[] = {static_cast<int>(uiRowPitch), 0, 0, 0};
  if (sws_scale(m_pScale, data, strides, 0, m_pVideo->height, m_pVideoFrame->data, m_pVideoFrame->linesize) != m_pVideo->height)
    return EZ_FAILURE;
  m_pVideoFrame->pts = m_iVideoPts++;
  return Send(m_pVideo, m_pVideoStream, m_pVideoFrame);
}

ezResult ezMovieEncoder::WriteAudio(const float* pSamples, ezUInt32 uiFrames)
{
  const ezUInt32 uiChannels = m_pAudio->ch_layout.nb_channels;
  for (ezUInt32 i = 0; i < uiFrames; ++i)
  {
    if (m_uiAudioFill == 0 && av_frame_make_writable(m_pAudioFrame) < 0)
      return EZ_FAILURE;
    for (ezUInt32 c = 0; c < uiChannels; ++c)
    {
      if (m_pAudio->sample_fmt == AV_SAMPLE_FMT_FLT)
        reinterpret_cast<float*>(m_pAudioFrame->data[0])[m_uiAudioFill * uiChannels + c] = pSamples[i * uiChannels + c];
      else
        reinterpret_cast<float*>(m_pAudioFrame->extended_data[c])[m_uiAudioFill] = pSamples[i * uiChannels + c];
    }
    if (++m_uiAudioFill == m_uiAudioFrameSize)
    {
      m_pAudioFrame->pts = m_iAudioPts;
      EZ_SUCCEED_OR_RETURN(Send(m_pAudio, m_pAudioStream, m_pAudioFrame));
      m_iAudioPts += m_uiAudioFill;
      m_uiAudioFill = 0;
    }
  }
  return EZ_SUCCESS;
}

ezResult ezMovieEncoder::Finish()
{
  if (m_bFinished)
    return EZ_FAILURE;
  if (m_uiAudioFill > 0)
  {
    // AAC accepts a final short frame and records encoder padding in the container.
    m_pAudioFrame->nb_samples = m_uiAudioFill;
    m_pAudioFrame->pts = m_iAudioPts;
    EZ_SUCCEED_OR_RETURN(Send(m_pAudio, m_pAudioStream, m_pAudioFrame));
  }
  EZ_SUCCEED_OR_RETURN(Send(m_pVideo, m_pVideoStream, nullptr));
  EZ_SUCCEED_OR_RETURN(Send(m_pAudio, m_pAudioStream, nullptr));
  if (av_write_trailer(m_pFormat) < 0 || avio_closep(&m_pFormat->pb) < 0)
    return EZ_FAILURE;
  m_bFinished = true;
  return EZ_SUCCESS;
}
