#pragma once

#include <DirectionalTargetIndicatorPlugin/DirectionalTargetIndicatorPluginDLL.h>
#include <RendererCore/Pipeline/RenderData.h>
#include <RendererCore/Pipeline/Renderer.h>
#include <RendererCore/Shader/ShaderResource.h>
#include <RendererCore/Textures/Texture2DResource.h>

struct ezDirectionalTargetIndicatorQuad
{
  ezTexture2DResourceHandle m_hTexture;
  ezVec2 m_vCenter;
  ezVec2 m_vAxisX;
  ezVec2 m_vAxisY;
};

/// All layers of one indicator stay together, preserving their order even with different textures.
class EZ_DIRECTIONALTARGETINDICATORPLUGIN_DLL ezDirectionalTargetIndicatorRenderData : public ezRenderData
{
  EZ_ADD_DYNAMIC_REFLECTION(ezDirectionalTargetIndicatorRenderData, ezRenderData);

public:
  ezHybridArray<ezDirectionalTargetIndicatorQuad, 3> m_Quads;
};

class EZ_DIRECTIONALTARGETINDICATORPLUGIN_DLL ezDirectionalTargetIndicatorRenderer : public ezRenderer
{
  EZ_ADD_DYNAMIC_REFLECTION(ezDirectionalTargetIndicatorRenderer, ezRenderer);

public:
  ezDirectionalTargetIndicatorRenderer();
  virtual void GetSupportedRenderDataTypes(ezDynamicArray<const ezRTTI*>& out_types) const override;
  virtual void RenderBatch(const ezRenderViewContext& renderViewContext, const ezRenderPipelinePass* pPass, const ezRenderDataBatch& batch) const override;

private:
  ezShaderResourceHandle m_hShader;
};
