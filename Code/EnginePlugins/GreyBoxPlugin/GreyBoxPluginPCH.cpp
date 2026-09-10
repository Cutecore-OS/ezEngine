#include <GreyBoxPlugin/GreyBoxPluginPCH.h>
EZ_STATICLINK_LIBRARY(GreyBoxPlugin)
{
  if (bReturn)
    return;
  EZ_STATICLINK_REFERENCE(GreyBoxPlugin_Components_GreyBoxConeComponent);
}
