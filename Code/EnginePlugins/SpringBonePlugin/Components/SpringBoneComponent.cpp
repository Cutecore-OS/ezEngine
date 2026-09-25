#include <SpringBonePlugin/SpringBonePluginPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/Interfaces/WindWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Logging/Log.h>
#include <Foundation/Serialization/AbstractObjectGraph.h>
#include <Foundation/Serialization/GraphPatch.h>
#include <GameEngine/Effects/Wind/SimpleWindWorldModule.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <JoltPlugin/Resources/JoltMaterial.h>
#include <JoltPlugin/System/JoltCore.h>
#include <JoltPlugin/System/JoltWorldModule.h>
#include <JoltPlugin/Utilities/JoltConversionUtils.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <SpringBonePlugin/Components/SpringBoneComponent.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/skeleton.h>

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_TYPE(ezSpringBoneSettings, ezNoBase, 2, ezRTTIDefaultAllocator<ezSpringBoneSettings>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Mass", m_fMass)->AddAttributes(new ezSuffixAttribute(" kg"), new ezGroupAttribute("Spring and Return"), new ezDefaultValueAttribute(0.1f), new ezClampValueAttribute(0.001f, 100.0f)),
    EZ_MEMBER_PROPERTY("Stiffness", m_fStiffness)->AddAttributes(new ezSuffixAttribute(" Hz"), new ezGroupAttribute("Spring and Return"), new ezDefaultValueAttribute(4.0f), new ezClampValueAttribute(0.0f, 60.0f)),
    EZ_MEMBER_PROPERTY("Damping", m_fDamping)->AddAttributes(new ezGroupAttribute("Spring and Return"), new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 2.0f)),
    EZ_MEMBER_PROPERTY("RootStiffness", m_fRootStiffness)->AddAttributes(new ezSuffixAttribute(" Hz"), new ezGroupAttribute("Spring and Return"), new ezDefaultValueAttribute(20.0f), new ezClampValueAttribute(0.0f, 60.0f)),
    EZ_MEMBER_PROPERTY("LengthStiffness", m_fLengthStiffness)->AddAttributes(new ezSuffixAttribute(" Hz"), new ezGroupAttribute("Spring and Return"), new ezDefaultValueAttribute(20.0f), new ezClampValueAttribute(0.0f, 60.0f)),
    EZ_MEMBER_PROPERTY("Softening", m_fSoftening)->AddAttributes(new ezGroupAttribute("Spring and Return"), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("GravityFactor", m_fGravityFactor)->AddAttributes(new ezGroupAttribute("Gravity and Resistance"), new ezDefaultValueAttribute(0.25f), new ezClampValueAttribute(-10.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("AirDrag", m_fAirDrag)->AddAttributes(new ezSuffixAttribute(" 1/s"), new ezGroupAttribute("Gravity and Resistance"), new ezDefaultValueAttribute(0.2f), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("Friction", m_fFriction)->AddAttributes(new ezGroupAttribute("Gravity and Resistance"), new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("Influence", m_fInfluence)->AddAttributes(new ezGroupAttribute("Spring and Return"), new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("WindInfluence", m_fWindInfluence)->AddAttributes(new ezGroupAttribute("Wind"), new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("WindFlutter", m_fWindFlutter)->AddAttributes(new ezGroupAttribute("Wind"), new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("MaxDistance", m_fMaxDistance)->AddAttributes(new ezSuffixAttribute(" m"), new ezGroupAttribute("Limits"), new ezClampValueAttribute(0.0f, 10.0f)),
    EZ_MEMBER_PROPERTY("MaxAngle", m_MaxAngle)->AddAttributes(new ezGroupAttribute("Limits"), new ezDefaultValueAttribute(ezAngle::MakeFromDegree(60)), new ezClampValueAttribute(ezAngle::MakeFromDegree(0), ezAngle::MakeFromDegree(179))),
    EZ_MEMBER_PROPERTY("LockRotationX", m_bLockRotationX)->AddAttributes(new ezGroupAttribute("Limits")),
    EZ_MEMBER_PROPERTY("LockRotationY", m_bLockRotationY)->AddAttributes(new ezGroupAttribute("Limits")),
    EZ_MEMBER_PROPERTY("LockRotationZ", m_bLockRotationZ)->AddAttributes(new ezGroupAttribute("Limits")),
    EZ_MEMBER_PROPERTY("MaxMotorForce", m_fMaxMotorForce)->AddAttributes(new ezSuffixAttribute(" N / Nm"), new ezGroupAttribute("Spring and Return"), new ezDefaultValueAttribute(100.0f), new ezClampValueAttribute(0.0f, 100000.0f)),
    EZ_MEMBER_PROPERTY("ExternalAcceleration", m_vExternalAcceleration)->AddAttributes(new ezSuffixAttribute(" m/s^2"), new ezGroupAttribute("Gravity and Resistance")),
    EZ_MEMBER_PROPERTY("SwayAcceleration", m_vSwayAcceleration)->AddAttributes(new ezSuffixAttribute(" m/s^2"), new ezGroupAttribute("Self Motion")),
    EZ_MEMBER_PROPERTY("SwayFrequency", m_fSwayFrequency)->AddAttributes(new ezSuffixAttribute(" Hz"), new ezGroupAttribute("Self Motion"), new ezDefaultValueAttribute(0.5f), new ezClampValueAttribute(0.0f, 20.0f)),
    EZ_MEMBER_PROPERTY("SwayPhase", m_fSwayPhase)->AddAttributes(new ezSuffixAttribute(" rad"), new ezGroupAttribute("Self Motion")),
    EZ_MEMBER_PROPERTY("SwayAngles", m_vSwayAngles)->AddAttributes(new ezGroupAttribute("Self Motion"), new ezSuffixAttribute(" deg")),
    EZ_MEMBER_PROPERTY("SwayAngularAcceleration", m_vSwayAngularAcceleration)->AddAttributes(new ezGroupAttribute("Self Motion"), new ezSuffixAttribute(" deg/s^2")),
    EZ_MEMBER_PROPERTY("SwayFrequencyScale", m_vSwayFrequencyScale)->AddAttributes(new ezGroupAttribute("Self Motion"), new ezDefaultValueAttribute(ezVec3(1.0f, 0.83f, 1.17f)), new ezClampValueAttribute(ezVec3::MakeZero(), ezVec3(20))),
    EZ_MEMBER_PROPERTY("SwayAxisPhase", m_vSwayAxisPhase)->AddAttributes(new ezGroupAttribute("Self Motion"), new ezDefaultValueAttribute(ezVec3(0, 1.3f, 2.1f)), new ezSuffixAttribute(" rad")),
    EZ_MEMBER_PROPERTY("SwayBonePhase", m_fSwayBonePhase)->AddAttributes(new ezGroupAttribute("Self Motion"), new ezDefaultValueAttribute(0.73f), new ezSuffixAttribute(" rad")),
    EZ_MEMBER_PROPERTY("ForceApplicationOffset", m_vForceApplicationOffset)->AddAttributes(new ezGroupAttribute("Gravity and Resistance"), new ezSuffixAttribute(" m")),
  }
  EZ_END_PROPERTIES;
}
EZ_END_STATIC_REFLECTED_TYPE;

EZ_BEGIN_STATIC_REFLECTED_TYPE(ezSpringBoneOverride, ezNoBase, 1, ezRTTIDefaultAllocator<ezSpringBoneOverride>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Bone", m_sBone)->AddAttributes(new ezDynamicStringEnumAttribute("SpringBoneNames")),
    EZ_MEMBER_PROPERTY("Enabled", m_bEnabled)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_MEMBER_PROPERTY("Settings", m_Settings),
  }
  EZ_END_PROPERTIES;
}
EZ_END_STATIC_REFLECTED_TYPE;

EZ_BEGIN_COMPONENT_TYPE(ezSpringBoneComponent, 2, ezComponentMode::Dynamic)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_ACCESSOR_PROPERTY("Skeleton", GetSkeleton, SetSkeleton)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_Skeleton")),
    EZ_ACCESSOR_PROPERTY("RootBone", GetRootBone, SetRootBone)->AddAttributes(new ezDynamicStringEnumAttribute("SpringBoneNames")),
    EZ_ACCESSOR_PROPERTY("IncludeDescendants", GetIncludeDescendants, SetIncludeDescendants)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_MEMBER_PROPERTY("Settings", m_Settings),
    EZ_ARRAY_MEMBER_PROPERTY("BoneOverrides", m_BoneOverrides),
    EZ_MEMBER_PROPERTY("SelfCollision", m_bSelfCollision)->AddAttributes(new ezGroupAttribute("Collisions")),
    EZ_MEMBER_PROPERTY("CollideWithSkeleton", m_bCollideWithSkeleton)->AddAttributes(new ezGroupAttribute("Collisions"), new ezDefaultValueAttribute(true)),
    EZ_MEMBER_PROPERTY("TeleportDistance", m_fTeleportDistance)->AddAttributes(new ezSuffixAttribute(" m"), new ezDefaultValueAttribute(2.0f), new ezClampValueAttribute(0.01f, 1000.0f)),
    EZ_MEMBER_PROPERTY("TeleportAngle", m_TeleportAngle)->AddAttributes(new ezDefaultValueAttribute(ezAngle::MakeFromDegree(90)), new ezClampValueAttribute(ezAngle::MakeFromDegree(1), ezAngle::MakeFromDegree(180))),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgAnimationPoseUpdated, OnAnimationPoseUpdated),
    EZ_MESSAGE_HANDLER(ezMsgPhysicsAddImpulse, OnPhysicsAddImpulse),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_FUNCTIONS
  {
    EZ_SCRIPT_FUNCTION_PROPERTY(ResetSimulation),
  }
  EZ_END_FUNCTIONS;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Animation"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE;
// clang-format on

void ezSpringBoneSettings::Serialize(ezStreamWriter& stream) const
{
  stream << m_fMass << m_fStiffness << m_fDamping << m_fRootStiffness << m_fLengthStiffness << m_fSoftening;
  stream << m_fGravityFactor << m_fAirDrag << m_fFriction << m_fInfluence << m_fWindInfluence << m_fWindFlutter;
  stream << m_fMaxDistance << m_MaxAngle << m_fMaxMotorForce << m_vExternalAcceleration << m_vSwayAcceleration << m_fSwayFrequency << m_fSwayPhase;
  stream << m_bLockRotationX << m_bLockRotationY << m_bLockRotationZ;
  stream << m_vSwayAngles << m_vSwayAngularAcceleration << m_vSwayFrequencyScale << m_vSwayAxisPhase << m_fSwayBonePhase << m_vForceApplicationOffset;
}

void ezSpringBoneSettings::Deserialize(ezStreamReader& stream, ezUInt32 uiVersion)
{
  stream >> m_fMass >> m_fStiffness >> m_fDamping >> m_fRootStiffness >> m_fLengthStiffness >> m_fSoftening;
  stream >> m_fGravityFactor >> m_fAirDrag >> m_fFriction >> m_fInfluence >> m_fWindInfluence >> m_fWindFlutter;
  stream >> m_fMaxDistance >> m_MaxAngle >> m_fMaxMotorForce >> m_vExternalAcceleration >> m_vSwayAcceleration >> m_fSwayFrequency >> m_fSwayPhase;
  stream >> m_bLockRotationX >> m_bLockRotationY >> m_bLockRotationZ;
  m_vSwayAngles.SetZero();
  m_vSwayAngularAcceleration.SetZero();
  m_vSwayFrequencyScale = ezVec3(1.0f, 0.83f, 1.17f);
  m_vSwayAxisPhase = ezVec3(0.0f, 1.3f, 2.1f);
  m_fSwayBonePhase = 0.73f;
  m_vForceApplicationOffset.SetZero();
  if (uiVersion >= 2)
    stream >> m_vSwayAngles >> m_vSwayAngularAcceleration >> m_vSwayFrequencyScale >> m_vSwayAxisPhase >> m_fSwayBonePhase >> m_vForceApplicationOffset;
}

bool ezSpringBoneSettings::operator==(const ezSpringBoneSettings& rhs) const
{
  return m_fMass == rhs.m_fMass &&
         m_fStiffness == rhs.m_fStiffness &&
         m_fDamping == rhs.m_fDamping &&
         m_fRootStiffness == rhs.m_fRootStiffness &&
         m_fLengthStiffness == rhs.m_fLengthStiffness &&
         m_fSoftening == rhs.m_fSoftening &&
         m_fGravityFactor == rhs.m_fGravityFactor &&
         m_fAirDrag == rhs.m_fAirDrag &&
         m_fFriction == rhs.m_fFriction &&
         m_fInfluence == rhs.m_fInfluence &&
         m_fWindInfluence == rhs.m_fWindInfluence &&
         m_fWindFlutter == rhs.m_fWindFlutter &&
         m_fMaxDistance == rhs.m_fMaxDistance &&
         m_MaxAngle == rhs.m_MaxAngle &&
         m_bLockRotationX == rhs.m_bLockRotationX &&
         m_bLockRotationY == rhs.m_bLockRotationY &&
         m_bLockRotationZ == rhs.m_bLockRotationZ &&
         m_fMaxMotorForce == rhs.m_fMaxMotorForce &&
         m_vExternalAcceleration == rhs.m_vExternalAcceleration &&
         m_vSwayAcceleration == rhs.m_vSwayAcceleration &&
         m_fSwayFrequency == rhs.m_fSwayFrequency &&
         m_fSwayPhase == rhs.m_fSwayPhase &&
         m_vSwayAngles == rhs.m_vSwayAngles &&
         m_vSwayAngularAcceleration == rhs.m_vSwayAngularAcceleration &&
         m_vSwayFrequencyScale == rhs.m_vSwayFrequencyScale &&
         m_vSwayAxisPhase == rhs.m_vSwayAxisPhase &&
         m_fSwayBonePhase == rhs.m_fSwayBonePhase &&
         m_vForceApplicationOffset == rhs.m_vForceApplicationOffset;
}

struct ezSpringBoneComponent::Simulation
{
  struct Bone
  {
    JPH::BodyID m_Body;
    JPH::Ref<JPH::SixDOFConstraint> m_pConstraint;
    ezSpringBoneSettings m_Settings;
    ezUInt16 m_uiParent = ezInvalidJointIndex;
    bool m_bDynamic = false;
    ezVec3 m_vAnchor = ezVec3::MakeZero();
    ezQuat m_qFrame = ezQuat::MakeIdentity();
    ezVec3d m_vSwayPhase = ezVec3d::MakeZero();
    JPH::Mat44 m_LocalInertia = JPH::Mat44::sIdentity();
    ezVec3 m_vWindOffset = ezVec3::MakeZero(); ///< Center of pressure relative to COM, already scaled.
  };

  ezDynamicArray<Bone> m_Bones;
  ezDynamicArray<ezMat4> m_OutputPose;
  JPH::BodyID m_Anchor;
  JPH::Ref<JPH::GroupFilterTable> m_pFilter;
  bool m_bSelfCollision = false;
  bool m_bCollideWithSkeleton = true;
  ezUInt32 m_uiFilterID = ezInvalidIndex;
  ezUInt32 m_uiResourceChangeCounter = 0;
  ezUInt64 m_uiLastForceUpdate = ezMath::MaxValue<ezUInt64>();
  ezTransform m_LastOwnerTransform = ezTransform::MakeIdentity();
};

namespace
{
  // Matches Skeleton Asset / Jolt Ragdoll primitive conventions: X is length, Z is radius.
  JPH::RefConst<JPH::Shape> CreateBoneShape(const ezSkeletonResourceDescriptor& desc, ezUInt16 uiJoint, float fScale, const ezMat4& mRestPose)
  {
    JPH::StaticCompoundShapeSettings compound;
    const ezJoltMaterial* material = ezJoltCore::GetDefaultMaterial();
    const auto surfaceHandle = desc.m_Skeleton.GetJointByIndex(uiJoint).GetSurface();
    if (surfaceHandle.IsValid())
    {
      ezResourceLock<ezSurfaceResource> surface(surfaceHandle, ezResourceAcquireMode::BlockTillLoaded);
      if (surface->m_pPhysicsMaterialJolt)
        material = static_cast<const ezJoltMaterial*>(surface->m_pPhysicsMaterialJolt);
    }
    const ezQuat qDirection = ezBasisAxis::GetBasisRotation(ezBasisAxis::PositiveX, desc.m_Skeleton.m_BoneDirection);
    for (const auto& geo : desc.m_Geometry)
    {
      if (geo.m_uiAttachedToJoint != uiJoint || geo.m_Type == ezSkeletonJointGeometryType::None)
        continue;

      ezVec3 pos = qDirection * geo.m_Transform.m_vPosition * fScale;
      ezQuat rot = qDirection * geo.m_Transform.m_qRotation;
      const ezVec3 size = geo.m_Transform.m_vScale.Abs() * fScale;
      JPH::ShapeSettings::ShapeResult result;
      switch (geo.m_Type)
      {
        case ezSkeletonJointGeometryType::Sphere:
          result = JPH::SphereShapeSettings(ezMath::Max(size.z, 0.001f), material).Create();
          break;
        case ezSkeletonJointGeometryType::Box:
          result = JPH::BoxShapeSettings(ezJoltConversionUtils::ToVec3((size * 0.5f).CompMax(ezVec3(0.001f))), 0.0f, material).Create();
          pos += qDirection * ezVec3(size.x * 0.5f, 0, 0);
          break;
        case ezSkeletonJointGeometryType::Capsule:
        case ezSkeletonJointGeometryType::CapsuleSideways:
          if (size.x > 0.0001f)
            result = JPH::CapsuleShapeSettings(size.x * 0.5f, ezMath::Max(size.z, 0.001f), material).Create();
          else
            result = JPH::SphereShapeSettings(ezMath::Max(size.z, 0.001f), material).Create();
          if (geo.m_Type == ezSkeletonJointGeometryType::Capsule)
          {
            rot = rot * ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisZ(), ezAngle::MakeFromDegree(-90));
            pos += qDirection * ezVec3(size.x * 0.5f, 0, 0);
          }
          break;
        case ezSkeletonJointGeometryType::ConvexMesh:
        {
          // Imported vertices are in mesh space, independent of the current animation pose.
          ezDynamicArray<JPH::Vec3> vertices;
          const ezMat4 inverseRest = mRestPose.GetInverse();
          for (const auto& v : geo.m_VertexPositions)
            vertices.PushBack(ezJoltConversionUtils::ToVec3(inverseRest.TransformPosition(v) * fScale));
          result = JPH::ConvexHullShapeSettings(vertices.GetData(), vertices.GetCount(), 0.0f, material).Create();
          pos.SetZero();
          rot.SetIdentity();
          break;
        }
        default:
          continue;
      }
      if (result.HasError())
      {
        ezLog::Warning("Spring Bone: invalid Bone Shape on '{}': {}", desc.m_Skeleton.GetJointByIndex(uiJoint).GetName(), result.GetError().c_str());
        continue;
      }
      compound.AddShape(ezJoltConversionUtils::ToVec3(pos), ezJoltConversionUtils::ToQuat(rot), result.Get());
    }

    if (compound.mSubShapes.empty())
      return new JPH::EmptyShape();
    const auto result = compound.Create();
    if (result.HasError())
    {
      ezLog::Warning("Spring Bone: failed to create compound shape: {}", result.GetError().c_str());
      return new JPH::EmptyShape();
    }
    return result.Get();
  }

  ezTransform GetBoneWorldTransform(const ezTransform& owner, const ezTransform& root, const ezMat4& model)
  {
    ezTransform result;
    ezMat4 full;
    ezMsgAnimationPoseUpdated::ComputeFullBoneTransform(owner.GetAsMat4() * root.GetAsMat4(), model, full, result.m_qRotation);
    result.m_vPosition = full.GetTranslationVector();
    result.m_vScale.Set(1.0f);
    return result;
  }
} // namespace

ezSpringBoneComponent::ezSpringBoneComponent() = default;
ezSpringBoneComponent::~ezSpringBoneComponent() = default;

void ezSpringBoneComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s << m_hSkeleton;
  s << m_sRootBone << m_bIncludeDescendants;
  m_Settings.Serialize(s);
  s << m_BoneOverrides.GetCount();
  for (const auto& bone : m_BoneOverrides)
  {
    s << bone.m_sBone << bone.m_bEnabled;
    bone.m_Settings.Serialize(s);
  }
  s << m_bSelfCollision << m_bCollideWithSkeleton << m_fTeleportDistance << m_TeleportAngle;
}

void ezSpringBoneComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s >> m_hSkeleton;
  s >> m_sRootBone >> m_bIncludeDescendants;
  const ezUInt32 version = inout_stream.GetComponentTypeVersion(GetStaticRTTI());
  if (version == 1)
  {
    ezUInt8 oldPreset;
    s >> oldPreset; // Preserve the serialized numbers, not the old profile selection.
  }
  m_Settings.Deserialize(s, version);
  ezUInt32 count;
  s >> count;
  m_BoneOverrides.SetCount(count);
  for (auto& bone : m_BoneOverrides)
  {
    s >> bone.m_sBone >> bone.m_bEnabled;
    bone.m_Settings.Deserialize(s, version);
  }
  s >> m_bSelfCollision >> m_bCollideWithSkeleton >> m_fTeleportDistance >> m_TeleportAngle;
}

void ezSpringBoneComponent::SetSkeleton(const ezSkeletonResourceHandle& hSkeleton)
{
  if (m_hSkeleton == hSkeleton)
    return;
  m_hSkeleton = hSkeleton;
  m_AnimationPose.Clear();
  ResetSimulation();
}

void ezSpringBoneComponent::SetRootBone(ezStringView sBone)
{
  m_sRootBone = sBone;
  ResetSimulation();
}

void ezSpringBoneComponent::SetIncludeDescendants(bool bInclude)
{
  m_bIncludeDescendants = bInclude;
  ResetSimulation();
}

void ezSpringBoneComponent::ResetSimulation()
{
  m_bReset = true;
}

void ezSpringBoneComponent::OnActivated()
{
  SUPER::OnActivated();
  ResetSimulation();
}

ezArrayPtr<const ezMat4> ezSpringBoneComponent::GetSimulatedPose() const
{
  if (m_pSimulation)
    return m_pSimulation->m_OutputPose;
  return {};
}

void ezSpringBoneComponent::OnSimulationStarted()
{
  SUPER::OnSimulationStarted();
  m_pJolt = GetWorld()->GetOrCreateModule<ezJoltWorldModule>();
  // Wind volumes do not create a wind module themselves. Support volume-only worlds,
  // while keeping an already installed custom wind implementation.
  if (GetWorld()->GetModuleReadOnly<ezWindWorldModuleInterface>() == nullptr)
    GetWorld()->GetOrCreateModule<ezSimpleWindWorldModule>();
  ResetSimulation();
}

void ezSpringBoneComponent::OnDeactivated()
{
  DestroySimulation();
  m_AnimationPose.Clear();
  SUPER::OnDeactivated();
}

void ezSpringBoneComponent::OnAnimationPoseUpdated(ezMsgAnimationPoseUpdated& ref_msg)
{
  if (m_bSendingPose || !GetWorld()->GetWorldSimulationEnabled() || ref_msg.m_pSkeleton == nullptr || ref_msg.m_pRootTransform == nullptr)
    return;
  // Do not write through the message's const view or stop the animation controller.
  if (m_hSkeleton.IsValid())
  {
    ezResourceLock<ezSkeletonResource> skeleton(m_hSkeleton, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
    if (skeleton.GetAcquireResult() != ezResourceAcquireResult::Final || &skeleton->GetDescriptor().m_Skeleton != ref_msg.m_pSkeleton)
      return;
  }
  m_AnimationPose = ref_msg.m_ModelTransforms;
  m_RootTransform = *ref_msg.m_pRootTransform;
}

bool ezSpringBoneComponent::EnsurePose()
{
  if (!m_hSkeleton.IsValid())
  {
    ezMsgQueryAnimationSkeleton query;
    GetOwner()->SendMessage(query);
    m_hSkeleton = query.m_hSkeleton;
  }
  if (!m_hSkeleton.IsValid())
    return false;
  ezResourceLock<ezSkeletonResource> resource(m_hSkeleton, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (resource.GetAcquireResult() != ezResourceAcquireResult::Final)
    return false;
  const auto& desc = resource->GetDescriptor();
  if (desc.m_Skeleton.GetJointCount() == 0)
    return false;
  if (m_pSimulation && m_pSimulation->m_uiResourceChangeCounter != resource->GetCurrentResourceChangeCounter())
  {
    m_bReset = true;
    m_AnimationPose.Clear();
  }
  if (m_AnimationPose.GetCount() != desc.m_Skeleton.GetJointCount())
  {
    ezDynamicArray<ozz::math::Float4x4, ezAlignedAllocatorWrapper> pose;
    pose.SetCountUninitialized(desc.m_Skeleton.GetJointCount());
    ozz::animation::LocalToModelJob job;
    job.skeleton = &desc.m_Skeleton.GetOzzSkeleton();
    job.input = job.skeleton->joint_rest_poses();
    job.output = ozz::span<ozz::math::Float4x4>(pose.GetData(), pose.GetCount());
    if (!job.Run())
      return false;
    m_AnimationPose = pose.GetArrayPtr().Cast<const ezMat4>();
    m_RootTransform = desc.m_RootTransform;
  }
  return true;
}

bool ezSpringBoneComponent::CreateSimulation()
{
  for (const auto* component : GetOwner()->GetComponents())
  {
    if (component == this)
      break;
    if (component->IsInstanceOf<ezSpringBoneComponent>() && component->IsActive())
    {
      ezLog::Warning("Only one Spring Bone Component may simulate a skeleton. Use BoneOverrides for additional chains.");
      return false;
    }
  }
  ezResourceLock<ezSkeletonResource> resource(m_hSkeleton, ezResourceAcquireMode::BlockTillLoaded);
  const auto& desc = resource->GetDescriptor();
  const auto& skeleton = desc.m_Skeleton;
  const ezUInt16 root = skeleton.FindJointByName(ezTempHashedString(m_sRootBone));
  if (root == ezInvalidJointIndex)
  {
    ezLog::Warning("Spring Bone: root bone '{}' was not found in the skeleton.", m_sRootBone);
    return false;
  }

  const ezTransform owner = GetOwner()->GetGlobalTransform();
  if (owner.m_vScale.x <= 0 || !owner.m_vScale.IsEqual(ezVec3(owner.m_vScale.x), 0.001f))
  {
    ezLog::Warning("Spring Bone requires positive uniform owner scale.");
    return false;
  }
  const float scale = owner.m_vScale.x;
  m_pSimulation = EZ_DEFAULT_NEW(Simulation);
  auto& sim = *m_pSimulation;
  sim.m_uiFilterID = m_pJolt->CreateObjectFilterID();
  sim.m_uiResourceChangeCounter = resource->GetCurrentResourceChangeCounter();
  sim.m_LastOwnerTransform = owner;
  sim.m_Bones.SetCount(skeleton.GetJointCount());
  sim.m_OutputPose = m_AnimationPose;

  ezDynamicArray<ezMat4> restPose;
  restPose.SetCount(skeleton.GetJointCount());
  for (ezUInt16 i = 0; i < skeleton.GetJointCount(); ++i)
  {
    const auto& joint = skeleton.GetJointByIndex(i);
    const auto parent = joint.GetParentIndex();
    restPose[i] = parent == ezInvalidJointIndex ? joint.GetRestPoseLocalTransform().GetAsMat4() : restPose[parent] * joint.GetRestPoseLocalTransform().GetAsMat4();
    auto& bone = sim.m_Bones[i];
    bone.m_uiParent = parent;
    bone.m_bDynamic = i == root || (m_bIncludeDescendants && skeleton.IsJointDescendantOf(i, root));
    bone.m_Settings = ResolveSettings(skeleton, i, bone.m_bDynamic);
    const auto& settings = bone.m_Settings;
    const double phase = GetWorld()->GetClock().GetAccumulatedTime().GetSeconds() * ezMath::Pi<double>() * 2.0 * settings.m_fSwayFrequency + settings.m_fSwayPhase + i * settings.m_fSwayBonePhase;
    for (ezUInt32 axis = 0; axis < 3; ++axis)
      bone.m_vSwayPhase.GetData()[axis] = phase * settings.m_vSwayFrequencyScale.GetData()[axis] + settings.m_vSwayAxisPhase.GetData()[axis];
  }

  JPH::Ref<JPH::GroupFilterTable> filter = new JPH::GroupFilterTable(skeleton.GetJointCount() + 1);
  sim.m_pFilter = filter;
  sim.m_bSelfCollision = m_bSelfCollision;
  sim.m_bCollideWithSkeleton = m_bCollideWithSkeleton;
  for (ezUInt16 i = 0; i < skeleton.GetJointCount(); ++i)
  {
    for (ezUInt16 j = 0; j < i; ++j)
    {
      const bool dynamicI = sim.m_Bones[i].m_bDynamic;
      const bool dynamicJ = sim.m_Bones[j].m_bDynamic;
      if ((!dynamicI && !dynamicJ) || (dynamicI && dynamicJ && !m_bSelfCollision) ||
          (dynamicI != dynamicJ && !m_bCollideWithSkeleton) || skeleton.GetJointByIndex(i).GetParentIndex() == j)
        filter->DisableCollision(i, j);
    }
    filter->DisableCollision(i, skeleton.GetJointCount());
  }

  auto& bodies = m_pJolt->GetBodyInterface();
  JPH::RefConst<JPH::Shape> empty = new JPH::EmptyShape();
  JPH::BodyCreationSettings anchor(empty, ezJoltConversionUtils::ToVec3(owner.m_vPosition), ezJoltConversionUtils::ToQuat(owner.m_qRotation),
    JPH::EMotionType::Kinematic, ezJoltCollisionFiltering::ConstructObjectLayer(0, ezJoltBroadphaseLayer::Ragdoll));
  sim.m_Anchor = bodies.CreateAndAddBody(anchor, JPH::EActivation::DontActivate);
  if (sim.m_Anchor.IsInvalid())
    return false;

  for (ezUInt16 i = 0; i < skeleton.GetJointCount(); ++i)
  {
    auto& bone = sim.m_Bones[i];
    const auto transform = GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[i]);
    auto shape = CreateBoneShape(desc, i, scale, restPose[i]);
    // Wind acts along the strand, not just at its pinned base. Use the midpoint of
    // the longest child segment; terminal bones inherit length from their parent.
    // This is a force application point only and does not add collision geometry.
    ezVec3 segment = ezVec3::MakeZero();
    for (ezUInt16 childIndex = i + 1; childIndex < skeleton.GetJointCount(); ++childIndex)
    {
      if (sim.m_Bones[childIndex].m_uiParent != i)
        continue;
      const auto child = GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[childIndex]);
      const ezVec3 candidate = transform.m_qRotation.GetInverse() * (child.m_vPosition - transform.m_vPosition);
      if (candidate.GetLengthSquared() > segment.GetLengthSquared())
        segment = candidate;
    }
    if (segment.IsZero() && bone.m_uiParent != ezInvalidJointIndex)
    {
      const auto parent = GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[bone.m_uiParent]);
      segment = ezBasisAxis::GetBasisVector(skeleton.m_BoneDirection) * (transform.m_vPosition - parent.m_vPosition).GetLength();
    }
    bone.m_vWindOffset = segment.IsZero() ? ezVec3::MakeZero() : segment * 0.5f - ezJoltConversionUtils::ToVec3(shape->GetCenterOfMass());
    JPH::BodyCreationSettings settings(shape, ezJoltConversionUtils::ToVec3(transform.m_vPosition), ezJoltConversionUtils::ToQuat(transform.m_qRotation),
      bone.m_bDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Kinematic,
      ezJoltCollisionFiltering::ConstructObjectLayer(skeleton.GetJointByIndex(i).GetCollisionLayer(), ezJoltBroadphaseLayer::Ragdoll));
    settings.mCollisionGroup = JPH::CollisionGroup(filter, sim.m_uiFilterID, i);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = ezMath::Clamp(bone.m_Settings.m_fMass, 0.001f, 100.0f);
    auto mass = shape->GetMassProperties();
    mass.ScaleToMass(settings.mMassPropertiesOverride.mMass);
    bone.m_LocalInertia = mass.mInertia;
    settings.mGravityFactor = bone.m_Settings.m_fGravityFactor;
    settings.mFriction = ezMath::Clamp(bone.m_Settings.m_fFriction, 0.0f, 1.0f);
    settings.mLinearDamping = ezMath::Clamp(bone.m_Settings.m_fAirDrag, 0.0f, 10.0f);
    settings.mAngularDamping = ezMath::Clamp(bone.m_Settings.m_fAirDrag, 0.0f, 10.0f) + 5.0f * ezMath::Clamp(bone.m_Settings.m_fFriction, 0.0f, 1.0f);
    settings.mAllowSleeping = false;
    // Empty joints carry constraints but intentionally have no invented collision geometry.
    settings.mMotionQuality = shape->GetInnerRadius() > 0 ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
    bone.m_Body = bodies.CreateAndAddBody(settings, bone.m_bDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    if (bone.m_Body.IsInvalid())
      return false;
  }

  for (ezUInt16 i = 0; i < skeleton.GetJointCount(); ++i)
  {
    auto& bone = sim.m_Bones[i];
    if (!bone.m_bDynamic)
      continue;
    const auto child = GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[i]);
    const auto parent = bone.m_uiParent == ezInvalidJointIndex ? owner : GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[bone.m_uiParent]);
    bone.m_vAnchor = parent.m_qRotation.GetInverse() * (child.m_vPosition - parent.m_vPosition);
    bone.m_qFrame = parent.m_qRotation.GetInverse() * child.m_qRotation;

    JPH::SixDOFConstraintSettings settings;
    settings.mPosition1 = settings.mPosition2 = ezJoltConversionUtils::ToVec3(child.m_vPosition);
    settings.mAxisX1 = settings.mAxisX2 = ezJoltConversionUtils::ToVec3(child.m_qRotation * ezVec3::MakeAxisX());
    settings.mAxisY1 = settings.mAxisY2 = ezJoltConversionUtils::ToVec3(child.m_qRotation * ezVec3::MakeAxisY());
    for (int a = 0; a < 6; ++a)
    {
      const auto axis = static_cast<JPH::SixDOFConstraint::EAxis>(a);
      const float limit = a < 3 ? ezMath::Max(bone.m_Settings.m_fMaxDistance * scale, 0.00001f) : ezMath::Clamp(bone.m_Settings.m_MaxAngle.GetRadian(), 0.0f, ezAngle::MakeFromDegree(179).GetRadian());
      const bool locked = (a == 3 && bone.m_Settings.m_bLockRotationX) || (a == 4 && bone.m_Settings.m_bLockRotationY) || (a == 5 && bone.m_Settings.m_bLockRotationZ);
      if (limit == 0 || locked)
        settings.MakeFixedAxis(axis);
      else
        settings.SetLimitedAxis(axis, -limit, limit);
      auto& motor = settings.mMotorSettings[a];
      const ezVec3 direction = ezBasisAxis::GetBasisVector(skeleton.m_BoneDirection);
      const bool lengthAxis = a < 3 && ezMath::Abs(direction.GetData()[a]) > 0.5f;
      motor.mSpringSettings.mFrequency = ezMath::Clamp(a < 3 ? (lengthAxis ? bone.m_Settings.m_fLengthStiffness : bone.m_Settings.m_fRootStiffness) : bone.m_Settings.m_fStiffness, 0.0f, 60.0f);
      motor.mSpringSettings.mFrequency *= 1.0f - 0.9f * ezMath::Clamp(bone.m_Settings.m_fSoftening, 0.0f, 1.0f);
      motor.mSpringSettings.mDamping = ezMath::Clamp(bone.m_Settings.m_fDamping, 0.0f, 2.0f);
      motor.SetForceLimit(ezMath::Max(bone.m_Settings.m_fMaxMotorForce, 0.0f));
      motor.SetTorqueLimit(ezMath::Max(bone.m_Settings.m_fMaxMotorForce, 0.0f));
      settings.mMaxFriction[a] = ezMath::Clamp(bone.m_Settings.m_fFriction, 0.0f, 1.0f) * ezMath::Clamp(bone.m_Settings.m_fMass, 0.001f, 100.0f) * 0.01f;
    }
    const JPH::BodyID parentID = bone.m_uiParent == ezInvalidJointIndex ? sim.m_Anchor : sim.m_Bones[bone.m_uiParent].m_Body;
    bone.m_pConstraint = static_cast<JPH::SixDOFConstraint*>(bodies.CreateConstraint(&settings, parentID, bone.m_Body));
    if (bone.m_pConstraint == nullptr)
      return false;
    for (int a = 0; a < 6; ++a)
    {
      const auto axis = static_cast<JPH::SixDOFConstraint::EAxis>(a);
      if (!bone.m_pConstraint->IsFixedAxis(axis))
        bone.m_pConstraint->SetMotorState(axis, JPH::EMotorState::Position);
    }
    m_pJolt->GetJoltSystem()->AddConstraint(bone.m_pConstraint);
  }
  return true;
}

void ezSpringBoneComponent::DestroySimulation()
{
  if (!m_pSimulation)
    return;
  auto& bodies = m_pJolt->GetBodyInterface();
  for (auto& bone : m_pSimulation->m_Bones)
  {
    if (bone.m_pConstraint)
      m_pJolt->GetJoltSystem()->RemoveConstraint(bone.m_pConstraint);
    bone.m_pConstraint = nullptr;
  }
  for (const auto& bone : m_pSimulation->m_Bones)
  {
    if (!bone.m_Body.IsInvalid())
    {
      bodies.RemoveBody(bone.m_Body);
      bodies.DestroyBody(bone.m_Body);
    }
  }
  if (!m_pSimulation->m_Anchor.IsInvalid())
  {
    bodies.RemoveBody(m_pSimulation->m_Anchor);
    bodies.DestroyBody(m_pSimulation->m_Anchor);
  }
  m_pJolt->DeleteObjectFilterID(m_pSimulation->m_uiFilterID);
  m_pSimulation = nullptr;
}

const ezSpringBoneSettings& ezSpringBoneComponent::ResolveSettings(const ezSkeleton& skeleton, ezUInt16 uiJoint, bool& out_bDynamic) const
{
  const ezUInt16 root = skeleton.FindJointByName(ezTempHashedString(m_sRootBone));
  out_bDynamic = root != ezInvalidJointIndex && (uiJoint == root || (m_bIncludeDescendants && skeleton.IsJointDescendantOf(uiJoint, root)));
  const ezSpringBoneSettings* settings = &m_Settings;
  for (const auto& entry : m_BoneOverrides)
  {
    if (skeleton.GetJointByIndex(uiJoint).GetName().GetString() == entry.m_sBone)
    {
      out_bDynamic = entry.m_bEnabled;
      settings = &entry.m_Settings;
    }
  }
  return *settings;
}

void ezSpringBoneComponent::SynchronizeSettings()
{
  if (!m_pSimulation || m_bReset)
    return;
  ezResourceLock<ezSkeletonResource> resource(m_hSkeleton, ezResourceAcquireMode::BlockTillLoaded);
  const auto& skeleton = resource->GetDescriptor().m_Skeleton;
  auto& sim = *m_pSimulation;
  auto& bodies = m_pJolt->GetBodyInterface();
  for (ezUInt16 i = 0; i < sim.m_Bones.GetCount(); ++i)
  {
    bool dynamic;
    const auto& settings = ResolveSettings(skeleton, i, dynamic);
    auto& bone = sim.m_Bones[i];
    if (dynamic != bone.m_bDynamic)
    {
      m_bReset = true;
      return;
    }
    if (settings == bone.m_Settings)
      continue;

    // Integrate frequency rather than multiplying world time, so frequency edits do not jump phase.
    for (ezUInt32 axis = 0; axis < 3; ++axis)
    {
      bone.m_vSwayPhase.GetData()[axis] +=
        (settings.m_fSwayPhase - bone.m_Settings.m_fSwayPhase + i * (settings.m_fSwayBonePhase - bone.m_Settings.m_fSwayBonePhase)) * settings.m_vSwayFrequencyScale.GetData()[axis] +
        settings.m_vSwayAxisPhase.GetData()[axis] - bone.m_Settings.m_vSwayAxisPhase.GetData()[axis];
    }
    const bool massChanged = settings.m_fMass != bone.m_Settings.m_fMass;
    bone.m_Settings = settings;
    if (!bone.m_bDynamic)
      continue;

    {
      JPH::BodyLockWrite lock(m_pJolt->GetJoltSystem()->GetBodyLockInterface(), bone.m_Body);
      if (lock.Succeeded())
      {
        auto& body = lock.GetBody();
        auto* motion = body.GetMotionProperties();
        if (massChanged)
        {
          auto mass = body.GetShape()->GetMassProperties();
          mass.ScaleToMass(ezMath::Clamp(settings.m_fMass, 0.001f, 100.0f));
          motion->SetMassProperties(motion->GetAllowedDOFs(), mass);
          bone.m_LocalInertia = mass.mInertia;
        }
        motion->SetGravityFactor(ezMath::Clamp(settings.m_fGravityFactor, -10.0f, 10.0f));
        motion->SetLinearDamping(ezMath::Clamp(settings.m_fAirDrag, 0.0f, 10.0f));
        motion->SetAngularDamping(ezMath::Clamp(settings.m_fAirDrag, 0.0f, 10.0f) + 5.0f * ezMath::Clamp(settings.m_fFriction, 0.0f, 1.0f));
      }
    }

    const float angle = ezMath::Clamp(settings.m_MaxAngle.GetRadian(), 0.0f, ezAngle::MakeFromDegree(179).GetRadian());
    JPH::Vec3 minAngle(-angle, -angle, -angle), maxAngle(angle, angle, angle);
    const bool locks[] = {settings.m_bLockRotationX, settings.m_bLockRotationY, settings.m_bLockRotationZ};
    for (ezUInt32 a = 0; a < 3; ++a)
      if (locks[a] || angle == 0)
      {
        minAngle.SetComponent(a, FLT_MAX);
        maxAngle.SetComponent(a, -FLT_MAX);
      }
    // Jolt supports changing limits in-place, preserving body poses, velocities and joint frames.
    bone.m_pConstraint->SetRotationLimits(minAngle, maxAngle);
    for (int a = 0; a < 6; ++a)
    {
      const auto axis = static_cast<JPH::SixDOFConstraint::EAxis>(a);
      auto& motor = bone.m_pConstraint->GetMotorSettings(axis);
      const ezVec3 direction = ezBasisAxis::GetBasisVector(skeleton.m_BoneDirection);
      const bool lengthAxis = a < 3 && ezMath::Abs(direction.GetData()[a]) > 0.5f;
      motor.mSpringSettings.mFrequency = ezMath::Clamp(a < 3 ? (lengthAxis ? settings.m_fLengthStiffness : settings.m_fRootStiffness) : settings.m_fStiffness, 0.0f, 60.0f) *
                                         (1.0f - 0.9f * ezMath::Clamp(settings.m_fSoftening, 0.0f, 1.0f));
      motor.mSpringSettings.mDamping = ezMath::Clamp(settings.m_fDamping, 0.0f, 2.0f);
      motor.SetForceLimit(ezMath::Max(settings.m_fMaxMotorForce, 0.0f));
      motor.SetTorqueLimit(ezMath::Max(settings.m_fMaxMotorForce, 0.0f));
      bone.m_pConstraint->SetMaxFriction(axis, ezMath::Clamp(settings.m_fFriction, 0.0f, 1.0f) * ezMath::Clamp(settings.m_fMass, 0.001f, 100.0f) * 0.01f);
      bone.m_pConstraint->SetMotorState(axis, bone.m_pConstraint->IsFixedAxis(axis) ? JPH::EMotorState::Off : JPH::EMotorState::Position);
    }
  }
  if (sim.m_bSelfCollision != m_bSelfCollision || sim.m_bCollideWithSkeleton != m_bCollideWithSkeleton)
  {
    for (ezUInt16 i = 0; i < sim.m_Bones.GetCount(); ++i)
      for (ezUInt16 j = 0; j < i; ++j)
      {
        const bool a = sim.m_Bones[i].m_bDynamic, b = sim.m_Bones[j].m_bDynamic;
        const bool collide = (a || b) && (a != b ? m_bCollideWithSkeleton : m_bSelfCollision) && sim.m_Bones[i].m_uiParent != j;
        if (collide)
          sim.m_pFilter->EnableCollision(i, j);
        else
          sim.m_pFilter->DisableCollision(i, j);
      }
    sim.m_bSelfCollision = m_bSelfCollision;
    sim.m_bCollideWithSkeleton = m_bCollideWithSkeleton;
    for (const auto& bone : sim.m_Bones)
      bodies.InvalidateContactCache(bone.m_Body);
  }
}

void ezSpringBoneComponent::PrePhysics()
{
  if (!EnsurePose())
    return;
  const ezTransform owner = GetOwner()->GetGlobalTransform();
  if (m_pSimulation)
  {
    const auto& previous = m_pSimulation->m_LastOwnerTransform;
    const ezQuat delta = previous.m_qRotation.GetInverse() * owner.m_qRotation;
    const float angle = 2.0f * ezMath::ACos(ezMath::Clamp(ezMath::Abs(delta.w), 0.0f, 1.0f)).GetRadian();
    if ((owner.m_vPosition - previous.m_vPosition).GetLength() > ezMath::Max(m_fTeleportDistance, 0.01f) ||
        angle > m_TeleportAngle.GetRadian() || !owner.m_vScale.IsEqual(previous.m_vScale, 0.001f))
      m_bReset = true;
  }
  SynchronizeSettings();
  if (m_bReset)
  {
    DestroySimulation();
    m_bReset = false;
    if (!CreateSimulation())
    {
      DestroySimulation();
      return;
    }
  }
  if (!m_pSimulation)
    return;

  auto& sim = *m_pSimulation;
  sim.m_LastOwnerTransform = owner;
  auto& bodies = m_pJolt->GetBodyInterface();
  const float dt = GetWorld()->GetClock().GetTimeDiff().AsFloatInSeconds();
  if (dt <= 0.0f)
    return;
  bodies.MoveKinematic(sim.m_Anchor, ezJoltConversionUtils::ToVec3(owner.m_vPosition), ezJoltConversionUtils::ToQuat(owner.m_qRotation), dt);
  const auto* wind = GetWorld()->GetModuleReadOnly<ezWindWorldModuleInterface>();
  const bool applyForces = sim.m_uiLastForceUpdate != m_pJolt->GetJoltUpdateCounter();
  sim.m_uiLastForceUpdate = m_pJolt->GetJoltUpdateCounter();

  ezDynamicArray<ezMat4> colliderPose;
  colliderPose.SetCountUninitialized(sim.m_Bones.GetCount());
  const ezMat4 worldRoot = owner.GetAsMat4() * m_RootTransform.GetAsMat4();
  const ezMat4 inverseRoot = worldRoot.GetInverse();
  for (ezUInt16 i = 0; i < sim.m_Bones.GetCount(); ++i)
  {
    auto& bone = sim.m_Bones[i];
    const auto target = GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[i]);
    if (!bone.m_bDynamic)
    {
      const ezMat4 local = bone.m_uiParent == ezInvalidJointIndex ? m_AnimationPose[i] : m_AnimationPose[bone.m_uiParent].GetInverse() * m_AnimationPose[i];
      colliderPose[i] = bone.m_uiParent == ezInvalidJointIndex ? local : colliderPose[bone.m_uiParent] * local;
      const auto collider = GetBoneWorldTransform(owner, m_RootTransform, colliderPose[i]);
      bodies.MoveKinematic(bone.m_Body, ezJoltConversionUtils::ToVec3(collider.m_vPosition), ezJoltConversionUtils::ToQuat(collider.m_qRotation), dt);
      continue;
    }

    JPH::RVec3 physicalPosition;
    JPH::Quat physicalRotation;
    bodies.GetPositionAndRotation(bone.m_Body, physicalPosition, physicalRotation);
    const auto physical = ezJoltConversionUtils::ToTransform(physicalPosition, physicalRotation);
    colliderPose[i] = inverseRoot * physical.GetAsMat4() * target.GetAsMat4().GetInverse() * worldRoot * m_AnimationPose[i];

    const auto parent = bone.m_uiParent == ezInvalidJointIndex ? owner : GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[bone.m_uiParent]);
    const ezVec3 offset = bone.m_qFrame.GetInverse() * (parent.m_qRotation.GetInverse() * (target.m_vPosition - parent.m_vPosition) - bone.m_vAnchor);
    bone.m_pConstraint->SetTargetPositionCS(ezJoltConversionUtils::ToVec3(offset));
    const auto& settings = bone.m_Settings;
    ezVec3 wave;
    for (ezUInt32 axis = 0; axis < 3; ++axis)
    {
      double& phase = bone.m_vSwayPhase.GetData()[axis];
      phase += dt * ezMath::Pi<double>() * 2.0 * ezMath::Clamp(settings.m_fSwayFrequency, 0.0f, 20.0f) * ezMath::Clamp(settings.m_vSwayFrequencyScale.GetData()[axis], 0.0f, 20.0f);
      phase = ezMath::Mod(phase, ezMath::Pi<double>() * 2.0);
      wave.GetData()[axis] = static_cast<float>(sin(phase));
    }
    const ezVec3 angles = settings.m_vSwayAngles.CompMul(wave);
    const ezQuat sway = ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisZ(), ezAngle::MakeFromDegree(angles.z)) *
                        ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisY(), ezAngle::MakeFromDegree(angles.y)) *
                        ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisX(), ezAngle::MakeFromDegree(angles.x));
    bone.m_pConstraint->SetTargetOrientationBS(ezJoltConversionUtils::ToQuat(parent.m_qRotation.GetInverse() * target.m_qRotation * sway));
    const float distance = ezMath::Max(bone.m_Settings.m_fMaxDistance * owner.m_vScale.x, 0.00001f);
    // Keep a tiny interval: Jolt treats equal min/max as an axis fixed at zero.
    bone.m_pConstraint->SetTranslationLimits(ezJoltConversionUtils::ToVec3(offset - ezVec3(distance)), ezJoltConversionUtils::ToVec3(offset + ezVec3(distance)));

    if (!applyForces)
      continue;

    const JPH::Vec3 arm = physicalRotation * ezJoltConversionUtils::ToVec3(settings.m_vForceApplicationOffset * owner.m_vScale.x);
    const JPH::Vec3 windArm = settings.m_vForceApplicationOffset.IsZero() ? physicalRotation * ezJoltConversionUtils::ToVec3(bone.m_vWindOffset) : arm;
    const auto centerOfMass = bodies.GetCenterOfMassPosition(bone.m_Body);
    const ezVec3 windPosition = ezJoltConversionUtils::ToVec3(centerOfMass + windArm);
    ezVec3 acceleration = settings.m_vExternalAcceleration;
    acceleration += owner.m_qRotation * settings.m_vSwayAcceleration.CompMul(wave);
    ezVec3 windAcceleration = ezVec3::MakeZero();
    if (wind && settings.m_fWindInfluence > 0)
    {
      // A local volume may cover only the root, middle or end of the strand.
      // Average samples along it, so moving the pressure point does not remove
      // wind reception at the root and uniform wind retains its original strength.
      const ezVec3 rootPosition = ezJoltConversionUtils::ToVec3(physicalPosition);
      const ezVec3 midpoint = ezJoltConversionUtils::ToVec3(centerOfMass + physicalRotation * ezJoltConversionUtils::ToVec3(bone.m_vWindOffset));
      ezVec3 velocity = wind->GetWindAt(rootPosition) + wind->GetWindAt(midpoint) + wind->GetWindAt(midpoint * 2.0f - rootPosition);
      float sampleCount = 3.0f;
      if (!settings.m_vForceApplicationOffset.IsZero())
      {
        velocity += wind->GetWindAt(windPosition);
        sampleCount += 1.0f;
      }
      velocity *= settings.m_fWindInfluence / sampleCount;
      windAcceleration = velocity;
      windAcceleration += wind->ComputeWindFlutter(velocity, target.m_qRotation * ezVec3::MakeAxisX(), settings.m_fWindFlutter, GetOwner()->GetStableRandomSeed() + i);
    }
    const auto force = ezJoltConversionUtils::ToVec3(acceleration * ezMath::Clamp(settings.m_fMass, 0.001f, 100.0f));
    // Offset is relative to the COM; zero preserves version 1 force application.
    const auto windForce = ezJoltConversionUtils::ToVec3(windAcceleration * ezMath::Clamp(settings.m_fMass, 0.001f, 100.0f));
    bodies.AddForce(bone.m_Body, force + windForce);
    const JPH::Vec3 angularAcceleration = ezJoltConversionUtils::ToVec3(settings.m_vSwayAngularAcceleration.CompMul(wave) * ezMath::Pi<float>() / 180.0f);
    const JPH::Vec3 torque = physicalRotation * bone.m_LocalInertia.Multiply3x3(angularAcceleration) + arm.Cross(force) + windArm.Cross(windForce);
    bodies.AddTorque(bone.m_Body, torque);
  }
}

void ezSpringBoneComponent::PostPhysics()
{
  if (!m_pSimulation || m_AnimationPose.GetCount() != m_pSimulation->m_Bones.GetCount())
    return;
  ezResourceLock<ezSkeletonResource> resource(m_hSkeleton, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (resource.GetAcquireResult() != ezResourceAcquireResult::Final || resource->GetCurrentResourceChangeCounter() != m_pSimulation->m_uiResourceChangeCounter)
    return;
  auto& sim = *m_pSimulation;
  const ezTransform owner = GetOwner()->GetGlobalTransform();
  const ezMat4 inverseRoot = (owner.GetAsMat4() * m_RootTransform.GetAsMat4()).GetInverse();
  auto& bodies = m_pJolt->GetBodyInterface();
  for (ezUInt16 i = 0; i < sim.m_Bones.GetCount(); ++i)
  {
    const auto& bone = sim.m_Bones[i];
    const ezMat4 animationLocal = bone.m_uiParent == ezInvalidJointIndex ? m_AnimationPose[i] : m_AnimationPose[bone.m_uiParent].GetInverse() * m_AnimationPose[i];
    const ezMat4 inherited = bone.m_uiParent == ezInvalidJointIndex ? animationLocal : sim.m_OutputPose[bone.m_uiParent] * animationLocal;
    sim.m_OutputPose[i] = inherited;
    if (!bone.m_bDynamic)
      continue;

    JPH::RVec3 position;
    JPH::Quat rotation;
    bodies.GetPositionAndRotation(bone.m_Body, position, rotation);
    const ezTransform body = ezJoltConversionUtils::ToTransform(position, rotation);
    // Preserve the animated scale, including the skeleton import transform.
    const ezTransform target = GetBoneWorldTransform(owner, m_RootTransform, m_AnimationPose[i]);
    const ezMat4 physical = inverseRoot * body.GetAsMat4() * target.GetAsMat4().GetInverse() * owner.GetAsMat4() * m_RootTransform.GetAsMat4() * m_AnimationPose[i];
    ezTransform base = ezTransform::MakeFromMat4(inherited);
    const ezTransform result = ezTransform::MakeFromMat4(physical);
    const float blend = ezMath::Clamp(bone.m_Settings.m_fInfluence, 0.0f, 1.0f);
    base.m_vPosition = ezMath::Lerp(base.m_vPosition, result.m_vPosition, blend);
    base.m_qRotation = ezQuat::MakeSlerp(base.m_qRotation, result.m_qRotation, blend);
    sim.m_OutputPose[i] = base.GetAsMat4();
  }
  ezMsgAnimationPoseUpdated msg;
  msg.m_pSkeleton = &resource->GetDescriptor().m_Skeleton;
  msg.m_pRootTransform = &m_RootTransform;
  msg.m_ModelTransforms = sim.m_OutputPose;
  m_bSendingPose = true;
  GetOwner()->SendMessageRecursive(msg);
  m_bSendingPose = false;
}

void ezSpringBoneComponent::OnPhysicsAddImpulse(ezMsgPhysicsAddImpulse& ref_msg)
{
  if (!m_pSimulation)
    return;
  auto& bodies = m_pJolt->GetBodyInterface();
  JPH::BodyID nearest;
  float best = ezMath::MaxValue<float>();
  for (const auto& bone : m_pSimulation->m_Bones)
  {
    if (!bone.m_bDynamic)
      continue;
    const float distance = (ezJoltConversionUtils::ToVec3(bodies.GetPosition(bone.m_Body)) - ref_msg.m_vGlobalPosition).GetLengthSquared();
    if (distance < best)
    {
      best = distance;
      nearest = bone.m_Body;
    }
  }
  if (!nearest.IsInvalid())
    bodies.AddImpulse(nearest, ezJoltConversionUtils::ToVec3(ref_msg.m_vImpulse), ezJoltConversionUtils::ToVec3(ref_msg.m_vGlobalPosition));
}

ezSpringBoneComponentManager::ezSpringBoneComponentManager(ezWorld* pWorld)
  : ezComponentManager<ezSpringBoneComponent, ezBlockStorageType::FreeList>(pWorld)
{
}

void ezSpringBoneComponentManager::Initialize()
{
  SUPER::Initialize();
  auto pre = EZ_CREATE_MODULE_UPDATE_FUNCTION_DESC(ezSpringBoneComponentManager::PrePhysics, this);
  pre.m_Phase = ezWorldUpdatePhase::PreAsync;
  pre.m_fPriority = -90000.0f;
  pre.m_bOnlyUpdateWhenSimulating = true;
  RegisterUpdateFunction(pre);
  auto post = EZ_CREATE_MODULE_UPDATE_FUNCTION_DESC(ezSpringBoneComponentManager::PostPhysics, this);
  post.m_Phase = ezWorldUpdatePhase::PostAsync;
  post.m_fPriority = -1000.0f;
  post.m_bOnlyUpdateWhenSimulating = true;
  RegisterUpdateFunction(post);
}

void ezSpringBoneComponentManager::PrePhysics(const ezWorldModule::UpdateContext& context)
{
  for (auto it = GetComponents(); it.IsValid(); ++it)
    if (it->IsActiveAndInitialized())
      it->PrePhysics();
}

void ezSpringBoneComponentManager::PostPhysics(const ezWorldModule::UpdateContext& context)
{
  for (auto it = GetComponents(); it.IsValid(); ++it)
    if (it->IsActiveAndInitialized())
      it->PostPhysics();
}

// Editor scene/prefab migration deliberately keeps all stored settings.
class ezSpringBoneComponentPatch_1_2 : public ezGraphPatch
{
public:
  ezSpringBoneComponentPatch_1_2()
    : ezGraphPatch("ezSpringBoneComponent", 2)
  {
  }
  void Patch(ezGraphPatchContext& context, ezAbstractObjectGraph* graph, ezAbstractObjectNode* node) const override
  {
    node->RemoveProperty("Preset");
  }
};
static ezSpringBoneComponentPatch_1_2 s_SpringBoneComponentPatch;

EZ_STATICLINK_FILE(SpringBonePlugin, SpringBonePlugin_Components_SpringBoneComponent);
