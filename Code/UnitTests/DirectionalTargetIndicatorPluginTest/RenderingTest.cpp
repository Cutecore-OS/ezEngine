#include <Core/World/World.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <DirectionalTargetIndicatorPlugin/Components/DirectionalTargetIndicatorComponent.h>
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
#include <RendererCore/Pipeline/Passes/SimpleRenderPass.h>
#include <RendererCore/Pipeline/Passes/SourcePass.h>
#include <RendererCore/Pipeline/Passes/TargetPass.h>
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
#include <TestFramework/Framework/TestFramework.h>

void TestDirectionalTargetIndicatorMaterial(ezGALDevice* pDevice)
{
  ezTestFramework::Output(ezTestOutput::Message, "Directional indicator projection, blending and serialization");
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Workspace", "DirectionalTargetIndicatorTests", "shadercache", ezDataDirUsage::AllowWrites).Succeeded());
  ezShaderManager::Configure("DX11_SM50", true);
  const char* szShaderModel = nullptr;
  const char* szShaderCompiler = nullptr;
  ezGALDeviceFactory::GetShaderModelAndCompiler("DX11", szShaderModel, szShaderCompiler);
  EZ_TEST_BOOL(ezPlugin::LoadPlugin(szShaderCompiler).Succeeded());
  auto* pMultithreading = static_cast<ezCVarBool*>(ezCVar::FindCVarByName("Rendering.Multithreading"));
  const bool bMultithreading = *pMultithreading;
  *pMultithreading = false;

  ezWorldDesc worldDesc("DirectionalTargetIndicator material rendering");
  ezWorld world(worldDesc);
  world.SetWorldSimulationEnabled(false);
  ezCamera camera;
  camera.SetCameraMode(ezCameraMode::PerspectiveFixedFovY, 90.0f, 0.1f, 100);
  camera.LookAt(ezVec3::MakeZero(), ezVec3(1, 0, 0), ezVec3(0, 0, 1));

  ezDynamicArray<ezUniquePtr<ezRenderPipelinePass>> passes;
  ezUniquePtr<ezSourcePass> source = EZ_DEFAULT_NEW(ezSourcePass);
  auto* pSource = source.Borrow();
  ezReflectionUtils::SetMemberPropertyValue(static_cast<const ezAbstractMemberProperty*>(pSource->GetDynamicRTTI()->FindPropertyByName("Clear")), pSource, true);
  ezUniquePtr<ezSimpleRenderPass> simple = EZ_DEFAULT_NEW(ezSimpleRenderPass);
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
  const auto hPipeline = ezResourceManager::CreateResource<ezRenderPipelineResource>("DirectionalTargetIndicatorMaterialPipeline", std::move(pipelineDesc));

  ezGALTextureCreationDescription targetDesc;
  targetDesc.m_uiWidth = targetDesc.m_uiHeight = 64;
  targetDesc.m_Format = ezGALResourceFormat::RGBAUByteNormalizedsRGB;
  targetDesc.m_TextureFlags = ezGALTextureUsageFlags::RenderTarget | ezGALTextureUsageFlags::ShaderResource;
  auto hTarget = pDevice->CreateTexture(targetDesc);
  ezGALRenderTargets targets;
  targets.m_hRTs[0] = hTarget;
  ezView* pView = nullptr;
  const auto hView = ezRenderWorld::CreateView("DirectionalTargetIndicator material", pView);
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
  const auto hTexture = ezResourceManager::CreateResource<ezTexture2DResource>("DirectionalTargetIndicatorChecker", std::move(textureDesc));

  ezDirectionalTargetIndicatorComponent* pBillboard = nullptr;
  ezGameObject* pObject = nullptr;
  {
    EZ_LOCK(world.GetWriteMarker());
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    world.CreateObject(objectDesc, pObject);
    world.GetOrCreateComponentManager<ezDirectionalTargetIndicatorComponentManager>()->CreateComponent(pObject, pBillboard);
    pObject->SetLocalPosition(ezVec3(10, 0, 0));
    pBillboard->m_hMarkerTexture = hTexture;
    pBillboard->m_vMarkerSize = ezVec2(16);
    pBillboard->m_fScreenMargin = 4;
    pBillboard->m_fScreenOffset = 0;
    pBillboard->m_fCornerRadius = 0;
    auto& element = pBillboard->m_OffscreenElements.ExpandAndGetRef();
    element.m_hTexture = hTexture;
    element.m_vSize = ezVec2(16);
    element.m_bRotate = false;
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


  auto Pixel = [&](const ezDynamicArray<ezUInt8>& image, ezUInt32 x, ezUInt32 y, ezUInt32 c)
  {
    return image[(y * 64 + x) * 4 + c];
  };
  auto SetPosition = [&](const ezVec3& vPosition)
  {
    EZ_LOCK(world.GetWriteMarker());
    pObject->SetLocalPosition(vPosition);
  };
  auto reference = Render();
  if (EZ_TEST_INT(reference.GetCount(), 64 * 64 * 4))
  {
    EZ_TEST_INT(Pixel(reference, 28, 28, 0), 255);
    EZ_TEST_INT(Pixel(reference, 36, 28, 1), 255);
    EZ_TEST_INT(Pixel(reference, 28, 36, 2), 0); // Transparent texel.
    SetPosition(ezVec3(50, 0, 0));
    EZ_TEST_BOOL(Render() == reference);         // Size does not change with distance.
    SetPosition(camera.GetCenterDirForwards() * 10.0f + camera.GetCenterDirRight() * 30.0f);
    auto right = Render();
    EZ_TEST_INT(Pixel(right, 48, 28, 0), 255);
    EZ_TEST_INT(Pixel(right, 56, 28, 1), 255);
    SetPosition(camera.GetCenterDirForwards() * -10.0f + camera.GetCenterDirRight() * 30.0f);
    EZ_TEST_BOOL(Render() == right); // Behind-camera projection must not mirror the arrow.
    SetPosition(camera.GetCenterDirForwards() * 10.0f - camera.GetCenterDirRight() * 30.0f);
    EZ_TEST_INT(Pixel(Render(), 8, 28, 0), 255);
    SetPosition(ezVec3(10, 0, 30));
    EZ_TEST_INT(Pixel(Render(), 28, 8, 0), 255);
    SetPosition(ezVec3(10, 0, -30));
    EZ_TEST_INT(Pixel(Render(), 28, 48, 0), 255);
    SetPosition(ezVec3(-10, 0, 0));
    EZ_TEST_INT(Pixel(Render(), 28, 48, 0), 255);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_bShowOffscreenIndicator = false;
    }
    EZ_TEST_INT(Pixel(Render(), 28, 48, 0), 0);
    SetPosition(ezVec3(10, 0, 0));
    EZ_TEST_BOOL(Render() == reference);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_vCaptureOffset = ezVec3(0, 0, 5);
    }
    EZ_TEST_INT(Pixel(Render(), 28, 12, 0), 255);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_vCaptureOffset.SetZero();
      pBillboard->m_bShowOffscreenIndicator = true;
      pBillboard->m_OffscreenElements[0].m_bRotate = true;
      pBillboard->m_OffscreenElements[0].m_vOffset = ezVec2(0, -4);
    }
    SetPosition(camera.GetCenterDirForwards() * 10.0f + camera.GetCenterDirRight() * 30.0f);
    auto rotated = Render();
    EZ_TEST_INT(Pixel(rotated, 56, 28, 0), 255); // Top-left texel rotates to top-right.
    EZ_TEST_INT(Pixel(rotated, 56, 36, 1), 255);
    {
      EZ_LOCK(world.GetWriteMarker());
      ezDirectionalTargetIndicatorElement layer;
      layer.m_hTexture = hTexture;
      layer.m_vSize = ezVec2(8);
      layer.m_bRotate = false;
      pBillboard->m_OffscreenElements.PushBack(layer);
      layer.m_vSize = ezVec2(4);
      layer.m_vOffset = ezVec2(-4, 0);
      pBillboard->m_OffscreenElements.PushBack(layer);
    }
    auto layered = Render();
    EZ_TEST_INT(Pixel(layered, 50, 30, 1), 255); // Second layer overlays the rotated first layer.
    EZ_TEST_INT(Pixel(layered, 45, 31, 1), 255); // Third layer retains its independent offset and size.
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_OffscreenElements.SetCount(1);
    }
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_OffscreenElements[0].m_bRotate = false;
      pBillboard->m_OffscreenElements[0].m_vOffset.SetZero();
      pBillboard->m_fScreenOffset = 6.25f; // Four pixels at this viewport resolution.
      pBillboard->m_fRightOffset = 6.25f;  // Adds another four pixels on the right only.
    }
    auto inset = Render();
    EZ_TEST_INT(Pixel(inset, 40, 28, 0), 255);
    EZ_TEST_INT(Pixel(inset, 56, 28, 1), 0);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_fScreenOffset = 0;
      pBillboard->m_fRightOffset = 0;
    }
    SetPosition(camera.GetCenterDirForwards() * 10.0f + camera.GetCenterDirRight() * 30.0f - camera.GetCenterDirUp() * 30.0f);
    EZ_TEST_INT(Pixel(Render(), 57, 57, 1), 255);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_fCornerRadius = 25;
    }
    EZ_TEST_INT(Pixel(Render(), 57, 57, 1), 0); // Rounded path cuts across the sharp corner.
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_fScreenOffset = 2;
      pBillboard->m_fLeftOffset = 3;
      pBillboard->m_fRightOffset = 4;
      pBillboard->m_fTopOffset = 5;
      pBillboard->m_fBottomOffset = 6;
      pBillboard->m_OffscreenElements[0].m_bRotate = true;
      pBillboard->m_OffscreenElements[0].m_vOffset = ezVec2(0, -4);
    }
    camera.SetCameraMode(ezCameraMode::OrthoFixedHeight, 20, 0.1f, 100);
    SetPosition(ezVec3(10, 0, 0));
    EZ_TEST_BOOL(Render() == reference);
    {
      EZ_LOCK(world.GetWriteMarker());
      pBillboard->m_vCaptureOffset = ezVec3(1, 2, 3);
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter stream(&storage);
      ezWorldWriter writer;
      writer.WriteWorld(stream, world);
      ezMemoryStreamReader input(&storage);
      ezWorldReader reader;
      EZ_TEST_RESULT(reader.ReadWorldDescription(input));
      ezWorld restored(worldDesc);
      EZ_LOCK(restored.GetWriteMarker());
      reader.InstantiateWorld(restored);
      restored.Update();
      auto components = restored.GetOrCreateComponentManager<ezDirectionalTargetIndicatorComponentManager>()->GetComponents();
      if (EZ_TEST_BOOL(components.IsValid()))
      {
        EZ_TEST_BOOL(components->m_hMarkerTexture == hTexture);
        EZ_TEST_BOOL(components->m_vMarkerSize == ezVec2(16));
        EZ_TEST_BOOL(components->m_vCaptureOffset == ezVec3(1, 2, 3));
        EZ_TEST_BOOL(components->m_bShowOffscreenIndicator);
        EZ_TEST_FLOAT(components->m_fScreenMargin, 4, 0);
        EZ_TEST_FLOAT(components->m_fScreenOffset, 2, 0);
        EZ_TEST_FLOAT(components->m_fLeftOffset, 3, 0);
        EZ_TEST_FLOAT(components->m_fRightOffset, 4, 0);
        EZ_TEST_FLOAT(components->m_fTopOffset, 5, 0);
        EZ_TEST_FLOAT(components->m_fBottomOffset, 6, 0);
        EZ_TEST_FLOAT(components->m_fCornerRadius, 25, 0);
        if (EZ_TEST_INT(components->m_OffscreenElements.GetCount(), 1))
        {
          const auto& element = components->m_OffscreenElements[0];
          EZ_TEST_BOOL(element.m_hTexture == hTexture);
          EZ_TEST_BOOL(element.m_vSize == ezVec2(16));
          EZ_TEST_BOOL(element.m_bRotate);
          EZ_TEST_BOOL(element.m_vOffset == ezVec2(0, -4));
        }
      }
    }
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
