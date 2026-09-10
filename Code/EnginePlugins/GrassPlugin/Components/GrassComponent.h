#pragma once

#include <GrassPlugin/GrassPluginDLL.h>
#include <RendererCore/Meshes/CpuMeshResource.h>
#include <RendererCore/Meshes/CustomMeshComponent.h>

class ezGrassPatchComponent;
class ezPhysicsWorldModuleInterface;
class ezWindWorldModuleInterface;

/// A normalized texture rectangle and its strand size multiplier, matching Wicked's AtlasRect.
struct EZ_GRASSPLUGIN_DLL ezGrassAtlasRect
{
  ezVec4 m_vTexMulAdd = ezVec4(1, 1, 0, 0);
  float m_fSize = 1.0f;
};
EZ_DECLARE_REFLECTABLE_TYPE(EZ_GRASSPLUGIN_DLL, ezGrassAtlasRect);

class EZ_GRASSPLUGIN_DLL ezGrassComponentManager : public ezComponentManager<class ezGrassComponent, ezBlockStorageType::Compact>
{
public:
  ezGrassComponentManager(ezWorld* pWorld);
  virtual void Initialize() override;

private:
  void Update(const ezWorldModule::UpdateContext& context);
};

/// Grows textured, segmented grass cards on a mesh asset in owner-local space.
/// Uses ezEngine's dynamic mesh renderer, wind interface and physics queries; no Jolt dependency.
/// The Material should use Shaders/Grass.ezShader for distance fading and camera bending.
class EZ_GRASSPLUGIN_DLL ezGrassComponent : public ezCustomMeshComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezGrassComponent, ezCustomMeshComponent, ezGrassComponentManager);

public:
  ezGrassComponent();
  ~ezGrassComponent();
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;

  void SetMesh(const ezMeshResourceHandle& hMesh);
  const ezMeshResourceHandle& GetMesh() const { return m_hMesh; }
  EZ_ADD_RESOURCEHANDLE_ACCESSORS(Mesh, m_hMesh);

  // Topology and distribution edits rebuild at the next world update, including in the editor.
  void SetStrandCount(ezUInt32 uiValue);
  ezUInt32 GetStrandCount() const { return m_uiStrandCount; }
  void SetSegmentCount(ezUInt32 uiValue);
  ezUInt32 GetSegmentCount() const { return m_uiSegmentCount; }
  void SetBillboardCount(ezUInt32 uiValue);
  ezUInt32 GetBillboardCount() const { return m_uiBillboardCount; }
  void SetRandomSeed(ezUInt32 uiValue);
  ezUInt32 GetRandomSeed() const { return m_uiRandomSeed; }
  void SetLength(float fValue);
  float GetLength() const { return m_fLength; }
  void SetWidth(float fValue);
  float GetWidth() const { return m_fWidth; }
  void SetRandomness(float fValue);
  float GetRandomness() const { return m_fRandomness; }
  void SetUniformity(float fValue);
  float GetUniformity() const { return m_fUniformity; }

  float m_fStiffness = 0.5f;
  float m_fDrag = 0.1f;
  float m_fGravityPower = 0.0f;
  float m_fSimulationDistance = 40.0f;
  float m_fViewDistance = 200.0f;
  float m_fLodStartDistance = 15.0f;
  void SetPatchSize(float fValue);
  float GetPatchSize() const { return m_fPatchSize; }
  bool m_bCameraBend = true;
  float m_fWindInfluence = 1.0f;
  bool m_bCollision = true;
  ezUInt8 m_uiCollisionLayer = 0;
  float m_fCollisionRadius = 0.02f;

  ezUInt32 AtlasRects_GetCount() const { return m_AtlasRects.GetCount(); }
  ezGrassAtlasRect AtlasRects_GetValue(ezUInt32 uiIndex) const { return m_AtlasRects[uiIndex]; }
  void AtlasRects_SetValue(ezUInt32 uiIndex, ezGrassAtlasRect value);
  void AtlasRects_Insert(ezUInt32 uiIndex, ezGrassAtlasRect value);
  void AtlasRects_Remove(ezUInt32 uiIndex);
  ezUInt32 VertexLengths_GetCount() const { return m_VertexLengths.GetCount(); }
  float VertexLengths_GetValue(ezUInt32 uiIndex) const { return m_VertexLengths[uiIndex]; }
  void VertexLengths_SetValue(ezUInt32 uiIndex, float fValue);
  void VertexLengths_Insert(ezUInt32 uiIndex, float fValue);
  void VertexLengths_Remove(ezUInt32 uiIndex);

  struct Statistics
  {
    ezUInt32 m_uiVisiblePatches = 0;
    ezUInt32 m_uiSimulatedParticles = 0;
    ezUInt32 m_uiUploadedVertices = 0;
    ezUInt32 m_uiRenderTriangles = 0;
  };
  const Statistics& GetStatistics() const { return m_Statistics; }
  ezUInt32 GetPatchCount() const { return m_Patches.GetCount(); }
  ezComponentHandle GetPatch(ezUInt32 uiIndex) const { return m_Patches[uiIndex].m_hComponent; }

private:
  friend class ezGrassComponentManager;
  void Update();
  void Rebuild();
  void ClearPatches();
  void CreatePatches();
  void UpdatePatchBounds();

  struct Patch;
  void SetPatchLod(Patch& ref_patch, ezGrassPatchComponent& ref_component, ezUInt32 uiLod);
  bool SimulatePatch(Patch& ref_patch, float fDeltaTime, const ezPhysicsWorldModuleInterface* pPhysics, const ezWindWorldModuleInterface* pWind);
  void WritePatchMesh(Patch& ref_patch, ezGrassPatchComponent& ref_component, bool bTopologyChanged);

  struct Strand
  {
    ezVec3 m_vRoot;
    ezVec3 m_vNormal;
    ezVec3 m_vTangent;
    float m_fLength;
    ezGrassAtlasRect m_Atlas;
    float m_fBillboardAngle[10] = {};
    float m_fBillboardSize[10] = {};
    ezUInt32 m_uiContactId = ezInvalidIndex;
    ezTransform m_ContactTransform = ezTransform::MakeIdentity();
    ezBoundingBox m_ContactBounds = ezBoundingBox::MakeInvalid();
  };
  struct Particle
  {
    ezVec3 m_vCurrent;
    ezVec3 m_vPrevious;
  };
  struct Patch
  {
    ezGameObjectHandle m_hObject;
    ezComponentHandle m_hComponent;
    ezDynamicArray<ezUInt32> m_Strands;
    ezUInt32 m_uiLod = ezInvalidIndex;
    ezUInt32 m_uiSegments = 1;
    ezUInt32 m_uiBillboards = 1;
    bool m_bWasSimulating = false;
    ezTransform m_LastTransform = ezTransform::MakeIdentity();
  };
  ezDynamicArray<Patch> m_Patches;
  Statistics m_Statistics;
  float m_fPatchSize = 8.0f;
  ezMeshResourceHandle m_hMesh;
  ezCpuMeshResourceHandle m_hCpuMesh;
  ezUInt32 m_uiMeshChangeCounter = 0;
  ezUInt32 m_uiStrandCount = 1000;
  ezUInt32 m_uiSegmentCount = 1;
  ezUInt32 m_uiBillboardCount = 1;
  ezUInt32 m_uiRandomSeed = 1;
  float m_fLength = 1.0f;
  float m_fWidth = 1.0f;
  float m_fRandomness = 0.2f;
  float m_fUniformity = 1.0f;
  ezDynamicArray<ezGrassAtlasRect> m_AtlasRects;
  ezDynamicArray<float> m_VertexLengths;
  ezDynamicArray<Strand> m_Strands;
  ezDynamicArray<Particle> m_Particles;
  bool m_bRebuild = true;
  float m_fAccumulatedTime = 0.0f;
  float m_fTextureAspect = 1.0f;
  ezTransform m_LastTransform = ezTransform::MakeIdentity();
};
