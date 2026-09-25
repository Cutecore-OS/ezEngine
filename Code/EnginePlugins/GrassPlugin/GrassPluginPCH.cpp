#include <GrassPlugin/GrassPluginPCH.h>

EZ_STATICLINK_LIBRARY(GrassPlugin)
{
  if (bReturn)
    return;

  EZ_STATICLINK_REFERENCE(GrassPlugin_Components_GrassComponent);
  EZ_STATICLINK_REFERENCE(GrassPlugin_Components_GrassPatchComponent);
}
