#include <RendererCore/RendererCorePCH.h>

#include <RendererCore/AnimationSystem/EditableBlendShapes.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezEditableBlendShapeChannel, 1, ezRTTIDefaultAllocator<ezEditableBlendShapeChannel>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Name", m_sName)->AddAttributes(new ezReadOnlyAttribute()),
    EZ_MEMBER_PROPERTY("DefaultWeight", m_fDefaultWeight)->AddAttributes(new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("MinWeight", m_fMinWeight),
    EZ_MEMBER_PROPERTY("MaxWeight", m_fMaxWeight),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezEditableBlendShapes, 1, ezRTTIDefaultAllocator<ezEditableBlendShapes>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("File", m_sSourceFile)->AddAttributes(new ezFileBrowserAttribute("Select Mesh", ezFileBrowserAttribute::MeshesWithAnimations), new ezRequiredAttribute()),
    EZ_MEMBER_PROPERTY("PreviewMesh", m_sPreviewMesh)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_Skinned")),
    EZ_ARRAY_MEMBER_PROPERTY("Channels", m_Channels)->AddAttributes(new ezContainerAttribute(false, false, false)),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezEditableBlendShapes::ezEditableBlendShapes() = default;
ezEditableBlendShapes::~ezEditableBlendShapes() = default;

void ezEditableBlendShapes::FillResourceDescriptor(ezBlendShapeResourceDescriptor& ref_desc) const
{
  ref_desc.m_MeshBufferDesc = m_MeshBufferDesc;
  ref_desc.m_Channels.SetCount(m_Channels.GetCount());

  for (ezUInt32 i = 0; i < m_Channels.GetCount(); ++i)
  {
    ref_desc.m_Channels[i].m_sName.Assign(m_Channels[i].m_sName);
    ref_desc.m_Channels[i].m_fDefaultWeight = m_Channels[i].m_fDefaultWeight;
    ref_desc.m_Channels[i].m_fMinWeight = m_Channels[i].m_fMinWeight;
    ref_desc.m_Channels[i].m_fMaxWeight = m_Channels[i].m_fMaxWeight;
    ref_desc.m_Channels[i].m_Deltas = m_Channels[i].m_Deltas;
  }
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_AnimationSystem_Implementation_EditableBlendShapes);
