#include <Core/Graphics/Geometry.h>
#include <Foundation/IO/MemoryStream.h>
#include <GreyBoxPlugin/Geometry/ConeGeometry.h>
#include <TestFramework/Framework/TestFramework.h>

EZ_CREATE_SIMPLE_TEST(GreyBox, ConeGeometry)
{
  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Closed cones, pyramids, frusta and inverted cones have valid outward triangles")
  {
    for (ezUInt32 sides : {3u, 4u, 32u, 128u})
    {
      for (ezUInt32 segments : {1u, 8u, 64u})
      {
        for (float top : {0.0f, 0.35f, 1.5f})
        {
          for (bool inverted : {false, true})
          {
            ezGreyBoxConeSettings settings;
            settings.m_uiSides = sides;
            settings.m_uiHeightSegments = segments;
            settings.m_fTopRadiusScale = top;
            if (inverted)
              ezMath::Swap(settings.m_fTopRadiusScale, settings.m_fBaseRadiusScale);
            ezGeometry geometry;
            EZ_TEST_BOOL(settings.BuildGeometry(geometry).Succeeded());
            const ezUInt32 caps = top == 0 ? 1 : 2;
            EZ_TEST_INT(geometry.GetPolygons().GetCount(), sides * segments + caps);
            geometry.TriangulatePolygons();
            float volume = 0;
            for (const auto& polygon : geometry.GetPolygons())
            {
              const ezVec3 a = geometry.GetVertices()[polygon.m_Vertices[0]].m_vPosition;
              const ezVec3 b = geometry.GetVertices()[polygon.m_Vertices[1]].m_vPosition;
              const ezVec3 c = geometry.GetVertices()[polygon.m_Vertices[2]].m_vPosition;
              const ezVec3 cross = (b - a).CrossRH(c - a);
              EZ_TEST_BOOL(cross.GetLengthSquared() > 0);
              volume += a.Dot(b.CrossRH(c)) / 6.0f;
            }
            EZ_TEST_BOOL(volume > 0);
            for (const auto& vertex : geometry.GetVertices())
            {
              EZ_TEST_BOOL(vertex.m_vPosition.IsValid());
              EZ_TEST_BOOL(vertex.m_vNormal.IsNormalized(0.001f));
            }
          }
        }
      }
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Square pyramid fills Size, flat normals, curve changes middle radius")
  {
    ezGreyBoxConeSettings settings;
    settings.m_uiSides = 4;
    settings.m_uiHeightSegments = 1;
    settings.m_bSmoothShading = false;
    ezGeometry geometry;
    EZ_TEST_BOOL(settings.BuildGeometry(geometry).Succeeded());
    ezBoundingBox bounds = ezBoundingBox::MakeInvalid();
    for (const auto& vertex : geometry.GetVertices())
      bounds.ExpandToInclude(vertex.m_vPosition);
    EZ_TEST_BOOL(bounds.m_vMin.IsEqual(ezVec3(-0.5f), 0.00001f));
    EZ_TEST_BOOL(bounds.m_vMax.IsEqual(ezVec3(0.5f), 0.00001f));
    for (const auto& polygon : geometry.GetPolygons())
      for (ezUInt32 i : polygon.m_Vertices)
        EZ_TEST_BOOL(geometry.GetVertices()[i].m_vNormal == geometry.GetVertices()[polygon.m_Vertices[0]].m_vNormal);

    settings.m_uiSides = 32;
    settings.m_uiHeightSegments = 2;
    settings.m_fTopRadiusScale = 0.5f;
    for (float curve : {-0.8f, 0.0f, 0.8f})
    {
      settings.m_fProfileCurve = curve;
      EZ_TEST_BOOL(settings.BuildGeometry(geometry).Succeeded());
      float middleRadius = 0;
      for (const auto& vertex : geometry.GetVertices())
        if (vertex.m_vPosition.z == 0)
          middleRadius = ezMath::Max(middleRadius, vertex.m_vPosition.GetLength());
      EZ_TEST_FLOAT(middleRadius, 0.375f * (1.0f + curve), 0.00001f);
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Serialization and invalid settings")
  {
    ezGreyBoxConeSettings settings;
    settings.m_vNegative = ezVec3(36, 2, 3);
    settings.m_vPositive = ezVec3(-34, 4, 5);
    settings.m_uiSides = 7;
    settings.m_uiHeightSegments = 11;
    settings.m_fBaseRadiusScale = 1.2f;
    settings.m_fTopRadiusScale = 0.4f;
    settings.m_fProfileCurve = -0.6f;
    settings.m_bSmoothShading = false;
    ezDefaultMemoryStreamStorage storage;
    ezMemoryStreamWriter writer(&storage);
    settings.Serialize(writer);
    ezMemoryStreamReader reader(&storage);
    ezGreyBoxConeSettings restored;
    restored.Deserialize(reader);
    EZ_TEST_BOOL(restored.GetHash() == settings.GetHash());
    ezGeometry geometry;
    EZ_TEST_BOOL(restored.BuildGeometry(geometry).Succeeded());
    restored.m_fBaseRadiusScale = 0;
    restored.m_fTopRadiusScale = 0;
    EZ_TEST_BOOL(restored.BuildGeometry(geometry).Failed());
    EZ_TEST_BOOL(geometry.GetVertices().IsEmpty());
    restored = settings;
    restored.m_fProfileCurve = ezMath::NaN<float>();
    EZ_TEST_BOOL(restored.BuildGeometry(geometry).Failed());
    restored = settings;
    restored.m_vPositive = -restored.m_vNegative;
    EZ_TEST_BOOL(restored.BuildGeometry(geometry).Failed());
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Native smooth normals and per-polygon flat normals preserve positions and UVs")
  {
    for (ezUInt8 shape = 0; shape <= 12; ++shape)
    {
      ezGreyBoxConeSettings settings;
      settings.m_uiShape = shape;
      settings.m_Curvature = ezAngle::MakeFromDegree(180);
      settings.m_fThickness = 0.2f;
      ezGeometry smooth, flat;
      EZ_TEST_BOOL(settings.BuildGeometry(smooth).Succeeded());
      const ezUInt64 smoothHash = settings.GetHash();
      settings.m_bSmoothShading = false;
      EZ_TEST_BOOL(settings.GetHash() != smoothHash);
      EZ_TEST_BOOL(settings.BuildGeometry(flat).Succeeded());
      EZ_TEST_INT(smooth.GetPolygons().GetCount(), flat.GetPolygons().GetCount());
      bool changedNormals = false;
      for (ezUInt32 p = 0; p < flat.GetPolygons().GetCount(); ++p)
      {
        const auto& fp = flat.GetPolygons()[p];
        const auto& sp = smooth.GetPolygons()[p];
        EZ_TEST_INT(fp.m_Vertices.GetCount(), sp.m_Vertices.GetCount());
        const auto n = flat.GetVertices()[fp.m_Vertices[0]].m_vNormal;
        for (ezUInt32 v = 0; v < fp.m_Vertices.GetCount(); ++v)
        {
          const auto& fv = flat.GetVertices()[fp.m_Vertices[v]];
          const auto& sv = smooth.GetVertices()[sp.m_Vertices[v]];
          EZ_TEST_BOOL(fv.m_vPosition == sv.m_vPosition);
          EZ_TEST_BOOL(fv.m_vTexCoord == sv.m_vTexCoord);
          EZ_TEST_BOOL(fv.m_vNormal.IsEqual(n, 0.0001f));
          changedNormals |= !fv.m_vNormal.IsEqual(sv.m_vNormal, 0.001f);
        }
      }
      if (shape == 5)
        EZ_TEST_BOOL(changedNormals);
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter writer(&storage);
      settings.Serialize(writer);
      ezMemoryStreamReader reader(&storage);
      ezGreyBoxConeSettings restored;
      restored.Deserialize(reader);
      EZ_TEST_BOOL(restored.GetHash() == settings.GetHash());
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Version 1 cone settings remain readable")
  {
    ezGreyBoxConeSettings settings;
    ezDefaultMemoryStreamStorage storage;
    ezMemoryStreamWriter writer(&storage);
    writer << settings.m_vNegative << settings.m_vPositive << settings.m_fBaseRadiusScale << settings.m_fTopRadiusScale;
    writer << settings.m_uiSides << settings.m_uiHeightSegments << settings.m_fProfileCurve << settings.m_bSmoothShading;
    writer << ezUInt32(123456);
    ezMemoryStreamReader reader(&storage);
    ezGreyBoxConeSettings restored;
    restored.Deserialize(reader, 1);
    ezUInt32 sentinel;
    reader >> sentinel;
    EZ_TEST_INT(sentinel, 123456);
    EZ_TEST_INT(restored.m_uiShape, 13);
    EZ_TEST_BOOL(restored.GetHash() == settings.GetHash());
  }
  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Default shading and explicit box smoothing")
  {
    ezGreyBoxConeSettings settings;
    settings.m_uiShape = 0;
    EZ_TEST_BOOL(!settings.GetDefaultSmoothShading());
    settings.m_uiShape = 5;
    EZ_TEST_BOOL(settings.GetDefaultSmoothShading());
    for (ezUInt8 shape = 1; shape <= 4; ++shape)
    {
      settings.m_uiShape = shape;
      EZ_TEST_BOOL(!settings.GetDefaultSmoothShading());
    }
    settings.m_uiShape = 0;
    settings.m_bSmoothShading = true;
    ezGeometry smooth;
    EZ_TEST_BOOL(settings.BuildGeometry(smooth).Succeeded());
    for (const auto& vertex : smooth.GetVertices())
      EZ_TEST_BOOL(vertex.m_vNormal.IsEqual(vertex.m_vPosition.GetNormalized(), 0.001f));
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Ramp shading normals and tangent bases")
  {
    for (ezUInt8 shape = 1; shape <= 4; ++shape)
    {
      for (float height : {0.02f, 0.2f, 2.0f, 20.0f})
      {
        ezGreyBoxConeSettings settings;
        settings.m_uiShape = shape;
        settings.m_vNegative = settings.m_vPositive = ezVec3(4, 2, height);
        ezGeometry geometry;
        EZ_TEST_BOOL(settings.BuildGeometry(geometry).Succeeded());
        geometry.TriangulatePolygons();
        geometry.ComputeFaceNormals();
        for (const auto& polygon : geometry.GetPolygons())
          for (ezUInt32 index : polygon.m_Vertices)
          {
            const auto normal = geometry.GetVertices()[index].m_vNormal;
            EZ_TEST_BOOL(normal.IsNormalized(0.001f));
            if (ezMath::Abs(polygon.m_vNormal.z) > 0.5f)
              EZ_TEST_BOOL_MSG(normal.Dot(polygon.m_vNormal) > 0.1f, "Shape {}, height {}, normal faces away from its surface", shape, height);
          }
        geometry.ComputeTangents();
        for (const auto& vertex : geometry.GetVertices())
        {
          EZ_TEST_BOOL(vertex.m_vTangent.IsValid());
          EZ_TEST_BOOL(vertex.m_vTangent.IsNormalized(0.001f));
          EZ_TEST_FLOAT(vertex.m_vNormal.Dot(vertex.m_vTangent), 0, 0.001f);
          EZ_TEST_FLOAT(ezMath::Abs(vertex.m_fBiTangentSign), 1, 0.001f);
        }
      }
    }
  }
}
