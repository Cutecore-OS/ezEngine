#include <DirectionalTargetIndicatorPlugin/DirectionalTargetIndicatorPluginPCH.h>

EZ_STATICLINK_LIBRARY(DirectionalTargetIndicatorPlugin)
{
  if (bReturn)
    return;

  EZ_STATICLINK_REFERENCE(DirectionalTargetIndicatorPlugin_Components_DirectionalTargetIndicatorComponent);
  EZ_STATICLINK_REFERENCE(DirectionalTargetIndicatorPlugin_Rendering_DirectionalTargetIndicatorRenderer);
}
