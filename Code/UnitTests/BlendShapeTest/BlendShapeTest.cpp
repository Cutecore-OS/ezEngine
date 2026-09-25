#include <GameEngine/Animation/Skeletal/BlendShapeComponent.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <BlendShapeTest/BlendShapeTestPCH.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeImport.h>
#include <Foundation/IO/MemoryStream.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <ModelImporter2/ModelImporter.h>
#include <QApplication>
#include <QVariant>
#include <RendererCore/AnimationSystem/AnimGraph/Nodes/Blending/LerpPosesAnimNode.h>
#include <RendererCore/AnimationSystem/SkeletonBuilder.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>
#include <limits>

// The linked asset managers register Qt icons during core startup.
EZ_TESTFRAMEWORK_ENTRY_POINT_CODE_INJECTION
EZ_APPLICATION_ENTRY_POINT_CODE_INJECTION
int main(int argc, char** argv)
{
  QApplication application(argc, argv);
  application.setProperty("Shared", 1);
  ezTestSetup::InitTestFramework("BlendShapeTest", "Blend Shape Tests", argc, const_cast<const char**>(argv));
  while (ezTestSetup::RunTests() == ezTestAppRun::Continue)
  {
  }
  const ezInt32 failures = ezTestSetup::GetFailedTestCount();
  ezTestSetup::DeInitTestFramework();
  return failures;
}

EZ_CREATE_SIMPLE_TEST_GROUP(BlendShapes);


EZ_CREATE_SIMPLE_TEST(BlendShapes, ImportAndDeformation)
{
  ezBlendShapeResourceDescriptor desc;
  ezStringBuilder file(BLEND_SHAPE_TEST_DATA, "/Morph.gltf");
  EZ_TEST_BOOL(ezImportBlendShapeMesh(file, true, desc).Succeeded());
  if (desc.m_Targets.GetCount() != 2)
  {
    EZ_TEST_INT(desc.m_Targets.GetCount(), 2);
    return;
  }
  EZ_TEST_STRING(desc.m_Targets[0].m_sName.GetString(), "Face/Smile");
  EZ_TEST_STRING(desc.m_Targets[1].m_sName.GetString(), "Face/Blink");
  EZ_TEST_INT(desc.m_Targets[0].m_Deltas.GetCount(), 1);
  EZ_TEST_INT(desc.m_Targets[1].m_Deltas.GetCount(), 1);
  EZ_TEST_BOOL(desc.Validate().Succeeded());

  ezDynamicArray<ezVec3> positions, normals;
  ezDynamicArray<ezVec4> tangents;
  const float weights[] = {0.5f, -0.25f};
  desc.Deform(ezMakeArrayPtr(weights), positions, normals, tangents);
  EZ_TEST_FLOAT(positions[0].z, 0.5f, 0.00001f);
  EZ_TEST_FLOAT(positions[1].z, -0.5f, 0.00001f);
  EZ_TEST_FLOAT(positions[2].z, 0.0f, 0.00001f);
  const auto previous = positions;
  desc.Deform(ezMakeArrayPtr(weights), positions, normals, tangents);
  EZ_TEST_BOOL(previous == positions);
  const float zero[] = {0, 0};
  desc.Deform(ezMakeArrayPtr(zero), positions, normals, tangents);
  EZ_TEST_BOOL(positions.GetArrayPtr() == desc.m_Mesh.MeshBufferDesc().GetPositionData());
  for (ezUInt32 i = 0; i < normals.GetCount(); ++i)
  {
    EZ_TEST_BOOL(normals[i].IsNormalized());
    EZ_TEST_FLOAT(normals[i].Dot(tangents[i].GetAsVec3()), 0, 0.0001f);
  }
  EZ_TEST_BOOL(desc.m_Mesh.GetBounds().GetBox().Contains(ezVec3(1, 0, -2)));

  ezDefaultMemoryStreamStorage storage;
  ezMemoryStreamWriter writer(&storage);
  EZ_TEST_BOOL(desc.Save(writer).Succeeded());
  ezMemoryStreamReader reader(&storage);
  ezBlendShapeResourceDescriptor loaded;
  EZ_TEST_BOOL(loaded.Load(reader).Succeeded());
  EZ_TEST_INT(loaded.m_Targets.GetCount(), 2);
  loaded.Deform(ezMakeArrayPtr(weights), positions, normals, tangents);
  EZ_TEST_FLOAT(positions[0].z, 0.5f, 0.00001f);
  loaded.m_Targets[0].m_Deltas[0].m_uiVertex = 999;
  EZ_TEST_BOOL(loaded.Validate().Failed());
  loaded.m_Targets[0].m_Deltas[0].m_uiVertex = 0;
  loaded.m_Targets[1].m_sName = loaded.m_Targets[0].m_sName;
  EZ_TEST_BOOL(loaded.Validate().Failed());

  EZ_TEST_BOOL(ezImportBlendShapeMesh(file, false, loaded).Succeeded());
  EZ_TEST_INT(loaded.m_Targets.GetCount(), 0);
  EZ_TEST_BOOL(ezImportBlendShapeMesh(BLEND_SHAPE_TEST_DATA "/NodeDefaults.gltf", true, loaded).Succeeded());
  EZ_TEST_FLOAT(loaded.m_Targets[0].m_fDefaultWeight, 0.3f, 0.00001f);
  EZ_TEST_FLOAT(loaded.m_Targets[1].m_fDefaultWeight, -0.2f, 0.00001f);
  EZ_TEST_BOOL(ezImportBlendShapeMesh(BLEND_SHAPE_TEST_DATA "/Normal.gltf", true, loaded).Succeeded());
  EZ_TEST_FLOAT(loaded.m_Targets[0].m_Deltas[0].m_vNormal.z, 1.0f, 0.0001f);
}

EZ_CREATE_SIMPLE_TEST(BlendShapes, CurveInterpolation)
{
  ezStringBuilder file(BLEND_SHAPE_TEST_DATA, "/Morph.gltf");
  for (const char* animation : {"Linear", "Step", "Cubic"})
  {
    ezAnimationClipResourceDescriptor desc;
    const auto status = ezReadBlendShapeGltfCurves(file, animation, desc);
    EZ_TEST_BOOL_MSG(status.Succeeded(), "%s", status.GetMessageString().GetData());
    if (desc.m_CustomCurves.GetCount() != 2)
    {
      EZ_TEST_INT(desc.m_CustomCurves.GetCount(), 2);
      continue;
    }
    EZ_TEST_STRING(desc.m_CustomCurves[0].m_sName.GetString(), "BlendShape/Face/Smile");
    const auto& smile = desc.m_CustomCurves[0].m_Curve;
    if (ezStringUtils::IsEqual(animation, "Linear"))
    {
      EZ_TEST_DOUBLE(smile.Evaluate(0.25), 0.25, 0.001);
      EZ_TEST_DOUBLE(desc.m_CustomCurves[1].m_Curve.Evaluate(0.5), -0.5, 0.001);
    }
    else if (ezStringUtils::IsEqual(animation, "Step"))
    {
      EZ_TEST_DOUBLE(smile.Evaluate(0.9999), 0, 0.001);
      EZ_TEST_DOUBLE(smile.Evaluate(1.0), 1, 0.001);
    }
    else
    {
      // Hermite: p0=0, p1=1, outgoing slope=2, incoming slope=0.
      EZ_TEST_DOUBLE(smile.Evaluate(0.5), 0.75, 0.005);
    }
  }
  ezAnimationClipResourceDescriptor glb;
  file = BLEND_SHAPE_TEST_DATA "/Morph.glb";
  EZ_TEST_BOOL(ezReadBlendShapeGltfCurves(file, "Cubic", glb).Succeeded());
  EZ_TEST_INT(glb.m_CustomCurves.GetCount(), 2);
  ezAnimationClipResourceDescriptor invalid;
  file = BLEND_SHAPE_TEST_DATA "/Invalid.gltf";
  EZ_TEST_BOOL(ezReadBlendShapeGltfCurves(file, "Linear", invalid).Failed());
}

EZ_CREATE_SIMPLE_TEST(BlendShapes, Weights)
{
  EZ_TEST_FLOAT(ezBlendShapeResourceDescriptor::SanitizeWeight(0.0001f, 0.001f), 0, 0);
  EZ_TEST_FLOAT(ezBlendShapeResourceDescriptor::SanitizeWeight(-0.5f, 0.001f), -0.5f, 0);
  EZ_TEST_FLOAT(ezBlendShapeResourceDescriptor::SanitizeWeight(3.0f, 0), 1, 0);
  EZ_TEST_FLOAT(ezBlendShapeResourceDescriptor::SanitizeWeight(std::numeric_limits<float>::quiet_NaN(), 0), 0, 0);
  EZ_TEST_FLOAT(ezBlendShapeResourceDescriptor::SanitizeWeight(std::numeric_limits<float>::infinity(), 0), 0, 0);
  ezBlendShapePoseComponent component;
  component.SetWeight("Face/Smile", 0.4f);
  EZ_TEST_FLOAT(component.GetWeight("Face/Smile"), 0.4f, 0);
  component.RemoveWeight("Face/Smile");
  EZ_TEST_FLOAT(component.GetWeight("Face/Smile"), 0, 0);
  component.SetWeight("Face/Smile", -0.7f);
  component.ResetWeights();
  EZ_TEST_FLOAT(component.GetWeight("Face/Smile"), 0, 0);
}

#include <Core/World/World.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Threading/Lock.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <RendererCore/Pipeline/RenderDataManager.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Device/DeviceFactory.h>

EZ_CREATE_SIMPLE_TEST(BlendShapes, GPUInstanceIsolation)
{
  ezGALDeviceCreationDescription deviceDesc;
#ifdef BUILDSYSTEM_ENABLE_VULKAN_SUPPORT
  constexpr const char* szDefaultRenderer = "Vulkan";
#else
  constexpr const char* szDefaultRenderer = "DX11";
#endif
  const ezStringView renderer = ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-renderer", 0, szDefaultRenderer);
  ezGALDevice* pDevice = ezGALDeviceFactory::CreateDevice(renderer, ezFoundation::GetDefaultAllocator(), deviceDesc);
  EZ_TEST_BOOL(pDevice != nullptr);
  if (pDevice == nullptr)
    return;
  if (!EZ_TEST_BOOL(pDevice->Init().Succeeded()))
  {
    EZ_DEFAULT_DELETE(pDevice);
    return;
  }
  ezGALDevice::SetDefaultDevice(pDevice);
  {
    ezBlendShapeResourceDescriptor desc;
    ezModelImporter2::ImportOptions options;
    options.m_sSourceFile = BLEND_SHAPE_TEST_DATA "/Skinned.gltf";
    options.m_bImportSkinningData = true;
    options.m_bHighPrecision = true;
    options.m_bRecomputeTangents = true;
    EZ_TEST_BOOL(ezImportBlendShapeMesh(options, desc).Succeeded());
    for (ezUInt32 i = 0; i < desc.m_Mesh.GetMaterials().GetCount(); ++i)
      desc.m_Mesh.SetMaterial(i, "");
    ezSkeletonResourceDescriptor skeletonDesc;
    ezSkeletonBuilder builder;
    builder.AddJoint("Root", ezTransform::MakeIdentity());
    builder.BuildSkeleton(skeletonDesc.m_Skeleton);
    auto skeleton = ezResourceManager::CreateResource<ezSkeletonResource>("BlendShapeTestSkeleton", std::move(skeletonDesc));
    desc.m_Mesh.m_hDefaultSkeleton = skeleton;
    ezMeshResourceDescriptor meshDesc = desc.m_Mesh;
    auto resource = ezResourceManager::CreateResource<ezBlendShapeResource>("BlendShapeTestTargets", std::move(desc));
    meshDesc.m_sDefaultBlendShapes = resource.GetResourceID();
    auto mesh = ezResourceManager::CreateResource<ezMeshResource>("BlendShapeTestAnimatedMesh", std::move(meshDesc));
    ezWorldDesc worldDesc("BlendShapeTestWorld");
    ezWorld world(worldDesc);
    EZ_LOCK(world.GetWriteMarker());
    world.GetOrCreateModule<ezRenderDataManager>();
    ezGameObject* pA = nullptr;
    ezGameObject* pB = nullptr;
    ezGameObjectDesc object;
    world.CreateObject(object, pA);
    world.CreateObject(object, pB);
    ezAnimatedMeshComponent* pMeshA = nullptr;
    ezAnimatedMeshComponent* pMeshB = nullptr;
    ezAnimatedMeshComponent::CreateComponent(pA, pMeshA);
    ezAnimatedMeshComponent::CreateComponent(pB, pMeshB);
    pMeshA->SetMesh(mesh);
    pMeshB->SetMesh(mesh);
    ezBlendShapePoseComponent* pPoseA = nullptr;
    ezBlendShapePoseComponent* pPoseB = nullptr;
    ezBlendShapePoseComponent::CreateComponent(pA, pPoseA);
    ezBlendShapePoseComponent::CreateComponent(pB, pPoseB);
    world.SetWorldSimulationEnabled(false);
    world.Update();
    pDevice->BeginFrame();
    pDevice->EndFrame();
    auto* pDeformerA = static_cast<ezBlendShapeDeformer*>(pMeshA->GetDeformer());
    auto* pDeformerB = static_cast<ezBlendShapeDeformer*>(pMeshB->GetDeformer());
    EZ_TEST_BOOL(pDeformerA != nullptr && pDeformerB != nullptr);
    // A neutral instance must not allocate an extra GPU mesh or run deformation.
    EZ_TEST_BOOL(!pDeformerA->GetDeformedBuffer().IsValid());
    EZ_TEST_BOOL(!pDeformerB->GetDeformedBuffer().IsValid());
    EZ_TEST_INT(pDeformerA->GetDeformationRevision(), 0);
    EZ_TEST_INT(pDeformerB->GetDeformationRevision(), 0);
    pPoseA->SetWeight("Face/Smile", 0.5f);
    pPoseB->SetWeight("Face/Smile", -0.25f);
    world.Update();
    EZ_TEST_BOOL(pDeformerA->GetDeformedBuffer() != pDeformerB->GetDeformedBuffer());
    {
      ezResourceLock<ezDynamicMeshBufferResource> a(pDeformerA->GetDeformedBuffer(), ezResourceAcquireMode::BlockTillLoaded);
      ezResourceLock<ezDynamicMeshBufferResource> b(pDeformerB->GetDeformedBuffer(), ezResourceAcquireMode::BlockTillLoaded);
      EZ_TEST_FLOAT(a->AccessPositionData()[0].z, 0.5f, 0.0001f);
      EZ_TEST_FLOAT(b->AccessPositionData()[0].z, -0.25f, 0.0001f);
    }
    EZ_TEST_BOOL(pMeshA->GetMesh() == pMeshB->GetMesh());
    EZ_TEST_FLOAT(pDeformerA->GetWeight("Face/Smile"), 0.5f, 0);
    EZ_TEST_FLOAT(pDeformerB->GetWeight("Face/Smile"), -0.25f, 0);
    const ezUInt32 revisionA = pDeformerA->GetDeformationRevision();
    const ezUInt32 revisionB = pDeformerB->GetDeformationRevision();
    EZ_TEST_BOOL(revisionA > 0 && revisionB > 0);
    world.Update();
    EZ_TEST_INT(pDeformerA->GetDeformationRevision(), revisionA);
    EZ_TEST_INT(pDeformerB->GetDeformationRevision(), revisionB);
    pPoseA->SetWeight("Face/Smile", 0.8f);
    world.Update();
    EZ_TEST_INT(pDeformerA->GetDeformationRevision(), revisionA + 1);
    EZ_TEST_INT(pDeformerB->GetDeformationRevision(), revisionB);
    pPoseA->SetWeight("Face/Smile", 0.00001f);
    world.Update();
    EZ_TEST_FLOAT(pDeformerA->GetWeight("Face/Smile"), 0, 0);
    EZ_TEST_INT(pDeformerA->GetDeformationRevision(), revisionA + 1);
    EZ_TEST_BOOL(pDeformerA->CreateRenderData(world.GetModule<ezRenderDataManager>()) == nullptr);
    ezMsgAnimationCurveValue message;
    message.m_sCurveName.Assign("BlendShape/Face/Smile");
    message.m_fAverage = 0.6f;
    pA->SendMessage(message);
    world.Update();
    EZ_TEST_FLOAT(pDeformerA->GetWeight("Face/Smile"), 0, 0);
    pPoseA->RemoveWeight("Face/Smile");
    world.Update();
    EZ_TEST_FLOAT(pDeformerA->GetWeight("Face/Smile"), 0.6f, 0.0001f);
    ezDefaultMemoryStreamStorage sceneStorage;
    ezMemoryStreamWriter sceneWriter(&sceneStorage);
    ezWorldWriter worldWriter;
    worldWriter.WriteWorld(sceneWriter, world);
    ezMemoryStreamReader sceneReader(&sceneStorage);
    ezWorldReader worldReader;
    EZ_TEST_BOOL(worldReader.ReadWorldDescription(sceneReader).Succeeded());
    ezWorldDesc restoredDesc("RestoredBlendShapeWorld");
    ezWorld restored(restoredDesc);
    {
      EZ_LOCK(restored.GetWriteMarker());
      restored.GetOrCreateModule<ezRenderDataManager>();
      worldReader.InstantiateWorld(restored);
      restored.Update();
      ezUInt32 uiMeshes = 0;
      for (auto it = restored.GetObjects(); it.IsValid(); ++it)
      {
        ezAnimatedMeshComponent* pMesh = nullptr;
        if (it->TryGetComponentOfBaseType(pMesh))
        {
          EZ_TEST_BOOL(pMesh->GetMesh() == mesh);
          EZ_TEST_BOOL(pMesh->GetMesh() == pMeshA->GetMesh());
          ++uiMeshes;
        }
      }
      EZ_TEST_INT(uiMeshes, 2);
    }
    ezResourceLock<ezBlendShapeResource> base(resource, ezResourceAcquireMode::BlockTillLoaded);
    EZ_TEST_FLOAT(base->GetDescriptor().m_Mesh.MeshBufferDesc().GetPosition(0).z, 0, 0);
  }
  ezResourceManager::FreeAllUnusedResources();
  pDevice->BeginFrame();
  pDevice->EndFrame();
  pDevice->Shutdown().IgnoreResult();
  EZ_DEFAULT_DELETE(pDevice);
  ezGALDevice::SetDefaultDevice(nullptr);
}

#include <RendererCore/AnimationSystem/AnimGraph/Nodes/BlendShapes/BlendShapeAnimNode.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimController.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraph.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraphInstance.h>
#include <RendererCore/AnimationSystem/AnimGraph/Nodes/Pose/RestPoseAnimNode.h>

class ezBlendShapeTestSink : public ezAnimGraphNode
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeTestSink, ezAnimGraphNode);

public:
  ezAnimGraphLocalPoseInputPin m_InPose;
  mutable float m_fValue = -100;
  mutable ezUInt32 m_uiCount = 0;

protected:
  virtual ezResult SerializeNode(ezStreamWriter& stream) const override
  {
    EZ_SUCCEED_OR_RETURN(SUPER::SerializeNode(stream));
    return m_InPose.Serialize(stream);
  }
  virtual ezResult DeserializeNode(ezStreamReader& stream) override
  {
    EZ_SUCCEED_OR_RETURN(SUPER::DeserializeNode(stream));
    return m_InPose.Deserialize(stream);
  }
  virtual void Step(ezAnimController& controller, ezAnimGraphInstance& graph, ezTime diff, const ezSkeletonResource*, ezGameObject*) const override
  {
    const auto* pose = m_InPose.GetPose(controller, graph);
    if (pose == nullptr)
      return;
    m_uiCount = pose->m_CustomCurveValues.GetCount();
    for (const auto& value : pose->m_CustomCurveValues)
      if (value.m_sName.GetView() == "BlendShape/Face/Smile")
        m_fValue = value.m_fValue;
  }
};
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeTestSink, 1, ezRTTIDefaultAllocator<ezBlendShapeTestSink>)
  {
    EZ_BEGIN_PROPERTIES
    {
      EZ_MEMBER_PROPERTY("InPose", m_InPose),
    } EZ_END_PROPERTIES;
  }
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_CREATE_SIMPLE_TEST(BlendShapes, AnimationGraph)
{
  for (bool bMissing : {false, true})
  {
    ezAnimGraph graph;
    auto* pRest = graph.AddNode(EZ_DEFAULT_NEW(ezRestPoseAnimNode));
    auto pA = EZ_DEFAULT_NEW(ezBlendShapeWeightAnimNode);
    pA->m_sShape = "Face/Smile";
    pA->m_fWeight = 0.2f;
    auto* a = graph.AddNode(std::move(pA));
    auto pB = EZ_DEFAULT_NEW(ezBlendShapeWeightAnimNode);
    pB->m_sShape = bMissing ? "" : "Face/Smile";
    pB->m_fWeight = 0.8f;
    auto* b = graph.AddNode(std::move(pB));
    auto* blend = graph.AddNode(EZ_DEFAULT_NEW(ezLerpPosesAnimNode));
    auto* count = static_cast<const ezTypedMemberProperty<ezUInt8>*>(blend->GetDynamicRTTI()->FindPropertyByName("PosesCount"));
    count->SetValue(blend, 2);
    auto* sink = static_cast<ezBlendShapeTestSink*>(graph.AddNode(EZ_DEFAULT_NEW(ezBlendShapeTestSink)));
    graph.AddConnection(pRest, "OutPose", a, "InPose");
    graph.AddConnection(pRest, "OutPose", b, "InPose");
    graph.AddConnection(a, "OutPose", blend, "InPoses[0]");
    graph.AddConnection(b, "OutPose", blend, "InPoses[1]");
    graph.AddConnection(blend, "OutPose", sink, "InPose");
    graph.PrepareForUse();
    ezAnimGraphInstance instance;
    instance.Configure(graph);
    ezAnimPoseGenerator pose;
    ezAnimController controller;
    controller.Initialize({}, pose);
    instance.Update(controller, ezTime::MakeFromSeconds(0.016), nullptr, nullptr);
    EZ_TEST_INT(sink->m_uiCount, 1);
    EZ_TEST_FLOAT(sink->m_fValue, bMissing ? 0.1f : 0.5f, 0.00001f);
  }
}

#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>


EZ_CREATE_SIMPLE_TEST(BlendShapes, NativeClips)
{
  ezAnimationClipAssetProperties properties;
  properties.m_sSourceFile = BLEND_SHAPE_TEST_DATA "/Skinned.gltf";
  properties.m_sAnimationClipToExtract = "Linear";
  auto& curve = properties.m_BlendShapes.ExpandAndGetRef();
  curve.m_sName = "Face/Smile";
  curve.m_bOverrideSource = true;
  auto& key = curve.m_Curve.m_ControlPoints.ExpandAndGetRef();
  key.m_iTick = 0;
  key.m_fValue = 0.25;
  ezAnimationClipResourceDescriptor desc;
  auto importer = ezModelImporter2::RequestImporterForFileType(properties.m_sSourceFile);
  ezModelImporter2::ImportOptions options;
  options.m_sSourceFile = properties.m_sSourceFile;
  options.m_pAnimationOutput = &desc;
  options.m_sAnimationToImport = properties.m_sAnimationClipToExtract;
  EZ_TEST_BOOL(importer->Import(options).Succeeded());
  const auto* joint = desc.GetJointInfo(ezTempHashedString("Root"));
  EZ_TEST_BOOL(joint != nullptr);
  const ezUInt32 jointCount = desc.GetNumJoints();
  auto& ordinary = desc.m_CustomCurves.ExpandAndGetRef();
  ordinary.m_sName.Assign("FootstepStrength");
  ordinary.m_Curve.AddControlPoint(0).m_Position.y = 0.9;
  ordinary.m_Curve.SortControlPoints();
  ordinary.m_Curve.CreateLinearApproximation();
  ezDynamicArray<ezAnimationClipCurveData> editable;
  EZ_TEST_BOOL(ezMergeBlendShapeClipCurves(properties, properties.m_sSourceFile, desc, editable).Succeeded());
  EZ_TEST_INT(desc.GetNumJoints(), jointCount);
  EZ_TEST_INT(desc.m_CustomCurves.GetCount(), 3);
  EZ_TEST_INT(editable.GetCount(), 2);
  for (const auto& c : desc.m_CustomCurves)
  {
    if (c.m_sName.GetView() == "BlendShape/Face/Smile")
      EZ_TEST_DOUBLE(c.m_Curve.Evaluate(0.5), 0.25, 0.001);
    if (c.m_sName.GetView() == "BlendShape/Face/Blink")
      EZ_TEST_DOUBLE(c.m_Curve.Evaluate(0.5), -0.5, 0.001);
    if (c.m_sName.GetView() == "FootstepStrength")
      EZ_TEST_DOUBLE(c.m_Curve.Evaluate(0.5), 0.9, 0.001);
  }
  ezDefaultMemoryStreamStorage storage;
  ezMemoryStreamWriter writer(&storage);
  EZ_TEST_BOOL(desc.Serialize(writer).Succeeded());
  ezMemoryStreamReader reader(&storage);
  ezAnimationClipResourceDescriptor loaded;
  EZ_TEST_BOOL(loaded.Deserialize(reader).Succeeded());
  EZ_TEST_INT(loaded.GetNumJoints(), jointCount);
  EZ_TEST_INT(loaded.m_CustomCurves.GetCount(), 3);
  // Disabling an override restores the source channel without touching the GLB.
  properties.m_BlendShapes[0].m_bOverrideSource = false;
  desc.m_CustomCurves.Clear();
  EZ_TEST_BOOL(ezMergeBlendShapeClipCurves(properties, properties.m_sSourceFile, desc, editable).Succeeded());
  EZ_TEST_DOUBLE(desc.m_CustomCurves[0].m_Curve.Evaluate(0.5), 0.5, 0.001);
  EZ_TEST_BOOL(!editable[0].m_bOverrideSource);
  EZ_TEST_BOOL(editable[0].m_Curve.m_ControlPoints.GetCount() >= 2);
  properties.m_bImportBlendShapes = false;
  desc.m_CustomCurves.Clear();
  EZ_TEST_BOOL(ezMergeBlendShapeClipCurves(properties, properties.m_sSourceFile, desc, editable).Succeeded());
  EZ_TEST_INT(desc.m_CustomCurves.GetCount(), 0);
  EZ_TEST_INT(desc.GetNumJoints(), jointCount);
  properties.m_BlendShapes[0].m_bOverrideSource = true;
  properties.m_BlendShapes.PushBack(properties.m_BlendShapes[0]);
  EZ_TEST_BOOL(ezMergeBlendShapeClipCurves(properties, properties.m_sSourceFile, desc, editable).Failed());
}

EZ_CREATE_SIMPLE_TEST(BlendShapes, NativeVertexCorrespondence)
{
  ezModelImporter2::ImportOptions options;
  const ezStringView source = ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-blendShapeSource");
  options.m_sSourceFile = source.IsEmpty() ? ezStringView(BLEND_SHAPE_TEST_DATA "/Skinned.gltf") : source;
  options.m_bImportSkinningData = true;
  options.m_bHighPrecision = true;
  options.m_bRecomputeTangents = true;
  ezBlendShapeResourceDescriptor shapes;
  EZ_TEST_BOOL(ezImportBlendShapeMesh(options, shapes).Succeeded());
  ezMeshResourceDescriptor native;
  options.m_pMeshOutput = &native;
  auto importer = ezModelImporter2::RequestImporterForFileType(options.m_sSourceFile);
  EZ_TEST_BOOL(importer->Import(options).Succeeded());
  EZ_TEST_INT(shapes.m_Mesh.m_Bones.GetCount(), native.m_Bones.GetCount());
  if (source.IsEmpty())
  {
    EZ_TEST_INT(shapes.m_Mesh.m_Bones.GetCount(), 1);
    EZ_TEST_BOOL(shapes.m_Mesh.m_Bones.Contains(ezTempHashedString("Root")));
  }
  const auto& a = shapes.m_Mesh.MeshBufferDesc();
  const auto& b = native.MeshBufferDesc();
  for (ezUInt32 i = 0; i < ezMeshVertexStreamType::DataOffsets; ++i)
    EZ_TEST_BOOL_MSG(a.GetVertexBufferData(static_cast<ezMeshVertexStreamType::Enum>(i)) == b.GetVertexBufferData(static_cast<ezMeshVertexStreamType::Enum>(i)), "Vertex stream %u differs", i);
  EZ_TEST_BOOL(a.GetIndexBufferData() == b.GetIndexBufferData());
  EZ_TEST_BOOL(!shapes.m_Targets.IsEmpty());
  ezDynamicArray<ezBlendShapeTarget> exposed;
  EZ_TEST_BOOL(ezReadBlendShapeGltfNames(options.m_sSourceFile, exposed, &options).Succeeded());
  EZ_TEST_INT(exposed.GetCount(), shapes.m_Targets.GetCount());
  for (const auto& target : shapes.m_Targets)
  {
    bool found = false;
    for (const auto& parameter : exposed)
      if (parameter.m_sName == target.m_sName)
      {
        found = true;
        EZ_TEST_FLOAT(parameter.m_fDefaultWeight, target.m_fDefaultWeight, 0.00001f);
      }
    EZ_TEST_BOOL_MSG(found, "Missing exposed shape %s", target.m_sName.GetString().GetData());
  }
  if (source.IsEmpty())
    EZ_TEST_INT(shapes.m_Targets.GetCount(), 2);
  // The optional resource reference survives the ordinary mesh binary format.
  native.m_sDefaultBlendShapes = "TestShapeAsset";
  ezDefaultMemoryStreamStorage storage;
  ezMemoryStreamWriter writer(&storage);
  native.Save(writer);
  ezMemoryStreamReader reader(&storage);
  ezMeshResourceDescriptor loaded;
  EZ_TEST_BOOL(loaded.Load(reader).Succeeded());
  EZ_TEST_STRING(loaded.m_sDefaultBlendShapes, "TestShapeAsset");
  // Loading a legacy/plain mesh into the same descriptor must clear the optional reference.
  native.m_sDefaultBlendShapes.Clear();
  ezDefaultMemoryStreamStorage plainStorage;
  ezMemoryStreamWriter plainWriter(&plainStorage);
  native.Save(plainWriter);
  ezMemoryStreamReader plainReader(&plainStorage);
  EZ_TEST_BOOL(loaded.Load(plainReader).Succeeded());
  EZ_TEST_BOOL(loaded.m_sDefaultBlendShapes.IsEmpty());
  EZ_TEST_INT(loaded.m_Bones.GetCount(), native.m_Bones.GetCount());
}

#include <RendererCore/AnimationSystem/AnimGraph/Nodes/Pose/SampleFrameAnimNode.h>

EZ_CREATE_SIMPLE_TEST(BlendShapes, SampleFrame)
{
  ezAnimationClipResourceDescriptor desc;
  EZ_TEST_BOOL(ezReadBlendShapeGltfCurves(BLEND_SHAPE_TEST_DATA "/Morph.gltf", "Linear", desc).Succeeded());
  desc.SetDuration(ezTime::MakeFromSeconds(1));
  desc.m_EventTrack.AddControlPoint(ezTime::MakeFromSeconds(0.5), "Marker");
  desc.m_vConstantRootMotion = ezVec3(1, 2, 3);
  desc.m_bAdditive = true;
  auto clip = ezResourceManager::CreateResource<ezAnimationClipResource>("BlendShapeFrameTestClip", std::move(desc));
  {
    ezResourceLock<ezAnimationClipResource> resource(clip, ezResourceAcquireMode::BlockTillLoaded);
    EZ_TEST_INT(resource->GetDescriptor().m_CustomCurves.GetCount(), 2);
    EZ_TEST_BOOL(resource->GetDescriptor().m_bAdditive);
    EZ_TEST_DOUBLE(resource->GetDescriptor().GetDuration().GetSeconds(), 1.0, 0.00001);
    ezDynamicArray<ezHashedString> events;
    resource->GetDescriptor().m_EventTrack.Sample(ezTime::MakeZero(), ezTime::MakeFromSeconds(1), events);
    if (EZ_TEST_INT(events.GetCount(), 1))
      EZ_TEST_STRING(events[0].GetString(), "Marker");
    EZ_TEST_VEC3(resource->GetDescriptor().m_vConstantRootMotion, ezVec3(1, 2, 3), 0.00001f);
  }
  ezAnimGraph graph;
  auto* sample = static_cast<ezSampleFrameAnimNode*>(graph.AddNode(EZ_DEFAULT_NEW(ezSampleFrameAnimNode)));
  sample->SetClip("Morph");
  sample->m_fNormalizedSamplePosition = 0.25f;
  auto* sink = static_cast<ezBlendShapeTestSink*>(graph.AddNode(EZ_DEFAULT_NEW(ezBlendShapeTestSink)));
  graph.AddConnection(sample, "OutPose", sink, "InPose");
  graph.PrepareForUse();
  ezAnimGraphInstance instance;
  instance.Configure(graph);
  ezAnimPoseGenerator pose;
  ezAnimController controller;
  controller.Initialize({}, pose);
  ezAnimController::AnimClipInfo info;
  info.m_hClip = clip;
  controller.SetAnimationClipInfo(sample->m_sClip, info);
  instance.Update(controller, ezTime::MakeFromSeconds(0.016), nullptr, nullptr);
  EZ_TEST_INT(sink->m_uiCount, 2);
  EZ_TEST_FLOAT(sink->m_fValue, 0.25f, 0.00001f);
}
