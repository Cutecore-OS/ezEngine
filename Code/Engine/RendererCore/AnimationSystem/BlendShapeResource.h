#pragma once
#include <RendererCore/RendererCoreDLL.h>
#include <RendererCore/Meshes/MeshResource.h>

/// Sparse deltas in the same mesh space and vertex order as the embedded base mesh.
struct EZ_RENDERERCORE_DLL ezBlendShapeDelta
{
  ezUInt32 m_uiVertex = 0;
  ezVec3 m_vPosition = ezVec3::MakeZero();
  ezVec3 m_vNormal = ezVec3::MakeZero();
  ezVec3 m_vTangent = ezVec3::MakeZero();
};

struct EZ_RENDERERCORE_DLL ezBlendShapeTarget
{
  ezHashedString m_sName;
  float m_fDefaultWeight = 0.0f;
  ezDynamicArray<ezBlendShapeDelta> m_Deltas;
};

struct EZ_RENDERERCORE_DLL ezBlendShapeResourceDescriptor
{
  ezString m_sMesh; ///< Associated Animated Mesh resource ID; empty for procedural data.
  ezMeshResourceDescriptor m_Mesh;
  ezDynamicArray<ezBlendShapeTarget> m_Targets;

  ezResult Save(ezStreamWriter& inout_stream);
  ezResult Load(ezStreamReader& inout_stream);
  ezResult Validate() const;
  ezUInt64 GetHeapMemoryUsage() const;

  /// Namespaced curves cannot accidentally consume ordinary gameplay curves.
  static void MakeCurveName(ezStringView sTarget, ezStringBuilder& out_sName);
  static float SanitizeWeight(float fWeight, float fThreshold);

  /// Rebuild from the base pose; never accumulate on the previous frame.
  void Deform(ezArrayPtr<const float> weights, ezDynamicArray<ezVec3>& out_positions,
    ezDynamicArray<ezVec3>& out_normals, ezDynamicArray<ezVec4>& out_tangents) const;
};

using ezBlendShapeResourceHandle = ezTypedResourceHandle<class ezBlendShapeResource>;

class EZ_RENDERERCORE_DLL ezBlendShapeResource : public ezResource
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeResource, ezResource);
  EZ_RESOURCE_DECLARE_COMMON_CODE(ezBlendShapeResource);
  EZ_RESOURCE_DECLARE_CREATEABLE(ezBlendShapeResource, ezBlendShapeResourceDescriptor);

public:
  ezBlendShapeResource();
  const ezBlendShapeResourceDescriptor& GetDescriptor() const { return m_Descriptor; }

private:
  virtual ezResourceLoadDesc UnloadData(Unload what) override;
  virtual ezResourceLoadDesc UpdateContent(ezStreamReader* pStream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_usage) override;
  ezBlendShapeResourceDescriptor m_Descriptor;
};
