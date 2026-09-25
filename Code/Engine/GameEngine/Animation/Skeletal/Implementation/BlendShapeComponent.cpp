#include <GameEngine/GameEnginePCH.h>
#include <Core/World/World.h>
#include <Foundation/Reflection/Reflection.h>

#include <GameEngine/Animation/Skeletal/BlendShapeComponent.h>
#include <RendererCore/Meshes/BlendShapeRenderer.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Types/Uuid.h>
#include <RendererCore/Pipeline/RenderDataManager.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Resources/Buffer.h>

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezBlendShapePoseComponent, 2, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_MEMBER_PROPERTY("Mesh", m_hMesh)->AddAttributes(new ezHiddenAttribute(), new ezAssetBrowserAttribute("CompatibleAsset_Mesh_Skinned")),
    EZ_MAP_ACCESSOR_PROPERTY("Weights", GetWeightKeys, GetWeightValue, SetWeight, RemoveWeight)->AddAttributes(new ezExposedParametersAttribute("Mesh"), new ezContainerAttribute(false, true, false)),
    EZ_ACCESSOR_PROPERTY("WeightThreshold", GetWeightThreshold, SetWeightThreshold)->AddAttributes(new ezDefaultValueAttribute(0.001f), new ezClampValueAttribute(0.0f, 0.1f)),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_FUNCTIONS
  {
    EZ_SCRIPT_FUNCTION_PROPERTY(SetWeight, In, "Name", In, "Weight"),
    EZ_SCRIPT_FUNCTION_PROPERTY(GetWeight, In, "Name"),
    EZ_SCRIPT_FUNCTION_PROPERTY(RemoveWeight, In, "Name"),
    EZ_SCRIPT_FUNCTION_PROPERTY(ResetWeights),
  }
  EZ_END_FUNCTIONS;
  EZ_BEGIN_ATTRIBUTES { new ezCategoryAttribute("Animation"), } EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE;
// clang-format on

ezBlendShapeDeformer::ezBlendShapeDeformer(ezAnimatedMeshComponent& component)
  : m_Component(component)
{
}

void ezBlendShapeDeformer::OnAnimationCurve(const ezMsgAnimationCurveValue& msg)
{
  ezStringView name = msg.m_sCurveName.GetView();
  if (!name.StartsWith("BlendShape/"))
    return;
  name.Shrink(11, 0);
  ezHashedString target;
  target.Assign(name);
  auto& value = m_CurveValues[target];
  value.m_fWeight = ezBlendShapeResourceDescriptor::SanitizeWeight(msg.m_fAverage, 0);
  value.m_LastUpdate = m_Component.GetWorld()->GetClock().GetAccumulatedTime();
}

void ezBlendShapeDeformer::Update()
{
  const auto hMesh = m_Component.GetMesh();
  if (!hMesh.IsValid())
  {
    m_CurveValues.Clear();
    m_bUseDeformedMesh = false;
    m_hMesh.Invalidate();
    m_hBlendShapes.Invalidate();
    m_hDeformedMesh.Invalidate();
    m_LastWeights.Clear();
    return;
  }
  ezResourceLock<ezMeshResource> mesh(hMesh, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (mesh.GetAcquireResult() != ezResourceAcquireResult::Final)
  {
    if (m_bUseDeformedMesh)
      m_Component.InvalidateDeformedRenderData();
    m_bUseDeformedMesh = false;
    m_LastWeights.Clear();
    return;
  }
  if (m_hMesh != hMesh || m_uiMeshVersion != mesh->GetCurrentResourceChangeCounter())
  {
    if (m_hMesh.IsValid() && m_hMesh != hMesh)
      m_CurveValues.Clear();
    m_hMesh = hMesh;
    m_uiMeshVersion = mesh->GetCurrentResourceChangeCounter();
    m_hBlendShapes.Invalidate();
    if (!mesh->m_sDefaultBlendShapes.IsEmpty())
      m_hBlendShapes = ezResourceManager::LoadResource<ezBlendShapeResource>(mesh->m_sDefaultBlendShapes);
    m_uiResourceVersion = ezInvalidIndex;
    m_LastWeights.Clear();
    m_hDeformedMesh.Invalidate();
    m_bUseDeformedMesh = false;
    m_Component.InvalidateDeformedRenderData();
  }
  if (!m_hBlendShapes.IsValid())
    return;
  ezResourceLock<ezBlendShapeResource> shapes(m_hBlendShapes, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (shapes.GetAcquireResult() != ezResourceAcquireResult::Final)
  {
    if (m_bUseDeformedMesh)
      m_Component.InvalidateDeformedRenderData();
    m_bUseDeformedMesh = false;
    m_LastWeights.Clear();
    return;
  }
  const auto& desc = shapes->GetDescriptor();
  if (!desc.m_sMesh.IsEmpty() && desc.m_sMesh != hMesh.GetResourceID())
  {
    if (m_bUseDeformedMesh)
      m_Component.InvalidateDeformedRenderData();
    m_bUseDeformedMesh = false;
    m_LastWeights.Clear();
    return;
  }
  ezResourceLock<ezMeshBufferResource> base(mesh->GetMeshBuffer(), ezResourceAcquireMode::BlockTillLoaded);
  const auto* positionBuffer = ezGALDevice::GetDefaultDevice()->GetBuffer(base->GetVertexBuffers()[ezMeshVertexStreamType::Position]);
  if (positionBuffer == nullptr || !base->GetVertexStreamConfig().m_bUseHighPrecision || positionBuffer->GetSize() / sizeof(ezVec3) != desc.m_Mesh.MeshBufferDesc().GetVertexCount())
  {
    if (m_bUseDeformedMesh)
      m_Component.InvalidateDeformedRenderData();
    m_bUseDeformedMesh = false;
    m_LastWeights.Clear();
    return; // The dependent resource is being reloaded; never bind an incompatible stream.
  }
  if (m_uiResourceVersion != shapes->GetCurrentResourceChangeCounter())
  {
    m_uiResourceVersion = shapes->GetCurrentResourceChangeCounter();
    m_LastWeights.Clear();
    m_hDeformedMesh.Invalidate();
  }
  ezBlendShapePoseComponent* pPose = nullptr;
  if (!m_Component.GetOwner()->TryGetComponentOfBaseType(pPose))
    pPose = nullptr;
  if (pPose && !pPose->IsActiveAndInitialized())
    pPose = nullptr;
  const float threshold = pPose ? pPose->GetWeightThreshold() : 0.001f;
  ezHybridArray<float, 32> weights;
  bool active = false;
  const auto now = m_Component.GetWorld()->GetClock().GetAccumulatedTime();
  for (const auto& target : desc.m_Targets)
  {
    float weight = target.m_fDefaultWeight;
    CurveValue curve;
    if (m_CurveValues.TryGetValue(target.m_sName, curve) && now - curve.m_LastUpdate < ezTime::MakeFromSeconds(0.5))
      weight = curve.m_fWeight;
    if (pPose)
      pPose->GetWeightValue(target.m_sName.GetString(), weight);
    weight = ezBlendShapeResourceDescriptor::SanitizeWeight(weight, threshold);
    weights.PushBack(weight);
    active |= weight != 0;
  }
  if (m_LastWeights.GetArrayPtr() == weights.GetArrayPtr())
    return;
  m_LastWeights = weights;
  m_bUseDeformedMesh = active;
  m_Component.InvalidateDeformedRenderData();
  if (!active)
    return;
  const auto& source = desc.m_Mesh.MeshBufferDesc();
  if (!m_hDeformedMesh.IsValid())
  {
    ezDynamicMeshBufferResourceDescriptor buffer;
    buffer.m_uiMaxVertices = source.GetVertexCount();
    buffer.m_IndexType = ezGALIndexType::None;
    ezStringBuilder id;
    id.SetFormat("BlendShape-{}", ezUuid::MakeUuid());
    m_hDeformedMesh = ezResourceManager::CreateResource<ezDynamicMeshBufferResource>(id, std::move(buffer));
  }
  desc.Deform(weights, m_Positions, m_Normals, m_Tangents);
  ++m_uiDeformationRevision;
  ezResourceLock<ezDynamicMeshBufferResource> buffer(m_hDeformedMesh, ezResourceAcquireMode::BlockTillLoaded);
  buffer->AccessPositionData().CopyFrom(m_Positions);
  auto ntt = buffer->AccessNormalTangentTexCoord0Data();
  for (ezUInt32 i = 0; i < source.GetVertexCount(); ++i)
  {
    ntt[i].EncodeNormal(m_Normals[i]);
    ntt[i].EncodeTangent(m_Tangents[i].GetAsVec3(), m_Tangents[i].w);
    ntt[i].m_vTexCoord = source.GetTexCoord0(i);
  }
}

ezSkinnedMeshRenderData* ezBlendShapeDeformer::CreateRenderData(const ezRenderDataManager* pManager) const
{
  if (!m_bUseDeformedMesh)
    return nullptr;
  auto* data = pManager->CreateRenderDataForThisFrame<ezBlendShapeRenderData>(m_Component.GetOwner());
  data->m_hDeformedMesh = m_hDeformedMesh;
  return data;
}

float ezBlendShapeDeformer::GetWeight(ezStringView sName) const
{
  if (!m_hBlendShapes.IsValid())
    return 0;
  ezResourceLock<ezBlendShapeResource> resource(m_hBlendShapes, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (resource.GetAcquireResult() != ezResourceAcquireResult::Final)
    return 0;
  for (ezUInt32 i = 0; i < resource->GetDescriptor().m_Targets.GetCount(); ++i)
    if (resource->GetDescriptor().m_Targets[i].m_sName.GetView() == sName)
      return i < m_LastWeights.GetCount() ? m_LastWeights[i] : resource->GetDescriptor().m_Targets[i].m_fDefaultWeight;
  return 0;
}

const ezRangeView<const char*, ezUInt32> ezBlendShapePoseComponent::GetWeightKeys() const
{
  return ezRangeView<const char*, ezUInt32>([]()
    { return 0u; }, [this]()
    { return m_Weights.GetCount(); }, [](ezUInt32& i)
    { ++i; },
    [this](const ezUInt32& i)
    { auto it = m_Weights.GetIterator(); for (ezUInt32 n = 0; n < i; ++n) ++it; return it.Key().GetData(); });
}
bool ezBlendShapePoseComponent::GetWeightValue(const char* key, float& out_value) const
{
  return m_Weights.TryGetValue(key, out_value);
}
void ezBlendShapePoseComponent::SetWeight(const char* key, float value)
{
  m_Weights[key] = ezBlendShapeResourceDescriptor::SanitizeWeight(value, 0);
}
void ezBlendShapePoseComponent::RemoveWeight(const char* key)
{
  m_Weights.Remove(key);
}
void ezBlendShapePoseComponent::ResetWeights()
{
  m_Weights.Clear();
}
float ezBlendShapePoseComponent::GetWeight(const char* key) const
{
  float value = 0;
  if (GetWeightValue(key, value))
    return ezBlendShapeResourceDescriptor::SanitizeWeight(value, m_fWeightThreshold);
  const ezAnimatedMeshComponent* mesh = nullptr;
  if (GetOwner() && GetOwner()->TryGetComponentOfBaseType(mesh) && mesh->GetDeformer())
    return static_cast<ezBlendShapeDeformer*>(mesh->GetDeformer())->GetWeight(key);
  return 0;
}
void ezBlendShapePoseComponent::SetWeightThreshold(float threshold)
{
  m_fWeightThreshold = ezMath::IsFinite(threshold) ? ezMath::Clamp(threshold, 0.0f, 0.1f) : 0.001f;
}
void ezBlendShapePoseComponent::SerializeComponent(ezWorldWriter& stream) const
{
  SUPER::SerializeComponent(stream);
  stream.GetStream() << m_hMesh;
  stream.GetStream() << m_fWeightThreshold;
  stream.GetStream().WriteMap(m_Weights).AssertSuccess();
}
void ezBlendShapePoseComponent::DeserializeComponent(ezWorldReader& stream)
{
  SUPER::DeserializeComponent(stream);
  if (stream.GetComponentTypeVersion(GetStaticRTTI()) >= 2)
  {
    stream.GetStream() >> m_hMesh;
    stream.GetStream() >> m_fWeightThreshold;
    SetWeightThreshold(m_fWeightThreshold);
  }
  stream.GetStream().ReadMap(m_Weights).AssertSuccess();
}

EZ_STATICLINK_FILE(GameEngine, GameEngine_Animation_Skeletal_Implementation_BlendShapeComponent);
