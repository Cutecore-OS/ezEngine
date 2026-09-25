#include <Foundation/Configuration/Plugin.h>
#include <RmlUiVideoPlugin/RmlUiVideoPluginPCH.h>

EZ_PLUGIN_DEPENDENCY(ezRmlUiPlugin);

EZ_STATICLINK_LIBRARY(RmlUiVideoPlugin)
{
  if (bReturn)
    return;

  EZ_STATICLINK_REFERENCE(RmlUiVideoPlugin_Startup);
}
