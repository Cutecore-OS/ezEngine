#include <GreyBoxPlugin/GreyBoxPluginPCH.h>

#include <Core/Graphics/Geometry.h>
#include <Foundation/Algorithm/HashStream.h>
#include <Foundation/Containers/Map.h>
#include <GameEngine/Gameplay/GreyBoxComponent.h>
#include <GreyBoxPlugin/Geometry/ConeGeometry.h>



namespace
{
  bool HasSmoothNormals(const ezGeometry& geometry)
  {
    for (const auto& polygon : geometry.GetPolygons())
    {
      if (polygon.m_Vertices.IsEmpty())
        continue;
      const ezVec3 normal = geometry.GetVertices()[polygon.m_Vertices[0]].m_vNormal;
      for (ezUInt32 index : polygon.m_Vertices)
        if (!geometry.GetVertices()[index].m_vNormal.IsEqual(normal, 0.0001f))
          return true;
    }
    return false;
  }

  struct PositionComparer
  {
    bool Less(const ezVec3& a, const ezVec3& b) const
    {
      if (a.x != b.x)
        return a.x < b.x;
      if (a.y != b.y)
        return a.y < b.y;
      return a.z < b.z;
    }
    bool Equal(const ezVec3& a, const ezVec3& b) const { return a == b; }
  };

  void SmoothHardGeometry(ezGeometry& geometry)
  {
    struct Contribution
    {
      ezVec3 m_Normal;
      float m_fCornerAngle;
    };

    geometry.ComputeFaceNormals();
    ezMap<ezVec3, ezHybridArray<Contribution, 4>, PositionComparer> normals;
    for (const auto& polygon : geometry.GetPolygons())
    {
      const ezUInt32 count = polygon.m_Vertices.GetCount();
      for (ezUInt32 i = 0; i < count; ++i)
      {
        auto& vertex = geometry.GetVertices()[polygon.m_Vertices[i]];
        const ezVec3 position = vertex.m_vPosition;
        ezVec3 a = geometry.GetVertices()[polygon.m_Vertices[(i + count - 1) % count]].m_vPosition - position;
        ezVec3 b = geometry.GetVertices()[polygon.m_Vertices[(i + 1) % count]].m_vPosition - position;
        if (a.NormalizeIfNotZero(ezVec3::MakeAxisX()).Failed() || b.NormalizeIfNotZero(ezVec3::MakeAxisX()).Failed())
          continue;
        const float angle = ezMath::ACos(ezMath::Clamp(a.Dot(b), -1.0f, 1.0f)).GetRadian();
        normals[position].PushBack({polygon.m_vNormal, angle});
        vertex.m_vNormal = polygon.m_vNormal;
      }
    }
    for (auto& vertex : geometry.GetVertices())
    {
      const auto it = normals.Find(vertex.m_vPosition);
      if (!it.IsValid())
        continue;
      ezVec3 normal = ezVec3::MakeZero();
      for (const auto& contribution : it.Value())
      {
        // Do not smooth through an acute crease into a back-facing surface. At a ramp's
        // toe, blending the underside into the slope creates nearly tangent normals.
        if (contribution.m_Normal.Dot(vertex.m_vNormal) >= -0.0001f)
          normal += contribution.m_Normal * contribution.m_fCornerAngle;
      }
      normal.NormalizeIfNotZero(vertex.m_vNormal).IgnoreResult();
      vertex.m_vNormal = normal;
    }
  }

} // namespace

namespace
{
  class ezNativeGreyBoxBuilder : public ezGreyBoxComponent
  {
  public:
    void Build(const ezGreyBoxConeSettings& settings, ezGeometry& geometry)
    {
      m_Shape = static_cast<ezGreyBoxShape::Enum>(settings.m_uiShape);
      m_fSizeNegX = settings.m_vNegative.x;
      m_fSizePosX = settings.m_vPositive.x;
      m_fSizeNegY = settings.m_vNegative.y;
      m_fSizePosY = settings.m_vPositive.y;
      m_fSizeNegZ = settings.m_vNegative.z;
      m_fSizePosZ = settings.m_vPositive.z;
      m_uiDetail = settings.m_uiDetail;
      SetCurvature(settings.m_Curvature);
      m_fThickness = settings.m_fThickness;
      m_bSlopedTop = settings.m_bSlopedTop;
      m_bSlopedBottom = settings.m_bSlopedBottom;
      BuildGeometry(geometry, m_Shape, false);
    }
  };

  ezResult BuildNativeShape(const ezGreyBoxConeSettings& settings, ezGeometry& geometry)
  {
    if (settings.m_uiShape > ezGreyBoxShape::SpiralStairs || settings.m_uiDetail < 1 || settings.m_uiDetail > 4096 ||
        !settings.m_vNegative.IsValid() || !settings.m_vPositive.IsValid() ||
        !(settings.m_vNegative + settings.m_vPositive).IsValid() ||
        !ezMath::IsFinite(settings.m_Curvature.GetRadian()) || !ezMath::IsFinite(settings.m_fThickness))
      return EZ_FAILURE;
    ezNativeGreyBoxBuilder builder;
    ezGeometry native;
    builder.Build(settings, native);
    if (settings.m_bSmoothShading)
    {
      if (!HasSmoothNormals(native))
        SmoothHardGeometry(native);
      geometry = std::move(native);
      return EZ_SUCCESS;
    }
    // Split shared vertices per polygon before assigning face normals, retaining UV seams.
    native.ComputeFaceNormals();
    for (const auto& polygon : native.GetPolygons())
    {
      ezHybridArray<ezUInt32, 32> indices;
      for (ezUInt32 index : polygon.m_Vertices)
      {
        const auto& vertex = native.GetVertices()[index];
        indices.PushBack(geometry.AddVertex(vertex.m_vPosition, polygon.m_vNormal, vertex.m_vTexCoord, vertex.m_Color,
          vertex.m_BoneIndices, vertex.m_BoneWeights));
      }
      geometry.AddPolygon(indices, false);
    }
    return EZ_SUCCESS;
  }
} // namespace

void ezGreyBoxConeSettings::Serialize(ezStreamWriter& stream) const
{
  stream << m_vNegative << m_vPositive << m_fBaseRadiusScale << m_fTopRadiusScale;
  stream << m_uiSides << m_uiHeightSegments << m_fProfileCurve << m_bSmoothShading;
  stream << m_uiShape << m_uiDetail << m_Curvature << m_fThickness << m_bSlopedTop << m_bSlopedBottom;
}

void ezGreyBoxConeSettings::Deserialize(ezStreamReader& stream, ezUInt32 uiVersion)
{
  stream >> m_vNegative >> m_vPositive >> m_fBaseRadiusScale >> m_fTopRadiusScale;
  stream >> m_uiSides >> m_uiHeightSegments >> m_fProfileCurve >> m_bSmoothShading;
  if (uiVersion >= 2)
    stream >> m_uiShape >> m_uiDetail >> m_Curvature >> m_fThickness >> m_bSlopedTop >> m_bSlopedBottom;
}

ezUInt64 ezGreyBoxConeSettings::GetHash() const
{
  ezHashStreamWriter64 stream;
  stream << ezUInt32(3); // Geometry algorithm revision; invalidate cached meshes after plugin reload.
  Serialize(stream);
  return stream.GetHashValue();
}

ezResult ezGreyBoxConeSettings::BuildGeometry(ezGeometry& out_geometry) const
{
  out_geometry.Clear();
  if (m_uiShape != 13)
    return BuildNativeShape(*this, out_geometry);
  const ezVec3 size = (m_vNegative + m_vPositive).Abs();
  const ezVec3 center = (m_vPositive - m_vNegative) * 0.5f;
  if (!size.IsValid() || !center.IsValid() || size.x <= 0 || size.y <= 0 || size.z <= 0 ||
      !ezMath::IsFinite(m_fBaseRadiusScale) || !ezMath::IsFinite(m_fTopRadiusScale) || !ezMath::IsFinite(m_fProfileCurve))
    return EZ_FAILURE;

  const ezUInt32 sides = ezMath::Clamp(m_uiSides, 3u, 128u);
  const ezUInt32 segments = ezMath::Clamp(m_uiHeightSegments, 1u, 64u);
  const float bottom = ezMath::Clamp(m_fBaseRadiusScale, 0.0f, 4.0f);
  const float top = ezMath::Clamp(m_fTopRadiusScale, 0.0f, 4.0f);
  const float curve = ezMath::Clamp(m_fProfileCurve, -0.95f, 1.0f);
  if (bottom == 0 && top == 0)
    return EZ_FAILURE;

  // Four sides make an axis-aligned square pyramid that fills the requested X/Y extents.
  const float polygonScale = sides == 4 ? ezMath::Sqrt(2.0f) : 1.0f;
  const float angleOffset = sides == 4 ? ezMath::Pi<float>() * 0.25f : 0.0f;
  const float rx = size.x * 0.5f * polygonScale;
  const float ry = size.y * 0.5f * polygonScale;
  auto radius = [bottom, top, curve](float t)
  {
    if (t == 0)
      return bottom;
    if (t == 1)
      return top;
    return ezMath::Lerp(bottom, top, t) * (1.0f + curve * ezMath::Sin(ezAngle::MakeFromRadian(ezMath::Pi<float>() * t)));
  };
  auto angle = [sides, angleOffset](ezUInt32 i)
  {
    return ezAngle::MakeFromRadian(angleOffset + (2.0f * ezMath::Pi<float>()) * static_cast<float>(i % sides) / sides);
  };
  auto position = [&](ezUInt32 i, float t)
  {
    const float r = radius(t);
    return center + ezVec3(rx * r * ezMath::Cos(angle(i)), ry * r * ezMath::Sin(angle(i)), (t - 0.5f) * size.z);
  };
  auto normal = [&](ezUInt32 i, float t)
  {
    const ezAngle a = angle(i);
    const ezAngle h = ezAngle::MakeFromRadian(ezMath::Pi<float>() * t);
    const float dr = (top - bottom) * (1.0f + curve * ezMath::Sin(h)) +
                     ezMath::Lerp(bottom, top, t) * curve * ezMath::Pi<float>() * ezMath::Cos(h);
    ezVec3 n(ry * ezMath::Cos(a), rx * ezMath::Sin(a), -rx * ry * dr / size.z);
    n.NormalizeIfNotZero(ezVec3::MakeAxisZ()).IgnoreResult();
    return n;
  };

  for (ezUInt32 j = 0; j < segments; ++j)
  {
    const float t0 = static_cast<float>(j) / segments;
    const float t1 = static_cast<float>(j + 1) / segments;
    for (ezUInt32 i = 0; i < sides; ++i)
    {
      ezVec3 points[4] = {position(i, t0), position(i + 1, t0), position(i + 1, t1), position(i, t1)};
      ezUInt32 sample[4] = {0, 1, 2, 3};
      ezUInt32 count = 4;
      // Use a triangle at an apex rather than a zero-area quad.
      if (radius(t1) == 0)
        count = 3;
      else if (radius(t0) == 0)
      {
        sample[1] = 2;
        sample[2] = 3;
        count = 3;
      }
      ezVec3 faceNormal = (points[sample[1]] - points[0]).CrossRH(points[sample[2]] - points[0]);
      if (faceNormal.NormalizeIfNotZero(ezVec3::MakeAxisZ()).Failed())
        continue;
      ezUInt32 polygon[4];
      for (ezUInt32 k = 0; k < count; ++k)
      {
        const ezUInt32 s = sample[k];
        const ezUInt32 a = i + ((s == 1 || s == 2) ? 1 : 0);
        const float t = s >= 2 ? t1 : t0;
        polygon[k] = out_geometry.AddVertex(points[s], m_bSmoothShading ? normal(a, t) : faceNormal,
          ezVec2(static_cast<float>(a) / sides, t));
      }
      out_geometry.AddPolygon(ezMakeArrayPtr(polygon, count), false);
    }
  }

  for (ezUInt32 cap = 0; cap < 2; ++cap)
  {
    const float t = static_cast<float>(cap);
    if (radius(t) == 0)
      continue;
    ezDynamicArray<ezUInt32> polygon;
    for (ezUInt32 i = 0; i < sides; ++i)
    {
      const ezUInt32 index = cap == 0 ? sides - 1 - i : i;
      const ezVec3 p = position(index, t);
      polygon.PushBack(out_geometry.AddVertex(p, cap == 0 ? -ezVec3::MakeAxisZ() : ezVec3::MakeAxisZ(),
        ezVec2((p.x - center.x) / size.x + 0.5f, (p.y - center.y) / size.y + 0.5f)));
    }
    out_geometry.AddPolygon(polygon, false);
  }
  return EZ_SUCCESS;
}

bool ezGreyBoxConeSettings::GetDefaultSmoothShading() const
{
  if (m_uiShape == 13)
    return true;
  if (m_uiShape > 12 || m_uiDetail == 0 || m_uiDetail > 4096)
    return false;
  ezNativeGreyBoxBuilder builder;
  ezGeometry geometry;
  builder.Build(*this, geometry);
  return HasSmoothNormals(geometry);
}
