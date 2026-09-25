#pragma once
#include <RendererCore/RendererCoreDLL.h>
#include <RendererCore/Meshes/DynamicMeshBufferResource.h>
#include <RendererCore/Meshes/MeshRenderer.h>
#include <RendererCore/Meshes/SkinnedMeshRenderData.h>

class EZ_RENDERERCORE_DLL ezBlendShapeRenderData : public ezSkinnedMeshRenderData
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeRenderData, ezSkinnedMeshRenderData);

public:
  ezDynamicMeshBufferResourceHandle m_hDeformedMesh;
  virtual bool CanBatch(const ezRenderData& other) const override;
};

class EZ_RENDERERCORE_DLL ezBlendShapeRenderer : public ezMeshRenderer
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeRenderer, ezMeshRenderer);

public:
  virtual void GetSupportedRenderDataTypes(ezDynamicArray<const ezRTTI*>& out_types) const override;

protected:
  virtual void SetAdditionalData(const ezRenderViewContext& context, const ezMeshRenderData* pData) const override;
};
