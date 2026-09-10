#pragma once

#include <Foundation/IO/Stream.h>
#include <Foundation/Math/Vec3.h>
#include <GreyBoxPlugin/GreyBoxPluginDLL.h>

class ezGeometry;

/// Radius scales are relative to half the Size X/Y extents; the cone points along local +Z.
struct EZ_GREYBOXPLUGIN_DLL ezGreyBoxConeSettings
{
  ezVec3 m_vNegative = ezVec3(0.5f);
  ezVec3 m_vPositive = ezVec3(0.5f);
  float m_fBaseRadiusScale = 1.0f;
  float m_fTopRadiusScale = 0.0f;
  ezUInt32 m_uiSides = 32;
  ezUInt32 m_uiHeightSegments = 8;
  float m_fProfileCurve = 0.0f;
  bool m_bSmoothShading = true;

  // Native Grey Boxing parameters. Shape 13 is the procedural cone.
  ezUInt8 m_uiShape = 13;
  ezUInt32 m_uiDetail = 16;
  ezAngle m_Curvature;
  float m_fThickness = 0.5f;
  bool m_bSlopedTop = false;
  bool m_bSlopedBottom = false;

  void Serialize(ezStreamWriter& stream) const;
  void Deserialize(ezStreamReader& stream, ezUInt32 uiVersion = 2);
  ezUInt64 GetHash() const;
  bool GetDefaultSmoothShading() const;
  ezResult BuildGeometry(ezGeometry& out_geometry) const;
};
