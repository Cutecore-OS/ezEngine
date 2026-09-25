#include <DirectionalTargetIndicatorPlugin/DirectionalTargetIndicatorPluginPCH.h>

#include <DirectionalTargetIndicatorPlugin/Rendering/DirectionalTargetIndicatorRenderer.h>
#include <RendererCore/Pipeline/RenderDataBatch.h>
#include <RendererCore/RenderContext/RenderContext.h>

#include <Shaders/DirectionalTargetIndicatorConstants.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezDirectionalTargetIndicatorRenderData, 1, ezRTTIDefaultAllocator<ezDirectionalTargetIndicatorRenderData>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezDirectionalTargetIndicatorRenderer, 1, ezRTTIDefaultAllocator<ezDirectionalTargetIndicatorRenderer>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

ezDirectionalTargetIndicatorRenderer::ezDirectionalTargetIndicatorRenderer()
{
  m_hShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/DirectionalTargetIndicator.ezShader");
}

void ezDirectionalTargetIndicatorRenderer::GetSupportedRenderDataTypes(ezDynamicArray<const ezRTTI*>& out_types) const
{
  out_types.PushBack(ezGetStaticRTTI<ezDirectionalTargetIndicatorRenderData>());
}

void ezDirectionalTargetIndicatorRenderer::RenderBatch(const ezRenderViewContext& renderViewContext, const ezRenderPipelinePass* pPass, const ezRenderDataBatch& batch) const
{
  auto pContext = renderViewContext.m_pRenderContext;
  pContext->BindShader(m_hShader);
  pContext->BindNullMeshBuffer(ezGALPrimitiveTopology::Triangles, 2);

  for (auto it = batch.GetIterator<ezDirectionalTargetIndicatorRenderData>(); it.IsValid(); ++it)
  {
    for (const auto& quad : it->m_Quads)
    {
      ezDirectionalTargetIndicatorConstants constants;
      constants.CenterAndAxisX = ezVec4(quad.m_vCenter.x, quad.m_vCenter.y, quad.m_vAxisX.x, quad.m_vAxisX.y);
      constants.AxisY = ezVec4(quad.m_vAxisY.x, quad.m_vAxisY.y, 0.0f, 0.0f);
      pContext->SetPushConstants("ezDirectionalTargetIndicatorConstants", constants);
      pContext->GetBindGroup(EZ_GAL_BIND_GROUP_DRAW_CALL).BindTexture("IndicatorTexture", quad.m_hTexture);
      pContext->DrawMeshBuffer().IgnoreResult();
    }
  }
}

EZ_STATICLINK_FILE(DirectionalTargetIndicatorPlugin, DirectionalTargetIndicatorPlugin_Rendering_DirectionalTargetIndicatorRenderer);
