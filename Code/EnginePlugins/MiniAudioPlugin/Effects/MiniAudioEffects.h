#pragma once

#include <Core/World/Declarations.h>
#include <Foundation/Math/Vec3.h>
#include <MiniAudioPlugin/MiniAudioPluginDLL.h>

struct ma_engine;
struct ezMiniAudioSoundInstance;

void ezMiniAudioRegisterEffectVolume(ezComponentHandle hComponent);
void ezMiniAudioUnregisterEffectVolume(ezComponentHandle hComponent);
void ezMiniAudioRegisterSoundVolume(ezComponentHandle hComponent);
void ezMiniAudioUnregisterSoundVolume(ezComponentHandle hComponent);
void ezMiniAudioUpdateEffectVolumes(const ezVec3& vListener);

/// Called with the backend locked, before playback and after world updates.
EZ_MINIAUDIOPLUGIN_DLL void ezMiniAudioConfigureEffects(ezMiniAudioSoundInstance& instance, ma_engine* pEngine, const ezVec3& vListener);
