#include <SkyBillboardPlugin/SkyBillboardPluginPCH.h>

EZ_STATICLINK_LIBRARY(SkyBillboardPlugin)
{
  if (bReturn)
    return;

  EZ_STATICLINK_REFERENCE(SkyBillboardPlugin_Components_SkyBillboardComponent);
}
