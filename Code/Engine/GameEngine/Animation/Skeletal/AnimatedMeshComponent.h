#pragma once

#include <GameEngine/GameEngineDLL.h>
#include <RendererCore/AnimationSystem/AnimationPose.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <RendererCore/Meshes/SkinnedMeshRenderData.h>

using ezSkeletonResourceHandle = ezTypedResourceHandle<class ezSkeletonResource>;
using ezBlendShapeResourceHandle = ezTypedResourceHandle<class ezBlendShapeResource>;
struct ezMsgAnimationCurveValue;
struct ezMsgSetBlendShapeWeight;
struct ezMsgBlendShapesPoseUpdated;
struct ezMsgQueryAnimationBlendShapes;

class EZ_GAMEENGINE_DLL ezAnimatedMeshComponentManager : public ezComponentManager<class ezAnimatedMeshComponent, ezBlockStorageType::FreeList>
{
public:
  ezAnimatedMeshComponentManager(ezWorld* pWorld);
  ~ezAnimatedMeshComponentManager();

  virtual void Initialize() override;

  void Update(const ezWorldModule::UpdateContext& context);
  void AddToUpdateList(ezAnimatedMeshComponent* pComponent);

private:
  void ResourceEventHandler(const ezResourceEvent& e);

  ezDeque<ezComponentHandle> m_ComponentsToUpdate;
};

/// Instantiates a mesh that can be animated through skeletal animation.
///
/// The referenced mesh has to contain skinning information.
///
/// This component only creates an animated mesh for rendering. It does not animate the mesh in any way.
/// The component handles messages of type ezMsgAnimationPoseUpdated. Using this message other systems can set a new pose
/// for the animated mesh.
///
/// For example the ezSkeletonPoseComponent, ezSimpleAnimationComponent and ezAnimationControllerComponent do this
/// to change the pose of the animated mesh.
class EZ_GAMEENGINE_DLL ezAnimatedMeshComponent : public ezMeshComponentBase
{
  EZ_DECLARE_COMPONENT_TYPE(ezAnimatedMeshComponent, ezMeshComponentBase, ezAnimatedMeshComponentManager);


  //////////////////////////////////////////////////////////////////////////
  // ezComponent

public:
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;

protected:
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // ezMeshComponentBase

protected:
  virtual ezTransform GetFinalGlobalTransform() const override;
  virtual ezMeshRenderData* CreateRenderData(const ezRenderDataManager* pRenderDataManager) const override;
  virtual ezResult GetLocalBounds(ezBoundingBoxSphere& bounds, bool& bAlwaysVisible, ezMsgUpdateLocalBounds& msg) override;
  virtual const ezMeshResourceHandle& GetMeshToRender() const override;

  //////////////////////////////////////////////////////////////////////////
  // ezAnimatedMeshComponent

public:
  ezAnimatedMeshComponent();
  ~ezAnimatedMeshComponent();

  void RetrievePose(ezDynamicArray<ezMat4>& out_modelTransforms, ezTransform& out_rootTransform, const ezSkeleton& skeleton);

  /// Sets the blend shape weight for the given shape key. (AngelScript / scriptable)
  void SetBlendShapeWeight(ezStringView sShapeName, float fWeight);

  /// Gets the blend shape weight for the given shape key. (AngelScript / scriptable)
  float GetBlendShapeWeight(ezStringView sShapeName) const;

  /// Resets all blend shape weights to zero. (AngelScript / scriptable)
  void ResetBlendShapeWeights();

  void SetDefaultBlendShapes(const ezBlendShapeResourceHandle& hBlendShapes);
  const ezBlendShapeResourceHandle& GetDefaultBlendShapes() const { return m_hDefaultBlendShapes; }

protected:
  void OnAnimationPoseUpdated(ezMsgAnimationPoseUpdated& msg);                          // [ msg handler ]
  void OnQueryAnimationSkeleton(ezMsgQueryAnimationSkeleton& msg);                      // [ msg handler ]
  void OnMsgCustomInstanceDataOffsetChanged(ezMsgCustomInstanceDataOffsetChanged& msg); // [ msg handler ]
  void OnMsgAnimationCurveValue(ezMsgAnimationCurveValue& msg);                          // [ msg handler ]
  void OnMsgSetBlendShapeWeight(ezMsgSetBlendShapeWeight& msg);                          // [ msg handler ]
  void OnMsgBlendShapesPoseUpdated(ezMsgBlendShapesPoseUpdated& msg);                    // [ msg handler ]
  void OnMsgQueryAnimationBlendShapes(ezMsgQueryAnimationBlendShapes& msg);              // [ msg handler ]

  void InitializeAnimationPose();

  void MapModelSpacePoseToSkinningSpace(const ezHashTable<ezHashedString, ezMeshResourceDescriptor::BoneData>& bones, const ezSkeleton& skeleton, ezArrayPtr<const ezMat4> modelSpaceTransforms, ezBoundingBox* bounds);

  void ApplyBlendShapes();
  void UpdateDeformedMeshBuffer(const ezMeshResource* pBaseMesh, const ezBlendShapeResourceDescriptor& blendShapeDesc, float fThreshold);

  ezTransform m_RootTransform = ezTransform::MakeIdentity();
  ezBoundingBox m_MaxBounds;
  ezSkinningState m_SkinningState;
  ezSkeletonResourceHandle m_hDefaultSkeleton;
  ezBlendShapeResourceHandle m_hDefaultBlendShapes;

  ezMap<ezHashedString, float> m_BlendShapeWeights;
  ezMap<ezHashedString, float> m_LastAppliedWeights;
  bool m_bBlendShapesDirty = false;
  ezMeshResourceHandle m_hDeformedMesh;
  ezMeshBufferResourceHandle m_hDeformedMeshBuffer;
};


struct ezRootMotionMode
{
  using StorageType = ezInt8;

  enum Enum
  {
    Ignore,
    ApplyToOwner,
    SendMoveCharacterMsg,

    Default = Ignore
  };

  EZ_GAMEENGINE_DLL static void Apply(ezRootMotionMode::Enum mode, ezGameObject* pObject, const ezVec3& vTranslation, ezAngle rotationX, ezAngle rotationY, ezAngle rotationZ);
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_GAMEENGINE_DLL, ezRootMotionMode);
