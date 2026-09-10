#include <Core/World/World.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Configuration/CVar.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/MemoryStream.h>
#include <Foundation/Memory/FrameAllocator.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <RendererCore/Material/MaterialResource.h>
#include <RendererCore/Meshes/MeshComponent.h>
#include <RendererCore/Pipeline/Extractor.h>
#include <RendererCore/Pipeline/Implementation/RenderPipelineResourceLoader.h>
#include <RendererCore/Pipeline/Passes/SourcePass.h>
#include <RendererCore/Pipeline/Passes/TargetPass.h>
#include <RendererCore/Pipeline/Passes/TransparentForwardRenderPass.h>
#include <RendererCore/Pipeline/RenderPipeline.h>
#include <RendererCore/Pipeline/RenderPipelineResource.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderContext/RenderContext.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererCore/ShaderCompiler/ShaderManager.h>
#include <RendererCore/Textures/Texture2DResource.h>
#include <RendererFoundation/CommandEncoder/CommandEncoder.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <RendererFoundation/Resources/ReadbackHelper.h>
#include <SkyBillboardPlugin/Components/SkyBillboardComponent.h>
#include <TestFramework/Framework/TestFramework.h>

void TestSkyBillboardMaterial(ezGALDevice* pDevice)
{
  ezTestFramework::Output(ezTestOutput::Message, "Offscreen sky projection, blending and serialization");
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Workspace", "SkyBillboardTests", "shadercache", ezDataDirUsage::AllowWrites).Succeeded());
  ezShaderManager::Configure("DX11_SM50", true);
  const char* szShaderModel = nullptr;
  const char* szShaderCompiler = nullptr;
  ezGALDeviceFactory::GetShaderModelAndCompiler("DX11", szShaderModel, szShaderCompiler);
  EZ_TEST_BOOL(ezPlugin::LoadPlugin(szShaderCompiler).Succeeded());
  auto* pMultithreading = static_cast<ezCVarBool*>(ezCVar::FindCVarByName("Rendering.Multithreading"));
  const bool bMultithreading = *pMultithreading;
  *pMultithreading = false;

  ezWorldDesc worldDesc("SkyBillboard material rendering");
  ezWorld world(worldDesc);
  world.SetWorldSimulationEnabled(false);
  ezCamera camera;
  camera.SetCameraMode(ezCameraMode::PerspectiveFixedFovY, 90.0f, 0.1f, 100);
  camera.LookAt(ezVec3::MakeZero(), ezVec3(1, 0, 0), ezVec3(0, 0, 1));

  ezDynamicArray<ezUniquePtr<ezRenderPipelinePass>> passes;
  ezUniquePtr<ezSourcePass> source = EZ_DEFAULT_NEW(ezSourcePass);
  auto* pSource = source.Borrow();
  ezReflectionUtils::SetMemberPropertyValue(static_cast<const ezAbstractMemberProperty*>(pSource->GetDynamicRTTI()->FindPropertyByName("Clear")), pSource, true);
  ezUniquePtr<ezTransparentForwardRenderPass> simple = EZ_DEFAULT_NEW(ezTransparentForwardRenderPass);
  ezUniquePtr<ezTargetPass> target = EZ_DEFAULT_NEW(ezTargetPass);
  ezUniquePtr<ezSourcePass> depth = EZ_DEFAULT_NEW(ezSourcePass);
  auto* pDepth = depth.Borrow();
  ezReflectionUtils::SetMemberPropertyValue(static_cast<const ezAbstractMemberProperty*>(pDepth->GetDynamicRTTI()->FindPropertyByName("Clear")), pDepth, true);
  ezReflectionUtils::SetMemberPropertyValue(static_cast<const ezAbstractMemberProperty*>(pDepth->GetDynamicRTTI()->FindPropertyByName("Type")), pDepth, ezRequiredTextureType::Depth);
  ezReflectionUtils::SetMemberPropertyValue(static_cast<const ezAbstractMemberProperty*>(pDepth->GetDynamicRTTI()->FindPropertyByName("Precision")), pDepth, ezRequiredTexturePrecision::Bits_24);
  ezReflectionUtils::SetMemberPropertyValue(static_cast<const ezAbstractMemberProperty*>(pDepth->GetDynamicRTTI()->FindPropertyByName("Channels")), pDepth, ezRequiredTextureChannels::Channels_2);
  passes.PushBack(std::move(depth));
  passes.PushBack(std::move(source));
  passes.PushBack(std::move(simple));
  passes.PushBack(std::move(target));
  ezDynamicArray<ezUniquePtr<ezExtractor>> extractors;
  extractors.PushBack(EZ_DEFAULT_NEW(ezVisibleObjectsExtractor));
  ezDynamicArray<const ezRenderPipelinePass*> passPointers;
  for (const auto& pPass : passes)
    passPointers.PushBack(pPass.Borrow());
  ezDynamicArray<const ezExtractor*> extractorPointers;
  extractorPointers.PushBack(extractors[0].Borrow());
  ezDynamicArray<ezRenderPipelineResourceLoaderConnection> connections;
  connections.PushBack({1, 2, "Output", "Color"});
  connections.PushBack({2, 3, "Color", "Color0"});
  connections.PushBack({0, 2, "Output", "DepthStencil"});
  ezRenderPipelineResourceDescriptor pipelineDesc;
  ezMemoryStreamContainerWrapperStorage<ezDynamicArray<ezUInt8>> pipelineStorage(&pipelineDesc.m_SerializedPipeline);
  ezMemoryStreamWriter pipelineWriter(&pipelineStorage);
  EZ_TEST_RESULT(ezRenderPipelineResourceLoader::ExportPipeline(passPointers, extractorPointers, connections, pipelineWriter));
  const auto hPipeline = ezResourceManager::CreateResource<ezRenderPipelineResource>("SkyBillboardMaterialPipeline", std::move(pipelineDesc));

  ezGALTextureCreationDescription targetDesc;
  targetDesc.m_uiWidth = targetDesc.m_uiHeight = 64;
  targetDesc.m_Format = ezGALResourceFormat::RGBAUByteNormalizedsRGB;
  targetDesc.m_TextureFlags = ezGALTextureUsageFlags::RenderTarget | ezGALTextureUsageFlags::ShaderResource;
  auto hTarget = pDevice->CreateTexture(targetDesc);
  ezGALRenderTargets targets;
  targets.m_hRTs[0] = hTarget;
  ezView* pView = nullptr;
  const auto hView = ezRenderWorld::CreateView("SkyBillboard material", pView);
  pView->SetWorld(&world);
  pView->SetCamera(&camera);
  pView->SetViewport(ezRectFloat(0, 0, 64, 64));
  pView->SetRenderTargets(targets);
  pView->SetRenderPipelineResource(hPipeline);
  pView->SetShaderPermutationVariable("FORWARD_PASS_WRITE_DEPTH", "TRUE");
  ezRenderWorld::AddMainView(hView);

  const ezUInt8 pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 0, 255, 255, 0, 255};
  ezGALSystemMemoryDescription memory;
  memory.m_pData = ezConstByteBlobPtr(pixels, sizeof(pixels));
  memory.m_uiRowPitch = 8;
  memory.m_uiSlicePitch = sizeof(pixels);
  ezTexture2DResourceDescriptor textureDesc;
  textureDesc.m_DescGAL.m_uiWidth = textureDesc.m_DescGAL.m_uiHeight = 2;
  textureDesc.m_DescGAL.m_Format = ezGALResourceFormat::RGBAUByteNormalizedsRGB;
  textureDesc.m_InitialContent = ezMakeArrayPtr(&memory, 1);
  textureDesc.m_SamplerDesc.m_MinFilter = ezGALTextureFilterMode::Point;
  textureDesc.m_SamplerDesc.m_MagFilter = ezGALTextureFilterMode::Point;
  const auto hTexture = ezResourceManager::CreateResource<ezTexture2DResource>("SkyBillboardChecker", std::move(textureDesc));

  ezSkyBillboardComponent* pBillboard = nullptr;
  ezGameObject* pObject = nullptr;
  {
    EZ_LOCK(world.GetWriteMarker());
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    world.CreateObject(objectDesc, pObject);
    world.GetOrCreateComponentManager<ezSkyBillboardComponentManager>()->CreateComponent(pObject, pBillboard);
    pBillboard->SetTexture(hTexture);
    pBillboard->SetSize(90);
  }

  auto Render = [&]()
  {
    for (ezUInt32 frame = 0; frame < 6; ++frame)
    {
      pDevice->BeginFrame();
      {
        EZ_LOCK(world.GetWriteMarker());
        world.Update();
      }
      ezRenderWorld::ExtractMainViews();
      ezRenderWorld::BeginFrame();
      ezRenderWorld::Render(ezRenderContext::GetDefaultInstance());
      ezRenderWorld::EndFrame();
      pDevice->EndFrame();
      ezFrameAllocator::Swap();
    }
    ezGALReadbackTextureHelper readback;
    auto* pEncoder = pDevice->BeginCommands("Sky billboard readback");
    readback.ReadbackTexture(*pEncoder, hTarget);
    pEncoder->Flush();
    pDevice->EndCommands(pEncoder);
    EZ_TEST_BOOL(readback.GetReadbackResult(ezTime::MakeFromSeconds(10)) == ezGALAsyncResult::Ready);
    ezGALTextureSubresource subresource;
    ezDynamicArray<ezGALSystemMemoryDescription> result;
    auto lock = readback.LockTexture(ezMakeArrayPtr(&subresource, 1), result);
    ezDynamicArray<ezUInt8> image;
    if (result.IsEmpty())
      return image;
    for (ezUInt32 y = 0; y < 64; ++y)
    {
      const auto* pRow = result[0].m_pData.GetPtr() + y * result[0].m_uiRowPitch;
      for (ezUInt32 x = 0; x < 64 * 4; ++x)
        image.PushBack(pRow[x]);
    }
    return image;
  };

  auto reference = Render();
  auto Pixel = [&](const ezDynamicArray<ezUInt8>& image, ezUInt32 x, ezUInt32 y, ezUInt32 c)
  {
    return image[(y * 64 + x) * 4 + c];
  };
  if (EZ_TEST_INT(reference.GetCount(), 64 * 64 * 4))
  {
    for (ezUInt32 y : {16u, 48u})
      for (ezUInt32 x : {16u, 48u})
        ezTestFramework::Output(ezTestOutput::Message, "Pixel %u %u: %u %u %u", x, y, Pixel(reference, x, y, 0), Pixel(reference, x, y, 1), Pixel(reference, x, y, 2));
    EZ_TEST_INT(Pixel(reference, 16, 16, 0), 255);
    EZ_TEST_INT(Pixel(reference, 48, 16, 1), 255);
    EZ_TEST_INT(Pixel(reference, 16, 48, 2), 0); // The blue texel has zero alpha.
    {
      EZ_LOCK(world.GetWriteMarker());
      pObject->SetLocalPosition(ezVec3(200, -40, 8));
      pObject->SetLocalScaling(ezVec3(2, 3, 4));
    }
    camera.LookAt(ezVec3(30, 20, 10), ezVec3(31, 20, 10), ezVec3(0, 0, 1));
    EZ_TEST_BOOL(Render() == reference);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->SetOpacity(0);
    }
    auto hidden = Render();
    EZ_TEST_INT(Pixel(hidden, 16, 16, 0), 0);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->SetOpacity(1);
      pBillboard->SetSize(30);
    }
    auto small = Render();
    EZ_TEST_INT(Pixel(small, 16, 16, 0), 0);
    EZ_TEST_INT(Pixel(small, 28, 28, 0), 255);
    {
      EZ_LOCK(world.GetWriteMarker());
      pObject->SetLocalRotation(ezQuat::MakeFromAxisAndAngle(ezVec3(0, 0, 1), ezAngle::MakeFromDegree(180)));
    }
    EZ_TEST_BOOL(Render() == hidden);
    {
      EZ_LOCK(world.GetWriteMarker());
      pObject->SetLocalRotation(ezQuat::MakeIdentity());
      pBillboard->SetSize(90);
      pBillboard->SetAdditive(true);
      pBillboard->SetOpacity(0.5f);
      pBillboard->SetGlowIntensity(2);
      pBillboard->SetGlowColor(ezColor(0, 1, 0));
      pBillboard->SetLayer(7);
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter stream(&storage);
      ezWorldWriter writer;
      writer.WriteWorld(stream, world);
      ezMemoryStreamReader input(&storage);
      ezWorldReader reader;
      EZ_TEST_BOOL(reader.ReadWorldDescription(input).Succeeded());
      ezWorld restored(worldDesc);
      EZ_LOCK(restored.GetWriteMarker());
      reader.InstantiateWorld(restored);
      restored.Update();
      auto components = restored.GetOrCreateComponentManager<ezSkyBillboardComponentManager>()->GetComponents();
      if (EZ_TEST_BOOL(components.IsValid()))
      {
        EZ_TEST_BOOL(components->GetTexture() == hTexture);
        EZ_TEST_BOOL(components->GetAdditive());
        EZ_TEST_FLOAT(components->GetOpacity(), 0.5f, 0);
        EZ_TEST_FLOAT(components->GetGlowIntensity(), 2, 0);
        EZ_TEST_BOOL(components->GetGlowColor() == ezColor(0, 1, 0));
        EZ_TEST_INT(components->GetLayer(), 7);
      }
    }
    auto glow = Render();
    EZ_TEST_BOOL(Pixel(glow, 16, 16, 0) > 170 && Pixel(glow, 16, 16, 0) < 200);
    EZ_TEST_INT(Pixel(glow, 48, 16, 1), 255);

    const ezUInt8 cubePixel[] = {0, 0, 255, 128};
    ezGALSystemMemoryDescription faces[6];
    for (auto& face : faces)
    {
      face.m_pData = ezConstByteBlobPtr(cubePixel, sizeof(cubePixel));
      face.m_uiRowPitch = face.m_uiSlicePitch = 4;
    }
    ezTextureCubeResourceDescriptor cubeDesc;
    cubeDesc.m_DescGAL.m_uiWidth = cubeDesc.m_DescGAL.m_uiHeight = 1;
    cubeDesc.m_DescGAL.m_Type = ezGALTextureType::TextureCube;
    cubeDesc.m_DescGAL.m_Format = ezGALResourceFormat::RGBAUByteNormalizedsRGB;
    cubeDesc.m_InitialContent = ezMakeArrayPtr(faces);
    auto hCubeMap = ezResourceManager::CreateResource<ezTextureCubeResource>("SkyBillboardBlueCube", std::move(cubeDesc));
    ezSkyBillboardComponent* pHorizon = nullptr;
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->SetOpacity(1);
      pBillboard->SetGlowIntensity(0);
      pBillboard->SetAdditive(false);
      ezGameObject* pHorizonObject = nullptr;
      world.CreateObject(ezGameObjectDesc(), pHorizonObject);
      world.GetOrCreateComponentManager<ezSkyBillboardComponentManager>()->CreateComponent(pHorizonObject, pHorizon);
      pHorizon->SetUseCubeMap(true);
      pHorizon->SetCubeMap(hCubeMap);
      pHorizon->SetLayer(6);
    }
    auto behind = Render();
    EZ_TEST_INT(Pixel(behind, 16, 16, 0), 255);
    EZ_TEST_INT(Pixel(behind, 16, 16, 2), 0);
    EZ_TEST_BOOL(Pixel(behind, 16, 48, 2) > 180);
    {
      EZ_LOCK(world.GetWriteMarker());
      pHorizon->SetLayer(8);
    }
    auto over = Render();
    EZ_TEST_BOOL(Pixel(over, 16, 16, 0) > 180 && Pixel(over, 16, 16, 0) < 195);
    EZ_TEST_BOOL(Pixel(over, 16, 16, 2) > 180 && Pixel(over, 16, 16, 2) < 195);
    {
      EZ_LOCK(world.GetWriteMarker());
      pHorizon->SetAdditive(true);
    }
    auto additive = Render();
    EZ_TEST_INT(Pixel(additive, 16, 16, 0), 255);
    EZ_TEST_BOOL(Pixel(additive, 16, 16, 2) > 180);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->SetActiveFlag(false);
      pHorizon->SetSize(1);
      pHorizon->SetAspectRatio(10);
    }
    auto panorama = Render();
    EZ_TEST_BOOL(Pixel(panorama, 1, 1, 2) > 180);
    EZ_TEST_BOOL(Pixel(panorama, 62, 62, 2) > 180);
    camera.SetCameraMode(ezCameraMode::OrthoFixedHeight, 2, 0.1f, 100);
    EZ_TEST_BOOL(Render() == hidden);
  }
  ezRenderWorld::RemoveMainView(hView);
  ezRenderWorld::DeleteView(hView);
  for (ezUInt32 frame = 0; frame < 2; ++frame)
  {
    pDevice->BeginFrame();
    ezRenderWorld::ExtractMainViews();
    ezRenderWorld::BeginFrame();
    ezRenderWorld::Render(ezRenderContext::GetDefaultInstance());
    ezRenderWorld::EndFrame();
    pDevice->EndFrame();
    ezFrameAllocator::Swap();
  }
  pDevice->DestroyTexture(hTarget);
  *pMultithreading = bMultithreading;
}
