#pragma once

#include <Core/World/ComponentManager.h>
#include <RendererCore/AnimationSystem/Declarations.h>
#include <SpringBonePlugin/SpringBonePluginDLL.h>

class ezJoltWorldModule;
struct ezMsgPhysicsAddImpulse;

/// Frequencies are in Hz, damping is a damping ratio (1 = critically damped).
/// Translation limits are in meters, in the joint's initial constraint frame.
struct EZ_SPRINGBONEPLUGIN_DLL ezSpringBoneSettings
{
  float m_fMass = 0.1f;
  float m_fStiffness = 4.0f;
  float m_fDamping = 0.5f;
  float m_fRootStiffness = 20.0f;
  float m_fLengthStiffness = 20.0f;
  float m_fSoftening = 0.0f;
  float m_fGravityFactor = 0.25f;
  float m_fAirDrag = 0.2f;
  float m_fFriction = 0.5f;
  float m_fInfluence = 1.0f;
  float m_fWindInfluence = 1.0f;
  float m_fWindFlutter = 0.5f;
  float m_fMaxDistance = 0.0f;
  ezAngle m_MaxAngle = ezAngle::MakeFromDegree(60.0f);
  bool m_bLockRotationX = false;
  bool m_bLockRotationY = false;
  bool m_bLockRotationZ = false;
  float m_fMaxMotorForce = 100.0f;
  ezVec3 m_vExternalAcceleration = ezVec3::MakeZero();
  ezVec3 m_vSwayAcceleration = ezVec3::MakeZero();
  float m_fSwayFrequency = 0.5f;
  float m_fSwayPhase = 0.0f;
  ezVec3 m_vSwayAngles = ezVec3::MakeZero();              ///< Desired local rotation amplitude in degrees.
  ezVec3 m_vSwayAngularAcceleration = ezVec3::MakeZero(); ///< Local angular acceleration in degrees/s^2.
  ezVec3 m_vSwayFrequencyScale = ezVec3(1.0f, 0.83f, 1.17f);
  ezVec3 m_vSwayAxisPhase = ezVec3(0.0f, 1.3f, 2.1f);     ///< Radians.
  float m_fSwayBonePhase = 0.73f;                         ///< Radians per skeleton joint index.
  ezVec3 m_vForceApplicationOffset = ezVec3::MakeZero();  ///< Offset from center of mass, in local bone axes, meters.

  void Serialize(ezStreamWriter& stream) const;
  void Deserialize(ezStreamReader& stream, ezUInt32 uiVersion = 2);
  bool operator==(const ezSpringBoneSettings& rhs) const;
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_SPRINGBONEPLUGIN_DLL, ezSpringBoneSettings);

struct EZ_SPRINGBONEPLUGIN_DLL ezSpringBoneOverride
{
  ezString m_sBone;
  bool m_bEnabled = true;
  ezSpringBoneSettings m_Settings;
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_SPRINGBONEPLUGIN_DLL, ezSpringBoneOverride);

class EZ_SPRINGBONEPLUGIN_DLL ezSpringBoneComponentManager : public ezComponentManager<class ezSpringBoneComponent, ezBlockStorageType::FreeList>
{
public:
  ezSpringBoneComponentManager(ezWorld* pWorld);
  virtual void Initialize() override;

private:
  void PrePhysics(const ezWorldModule::UpdateContext& context);
  void PostPhysics(const ezWorldModule::UpdateContext& context);
};

/// Simulates a selected bone or subtree on top of the incoming animation pose.
/// Add to the same object as the animated mesh. Bone Shapes are the only collision geometry.
/// Unselected joints remain animated; selected joints use Jolt six degree of freedom springs.
class EZ_SPRINGBONEPLUGIN_DLL ezSpringBoneComponent : public ezComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezSpringBoneComponent, ezComponent, ezSpringBoneComponentManager);

public:
  ezSpringBoneComponent();
  ~ezSpringBoneComponent();

  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;

  void SetSkeleton(const ezSkeletonResourceHandle& hSkeleton);
  const ezSkeletonResourceHandle& GetSkeleton() const { return m_hSkeleton; }
  void SetRootBone(ezStringView sBone);
  ezStringView GetRootBone() const { return m_sRootBone; }
  void SetIncludeDescendants(bool bInclude);
  bool GetIncludeDescendants() const { return m_bIncludeDescendants; }

  ezSpringBoneSettings m_Settings;
  ezDynamicArray<ezSpringBoneOverride> m_BoneOverrides;
  bool m_bSelfCollision = false;
  bool m_bCollideWithSkeleton = true;
  float m_fTeleportDistance = 2.0f;
  ezAngle m_TeleportAngle = ezAngle::MakeFromDegree(90.0f);

  /// Rebuilds bodies from the current animation pose on the next safe update.
  void ResetSimulation(); // [ scriptable ]
  ezArrayPtr<const ezMat4> GetSimulatedPose() const;
  void OnAnimationPoseUpdated(ezMsgAnimationPoseUpdated& ref_msg);
  void OnPhysicsAddImpulse(ezMsgPhysicsAddImpulse& ref_msg);

protected:
  virtual void OnActivated() override;
  virtual void OnSimulationStarted() override;
  virtual void OnDeactivated() override;

private:
  friend class ezSpringBoneComponentManager;
  struct Simulation;
  ezUniquePtr<Simulation> m_pSimulation;

  void PrePhysics();
  void PostPhysics();
  void DestroySimulation();
  bool EnsurePose();
  bool CreateSimulation();
  void SynchronizeSettings();
  const ezSpringBoneSettings& ResolveSettings(const class ezSkeleton& skeleton, ezUInt16 uiJoint, bool& out_bDynamic) const;

  ezSkeletonResourceHandle m_hSkeleton;
  ezString m_sRootBone;
  bool m_bIncludeDescendants = true;
  bool m_bReset = true;
  bool m_bSendingPose = false;
  ezDynamicArray<ezMat4> m_AnimationPose;
  ezTransform m_RootTransform = ezTransform::MakeIdentity();
  ezJoltWorldModule* m_pJolt = nullptr;
};
