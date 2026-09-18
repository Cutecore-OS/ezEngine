#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

#include <Foundation/IO/MemoryStream.h>
#include <Foundation/Types/ScopeExit.h>
#include <MiniAudioPlugin/Resources/MiniAudioSoundDecoder.h>

// The MiniAudio DLL is built without a Vorbis backend. Keep this decoder local to
// the plugin so existing Ogg assets also work without changing the engine library.
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_INTEGER_CONVERSION
#define STB_VORBIS_MAX_CHANNELS 16
#if EZ_ENABLED(EZ_COMPILER_MSVC)
#  pragma warning(push, 0)
#endif
#include <MiniAudioPlugin/ThirdParty/stb_vorbis.h>
#if EZ_ENABLED(EZ_COMPILER_MSVC)
#  pragma warning(pop)
#endif

ezResult ezMiniAudioPrepareSoundData(ezDataBuffer& inout_data)
{
  if (inout_data.GetCount() < 4 || ezMemoryUtils::Compare(inout_data.GetData(), reinterpret_cast<const ezUInt8*>("OggS"), 4) != 0)
    return EZ_SUCCESS;

  if (inout_data.GetCount() > static_cast<ezUInt32>(ezMath::MaxValue<int>()))
    return EZ_FAILURE;

  int iError = 0;
  stb_vorbis* pDecoder = stb_vorbis_open_memory(inout_data.GetData(), static_cast<int>(inout_data.GetCount()), &iError, nullptr);
  if (pDecoder == nullptr)
    return EZ_FAILURE;
  EZ_SCOPE_EXIT(stb_vorbis_close(pDecoder));

  const auto info = stb_vorbis_get_info(pDecoder);
  const ezUInt32 uiFrames = stb_vorbis_stream_length_in_samples(pDecoder);
  const ezUInt64 uiSamples = static_cast<ezUInt64>(uiFrames) * info.channels;
  const ezUInt64 uiBytes = uiSamples * sizeof(float);
  if (info.channels <= 0 || info.channels > 16 || info.sample_rate == 0 || uiFrames == 0 ||
      uiSamples > static_cast<ezUInt64>(ezMath::MaxValue<int>()) || uiBytes > ezMath::MaxValue<ezUInt32>() - 44u)
    return EZ_FAILURE;

  ezDataBuffer decoded;
  decoded.SetCountUninitialized(static_cast<ezUInt32>(44 + uiBytes));
  ezRawMemoryStreamWriter writer(decoded.GetData(), decoded.GetCount());
  writer.WriteBytes("RIFF", 4).IgnoreResult();
  writer << static_cast<ezUInt32>(36 + uiBytes);
  writer.WriteBytes("WAVEfmt ", 8).IgnoreResult();
  writer << ezUInt32(16) << ezUInt16(3) << static_cast<ezUInt16>(info.channels);
  writer << info.sample_rate << static_cast<ezUInt32>(info.sample_rate * info.channels * sizeof(float));
  writer << static_cast<ezUInt16>(info.channels * sizeof(float)) << ezUInt16(32);
  writer.WriteBytes("data", 4).IgnoreResult();
  writer << static_cast<ezUInt32>(uiBytes);

  float* pSamples = reinterpret_cast<float*>(decoded.GetData() + 44);
  const int iDecodedFrames = stb_vorbis_get_samples_float_interleaved(pDecoder, info.channels, pSamples, static_cast<int>(uiSamples));
  if (iDecodedFrames < 0 || static_cast<ezUInt32>(iDecodedFrames) != uiFrames || stb_vorbis_get_error(pDecoder) != VORBIS__no_error)
    return EZ_FAILURE;

  inout_data.Swap(decoded);
  return EZ_SUCCESS;
}
