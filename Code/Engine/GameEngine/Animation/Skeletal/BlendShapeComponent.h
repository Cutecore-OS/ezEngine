#pragma once

#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <Foundation/Types/RangeView.h>
#include <GameEngine/Animation/Skeletal/AnimatedMeshComponent.h>
#include <RendererCore/AnimationSystem/Declarations.h>
#include <RendererCore/Meshes/DynamicMeshBufferResource.h>

/// Per-instance morph deformation owned by the Animated Mesh Component.
class EZ_GAMEENGINE_DLL ezBlendShapeDeformer
{
public:
  explicit ezBlendShapeDeformer(ezAnimatedMeshComponent& component);
  void Update();
  void OnAnimationCurve(const ezMsgAnimationCurveValue& msg);
  ezSkinnedMeshRenderData* CreateRenderData(const ezRenderDataManager* pManager) const;
  float GetWeight(ezStringView sName) const;
  ezUInt32 GetDeformationRevision() const { return m_uiDeformationRevision; }
  const ezDynamicMeshBufferResourceHandle& GetDeformedBuffer() const { return m_hDeformedMesh; }

private:
  ezAnimatedMeshComponent& m_Component;
  ezMeshResourceHandle m_hMesh;
  ezBlendShapeResourceHandle m_hBlendShapes;
  ezDynamicMeshBufferResourceHandle m_hDeformedMesh;
  struct CurveValue
  {
    float m_fWeight = 0;
    ezTime m_LastUpdate;
  };
  ezHashTable<ezHashedString, CurveValue> m_CurveValues;
  ezDynamicArray<float> m_LastWeights;
  ezDynamicArray<ezVec3> m_Positions;
  ezDynamicArray<ezVec3> m_Normals;
  ezDynamicArray<ezVec4> m_Tangents;
  ezUInt32 m_uiMeshVersion = ezInvalidIndex;
  ezUInt32 m_uiResourceVersion = ezInvalidIndex;
  ezUInt32 m_uiDeformationRevision = 0;
  bool m_bUseDeformedMesh = false;
};

using ezBlendShapePoseComponentManager = ezComponentManager<class ezBlendShapePoseComponent, ezBlockStorageType::Compact>;

/// Manual morph pose for the Animated Mesh on the same object. Names/defaults are exposed by its mesh asset.
class EZ_GAMEENGINE_DLL ezBlendShapePoseComponent : public ezComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezBlendShapePoseComponent, ezComponent, ezBlendShapePoseComponentManager);

public:
  const ezRangeView<const char*, ezUInt32> GetWeightKeys() const;
  bool GetWeightValue(const char* szKey, float& out_fValue) const;
  void SetWeight(const char* szKey, float fValue);
  void RemoveWeight(const char* szKey);
  void ResetWeights();
  float GetWeight(const char* szKey) const;
  float GetWeightThreshold() const { return m_fWeightThreshold; }
  void SetWeightThreshold(float fThreshold);
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;

private:
  // Synchronized by the editor with the sibling Animated Mesh; only used to expose its parameter metadata.
  ezMeshResourceHandle m_hMesh;
  ezMap<ezString, float> m_Weights;
  float m_fWeightThreshold = 0.001f;
};
