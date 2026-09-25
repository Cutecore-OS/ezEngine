#include <Core/World/World.h>
#include <Core/World/SpatialSystem.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Basics.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/MemoryStream.h>
#include <GameEngine/Effects/Wind/SimpleWindWorldModule.h>
#include <GrassPlugin/Components/GrassComponent.h>
#include <GrassPlugin/Components/GrassPatchComponent.h>
#include <JoltPlugin/Actors/JoltDynamicActorComponent.h>
#include <JoltPlugin/Shapes/JoltShapeCapsuleComponent.h>
#include <RendererCore/Material/MaterialResource.h>
#include <RendererCore/Meshes/DynamicMeshBufferResource.h>
#include <RendererCore/Meshes/MeshResource.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>

EZ_TESTFRAMEWORK_ENTRY_POINT("GrassPluginTest", "Grass Plugin Tests")
EZ_CREATE_SIMPLE_TEST_GROUP(Grass);

void TestGrassMaterial(ezGALDevice* pDevice);

EZ_CREATE_SIMPLE_TEST(Grass, MeshWindAndCollision)
{
  ezStartup::StartupCoreSystems();
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Base", "GrassTests", "base").Succeeded());
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Plugins/GrassPlugin", "GrassTests", "grass").Succeeded());

  ezGALDeviceCreationDescription deviceDesc;
  ezGALDevice* pDevice = ezGALDeviceFactory::CreateDevice("DX11", ezFoundation::GetDefaultAllocator(), deviceDesc);
  if (!EZ_TEST_BOOL(pDevice != nullptr && pDevice->Init().Succeeded()))
    return;
  ezGALDevice::SetDefaultDevice(pDevice);
  ezStartup::StartupHighLevelSystems();

  {
    ezMeshResourceDescriptor meshDesc;
    auto& buffer = meshDesc.MeshBufferDesc();
    buffer.AddCommonStreams();
    buffer.AllocateStreams(3, ezGALPrimitiveTopology::Triangles, 1);
    buffer.SetPosition(0, ezVec3(-1, -1, 0));
    buffer.SetPosition(1, ezVec3(1, -1, 0));
    buffer.SetPosition(2, ezVec3(0, 1, 0));
    for (ezUInt32 i = 0; i < 3; ++i)
    {
      buffer.SetNormal(i, ezVec3(0, 0, 1));
      buffer.SetTangent(i, ezVec4(1, 0, 0, 1));
      buffer.SetTexCoord0(i, ezVec2(0));
    }
    buffer.SetTriangleIndices(0, 0, 1, 2);
    meshDesc.AddSubMesh(1, 0, 0);
    meshDesc.ComputeBounds();
    auto cpuDesc = meshDesc;
    const auto hCpuMesh = ezResourceManager::CreateResource<ezCpuMeshResource>("GrassTestEmitter", std::move(cpuDesc));
    const auto hMesh = ezResourceManager::CreateResource<ezMeshResource>("GrassTestEmitter", std::move(meshDesc));
    ezMaterialResourceDescriptor materialDesc;
    const auto hMaterial = ezResourceManager::CreateResource<ezMaterialResource>("GrassTestMaterial", std::move(materialDesc));

    ezWorldDesc worldDesc("GrassTest");
    ezWorld world(worldDesc);
    EZ_LOCK(world.GetWriteMarker());
    world.SetWorldSimulationEnabled(false);
    world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / 60.0));
    ezGameObject* pOwner;
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    world.CreateObject(objectDesc, pOwner);
    ezGrassComponent* pGrass;
    world.GetOrCreateComponentManager<ezGrassComponentManager>()->CreateComponent(pOwner, pGrass);
    pGrass->SetMesh(hMesh);
    pGrass->SetMaterial(hMaterial);
    pGrass->SetStrandCount(32);
    pGrass->SetSegmentCount(4);
    pGrass->SetBillboardCount(2);
    pGrass->SetRandomness(0);
    pGrass->SetWidth(0.05f);
    ezGrassAtlasRect atlas;
    atlas.m_vTexMulAdd = ezVec4(0.5f, 0.5f, 0.25f, 0.25f);
    pGrass->AtlasRects_Insert(0, atlas);
    world.Update();

    auto GetPatchMesh = [&]()
    {
      ezGrassPatchComponent* pPatch = nullptr;
      EZ_TEST_BOOL(world.TryGetComponent(pGrass->GetPatch(0), pPatch));
      return pPatch->GetMeshResource();
    };
    ezDynamicArray<ezVec3> original;
    auto CopyPositions = [&]()
    {
      ezResourceLock<ezDynamicMeshBufferResource> mesh(GetPatchMesh(), ezResourceAcquireMode::BlockTillLoaded);
      ezDynamicArray<ezVec3> result;
      result = mesh->AccessPositionData();
      return result;
    };

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Topology, bounds and deterministic regeneration")
    {
      ezTestFramework::Output(ezTestOutput::Message, "Topology, bounds and deterministic regeneration");
      EZ_TEST_BOOL((pGrass->GetPatchCount() > 0));
      ezResourceLock<ezDynamicMeshBufferResource> mesh(GetPatchMesh(), ezResourceAcquireMode::BlockTillLoaded);
      EZ_TEST_INT(mesh->GetDescriptor().m_uiMaxVertices, 32 * 5 * 2 * 2);
      EZ_TEST_INT(mesh->GetDescriptor().m_uiMaxPrimitives, 32 * 4 * 2 * 2);
      for (auto index : mesh->AccessIndex32Data())
        EZ_TEST_BOOL(index < mesh->GetDescriptor().m_uiMaxVertices);
      original = CopyPositions();
      for (const auto& p : original)
      {
        EZ_TEST_BOOL(p.IsValid());
        EZ_TEST_BOOL(p.z >= -0.0001f && p.z <= 1.0001f);
      }
      pGrass->SetRandomSeed(pGrass->GetRandomSeed());
      world.Update();
      EZ_TEST_BOOL(original == CopyPositions());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Vertex masks, empty geometry and recovery")
    {
      ezTestFramework::Output(ezTestOutput::Message, "Vertex masks, empty geometry and recovery");
      for (ezUInt32 i = 0; i < 3; ++i)
        pGrass->VertexLengths_Insert(i, 0.0f);
      world.Update();
      EZ_TEST_BOOL(!(pGrass->GetPatchCount() > 0));
      for (ezUInt32 i = 0; i < 3; ++i)
        pGrass->VertexLengths_SetValue(i, 1.0f);
      world.Update();
      EZ_TEST_BOOL(original == CopyPositions());
      pOwner->SetLocalScaling(ezVec3(0));
      world.Update();
      pOwner->SetLocalScaling(ezVec3(1));
      world.Update();
      EZ_TEST_BOOL(original == CopyPositions());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Wind bends tips while roots stay pinned")
    {
      ezTestFramework::Output(ezTestOutput::Message, "Wind bends tips while roots stay pinned");
      auto* pWind = world.GetOrCreateModule<ezSimpleWindWorldModule>();
      pWind->SetFallbackWind(ezVec3(3, 0, 0));
      world.SetWorldSimulationEnabled(true);
      for (ezUInt32 frame = 0; frame < 30; ++frame)
        world.Update();
      const auto bent = CopyPositions();
      float fDisplacement = 0;
      for (ezUInt32 i = 0; i < 32; ++i)
      {
        const ezUInt32 v = i * 20;
        const ezVec3 oldRoot = (original[v] + original[v + 1]) * 0.5f;
        const ezVec3 root = (bent[v] + bent[v + 1]) * 0.5f;
        EZ_TEST_VEC3(root, oldRoot, 0.0001f);
        fDisplacement += (bent[v + 8] - original[v + 8]).GetLength();
      }
      EZ_TEST_BOOL(fDisplacement > 0.1f);
      pWind->SetFallbackWind(ezVec3::MakeZero());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Kinematic capsule bends grass and grass recovers")
    {
      ezTestFramework::Output(ezTestOutput::Message, "Kinematic capsule bends grass and grass recovers");
      pGrass->SetRandomSeed(1);
      pGrass->m_fStiffness = 3;
      pGrass->m_fDrag = 0.5f;
      world.Update();
      ezGameObject* pCollider;
      objectDesc.m_LocalPosition = ezVec3(0, 0, 0.7f);
      world.CreateObject(objectDesc, pCollider);
      ezJoltShapeCapsuleComponent* pShape;
      world.GetOrCreateComponentManager<ezJoltShapeCapsuleComponentManager>()->CreateComponent(pCollider, pShape);
      pShape->SetRadius(0.5f);
      pShape->SetHeight(1);
      ezJoltDynamicActorComponent* pActor;
      world.GetOrCreateComponentManager<ezJoltDynamicActorComponentManager>()->CreateComponent(pCollider, pActor);
      pActor->SetKinematic(true);
      for (ezUInt32 frame = 0; frame < 20; ++frame)
        world.Update();
      auto Displacement = [&](const ezDynamicArray<ezVec3>& points)
      {
        float fSum = 0;
        for (ezUInt32 i = 0; i < points.GetCount(); ++i)
          fSum += (points[i] - original[i]).GetLength();
        return fSum;
      };
      const float fBent = Displacement(CopyPositions());
      EZ_TEST_BOOL(fBent > 0.1f);
      const auto held = CopyPositions();
      float fMaxContactMovement = 0;
      for (ezUInt32 frame = 0; frame < 120; ++frame)
      {
        world.Update();
        const auto points = CopyPositions();
        for (ezUInt32 i = 0; i < points.GetCount(); ++i)
        {
          // Check strands that were pushed substantially by the capsule.
          if ((held[i] - original[i]).GetLength() > 0.2f)
            fMaxContactMovement = ezMath::Max(fMaxContactMovement, (points[i] - held[i]).GetLength());
        }
      }
      EZ_TEST_FLOAT(fMaxContactMovement, 0.0f, 0.005f);
      pCollider->SetLocalPosition(ezVec3(10, 0, 0));
      for (ezUInt32 frame = 0; frame < 240; ++frame)
        world.Update();
      EZ_TEST_BOOL(Displacement(CopyPositions()) < fBent * 0.5f);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Scene serialization preserves settings")
    {
      ezTestFramework::Output(ezTestOutput::Message, "Scene serialization preserves settings");
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter stream(&storage);
      ezWorldWriter writer;
      writer.WriteWorld(stream, world);
      ezMemoryStreamReader input(&storage);
      ezWorldReader reader;
      EZ_TEST_BOOL(reader.ReadWorldDescription(input).Succeeded());
      ezWorld restored(worldDesc);
      EZ_LOCK(restored.GetWriteMarker());
      restored.SetWorldSimulationEnabled(false);
      reader.InstantiateWorld(restored);
      restored.Update();
      auto components = restored.GetOrCreateComponentManager<ezGrassComponentManager>()->GetComponents();
      EZ_TEST_BOOL(components.IsValid());
      EZ_TEST_INT(components->GetStrandCount(), 32);
      EZ_TEST_INT(components->GetSegmentCount(), 4);
      EZ_TEST_INT(components->VertexLengths_GetCount(), 3);
      EZ_TEST_INT(components->AtlasRects_GetCount(), 1);
      EZ_TEST_BOOL(components->AtlasRects_GetValue(0).m_vTexMulAdd == atlas.m_vTexMulAdd);
      EZ_TEST_BOOL(components->GetMesh() == hMesh);
      EZ_TEST_INT(components->GetPatchCount(), 1);
      EZ_TEST_INT(restored.GetObjectCount(), world.GetObjectCount());
      EZ_TEST_FLOAT(components->m_fSimulationDistance, pGrass->m_fSimulationDistance, 0);
      EZ_TEST_FLOAT(components->m_fViewDistance, pGrass->m_fViewDistance, 0);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Patch LOD, frustum, occlusion and separate distances")
    {
      ezTestFramework::Output(ezTestOutput::Message, "Patch LOD, frustum, occlusion and separate distances");
      pGrass->SetStrandCount(1024);
      pGrass->SetSegmentCount(10);
      pGrass->SetBillboardCount(4);
      pGrass->m_bCollision = false;
      pGrass->m_fViewDistance = 100;
      pGrass->m_fSimulationDistance = 40;
      pGrass->m_fLodStartDistance = 15;
      auto* pWind = world.GetOrCreateModule<ezSimpleWindWorldModule>();
      pWind->SetFallbackWind(ezVec3(1, 0, 0));
      ezCamera camera;
      camera.SetCameraMode(ezCameraMode::PerspectiveFixedFovY, 60, 0.1f, 1000);
      camera.LookAt(ezVec3(0, -8, 2), ezVec3(0, 0, 0.5f), ezVec3(0, 0, 1));
      ezView* pView = nullptr;
      const auto hView = ezRenderWorld::CreateView("Grass culling test", pView);
      pView->SetWorld(&world);
      pView->SetCamera(&camera);
      pView->SetViewport(ezRectFloat(0, 0, 256, 256));
      pView->SetCameraUsageHint(ezCameraUsageHint::MainView);
      ezRenderWorld::AddMainView(hView);
      auto QueryVisibility = [&](bool bOccluded)
      {
        ezFrustum frustum;
        pView->ComputeCullingFrustum(frustum);
        ezSpatialSystem::QueryParams query;
        query.m_uiCategoryBitmask = 0xFFFFFFFF;
        ezDynamicArray<const ezGameObject*> visible;
        world.GetSpatialSystem()->FindVisibleObjects(frustum, query, visible,
          [bOccluded](const ezSimdBBox&) { return bOccluded; }, ezVisibilityState::Direct);
      };
      world.Update();
      EZ_TEST_INT(pGrass->GetPatchCount(), 2);
      QueryVisibility(false);
      world.Update();
      EZ_TEST_INT(pGrass->GetStatistics().m_uiRenderTriangles, 1024 * 10 * 4 * 2);
      EZ_TEST_BOOL(pGrass->GetStatistics().m_uiSimulatedParticles > 0);
      camera.LookAt(ezVec3(0, -30, 2), ezVec3(0, 0, 0.5f), ezVec3(0, 0, 1));
      QueryVisibility(false);
      world.Update();
      EZ_TEST_INT(pGrass->GetStatistics().m_uiRenderTriangles, 1024 * 5 * 2 * 2);
      camera.LookAt(ezVec3(0, -70, 2), ezVec3(0, 0, 0.5f), ezVec3(0, 0, 1));
      QueryVisibility(false);
      world.Update();
      EZ_TEST_INT(pGrass->GetStatistics().m_uiRenderTriangles, 1024 * 1 * 1 * 2);
      EZ_TEST_INT(pGrass->GetStatistics().m_uiSimulatedParticles, 0);
      QueryVisibility(false);
      world.Update();
      EZ_TEST_INT(pGrass->GetStatistics().m_uiUploadedVertices, 0);
      ezGrassPatchComponent* pPatch = nullptr;
      EZ_TEST_BOOL(world.TryGetComponent(pGrass->GetPatch(0), pPatch));
      EZ_TEST_BOOL(pPatch->IsWithinViewDistance(camera.GetCenterPosition()));
      camera.LookAt(ezVec3(0, -120, 2), ezVec3(0, 0, 0.5f), ezVec3(0, 0, 1));
      QueryVisibility(false);
      world.Update();
      EZ_TEST_BOOL(!pPatch->IsWithinViewDistance(camera.GetCenterPosition()));
      EZ_TEST_INT(pGrass->GetStatistics().m_uiVisiblePatches, 0);
      EZ_TEST_INT(pGrass->GetStatistics().m_uiUploadedVertices, 0);
      camera.LookAt(ezVec3(0, -8, 2), ezVec3(0, -20, 2), ezVec3(0, 0, 1));
      QueryVisibility(false);
      world.Update();
      EZ_TEST_INT(pGrass->GetStatistics().m_uiVisiblePatches, 0);
      EZ_TEST_INT(pGrass->GetStatistics().m_uiSimulatedParticles, 0);
      camera.LookAt(ezVec3(0, -8, 2), ezVec3(0, 0, 0.5f), ezVec3(0, 0, 1));
      for (ezUInt32 frame = 0; frame < 5; ++frame)
      {
        QueryVisibility(true);
        world.Update();
      }
      EZ_TEST_INT(pGrass->GetStatistics().m_uiVisiblePatches, 0);
      EZ_TEST_INT(pGrass->GetStatistics().m_uiSimulatedParticles, 0);
      EZ_TEST_INT(pGrass->GetStatistics().m_uiUploadedVertices, 0);
      QueryVisibility(false);
      world.Update();
      EZ_TEST_INT(pGrass->GetStatistics().m_uiVisiblePatches, 2);
      EZ_TEST_BOOL(pGrass->GetStatistics().m_uiSimulatedParticles > 0);
      ezRenderWorld::RemoveMainView(hView);
      ezRenderWorld::DeleteView(hView);
    }
  }

  TestGrassMaterial(pDevice);
  ezStartup::ShutdownHighLevelSystems();
  ezResourceManager::FreeAllUnusedResources();
  pDevice->Shutdown().IgnoreResult();
  EZ_DEFAULT_DELETE(pDevice);
  ezFileSystem::RemoveDataDirectoryGroup("GrassTests");
  ezStartup::ShutdownCoreSystems();
}
