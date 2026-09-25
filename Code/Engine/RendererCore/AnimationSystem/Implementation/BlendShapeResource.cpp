#include <RendererCore/RendererCorePCH.h>
#include <Core/World/World.h>
#include <Foundation/Reflection/Reflection.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <Foundation/Types/Uuid.h>
#include <Foundation/Utilities/AssetFileHeader.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeResource, 1, ezRTTIDefaultAllocator<ezBlendShapeResource>)
EZ_END_DYNAMIC_REFLECTED_TYPE;
EZ_RESOURCE_IMPLEMENT_COMMON_CODE(ezBlendShapeResource);

ezResult ezBlendShapeResourceDescriptor::Validate() const
{
  const auto& mesh = m_Mesh.MeshBufferDesc();
  if (mesh.GetVertexCount() == 0 || !mesh.GetVertexStreamConfig().HasNormalTangentAndTexCoord0() ||
      m_Targets.GetCount() > 4096 || !mesh.GetVertexStreamConfig().m_bUseHighPrecision || !m_Mesh.GetBounds().IsValid())
    return EZ_FAILURE;

  ezHashSet<ezHashedString> names;
  for (const auto& target : m_Targets)
  {
    if (target.m_sName.IsEmpty() || names.Contains(target.m_sName) || !ezMath::IsFinite(target.m_fDefaultWeight))
      return EZ_FAILURE;
    names.Insert(target.m_sName);
    ezUInt32 uiPrevious = ezInvalidIndex;
    for (const auto& delta : target.m_Deltas)
    {
      if (delta.m_uiVertex >= mesh.GetVertexCount() ||
          (uiPrevious != ezInvalidIndex && delta.m_uiVertex <= uiPrevious) ||
          !delta.m_vPosition.IsValid() || !delta.m_vNormal.IsValid() || !delta.m_vTangent.IsValid())
        return EZ_FAILURE;
      uiPrevious = delta.m_uiVertex;
    }
  }
  return EZ_SUCCESS;
}

ezResult ezBlendShapeResourceDescriptor::Save(ezStreamWriter& inout_stream)
{
  EZ_SUCCEED_OR_RETURN(Validate());
  inout_stream.WriteVersion(2);
  inout_stream << m_sMesh;
  m_Mesh.Save(inout_stream);
  inout_stream << m_Targets.GetCount();
  for (const auto& target : m_Targets)
  {
    {
      inout_stream << target.m_sName;
      inout_stream << target.m_fDefaultWeight;
      inout_stream << target.m_Deltas.GetCount();
    }
    for (const auto& delta : target.m_Deltas)
    {
      inout_stream << delta.m_uiVertex;
      inout_stream << delta.m_vPosition;
      inout_stream << delta.m_vNormal;
      inout_stream << delta.m_vTangent;
    }
  }
  inout_stream << ezUInt32(0x42534850);
  return EZ_SUCCESS;
}

ezResult ezBlendShapeResourceDescriptor::Load(ezStreamReader& inout_stream)
{
  const auto version = inout_stream.ReadVersion(2);
  m_sMesh.Clear();
  if (version >= 2)
    inout_stream >> m_sMesh;
  EZ_SUCCEED_OR_RETURN(m_Mesh.Load(inout_stream));
  ezUInt32 uiCount = 0;
  inout_stream >> uiCount;
  if (uiCount > 4096)
    return EZ_FAILURE;
  m_Targets.SetCount(uiCount);
  for (auto& target : m_Targets)
  {
    {
      inout_stream >> target.m_sName;
      inout_stream >> target.m_fDefaultWeight;
      inout_stream >> uiCount;
    }
    if (uiCount > m_Mesh.MeshBufferDesc().GetVertexCount())
      return EZ_FAILURE;
    target.m_Deltas.SetCount(uiCount);
    for (auto& delta : target.m_Deltas)
    {
      inout_stream >> delta.m_uiVertex;
      inout_stream >> delta.m_vPosition;
      inout_stream >> delta.m_vNormal;
      inout_stream >> delta.m_vTangent;
    }
  }
  ezUInt32 uiEndMarker = 0;
  inout_stream >> uiEndMarker;
  if (uiEndMarker != 0x42534850)
    return EZ_FAILURE;
  return Validate();
}

ezUInt64 ezBlendShapeResourceDescriptor::GetHeapMemoryUsage() const
{
  ezUInt64 uiSize = m_Targets.GetHeapMemoryUsage();
  for (const auto& target : m_Targets)
    uiSize += target.m_Deltas.GetHeapMemoryUsage();
  const auto& mesh = m_Mesh.MeshBufferDesc();
  for (ezUInt32 i = 0; i < mesh.GetNumVertexBuffers(); ++i)
    uiSize += mesh.GetVertexBufferData(static_cast<ezMeshVertexStreamType::Enum>(i)).GetCount();
  return uiSize + mesh.GetIndexBufferData().GetCount();
}

void ezBlendShapeResourceDescriptor::MakeCurveName(ezStringView sTarget, ezStringBuilder& out_sName)
{
  out_sName.Set("BlendShape/", sTarget);
}

float ezBlendShapeResourceDescriptor::SanitizeWeight(float fWeight, float fThreshold)
{
  if (!ezMath::IsFinite(fWeight))
    return 0.0f;
  fWeight = ezMath::Clamp(fWeight, -1.0f, 1.0f);
  return ezMath::Abs(fWeight) <= fThreshold ? 0.0f : fWeight;
}

void ezBlendShapeResourceDescriptor::Deform(ezArrayPtr<const float> weights, ezDynamicArray<ezVec3>& out_positions,
  ezDynamicArray<ezVec3>& out_normals, ezDynamicArray<ezVec4>& out_tangents) const
{
  const auto& mesh = m_Mesh.MeshBufferDesc();
  const ezUInt32 uiVertices = mesh.GetVertexCount();
  out_positions = mesh.GetPositionData();
  out_normals.SetCountUninitialized(uiVertices);
  out_tangents.SetCountUninitialized(uiVertices);
  for (ezUInt32 v = 0; v < uiVertices; ++v)
  {
    out_normals[v] = mesh.GetNormal(v);
    out_tangents[v] = mesh.GetTangent(v);
  }
  for (ezUInt32 t = 0; t < ezMath::Min(weights.GetCount(), m_Targets.GetCount()); ++t)
  {
    const float fWeight = SanitizeWeight(weights[t], 0.0f);
    if (fWeight == 0.0f)
      continue;
    for (const auto& delta : m_Targets[t].m_Deltas)
    {
      out_positions[delta.m_uiVertex] += delta.m_vPosition * fWeight;
      out_normals[delta.m_uiVertex] += delta.m_vNormal * fWeight;
      out_tangents[delta.m_uiVertex] += delta.m_vTangent.GetAsVec4(0.0f) * fWeight;
    }
  }
  for (ezUInt32 v = 0; v < uiVertices; ++v)
  {
    auto& n = out_normals[v];
    n.NormalizeIfNotZero(ezVec3(0, 0, 1)).IgnoreResult();
    ezVec3 t = out_tangents[v].GetAsVec3();
    t -= n * n.Dot(t);
    t.NormalizeIfNotZero(n.GetOrthogonalVector()).IgnoreResult();
    out_tangents[v] = t.GetAsVec4(out_tangents[v].w);
  }
}

ezBlendShapeResource::ezBlendShapeResource()
  : ezResource(DoUpdate::OnAnyThread, 1)
{
}

ezResourceLoadDesc ezBlendShapeResource::UnloadData(Unload what)
{
  m_Descriptor = ezBlendShapeResourceDescriptor();
  return {ezResourceState::Unloaded, 0, 0};
}

ezResourceLoadDesc ezBlendShapeResource::UpdateContent(ezStreamReader* pStream)
{
  if (pStream == nullptr)
    return {ezResourceState::LoadedResourceMissing, 0, 0};
  ezStringBuilder sPath;
  *pStream >> sPath;
  ezAssetFileHeader header;
  ezBlendShapeResourceDescriptor desc;
  if (header.Read(*pStream).Failed() || desc.Load(*pStream).Failed())
    return {ezResourceState::LoadedResourceMissing, 0, 0};
  return CreateResource(std::move(desc));
}

EZ_RESOURCE_IMPLEMENT_CREATEABLE(ezBlendShapeResource, ezBlendShapeResourceDescriptor)
{
  if (descriptor.Validate().Failed())
    return {ezResourceState::LoadedResourceMissing, 0, 0};
  m_Descriptor = std::move(descriptor);
  return {ezResourceState::Loaded, 0, 0};
}

void ezBlendShapeResource::UpdateMemoryUsage(MemoryUsage& out_usage)
{
  out_usage.m_uiMemoryCPU = static_cast<ezUInt32>(sizeof(*this) + m_Descriptor.GetHeapMemoryUsage());
  out_usage.m_uiMemoryGPU = 0;
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_AnimationSystem_Implementation_BlendShapeResource);
