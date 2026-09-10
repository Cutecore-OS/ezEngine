#include <GreyBoxPlugin/GreyBoxPluginPCH.h>

#include <Core/Graphics/Geometry.h>
#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <GreyBoxPlugin/Components/GreyBoxConeComponent.h>
#include <RendererCore/Meshes/CpuMeshResource.h>
#include <RendererCore/Pipeline/RenderData.h>
#include <RendererCore/Utils/WorldGeoExtractionUtil.h>

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_ENUM(ezGreyBoxConeShape, 2)
  EZ_ENUM_CONSTANTS(ezGreyBoxConeShape::Box, ezGreyBoxConeShape::RampPosX, ezGreyBoxConeShape::RampNegX, ezGreyBoxConeShape::RampPosY, ezGreyBoxConeShape::RampNegY)
  EZ_ENUM_CONSTANTS(ezGreyBoxConeShape::Column, ezGreyBoxConeShape::StairsPosX, ezGreyBoxConeShape::StairsNegX, ezGreyBoxConeShape::StairsPosY, ezGreyBoxConeShape::StairsNegY)
  EZ_ENUM_CONSTANTS(ezGreyBoxConeShape::ArchX, ezGreyBoxConeShape::ArchY, ezGreyBoxConeShape::SpiralStairs, ezGreyBoxConeShape::Cone)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_COMPONENT_TYPE(ezGreyBoxConeComponent, 2, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ENUM_ACCESSOR_PROPERTY("Shape", ezGreyBoxConeShape, GetShape, SetShape),
    EZ_RESOURCE_ACCESSOR_PROPERTY("Material", GetMaterial, SetMaterial)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Material")),
    EZ_ACCESSOR_PROPERTY("Color", GetColor, SetColor)->AddAttributes(new ezExposeColorAlphaAttribute()),
    EZ_ACCESSOR_PROPERTY("CustomData", GetCustomData, SetCustomData)->AddAttributes(new ezDefaultValueAttribute(ezVec4(0, 1, 0, 1))),
    EZ_ACCESSOR_PROPERTY("SizeNegX", GetSizeNegX, SetSizeNegX)->AddAttributes(new ezGroupAttribute("Size", "Size"), new ezDefaultValueAttribute(0.5f)),
    EZ_ACCESSOR_PROPERTY("SizePosX", GetSizePosX, SetSizePosX)->AddAttributes(new ezDefaultValueAttribute(0.5f)),
    EZ_ACCESSOR_PROPERTY("SizeNegY", GetSizeNegY, SetSizeNegY)->AddAttributes(new ezDefaultValueAttribute(0.5f)),
    EZ_ACCESSOR_PROPERTY("SizePosY", GetSizePosY, SetSizePosY)->AddAttributes(new ezDefaultValueAttribute(0.5f)),
    EZ_ACCESSOR_PROPERTY("SizeNegZ", GetSizeNegZ, SetSizeNegZ)->AddAttributes(new ezDefaultValueAttribute(0.5f)),
    EZ_ACCESSOR_PROPERTY("SizePosZ", GetSizePosZ, SetSizePosZ)->AddAttributes(new ezDefaultValueAttribute(0.5f)),
    EZ_ACCESSOR_PROPERTY("BaseRadiusScale", GetBaseRadiusScale, SetBaseRadiusScale)->AddAttributes(new ezGroupAttribute("Cone"), new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 4.0f)),
    EZ_ACCESSOR_PROPERTY("TopRadiusScale", GetTopRadiusScale, SetTopRadiusScale)->AddAttributes(new ezClampValueAttribute(0.0f, 4.0f)),
    EZ_ACCESSOR_PROPERTY("Sides", GetSides, SetSides)->AddAttributes(new ezDefaultValueAttribute(32), new ezClampValueAttribute(3, 128)),
    EZ_ACCESSOR_PROPERTY("HeightSegments", GetHeightSegments, SetHeightSegments)->AddAttributes(new ezDefaultValueAttribute(8), new ezClampValueAttribute(1, 64)),
    EZ_ACCESSOR_PROPERTY("ProfileCurve", GetProfileCurve, SetProfileCurve)->AddAttributes(new ezClampValueAttribute(-0.95f, 1.0f)),
    EZ_ACCESSOR_PROPERTY("SmoothShading", GetSmoothShading, SetSmoothShading)->AddAttributes(new ezGroupAttribute("Misc"), new ezDefaultValueAttribute(true)),
    EZ_ACCESSOR_PROPERTY("Detail", GetDetail, SetDetail)->AddAttributes(new ezGroupAttribute("Misc"), new ezDefaultValueAttribute(16), new ezClampValueAttribute(3, 32)),
    EZ_ACCESSOR_PROPERTY("Curvature", GetCurvature, SetCurvature)->AddAttributes(new ezClampValueAttribute(ezAngle::MakeFromDegree(-360), ezAngle::MakeFromDegree(360))),
    EZ_ACCESSOR_PROPERTY("Thickness", GetThickness, SetThickness)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, ezVariant())),
    EZ_ACCESSOR_PROPERTY("SlopedTop", GetSlopedTop, SetSlopedTop),
    EZ_ACCESSOR_PROPERTY("SlopedBottom", GetSlopedBottom, SetSlopedBottom),
    EZ_ACCESSOR_PROPERTY("GenerateCollision", GetGenerateCollision, SetGenerateCollision)->AddAttributes(new ezGroupAttribute("Misc"), new ezDefaultValueAttribute(true)),
    EZ_ACCESSOR_PROPERTY("UseAsOccluder", GetUseAsOccluder, SetUseAsOccluder)->AddAttributes(new ezDefaultValueAttribute(true)),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Construction"),
    new ezNonUniformBoxManipulatorAttribute("SizeNegX", "SizePosX", "SizeNegY", "SizePosY", "SizeNegZ", "SizePosZ"),
  }
  EZ_END_ATTRIBUTES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgBuildStaticMesh, OnBuildStaticMesh),
    EZ_MESSAGE_HANDLER(ezMsgExtractGeometry, OnMsgExtractGeometry),
    EZ_MESSAGE_HANDLER(ezMsgExtractOccluderData, OnMsgExtractOccluderData),
  }
  EZ_END_MESSAGEHANDLERS;
}
EZ_END_COMPONENT_TYPE;
// clang-format on

void ezGreyBoxConeComponent::SerializeComponent(ezWorldWriter& stream) const
{
  // The mesh is a procedural cache, so serialize its settings instead of the transient resource.
  ezRenderComponent::SerializeComponent(stream);
  auto& s = stream.GetStream();
  m_Settings.Serialize(s);
  s << GetMaterial();
  s << m_Color << m_vCustomData << m_bGenerateCollision << m_bUseAsOccluder;
}

void ezGreyBoxConeComponent::DeserializeComponent(ezWorldReader& stream)
{
  ezRenderComponent::DeserializeComponent(stream);
  auto& s = stream.GetStream();
  m_Settings.Deserialize(s, stream.GetComponentTypeVersion(GetStaticRTTI()));
  m_Materials.SetCount(1);
  s >> m_Materials[0];
  s >> m_Color >> m_vCustomData >> m_bGenerateCollision >> m_bUseAsOccluder;
}

void ezGreyBoxConeComponent::OnActivated()
{
  RebuildMesh();
  SUPER::OnActivated();
}

void ezGreyBoxConeComponent::GeometryChanged()
{
  m_pOccluder = nullptr;
  if (IsActiveAndInitialized())
    RebuildMesh();
}

bool ezGreyBoxConeComponent::BuildDescriptor(ezMeshResourceDescriptor& descriptor) const
{
  ezGeometry geometry;
  if (m_Settings.BuildGeometry(geometry).Failed())
    return false;
  geometry.TriangulatePolygons();
  geometry.ComputeTangents();
  descriptor.SetMaterial(0, "{ 1c47ee4c-0379-4280-85f5-b8cda61941d2 }");
  descriptor.MeshBufferDesc().AddCommonStreams();
  descriptor.MeshBufferDesc().AllocateStreamsFromGeometry(geometry, ezGALPrimitiveTopology::Triangles);
  descriptor.AddSubMesh(descriptor.MeshBufferDesc().GetPrimitiveCount(), 0, 0);
  descriptor.ComputeBounds();
  return true;
}

void ezGreyBoxConeComponent::RebuildMesh()
{
  ezStringBuilder name;
  name.SetFormat("GreyBoxCone:{}", m_Settings.GetHash());
  auto mesh = ezResourceManager::GetExistingResource<ezMeshResource>(name);
  if (!mesh.IsValid())
  {
    ezMeshResourceDescriptor descriptor;
    if (BuildDescriptor(descriptor))
      mesh = ezResourceManager::GetOrCreateResource<ezMeshResource>(name, std::move(descriptor), name);
  }
  SetMesh(mesh);
}

ezResult ezGreyBoxConeComponent::GetLocalBounds(ezBoundingBoxSphere& bounds, bool& alwaysVisible, ezMsgUpdateLocalBounds& msg)
{
  EZ_SUCCEED_OR_RETURN(SUPER::GetLocalBounds(bounds, alwaysVisible, msg));
  if (m_bUseAsOccluder)
    msg.AddBounds(bounds, GetOwner()->IsStatic() ? ezDefaultSpatialDataCategories::OcclusionStatic : ezDefaultSpatialDataCategories::OcclusionDynamic);
  return EZ_SUCCESS;
}

void ezGreyBoxConeComponent::SetUseAsOccluder(bool value)
{
  if (m_bUseAsOccluder == value)
    return;
  m_bUseAsOccluder = value;
  TriggerLocalBoundsUpdate();
}

void ezGreyBoxConeComponent::OnBuildStaticMesh(ezMsgBuildStaticMesh& msg) const
{
  if (!m_bGenerateCollision || GetOwner()->IsDynamic())
    return;
  ezGeometry geometry;
  if (m_Settings.BuildGeometry(geometry).Failed())
    return;
  geometry.TriangulatePolygons();
  auto& desc = *msg.m_pStaticMeshDescription;
  auto& subMesh = desc.m_SubMeshes.ExpandAndGetRef();
  subMesh.m_uiFirstTriangle = desc.m_Triangles.GetCount();
  const ezTransform transform = GetOwner()->GetGlobalTransform();
  const ezUInt32 offset = desc.m_Vertices.GetCount();
  for (const auto& vertex : geometry.GetVertices())
    desc.m_Vertices.PushBack(transform.TransformPosition(vertex.m_vPosition));
  const bool flip = transform.HasMirrorScaling();
  for (const auto& polygon : geometry.GetPolygons())
  {
    auto& triangle = desc.m_Triangles.ExpandAndGetRef();
    triangle.m_uiVertexIndices[0] = offset + polygon.m_Vertices[0];
    triangle.m_uiVertexIndices[1] = offset + polygon.m_Vertices[flip ? 2 : 1];
    triangle.m_uiVertexIndices[2] = offset + polygon.m_Vertices[flip ? 1 : 2];
  }
  subMesh.m_uiNumTriangles = desc.m_Triangles.GetCount() - subMesh.m_uiFirstTriangle;
  auto material = GetMaterial();
  if (!material.IsValid())
    material = ezResourceManager::LoadResource<ezMaterialResource>("{ 1c47ee4c-0379-4280-85f5-b8cda61941d2 }");
  ezResourceLock<ezMaterialResource> resource(material, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (resource.GetAcquireResult() == ezResourceAcquireResult::Final)
  {
    const ezString surface = resource->GetSurface().GetString();
    if (!surface.IsEmpty())
    {
      ezUInt32 index = desc.m_Surfaces.IndexOf(surface);
      if (index == ezInvalidIndex)
      {
        index = desc.m_Surfaces.GetCount();
        desc.m_Surfaces.PushBack(surface);
      }
      subMesh.m_uiSurfaceIndex = static_cast<ezUInt16>(index);
    }
  }
}

void ezGreyBoxConeComponent::OnMsgExtractGeometry(ezMsgExtractGeometry& msg) const
{
  if (msg.m_Mode == ezWorldGeoExtractionUtil::ExtractionMode::CollisionMesh && (!m_bGenerateCollision || GetOwner()->IsDynamic()))
    return;
  ezStringBuilder name;
  name.SetFormat("GreyBoxCone:{}", m_Settings.GetHash());
  auto mesh = ezResourceManager::GetExistingResource<ezCpuMeshResource>(name);
  if (!mesh.IsValid())
  {
    ezMeshResourceDescriptor descriptor;
    if (!BuildDescriptor(descriptor))
      return;
    mesh = ezResourceManager::GetOrCreateResource<ezCpuMeshResource>(name, std::move(descriptor), name);
  }
  msg.AddMeshObject(GetOwner()->GetGlobalTransform(), mesh);
}

void ezGreyBoxConeComponent::OnMsgExtractOccluderData(ezMsgExtractOccluderData& msg) const
{
  if (!IsActiveAndInitialized() || !m_bUseAsOccluder)
    return;
  if (m_pOccluder == nullptr)
  {
    ezStringBuilder name;
    name.SetFormat("GreyBoxCone:{}", m_Settings.GetHash());
    m_pOccluder = ezRasterizerObject::GetObject(name);
    if (m_pOccluder == nullptr)
    {
      ezGeometry geometry;
      if (m_Settings.BuildGeometry(geometry).Failed())
        return;
      // The rasterizer accepts triangles and quads only, including the end caps.
      geometry.TriangulatePolygons();
      m_pOccluder = ezRasterizerObject::CreateMesh(name, geometry);
    }
  }
  msg.AddOccluder(m_pOccluder.Borrow(), GetOwner()->GetGlobalTransform());
}

void ezGreyBoxConeComponent::SetSizeNegX(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  if (m_Settings.m_vNegative.x == value)
    return;
  m_Settings.m_vNegative.x = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSizePosX(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  if (m_Settings.m_vPositive.x == value)
    return;
  m_Settings.m_vPositive.x = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSizeNegY(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  if (m_Settings.m_vNegative.y == value)
    return;
  m_Settings.m_vNegative.y = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSizePosY(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  if (m_Settings.m_vPositive.y == value)
    return;
  m_Settings.m_vPositive.y = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSizeNegZ(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  if (m_Settings.m_vNegative.z == value)
    return;
  m_Settings.m_vNegative.z = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSizePosZ(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  if (m_Settings.m_vPositive.z == value)
    return;
  m_Settings.m_vPositive.z = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetBaseRadiusScale(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  value = ezMath::Clamp(value, 0.0f, 4.0f);
  if (m_Settings.m_fBaseRadiusScale == value)
    return;
  m_Settings.m_fBaseRadiusScale = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetTopRadiusScale(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  value = ezMath::Clamp(value, 0.0f, 4.0f);
  if (m_Settings.m_fTopRadiusScale == value)
    return;
  m_Settings.m_fTopRadiusScale = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSides(ezUInt32 value)
{
  value = ezMath::Clamp(value, 3u, 128u);
  if (m_Settings.m_uiSides == value)
    return;
  m_Settings.m_uiSides = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetHeightSegments(ezUInt32 value)
{
  value = ezMath::Clamp(value, 1u, 64u);
  if (m_Settings.m_uiHeightSegments == value)
    return;
  m_Settings.m_uiHeightSegments = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetProfileCurve(float value)
{
  if (!ezMath::IsFinite(value))
    return;
  value = ezMath::Clamp(value, -0.95f, 1.0f);
  if (m_Settings.m_fProfileCurve == value)
    return;
  m_Settings.m_fProfileCurve = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSmoothShading(bool value)
{
  if (m_Settings.m_bSmoothShading == value)
    return;
  m_Settings.m_bSmoothShading = value;
  GeometryChanged();
}

EZ_STATICLINK_FILE(GreyBoxPlugin, GreyBoxPlugin_Components_GreyBoxConeComponent);

void ezGreyBoxConeComponent::SetShape(ezEnum<ezGreyBoxConeShape> shape)
{
  if (shape.GetValue() > ezGreyBoxConeShape::Cone || m_Settings.m_uiShape == shape.GetValue())
    return;
  m_Settings.m_uiShape = shape.GetValue();
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetDetail(ezUInt32 value)
{
  if (m_Settings.m_uiDetail == value)
    return;
  m_Settings.m_uiDetail = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetCurvature(ezAngle value)
{
  if (m_Settings.m_Curvature == value)
    return;
  m_Settings.m_Curvature = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetThickness(float value)
{
  if (m_Settings.m_fThickness == value)
    return;
  m_Settings.m_fThickness = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSlopedTop(bool value)
{
  if (m_Settings.m_bSlopedTop == value)
    return;
  m_Settings.m_bSlopedTop = value;
  GeometryChanged();
}

void ezGreyBoxConeComponent::SetSlopedBottom(bool value)
{
  if (m_Settings.m_bSlopedBottom == value)
    return;
  m_Settings.m_bSlopedBottom = value;
  GeometryChanged();
}
