#include <EnginePluginAssets/EnginePluginAssetsPCH.h>

#include <EnginePluginAssets/BlendShapeAsset/BlendShapeContext.h>
#include <EnginePluginAssets/BlendShapeAsset/BlendShapeView.h>

#include <GameEngine/Animation/Skeletal/AnimatedMeshComponent.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeContext, 1, ezRTTIDefaultAllocator<ezBlendShapeContext>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_CONSTANT_PROPERTY("DocumentType", (const char*) "BlendShapes"),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezBlendShapeContext::ezBlendShapeContext()
  : ezEngineProcessDocumentContext(ezEngineProcessDocumentContextFlags::CreateWorld)
{
}

void ezBlendShapeContext::HandleMessage(const ezEditorEngineDocumentMsg* pDocMsg)
{
  if (auto pMsg = ezDynamicCast<const ezQuerySelectionBBoxMsgToEngine*>(pDocMsg))
  {
    QuerySelectionBBox(pMsg);
    return;
  }

  if (auto pMsg = ezDynamicCast<const ezSimpleDocumentConfigMsgToEngine*>(pDocMsg))
  {
    if (pMsg->m_sWhatToDo == "CommonAssetUiState")
    {
      if (pMsg->m_sPayload == "Grid")
      {
        m_bDisplayGrid = pMsg->m_PayloadValue.ConvertTo<float>() > 0;
        return;
      }
    }
    else if (pMsg->m_sWhatToDo == "PreviewMesh" && m_sPreviewMeshToUse != pMsg->m_sPayload)
    {
      m_sPreviewMeshToUse = pMsg->m_sPayload;

      auto pWorld = m_pWorld;
      EZ_LOCK(pWorld->GetWriteMarker());

      ezAnimatedMeshComponent* pAnimMesh;
      if (pWorld->TryGetComponent(m_hAnimMeshComponent, pAnimMesh))
      {
        m_hAnimMeshComponent.Invalidate();
        pAnimMesh->DeleteComponent();
      }

      if (!m_sPreviewMeshToUse.IsEmpty())
      {
        m_hAnimMeshComponent = ezAnimatedMeshComponent::CreateComponent(m_pGameObject, pAnimMesh);
        pAnimMesh->SetMeshFile(m_sPreviewMeshToUse);
        pAnimMesh->SetDefaultBlendShapes(m_hBlendShapes);
      }
    }
  }

  ezEngineProcessDocumentContext::HandleMessage(pDocMsg);
}

void ezBlendShapeContext::OnInitialize()
{
  auto pWorld = m_pWorld;
  EZ_LOCK(pWorld->GetWriteMarker());

  ezGameObjectDesc obj;

  // Preview Mesh
  {
    obj.m_sName.Assign("BlendShapePreview");
    obj.m_bDynamic = true;
    pWorld->CreateObject(obj, m_pGameObject);

    ezStringBuilder sBlendShapeGuid;
    ezConversionUtils::ToString(GetDocumentGuid(), sBlendShapeGuid);
    m_hBlendShapes = ezResourceManager::LoadResource<ezBlendShapeResource>(sBlendShapeGuid);
  }
}

ezEngineProcessViewContext* ezBlendShapeContext::CreateViewContext()
{
  return EZ_DEFAULT_NEW(ezBlendShapeViewContext, this);
}

void ezBlendShapeContext::DestroyViewContext(ezEngineProcessViewContext* pContext)
{
  EZ_DEFAULT_DELETE(pContext);
}

bool ezBlendShapeContext::UpdateThumbnailViewContext(ezEngineProcessViewContext* pThumbnailViewContext)
{
  ezBoundingBoxSphere bounds = GetWorldBounds(m_pWorld);

  ezBlendShapeViewContext* pMeshViewContext = static_cast<ezBlendShapeViewContext*>(pThumbnailViewContext);
  return pMeshViewContext->UpdateThumbnailCamera(bounds);
}


void ezBlendShapeContext::QuerySelectionBBox(const ezEditorEngineDocumentMsg* pMsg)
{
  if (m_pGameObject == nullptr)
    return;

  ezBoundingBoxSphere bounds = ezBoundingBoxSphere::MakeInvalid();

  {
    EZ_LOCK(m_pWorld->GetWriteMarker());

    m_pGameObject->UpdateLocalBounds();
    m_pGameObject->UpdateGlobalTransformAndBounds();
    const auto& b = m_pGameObject->GetGlobalBounds();

    if (b.IsValid())
      bounds.ExpandToInclude(b);
  }

  const ezQuerySelectionBBoxMsgToEngine* msg = static_cast<const ezQuerySelectionBBoxMsgToEngine*>(pMsg);

  ezQuerySelectionBBoxResultMsgToEditor res;
  res.m_uiViewID = msg->m_uiViewID;
  res.m_iPurpose = msg->m_iPurpose;
  res.m_vCenter = bounds.m_vCenter;
  res.m_vHalfExtents = bounds.m_vBoxHalfExtents;
  res.m_DocumentGuid = pMsg->m_DocumentGuid;

  SendProcessMessage(&res);
}
