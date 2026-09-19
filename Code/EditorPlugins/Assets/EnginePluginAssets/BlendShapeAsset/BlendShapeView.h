#pragma once

#include <EditorEngineProcessFramework/EngineProcess/EngineProcessViewContext.h>

class ezBlendShapeContext;

class ezBlendShapeViewContext : public ezEngineProcessViewContext
{
public:
  ezBlendShapeViewContext(ezBlendShapeContext* pContext);
  ~ezBlendShapeViewContext();

  bool UpdateThumbnailCamera(const ezBoundingBoxSphere& bounds);

  virtual void Redraw(bool bRenderEditorGizmos) override;

protected:
  virtual ezViewHandle CreateView() override;
  virtual void SetCamera(const ezViewRedrawMsgToEngine* pMsg) override;

  virtual void HandleViewMessage(const ezEditorEngineViewMsg* pMsg) override;

  void PickObjectAt(ezUInt16 x, ezUInt16 y);

  ezBlendShapeContext* m_pContext = nullptr;
};
