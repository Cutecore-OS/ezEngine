#include <GrassPlugin/GrassPluginPCH.h>

#include <GrassPlugin/Components/GrassPatchComponent.h>
#include <RendererCore/Material/MaterialResource.h>
#include <RendererCore/Pipeline/RenderDataManager.h>
#include <RendererCore/Pipeline/View.h>

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezGrassPatchComponent, 1, ezComponentMode::Static)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezHiddenAttribute(),
  }
  EZ_END_ATTRIBUTES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgExtractRenderData, OnMsgExtractRenderData),
  }
  EZ_END_MESSAGEHANDLERS;
}
EZ_END_COMPONENT_TYPE;
// clang-format on

bool ezGrassPatchComponent::IsWithinViewDistance(const ezVec3& vCameraPosition) const
{
  return m_fViewDistance > 0 && GetOwner()->GetGlobalBounds().GetBox().GetDistanceSquaredTo(vCameraPosition) < ezMath::Square(m_fViewDistance);
}

void ezGrassPatchComponent::OnMsgExtractRenderData(ezMsgExtractRenderData& ref_msg) const
{
  // Use the LOD camera, including for shadow views, just like Kraut.
  if (!m_hDynamicMesh.IsValid() || !m_hMaterial.IsValid() || !IsWithinViewDistance(ref_msg.m_pView->GetLodCamera()->GetCenterPosition()))
    return;

  const auto hInstanceBuffer = ref_msg.m_pRenderDataManager->GetOrCreateInstanceDataAndFill(*this,
    GetOwner()->IsDynamic(), GetOwner()->GetGlobalTransform(), m_InstanceDataOffset, m_uiSelectionId, m_Color, m_vCustomData);

  auto* pData = ref_msg.m_pRenderDataManager->CreateRenderDataForThisFrame<ezCustomMeshRenderData>(GetOwner());
  pData->m_uiNumInstances = 1;
  pData->m_DataOffsets.m_uiInstance = m_InstanceDataOffset.m_uiOffset;
  pData->m_hInstanceDataBuffer = hInstanceBuffer;
  pData->m_fSortingDepthOffset = m_fSortingDepthOffset;
  pData->m_hMaterial = m_hMaterial;
  pData->m_hDynamicMeshBuffer = m_hDynamicMesh;
  pData->m_uiFirstPrimitive = 0;
  pData->m_uiNumPrimitives = m_uiNumPrimitives;
#if EZ_ENABLED(EZ_COMPILE_FOR_DEVELOPMENT)
  pData->m_FallbackGlobalBBox = GetOwner()->GetGlobalBounds().GetBox();
#endif
  pData->FillSortingKey();

  ezResourceLock<ezMaterialResource> pMaterial(m_hMaterial, ezResourceAcquireMode::AllowLoadingFallback);
  // Distance and LOD are view-dependent even for static owners. Caching would bypass the distance test.
  ref_msg.AddRenderData(pData, pMaterial->GetRenderDataCategory(), ezRenderData::Caching::Never);
}

EZ_STATICLINK_FILE(GrassPlugin, GrassPlugin_Components_GrassPatchComponent);
