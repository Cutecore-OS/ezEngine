#pragma once

#include <EditorEngineProcessFramework/EngineProcess/EngineProcessDocumentContext.h>
#include <EnginePluginAssets/EnginePluginAssetsDLL.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <RendererCore/Declarations.h>

class EZ_ENGINEPLUGINASSETS_DLL ezBlendShapeContext : public ezEngineProcessDocumentContext
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeContext, ezEngineProcessDocumentContext);

public:
  ezBlendShapeContext();

  virtual void HandleMessage(const ezEditorEngineDocumentMsg* pMsg) override;

  ezBlendShapeResourceHandle GetBlendShapes() const { return m_hBlendShapes; }

  bool m_bDisplayGrid = true;

protected:
  virtual void OnInitialize() override;

  virtual ezEngineProcessViewContext* CreateViewContext() override;
  virtual void DestroyViewContext(ezEngineProcessViewContext* pContext) override;
  virtual bool UpdateThumbnailViewContext(ezEngineProcessViewContext* pThumbnailViewContext) override;

private:
  void QuerySelectionBBox(const ezEditorEngineDocumentMsg* pMsg);

  ezGameObject* m_pGameObject = nullptr;
  ezBlendShapeResourceHandle m_hBlendShapes;
  ezString m_sPreviewMeshToUse;
  ezComponentHandle m_hAnimMeshComponent;
};
