#include <GrassPlugin/GrassPluginPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/Interfaces/WindWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Math/Random.h>
#include <Foundation/Containers/Map.h>
#include <Foundation/Profiling/Profiling.h>
#include <Foundation/Serialization/AbstractObjectGraph.h>
#include <Foundation/Serialization/GraphPatch.h>
#include <Foundation/SimdMath/SimdNoise.h>
#include <GrassPlugin/Components/GrassComponent.h>
#include <GrassPlugin/Components/GrassPatchComponent.h>
#include <RendererCore/Material/MaterialResource.h>
#include <RendererCore/Meshes/DynamicMeshBufferResource.h>
#include <RendererCore/Meshes/MeshResource.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererCore/Textures/Texture2DResource.h>

// Simulation adapted from Wicked Engine's hairparticle_simulateCS.hlsl.
// See ../LICENSE.WickedEngine.txt for the original MIT license.

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_TYPE(ezGrassAtlasRect, ezNoBase, 1, ezRTTIDefaultAllocator<ezGrassAtlasRect>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("TexMulAdd", m_vTexMulAdd)->AddAttributes(new ezDefaultValueAttribute(ezVec4(1, 1, 0, 0))),
    EZ_MEMBER_PROPERTY("Size", m_fSize)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 10.0f)),
  }
  EZ_END_PROPERTIES;
}
EZ_END_STATIC_REFLECTED_TYPE;

EZ_BEGIN_COMPONENT_TYPE(ezGrassComponent, 2, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_ACCESSOR_PROPERTY("Mesh", GetMesh, SetMesh)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_Static"), new ezRequiredAttribute()),
    EZ_ACCESSOR_PROPERTY("StrandCount", GetStrandCount, SetStrandCount)->AddAttributes(new ezDefaultValueAttribute(1000), new ezClampValueAttribute(0, 100000)),
    EZ_ACCESSOR_PROPERTY("SegmentCount", GetSegmentCount, SetSegmentCount)->AddAttributes(new ezDefaultValueAttribute(1), new ezClampValueAttribute(1, 10)),
    EZ_ACCESSOR_PROPERTY("BillboardCount", GetBillboardCount, SetBillboardCount)->AddAttributes(new ezDefaultValueAttribute(1), new ezClampValueAttribute(1, 10)),
    EZ_ACCESSOR_PROPERTY("RandomSeed", GetRandomSeed, SetRandomSeed)->AddAttributes(new ezDefaultValueAttribute(1)),
    EZ_ACCESSOR_PROPERTY("Length", GetLength, SetLength)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 100.0f)),
    EZ_ACCESSOR_PROPERTY("Width", GetWidth, SetWidth)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_ACCESSOR_PROPERTY("Randomness", GetRandomness, SetRandomness)->AddAttributes(new ezDefaultValueAttribute(0.2f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_ACCESSOR_PROPERTY("Uniformity", GetUniformity, SetUniformity)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.01f, 100.0f)),
    EZ_MEMBER_PROPERTY("Stiffness", m_fStiffness)->AddAttributes(new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("Drag", m_fDrag)->AddAttributes(new ezDefaultValueAttribute(0.1f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("GravityPower", m_fGravityPower)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("ViewDistance", m_fViewDistance)->AddAttributes(new ezDefaultValueAttribute(200.0f), new ezClampValueAttribute(0.0f, 10000.0f)),
    EZ_MEMBER_PROPERTY("SimulationDistance", m_fSimulationDistance)->AddAttributes(new ezDefaultValueAttribute(40.0f), new ezClampValueAttribute(0.0f, 10000.0f)),
    EZ_MEMBER_PROPERTY("LodStartDistance", m_fLodStartDistance)->AddAttributes(new ezDefaultValueAttribute(15.0f), new ezClampValueAttribute(1.0f, 1000.0f)),
    EZ_ACCESSOR_PROPERTY("PatchSize", GetPatchSize, SetPatchSize)->AddAttributes(new ezDefaultValueAttribute(8.0f), new ezClampValueAttribute(1.0f, 64.0f)),
    EZ_MEMBER_PROPERTY("CameraBend", m_bCameraBend)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_ARRAY_ACCESSOR_PROPERTY("AtlasRects", AtlasRects_GetCount, AtlasRects_GetValue, AtlasRects_SetValue, AtlasRects_Insert, AtlasRects_Remove),
    EZ_ARRAY_ACCESSOR_PROPERTY("VertexLengths", VertexLengths_GetCount, VertexLengths_GetValue, VertexLengths_SetValue, VertexLengths_Insert, VertexLengths_Remove),
    EZ_MEMBER_PROPERTY("WindInfluence", m_fWindInfluence)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("Collision", m_bCollision)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_MEMBER_PROPERTY("CollisionLayer", m_uiCollisionLayer)->AddAttributes(new ezDynamicEnumAttribute("PhysicsCollisionLayer")),
    EZ_MEMBER_PROPERTY("CollisionRadius", m_fCollisionRadius)->AddAttributes(new ezDefaultValueAttribute(0.02f), new ezClampValueAttribute(0.001f, 1.0f)),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Rendering/Vegetation"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE;
// clang-format on

ezGrassComponent::ezGrassComponent() = default;
ezGrassComponent::~ezGrassComponent() = default;

void ezGrassComponent::SetMesh(const ezMeshResourceHandle& hMesh)
{
  m_hMesh = hMesh;
  m_hCpuMesh.Invalidate();
  m_bRebuild = true;
}

void ezGrassComponent::OnActivated()
{
  SUPER::OnActivated();
  if (!GetMaterial().IsValid())
    SetMaterialFile("Materials/Grass.ezMaterial");
  m_bRebuild = true;
}

void ezGrassComponent::OnDeactivated()
{
  ClearPatches();
  SetMeshResource({});
  m_Strands.Clear();
  m_Particles.Clear();
  m_hCpuMesh.Invalidate();
  SUPER::OnDeactivated();
}

void ezGrassComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s << m_hMesh;
  s << m_uiStrandCount << m_uiSegmentCount << m_uiBillboardCount << m_uiRandomSeed;
  s << m_fLength << m_fWidth << m_fRandomness << m_fUniformity;
  s << m_fStiffness << m_fDrag << m_fGravityPower << m_fSimulationDistance << m_bCameraBend;
  s << m_fWindInfluence << m_bCollision << m_uiCollisionLayer << m_fCollisionRadius;
  s.WriteArray(m_VertexLengths).AssertSuccess();
  s << m_AtlasRects.GetCount();
  for (const auto& rect : m_AtlasRects)
    s << rect.m_vTexMulAdd << rect.m_fSize;
  s << m_fViewDistance << m_fLodStartDistance << m_fPatchSize;
}

void ezGrassComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s >> m_hMesh;
  s >> m_uiStrandCount >> m_uiSegmentCount >> m_uiBillboardCount >> m_uiRandomSeed;
  s >> m_fLength >> m_fWidth >> m_fRandomness >> m_fUniformity;
  s >> m_fStiffness >> m_fDrag >> m_fGravityPower >> m_fSimulationDistance >> m_bCameraBend;
  s >> m_fWindInfluence >> m_bCollision >> m_uiCollisionLayer >> m_fCollisionRadius;
  s.ReadArray(m_VertexLengths).AssertSuccess();
  ezUInt32 uiCount;
  s >> uiCount;
  m_AtlasRects.SetCount(uiCount);
  for (auto& rect : m_AtlasRects)
    s >> rect.m_vTexMulAdd >> rect.m_fSize;
  if (inout_stream.GetComponentTypeVersion(GetStaticRTTI()) >= 2)
    s >> m_fViewDistance >> m_fLodStartDistance >> m_fPatchSize;
  SetStrandCount(m_uiStrandCount);
  SetSegmentCount(m_uiSegmentCount);
  SetBillboardCount(m_uiBillboardCount);
  SetLength(m_fLength);
  SetWidth(m_fWidth);
  SetRandomness(m_fRandomness);
  SetUniformity(m_fUniformity);
}

void ezGrassComponent::Update()
{
  EZ_PROFILE_SCOPE("Grass.Update");
  m_Statistics = {};
  const ezTransform transform = GetOwner()->GetGlobalTransform();
  if (ezMath::Abs(transform.m_vScale.x) < 0.00001f || ezMath::Abs(transform.m_vScale.y) < 0.00001f || ezMath::Abs(transform.m_vScale.z) < 0.00001f)
  {
    ClearPatches();
    m_bRebuild = true;
    return;
  }

  if (m_hMesh.IsValid() && !m_hCpuMesh.IsValid())
    m_hCpuMesh = ezResourceManager::LoadResource<ezCpuMeshResource>(m_hMesh.GetResourceID());
  if (m_hCpuMesh.IsValid())
  {
    ezResourceLock<ezCpuMeshResource> pMesh(m_hCpuMesh, ezResourceAcquireMode::AllowLoadingFallback_NeverFail);
    if (pMesh.GetAcquireResult() != ezResourceAcquireResult::Final)
    {
      ClearPatches();
      m_bRebuild = true;
      return;
    }
    m_bRebuild |= m_uiMeshChangeCounter != pMesh->GetCurrentResourceChangeCounter();
  }

  float fAspect = 1.0f;
  if (GetMaterial().IsValid())
  {
    ezResourceLock<ezMaterialResource> pMaterial(GetMaterial(), ezResourceAcquireMode::AllowLoadingFallback_NeverFail);
    if (pMaterial.GetAcquireResult() == ezResourceAcquireResult::Final)
    {
      auto hTexture = pMaterial->GetTexture2DBinding("BaseTexture");
      if (hTexture.IsValid())
      {
        ezResourceLock<ezTexture2DResource> pTexture(hTexture, ezResourceAcquireMode::AllowLoadingFallback_NeverFail);
        if (pTexture.GetAcquireResult() == ezResourceAcquireResult::Final)
          fAspect = float(pTexture->GetWidth()) / ezMath::Max(1u, pTexture->GetHeight());
      }
    }
  }
  m_bRebuild |= m_fTextureAspect != fAspect;
  m_fTextureAspect = fAspect;
  const bool bRebuilt = m_bRebuild;
  if (m_bRebuild)
    Rebuild();

  struct Camera
  {
    ezVec3 m_vPosition;
    ezFrustum m_Frustum;
  };
  ezHybridArray<Camera, 4> cameras;
  for (const auto& hView : ezRenderWorld::GetMainViews())
  {
    ezView* pView = nullptr;
    if (ezRenderWorld::TryGetView(hView, pView) && pView->GetWorld() == GetWorld() && pView->GetCamera() != nullptr &&
        pView->GetCameraUsageHint() != ezCameraUsageHint::Shadow)
    {
      auto& camera = cameras.ExpandAndGetRef();
      camera.m_vPosition = pView->GetLodCamera()->GetCenterPosition();
      pView->ComputeCullingFrustum(camera.m_Frustum);
    }
  }

  constexpr float fStep = 1.0f / 60.0f;
  ezUInt32 uiSteps = 0;
  if (GetWorld()->GetWorldSimulationEnabled())
  {
    m_fAccumulatedTime = ezMath::Min(m_fAccumulatedTime + GetWorld()->GetClock().GetTimeDiff().AsFloatInSeconds(), fStep * 4);
    uiSteps = static_cast<ezUInt32>(m_fAccumulatedTime / fStep);
    m_fAccumulatedTime -= uiSteps * fStep;
  }
  else
    m_fAccumulatedTime = 0;

  const auto* pWind = GetWorld()->GetModuleReadOnly<ezWindWorldModuleInterface>();
  const auto* pPhysics = m_bCollision ? GetWorld()->GetModuleReadOnly<ezPhysicsWorldModuleInterface>() : nullptr;
  const bool bTransformChanged = transform != m_LastTransform;
  if (bTransformChanged)
    UpdatePatchBounds();

  for (auto& patch : m_Patches)
  {
    ezGrassPatchComponent* pComponent = nullptr;
    if (!GetWorld()->TryGetComponent(patch.m_hComponent, pComponent))
      continue;
    pComponent->m_fViewDistance = ezMath::Max(0.0f, m_fViewDistance);
    pComponent->m_uiSelectionId = GetUniqueIdForRendering();
    if (pComponent->GetMaterial() != GetMaterial())
      pComponent->SetMaterial(GetMaterial());
    if (pComponent->GetColor() != GetColor())
      pComponent->SetColor(GetColor());
    pComponent->GetOwner()->SetTags(GetOwner()->GetTags());
    const ezVec4 data(float(patch.m_uiSegments + 1), m_bCameraBend ? 1.0f : 0.0f, 0, 1731.0f);
    if (pComponent->GetCustomData() != data)
      pComponent->SetCustomData(data);

    float fDistance = cameras.IsEmpty() ? 0.0f : ezMath::MaxValue<float>();
    bool bInFrustum = cameras.IsEmpty();
    const auto box = pComponent->GetOwner()->GetGlobalBounds().GetBox();
    for (const auto& camera : cameras)
    {
      fDistance = ezMath::Min(fDistance, box.GetDistanceTo(camera.m_vPosition));
      bInFrustum |= camera.m_Frustum.GetObjectPosition(box) != ezVolumePosition::Outside;
    }
    // Spatial visibility includes the render pipeline's occlusion result and indirect views.
    // New patches get one initialization frame; visibility queries wake them without polling geometry.
    const bool bVisible = bInFrustum && fDistance < m_fViewDistance &&
      (cameras.IsEmpty() || bRebuilt || pComponent->GetOwner()->GetVisibilityState(2) != ezVisibilityState::Invisible);
    if (!bVisible)
    {
      patch.m_bWasSimulating = false;
      continue;
    }
    ++m_Statistics.m_uiVisiblePatches;

    const float fLodDistance = ezMath::Max(1.0f, m_fLodStartDistance);
    ezUInt32 uiLod = patch.m_uiLod == ezInvalidIndex ? 2 : patch.m_uiLod;
    // Hysteresis prevents topology changes while hovering at a LOD boundary.
    while (uiLod < 2 && fDistance > fLodDistance * (uiLod == 0 ? 1.0f : 3.0f) * 1.1f)
      ++uiLod;
    while (uiLod > 0 && fDistance < fLodDistance * (uiLod == 1 ? 1.0f : 3.0f) * 0.9f)
      --uiLod;

    const bool bTopologyChanged = patch.m_uiLod != uiLod;
    if (bTopologyChanged)
    {
      SetPatchLod(patch, *pComponent, uiLod);
      pComponent->SetCustomData(ezVec4(float(patch.m_uiSegments + 1), m_bCameraBend ? 1.0f : 0.0f, 0, 1731.0f));
    }

    bool bChanged = bTopologyChanged || bRebuilt || bTransformChanged;
    const bool bSimulate = GetWorld()->GetWorldSimulationEnabled() && m_fSimulationDistance > 0 && fDistance < m_fSimulationDistance;
    if (bSimulate && uiSteps > 0)
    {
      // Drop stale velocity when a previously hidden or distant patch wakes up.
      if (!patch.m_bWasSimulating)
        for (ezUInt32 i : patch.m_Strands)
          for (ezUInt32 j = 0; j < patch.m_uiSegments; ++j)
            m_Particles[i * m_uiSegmentCount + j].m_vPrevious = m_Particles[i * m_uiSegmentCount + j].m_vCurrent;
      for (ezUInt32 step = 0; step < uiSteps; ++step)
        bChanged |= SimulatePatch(patch, fStep, pPhysics, pWind);
      patch.m_bWasSimulating = true;
    }
    else if (!bSimulate)
      patch.m_bWasSimulating = false;

    if (bChanged)
      WritePatchMesh(patch, *pComponent, bTopologyChanged);
    m_Statistics.m_uiRenderTriangles += patch.m_Strands.GetCount() * patch.m_uiSegments * patch.m_uiBillboards * 2;
  }
  m_LastTransform = transform;
}
void ezGrassComponent::Rebuild()
{
  ClearPatches();
  m_bRebuild = false;
  m_fAccumulatedTime = 0;
  m_Strands.Clear();
  m_Particles.Clear();
  SetMeshResource({});
  SetBounds(ezBoundingBoxSphere::MakeInvalid());
  if (!m_hCpuMesh.IsValid() || m_uiStrandCount == 0 || m_fLength <= 0)
    return;

  ezResourceLock<ezCpuMeshResource> pMesh(m_hCpuMesh, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (pMesh.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;
  m_uiMeshChangeCounter = pMesh->GetCurrentResourceChangeCounter();
  const auto& buffer = pMesh->GetDescriptor().MeshBufferDesc();
  if (buffer.GetTopology() != ezGALPrimitiveTopology::Triangles || buffer.GetVertexCount() < 3)
    return;

  const auto positions = buffer.GetPositionData();
  const auto indices = buffer.GetIndexBufferData();
  const auto GetIndex = [&](ezUInt32 i) -> ezUInt32
  {
    if (!buffer.HasIndexBuffer())
      return i;
    return buffer.Uses32BitIndices() ? reinterpret_cast<const ezUInt32*>(indices.GetPtr())[i] : reinterpret_cast<const ezUInt16*>(indices.GetPtr())[i];
  };
  const auto GetLength = [&](ezUInt32 i) -> float
  {
    return i < m_VertexLengths.GetCount() ? ezMath::Clamp(m_VertexLengths[i], 0.0f, 1.0f) : 1.0f;
  };

  // Wicked selects from triangles whose vertex-length mask is non-zero.
  // Area weighting avoids changes in density when the source mesh is tessellated unevenly.
  struct Triangle
  {
    EZ_DECLARE_POD_TYPE();
    ezUInt32 m_uiIndices[3];
    float m_fCumulativeArea;
  };
  ezDynamicArray<Triangle> triangles;
  float fArea = 0;
  for (ezUInt32 t = 0; t < buffer.GetPrimitiveCount(); ++t)
  {
    Triangle tri = {{GetIndex(t * 3), GetIndex(t * 3 + 1), GetIndex(t * 3 + 2)}, 0};
    const auto* v = tri.m_uiIndices;
    if (v[0] >= positions.GetCount() || v[1] >= positions.GetCount() || v[2] >= positions.GetCount())
      continue;
    if (GetLength(v[0]) + GetLength(v[1]) + GetLength(v[2]) <= 0)
      continue;
    float fTriangleArea = (positions[v[1]] - positions[v[0]]).CrossRH(positions[v[2]] - positions[v[0]]).GetLength();
    if (!ezMath::IsFinite(fTriangleArea) || fTriangleArea < 0.000001f)
      continue;
    fArea += fTriangleArea;
    tri.m_fCumulativeArea = fArea;
    triangles.PushBack(tri);
  }
  if (triangles.IsEmpty())
    return;

  // Geometry is allocated per patch at its active LOD. Particle storage is bounded by the reflected count limits.
  ezRandom random;
  random.Initialize(m_uiRandomSeed);
  ezSimdPerlinNoise atlasNoise(m_uiRandomSeed);
  m_Strands.Reserve(m_uiStrandCount);
  for (ezUInt32 i = 0; i < m_uiStrandCount; ++i)
  {
    const float fSample = static_cast<float>(random.DoubleZeroToOneExclusive()) * fArea;
    ezUInt32 lo = 0, hi = triangles.GetCount() - 1;
    while (lo < hi)
    {
      const ezUInt32 mid = (lo + hi) / 2;
      if (triangles[mid].m_fCumulativeArea < fSample)
        lo = mid + 1;
      else
        hi = mid;
    }
    const auto* v = triangles[lo].m_uiIndices;
    float f = static_cast<float>(random.DoubleZeroToOneExclusive());
    float g = static_cast<float>(random.DoubleZeroToOneExclusive());
    if (f + g > 1)
    {
      f = 1 - f;
      g = 1 - g;
    }
    const float h = 1 - f - g;
    Strand strand;
    strand.m_vRoot = positions[v[0]] * h + positions[v[1]] * f + positions[v[2]] * g;
    strand.m_vNormal = (positions[v[1]] - positions[v[0]]).CrossRH(positions[v[2]] - positions[v[0]]).GetNormalized();
    if (buffer.GetVertexStreamConfig().HasNormal())
    {
      ezVec3 normal = buffer.GetNormal(v[0]) * h + buffer.GetNormal(v[1]) * f + buffer.GetNormal(v[2]) * g;
      normal.NormalizeIfNotZero(strand.m_vNormal).IgnoreResult();
      strand.m_vNormal = normal;
    }
    const ezVec3 tangent = strand.m_vNormal.GetOrthogonalVector().GetNormalized();
    const ezAngle angle = ezAngle::MakeFromRadian(static_cast<float>(random.DoubleZeroToOneExclusive()) * (2.0f * ezMath::Pi<float>()));
    strand.m_vTangent = tangent * ezMath::Cos(angle) + strand.m_vNormal.CrossRH(tangent) * ezMath::Sin(angle);
    if (!m_AtlasRects.IsEmpty())
    {
      // Smooth spatial variation gives nearby roots related atlas choices.
      const ezVec3 p = strand.m_vRoot * m_fUniformity;
      const float fNoise = atlasNoise.NoiseZeroToOne(ezSimdVec4f(p.x), ezSimdVec4f(p.y), ezSimdVec4f(p.z)).x();
      strand.m_Atlas = m_AtlasRects[static_cast<ezUInt32>(ezMath::Saturate(fNoise) * 1000) % m_AtlasRects.GetCount()];
    }
    strand.m_fLength = m_fLength * ezMath::Lerp(1.0f, static_cast<float>(random.DoubleZeroToOneExclusive()), m_fRandomness) *
                       (GetLength(v[0]) * h + GetLength(v[1]) * f + GetLength(v[2]) * g) * ezMath::Clamp(strand.m_Atlas.m_fSize, 0.0f, 10.0f);
    strand.m_fBillboardAngle[0] = 0;
    strand.m_fBillboardSize[0] = 1;
    for (ezUInt32 b = 1; b < m_uiBillboardCount; ++b)
    {
      strand.m_fBillboardAngle[b] = static_cast<float>(random.DoubleZeroToOneExclusive()) * ezMath::Pi<float>();
      strand.m_fBillboardSize[b] = ezMath::Lerp(0.2f, 1.0f, static_cast<float>(random.DoubleZeroToOneExclusive()));
    }
    m_Strands.PushBack(strand);
  }

  m_Particles.SetCount(m_Strands.GetCount() * m_uiSegmentCount);
  m_LastTransform = GetOwner()->GetGlobalTransform();
  for (ezUInt32 i = 0; i < m_Strands.GetCount(); ++i)
  {
    const auto& strand = m_Strands[i];
    for (ezUInt32 j = 0; j < m_uiSegmentCount; ++j)
    {
      auto& particle = m_Particles[i * m_uiSegmentCount + j];
      particle.m_vCurrent = m_LastTransform.TransformPosition(strand.m_vRoot + strand.m_vNormal * (strand.m_fLength * (j + 1) / m_uiSegmentCount));
      particle.m_vPrevious = particle.m_vCurrent;
    }
  }
  CreatePatches();
}

void ezGrassComponent::ClearPatches()
{
  for (auto& patch : m_Patches)
  {
    ezGameObject* pObject = nullptr;
    if (GetWorld()->TryGetObject(patch.m_hObject, pObject))
    {
      // Deactivate immediately; deferred deletion is safe while the world iterates components.
      pObject->SetActiveFlag(false);
      GetWorld()->DeleteObjectDelayed(patch.m_hObject, false);
    }
  }
  m_Patches.Clear();
}

void ezGrassComponent::CreatePatches()
{
  struct Cell
  {
    ezInt32 x, y, z;
    bool operator==(const Cell& other) const { return x == other.x && y == other.y && z == other.z; }
    bool operator<(const Cell& other) const
    {
      if (x != other.x) return x < other.x;
      if (y != other.y) return y < other.y;
      return z < other.z;
    }
  };
  ezMap<Cell, ezUInt32> cells;
  ezBoundingBox roots = ezBoundingBox::MakeInvalid();
  for (const auto& strand : m_Strands)
    roots.ExpandToInclude(strand.m_vRoot);
  const float fCellSize = ezMath::Clamp(m_fPatchSize, 1.0f, 64.0f);
  for (ezUInt32 i = 0; i < m_Strands.GetCount(); ++i)
  {
    const ezVec3 vCell = (m_Strands[i].m_vRoot - roots.m_vMin) / fCellSize;
    const Cell cell = {static_cast<ezInt32>(ezMath::Floor(vCell.x)), static_cast<ezInt32>(ezMath::Floor(vCell.y)), static_cast<ezInt32>(ezMath::Floor(vCell.z))};
    auto it = cells.Find(cell);
    if (!it.IsValid() || m_Patches[it.Value()].m_Strands.GetCount() >= 512)
    {
      const ezUInt32 uiIndex = m_Patches.GetCount();
      auto& patch = m_Patches.ExpandAndGetRef();
      patch.m_uiSegments = m_uiSegmentCount;
      patch.m_LastTransform = GetOwner()->GetGlobalTransform();
      ezGameObjectDesc desc;
      desc.m_sName.Assign("Grass Patch");
      desc.m_hParent = GetOwner()->GetHandle();
      desc.m_bDynamic = GetOwner()->IsDynamic();
      desc.m_Tags = GetOwner()->GetTags();
      desc.m_uiTeamID = GetOwner()->GetTeamID();
      ezGameObject* pObject = nullptr;
      patch.m_hObject = GetWorld()->CreateObject(desc, pObject);
      pObject->SetCreatedByPrefab();
      pObject->UpdateGlobalTransform();
      ezGrassPatchComponent* pComponent = nullptr;
      patch.m_hComponent = GetWorld()->GetOrCreateComponentManager<ezGrassPatchComponentManager>()->CreateComponent(pObject, pComponent);
      pComponent->SetCreatedByPrefab();
      pComponent->EnsureInitialized();
      cells[cell] = uiIndex;
      it = cells.Find(cell);
    }
    m_Patches[it.Value()].m_Strands.PushBack(i);
  }
  UpdatePatchBounds();
  for (auto& patch : m_Patches)
  {
    ezGrassPatchComponent* pComponent = nullptr;
    if (GetWorld()->TryGetComponent(patch.m_hComponent, pComponent))
    {
      // A cheap initial representation can be extracted immediately, including by auxiliary views.
      SetPatchLod(patch, *pComponent, 2);
      WritePatchMesh(patch, *pComponent, true);
    }
  }
}

void ezGrassComponent::UpdatePatchBounds()
{
  const ezVec3 scale = GetOwner()->GetGlobalTransform().m_vScale.Abs();
  const float fScaleRatio = ezMath::Max(scale.x, scale.y, scale.z) / ezMath::Max(0.00001f, ezMath::Min(scale.x, scale.y, scale.z));
  for (auto& patch : m_Patches)
  {
    ezGrassPatchComponent* pComponent = nullptr;
    if (!GetWorld()->TryGetComponent(patch.m_hComponent, pComponent))
      continue;
    ezBoundingBox bounds = ezBoundingBox::MakeInvalid();
    for (ezUInt32 i : patch.m_Strands)
    {
      const auto& strand = m_Strands[i];
      const auto uv = strand.m_Atlas.m_vTexMulAdd;
      const float fAspect = ezMath::Abs(uv.y) > 0.00001f ? ezMath::Abs(uv.x / uv.y) : 1.0f;
      // Includes every possible constrained pose, card width and camera bend.
      // Stable bounds let the spatial system retain its occlusion/visibility history.
      const float fRadius = strand.m_fLength * (1.8f + m_fWidth * fAspect * m_fTextureAspect * 0.5f) * fScaleRatio;
      bounds.ExpandToInclude(ezBoundingBox::MakeFromCenterAndHalfExtents(strand.m_vRoot, ezVec3(fRadius)));
    }
    pComponent->SetBounds(ezBoundingBoxSphere::MakeFromBox(bounds));
    pComponent->GetOwner()->UpdateLocalBounds();
    pComponent->GetOwner()->UpdateGlobalTransformAndBounds();
  }
}

void ezGrassComponent::SetPatchLod(Patch& ref_patch, ezGrassPatchComponent& ref_component, ezUInt32 uiLod)
{
  const ezUInt32 uiSegments = uiLod == 0 ? m_uiSegmentCount : (uiLod == 1 ? ezMath::Max(1u, (m_uiSegmentCount + 1) / 2) : 1u);
  const ezUInt32 uiBillboards = uiLod == 0 ? m_uiBillboardCount : (uiLod == 1 ? ezMath::Min(2u, m_uiBillboardCount) : 1u);
  const auto transform = GetOwner()->GetGlobalTransform();
  for (ezUInt32 i : ref_patch.m_Strands)
  {
    ezHybridArray<ezVec3, 12> nodes;
    nodes.PushBack(ref_patch.m_LastTransform.TransformPosition(m_Strands[i].m_vRoot));
    for (ezUInt32 j = 0; j < ref_patch.m_uiSegments; ++j)
      nodes.PushBack(m_Particles[i * m_uiSegmentCount + j].m_vCurrent);
    for (ezUInt32 j = 0; j < uiSegments; ++j)
    {
      const float fIndex = float(j + 1) * ref_patch.m_uiSegments / uiSegments;
      const ezUInt32 uiIndex = ezMath::Min(static_cast<ezUInt32>(fIndex), ref_patch.m_uiSegments - 1);
      auto& particle = m_Particles[i * m_uiSegmentCount + j];
      particle.m_vCurrent = ezMath::Lerp(nodes[uiIndex], nodes[uiIndex + 1], fIndex - uiIndex);
      particle.m_vPrevious = particle.m_vCurrent;
    }
  }
  ref_patch.m_uiLod = uiLod;
  ref_patch.m_uiSegments = uiSegments;
  ref_patch.m_uiBillboards = uiBillboards;
  const ezUInt32 uiVertices = ref_patch.m_Strands.GetCount() * (uiSegments + 1) * 2 * uiBillboards;
  const ezUInt32 uiPrimitives = ref_patch.m_Strands.GetCount() * uiSegments * uiBillboards * 2;
  ref_component.CreateMeshResource(ezGALPrimitiveTopology::Triangles, uiVertices, uiPrimitives, ezGALIndexType::UInt);
  ref_component.SetUsePrimitiveRange(0, uiPrimitives);
}

bool ezGrassComponent::SimulatePatch(Patch& ref_patch, float fDeltaTime, const ezPhysicsWorldModuleInterface* pPhysics, const ezWindWorldModuleInterface* pWind)
{
  EZ_PROFILE_SCOPE("Grass.SimulatePatch");
  const auto transform = GetOwner()->GetGlobalTransform();
  const auto oldInverse = ref_patch.m_LastTransform.GetInverse();
  const bool bMoved = transform != ref_patch.m_LastTransform;
  const ezPhysicsQueryParameters query(m_uiCollisionLayer, ezPhysicsShapeType::Character | ezPhysicsShapeType::Dynamic);
  const float fRadius = ezMath::Clamp(m_fCollisionRadius, 0.001f, 1.0f);
  struct Collider
  {
    ezUInt32 m_uiId;
    ezTransform m_Transform;
    ezBoundingBox m_Bounds;
  };
  ezHybridArray<Collider, 8> colliders;
  ezGrassPatchComponent* pPatch = nullptr;
  GetWorld()->TryGetComponent(ref_patch.m_hComponent, pPatch);
  if (pPhysics != nullptr && pPatch != nullptr)
  {
    // One broad-phase query per patch, rather than one query per strand.
    const auto bounds = pPatch->GetOwner()->GetGlobalBounds();
    ezPhysicsOverlapResultArray overlaps;
    pPhysics->QueryShapesInSphere(overlaps, bounds.m_fSphereRadius + fRadius, bounds.m_vCenter, query);
    for (const auto& overlap : overlaps.m_Results)
    {
      ezGameObject* pActor = nullptr;
      if (!GetWorld()->TryGetObject(overlap.m_hActorObject, pActor))
        continue;
      auto& collider = colliders.ExpandAndGetRef();
      collider.m_uiId = overlap.m_uiObjectFilterID;
      collider.m_Transform = pActor->GetGlobalTransform();
      collider.m_Transform.m_vPosition = overlap.m_vCenterPosition;
      const auto actorBounds = pPhysics->GetWorldSpaceBounds(pActor, m_uiCollisionLayer, query.m_ShapeTypes, true);
      // Some backends do not expose character bounds through this interface.
      // The patch's query bounds are a conservative fallback, never a guessed capsule size.
      collider.m_Bounds = actorBounds.IsValid() ? actorBounds.GetBox() : bounds.GetBox();
    }
  }

  bool bChanged = bMoved;
  for (ezUInt32 i : ref_patch.m_Strands)
  {
    auto& strand = m_Strands[i];
    bool bRestingContact = false;
    if (!bMoved && strand.m_uiContactId != ezInvalidIndex)
      for (const auto& collider : colliders)
        if (strand.m_uiContactId == collider.m_uiId && strand.m_ContactTransform.IsEqual(collider.m_Transform, 0.001f) &&
            strand.m_ContactBounds.IsEqual(collider.m_Bounds, 0.001f))
          bRestingContact = true;
    if (bRestingContact)
      continue;

    strand.m_uiContactId = ezInvalidIndex;
    const ezVec3 root = transform.TransformPosition(strand.m_vRoot);
    const ezVec3 axis = transform.m_qRotation * strand.m_vNormal.CompDiv(transform.m_vScale).GetNormalized();
    const float fLength = transform.TransformDirection(strand.m_vNormal * strand.m_fLength).GetLength();
    const float fSegmentLength = fLength / ref_patch.m_uiSegments;
    const Collider* pClosestCollider = nullptr;
    float fClosestDistance = ezMath::MaxValue<float>();
    for (const auto& collider : colliders)
    {
      const float fDistance = collider.m_Bounds.GetDistanceSquaredTo(root);
      if (fDistance <= ezMath::Square(fLength + fRadius) && fDistance < fClosestDistance)
      {
        pClosestCollider = &collider;
        fClosestDistance = fDistance;
      }
    }

    ezVec3 wind = ezVec3::MakeZero();
    if (pWind != nullptr)
    {
      wind = pWind->GetWindAt(root);
      wind += pWind->ComputeWindFlutter(wind, axis, 1.0f, i + m_uiRandomSeed);
      wind *= m_fWindInfluence;
    }
    ezVec3 base = root;
    bool bContact = false;
    for (ezUInt32 j = 0; j < ref_patch.m_uiSegments; ++j)
    {
      ++m_Statistics.m_uiSimulatedParticles;
      auto& particle = m_Particles[i * m_uiSegmentCount + j];
      const ezVec3 current = bMoved ? transform.TransformPosition(oldInverse.TransformPosition(particle.m_vCurrent)) : particle.m_vCurrent;
      const ezVec3 previous = bMoved ? current : particle.m_vPrevious;
      const ezVec3 force = axis * ezMath::Max(0.0f, m_fStiffness) +
        ezVec3(0, 0, -ezMath::Max(0.0f, m_fGravityPower)) + wind * (float(j + 1) / ref_patch.m_uiSegments);
      ezVec3 next = current + (current - previous) * (1 - ezMath::Clamp(m_fDrag, 0.0f, 1.0f)) + force * fDeltaTime;
      auto Constrain = [&](const ezVec3& vPosition)
      {
        ezVec3 direction = vPosition - base;
        direction -= axis * ezMath::Min(0.0f, direction.Dot(axis));
        direction.NormalizeIfNotZero(axis).IgnoreResult();
        return base + direction * fSegmentLength;
      };
      next = Constrain(next);
      if (pClosestCollider != nullptr)
      {
        // Check the swept tip, so the spring cannot step through the supporting collider.
        const ezVec3 delta = next - current;
        const float fDistance = delta.GetLength();
        ezPhysicsCastResult hit;
        if (fDistance > 0.00001f && pPhysics->SweepTestSphere(hit, fRadius, current, delta / fDistance, fDistance, query))
        {
          next = Constrain(current + delta / fDistance * ezMath::Max(0.0f, hit.m_fDistance - 0.002f));
          bContact = true;
        }
        if (pPhysics->OverlapTestSphere(fRadius, next, query))
        {
          bContact = true;
          // Use the root, not the oscillating tip, for a stable bend direction.
          ezVec3 away = root - pClosestCollider->m_Transform.m_vPosition;
          away -= axis * away.Dot(axis);
          away.NormalizeIfNotZero(transform.m_qRotation * strand.m_vTangent).IgnoreResult();
          const ezVec3 initial = next - base;
          for (ezUInt32 k = 1; k <= 12; ++k)
          {
            const float fBlend = float(k) / 12.0f;
            next = Constrain(base + ezMath::Lerp(initial, away * fSegmentLength, fBlend));
            if (!pPhysics->OverlapTestSphere(fRadius + 0.002f, next, query))
              break;
          }
        }
      }
      bChanged |= !next.IsEqual(current, 0.000001f);
      // Contact corrections are not velocity. Keeping them in Verlet history injects energy every frame.
      particle.m_vPrevious = bContact ? next : current;
      particle.m_vCurrent = next;
      base = next;
    }
    if (bContact && pClosestCollider != nullptr)
    {
      strand.m_uiContactId = pClosestCollider->m_uiId;
      strand.m_ContactTransform = pClosestCollider->m_Transform;
      strand.m_ContactBounds = pClosestCollider->m_Bounds;
      for (ezUInt32 j = 0; j < ref_patch.m_uiSegments; ++j)
        m_Particles[i * m_uiSegmentCount + j].m_vPrevious = m_Particles[i * m_uiSegmentCount + j].m_vCurrent;
    }
  }
  ref_patch.m_LastTransform = transform;
  return bChanged;
}

void ezGrassComponent::WritePatchMesh(Patch& ref_patch, ezGrassPatchComponent& ref_component, bool bTopologyChanged)
{
  EZ_PROFILE_SCOPE("Grass.WritePatchMesh");
  const auto transform = GetOwner()->GetGlobalTransform();
  const auto inverse = transform.GetInverse();
  const auto oldInverse = ref_patch.m_LastTransform.GetInverse();
  const bool bMoved = transform != ref_patch.m_LastTransform;
  const float fScale = ezMath::Max(ezMath::Abs(transform.m_vScale.x), ezMath::Abs(transform.m_vScale.y), ezMath::Abs(transform.m_vScale.z));
  ezResourceLock<ezDynamicMeshBufferResource> pMesh(ref_component.GetMeshResource(), ezResourceAcquireMode::BlockTillLoaded);
  auto positions = pMesh->AccessPositionData();
  auto ntts = pMesh->AccessNormalTangentTexCoord0Data();
  ezArrayPtr<ezColorLinear16f> colors;
  ezArrayPtr<ezUInt32> indices;
  if (bTopologyChanged)
  {
    colors = pMesh->AccessColorData();
    indices = pMesh->AccessIndex32Data();
  }
  m_Statistics.m_uiUploadedVertices += positions.GetCount();
  const ezUInt32 uiVerticesPerStrand = (ref_patch.m_uiSegments + 1) * 2 * ref_patch.m_uiBillboards;
  ezUInt32 uiStrandIndex = 0;
  ezUInt32 uiIndex = 0;
  for (ezUInt32 i : ref_patch.m_Strands)
  {
    const auto& strand = m_Strands[i];
    ezHybridArray<ezVec3, 12> nodes;
    const ezVec3 root = transform.TransformPosition(strand.m_vRoot);
    nodes.PushBack(root);
    for (ezUInt32 j = 0; j < ref_patch.m_uiSegments; ++j)
    {
      auto& particle = m_Particles[i * m_uiSegmentCount + j];
      if (bMoved)
      {
        particle.m_vCurrent = transform.TransformPosition(oldInverse.TransformPosition(particle.m_vCurrent));
        particle.m_vPrevious = particle.m_vCurrent;
      }
      nodes.PushBack(particle.m_vCurrent);
    }
    const ezVec3 axis = transform.m_qRotation * strand.m_vNormal;
    for (ezUInt32 b = 0; b < ref_patch.m_uiBillboards; ++b)
    {
      const ezAngle angle = ezAngle::MakeFromRadian(strand.m_fBillboardAngle[b]);
      const ezVec3 tangent = transform.m_qRotation * (strand.m_vTangent * ezMath::Cos(angle) + strand.m_vNormal.CrossRH(strand.m_vTangent) * ezMath::Sin(angle));
      const auto uv = strand.m_Atlas.m_vTexMulAdd;
      const float fAspect = ezMath::Abs(uv.y) > 0.00001f ? ezMath::Abs(uv.x / uv.y) : 1.0f;
      const float fHalfWidth = strand.m_fLength * m_fWidth * fAspect * m_fTextureAspect * fScale * 0.5f * strand.m_fBillboardSize[b];
      for (ezUInt32 j = 0; j <= ref_patch.m_uiSegments; ++j)
      {
        ezVec3 direction = nodes[ezMath::Min(j + 1, ref_patch.m_uiSegments)] - nodes[j == ref_patch.m_uiSegments ? j - 1 : j];
        direction.NormalizeIfNotZero(axis).IgnoreResult();
        ezVec3 side = tangent - direction * tangent.Dot(direction);
        side.NormalizeIfNotZero(direction.GetOrthogonalVector().GetNormalized()).IgnoreResult();
        for (ezUInt32 edge = 0; edge < 2; ++edge)
        {
          const ezUInt32 v = uiStrandIndex * uiVerticesPerStrand + b * (ref_patch.m_uiSegments + 1) * 2 + j * 2 + edge;
          const ezVec3 center = ezMath::Lerp(root, nodes[j], strand.m_fBillboardSize[b]);
          positions[v] = inverse.TransformPosition(center + side * (edge == 0 ? -fHalfWidth : fHalfWidth));
          ntts[v].EncodeNormal((transform.m_qRotation.GetInverse() * direction).CompMul(transform.m_vScale).GetNormalized());
          ntts[v].EncodeTangent(inverse.TransformDirection(side).GetNormalized(), 1.0f);
          if (bTopologyChanged)
          {
            ntts[v].m_vTexCoord = ezVec2(edge * uv.x + uv.z, (1.0f - float(j) / ref_patch.m_uiSegments) * uv.y + uv.w);
            colors[v] = ezColor(1, 1, 1, strand.m_fLength * float(j) / ref_patch.m_uiSegments * strand.m_fBillboardSize[b]);
          }
        }
        if (bTopologyChanged && j < ref_patch.m_uiSegments)
        {
          const ezUInt32 v = uiStrandIndex * uiVerticesPerStrand + b * (ref_patch.m_uiSegments + 1) * 2 + j * 2;
          indices[uiIndex++] = v;
          indices[uiIndex++] = v + 1;
          indices[uiIndex++] = v + 2;
          indices[uiIndex++] = v + 2;
          indices[uiIndex++] = v + 1;
          indices[uiIndex++] = v + 3;
        }
      }
    }
    ++uiStrandIndex;
  }
  ref_patch.m_LastTransform = transform;
}

ezGrassComponentManager::ezGrassComponentManager(ezWorld* pWorld)
  : ezComponentManager(pWorld)
{
}

void ezGrassComponentManager::Initialize()
{
  SUPER::Initialize();
  auto desc = EZ_CREATE_MODULE_UPDATE_FUNCTION_DESC(ezGrassComponentManager::Update, this);
  desc.m_Phase = ezWorldUpdatePhase::PostTransform;
  desc.m_bOnlyUpdateWhenSimulating = false;
  RegisterUpdateFunction(desc);
}

void ezGrassComponentManager::Update(const ezWorldModule::UpdateContext& context)
{
  for (auto it = m_ComponentStorage.GetIterator(context.m_uiFirstComponentIndex, context.m_uiComponentCount); it.IsValid(); ++it)
    if (it->IsActiveAndInitialized())
      it->Update();
}

EZ_STATICLINK_FILE(GrassPlugin, GrassPlugin_Components_GrassComponent);

void ezGrassComponent::SetStrandCount(ezUInt32 value)
{
  m_uiStrandCount = ezMath::Min(value, 100000u);
  m_bRebuild = true;
}

void ezGrassComponent::SetPatchSize(float value)
{
  m_fPatchSize = ezMath::IsFinite(value) ? ezMath::Clamp(value, 1.0f, 64.0f) : 8.0f;
  m_bRebuild = true;
}

class ezGrassComponent_1_2 : public ezGraphPatch
{
public:
  ezGrassComponent_1_2()
    : ezGraphPatch("ezGrassComponent", 2)
  {
  }

  virtual void Patch(ezGraphPatchContext& ref_context, ezAbstractObjectGraph* pGraph, ezAbstractObjectNode* pNode) const override
  {
    pNode->RenameProperty("ViewDistance", "SimulationDistance");
  }
};

ezGrassComponent_1_2 g_ezGrassComponent_1_2;

void ezGrassComponent::SetSegmentCount(ezUInt32 value)
{
  m_uiSegmentCount = ezMath::Clamp(value, 1u, 10u);
  m_bRebuild = true;
}

void ezGrassComponent::SetBillboardCount(ezUInt32 value)
{
  m_uiBillboardCount = ezMath::Clamp(value, 1u, 10u);
  m_bRebuild = true;
}

void ezGrassComponent::SetRandomSeed(ezUInt32 value)
{
  m_uiRandomSeed = value;
  m_bRebuild = true;
}

void ezGrassComponent::SetLength(float value)
{
  m_fLength = ezMath::IsFinite(value) ? ezMath::Clamp(value, 0.0f, 100.0f) : 1.0f;
  m_bRebuild = true;
}

void ezGrassComponent::SetWidth(float value)
{
  m_fWidth = ezMath::IsFinite(value) ? ezMath::Clamp(value, 0.0f, 10.0f) : 1.0f;
  m_bRebuild = true;
}

void ezGrassComponent::SetRandomness(float value)
{
  m_fRandomness = ezMath::IsFinite(value) ? ezMath::Clamp(value, 0.0f, 1.0f) : 0.2f;
  m_bRebuild = true;
}

void ezGrassComponent::SetUniformity(float value)
{
  m_fUniformity = ezMath::IsFinite(value) ? ezMath::Clamp(value, 0.01f, 100.0f) : 1.0f;
  m_bRebuild = true;
}

void ezGrassComponent::AtlasRects_SetValue(ezUInt32 uiIndex, ezGrassAtlasRect value)
{
  m_AtlasRects[uiIndex] = value;
  m_bRebuild = true;
}
void ezGrassComponent::AtlasRects_Insert(ezUInt32 uiIndex, ezGrassAtlasRect value)
{
  m_AtlasRects.InsertAt(uiIndex, value);
  m_bRebuild = true;
}
void ezGrassComponent::AtlasRects_Remove(ezUInt32 uiIndex)
{
  m_AtlasRects.RemoveAtAndCopy(uiIndex);
  m_bRebuild = true;
}

void ezGrassComponent::VertexLengths_SetValue(ezUInt32 uiIndex, float value)
{
  m_VertexLengths[uiIndex] = value;
  m_bRebuild = true;
}
void ezGrassComponent::VertexLengths_Insert(ezUInt32 uiIndex, float value)
{
  m_VertexLengths.InsertAt(uiIndex, value);
  m_bRebuild = true;
}
void ezGrassComponent::VertexLengths_Remove(ezUInt32 uiIndex)
{
  m_VertexLengths.RemoveAtAndCopy(uiIndex);
  m_bRebuild = true;
}
