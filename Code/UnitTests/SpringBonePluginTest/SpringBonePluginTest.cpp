#include <Core/World/World.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/MemoryStream.h>
#include <Foundation/Serialization/AbstractObjectGraph.h>
#include <Foundation/Serialization/GraphVersioning.h>
#include <GameEngine/Effects/Wind/SimpleWindWorldModule.h>
#include <GameEngine/Effects/Wind/WindVolumeComponent.h>
#include <JoltPlugin/System/JoltWorldModule.h>
#include <RendererCore/AnimationSystem/SkeletonBuilder.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <SpringBonePlugin/Components/SpringBoneComponent.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>

EZ_TESTFRAMEWORK_ENTRY_POINT("SpringBonePluginTest", "Spring Bone Plugin Tests")
EZ_CREATE_SIMPLE_TEST_GROUP(SpringBone);
void TestSpringBoneEditorMirror(ezSpringBoneComponent& spring, ezWorld& world);
void TestRagdollsWind();

EZ_CREATE_SIMPLE_TEST(SpringBone, Simulation)
{
  ezStartup::StartupCoreSystems();
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Base", "SpringBoneTests", "base").Succeeded());
  ezGALDeviceCreationDescription deviceDesc;
  ezGALDevice* device = ezGALDeviceFactory::CreateDevice("DX11", ezFoundation::GetDefaultAllocator(), deviceDesc);
  if (!EZ_TEST_BOOL(device != nullptr && device->Init().Succeeded()))
    return;
  ezGALDevice::SetDefaultDevice(device);
  ezStartup::StartupHighLevelSystems();
  TestRagdollsWind();
  {
    ezSkeletonResourceDescriptor desc;
    ezSkeletonBuilder builder;
    builder.AddJoint("Root", ezTransform::MakeIdentity());
    builder.AddJoint("Spring", ezTransform(ezVec3(0, 0, 1)), 0);
    builder.AddJoint("Tip", ezTransform(ezVec3(0.4f, 0, 0)), 1);
    builder.AddJoint("Unaffected", ezTransform(ezVec3(0, 1, 0)), 0);
    builder.BuildSkeleton(desc.m_Skeleton);
    desc.m_Skeleton.m_BoneDirection = ezBasisAxis::PositiveX;
    auto& shape = desc.m_Geometry.ExpandAndGetRef();
    shape.m_uiAttachedToJoint = 1;
    shape.m_Type = ezSkeletonJointGeometryType::Capsule;
    shape.m_Transform = ezTransform::MakeIdentity();
    shape.m_Transform.m_vScale = ezVec3(0.3f, 0.04f, 0.04f);
    const auto skeleton = ezResourceManager::CreateResource<ezSkeletonResource>("SpringBoneTestSkeleton", std::move(desc));

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Spawned cone wind starts a resting bone without Simple Wind")
    {
      for (bool existingWindModule : {false, true})
      for (float coneX : {0.0f, 0.2f, 0.4f})
      {
        ezWorldDesc coneWorldDesc("SpringBoneConeTest");
        ezWorld coneWorld(coneWorldDesc);
        EZ_LOCK(coneWorld.GetWriteMarker());
        coneWorld.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / 60.0));
        if (existingWindModule)
          coneWorld.GetOrCreateModule<ezSimpleWindWorldModule>();
        ezGameObjectDesc boneDesc;
        boneDesc.m_bDynamic = true;
        ezGameObject* boneOwner;
        coneWorld.CreateObject(boneDesc, boneOwner);
        ezSpringBoneComponent* spring;
        ezSpringBoneComponent::CreateComponent(boneOwner, spring);
        spring->SetSkeleton(skeleton);
        spring->SetRootBone("Spring");
        spring->SetIncludeDescendants(false);
        spring->m_Settings.m_fGravityFactor = 0;
        spring->m_Settings.m_fStiffness = 1;
        spring->m_Settings.m_fDamping = 1;
        spring->m_Settings.m_fWindFlutter = 0;
        for (ezUInt32 i = 0; i < 120; ++i)
          coneWorld.Update();

        ezGameObjectDesc coneDesc;
        coneDesc.m_LocalPosition = ezVec3(coneX, 0, 0);
        coneDesc.m_LocalRotation = ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisY(), ezAngle::MakeFromDegree(-90));
        ezGameObject* coneOwner;
        coneWorld.CreateObject(coneDesc, coneOwner);
        ezWindVolumeConeComponent* cone;
        ezWindVolumeConeComponent::CreateComponent(coneOwner, cone);
        cone->SetLength(2);
        cone->SetAngle(ezAngle::MakeFromDegree(10));
        cone->m_Strength = ezWindStrength::Storm;
        for (ezUInt32 i = 0; i < 6; ++i)
          coneWorld.Update();
        const auto* wind = coneWorld.GetModuleReadOnly<ezWindWorldModuleInterface>();
        if (EZ_TEST_BOOL(wind != nullptr))
        {
          // Test separate narrow cones at the attachment, midpoint and tip.
          EZ_TEST_BOOL(wind->GetWindAt(ezVec3(coneX, 0, 1)).z > 10);
        }
        EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.001f);
        cone->SetActiveFlag(false);
        for (ezUInt32 i = 0; i < 240; ++i)
          coneWorld.Update();
        EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1, 0.001f);
        cone->SetActiveFlag(true);
        for (ezUInt32 i = 0; i < 6; ++i)
          coneWorld.Update();
        EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.001f);
      }
    }

    ezWorldDesc worldDesc("SpringBoneTest");
    ezWorld world(worldDesc);
    EZ_LOCK(world.GetWriteMarker());
    world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / 60.0));
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    ezGameObject* owner;
    world.CreateObject(objectDesc, owner);
    ezSpringBoneComponent* spring;
    ezSpringBoneComponent::CreateComponent(owner, spring);
    spring->SetSkeleton(skeleton);
    spring->SetRootBone("Spring");
    spring->SetIncludeDescendants(false);
    spring->m_Settings.m_fGravityFactor = 0;
    spring->m_Settings.m_fFriction = 0;
    world.Update();
    auto* jolt = world.GetModule<ezJoltWorldModule>();
    const ezUInt32 bodyCount = jolt->GetJoltSystem()->GetNumBodies();
    EZ_TEST_INT(bodyCount, 5);
    auto Tick = [&](ezUInt32 count)
    {
      for (ezUInt32 i = 0; i < count; ++i)
        world.Update();
    };

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Rest pose, selection and child propagation")
    {
      Tick(30);
      auto pose = spring->GetSimulatedPose();
      EZ_TEST_INT(pose.GetCount(), 4);
      EZ_TEST_BOOL(pose[2].GetTranslationVector().IsEqual(ezVec3(0.4f, 0, 1), 0.005f));
      ezMsgPhysicsAddImpulse impulse;
      impulse.m_vGlobalPosition = ezVec3(0.3f, 0, 1);
      impulse.m_vImpulse = ezVec3(0, 0, 0.03f);
      spring->OnPhysicsAddImpulse(impulse);
      Tick(5);
      pose = spring->GetSimulatedPose();
      EZ_TEST_BOOL(ezMath::Abs(pose[2].GetTranslationVector().z - 1.0f) > 0.005f);
      EZ_TEST_BOOL(pose[1].GetTranslationVector().IsEqual(ezVec3(0, 0, 1), 0.01f));
      EZ_TEST_BOOL(pose[3].GetTranslationVector().IsEqual(ezVec3(0, 1, 0), 0.001f));
      Tick(180);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().IsEqual(ezVec3(0.4f, 0, 1), 0.01f));
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Animated translation, sway and wind")
    {
      spring->m_Settings.m_vSwayAcceleration = ezVec3(0, 0, 3);
      spring->ResetSimulation();
      Tick(25);
      EZ_TEST_BOOL(ezMath::Abs(spring->GetSimulatedPose()[2].GetTranslationVector().z - 1.0f) > 0.002f);
      spring->m_Settings.m_vSwayAcceleration.SetZero();
      spring->m_Settings.m_fWindInfluence = 1;
      auto* wind = world.GetOrCreateModule<ezSimpleWindWorldModule>();
      wind->SetFallbackWind(ezVec3(0, 0, 5));
      spring->ResetSimulation();
      Tick(60);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.005f);
      wind->SetFallbackWind(ezVec3::MakeZero());

      ezResourceLock<ezSkeletonResource> resource(skeleton, ezResourceAcquireMode::BlockTillLoaded);
      ezDynamicArray<ezMat4> animation;
      animation = spring->GetSimulatedPose();
      animation[1].SetTranslationVector(ezVec3(0, 0, 1.2f));
      animation[2].SetTranslationVector(ezVec3(0.4f, 0, 1.2f));
      ezMsgAnimationPoseUpdated pose;
      pose.m_pSkeleton = &resource->GetDescriptor().m_Skeleton;
      pose.m_pRootTransform = &resource->GetDescriptor().m_RootTransform;
      pose.m_ModelTransforms = animation;
      spring->OnAnimationPoseUpdated(pose);
      Tick(120);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[1].GetTranslationVector().z, 1.2f, 0.01f);
      EZ_TEST_BOOL(pose.m_bContinueAnimating);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Teleport, pause, reset and cleanup")
    {
      owner->SetLocalPosition(ezVec3(100, 0, 0));
      Tick(5);
      for (const auto& matrix : spring->GetSimulatedPose())
        EZ_TEST_BOOL(matrix.IsValid());
      EZ_TEST_BOOL(spring->GetSimulatedPose()[1].GetTranslationVector().GetLength() < 3);
      EZ_TEST_INT(jolt->GetJoltSystem()->GetNumBodies(), bodyCount);
      world.SetWorldSimulationEnabled(false);
      Tick(5);
      world.SetWorldSimulationEnabled(true);
      spring->ResetSimulation();
      Tick(5);
      EZ_TEST_INT(jolt->GetJoltSystem()->GetNumBodies(), bodyCount);
      spring->SetActiveFlag(false);
      Tick(2);
      EZ_TEST_INT(jolt->GetJoltSystem()->GetNumBodies(), 0);
      spring->SetActiveFlag(true);
      Tick(2);
      EZ_TEST_INT(jolt->GetJoltSystem()->GetNumBodies(), bodyCount);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Settings serialization")
    {
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter writer(&storage);
      spring->m_Settings.Serialize(writer);
      ezMemoryStreamReader reader(&storage);
      ezSpringBoneSettings restored;
      restored.Deserialize(reader);
      EZ_TEST_FLOAT(restored.m_fStiffness, spring->m_Settings.m_fStiffness, 0);
      EZ_TEST_FLOAT(restored.m_fDamping, spring->m_Settings.m_fDamping, 0);
      EZ_TEST_FLOAT(restored.m_fWindInfluence, spring->m_Settings.m_fWindInfluence, 0);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Bone Shapes, collision filtering and convex mesh coordinates")
    {
      owner->SetLocalPosition(ezVec3::MakeZero());
      ezSkeletonResourceDescriptor collisionDesc;
      ezSkeletonBuilder collisionBuilder;
      collisionBuilder.AddJoint("Root", ezTransform::MakeIdentity());
      collisionBuilder.AddJoint("Spring", ezTransform(ezVec3(0, 0, 1)), 0);
      collisionBuilder.AddJoint("Tip", ezTransform(ezVec3(0.4f, 0, 0)), 1);
      collisionBuilder.AddJoint("Obstacle", ezTransform(ezVec3(0, 0, 0.7f)), 0);
      collisionBuilder.AddJoint("Sphere", ezTransform(ezVec3(2, 0, 1)), 0);
      collisionBuilder.AddJoint("Sideways", ezTransform(ezVec3(3, 0, 1)), 0);
      collisionBuilder.AddJoint("Convex", ezTransform(ezVec3(4, 0, 1)), 0);
      collisionBuilder.BuildSkeleton(collisionDesc.m_Skeleton);
      collisionDesc.m_Skeleton.m_BoneDirection = ezBasisAxis::PositiveX;
      auto AddShape = [&](ezUInt16 joint, ezSkeletonJointGeometryType::Enum type, ezVec3 size) -> ezSkeletonResourceGeometry&
      {
        auto& geo = collisionDesc.m_Geometry.ExpandAndGetRef();
        geo.m_uiAttachedToJoint = joint;
        geo.m_Type = type;
        geo.m_Transform = ezTransform::MakeIdentity();
        geo.m_Transform.m_vScale = size;
        return geo;
      };
      AddShape(1, ezSkeletonJointGeometryType::Capsule, ezVec3(0.3f, 0.04f, 0.04f));
      AddShape(3, ezSkeletonJointGeometryType::Box, ezVec3(0.7f, 0.5f, 0.1f));
      AddShape(4, ezSkeletonJointGeometryType::Sphere, ezVec3(0.1f));
      AddShape(5, ezSkeletonJointGeometryType::CapsuleSideways, ezVec3(0.3f, 0.1f, 0.1f));
      auto& convex = AddShape(6, ezSkeletonJointGeometryType::ConvexMesh, ezVec3(1));
      for (ezUInt32 vertex = 0; vertex < 8; ++vertex)
        convex.m_VertexPositions.PushBack(ezVec3(4, 0, 1) + ezVec3((vertex & 1) ? 0.1f : -0.1f, (vertex & 2) ? 0.1f : -0.1f, (vertex & 4) ? 0.1f : -0.1f));
      const auto collisionSkeleton = ezResourceManager::CreateResource<ezSkeletonResource>("SpringBoneCollisionSkeleton", std::move(collisionDesc));
      spring->SetSkeleton(collisionSkeleton);
      spring->m_Settings = ezSpringBoneSettings();
      spring->m_Settings.m_fGravityFactor = 1;
      spring->m_Settings.m_MaxAngle = ezAngle::MakeFromDegree(120);
      spring->m_Settings.m_fStiffness = 0;
      spring->m_Settings.m_fFriction = 0;
      spring->m_Settings.m_fWindInfluence = 0;
      Tick(180);
      const float collidingHeight = spring->GetSimulatedPose()[2].GetTranslationVector().z;
      EZ_TEST_BOOL(collidingHeight > 0.65f);
      for (int x = 2; x <= 4; ++x)
      {
        ezPhysicsCastResult hit;
        ezPhysicsQueryParameters params(0);
        params.m_ShapeTypes = ezPhysicsShapeType::Ragdoll;
        EZ_TEST_BOOL(jolt->Raycast(hit, ezVec3(static_cast<float>(x), 0, 2), ezVec3(0, 0, -1), 2, params));
        EZ_TEST_FLOAT(hit.m_vPosition.z, 1.1f, 0.02f);
      }
      spring->m_bCollideWithSkeleton = false;
      Tick(180);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z < collidingHeight - 0.1f);

      ezGameObjectDesc floorDesc;
      floorDesc.m_LocalPosition = ezVec3(0.35f, 0, 0.7f);
      ezGameObject* floor;
      world.CreateObject(floorDesc, floor);
      jolt->AddStaticCollisionBox(floor, ezVec3(0.7f, 0.5f, 0.1f));
      spring->ResetSimulation();
      Tick(180);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 0.65f);
      world.DeleteObjectNow(floor->GetHandle());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Wind consistency at different frame rates")
    {
      spring->SetSkeleton(skeleton);
      spring->m_Settings = ezSpringBoneSettings();
      spring->m_Settings.m_fGravityFactor = 0;
      auto* wind = world.GetModule<ezSimpleWindWorldModule>();
      wind->SetFallbackWind(ezVec3(0, 0, 3));
      spring->m_Settings.m_fWindFlutter = 0;
      ezVec3 previous = ezVec3::MakeZero();
      for (ezUInt32 rate : {30u, 60u, 120u})
      {
        world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / rate));
        spring->ResetSimulation();
        Tick(rate * 4);
        const ezVec3 tip = spring->GetSimulatedPose()[2].GetTranslationVector();
        if (!previous.IsZero())
          EZ_TEST_BOOL(tip.IsEqual(previous, 0.02f));
        previous = tip;
      }
      wind->SetFallbackWind(ezVec3::MakeZero());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Overrides, influence and world serialization")
    {
      spring->SetIncludeDescendants(true);
      spring->m_Settings.m_fInfluence = 0;
      auto& override = spring->m_BoneOverrides.ExpandAndGetRef();
      override.m_sBone = "Tip";
      override.m_bEnabled = false;
      override.m_Settings.m_fStiffness = 11;
      override.m_Settings.m_vSwayAngles = ezVec3(3, 5, 7);
      spring->m_Settings.m_vSwayAngularAcceleration = ezVec3(12, 23, 34);
      spring->m_Settings.m_vForceApplicationOffset = ezVec3(0.1f, 0.2f, 0.3f);
      spring->ResetSimulation();
      Tick(5);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().IsEqual(ezVec3(0.4f, 0, 1), 0.001f));
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter writer(&storage);
      ezWorldWriter worldWriter;
      worldWriter.WriteWorld(writer, world);
      ezMemoryStreamReader reader(&storage);
      ezWorldReader worldReader;
      EZ_TEST_BOOL(worldReader.ReadWorldDescription(reader).Succeeded());
      ezWorldDesc restoredDesc("SpringBoneRestored");
      ezWorld restoredWorld(restoredDesc);
      EZ_LOCK(restoredWorld.GetWriteMarker());
      restoredWorld.SetWorldSimulationEnabled(false);
      worldReader.InstantiateWorld(restoredWorld);
      restoredWorld.Update();
      auto it = restoredWorld.GetComponentManager<ezSpringBoneComponentManager>()->GetComponents();
      EZ_TEST_BOOL(it.IsValid());
      EZ_TEST_STRING(it->GetRootBone(), "Spring");
      EZ_TEST_BOOL(it->GetSkeleton() == skeleton);
      EZ_TEST_INT(it->m_BoneOverrides.GetCount(), 1);
      EZ_TEST_BOOL(!it->m_BoneOverrides[0].m_bEnabled);
      EZ_TEST_FLOAT(it->m_BoneOverrides[0].m_Settings.m_fStiffness, 11, 0);
      EZ_TEST_BOOL(it->m_Settings == spring->m_Settings);
      EZ_TEST_BOOL(it->m_BoneOverrides[0].m_Settings == spring->m_BoneOverrides[0].m_Settings);
      EZ_TEST_BOOL(it->GetDynamicRTTI()->FindPropertyByName("Preset") == nullptr);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Live settings, angular sway without shapes and fixed base")
    {
      ezSkeletonResourceDescriptor emptyDesc;
      builder.BuildSkeleton(emptyDesc.m_Skeleton);
      emptyDesc.m_Skeleton.m_BoneDirection = ezBasisAxis::PositiveX;
      const auto emptySkeleton = ezResourceManager::CreateResource<ezSkeletonResource>("SpringBoneEmptySkeleton", std::move(emptyDesc));
      spring->SetSkeleton(emptySkeleton);
      spring->SetIncludeDescendants(false);
      spring->m_BoneOverrides.Clear();
      spring->m_Settings = ezSpringBoneSettings();
      spring->m_Settings.m_fGravityFactor = 0;
      spring->m_Settings.m_fWindInfluence = 0;
      spring->m_Settings.m_fDamping = 1;
      spring->m_Settings.m_fSwayFrequency = 0;
      spring->m_Settings.m_vSwayFrequencyScale = ezVec3(1);
      spring->m_Settings.m_vSwayAxisPhase.SetZero();
      spring->m_Settings.m_fSwayBonePhase = 0;
      spring->m_Settings.m_fSwayPhase = ezMath::Pi<float>() * 0.5f;
      world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / 60));
      Tick(1);
      JPH::BodyIDVector originalBodies;
      jolt->GetJoltSystem()->GetBodies(originalBodies);
      // A pinned bone with no geometry must respond to wind from rest, without an impulse,
      // owner movement, sway or a manually configured force offset.
      spring->m_Settings.m_fStiffness = 1;
      spring->m_Settings.m_fWindFlutter = 0;
      spring->m_Settings.m_fWindInfluence = 1;
      auto* startupWind = world.GetModule<ezSimpleWindWorldModule>();
      startupWind->SetFallbackWind(ezVec3(0, 0, 20));
      Tick(6);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.001f);
      spring->m_Settings.m_fWindInfluence = 0;
      Tick(240);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1, 0.001f);
      spring->m_Settings.m_fWindInfluence = 1;
      Tick(6);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.001f);
      startupWind->SetFallbackWind(ezVec3::MakeZero());
      Tick(240);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1, 0.001f);
      startupWind->SetFallbackWind(ezVec3(0, 0, 20));
      Tick(6);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.001f);
      spring->m_Settings.m_fWindInfluence = 0;
      startupWind->SetFallbackWind(ezVec3::MakeZero());
      spring->m_Settings.m_fStiffness = 4;
      Tick(180);
      spring->m_Settings.m_vSwayAngles = ezVec3(0, 25, 0);
      Tick(120); // No reset: edit the actual public property, as the editor mirror does.
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z < 0.9f);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[1].GetTranslationVector().IsEqual(ezVec3(0, 0, 1), 0.001f));
      spring->m_Settings.m_bLockRotationY = true;
      Tick(90);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1.0f, 0.01f);
      spring->m_Settings.m_bLockRotationY = false;
      spring->m_Settings.m_MaxAngle = ezAngle::MakeFromDegree(10);
      Tick(90);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 0.92f);
      spring->m_Settings.m_vSwayAngles.SetZero();
      spring->m_Settings.m_fStiffness = 8;
      spring->m_Settings.m_fMass = 0.25f;
      spring->m_Settings.m_fAirDrag = 1;
      Tick(120);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1.0f, 0.01f);

      spring->m_Settings.m_fStiffness = 1;
      spring->m_Settings.m_MaxAngle = ezAngle::MakeFromDegree(60);
      spring->m_Settings.m_vSwayAngularAcceleration = ezVec3(0, 300, 0);
      Tick(120);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z < 0.98f);
      spring->m_Settings.m_vSwayAngularAcceleration.SetZero();
      spring->m_Settings.m_vSwayAcceleration = ezVec3(0, 0, 20);
      Tick(180);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1.0f, 0.01f);
      spring->m_Settings.m_vForceApplicationOffset = ezVec3(0.2f, 0, 0);
      Tick(120);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.02f);
      spring->m_Settings.m_vSwayAcceleration.SetZero();
      spring->m_Settings.m_fWindInfluence = 1;
      world.GetModule<ezSimpleWindWorldModule>()->SetFallbackWind(ezVec3(0, 0, 20));
      Tick(120);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > 1.02f);
      spring->m_Settings.m_fWindInfluence = 0;
      Tick(180);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1.0f, 0.01f);
      world.GetModule<ezSimpleWindWorldModule>()->SetFallbackWind(ezVec3::MakeZero());

      // Override edits and their removal must affect the live simulation, too.
      auto& entry = spring->m_BoneOverrides.ExpandAndGetRef();
      entry.m_sBone = "Spring";
      entry.m_Settings = spring->m_Settings;
      entry.m_Settings.m_vSwayAngles = ezVec3(0, 20, 0);
      Tick(120);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z < 0.92f);
      entry.m_Settings.m_fInfluence = 0;
      Tick(1);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1.0f, 0.001f);
      spring->m_BoneOverrides.Clear();
      Tick(180);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1.0f, 0.01f);

      JPH::BodyIDVector finalBodies;
      jolt->GetJoltSystem()->GetBodies(finalBodies);
      EZ_TEST_BOOL(originalBodies == finalBodies); // No hidden recreation during any of these edits.
    }

    TestSpringBoneEditorMirror(*spring, world);

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Angular sway with Bone Shapes, wind and phase continuity")
    {
      spring->SetSkeleton(skeleton);
      spring->m_Settings.m_vForceApplicationOffset.SetZero();
      spring->m_Settings.m_fStiffness = 4;
      spring->m_Settings.m_vSwayAngles = ezVec3(0, 25, 0);
      spring->m_Settings.m_fSwayFrequency = 0;
      ezVec3 previous = ezVec3::MakeZero();
      for (ezUInt32 rate : {30u, 60u, 120u})
      {
        world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / rate));
        spring->ResetSimulation();
        Tick(rate * 3);
        const auto tip = spring->GetSimulatedPose()[2].GetTranslationVector();
        EZ_TEST_BOOL(tip.z < 0.9f);
        if (!previous.IsZero())
          EZ_TEST_BOOL(tip.IsEqual(previous, 0.015f));
        previous = tip;
      }
      // A frequency change at the wave peak must not jump to world-time-derived phase.
      spring->m_Settings.m_fSwayFrequency = 0.25f;
      Tick(1);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().IsEqual(previous, 0.002f));
      Tick(120);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].GetTranslationVector().z > previous.z + 0.08f);
      spring->m_Settings.m_fWindInfluence = 1;
      world.GetModule<ezSimpleWindWorldModule>()->SetFallbackWind(ezVec3(0, 0, 4));
      Tick(240);
      EZ_TEST_BOOL(spring->GetSimulatedPose()[2].IsValid());
      spring->m_Settings.m_vSwayAngles.SetZero();
      spring->m_Settings.m_fWindInfluence = 0;
      Tick(360);
      EZ_TEST_FLOAT(spring->GetSimulatedPose()[2].GetTranslationVector().z, 1, 0.01f);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Version 1 settings and graph migration")
    {
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter writer(&storage);
      ezSpringBoneSettings legacy;
      legacy.m_fStiffness = 17;
      legacy.m_fSwayPhase = 2;
      legacy.m_vSwayAcceleration = ezVec3(1, 2, 3);
      // Exact version 1 settings layout, followed by a sentinel from the containing component.
      writer << legacy.m_fMass << legacy.m_fStiffness << legacy.m_fDamping << legacy.m_fRootStiffness << legacy.m_fLengthStiffness << legacy.m_fSoftening;
      writer << legacy.m_fGravityFactor << legacy.m_fAirDrag << legacy.m_fFriction << legacy.m_fInfluence << legacy.m_fWindInfluence << legacy.m_fWindFlutter;
      writer << legacy.m_fMaxDistance << legacy.m_MaxAngle << legacy.m_fMaxMotorForce << legacy.m_vExternalAcceleration << legacy.m_vSwayAcceleration << legacy.m_fSwayFrequency << legacy.m_fSwayPhase;
      writer << legacy.m_bLockRotationX << legacy.m_bLockRotationY << legacy.m_bLockRotationZ;
      writer << ezUInt32(0xAABBCCDD);
      ezMemoryStreamReader reader(&storage);
      ezSpringBoneSettings restored;
      restored.Deserialize(reader, 1);
      EZ_TEST_BOOL(restored == legacy);
      ezUInt32 sentinel;
      reader >> sentinel;
      EZ_TEST_INT(sentinel, 0xAABBCCDD);
      ezAbstractObjectGraph graph;
      auto* node = graph.AddNode(ezUuid::MakeUuid(), "ezSpringBoneComponent", 1);
      node->AddProperty("Preset", 5);
      node->AddProperty("Settings", ezUuid::MakeUuid());
      const ezVariant settingsID = node->FindProperty("Settings")->m_Value;
      ezAbstractObjectGraph types;
      auto* type = types.AddNode(ezUuid::MakeUuid(), "ezReflectedTypeDescriptor", 1);
      type->AddProperty("TypeName", "ezSpringBoneComponent");
      type->AddProperty("ParentTypeName", "ezComponent");
      type->AddProperty("TypeVersion", ezUInt32(1));
      ezGraphVersioning::GetSingleton()->PatchGraph(&graph, &types);
      EZ_TEST_BOOL(node->FindProperty("Preset") == nullptr);
      EZ_TEST_BOOL(node->FindProperty("Settings")->m_Value == settingsID);
      EZ_TEST_INT(node->GetTypeVersion(), 2);
    }
  }
  ezStartup::ShutdownHighLevelSystems();
  ezResourceManager::FreeAllUnusedResources();
  device->Shutdown().IgnoreResult();
  EZ_DEFAULT_DELETE(device);
  ezFileSystem::RemoveDataDirectoryGroup("SpringBoneTests");
  ezStartup::ShutdownCoreSystems();
}
