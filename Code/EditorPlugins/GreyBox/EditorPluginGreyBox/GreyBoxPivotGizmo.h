#pragma once

#include <EditorFramework/Gizmos/GizmoBase.h>
#include <EditorPluginGreyBox/EditorPluginGreyBoxDLL.h>

/// Clickable bounding-box anchors. Picking changes only the selected anchor, never the document.
class EZ_EDITORPLUGINGREYBOX_DLL ezGreyBoxPivotGizmo : public ezGizmo
{
  EZ_ADD_DYNAMIC_REFLECTION(ezGreyBoxPivotGizmo, ezGizmo);

public:
  ezGreyBoxPivotGizmo();
  void SetBounds(const ezVec3& vNegative, const ezVec3& vPositive);
  void SetSelectedAnchor(ezUInt32 uiAnchor);
  ezUInt32 GetSelectedAnchor() const { return m_uiSelectedAnchor; }
  static ezVec3 GetAnchor(ezUInt32 uiAnchor);
  ezEngineGizmoHandle& GetHandle(ezUInt32 uiAnchor) { return m_Handles[uiAnchor]; }

protected:
  void OnSetOwner(ezQtEngineDocumentWindow* pOwnerWindow, ezQtEngineViewWidget* pOwnerView) override;
  void OnVisibleChanged(bool bVisible) override;
  void OnTransformationChanged(const ezTransform& transform) override;
  ezEditorInput DoMousePressEvent(QMouseEvent* e) override;
  ezEditorInput DoMouseReleaseEvent(QMouseEvent* e) override;
  ezEditorInput DoMouseMoveEvent(QMouseEvent* e) override;
  void DoFocusLost(bool bCancel) override;

private:
  ezEngineGizmoHandle m_Handles[27];
  ezVec3 m_vNegative = ezVec3(0.5f);
  ezVec3 m_vPositive = ezVec3(0.5f);
  ezUInt32 m_uiSelectedAnchor = 13;
};
