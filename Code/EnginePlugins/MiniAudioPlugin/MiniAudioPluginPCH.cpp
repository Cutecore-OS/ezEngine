#include <MiniAudioPlugin/MiniAudioPluginPCH.h>

EZ_STATICLINK_LIBRARY(MiniAudioPlugin)
{
  if (bReturn)
    return;

  EZ_STATICLINK_REFERENCE(MiniAudioPlugin_Effects_MiniAudioEffect);
  EZ_STATICLINK_REFERENCE(MiniAudioPlugin_Components_MiniAudioEffectVolumeComponent);
  EZ_STATICLINK_REFERENCE(MiniAudioPlugin_Components_MiniAudioSoundVolumeComponent);
}
