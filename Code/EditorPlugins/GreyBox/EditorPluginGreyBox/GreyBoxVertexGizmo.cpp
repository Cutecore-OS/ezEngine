#include <EditorPluginGreyBox/EditorPluginGreyBoxPCH.h>

#include <EditorFramework/Assets/AssetDocument.h>
#include <EditorFramework/DocumentWindow/EngineDocumentWindow.moc.h>
#include <EditorPluginGreyBox/GreyBoxVertexGizmo.h>
#include <Foundation/Math/ColorScheme.h>

#include <QMouseEvent>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezGreyBoxVertexGizmo, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;

ezGreyBoxVertexGizmo::ezGreyBoxVertexGizmo()
{
  SetTransformation(ezTransform::MakeIdentity());
}

void ezGreyBoxVertexGizmo::SetMarkers(ezArrayPtr<const ezGreyBoxVertexMarker> markers)
{
  // Picking results refer to these handles until the mouse is released.
  if (IsActiveInputContext())
    return;
  m_Markers = markers;
  while (m_Handles.GetCount() < markers.GetCount())
  {
    ezUniquePtr<ezEngineGizmoHandle> handle = EZ_DEFAULT_NEW(ezEngineGizmoHandle);
    handle->ConfigureHandle(this, ezEngineGizmoHandleType::Box, ezColorScheme::LightUI(ezColorScheme::Yellow),
      ezGizmoFlags::ConstantSize | ezGizmoFlags::OnTop | ezGizmoFlags::ShowInOrtho | ezGizmoFlags::Pickable);
    if (m_bHasOwner)
      GetOwnerWindow()->GetDocument()->AddSyncObject(handle.Borrow());
    m_Handles.PushBack(std::move(handle));
  }
  for (ezUInt32 i = 0; i < m_Handles.GetCount(); ++i)
  {
    if (i < markers.GetCount())
    {
      ezTransform transform = ezTransform::MakeIdentity();
      transform.m_vPosition = markers[i].m_vWorldPosition;
      transform.m_vScale.Set(markers[i].m_bPinned ? 0.12f : 0.075f);
      m_Handles[i]->SetTransformation(transform);
      m_Handles[i]->SetColor(ezColorScheme::LightUI(markers[i].m_bPinned ? ezColorScheme::Orange : ezColorScheme::Yellow));
    }
    m_Handles[i]->SetVisible(IsVisible() && i < markers.GetCount());
  }
}

bool ezGreyBoxVertexGizmo::OwnsHandle(const ezUuid& guid) const
{
  for (ezUInt32 i = 0; i < m_Markers.GetCount(); ++i)
  {
    if (m_Handles[i]->GetGuid() == guid)
      return true;
  }
  return false;
}

void ezGreyBoxVertexGizmo::OnSetOwner(ezQtEngineDocumentWindow* pOwnerWindow, ezQtEngineViewWidget* pOwnerView)
{
  m_bHasOwner = true;
  for (auto& handle : m_Handles)
    pOwnerWindow->GetDocument()->AddSyncObject(handle.Borrow());
}

void ezGreyBoxVertexGizmo::OnVisibleChanged(bool bVisible)
{
  for (ezUInt32 i = 0; i < m_Handles.GetCount(); ++i)
    m_Handles[i]->SetVisible(bVisible && i < m_Markers.GetCount());
  if (!bVisible && IsActiveInputContext())
    SetActiveInputContext(nullptr);
}

ezEditorInput ezGreyBoxVertexGizmo::DoMousePressEvent(QMouseEvent* e)
{
  if (!IsVisible() || e->button() != Qt::LeftButton || e->modifiers() != Qt::NoModifier)
    return ezEditorInput::MayBeHandledByOthers;
  for (ezUInt32 i = 0; i < m_Markers.GetCount(); ++i)
  {
    if (m_pInteractionGizmoHandle != m_Handles[i].Borrow())
      continue;
    m_PickedVertex = m_Markers[i];
    SetMouseMode(MouseMode::Normal);
    SetActiveInputContext(this);
    ezGizmoEvent event;
    event.m_pGizmo = this;
    event.m_Type = ezGizmoEvent::Type::Interaction;
    m_GizmoEvents.Broadcast(event);
    return ezEditorInput::WasExclusivelyHandled;
  }
  return ezEditorInput::MayBeHandledByOthers;
}

ezEditorInput ezGreyBoxVertexGizmo::DoMouseReleaseEvent(QMouseEvent* e)
{
  if (!IsActiveInputContext())
    return ezEditorInput::MayBeHandledByOthers;
  if (e->button() == Qt::LeftButton)
    SetActiveInputContext(nullptr);
  return ezEditorInput::WasExclusivelyHandled;
}

ezEditorInput ezGreyBoxVertexGizmo::DoMouseMoveEvent(QMouseEvent* e)
{
  return IsActiveInputContext() ? ezEditorInput::WasExclusivelyHandled : ezEditorInput::MayBeHandledByOthers;
}

void ezGreyBoxVertexGizmo::DoFocusLost(bool bCancel)
{
  if (IsActiveInputContext())
    SetActiveInputContext(nullptr);
}
