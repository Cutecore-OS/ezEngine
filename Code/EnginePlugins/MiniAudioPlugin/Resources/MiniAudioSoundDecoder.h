#pragma once

#include <Foundation/Types/VariantType.h>
#include <MiniAudioPlugin/MiniAudioPluginDLL.h>

/// Expands Ogg Vorbis to a float WAV once during resource loading.
/// Other formats remain unchanged and are decoded by MiniAudio.
EZ_MINIAUDIOPLUGIN_DLL ezResult ezMiniAudioPrepareSoundData(ezDataBuffer& inout_data);
