#pragma once

#include <RendererCore/RendererCoreDLL.h>

#include <Core/ResourceManager/Resource.h>
#include <Core/ResourceManager/ResourceHandle.h>
#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Strings/HashedString.h>
#include <RendererCore/Meshes/MeshBufferResource.h>

/// A single sparse vertex delta for a blend shape channel.
struct EZ_RENDERERCORE_DLL ezBlendShapeVertexDelta
{
  EZ_DECLARE_POD_TYPE();

  ezUInt32 m_uiVertexIndex = 0;
  ezVec3 m_vPositionDelta = ezVec3::MakeZero();
  ezVec3 m_vNormalDelta = ezVec3::MakeZero();
};

/// Represents a single blend shape (morph target / shape key) channel.
struct EZ_RENDERERCORE_DLL ezBlendShapeChannel
{
  ezHashedString m_sName;
  float m_fDefaultWeight = 0.0f;
  float m_fMinWeight = 0.0f;
  float m_fMaxWeight = 1.0f;
  ezDynamicArray<ezBlendShapeVertexDelta> m_Deltas;

  ezResult Serialize(ezStreamWriter& inout_stream) const;
  ezResult Deserialize(ezStreamReader& inout_stream);
};

/// Descriptor containing all blend shape channels and base mesh geometry.
struct EZ_RENDERERCORE_DLL ezBlendShapeResourceDescriptor
{
  ezBlendShapeResourceDescriptor();
  ~ezBlendShapeResourceDescriptor();
  ezBlendShapeResourceDescriptor(ezBlendShapeResourceDescriptor&& rhs) noexcept;
  void operator=(ezBlendShapeResourceDescriptor&& rhs) noexcept;

  ezResult Serialize(ezStreamWriter& inout_stream) const;
  ezResult Deserialize(ezStreamReader& inout_stream);

  ezUInt64 GetHeapMemoryUsage() const;

  ezUInt32 FindChannelByName(const ezTempHashedString& sName) const;

  ezDynamicArray<ezVec3> m_BasePositions;
  ezDynamicArray<ezVec3> m_BaseNormals;
  ezDynamicArray<ezBlendShapeChannel> m_Channels;
  ezMeshBufferResourceDescriptor m_MeshBufferDesc;
};

using ezBlendShapeResourceHandle = ezTypedResourceHandle<class ezBlendShapeResource>;

/// Runtime resource containing blend shapes (morph targets / shape keys).
class EZ_RENDERERCORE_DLL ezBlendShapeResource : public ezResource
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeResource, ezResource);
  EZ_RESOURCE_DECLARE_COMMON_CODE(ezBlendShapeResource);
  EZ_RESOURCE_DECLARE_CREATEABLE(ezBlendShapeResource, ezBlendShapeResourceDescriptor);

public:
  ezBlendShapeResource();
  ~ezBlendShapeResource();

  const ezBlendShapeResourceDescriptor& GetDescriptor() const { return *m_pDescriptor; }

  ezUInt32 FindChannelByName(const ezTempHashedString& sName) const
  {
    return m_pDescriptor ? m_pDescriptor->FindChannelByName(sName) : ezInvalidIndex;
  }

private:
  virtual ezResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual ezResourceLoadDesc UpdateContent(ezStreamReader* Stream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

  ezUniquePtr<ezBlendShapeResourceDescriptor> m_pDescriptor;
};
