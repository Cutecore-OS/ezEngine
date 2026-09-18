#include <RendererCore/RendererCorePCH.h>

#include <Foundation/Utilities/AssetFileHeader.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeResource, 1, ezRTTIDefaultAllocator<ezBlendShapeResource>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_RESOURCE_IMPLEMENT_COMMON_CODE(ezBlendShapeResource);
// clang-format on

ezResult ezBlendShapeChannel::Serialize(ezStreamWriter& inout_stream) const
{
  inout_stream.WriteVersion(1);

  inout_stream << m_sName;
  inout_stream << m_fDefaultWeight;
  inout_stream << m_fMinWeight;
  inout_stream << m_fMaxWeight;

  const ezUInt32 uiNumDeltas = m_Deltas.GetCount();
  inout_stream << uiNumDeltas;

  if (uiNumDeltas > 0)
  {
    inout_stream.WriteBytes(m_Deltas.GetData(), sizeof(ezBlendShapeVertexDelta) * uiNumDeltas).IgnoreResult();
  }

  return EZ_SUCCESS;
}

ezResult ezBlendShapeChannel::Deserialize(ezStreamReader& inout_stream)
{
  const auto uiVersion = inout_stream.ReadVersion(1);
  EZ_IGNORE_UNUSED(uiVersion);

  inout_stream >> m_sName;
  inout_stream >> m_fDefaultWeight;
  inout_stream >> m_fMinWeight;
  inout_stream >> m_fMaxWeight;

  ezUInt32 uiNumDeltas = 0;
  inout_stream >> uiNumDeltas;

  m_Deltas.SetCountUninitialized(uiNumDeltas);
  if (uiNumDeltas > 0)
  {
    inout_stream.ReadBytes(m_Deltas.GetData(), sizeof(ezBlendShapeVertexDelta) * uiNumDeltas);
  }

  return EZ_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////

ezBlendShapeResourceDescriptor::ezBlendShapeResourceDescriptor() = default;
ezBlendShapeResourceDescriptor::~ezBlendShapeResourceDescriptor() = default;

ezBlendShapeResourceDescriptor::ezBlendShapeResourceDescriptor(ezBlendShapeResourceDescriptor&& rhs) noexcept
{
  *this = std::move(rhs);
}

void ezBlendShapeResourceDescriptor::operator=(ezBlendShapeResourceDescriptor&& rhs) noexcept
{
  m_BasePositions = std::move(rhs.m_BasePositions);
  m_BaseNormals = std::move(rhs.m_BaseNormals);
  m_Channels = std::move(rhs.m_Channels);
  m_MeshBufferDesc = std::move(rhs.m_MeshBufferDesc);
}

ezResult ezBlendShapeResourceDescriptor::Serialize(ezStreamWriter& inout_stream) const
{
  inout_stream.WriteVersion(2);

  const ezUInt32 uiNumPositions = m_BasePositions.GetCount();
  inout_stream << uiNumPositions;
  if (uiNumPositions > 0)
  {
    inout_stream.WriteBytes(m_BasePositions.GetData(), sizeof(ezVec3) * uiNumPositions).IgnoreResult();
  }

  const ezUInt32 uiNumNormals = m_BaseNormals.GetCount();
  inout_stream << uiNumNormals;
  if (uiNumNormals > 0)
  {
    inout_stream.WriteBytes(m_BaseNormals.GetData(), sizeof(ezVec3) * uiNumNormals).IgnoreResult();
  }

  const ezUInt32 uiNumChannels = m_Channels.GetCount();
  inout_stream << uiNumChannels;

  for (ezUInt32 i = 0; i < uiNumChannels; ++i)
  {
    EZ_SUCCEED_OR_RETURN(m_Channels[i].Serialize(inout_stream));
  }

  // Version 2: Base mesh buffer descriptor
  inout_stream << m_MeshBufferDesc.GetVertexCount();
  inout_stream << m_MeshBufferDesc.GetPrimitiveCount();
  inout_stream << static_cast<ezUInt8>(m_MeshBufferDesc.GetTopology());
  inout_stream << m_MeshBufferDesc.GetVertexStreamConfig().m_uiTypesMask;
  inout_stream << m_MeshBufferDesc.GetVertexStreamConfig().m_bUseHighPrecision;

  for (ezUInt32 i = 0; i < ezMeshVertexStreamType::Count; ++i)
  {
    auto type = static_cast<ezMeshVertexStreamType::Enum>(i);
    const auto& data = m_MeshBufferDesc.GetVertexBufferData(type);
    inout_stream << data.GetCount();
    if (!data.IsEmpty())
    {
      inout_stream.WriteBytes(data.GetPtr(), data.GetCount()).IgnoreResult();
    }
  }

  const auto& idxData = m_MeshBufferDesc.GetIndexBufferData();
  inout_stream << idxData.GetCount();
  if (!idxData.IsEmpty())
  {
    inout_stream.WriteBytes(idxData.GetPtr(), idxData.GetCount()).IgnoreResult();
  }

  return EZ_SUCCESS;
}

ezResult ezBlendShapeResourceDescriptor::Deserialize(ezStreamReader& inout_stream)
{
  const auto uiVersion = inout_stream.ReadVersion(2);

  ezUInt32 uiNumPositions = 0;
  inout_stream >> uiNumPositions;
  m_BasePositions.SetCountUninitialized(uiNumPositions);
  if (uiNumPositions > 0)
  {
    inout_stream.ReadBytes(m_BasePositions.GetData(), sizeof(ezVec3) * uiNumPositions);
  }

  ezUInt32 uiNumNormals = 0;
  inout_stream >> uiNumNormals;
  m_BaseNormals.SetCountUninitialized(uiNumNormals);
  if (uiNumNormals > 0)
  {
    inout_stream.ReadBytes(m_BaseNormals.GetData(), sizeof(ezVec3) * uiNumNormals);
  }

  ezUInt32 uiNumChannels = 0;
  inout_stream >> uiNumChannels;

  m_Channels.SetCount(uiNumChannels);
  for (ezUInt32 i = 0; i < uiNumChannels; ++i)
  {
    EZ_SUCCEED_OR_RETURN(m_Channels[i].Deserialize(inout_stream));
  }

  if (uiVersion >= 2)
  {
    ezUInt32 uiVertexCount = 0;
    ezUInt32 uiPrimitiveCount = 0;
    ezUInt8 uiTopology = 0;
    ezUInt16 uiTypesMask = 0;
    bool bHighPrecision = false;

    inout_stream >> uiVertexCount;
    inout_stream >> uiPrimitiveCount;
    inout_stream >> uiTopology;
    inout_stream >> uiTypesMask;
    inout_stream >> bHighPrecision;

    m_MeshBufferDesc.Clear();
    for (ezUInt32 i = 0; i < ezMeshVertexStreamType::Count; ++i)
    {
      if ((uiTypesMask & EZ_BIT(i)) != 0)
      {
        m_MeshBufferDesc.AddStream(static_cast<ezMeshVertexStreamType::Enum>(i), bHighPrecision);
      }
    }

    m_MeshBufferDesc.AllocateStreams(uiVertexCount, static_cast<ezGALPrimitiveTopology::Enum>(uiTopology), uiPrimitiveCount);

    for (ezUInt32 i = 0; i < ezMeshVertexStreamType::Count; ++i)
    {
      auto type = static_cast<ezMeshVertexStreamType::Enum>(i);
      ezUInt32 count = 0;
      inout_stream >> count;
      if (count > 0)
      {
        auto& data = m_MeshBufferDesc.GetVertexBufferData(type);
        data.SetCountUninitialized(count);
        inout_stream.ReadBytes(data.GetData(), count);
      }
    }

    ezUInt32 idxCount = 0;
    inout_stream >> idxCount;
    if (idxCount > 0)
    {
      auto& idxData = m_MeshBufferDesc.GetIndexBufferData();
      idxData.SetCountUninitialized(idxCount);
      inout_stream.ReadBytes(idxData.GetData(), idxCount);
    }
  }

  return EZ_SUCCESS;
}

ezUInt64 ezBlendShapeResourceDescriptor::GetHeapMemoryUsage() const
{
  ezUInt64 mem = m_BasePositions.GetHeapMemoryUsage() + m_BaseNormals.GetHeapMemoryUsage() + m_Channels.GetHeapMemoryUsage();
  for (const auto& channel : m_Channels)
  {
    mem += channel.m_Deltas.GetHeapMemoryUsage();
  }
  return mem;
}

ezUInt32 ezBlendShapeResourceDescriptor::FindChannelByName(const ezTempHashedString& sName) const
{
  for (ezUInt32 i = 0; i < m_Channels.GetCount(); ++i)
  {
    if (m_Channels[i].m_sName == sName)
      return i;
  }
  return ezInvalidIndex;
}

//////////////////////////////////////////////////////////////////////////

ezBlendShapeResource::ezBlendShapeResource()
  : ezResource(DoUpdate::OnAnyThread, 1)
{
}

ezBlendShapeResource::~ezBlendShapeResource() = default;

ezResourceLoadDesc ezBlendShapeResource::UnloadData(Unload WhatToUnload)
{
  m_pDescriptor.Clear();

  ezResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = ezResourceState::Unloaded;

  return res;
}

ezResourceLoadDesc ezBlendShapeResource::UpdateContent(ezStreamReader* Stream)
{
  EZ_LOG_BLOCK("ezBlendShapeResource::UpdateContent", GetResourceIdOrDescription());

  ezResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;

  if (Stream == nullptr)
  {
    res.m_State = ezResourceState::LoadedResourceMissing;
    return res;
  }

  ezStringBuilder sAbsFilePath;
  (*Stream) >> sAbsFilePath;

  // skip the asset header
  ezAssetFileHeader AssetHash;
  AssetHash.Read(*Stream).IgnoreResult();

  m_pDescriptor = EZ_DEFAULT_NEW(ezBlendShapeResourceDescriptor);
  if (m_pDescriptor->Deserialize(*Stream).Failed())
  {
    m_pDescriptor.Clear();
    res.m_State = ezResourceState::LoadedResourceMissing;
    return res;
  }

  res.m_State = ezResourceState::Loaded;
  return res;
}

void ezBlendShapeResource::UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage)
{
  out_NewMemoryUsage.m_uiMemoryCPU = sizeof(ezBlendShapeResource);
  out_NewMemoryUsage.m_uiMemoryGPU = 0;

  if (m_pDescriptor)
  {
    out_NewMemoryUsage.m_uiMemoryCPU += sizeof(ezBlendShapeResourceDescriptor);
    out_NewMemoryUsage.m_uiMemoryCPU += m_pDescriptor->GetHeapMemoryUsage();
  }
}

EZ_RESOURCE_IMPLEMENT_CREATEABLE(ezBlendShapeResource, ezBlendShapeResourceDescriptor)
{
  m_pDescriptor = EZ_DEFAULT_NEW(ezBlendShapeResourceDescriptor);
  *m_pDescriptor = std::move(descriptor);

  ezResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = ezResourceState::Loaded;

  return res;
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_AnimationSystem_Implementation_BlendShapeResource);
