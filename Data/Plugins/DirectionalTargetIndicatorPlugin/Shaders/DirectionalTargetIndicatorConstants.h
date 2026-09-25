#pragma once

#include <Shaders/Common/ConstantBufferMacros.h>
#include <Shaders/Common/Platforms.h>

BEGIN_PUSH_CONSTANTS(ezDirectionalTargetIndicatorConstants)
{
  FLOAT4(CenterAndAxisX);
  FLOAT4(AxisY);
}
END_PUSH_CONSTANTS(ezDirectionalTargetIndicatorConstants)
