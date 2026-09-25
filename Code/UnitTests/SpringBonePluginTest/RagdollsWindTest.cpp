#include <Core/World/World.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/Serialization/DdlSerializer.h>
#include <Foundation/Serialization/RttiConverter.h>
#include <Foundation/Utilities/AssetFileHeader.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <GameEngine/Effects/Wind/WindVolumeComponent.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <SpringBonePlugin/Components/SpringBoneComponent.h>
#include <TestFramework/Framework/TestFramework.h>

// Opt-in diagnostic against the user's saved scene and transformed skeleton. Does not edit assets.
void TestRagdollsWind()
{
  if (!ezCommandLineUtils::GetGlobalInstance()->GetBoolOption("-ragdollsWind"))
    return;
  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Saved Ragdolls wind and Spring Bone settings")
  {
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Samples/Testing Chambers", "SpringBoneTests", "ragdolls").Succeeded());
    ezFileReader sceneFile;
    if (!EZ_TEST_BOOL(sceneFile.Open(":ragdolls/Scenes/Ragdolls.ezScene").Succeeded()))
      return;
    ezUniquePtr<ezAbstractObjectGraph> header, graph, types;
    if (!EZ_TEST_BOOL(ezAbstractGraphDdlSerializer::ReadDocument(sceneFile, header, graph, types, false).Succeeded()))
      return;
    ezFileReader skeletonFile;
    if (!EZ_TEST_BOOL(skeletonFile.Open(":ragdolls/AssetCache/Common/Animations/Mattress/Mattress.ezBinSkeleton").Succeeded()))
      return;
    ezAssetFileHeader assetHeader;
    EZ_TEST_BOOL(assetHeader.Read(skeletonFile).Succeeded());
    ezSkeletonResourceDescriptor desc;
    EZ_TEST_BOOL(desc.Deserialize(skeletonFile).Succeeded());
    const auto skeleton = ezResourceManager::CreateResource<ezSkeletonResource>("RagdollsWindSkeleton", std::move(desc));
    auto FindOwner = [&](const ezUuid& id) -> const ezAbstractObjectNode*
    {
      for (auto it = graph->GetAllNodes().GetIterator(); it.IsValid(); ++it)
        if (const auto* components = it.Value()->FindProperty("Components"))
          for (const auto& value : components->m_Value.Get<ezVariantArray>())
            if (value == id)
              return it.Value();
      return nullptr;
    };
    auto ObjectDesc = [](const ezAbstractObjectNode* node)
    {
      ezGameObjectDesc result;
      result.m_LocalPosition = node->FindProperty("LocalPosition")->m_Value.Get<ezVec3>();
      result.m_LocalRotation = node->FindProperty("LocalRotation")->m_Value.Get<ezQuat>();
      result.m_LocalScaling = node->FindProperty("LocalScaling")->m_Value.Get<ezVec3>();
      result.m_LocalScaling *= node->FindProperty("LocalUniformScaling")->m_Value.ConvertTo<float>();
      return result;
    };
    ezWorldDesc worldDesc("SavedRagdollsWind");
    ezWorld world(worldDesc);
    EZ_LOCK(world.GetWriteMarker());
    world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / 60.0));
    ezRttiConverterContext context;
    ezRttiConverterReader reader(graph.Borrow(), &context);
    ezDynamicArray<ezSpringBoneComponent*> springs;
    ezDynamicArray<ezWindVolumeConeComponent*> cones;
    for (auto it = graph->GetAllNodes().GetIterator(); it.IsValid(); ++it)
    {
      const auto* node = it.Value();
      if (node->GetType() != "ezSpringBoneComponent" && node->GetType() != "ezWindVolumeConeComponent")
        continue;
      const auto* ownerNode = FindOwner(node->GetGuid());
      if (!EZ_TEST_BOOL(ownerNode != nullptr))
        continue;
      auto objectDesc = ObjectDesc(ownerNode);
      objectDesc.m_bDynamic = true;
      ezGameObject* owner;
      world.CreateObject(objectDesc, owner);
      if (node->GetType() == "ezSpringBoneComponent")
      {
        ezSpringBoneComponent* spring;
        ezSpringBoneComponent::CreateComponent(owner, spring);
        spring->SetSkeleton(skeleton);
        spring->SetRootBone(node->FindProperty("RootBone")->m_Value.Get<ezString>());
        spring->SetIncludeDescendants(node->FindProperty("IncludeDescendants")->m_Value.Get<bool>());
        const auto* settings = graph->GetNode(node->FindProperty("Settings")->m_Value.Get<ezUuid>());
        reader.ApplyPropertiesToObject(settings, ezGetStaticRTTI<ezSpringBoneSettings>(), &spring->m_Settings);
        springs.PushBack(spring);
      }
      else
      {
        ezWindVolumeConeComponent* cone;
        ezWindVolumeConeComponent::CreateComponent(owner, cone);
        reader.ApplyPropertiesToObject(node, cone->GetDynamicRTTI(), cone);
        cone->SetActiveFlag(false);
        cones.PushBack(cone);
      }
    }
    for (ezUInt32 i = 0; i < 180; ++i)
      world.Update();
    ezDynamicArray<ezDynamicArray<ezMat4>> restingPoses;
    for (auto* spring : springs)
      restingPoses.ExpandAndGetRef() = spring->GetSimulatedPose();
    for (auto* cone : cones)
      cone->SetActiveFlag(true);
    for (ezUInt32 i = 0; i < 180; ++i)
      world.Update();
    ezResourceLock<ezSkeletonResource> resource(skeleton, ezResourceAcquireMode::BlockTillLoaded);
    const auto* wind = world.GetModuleReadOnly<ezWindWorldModuleInterface>();
    for (ezUInt32 springIndex = 0; springIndex < springs.GetCount(); ++springIndex)
    {
      auto* spring = springs[springIndex];
      const auto pose = spring->GetSimulatedPose();
      for (ezUInt32 i = 0; i < pose.GetCount(); ++i)
      {
        const auto& joint = resource->GetDescriptor().m_Skeleton.GetJointByIndex(i);
        if (!resource->GetDescriptor().m_Skeleton.IsJointDescendantOf(i, resource->GetDescriptor().m_Skeleton.FindJointByName(ezTempHashedString(spring->GetRootBone()))))
          continue;
        const auto position = spring->GetOwner()->GetGlobalTransform().TransformPosition(resource->GetDescriptor().m_RootTransform.TransformPosition(pose[i].GetTranslationVector()));
        const auto velocity = wind->GetWindAt(position);
        const float displacement = (pose[i].GetTranslationVector() - restingPoses[springIndex][i].GetTranslationVector()).GetLength();
        ezTestFramework::Output(ezTestOutput::Details, "%s at (%.3f, %.3f, %.3f): wind (%.3f, %.3f, %.3f), stiffness %.1f Hz, displacement %.5f m", joint.GetName().GetData(), position.x, position.y, position.z, velocity.x, velocity.y, velocity.z, spring->m_Settings.m_fStiffness, displacement);
      }
    }
  }
}
