#include <SkyBillboardPlugin/SkyBillboardPluginPCH.h>

#include <Core/Graphics/Geometry.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <RendererCore/Pipeline/RenderDataManager.h>
#include <RendererCore/Pipeline/View.h>
#include <SkyBillboardPlugin/Components/SkyBillboardComponent.h>

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezSkyBillboardComponent, 1, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_ACCESSOR_PROPERTY("Texture", GetTexture, SetTexture)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Texture_2D")),
    EZ_ACCESSOR_PROPERTY("UseCubeMap", GetUseCubeMap, SetUseCubeMap),
    EZ_RESOURCE_ACCESSOR_PROPERTY("CubeMap", GetCubeMap, SetCubeMap)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Texture_Cube")),
    EZ_ACCESSOR_PROPERTY("Size", GetSize, SetSize)->AddAttributes(new ezDefaultValueAttribute(10.0f), new ezClampValueAttribute(0.01f, 179.0f), new ezSuffixAttribute("\xC2\xB0")),
    EZ_ACCESSOR_PROPERTY("AspectRatio", GetAspectRatio, SetAspectRatio)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.01f, 100.0f)),
    EZ_ACCESSOR_PROPERTY("Color", GetColor, SetColor)->AddAttributes(new ezDefaultValueAttribute(ezColor::White)),
    EZ_ACCESSOR_PROPERTY("GlowColor", GetGlowColor, SetGlowColor)->AddAttributes(new ezDefaultValueAttribute(ezColor::White)),
    EZ_ACCESSOR_PROPERTY("GlowIntensity", GetGlowIntensity, SetGlowIntensity)->AddAttributes(new ezClampValueAttribute(0.0f, 10000.0f)),
    EZ_ACCESSOR_PROPERTY("Opacity", GetOpacity, SetOpacity)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_ACCESSOR_PROPERTY("Additive", GetAdditive, SetAdditive),
    EZ_ACCESSOR_PROPERTY("Layer", GetLayer, SetLayer),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Rendering"),
    new ezDirectionVisualizerAttribute(ezBasisAxis::PositiveX, 1.0f, ezColor::Orange),
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

ezSkyBillboardComponent::ezSkyBillboardComponent() = default;
ezSkyBillboardComponent::~ezSkyBillboardComponent() = default;

void ezSkyBillboardComponent::Initialize()
{
  SUPER::Initialize();

  const char* szMeshName = "SkyBillboardMesh";
  m_hMesh = ezResourceManager::GetExistingResource<ezMeshResource>(szMeshName);
  if (!m_hMesh.IsValid())
  {
    ezGeometry geom;
    geom.AddRect(ezVec2(2.0f));
    ezMeshResourceDescriptor desc;
    desc.MeshBufferDesc().AddStream(ezMeshVertexStreamType::Position);
    desc.MeshBufferDesc().AllocateStreamsFromGeometry(geom, ezGALPrimitiveTopology::Triangles);
    desc.AddSubMesh(2, 0, 0);
    desc.ComputeBounds();
    m_hMesh = ezResourceManager::GetOrCreateResource<ezMeshResource>(szMeshName, std::move(desc), szMeshName);
  }

  // Every component owns its material; editing one layer must not modify another world or layer.
  ezStringBuilder sMaterialName;
  sMaterialName.SetFormat("SkyBillboardMaterial_{0}", ezUuid::MakeUuid());
  ezMaterialResourceDescriptor desc;
  desc.m_hShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/SkyBillboard.ezShader");
  desc.m_RenderDataCategory = ezDefaultRenderDataCategories::LitTransparent;
  m_hMaterial = ezResourceManager::CreateResource<ezMaterialResource>(sMaterialName, std::move(desc), sMaterialName);
  UpdateMaterial();
}

void ezSkyBillboardComponent::OnDeactivated()
{
  GetWorld()->GetModule<ezRenderDataManager>()->DeleteInstanceData(m_InstanceDataOffset);
  SUPER::OnDeactivated();
}

ezResult ezSkyBillboardComponent::GetLocalBounds(ezBoundingBoxSphere& ref_bounds, bool& ref_bAlwaysVisible, ezMsgUpdateLocalBounds& ref_msg)
{
  ref_bAlwaysVisible = true;
  return EZ_SUCCESS;
}

void ezSkyBillboardComponent::OnMsgExtractRenderData(ezMsgExtractRenderData& msg) const
{
  if (msg.m_OverrideCategory != ezInvalidRenderDataCategory || msg.m_pView->GetCamera()->IsOrthographic())
    return;

  if (m_bUseCubeMap ? !m_hCubeMap.IsValid() : !m_hTexture.IsValid())
    return;

  ezTransform transform = ezTransform::MakeIdentity();
  transform.m_qRotation = GetOwner()->GetGlobalRotation();
  auto hInstanceData = msg.m_pRenderDataManager->GetOrCreateInstanceDataAndFill(
    *this, GetOwner()->IsDynamic(), transform, m_InstanceDataOffset, GetUniqueIdForRendering());

  auto pRenderData = msg.m_pRenderDataManager->CreateRenderDataForThisFrame<ezMeshRenderData>(GetOwner());
  pRenderData->Fill(m_InstanceDataOffset, hInstanceData, m_hMaterial, m_hMesh);
  // Transparent pass follows the sky pass. Saturate the sorting distance at the far plane,
  // then sort layers explicitly instead of using the material resource hash.
  pRenderData->m_fSortingDepthOffset = ezMath::MaxValue<float>();
  pRenderData->m_uiSortingKey = m_uiLayer;
  // Extraction depends on the view type; a cached perspective layer must not leak
  // into an orthographic or selection view of the same static object.
  msg.AddRenderData(pRenderData, ezDefaultRenderDataCategories::LitTransparent, ezRenderData::Caching::Never);
}

void ezSkyBillboardComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s << m_hTexture;
  s << m_hCubeMap;
  s << m_bUseCubeMap << m_fSize << m_fAspectRatio;
  s << m_Color << m_GlowColor << m_fGlowIntensity << m_fOpacity << m_bAdditive << m_uiLayer;
}

void ezSkyBillboardComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s >> m_hTexture;
  s >> m_hCubeMap;
  s >> m_bUseCubeMap >> m_fSize >> m_fAspectRatio;
  s >> m_Color >> m_GlowColor >> m_fGlowIntensity >> m_fOpacity >> m_bAdditive >> m_uiLayer;
}

void ezSkyBillboardComponent::SetTexture(const ezTexture2DResourceHandle& hTexture)
{
  m_hTexture = hTexture;
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetCubeMap(const ezTextureCubeResourceHandle& hCubeMap)
{
  m_hCubeMap = hCubeMap;
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetUseCubeMap(bool bUseCubeMap)
{
  m_bUseCubeMap = bUseCubeMap;
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetSize(float fSize)
{
  m_fSize = ezMath::Clamp(fSize, 0.01f, 179.0f);
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetAspectRatio(float fAspectRatio)
{
  m_fAspectRatio = ezMath::Clamp(fAspectRatio, 0.01f, 100.0f);
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetColor(const ezColor& color)
{
  m_Color = color;
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetGlowColor(const ezColor& color)
{
  m_GlowColor = color;
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetGlowIntensity(float fIntensity)
{
  m_fGlowIntensity = ezMath::Clamp(fIntensity, 0.0f, 10000.0f);
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetOpacity(float fOpacity)
{
  m_fOpacity = ezMath::Saturate(fOpacity);
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetAdditive(bool bAdditive)
{
  m_bAdditive = bAdditive;
  UpdateMaterial();
}

void ezSkyBillboardComponent::SetLayer(ezUInt16 uiLayer)
{
  m_uiLayer = uiLayer;
  InvalidateCachedRenderData();
}

void ezSkyBillboardComponent::UpdateMaterial()
{
  InvalidateCachedRenderData();
  if (!m_hMaterial.IsValid())
    return;

  ezResourceLock<ezMaterialResource> pMaterial(m_hMaterial, ezResourceAcquireMode::AllowLoadingFallback);
  pMaterial->SetTexture2DBinding("SkyTexture", m_hTexture);
  pMaterial->SetTextureCubeBinding("SkyCubeMap", m_hCubeMap);
  pMaterial->SetParameter("UseCubeMap", m_bUseCubeMap);
  pMaterial->SetParameter("HalfSize", ezMath::Tan(ezAngle::MakeFromDegree(m_fSize * 0.5f)));
  pMaterial->SetParameter("AspectRatio", m_fAspectRatio);
  pMaterial->SetParameter("Color", m_Color);
  pMaterial->SetParameter("GlowColor", m_GlowColor);
  pMaterial->SetParameter("GlowIntensity", m_fGlowIntensity);
  pMaterial->SetParameter("Opacity", m_fOpacity);
  pMaterial->SetParameter("Additive", m_bAdditive);
  pMaterial->PreserveCurrentDesc();
}

EZ_STATICLINK_FILE(SkyBillboardPlugin, SkyBillboardPlugin_Components_SkyBillboardComponent);
