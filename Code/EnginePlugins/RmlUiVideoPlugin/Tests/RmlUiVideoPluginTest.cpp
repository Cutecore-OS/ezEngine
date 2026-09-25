#include <RmlUiVideoPlugin/VideoElement.h>

#include <Foundation/Configuration/CVar.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/Archive/ArchiveBuilder.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/Threading/ThreadUtils.h>
#include <Foundation/Time/Clock.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <RendererCore/RenderContext/RenderContext.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererCore/ShaderCompiler/ShaderManager.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <RendererFoundation/Resources/ReadbackHelper.h>
#include <RmlUiPlugin/Components/RmlUiCanvas2DComponent.h>
#include <RmlUiPlugin/Components/RmlUiCanvas3DComponent.h>
#include <RmlUiPlugin/RmlUiContext.h>
#include <RmlUiPlugin/RmlUiSingleton.h>
#include <RmlUiVideoPlugin/VideoDecoder.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>

EZ_TESTFRAMEWORK_ENTRY_POINT("RmlUiVideoPluginTest", "RmlUi Video Tests")
EZ_CREATE_SIMPLE_TEST_GROUP(RmlUiVideo);

namespace
{
  bool WaitForFrame(ezRmlUiVideoDecoder& ref_decoder, ezRmlUiVideoDecoder::Frame& out_frame, double fTime, bool bSeek = false)
  {
    ref_decoder.RequestFrame(ezTime::MakeFromSeconds(fTime), bSeek);
    const ezTime deadline = ezTime::Now() + ezTime::MakeFromSeconds(10);
    while (ezTime::Now() < deadline)
    {
      bool bEnded = false, bFailed = false;
      if (ref_decoder.TakeFrame(out_frame, bEnded, bFailed))
        return true;
      if (bFailed)
        return false;
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(2));
    }
    return false;
  }

  void CheckColor(const ezRmlUiVideoDecoder::Frame& frame, bool bRed)
  {
    EZ_TEST_INT(frame.m_uiWidth, 96);
    EZ_TEST_INT(frame.m_uiHeight, 64);
    EZ_TEST_INT(frame.m_Pixels.GetCount(), 96 * 64 * 4);
    if (frame.m_Pixels.GetCount() < 4)
      return;
    EZ_TEST_BOOL(frame.m_Pixels[bRed ? 0 : 2] > 200);
    EZ_TEST_BOOL(frame.m_Pixels[bRed ? 2 : 0] < 40);
    EZ_TEST_INT(frame.m_Pixels[3], 255);
  }
} // namespace

EZ_CREATE_SIMPLE_TEST(RmlUiVideo, DecodeAndSeek)
{
  EZ_TEST_BLOCK(ezTestBlock::Enabled, "H264 MP4 and VP8 WebM")
  {
    for (const char* szName : {"colors.mp4", "colors.webm", "colors-bframes.mp4"})
    {
      ezStringBuilder path(RMLUI_VIDEO_TEST_DATA_DIR, "/", szName);
      ezRmlUiVideoDecoder decoder(path);
      ezRmlUiVideoDecoder::Frame frame;
      if (EZ_TEST_BOOL(WaitForFrame(decoder, frame, 0)))
        CheckColor(frame, true);

      // Decode across a GOP, then seek backwards and flush delayed decoder output.
      if (EZ_TEST_BOOL(WaitForFrame(decoder, frame, 1.5, true)))
        CheckColor(frame, false);
      if (EZ_TEST_BOOL(WaitForFrame(decoder, frame, 0, true)))
        CheckColor(frame, true);

      decoder.RequestFrame(ezTime::MakeFromSeconds(10));
      bool bEnded = false, bFailed = false;
      const ezTime deadline = ezTime::Now() + ezTime::MakeFromSeconds(10);
      while (!bEnded && !bFailed && ezTime::Now() < deadline)
      {
        decoder.TakeFrame(frame, bEnded, bFailed);
        ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(2));
      }
      EZ_TEST_BOOL(bEnded);
      EZ_TEST_BOOL(!bFailed);
      if (EZ_TEST_BOOL(WaitForFrame(decoder, frame, 0, true)))
        CheckColor(frame, true);
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Independent instances and early destruction")
  {
    ezStringBuilder path(RMLUI_VIDEO_TEST_DATA_DIR, "/colors.mp4");
    ezRmlUiVideoDecoder first(path);
    ezRmlUiVideoDecoder second(path);
    ezRmlUiVideoDecoder::Frame frame;
    if (EZ_TEST_BOOL(WaitForFrame(first, frame, 0)))
      CheckColor(frame, true);
    if (EZ_TEST_BOOL(WaitForFrame(second, frame, 1.5, true)))
      CheckColor(frame, false);
    first.RequestFrame(ezTime::MakeFromSeconds(100));
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Invalid media terminates with an error")
  {
    ezStringBuilder path(RMLUI_VIDEO_TEST_DATA_DIR, "/invalid.mp4");
    ezRmlUiVideoDecoder decoder(path);
    ezRmlUiVideoDecoder::Frame frame;
    bool bEnded = false, bFailed = false;
    const ezTime deadline = ezTime::Now() + ezTime::MakeFromSeconds(10);
    while (!bFailed && ezTime::Now() < deadline)
    {
      decoder.TakeFrame(frame, bEnded, bFailed);
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(2));
    }
    EZ_TEST_BOOL(bFailed);
    EZ_TEST_BOOL(!bEnded);
  }
}


EZ_CREATE_SIMPLE_TEST(RmlUiVideo, CanvasRendering)
{
  EZ_TEST_BLOCK(ezTestBlock::Enabled, "2D and 3D canvas video through the engine renderer")
  {
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Base", "VideoTest").Succeeded());
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Plugins/RmlUiPlugin", "VideoTest").Succeeded());
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(RMLUI_VIDEO_TEST_DATA_DIR, "VideoTest", "project").Succeeded());
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Output", "VideoTest", "shadercache", ezDataDirUsage::AllowWrites).Succeeded());
    EZ_SCOPE_EXIT(ezFileSystem::RemoveDataDirectoryGroup("VideoTest"));

#if EZ_ENABLED(EZ_PLATFORM_WINDOWS)
    const char* szDefaultRenderer = "DX11";
#else
    const char* szDefaultRenderer = "Vulkan";
#endif
    ezStringView sRenderer = ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-renderer", 0, szDefaultRenderer);
    const char* szShaderModel = "";
    const char* szCompiler = "";
    ezGALDeviceFactory::GetShaderModelAndCompiler(sRenderer, szShaderModel, szCompiler);
    ezShaderManager::Configure(szShaderModel, true);
    if (!EZ_TEST_BOOL(ezPlugin::LoadPlugin(szCompiler).Succeeded()))
      return;
    ezGALDeviceCreationDescription deviceDesc;
    ezGALDevice* pDevice = ezGALDeviceFactory::CreateDevice(sRenderer, ezFoundation::GetDefaultAllocator(), deviceDesc);
    if (!EZ_TEST_BOOL(pDevice != nullptr))
      return;
    if (!EZ_TEST_BOOL(pDevice->Init().Succeeded()))
    {
      EZ_DEFAULT_DELETE(pDevice);
      return;
    }
    ezGALDevice::SetDefaultDevice(pDevice);
    EZ_SCOPE_EXIT(pDevice->Shutdown().IgnoreResult(); EZ_DEFAULT_DELETE(pDevice));
    // The production renderer binds the blue-noise asset even for simple images.
    // Supply deterministic data without requiring an asset-curator generated lookup table.
    ezUInt8 noisePixel[4] = {255, 255, 255, 255};
    ezGALSystemMemoryDescription noiseMemory;
    noiseMemory.m_pData = ezMakeByteBlobPtr(noisePixel, 4);
    noiseMemory.m_uiRowPitch = 4;
    noiseMemory.m_uiSlicePitch = 4;
    ezTexture2DResourceDescriptor noiseDesc;
    noiseDesc.m_DescGAL.m_uiWidth = 1;
    noiseDesc.m_DescGAL.m_uiHeight = 1;
    noiseDesc.m_DescGAL.m_Format = ezGALResourceFormat::RGBAUByteNormalized;
    noiseDesc.m_InitialContent = ezMakeArrayPtr(&noiseMemory, 1);
    auto hNoise = ezResourceManager::CreateResource<ezTexture2DResource>("{ ac614d7c-2b31-4a7b-aa0c-c5d8200b7b89 }", std::move(noiseDesc));
    ezStartup::StartupHighLevelSystems();
    EZ_SCOPE_EXIT(ezStartup::ShutdownHighLevelSystems(); hNoise.Invalidate(); ezResourceManager::FreeAllUnusedResources());

    auto* pMultithreading = static_cast<ezCVarBool*>(ezCVar::FindCVarByName("Rendering.Multithreading"));
    const bool bMultithreading = pMultithreading->GetValue();
    *pMultithreading = false;
    EZ_SCOPE_EXIT(*pMultithreading = bMultithreading);
    const ezTime fixedStep = ezClock::GetGlobalClock()->GetFixedTimeStep();
    ezClock::GetGlobalClock()->SetFixedTimeStep(ezTime::MakeFromSeconds(0.05));
    EZ_SCOPE_EXIT(ezClock::GetGlobalClock()->SetFixedTimeStep(fixedStep));

    // Exercise the actual components' context creation and shared on-demand update path.
    // Rendering goes into the same kind of target used by the components in a scene.
    ezRmlUiCanvas2DComponent canvas2D;
    ezRmlUiCanvas3DComponent canvas3D;
    ezRmlUiCanvasComponentBase* canvases[] = {&canvas2D, &canvas3D};
    ezRmlUiContext* contexts[2] = {};
    ezRmlUiVideoElement* videos[2] = {};
    ezGALTextureHandle targets[2];
    for (ezUInt32 i = 0; i < 2; ++i)
    {
      contexts[i] = canvases[i]->GetOrCreateRmlContext();
      contexts[i]->SetSize(ezVec2U32(96, 64));
      EZ_TEST_BOOL(contexts[i]->LoadDocumentFromString(
                                "<rml><head><style>body { margin: 0; width: 96px; height: 64px; }"
                                "video { display: block; width: 96px; height: 64px; }</style></head>"
                                "<body><video id='movie' src='colors.mp4'/></body></rml>")
                     .Succeeded());
      contexts[i]->ShowDocument();
      videos[i] = rmlui_dynamic_cast<ezRmlUiVideoElement*>(contexts[i]->GetDocument(0)->GetElementById("movie"));
      EZ_TEST_BOOL(videos[i] != nullptr);
      ezGALTextureCreationDescription desc;
      desc.m_uiWidth = 96;
      desc.m_uiHeight = 64;
      desc.m_Format = ezGALResourceFormat::RGBAUByteNormalizedsRGB;
      desc.m_TextureFlags.Add(ezGALTextureUsageFlags::RenderTarget);
      targets[i] = pDevice->CreateTexture(desc);
      EZ_TEST_BOOL(!targets[i].IsInvalidated());
    }
    EZ_SCOPE_EXIT(
      for (ezUInt32 i = 0; i < 2; ++i) {
        ezRmlUi::GetSingleton()->DeleteContext(contexts[i]);
        pDevice->DestroyTexture(targets[i]);
      });

    ezGALReadbackTextureHelper readbacks[2];
    auto Tick = [&](bool bReadback)
    {
      ezClock::GetGlobalClock()->Update();
      for (ezUInt32 i = 0; i < 2; ++i)
      {
        canvases[i]->ezRmlUiCanvasComponentBase::Update();
        ezRmlUi::GetSingleton()->ExtractContext(*contexts[i], targets[i]);
      }
      ezRenderWorld::BeginFrame();
      ezRenderWorld::Render(ezRenderContext::GetDefaultInstance());
      if (bReadback)
      {
        auto* pEncoder = pDevice->BeginCommands("Video readback");
        for (ezUInt32 i = 0; i < 2; ++i)
          readbacks[i].ReadbackTexture(*pEncoder, targets[i]);
        pDevice->EndCommands(pEncoder);
      }
      ezRenderWorld::EndFrame();
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(2));
    };
    auto PixelMatches = [&](ezUInt32 uiCanvas, bool bRed)
    {
      if (readbacks[uiCanvas].GetReadbackResult(ezTime::MakeFromSeconds(5)) != ezGALAsyncResult::Ready)
        return false;
      ezGALTextureSubresource subresource;
      ezDynamicArray<ezGALSystemMemoryDescription> memory;
      auto lock = readbacks[uiCanvas].LockTexture(ezMakeArrayPtr(&subresource, 1), memory);
      if (memory.IsEmpty())
        return false;
      const auto* pPixel = memory[0].m_pData.GetPtr() + 32 * memory[0].m_uiRowPitch + 48 * 4;
      return pPixel[bRed ? 0 : 2] > 200 && pPixel[bRed ? 2 : 0] < 40 && pPixel[3] > 200;
    };
    auto WaitForColors = [&](bool bFirstRed, bool bSecondRed)
    {
      const ezTime deadline = ezTime::Now() + ezTime::MakeFromSeconds(10);
      while (ezTime::Now() < deadline)
      {
        Tick(true);
        if (PixelMatches(0, bFirstRed) && PixelMatches(1, bSecondRed))
          return true;
      }
      return false;
    };

    EZ_TEST_BOOL(WaitForColors(true, true));
    EZ_TEST_BOOL(videos[0]->IsPaused() && videos[1]->IsPaused());
    // A source change and control commands can occur together before the next update.
    videos[0]->SetAttribute("src", "colors-bframes.mp4");
    videos[0]->Play();
    videos[0]->Pause();
    videos[0]->Seek(1.5);
    EZ_TEST_BOOL(WaitForColors(false, true));
    const double fPausedTime = videos[0]->GetCurrentTime();
    for (ezUInt32 i = 0; i < 5; ++i)
      Tick(false);
    EZ_TEST_DOUBLE(videos[0]->GetCurrentTime(), fPausedTime, 0.0001);

    videos[0]->Play();
    for (ezUInt32 i = 0; i < 60 && !videos[0]->HasEnded(); ++i)
      Tick(false);
    EZ_TEST_BOOL(videos[0]->HasEnded());
    videos[0]->Play();
    videos[0]->Pause();
    EZ_TEST_BOOL(WaitForColors(true, true));

    videos[1]->SetAttribute("src", "colors.webm");
    EZ_TEST_BOOL(WaitForColors(true, true));
    videos[1]->SetAttribute("loop", "");
    videos[1]->Seek(1.9);
    videos[1]->Play();
    bool bLooped = false;
    for (ezUInt32 i = 0; i < 80; ++i)
    {
      Tick(false);
      if (videos[1]->GetCurrentTime() < 0.5)
      {
        bLooped = true;
        break;
      }
    }
    EZ_TEST_BOOL(bLooped);
    EZ_TEST_BOOL(!videos[1]->HasEnded());
    videos[1]->SetAttribute("paused", "");
    const double fAttributePausedTime = videos[1]->GetCurrentTime();
    for (ezUInt32 i = 0; i < 5; ++i)
      Tick(false);
    EZ_TEST_DOUBLE(videos[1]->GetCurrentTime(), fAttributePausedTime, 0.0001);
    videos[1]->RemoveAttribute("paused");
    EZ_TEST_BOOL(!videos[1]->IsPaused());

    // Drain submitted commands before destroying contexts and their texture resources.
    for (ezUInt32 i = 0; i < 2; ++i)
      videos[i]->Pause();
    for (ezUInt32 i = 0; i < 4; ++i)
      Tick(false);
  }
}



EZ_CREATE_SIMPLE_TEST(RmlUiVideo, ArchivePlayback)
{
  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Video and backwards seeking through an archive data directory")
  {
    ezStringBuilder sArchive(ezTestFramework::GetInstance()->GetAbsOutputPath(), "/video.ezArchive");
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(ezTestFramework::GetInstance()->GetAbsOutputPath(),
      "VideoArchiveFiles", "videoout", ezDataDirUsage::AllowWrites)
                   .Succeeded());
    EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(RMLUI_VIDEO_TEST_DATA_DIR, "VideoArchiveFiles").Succeeded());
    EZ_SCOPE_EXIT(ezFileSystem::RemoveDataDirectoryGroup("VideoArchiveFiles"));
    ezArchiveBuilder builder;
    auto& entry = builder.m_Entries.ExpandAndGetRef();
    entry.m_sAbsSourcePath = ezStringBuilder(RMLUI_VIDEO_TEST_DATA_DIR, "/colors.mp4");
    entry.m_sRelTargetPath = "Movies/colors.mp4";
    entry.m_CompressionMode = ezArchiveCompressionMode::Compressed_zstd;
    if (!EZ_TEST_BOOL(builder.WriteArchive(":videoout/video.ezArchive").Succeeded()))
      return;
    if (!EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(sArchive, "VideoArchiveTest", "videoarchive").Succeeded()))
      return;
    EZ_SCOPE_EXIT(ezFileSystem::RemoveDataDirectoryGroup("VideoArchiveTest"));

    ezRmlUiVideoDecoder decoder(":videoarchive/Movies/colors.mp4");
    ezRmlUiVideoDecoder::Frame frame;
    if (EZ_TEST_BOOL(WaitForFrame(decoder, frame, 0)))
      CheckColor(frame, true);
    if (EZ_TEST_BOOL(WaitForFrame(decoder, frame, 1.5, true)))
      CheckColor(frame, false);
    if (EZ_TEST_BOOL(WaitForFrame(decoder, frame, 0, true)))
      CheckColor(frame, true);
  }
}
