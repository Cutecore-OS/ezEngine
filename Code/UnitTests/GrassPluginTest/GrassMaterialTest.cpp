#include <Core/World/World.h>
#include <Foundation/Configuration/CVar.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/MemoryStream.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <Foundation/Memory/FrameAllocator.h>
#include <RendererCore/Material/MaterialResource.h>
#include <RendererCore/Meshes/MeshComponent.h>
#include <RendererCore/Pipeline/Extractor.h>
#include <RendererCore/Pipeline/Passes/SimpleRenderPass.h>
#include <RendererCore/Pipeline/Passes/SourcePass.h>
#include <RendererCore/Pipeline/Passes/TargetPass.h>
#include <RendererCore/Pipeline/RenderPipeline.h>
#include <RendererCore/Pipeline/RenderPipelineResource.h>
#include <RendererCore/Pipeline/Implementation/RenderPipelineResourceLoader.h>
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

void TestGrassMaterial(ezGALDevice* pDevice)
{
  ezTestFramework::Output(ezTestOutput::Message, "Offscreen material textures and alpha masking");
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Workspace", "GrassTests", "shadercache", ezDataDirUsage::AllowWrites).Succeeded());
  ezShaderManager::Configure("DX11_SM50", true);
  const char* szShaderModel = nullptr;
  const char* szShaderCompiler = nullptr;
  ezGALDeviceFactory::GetShaderModelAndCompiler("DX11", szShaderModel, szShaderCompiler);
  EZ_TEST_BOOL(ezPlugin::LoadPlugin(szShaderCompiler).Succeeded());
  auto* pMultithreading = static_cast<ezCVarBool*>(ezCVar::FindCVarByName("Rendering.Multithreading"));
  const bool bMultithreading = *pMultithreading;
  *pMultithreading = false;

  ezWorldDesc worldDesc("Grass material rendering");
  ezWorld world(worldDesc);
  ezCamera camera;
  camera.SetCameraMode(ezCameraMode::OrthoFixedHeight, 2.5f, 0.1f, 10);
  camera.LookAt(ezVec3(0, 0, 3), ezVec3::MakeZero(), ezVec3(0, 1, 0));

  ezDynamicArray<ezUniquePtr<ezRenderPipelinePass>> passes;
  ezUniquePtr<ezSourcePass> source = EZ_DEFAULT_NEW(ezSourcePass);
  auto* pSource = source.Borrow();
  ezReflectionUtils::SetMemberPropertyValue(static_cast<const ezAbstractMemberProperty*>(pSource->GetDynamicRTTI()->FindPropertyByName("Clear")), pSource, true);
  ezUniquePtr<ezSimpleRenderPass> simple = EZ_DEFAULT_NEW(ezSimpleRenderPass);
  ezUniquePtr<ezTargetPass> target = EZ_DEFAULT_NEW(ezTargetPass);
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
  connections.PushBack({0, 1, "Output", "Color"});
  connections.PushBack({1, 2, "Color", "Color0"});
  ezRenderPipelineResourceDescriptor pipelineDesc;
  ezMemoryStreamContainerWrapperStorage<ezDynamicArray<ezUInt8>> pipelineStorage(&pipelineDesc.m_SerializedPipeline);
  ezMemoryStreamWriter pipelineWriter(&pipelineStorage);
  EZ_TEST_RESULT(ezRenderPipelineResourceLoader::ExportPipeline(passPointers, extractorPointers, connections, pipelineWriter));
  const auto hPipeline = ezResourceManager::CreateResource<ezRenderPipelineResource>("GrassMaterialPipeline", std::move(pipelineDesc));

  ezGALTextureCreationDescription targetDesc;
  targetDesc.m_uiWidth = targetDesc.m_uiHeight = 64;
  targetDesc.m_Format = ezGALResourceFormat::RGBAUByteNormalizedsRGB;
  targetDesc.m_TextureFlags = ezGALTextureUsageFlags::RenderTarget | ezGALTextureUsageFlags::ShaderResource;
  auto hTarget = pDevice->CreateTexture(targetDesc);
  ezGALRenderTargets targets;
  targets.m_hRTs[0] = hTarget;
  ezView* pView = nullptr;
  const auto hView = ezRenderWorld::CreateView("Grass material", pView);
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
  const auto hTexture = ezResourceManager::CreateResource<ezTexture2DResource>("GrassChecker", std::move(textureDesc));

  ezMeshResourceDescriptor meshDesc;
  auto& buffer = meshDesc.MeshBufferDesc();
  buffer.AddCommonStreams();
  buffer.AllocateStreams(4, ezGALPrimitiveTopology::Triangles, 2);
  for (ezUInt32 i = 0; i < 4; ++i)
  {
    const ezVec2 uv(float(i & 1), float(i >> 1));
    buffer.SetPosition(i, ezVec3(uv.x * 2 - 1, uv.y * 2 - 1, 0));
    buffer.SetNormal(i, ezVec3(0, 0, 1));
    buffer.SetTangent(i, ezVec4(1, 0, 0, 1));
    buffer.SetTexCoord0(i, uv);
  }
  buffer.SetTriangleIndices(0, 0, 1, 2);
  buffer.SetTriangleIndices(1, 2, 1, 3);
  meshDesc.AddSubMesh(2, 0, 0);
  meshDesc.ComputeBounds();
  const auto hMesh = ezResourceManager::CreateResource<ezMeshResource>("GrassMaterialQuad", std::move(meshDesc));
  ezMeshComponent* pMesh = nullptr;
  {
    EZ_LOCK(world.GetWriteMarker());
    ezGameObject* pObject = nullptr;
    world.CreateObject(ezGameObjectDesc(), pObject);
    world.GetOrCreateComponentManager<ezMeshComponentManager>()->CreateComponent(pObject, pMesh);
    pMesh->SetMesh(hMesh);
  }

  ezDynamicArray<ezUInt8> reference[2];
  for (ezUInt32 shader = 0; shader < 2; ++shader)
  {
    for (ezUInt32 masked = 0; masked < 2; ++masked)
    {
      ezMaterialResourceDescriptor material;
      material.m_hShader = ezResourceManager::LoadResource<ezShaderResource>(shader == 0 ? "Shaders/Materials/DefaultMaterial.ezShader" : "Shaders/Grass.ezShader");
      material.m_RenderDataCategory = ezDefaultRenderDataCategories::SimpleOpaque;
      auto Permutation = [&](const char* szName, const char* szValue)
      {
        auto& value = material.m_PermutationVars.ExpandAndGetRef();
        value.m_sName.Assign(szName);
        value.m_sValue.Assign(szValue);
      };
      Permutation("BLEND_MODE", masked ? "BLEND_MODE_MASKED" : "BLEND_MODE_OPAQUE");
      Permutation("SHADING_MODE", "SHADING_MODE_FULLBRIGHT");
      Permutation("TWO_SIDED", "TRUE");
      material.m_Parameters.PushBack({ezMakeHashedString("BaseColor"), ezColor::White});
      material.m_Parameters.PushBack({ezMakeHashedString("UseBaseTexture"), true});
      material.m_Parameters.PushBack({ezMakeHashedString("MaskThreshold"), 0.5f});
      material.m_Texture2DBindings.PushBack({ezMakeHashedString("BaseTexture"), hTexture});
      ezStringBuilder name;
      name.SetFormat("GrassMaterial_{}_{}", shader, masked);
      const auto hMaterial = ezResourceManager::CreateResource<ezMaterialResource>(name, std::move(material));
      {
        EZ_LOCK(world.GetWriteMarker());
        pMesh->SetMaterial(0, hMaterial);
      }
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
      auto* pEncoder = pDevice->BeginCommands("Grass material readback");
      readback.ReadbackTexture(*pEncoder, hTarget);
      pEncoder->Flush();
      pDevice->EndCommands(pEncoder);
      EZ_TEST_BOOL(readback.GetReadbackResult(ezTime::MakeFromSeconds(10)) == ezGALAsyncResult::Ready);
      ezGALTextureSubresource subresource;
      ezDynamicArray<ezGALSystemMemoryDescription> result;
      auto lock = readback.LockTexture(ezMakeArrayPtr(&subresource, 1), result);
      ezDynamicArray<ezUInt8> image;
      ezUInt32 uiColored = 0;
      for (ezUInt32 y = 0; y < 64; ++y)
      {
        const auto* pRow = result[0].m_pData.GetPtr() + y * result[0].m_uiRowPitch;
        for (ezUInt32 x = 0; x < 64; ++x)
        {
          uiColored += pRow[x * 4] > 50 || pRow[x * 4 + 1] > 50 || pRow[x * 4 + 2] > 50;
          for (ezUInt32 c = 0; c < 3; ++c)
            image.PushBack(pRow[x * 4 + c]);
        }
      }
      ezTestFramework::Output(ezTestOutput::Message, "%s: %u colored pixels", name.GetData(), uiColored);
      ezTestFramework::Output(ezTestOutput::Message, "Samples: %u %u %u / %u %u %u", image[(16 * 64 + 16) * 3], image[(16 * 64 + 16) * 3 + 1], image[(16 * 64 + 16) * 3 + 2], image[(48 * 64 + 16) * 3], image[(48 * 64 + 16) * 3 + 1], image[(48 * 64 + 16) * 3 + 2]);
      EZ_TEST_BOOL(uiColored > 1000);
      if (shader == 0)
        reference[masked] = image;
      else
        EZ_TEST_BOOL(image == reference[masked]);
    }
  }
  EZ_TEST_BOOL(reference[0] != reference[1]);
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
