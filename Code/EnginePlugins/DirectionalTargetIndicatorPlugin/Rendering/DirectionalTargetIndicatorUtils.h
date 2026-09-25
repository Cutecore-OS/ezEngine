#pragma once

#include <Foundation/Math/Vec2.h>
#include <Foundation/Math/Vec4.h>

/// Pure screen-space calculations shared by extraction and the regression tests.
namespace ezDirectionalTargetIndicatorUtils
{
  inline ezVec2 Rotate(const ezVec2& vValue, const ezVec2& vRotation)
  {
    return ezVec2(vValue.x * vRotation.x - vValue.y * vRotation.y, vValue.x * vRotation.y + vValue.y * vRotation.x);
  }

  inline ezVec2 GetScreenDirection(const ezVec4& vClipPosition, const ezVec2& vViewportSize)
  {
    // Do not divide by signed W: targets behind the camera must not point to the opposite edge.
    ezVec2 vDirection(vClipPosition.x * vViewportSize.x, -vClipPosition.y * vViewportSize.y);
    if (vDirection.NormalizeIfNotZero(ezVec2(0.0f, 1.0f)).Failed())
      return ezVec2(0.0f, 1.0f); // Directly behind the camera: deterministically turn downwards.
    return vDirection;
  }

  inline ezVec2 GetEdgePosition(const ezVec2& vDirection, const ezVec2& vHalfSize)
  {
    const float fX = ezMath::Abs(vDirection.x) > 0.000001f ? vHalfSize.x / ezMath::Abs(vDirection.x) : ezMath::MaxValue<float>();
    const float fY = ezMath::Abs(vDirection.y) > 0.000001f ? vHalfSize.y / ezMath::Abs(vDirection.y) : ezMath::MaxValue<float>();
    return vDirection * ezMath::Min(fX, fY);
  }
  /// Offsets are percentages: left/right of width, top/bottom of height. Radius uses the shorter dimension.
  inline ezVec2 GetRoundedEdgePosition(const ezVec2& vDirection, const ezVec2& vViewportSize, const ezVec2& vExtent,
    float fCommonOffset, const ezVec4& vSideOffsets, float fCornerRadius)
  {
    const float fCommon = ezMath::Clamp(fCommonOffset, 0.0f, 50.0f);
    const ezVec2 vNearInset(
      ezMath::Min(50.0f, fCommon + ezMath::Max(0.0f, vSideOffsets.x)) * vViewportSize.x * 0.01f,
      ezMath::Min(50.0f, fCommon + ezMath::Max(0.0f, vSideOffsets.z)) * vViewportSize.y * 0.01f);
    const ezVec2 vFarInset(
      ezMath::Min(50.0f, fCommon + ezMath::Max(0.0f, vSideOffsets.y)) * vViewportSize.x * 0.01f,
      ezMath::Min(50.0f, fCommon + ezMath::Max(0.0f, vSideOffsets.w)) * vViewportSize.y * 0.01f);
    // Collapse towards the viewport center if the requested insets or layers consume all available space.
    const ezVec2 vMin = (vNearInset + vExtent - vViewportSize * 0.5f).CompMin(ezVec2::MakeZero());
    const ezVec2 vMax = (vViewportSize * 0.5f - vFarInset - vExtent).CompMax(ezVec2::MakeZero());
    const ezVec2 vCenter = (vMin + vMax) * 0.5f;
    const ezVec2 vHalfSize = (vMax - vMin) * 0.5f;
    const float fRadius = ezMath::Min(ezMath::Clamp(fCornerRadius, 0.0f, 50.0f) * 0.01f * ezMath::Min(vViewportSize.x, vViewportSize.y),
      ezMath::Min(vHalfSize.x, vHalfSize.y));
    ezVec2 vPoint = GetEdgePosition(vDirection, vHalfSize);
    const ezVec2 vCorner = vHalfSize - ezVec2(fRadius);
    if (fRadius > 0.0f && ezMath::Abs(vPoint.x) > vCorner.x && ezMath::Abs(vPoint.y) > vCorner.y)
    {
      // Intersect the ray with the circular corner instead of the sharp rectangular boundary.
      const ezVec2 vCircleCenter(vPoint.x < 0.0f ? -vCorner.x : vCorner.x, vPoint.y < 0.0f ? -vCorner.y : vCorner.y);
      const float fProjection = vDirection.Dot(vCircleCenter);
      const float fDiscriminant = ezMath::Max(0.0f, fProjection * fProjection - vCircleCenter.GetLengthSquared() + fRadius * fRadius);
      vPoint = vDirection * (fProjection + ezMath::Sqrt(fDiscriminant));
    }
    return vCenter + vPoint;
  }
} // namespace ezDirectionalTargetIndicatorUtils
