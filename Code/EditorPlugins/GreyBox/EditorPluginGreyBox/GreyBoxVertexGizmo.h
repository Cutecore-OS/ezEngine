#pragma once

#include <EditorFramework/Gizmos/GizmoBase.h>
#include <EditorPluginGreyBox/EditorPluginGreyBoxDLL.h>
#include <Foundation/Types/UniquePtr.h>

struct ezGreyBoxVertexMarker
{
  ezUuid m_Component;
  ezVec3 m_vLocalPosition = ezVec3::MakeZero();
  ezVec3 m_vWorldPosition = ezVec3::MakeZero();
  bool m_bPinned = false;
};

/// A pool of ordinary engine gizmo handles for actual mesh vertices, not bounding-box anchors.
class EZ_EDITORPLUGINGREYBOX_DLL ezGreyBoxVertexGizmo : public ezGizmo
{
  EZ_ADD_DYNAMIC_REFLECTION(ezGreyBoxVertexGizmo, ezGizmo);

public:
  ezGreyBoxVertexGizmo();
  void SetMarkers(ezArrayPtr<const ezGreyBoxVertexMarker> markers);
  const ezGreyBoxVertexMarker& GetPickedVertex() const { return m_PickedVertex; }
  bool OwnsHandle(const ezUuid& guid) const;
  ezEngineGizmoHandle& GetHandle(ezUInt32 uiIndex) { return *m_Handles[uiIndex]; }
  ezUInt32 GetMarkerCount() const { return m_Markers.GetCount(); }

protected:
  void OnSetOwner(ezQtEngineDocumentWindow* pOwnerWindow, ezQtEngineViewWidget* pOwnerView) override;
  void OnVisibleChanged(bool bVisible) override;
  void OnTransformationChanged(const ezTransform& transform) override {}
  ezEditorInput DoMousePressEvent(QMouseEvent* e) override;
  ezEditorInput DoMouseReleaseEvent(QMouseEvent* e) override;
  ezEditorInput DoMouseMoveEvent(QMouseEvent* e) override;
  void DoFocusLost(bool bCancel) override;

private:
  ezDynamicArray<ezUniquePtr<ezEngineGizmoHandle>> m_Handles;
  ezDynamicArray<ezGreyBoxVertexMarker> m_Markers;
  ezGreyBoxVertexMarker m_PickedVertex;
  bool m_bHasOwner = false;
};
