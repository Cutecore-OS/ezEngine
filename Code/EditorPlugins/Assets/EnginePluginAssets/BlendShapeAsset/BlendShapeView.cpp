#include <EnginePluginAssets/EnginePluginAssetsPCH.h>

#include <EditorEngineProcessFramework/EngineProcess/EngineProcessApp.h>
#include <EditorEngineProcessFramework/Gizmos/GizmoComponent.h>
#include <EnginePluginAssets/BlendShapeAsset/BlendShapeContext.h>
#include <EnginePluginAssets/BlendShapeAsset/BlendShapeView.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

ezBlendShapeViewContext::ezBlendShapeViewContext(ezBlendShapeContext* pContext)
  : ezEngineProcessViewContext(pContext)
{
  m_pContext = pContext;

  // Start with something valid.
  m_Camera.SetCameraMode(ezCameraMode::PerspectiveFixedFovX, 45.0f, 0.1f, 1000.0f);
  m_Camera.LookAt(ezVec3(1, 1, 1), ezVec3::MakeZero(), ezVec3(0.0f, 0.0f, 1.0f));
}

ezBlendShapeViewContext::~ezBlendShapeViewContext() = default;

bool ezBlendShapeViewContext::UpdateThumbnailCamera(const ezBoundingBoxSphere& bounds)
{
  return !FocusCameraOnObject(m_Camera, bounds, 45.0f, -ezVec3(5, -2, 3));
}


void ezBlendShapeViewContext::Redraw(bool bRenderEditorGizmos)
{
  ezView* pView = nullptr;
  if (ezRenderWorld::TryGetView(m_hView, pView))
  {
    const ezTag& tagNoOrtho = ezTagRegistry::GetGlobalRegistry().RegisterTag("NotInOrthoMode");

    if (pView->GetCamera()->IsOrthographic())
    {
      pView->m_ExcludeTags.Set(tagNoOrtho);
    }
    else
    {
      pView->m_ExcludeTags.Remove(tagNoOrtho);
    }

    EZ_LOCK(pView->GetWorld()->GetWriteMarker());
    if (auto pGizmoManager = pView->GetWorld()->GetComponentManager<ezGizmoComponentManager>())
    {
      pGizmoManager->m_uiHighlightID = GetDocumentContext()->m_Context.m_uiHighlightID;
    }
  }

  ezEngineProcessViewContext::Redraw(bRenderEditorGizmos);
}

ezViewHandle ezBlendShapeViewContext::CreateView()
{
  ezView* pView = CreateDefaultView("BlendShapes Editor - View");
  return pView->GetHandle();
}

void ezBlendShapeViewContext::SetCamera(const ezViewRedrawMsgToEngine* pMsg)
{
  if (m_pContext->m_bDisplayGrid)
  {
    ezEngineProcessViewContext::DrawSimpleGrid();
  }

  ezEngineProcessViewContext::SetCamera(pMsg);

  const ezUInt32 viewHeight = pMsg->m_uiWindowHeight;

  auto hBlendShapes = m_pContext->GetBlendShapes();
  if (hBlendShapes.IsValid())
  {
    ezResourceLock<ezBlendShapeResource> pBlendShapes(hBlendShapes, ezResourceAcquireMode::AllowLoadingFallback);

    const ezUInt32 uiNumChannels = pBlendShapes->GetDescriptor().m_Channels.GetCount();

    ezStringBuilder sText;
    sText.AppendFormat("Channels: {}\n", uiNumChannels);

    ezDebugRenderer::Draw2DText(m_hView, sText, ezVec2I32(10, viewHeight - 10), ezColor::White, 16, ezDebugTextHAlign::Left,
      ezDebugTextVAlign::Bottom);
  }
}

void ezBlendShapeViewContext::HandleViewMessage(const ezEditorEngineViewMsg* pMsg)
{
  if (pMsg->GetDynamicRTTI()->IsDerivedFrom<ezViewPickingMsgToEngine>())
  {
    const ezViewPickingMsgToEngine* pMsg2 = static_cast<const ezViewPickingMsgToEngine*>(pMsg);

    ezView* pView = nullptr;
    if (ezRenderWorld::TryGetView(m_hView, pView))
    {
      pView->GetBlackboard()->SetEntryValue(ezMakeHashedString("EditorPickingPass.Active"), true);
      pView->GetBlackboard()->SetEntryValue(ezMakeHashedString("EditorPickingPass.PickSelected"), true);
    }

    PickObjectAt(pMsg2->m_uiPickPosX, pMsg2->m_uiPickPosY);
  }
  else
  {
    ezEngineProcessViewContext::HandleViewMessage(pMsg);
  }
}

void ezBlendShapeViewContext::PickObjectAt(ezUInt16 x, ezUInt16 y)
{
  // remote processes do not support picking, just ignore this
  if (ezEditorEngineProcessApp::GetSingleton()->IsRemoteMode())
    return;

  ezViewPickingResultMsgToEditor res;
  EZ_SCOPE_EXIT(SendViewMessage(&res));

  ezView* pView = nullptr;
  if (ezRenderWorld::TryGetView(m_hView, pView) == false)
    return;

  auto pBlackboard = pView->GetBlackboard();
  pBlackboard->SetEntryValue(ezMakeHashedString("EditorPickingPass.PickingPosition"), ezVec2(x, y));

  auto pPickedPositionEntry = pBlackboard->GetEntry("EditorPickingPass.PickedPosition");
  if (pPickedPositionEntry == nullptr || pPickedPositionEntry->m_Value.IsA<ezVec3>() == false)
    return;

  const ezUInt32 uiPickingID = pBlackboard->GetEntryValue("EditorPickingPass.PickedID").ConvertTo<ezUInt32>();
  res.m_vPickedNormal = pBlackboard->GetEntryValue("EditorPickingPass.PickedNormal").ConvertTo<ezVec3>();
  res.m_vPickingRayStartPosition = pBlackboard->GetEntryValue("EditorPickingPass.PickedRayStartPosition").ConvertTo<ezVec3>();
  res.m_vPickedPosition = pPickedPositionEntry->m_Value.ConvertTo<ezVec3>();

  EZ_ASSERT_DEBUG(!res.m_vPickedPosition.IsNaN(), "");

  const ezUInt32 uiComponentID = (uiPickingID & 0x00FFFFFF);
  const ezUInt32 uiPartIndex = (uiPickingID >> 24) & 0xFF;

  res.m_ComponentGuid = GetDocumentContext()->m_Context.m_ComponentPickingMap.GetGuid(uiComponentID);
  res.m_OtherGuid = GetDocumentContext()->m_Context.m_OtherPickingMap.GetGuid(uiComponentID);

  if (res.m_ComponentGuid.IsValid())
  {
    ezComponentHandle hComponent = GetDocumentContext()->m_Context.m_ComponentMap.GetHandle(res.m_ComponentGuid);

    ezEngineProcessDocumentContext* pDocumentContext = GetDocumentContext();

    // check whether the component is still valid
    ezComponent* pComponent = nullptr;
    if (pDocumentContext->GetWorld()->TryGetComponent<ezComponent>(hComponent, pComponent))
    {
      // if yes, fill out the parent game object guid
      res.m_ObjectGuid = GetDocumentContext()->m_Context.m_GameObjectMap.GetGuid(pComponent->GetOwner()->GetHandle());
      res.m_uiPartIndex = uiPartIndex;
    }
    else
    {
      res.m_ComponentGuid = ezUuid();
    }
  }
}
