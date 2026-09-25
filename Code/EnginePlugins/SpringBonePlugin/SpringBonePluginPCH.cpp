#include <Foundation/Configuration/Plugin.h>
#include <SpringBonePlugin/SpringBonePluginPCH.h>

EZ_PLUGIN_DEPENDENCY(ezJoltPlugin);

EZ_STATICLINK_LIBRARY(SpringBonePlugin)
{
  if (bReturn)
    return;

  EZ_STATICLINK_REFERENCE(SpringBonePlugin_Components_SpringBoneComponent);
}
