#include <RendererCore/RendererCorePCH.h>
#include <Core/World/World.h>
#include <Foundation/Reflection/Reflection.h>
#include <RendererCore/Meshes/BlendShapeRenderer.h>
#include <RendererCore/Pipeline/RenderPipelinePass.h>
#include <RendererCore/RenderContext/RenderContext.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeRenderData, 1, ezRTTIDefaultAllocator<ezBlendShapeRenderData>)
EZ_END_DYNAMIC_REFLECTED_TYPE;
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeRenderer, 1, ezRTTIDefaultAllocator<ezBlendShapeRenderer>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

bool ezBlendShapeRenderData::CanBatch(const ezRenderData& other) const
{
  return m_hDeformedMesh == static_cast<const ezBlendShapeRenderData&>(other).m_hDeformedMesh && SUPER::CanBatch(other);
}

void ezBlendShapeRenderer::GetSupportedRenderDataTypes(ezDynamicArray<const ezRTTI*>& out_types) const
{
  out_types.PushBack(ezGetStaticRTTI<ezBlendShapeRenderData>());
}

void ezBlendShapeRenderer::SetAdditionalData(const ezRenderViewContext& context, const ezMeshRenderData* pData) const
{
  auto pMorph = static_cast<const ezBlendShapeRenderData*>(pData);
  if (!pMorph->m_hDeformedMesh.IsValid())
    return;
  ezResourceLock<ezDynamicMeshBufferResource> pBuffer(pMorph->m_hDeformedMesh, ezResourceAcquireMode::BlockTillLoaded);
  const auto buffers = pBuffer->GetVertexBuffers();
  context.m_pRenderContext->BindVertexBuffer(buffers[ezMeshVertexStreamType::Position], ezMeshVertexStreamType::Position);
  context.m_pRenderContext->BindVertexBuffer(buffers[ezMeshVertexStreamType::NormalTangentAndTexCoord0], ezMeshVertexStreamType::NormalTangentAndTexCoord0);
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_Meshes_Implementation_BlendShapeRenderer);
