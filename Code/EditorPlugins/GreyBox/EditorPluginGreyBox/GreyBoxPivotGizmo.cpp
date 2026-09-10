#include <EditorPluginGreyBox/EditorPluginGreyBoxPCH.h>

#include <EditorFramework/Assets/AssetDocument.h>
#include <EditorFramework/DocumentWindow/EngineDocumentWindow.moc.h>
#include <EditorPluginGreyBox/GreyBoxPivotGizmo.h>
#include <Foundation/Math/ColorScheme.h>

#include <QMouseEvent>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezGreyBoxPivotGizmo, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;

ezVec3 ezGreyBoxPivotGizmo::GetAnchor(ezUInt32 uiAnchor)
{
  return ezVec3(static_cast<float>(uiAnchor % 3) * 0.5f,
    static_cast<float>(uiAnchor / 3 % 3) * 0.5f, static_cast<float>(uiAnchor / 9) * 0.5f);
}

ezGreyBoxPivotGizmo::ezGreyBoxPivotGizmo()
{
  for (ezUInt32 i = 0; i < 27; ++i)
  {
    const ezVec3 vAnchor = GetAnchor(i);
    const ezUInt32 uiCenters = (vAnchor.x == 0.5f) + (vAnchor.y == 0.5f) + (vAnchor.z == 0.5f);
    ezColor color = ezColorScheme::LightUI(ezColorScheme::Yellow);
    if (uiCenters == 3)
      color = ezColorScheme::LightUI(ezColorScheme::Violet);
    else if (uiCenters == 2)
      color = ezColorScheme::LightUI(vAnchor.x != 0.5f ? ezColorScheme::Red : vAnchor.y != 0.5f ? ezColorScheme::Green
                                                                                                : ezColorScheme::Blue);
    else if (uiCenters == 1)
      color = ezColorScheme::LightUI(vAnchor.x == 0.5f ? ezColorScheme::Red : vAnchor.y == 0.5f ? ezColorScheme::Green
                                                                                                : ezColorScheme::Blue);

    m_Handles[i].ConfigureHandle(this, ezEngineGizmoHandleType::Box, color,
      ezGizmoFlags::ConstantSize | ezGizmoFlags::OnTop | ezGizmoFlags::ShowInOrtho | ezGizmoFlags::Pickable);
  }
  SetTransformation(ezTransform::MakeIdentity());
}

void ezGreyBoxPivotGizmo::SetBounds(const ezVec3& vNegative, const ezVec3& vPositive)
{
  if (m_vNegative == vNegative && m_vPositive == vPositive)
    return;
  m_vNegative = vNegative;
  m_vPositive = vPositive;
  OnTransformationChanged(GetTransformation());
}

void ezGreyBoxPivotGizmo::SetSelectedAnchor(ezUInt32 uiAnchor)
{
  EZ_ASSERT_DEV(uiAnchor < 27, "Invalid pivot anchor.");
  m_uiSelectedAnchor = uiAnchor;
  OnTransformationChanged(GetTransformation());
}

void ezGreyBoxPivotGizmo::OnSetOwner(ezQtEngineDocumentWindow* pOwnerWindow, ezQtEngineViewWidget* pOwnerView)
{
  for (auto& handle : m_Handles)
    pOwnerWindow->GetDocument()->AddSyncObject(&handle);
}

void ezGreyBoxPivotGizmo::OnVisibleChanged(bool bVisible)
{
  for (auto& handle : m_Handles)
    handle.SetVisible(bVisible);
  if (!bVisible && IsActiveInputContext())
    SetActiveInputContext(nullptr);
}

void ezGreyBoxPivotGizmo::OnTransformationChanged(const ezTransform& transform)
{
  for (ezUInt32 i = 0; i < 27; ++i)
  {
    const ezVec3 vAnchor = GetAnchor(i);
    const ezUInt32 uiCenters = (vAnchor.x == 0.5f) + (vAnchor.y == 0.5f) + (vAnchor.z == 0.5f);
    ezTransform handle = ezTransform::MakeIdentity();
    handle.m_vPosition = transform.TransformPosition(-m_vNegative + (m_vNegative + m_vPositive).CompMul(vAnchor));
    handle.m_qRotation = transform.m_qRotation;
    handle.m_vScale.Set((uiCenters == 2 ? 0.10f : uiCenters == 1 ? 0.12f
                                                                 : 0.15f) *
                        (i == m_uiSelectedAnchor ? 1.3f : 1.0f));
    m_Handles[i].SetTransformation(handle);
  }
}

ezEditorInput ezGreyBoxPivotGizmo::DoMousePressEvent(QMouseEvent* e)
{
  if (!IsVisible() || e->button() != Qt::LeftButton)
    return ezEditorInput::MayBeHandledByOthers;
  for (ezUInt32 i = 0; i < 27; ++i)
  {
    if (m_pInteractionGizmoHandle != &m_Handles[i])
      continue;
    SetSelectedAnchor(i);
    // Consume the matching release as well, so clicking a handle cannot select the object behind it.
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

ezEditorInput ezGreyBoxPivotGizmo::DoMouseReleaseEvent(QMouseEvent* e)
{
  if (!IsActiveInputContext())
    return ezEditorInput::MayBeHandledByOthers;
  if (e->button() == Qt::LeftButton)
    SetActiveInputContext(nullptr);
  return ezEditorInput::WasExclusivelyHandled;
}

ezEditorInput ezGreyBoxPivotGizmo::DoMouseMoveEvent(QMouseEvent* e)
{
  return IsActiveInputContext() ? ezEditorInput::WasExclusivelyHandled : ezEditorInput::MayBeHandledByOthers;
}

void ezGreyBoxPivotGizmo::DoFocusLost(bool bCancel)
{
  if (IsActiveInputContext())
    SetActiveInputContext(nullptr);
}
