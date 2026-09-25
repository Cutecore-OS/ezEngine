#include <Core/GameApplication/GameApplicationBase.h>
#include <Core/Input/InputManager.h>
#include <Core/ResourceManager/ResourceManager.h>
#include <Core/World/World.h>
#include <Foundation/Configuration/CVar.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/OSFile.h>
#include <Foundation/Threading/Mutex.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <GameEngine/GameApplication/GameApplication.h>
#include <GameEngine/GameState/GameState.h>
#include <MiniAudioPlugin/MiniAudioSingleton.h>
#include <MoviesPlugin/MovieEncoder.h>
#include <MoviesPlugin/MoviesPluginPCH.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderGraph/RenderGraph.h>
#include <RendererCore/RenderGraph/RenderGraphManager.h>
#include <RendererCore/Textures/TextureUtils.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Resources/ReadbackHelper.h>

EZ_PLUGIN_DEPENDENCY(ezMiniAudioPlugin);

namespace
{
  class MovieCapture
  {
  public:
    ezResult Initialize()
    {
      auto* pArgs = ezCommandLineUtils::GetGlobalInstance();
      m_sOutput = pArgs->GetStringOption("-movie-output");
      m_uiWidth = pArgs->GetIntOption("-movie-width", 1920);
      m_uiHeight = pArgs->GetIntOption("-movie-height", 1080);
      m_uiFps = pArgs->GetIntOption("-movie-fps", 30);
      m_uiFrames = pArgs->GetIntOption("-movie-frames", 300);
      const ezInt32 iBitrate = pArgs->GetIntOption("-movie-bitrate", 20000000);
      if (!m_sOutput.IsAbsolutePath() || ezOSFile::ExistsFile(m_sOutput) || m_uiWidth < 16 || m_uiWidth > 8192 ||
          m_uiHeight < 16 || m_uiHeight > 8192 || (m_uiWidth & 1) || (m_uiHeight & 1) || m_uiFps < 1 || m_uiFps > 240 ||
          m_uiFrames < 1 || m_uiFrames > 86400 * 240 || iBitrate < 100000 || iBitrate > 1000000000)
        return EZ_FAILURE;

      ezStringBuilder sLogPath(m_sOutput, ".log");
      if (m_Log.Open(sLogPath, ezFileOpenMode::Write, ezFileShareMode::SharedReads).Failed())
        return EZ_FAILURE;
      m_uiLogSubscription = ezGlobalLog::AddLogWriter(ezMakeDelegate(&MovieCapture::OnLog, this));
      ezLog::Info("Render Movie initialization");

      // The renderer's pipelined mode otherwise associates audio with the next video frame.
      auto* pMultithreading = ezCVar::FindCVarByName("Rendering.Multithreading");
      if (!pMultithreading || pMultithreading->GetType() != ezCVarType::Bool)
        return EZ_FAILURE;
      m_bPreviousMultithreading = *static_cast<ezCVarBool*>(pMultithreading);
      m_bPreviousVSync = ezGameApplication::cvar_AppVSync;
      m_bPreviousShowFps = ezGameApplication::cvar_AppShowFPS;
      m_PreviousTimeStep = ezClock::GetGlobalClock()->GetFixedTimeStep();
      m_sPreviousInputSet = ezInputManager::GetExclusiveInputSet();
      m_bPreviousFullQuality = ezTextureUtils::s_bForceFullQualityAlways;
      m_uiPreviousFallback = ezResourceManager::GetForceNoFallbackAcquisition();
      m_bRestoreSettings = true;
      *static_cast<ezCVarBool*>(pMultithreading) = false;
      ezGameApplication::cvar_AppShowFPS = false;
      ezTextureUtils::s_bForceFullQualityAlways = true;
      ezResourceManager::ForceNoFallbackAcquisition();
      ezGameApplication::cvar_AppVSync = false;
      ezClock::GetGlobalClock()->SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / m_uiFps));

      auto* pSound = ezMiniAudioSingleton::GetSingleton();
      if (!pSound || ezSingletonRegistry::GetSingletonInstance<ezSoundInterface>() != pSound)
        return EZ_FAILURE;
      m_pAudio = pSound->GetEngine();
      if (ma_engine_stop(m_pAudio) != MA_SUCCESS)
        return EZ_FAILURE;
      // Change the callback only after ma_engine_stop has joined device callbacks.
      // A later device restart must not advance the graph: only our explicit reads
      // own the movie timeline. The original callback is restored while stopped.
      m_pAudioDevice = ma_engine_get_device(m_pAudio);
      if (!m_pAudioDevice)
        return EZ_FAILURE;
      m_PreviousAudioCallback = m_pAudioDevice->onData;
      m_pAudioDevice->onData = &MovieCapture::SilenceDevice;
      m_uiSampleRate = ma_engine_get_sample_rate(m_pAudio);
      m_uiChannels = ma_engine_get_channels(m_pAudio);
      if (m_uiSampleRate == 0 || m_uiChannels == 0 || m_uiChannels > 8)
        return EZ_FAILURE;
      EZ_SUCCEED_OR_RETURN(m_Encoder.Open(m_sOutput, ezString(pArgs->GetStringOption("-movie-format", 0, "mp4")),
        ezString(pArgs->GetStringOption("-movie-encoder", 0, "auto")), m_uiWidth, m_uiHeight, m_uiFps, iBitrate, m_uiSampleRate, m_uiChannels));
      ezLog::Info("Render Movie: encoder {}, {}x{}, {} fps, {} frames", m_Encoder.GetCodecName(), m_uiWidth, m_uiHeight, m_uiFps, m_uiFrames);
      m_TextureDesc.SetAsRenderTarget(m_uiWidth, m_uiHeight, ezGALResourceFormat::RGBAUByteNormalizedsRGB);
      m_hTexture = ezGALDevice::GetDefaultDevice()->CreateTexture(m_TextureDesc);
      if (m_hTexture.IsInvalidated())
        return EZ_FAILURE;
      m_pGraph = ezRenderGraphManager::CreateRenderGraph("Render Movie", ezRenderGraphPhase::PostRender);
      ezGALDevice::s_Events.AddEventHandler(ezMakeDelegate(&MovieCapture::OnRenderEvent, this));
      m_bRenderSubscribed = true;
      ezLog::Info("Render Movie is waiting for the scene.");
      m_StartTime = ezTime::Now();
      return EZ_SUCCESS;
    }

    ~MovieCapture()
    {
      if (m_bRestoreSettings)
      {
        if (auto* pMultithreading = ezCVar::FindCVarByName("Rendering.Multithreading"))
          *static_cast<ezCVarBool*>(pMultithreading) = m_bPreviousMultithreading;
        ezGameApplication::cvar_AppVSync = m_bPreviousVSync;
        ezGameApplication::cvar_AppShowFPS = m_bPreviousShowFps;
        ezClock::GetGlobalClock()->SetFixedTimeStep(m_PreviousTimeStep);
        ezInputManager::SetExclusiveInputSet(m_sPreviousInputSet);
        ezTextureUtils::s_bForceFullQualityAlways = m_bPreviousFullQuality;
        ezResourceManager::ForceNoFallbackAcquisition(m_uiPreviousFallback);
      }
      if (m_pAudioDevice)
      {
        ma_device_stop(m_pAudioDevice);
        m_pAudioDevice->onData = m_PreviousAudioCallback;
      }
      if (m_uiLogSubscription != 0)
        ezGlobalLog::RemoveLogWriter(m_uiLogSubscription);
      if (m_bRenderSubscribed)
        ezGALDevice::s_Events.RemoveEventHandler(ezMakeDelegate(&MovieCapture::OnRenderEvent, this));
      m_pGraph = nullptr;
      m_Readback.Reset();
      if (!m_hTexture.IsInvalidated())
        ezGALDevice::GetDefaultDevice()->DestroyTexture(m_hTexture);
    }

    void OnEvent(const ezGameApplicationExecutionEvent& e)
    {
      if (m_bDone)
        return;
      if (m_bHadError)
      {
        Fail("The engine reported an error. See the movie log for details.");
        return;
      }
      auto* pApp = ezGameApplicationBase::GetGameApplicationBaseInstance();
      auto* pState = ezDynamicCast<ezGameState*>(pApp->GetActiveGameState());
      if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeginAppTick)
        ezInputManager::SetExclusiveInputSet("RenderMovie");
      if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeforeWorldUpdates)
      {
        if (!pState || pState->IsInLoadingScreen() || !pState->GetMainWorld())
        {
          if (ezTime::Now() - m_StartTime > ezTime::MakeFromSeconds(300))
            Fail("Timed out waiting for a loaded scene and an ezGameState main view.");
          return;
        }
        ezView* pView = pState->GetMainView();
        if (!pView)
        {
          Fail("The game state has no main view.");
          return;
        }
        // Configure every tick: a custom game state or a window resize can rebind the view.
        ezGALRenderTargets targets;
        targets.m_hRTs[0] = m_hTexture;
        pView->SetRenderTargets(targets);
        pView->SetViewport(ezRectFloat(0, 0, static_cast<float>(m_uiWidth), static_cast<float>(m_uiHeight)));
        for (ezUInt32 i = 0; i < ezWorld::GetWorldCount(); ++i)
        {
          auto* pWorld = ezWorld::GetWorld(static_cast<ezUInt8>(i));
          if (!pWorld)
            continue;
          EZ_LOCK(pWorld->GetWriteMarker());
          pWorld->GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(1.0 / m_uiFps));
        }
        m_bFrameActive = true;
      }
      else if (e.m_Type == ezGameApplicationExecutionEvent::Type::AfterUpdatePlugins && m_bFrameActive)
      {
        // Integer sample boundaries avoid drift when the sample rate is not divisible by FPS.
        const ezUInt64 uiSampleEnd = (static_cast<ezUInt64>(m_uiFrame) + 1) * m_uiSampleRate / m_uiFps;
        const ezUInt32 uiSamples = static_cast<ezUInt32>(uiSampleEnd - m_uiSamplesWritten);
        m_Audio.SetCountUninitialized(uiSamples * m_uiChannels);
        ma_uint64 uiRead = uiSamples;
        ma_result audioResult = MA_SUCCESS;
        if (ezMiniAudioSingleton::GetSingleton()->GetMasterChannelPaused())
          ezMemoryUtils::ZeroFill(m_Audio.GetData(), m_Audio.GetCount());
        else
          audioResult = ma_engine_read_pcm_frames(m_pAudio, m_Audio.GetData(), uiSamples, &uiRead);
        if (audioResult != MA_SUCCESS)
        {
          ezLog::Error("Render Movie: MiniAudio returned error {} while reading {} audio frames.", audioResult, uiSamples);
          Fail("Audio rendering failed. See the MiniAudio error in the movie log.");
          return;
        }
        // An empty or exhausted graph can return fewer frames successfully. Keep the
        // audio track aligned with video by padding the remainder with silence.
        if (uiRead < uiSamples)
          ezMemoryUtils::ZeroFill(m_Audio.GetData() + uiRead * m_uiChannels, (uiSamples - uiRead) * m_uiChannels);
        if (m_Encoder.WriteAudio(m_Audio.GetData(), uiSamples).Failed())
        {
          Fail("Audio encoding failed.");
          return;
        }
        m_uiSamplesWritten = uiSampleEnd;
        m_pGraph->Reset();
        auto hTexture = m_pGraph->ImportTexture(m_hTexture);
        {
          auto pass = m_pGraph->AddTransferPass("Movie readback");
          pass.ReadTexture(hTexture, {}, ezGALResourceState::CopySource);
          pass.HasSideEffects();
          pass.SetExecuteCallback([this, hTexture](const ezRenderGraphContext& context)
            { m_Readback.ReadbackTexture(*context.GetCommandEncoder(), context.ResolveTexture(hTexture)); });
        }
      }
      else if (e.m_Type == ezGameApplicationExecutionEvent::Type::BeforePresent && m_bFrameActive)
      {
        ezGALDevice::GetDefaultDevice()->Flush();
        if (m_Readback.GetReadbackResult(ezTime::MakeFromSeconds(60)) != ezGALAsyncResult::Ready)
        {
          Fail("GPU readback failed or timed out.");
          return;
        }
        ezGALTextureSubresource subresource;
        ezHybridArray<ezGALSystemMemoryDescription, 1> memory;
        {
          auto lock = m_Readback.LockTexture(ezArrayPtr<ezGALTextureSubresource>(&subresource, 1), memory);
          if (!lock)
          {
            Fail("Cannot map the rendered movie frame.");
            return;
          }
          // The offscreen target is RGBA8. Convert directly from mapped GPU staging memory,
          // respecting its row pitch, without an extra full-resolution CPU image copy.
          if (m_Encoder.WriteVideo(memory[0].m_pData.GetPtr(), memory[0].m_uiRowPitch).Failed())
          {
            Fail("Video conversion or encoding failed.");
            return;
          }
        }
        m_bFrameActive = false;
        ++m_uiFrame;
        if (m_uiFrame % 10 == 0 || m_uiFrame == m_uiFrames)
        {
          ezOSFile progress;
          ezStringBuilder sProgressPath(m_sOutput, ".progress");
          ezStringBuilder sProgress;
          sProgress.SetFormat("{}", m_uiFrame);
          if (progress.Open(sProgressPath, ezFileOpenMode::Write).Succeeded())
            progress.Write(sProgress.GetData(), sProgress.GetElementCount()).IgnoreResult();
        }
        if (m_uiFrame == m_uiFrames)
        {
          if (m_Encoder.Finish().Failed())
          {
            Fail("Could not finalize the movie container.");
            return;
          }
          m_bDone = true;
          ezLog::Success("Render Movie completed: {}", m_sOutput);
          pApp->SetReturnCode(0);
          pApp->QuitApplication();
        }
      }
    }

    static void SilenceDevice(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 uiFrameCount)
    {
      if (pOutput)
        ma_silence_pcm_frames(pOutput, uiFrameCount, pDevice->playback.format, pDevice->playback.channels);
    }

    void OnLog(const ezLoggingEventData& e)
    {
      if (e.m_EventType == ezLogMsgType::ErrorMsg)
        m_bHadError = true;
      EZ_LOCK(m_LogMutex);
      m_Log.Write(e.m_sText.GetStartPointer(), e.m_sText.GetElementCount()).IgnoreResult();
      m_Log.Write("\n", 1).IgnoreResult();
    }

    void OnRenderEvent(const ezGALDeviceEvent& e)
    {
      if (!m_bDone && m_bFrameActive && e.m_Type == ezGALDeviceEvent::AfterBeginFrame)
        ezRenderGraphManager::EnqueueRenderGraph(m_pGraph);
    }

    void Fail(const char* szReason)
    {
      m_bDone = true;
      ezLog::Error("Render Movie: {}", szReason);
      auto* pApp = ezGameApplicationBase::GetGameApplicationBaseInstance();
      pApp->SetReturnCode(1);
      pApp->QuitApplication();
    }

  private:
    bool m_bRestoreSettings = false;
    bool m_bPreviousMultithreading = false;
    bool m_bPreviousVSync = false;
    bool m_bPreviousShowFps = false;
    bool m_bPreviousFullQuality = false;
    ezUInt32 m_uiPreviousFallback = 0;
    ezString m_sPreviousInputSet;
    ezTime m_PreviousTimeStep;
    ezMutex m_LogMutex;
    ezOSFile m_Log;
    ezEventSubscriptionID m_uiLogSubscription = 0;
    ezMovieEncoder m_Encoder;
    ezString m_sOutput;
    ma_engine* m_pAudio = nullptr;
    ma_device* m_pAudioDevice = nullptr;
    ma_device_data_proc m_PreviousAudioCallback = nullptr;
    ezUInt32 m_uiWidth = 0, m_uiHeight = 0, m_uiFps = 0, m_uiFrames = 0, m_uiFrame = 0;
    ezUInt32 m_uiSampleRate = 0, m_uiChannels = 0;
    ezUInt64 m_uiSamplesWritten = 0;
    bool m_bFrameActive = false, m_bDone = false, m_bRenderSubscribed = false;
    ezTime m_StartTime;
    ezDynamicArray<float> m_Audio;
    ezAtomicBool m_bHadError = false;
    ezGALTextureCreationDescription m_TextureDesc;
    ezGALTextureHandle m_hTexture;
    ezSharedPtr<ezRenderGraph> m_pGraph;
    ezGALReadbackTextureHelper m_Readback;
  };

  ezUniquePtr<MovieCapture> s_pCapture;
  ezEventSubscriptionID s_uiSubscription = 0;
} // namespace

// clang-format off
EZ_BEGIN_SUBSYSTEM_DECLARATION(Movies, MoviesPlugin)
  BEGIN_SUBSYSTEM_DEPENDENCIES
    "MiniAudioPlugin", "GameEngine"
  END_SUBSYSTEM_DEPENDENCIES
  ON_HIGHLEVELSYSTEMS_STARTUP
  {
    if (ezCommandLineUtils::GetGlobalInstance()->HasOption("-movie-output"))
    {
      auto* pApp = ezGameApplicationBase::GetGameApplicationBaseInstance();
      pApp->SetReturnCode(1); // Early window close must never publish an incomplete movie.
      s_pCapture = EZ_DEFAULT_NEW(MovieCapture);
      if (s_pCapture->Initialize().Failed())
        s_pCapture->Fail("Initialization failed. Check settings, MiniAudio, FFmpeg and the selected GPU encoder.");
      else
        s_uiSubscription = pApp->m_ExecutionEvents.AddEventHandler(ezMakeDelegate(&MovieCapture::OnEvent, s_pCapture.Borrow()));
    }
  }
  ON_HIGHLEVELSYSTEMS_SHUTDOWN
  {
    if (s_uiSubscription != 0)
      ezGameApplicationBase::GetGameApplicationBaseInstance()->m_ExecutionEvents.RemoveEventHandler(s_uiSubscription);
    s_uiSubscription = 0;
    s_pCapture.Clear();
  }
EZ_END_SUBSYSTEM_DECLARATION;
// clang-format on
