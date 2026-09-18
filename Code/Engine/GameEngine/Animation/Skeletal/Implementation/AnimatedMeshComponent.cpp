#include <GameEngine/GameEnginePCH.h>

#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <GameEngine/Animation/Skeletal/AnimatedMeshComponent.h>
#include <GameEngine/Physics/CharacterControllerComponent.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <RendererCore/AnimationSystem/Declarations.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <RendererCore/Meshes/MeshBufferResource.h>
#include <RendererCore/Pipeline/RenderDataManager.h>

#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/span.h>

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezAnimatedMeshComponent, 14, ezComponentMode::Static);
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_ACCESSOR_PROPERTY("Mesh", GetMesh, SetMesh)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_Skinned"), new ezRequiredAttribute()),
    EZ_RESOURCE_ACCESSOR_PROPERTY("DefaultBlendShapes", GetDefaultBlendShapes, SetDefaultBlendShapes)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_BlendShapes")),
    EZ_ACCESSOR_PROPERTY("Color", GetColor, SetColor)->AddAttributes(new ezExposeColorAlphaAttribute()),
    EZ_ACCESSOR_PROPERTY("CustomData", GetCustomData, SetCustomData)->AddAttributes(new ezDefaultValueAttribute(ezVec4(0, 1, 0, 1))),
    EZ_ARRAY_ACCESSOR_PROPERTY("Materials", Materials_GetCount, Materials_GetValue, Materials_SetValue, Materials_Insert, Materials_Remove)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Material")),
  }
  EZ_END_PROPERTIES;

  EZ_BEGIN_ATTRIBUTES
  {
      new ezCategoryAttribute("Animation"),
  }
  EZ_END_ATTRIBUTES;

  EZ_BEGIN_FUNCTIONS
  {
    EZ_SCRIPT_FUNCTION_PROPERTY(SetBlendShapeWeight, In, "Shape", In, "Weight"),
    EZ_SCRIPT_FUNCTION_PROPERTY(GetBlendShapeWeight, In, "Shape"),
    EZ_SCRIPT_FUNCTION_PROPERTY(ResetBlendShapeWeights),
  }
  EZ_END_FUNCTIONS;

  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgAnimationPoseUpdated, OnAnimationPoseUpdated),
    EZ_MESSAGE_HANDLER(ezMsgQueryAnimationSkeleton, OnQueryAnimationSkeleton),
    EZ_MESSAGE_HANDLER(ezMsgCustomInstanceDataOffsetChanged, OnMsgCustomInstanceDataOffsetChanged),
    EZ_MESSAGE_HANDLER(ezMsgAnimationCurveValue, OnMsgAnimationCurveValue),
    EZ_MESSAGE_HANDLER(ezMsgSetBlendShapeWeight, OnMsgSetBlendShapeWeight),
    EZ_MESSAGE_HANDLER(ezMsgBlendShapesPoseUpdated, OnMsgBlendShapesPoseUpdated),
    EZ_MESSAGE_HANDLER(ezMsgQueryAnimationBlendShapes, OnMsgQueryAnimationBlendShapes),
  }
  EZ_END_MESSAGEHANDLERS;
}
EZ_END_COMPONENT_TYPE

EZ_BEGIN_STATIC_REFLECTED_ENUM(ezRootMotionMode, 1)
  EZ_ENUM_CONSTANTS(ezRootMotionMode::Ignore, ezRootMotionMode::ApplyToOwner, ezRootMotionMode::SendMoveCharacterMsg)
EZ_END_STATIC_REFLECTED_ENUM;
// clang-format on

ezAnimatedMeshComponent::ezAnimatedMeshComponent() = default;
ezAnimatedMeshComponent::~ezAnimatedMeshComponent() = default;

void ezAnimatedMeshComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_hDefaultBlendShapes;
}

void ezAnimatedMeshComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  const ezUInt32 uiVersion = inout_stream.GetComponentTypeVersion(GetStaticRTTI());
  auto& s = inout_stream.GetStream();

  EZ_ASSERT_DEV(uiVersion >= 13, "Unsupported version, delete the file and reexport it");

  if (uiVersion >= 14)
  {
    s >> m_hDefaultBlendShapes;
  }
}

void ezAnimatedMeshComponent::OnActivated()
{
  SUPER::OnActivated();

  InitializeAnimationPose();
  ApplyBlendShapes();
}

void ezAnimatedMeshComponent::OnDeactivated()
{
  m_hDeformedMesh.Invalidate();
  m_hDeformedMeshBuffer.Invalidate();
  m_SkinningState.Clear();

  SUPER::OnDeactivated();
}

void ezAnimatedMeshComponent::InitializeAnimationPose()
{
  m_MaxBounds = ezBoundingBox::MakeInvalid();

  if (!m_hMesh.IsValid())
    return;

  ezResourceLock<ezMeshResource> pMesh(m_hMesh, ezResourceAcquireMode::BlockTillLoaded);
  if (pMesh.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  m_hDefaultSkeleton = pMesh->m_hDefaultSkeleton;
  if (!m_hDefaultBlendShapes.IsValid())
  {
    m_hDefaultBlendShapes = pMesh->m_hDefaultBlendShapes;
  }
  const auto hSkeleton = m_hDefaultSkeleton;

  if (!hSkeleton.IsValid())
    return;

  ezResourceLock<ezSkeletonResource> pSkeleton(hSkeleton, ezResourceAcquireMode::BlockTillLoaded);
  if (pSkeleton.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  {
    const ozz::animation::Skeleton* pOzzSkeleton = &pSkeleton->GetDescriptor().m_Skeleton.GetOzzSkeleton();
    const ezUInt32 uiNumSkeletonJoints = pOzzSkeleton->num_joints();

    ezTempArray<ozz::math::Float4x4> poseMatrices;
    poseMatrices.SetCountUninitialized(uiNumSkeletonJoints);
    EZ_ASSERT_DEBUG(ezMemoryUtils::IsAligned(poseMatrices.GetData(), alignof(ozz::math::Float4x4)), "Unaligned cast");
    {
      ozz::animation::LocalToModelJob job;
      job.input = pOzzSkeleton->joint_rest_poses();
      job.output = ozz::span<ozz::math::Float4x4>(poseMatrices.GetData(), poseMatrices.GetCount());
      job.skeleton = pOzzSkeleton;
      job.Run();
    }

    ezMsgAnimationPoseUpdated msg;
    msg.m_ModelTransforms = poseMatrices.GetArrayPtr().Cast<const ezMat4>();
    msg.m_pRootTransform = &pSkeleton->GetDescriptor().m_RootTransform;
    msg.m_pSkeleton = &pSkeleton->GetDescriptor().m_Skeleton;

    OnAnimationPoseUpdated(msg);
  }

  TriggerLocalBoundsUpdate();
}


void ezAnimatedMeshComponent::MapModelSpacePoseToSkinningSpace(const ezHashTable<ezHashedString, ezMeshResourceDescriptor::BoneData>& bones, const ezSkeleton& skeleton, ezArrayPtr<const ezMat4> modelSpaceTransforms, ezBoundingBox* bounds)
{
  auto boneTransforms = m_SkinningState.GetOrCreateBoneTransformsForWriting(*this, bones.GetCount());

  if (bounds)
  {
    for (auto itBone : bones)
    {
      const ezUInt16 uiJointIdx = skeleton.FindJointByName(itBone.Key());

      if (uiJointIdx == ezInvalidJointIndex)
        continue;

      bounds->ExpandToInclude(modelSpaceTransforms[uiJointIdx].GetTranslationVector());
      boneTransforms[itBone.Value().m_uiBoneIndex] = modelSpaceTransforms[uiJointIdx] * itBone.Value().m_GlobalInverseRestPoseMatrix;
    }
  }
  else
  {
    for (auto itBone : bones)
    {
      const ezUInt16 uiJointIdx = skeleton.FindJointByName(itBone.Key());

      if (uiJointIdx == ezInvalidJointIndex)
        continue;

      boneTransforms[itBone.Value().m_uiBoneIndex] = modelSpaceTransforms[uiJointIdx] * itBone.Value().m_GlobalInverseRestPoseMatrix;
    }
  }
}

ezTransform ezAnimatedMeshComponent::GetFinalGlobalTransform() const
{
  return GetOwner()->GetGlobalTransform() * m_RootTransform;
}

ezMeshRenderData* ezAnimatedMeshComponent::CreateRenderData(const ezRenderDataManager* pRenderDataManager) const
{
  auto pRenderData = pRenderDataManager->CreateRenderDataForThisFrame<ezSkinnedMeshRenderData>(GetOwner());

  pRenderData->m_DataOffsets.m_uiSkinning = m_SkinningState.m_DataOffset.m_uiOffset;
  pRenderData->m_hSkinningBuffer = pRenderDataManager->GetSkinningDataBuffer();

  return pRenderData;
}

const ezMeshResourceHandle& ezAnimatedMeshComponent::GetMeshToRender() const
{
  if (m_hDeformedMesh.IsValid())
    return m_hDeformedMesh;

  return m_hMesh;
}

void ezAnimatedMeshComponent::SetDefaultBlendShapes(const ezBlendShapeResourceHandle& hBlendShapes)
{
  if (m_hDefaultBlendShapes != hBlendShapes)
  {
    m_hDefaultBlendShapes = hBlendShapes;
    m_bBlendShapesDirty = true;
    ApplyBlendShapes();
  }
}

void ezAnimatedMeshComponent::SetBlendShapeWeight(ezStringView sShapeName, float fWeight)
{
  ezHashedString sName;
  sName.Assign(sShapeName);
  m_BlendShapeWeights[sName] = fWeight;
  m_bBlendShapesDirty = true;
  ApplyBlendShapes();
}

float ezAnimatedMeshComponent::GetBlendShapeWeight(ezStringView sShapeName) const
{
  float fWeight = 0.0f;
  m_BlendShapeWeights.TryGetValue(ezTempHashedString(sShapeName), fWeight);
  return fWeight;
}

void ezAnimatedMeshComponent::ResetBlendShapeWeights()
{
  m_BlendShapeWeights.Clear();
  m_bBlendShapesDirty = true;
  ApplyBlendShapes();
}

void ezAnimatedMeshComponent::OnMsgAnimationCurveValue(ezMsgAnimationCurveValue& msg)
{
  SetBlendShapeWeight(msg.m_sCurveName.GetString(), msg.m_fAverage);
}

void ezAnimatedMeshComponent::OnMsgSetBlendShapeWeight(ezMsgSetBlendShapeWeight& msg)
{
  SetBlendShapeWeight(msg.m_sShapeName.GetString(), msg.m_fWeight);
}

void ezAnimatedMeshComponent::OnMsgBlendShapesPoseUpdated(ezMsgBlendShapesPoseUpdated& msg)
{
  for (ezUInt32 i = 0; i < msg.m_Weights.GetCount(); ++i)
  {
    m_BlendShapeWeights[msg.m_Weights.GetKey(i)] = msg.m_Weights.GetValue(i);
  }
  m_bBlendShapesDirty = true;
  ApplyBlendShapes();
}

void ezAnimatedMeshComponent::OnMsgQueryAnimationBlendShapes(ezMsgQueryAnimationBlendShapes& msg)
{
  if (!msg.m_hBlendShapes.IsValid() && m_hDefaultBlendShapes.IsValid())
  {
    msg.m_hBlendShapes = m_hDefaultBlendShapes;
  }
}

void ezAnimatedMeshComponent::ApplyBlendShapes()
{
  if (!m_hMesh.IsValid())
    return;

  if (!m_hDefaultBlendShapes.IsValid())
  {
    ezResourceLock<ezMeshResource> pMesh(m_hMesh, ezResourceAcquireMode::BlockTillLoaded);
    if (pMesh.GetAcquireResult() == ezResourceAcquireResult::Final)
    {
      m_hDefaultBlendShapes = pMesh->m_hDefaultBlendShapes;
    }
  }

  if (!m_hDefaultBlendShapes.IsValid())
    return;

  ezResourceLock<ezBlendShapeResource> pBlendShapes(m_hDefaultBlendShapes, ezResourceAcquireMode::BlockTillLoaded);
  if (pBlendShapes.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  const auto& desc = pBlendShapes->GetDescriptor();

  // Optimization: check if any blend shape channel has absolute weight >= 0.001f
  const float fThreshold = 0.001f;
  bool bAnyActive = false;
  for (const auto& channel : desc.m_Channels)
  {
    float fWeight = 0.0f;
    if (m_BlendShapeWeights.TryGetValue(channel.m_sName, fWeight))
    {
      if (ezMath::Abs(fWeight) >= fThreshold)
      {
        bAnyActive = true;
        break;
      }
    }
  }

  if (!bAnyActive)
  {
    if (m_hDeformedMesh.IsValid())
    {
      m_hDeformedMesh.Invalidate();
      m_hDeformedMeshBuffer.Invalidate();
      m_LastAppliedWeights.Clear();
      InvalidateCachedRenderData();
    }
    m_bBlendShapesDirty = false;
    return;
  }

  if (!m_bBlendShapesDirty)
  {
    bool bWeightsChanged = false;
    for (const auto& channel : desc.m_Channels)
    {
      float fWeight = 0.0f;
      m_BlendShapeWeights.TryGetValue(channel.m_sName, fWeight);
      float fLastWeight = 0.0f;
      m_LastAppliedWeights.TryGetValue(channel.m_sName, fLastWeight);
      if (ezMath::Abs(fWeight - fLastWeight) >= 0.0001f)
      {
        bWeightsChanged = true;
        break;
      }
    }
    if (!bWeightsChanged)
      return;
  }

  ezResourceLock<ezMeshResource> pBaseMesh(m_hMesh, ezResourceAcquireMode::BlockTillLoaded);
  if (pBaseMesh.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  UpdateDeformedMeshBuffer(pBaseMesh.GetPointer(), desc, fThreshold);

  m_LastAppliedWeights = m_BlendShapeWeights;
  m_bBlendShapesDirty = false;
  InvalidateCachedRenderData();
}

void ezAnimatedMeshComponent::UpdateDeformedMeshBuffer(const ezMeshResource* pBaseMesh, const ezBlendShapeResourceDescriptor& blendShapeDesc, float fThreshold)
{
  if (blendShapeDesc.m_MeshBufferDesc.GetVertexCount() == 0)
    return;

  ezMeshBufferResourceDescriptor deformedMbDesc = blendShapeDesc.m_MeshBufferDesc;
  const ezUInt32 uiVertexCount = deformedMbDesc.GetVertexCount();

  for (const auto& channel : blendShapeDesc.m_Channels)
  {
    float fWeight = 0.0f;
    if (!m_BlendShapeWeights.TryGetValue(channel.m_sName, fWeight) || ezMath::Abs(fWeight) < fThreshold)
      continue;

    for (const auto& delta : channel.m_Deltas)
    {
      if (delta.m_uiVertexIndex >= uiVertexCount)
        continue;

      ezVec3 pos = deformedMbDesc.GetPosition(delta.m_uiVertexIndex);
      pos += delta.m_vPositionDelta * fWeight;
      deformedMbDesc.SetPosition(delta.m_uiVertexIndex, pos);

      if (!delta.m_vNormalDelta.IsZero())
      {
        ezVec3 normal = deformedMbDesc.GetNormal(delta.m_uiVertexIndex);
        normal += delta.m_vNormalDelta * fWeight;
        normal.NormalizeIfNotZero(ezVec3(0, 0, 1)).IgnoreResult();
        deformedMbDesc.SetNormal(delta.m_uiVertexIndex, normal);
      }
    }
  }

  ezStringBuilder sDeformedMbName, sDeformedMeshName;
  sDeformedMbName.SetFormat("{}_BlendShape_MB_{}", pBaseMesh->GetResourceID(), GetOwner()->GetHandle().GetInternalID().m_Data);
  sDeformedMeshName.SetFormat("{}_BlendShape_Mesh_{}", pBaseMesh->GetResourceID(), GetOwner()->GetHandle().GetInternalID().m_Data);

  m_hDeformedMeshBuffer = ezResourceManager::GetOrCreateResource<ezMeshBufferResource>(sDeformedMbName, std::move(deformedMbDesc));

  ezMeshResourceDescriptor meshDesc;
  meshDesc.UseExistingMeshBuffer(m_hDeformedMeshBuffer);

  for (ezUInt32 i = 0; i < pBaseMesh->GetSubMeshes().GetCount(); ++i)
  {
    const auto& sm = pBaseMesh->GetSubMeshes()[i];
    meshDesc.AddSubMesh(sm.m_uiPrimitiveCount, sm.m_uiFirstPrimitive, sm.m_uiMaterialIndex);
  }

  for (ezUInt32 i = 0; i < pBaseMesh->GetMaterials().GetCount(); ++i)
  {
    meshDesc.SetMaterial(i, pBaseMesh->GetMaterials()[i].GetResourceID());
  }

  meshDesc.m_Bones = pBaseMesh->m_Bones;
  meshDesc.m_hDefaultSkeleton = pBaseMesh->m_hDefaultSkeleton;
  meshDesc.m_hDefaultBlendShapes = pBaseMesh->m_hDefaultBlendShapes;
  meshDesc.m_fMaxBoneVertexOffset = pBaseMesh->m_fMaxBoneVertexOffset;
  meshDesc.ComputeBounds();

  m_hDeformedMesh = ezResourceManager::GetOrCreateResource<ezMeshResource>(sDeformedMeshName, std::move(meshDesc));
}

void ezAnimatedMeshComponent::RetrievePose(ezDynamicArray<ezMat4>& out_modelTransforms, ezTransform& out_rootTransform, const ezSkeleton& skeleton)
{
  out_modelTransforms.Clear();

  if (!m_hMesh.IsValid())
    return;

  out_rootTransform = m_RootTransform;

  ezResourceLock<ezMeshResource> pMesh(m_hMesh, ezResourceAcquireMode::BlockTillLoaded);

  const ezHashTable<ezHashedString, ezMeshResourceDescriptor::BoneData>& bones = pMesh->m_Bones;
  auto boneTransforms = m_SkinningState.GetBoneTransformsForReading();

  out_modelTransforms.SetCount(skeleton.GetJointCount(), ezMat4::MakeIdentity());

  for (auto itBone : bones)
  {
    const ezUInt16 uiJointIdx = skeleton.FindJointByName(itBone.Key());

    if (uiJointIdx == ezInvalidJointIndex)
      continue;

    out_modelTransforms[uiJointIdx] = boneTransforms[itBone.Value().m_uiBoneIndex].GetAsMat4() * itBone.Value().m_GlobalInverseRestPoseMatrix.GetInverse();
  }
}

void ezAnimatedMeshComponent::OnAnimationPoseUpdated(ezMsgAnimationPoseUpdated& msg)
{
  if (!m_hMesh.IsValid())
    return;

  m_RootTransform = *msg.m_pRootTransform;

  ezResourceLock<ezMeshResource> pMesh(m_hMesh, ezResourceAcquireMode::BlockTillLoaded);

  ezBoundingBox poseBounds;
  poseBounds = ezBoundingBox::MakeInvalid();
  MapModelSpacePoseToSkinningSpace(pMesh->m_Bones, *msg.m_pSkeleton, msg.m_ModelTransforms, &poseBounds);

  if (poseBounds.IsValid() && (!m_MaxBounds.IsValid() || !m_MaxBounds.Contains(poseBounds)))
  {
    m_MaxBounds.ExpandToInclude(poseBounds);
    QueueLocalBoundsUpdate();
  }
  else if (((ezRenderWorld::GetFrameCounter() + GetUniqueIdForRendering()) & (EZ_BIT(10) - 1)) == 0) // reset the bbox every once in a while
  {
    m_MaxBounds = poseBounds;
    QueueLocalBoundsUpdate();
  }
}

void ezAnimatedMeshComponent::OnQueryAnimationSkeleton(ezMsgQueryAnimationSkeleton& msg)
{
  if (!msg.m_hSkeleton.IsValid() && m_hMesh.IsValid())
  {
    // only overwrite, if no one else had a better skeleton (e.g. the ezSkeletonComponent)

    ezResourceLock<ezMeshResource> pMesh(m_hMesh, ezResourceAcquireMode::BlockTillLoaded);
    if (pMesh.GetAcquireResult() == ezResourceAcquireResult::Final)
    {
      msg.m_hSkeleton = pMesh->m_hDefaultSkeleton;
    }
  }
}

void ezAnimatedMeshComponent::OnMsgCustomInstanceDataOffsetChanged(ezMsgCustomInstanceDataOffsetChanged& msg)
{
  m_SkinningState.m_DataOffset = msg.m_NewOffset;

  InvalidateCachedRenderData();
}

ezResult ezAnimatedMeshComponent::GetLocalBounds(ezBoundingBoxSphere& bounds, bool& bAlwaysVisible, ezMsgUpdateLocalBounds& msg)
{
  if (!m_MaxBounds.IsValid() || !m_hMesh.IsValid())
    return EZ_FAILURE;

  ezResourceLock<ezMeshResource> pMesh(m_hMesh, ezResourceAcquireMode::BlockTillLoaded);
  if (pMesh.GetAcquireResult() != ezResourceAcquireResult::Final)
    return EZ_FAILURE;

  ezBoundingBox bbox = m_MaxBounds;
  bbox.Grow(ezVec3(pMesh->m_fMaxBoneVertexOffset));
  bounds = ezBoundingBoxSphere::MakeFromBox(bbox);
  bounds.Transform(m_RootTransform.GetAsMat4());
  return EZ_SUCCESS;
}

void ezRootMotionMode::Apply(ezRootMotionMode::Enum mode, ezGameObject* pObject, const ezVec3& vTranslation, ezAngle rotationX, ezAngle rotationY, ezAngle rotationZ)
{
  switch (mode)
  {
    case ezRootMotionMode::Ignore:
      return;

    case ezRootMotionMode::ApplyToOwner:
    {
      ezVec3 vNewPos = pObject->GetLocalPosition();
      vNewPos += pObject->GetLocalRotation() * vTranslation;
      pObject->SetLocalPosition(vNewPos);

      // not tested whether this is actually correct
      ezQuat rotation = ezQuat::MakeFromEulerAngles(rotationX, rotationY, rotationZ);

      pObject->SetLocalRotation(rotation * pObject->GetLocalRotation());

      return;
    }

    case ezRootMotionMode::SendMoveCharacterMsg:
    {
      ezMsgApplyRootMotion msg;
      msg.m_vTranslation = vTranslation;
      msg.m_RotationX = rotationX;
      msg.m_RotationY = rotationY;
      msg.m_RotationZ = rotationZ;

      while (pObject != nullptr)
      {
        pObject->SendMessage(msg);
        pObject = pObject->GetParent();
      }

      return;
    }
  }
}

//////////////////////////////////////////////////////////////////////////


ezAnimatedMeshComponentManager::ezAnimatedMeshComponentManager(ezWorld* pWorld)
  : ezComponentManager<ComponentType, ezBlockStorageType::FreeList>(pWorld)
{
  ezResourceManager::GetResourceEvents().AddEventHandler(ezMakeDelegate(&ezAnimatedMeshComponentManager::ResourceEventHandler, this));
}

ezAnimatedMeshComponentManager::~ezAnimatedMeshComponentManager()
{
  ezResourceManager::GetResourceEvents().RemoveEventHandler(ezMakeDelegate(&ezAnimatedMeshComponentManager::ResourceEventHandler, this));
}

void ezAnimatedMeshComponentManager::Initialize()
{
  auto desc = EZ_CREATE_MODULE_UPDATE_FUNCTION_DESC(ezAnimatedMeshComponentManager::Update, this);

  RegisterUpdateFunction(desc);
}

void ezAnimatedMeshComponentManager::ResourceEventHandler(const ezResourceEvent& e)
{
  if (e.m_Type == ezResourceEvent::Type::ResourceContentUnloading)
  {
    if (ezMeshResource* pResource = ezDynamicCast<ezMeshResource*>(e.m_pResource))
    {
      ezMeshResourceHandle hMesh(pResource);

      for (auto it = GetComponents(); it.IsValid(); it.Next())
      {
        if (it->m_hMesh == hMesh)
        {
          AddToUpdateList(it);
        }
      }
    }

    if (ezSkeletonResource* pResource = ezDynamicCast<ezSkeletonResource*>(e.m_pResource))
    {
      ezSkeletonResourceHandle hSkeleton(pResource);

      for (auto it = GetComponents(); it.IsValid(); it.Next())
      {
        if (it->m_hDefaultSkeleton == hSkeleton)
        {
          AddToUpdateList(it);
        }
      }
    }

    if (ezBlendShapeResource* pResource = ezDynamicCast<ezBlendShapeResource*>(e.m_pResource))
    {
      ezBlendShapeResourceHandle hBlendShape(pResource);

      for (auto it = GetComponents(); it.IsValid(); it.Next())
      {
        if (it->m_hDefaultBlendShapes == hBlendShape)
        {
          AddToUpdateList(it);
        }
      }
    }
  }
}

void ezAnimatedMeshComponentManager::Update(const ezWorldModule::UpdateContext& context)
{
  for (auto hComp : m_ComponentsToUpdate)
  {
    ezAnimatedMeshComponent* pComponent = nullptr;
    if (!TryGetComponent(hComp, pComponent))
      continue;

    if (!pComponent->IsActive())
      continue;

    pComponent->InitializeAnimationPose();
  }

  m_ComponentsToUpdate.Clear();
}

void ezAnimatedMeshComponentManager::AddToUpdateList(ezAnimatedMeshComponent* pComponent)
{
  ezComponentHandle hComponent = pComponent->GetHandle();

  if (m_ComponentsToUpdate.IndexOf(hComponent) == ezInvalidIndex)
  {
    m_ComponentsToUpdate.PushBack(hComponent);
  }
}

EZ_STATICLINK_FILE(GameEngine, GameEngine_Animation_Skeletal_Implementation_AnimatedMeshComponent);
