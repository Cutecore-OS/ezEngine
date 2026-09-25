#include <Core/World/World.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/Threading/ThreadUtils.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <GameEngine/GameApplication/GameApplication.h>
#include <GameEngine/GameState/FallbackGameState.h>
#include <MiniAudioPlugin/MiniAudioSingleton.h>
#include <MoviesPluginTest/MoviesPluginTestPCH.h>
#include <RendererCore/Debug/DebugRenderer.h>

/// Deterministic audiovisual fixture, with no project assets or sound hardware timing dependency.
class ezMovieTestState : public ezFallbackGameState
{
public:
  void OnActivation(ezWorld* pWorld, ezStringView sStartPosition, const ezTransform& offset) override
  {
    ezWorldDesc desc("Movie Test");
    m_pWorld = EZ_DEFAULT_NEW(ezWorld, desc);
    ezFallbackGameState::OnActivation(m_pWorld.Borrow(), sStartPosition, offset);
    if (ezCommandLineUtils::GetGlobalInstance()->HasOption("-movie-test-silent"))
      return;
    auto* pEngine = ezMiniAudioSingleton::GetSingleton()->GetEngine();
    const auto uiChannels = ma_engine_get_channels(pEngine);
    const auto uiSampleRate = ma_engine_get_sample_rate(pEngine);
    m_Samples.SetCount(uiSampleRate * uiChannels);
    for (ezUInt32 i = 0; i < uiSampleRate; ++i)
      for (ezUInt32 c = 0; c < uiChannels; ++c)
        m_Samples[i * uiChannels + c] = 0.25f * ezMath::Sin(ezAngle::MakeFromRadian(440.0f * ezMath::Pi<float>() * 2.0f * i / uiSampleRate));
    auto config = ma_audio_buffer_config_init(ma_format_f32, uiChannels, uiSampleRate, m_Samples.GetData(), nullptr);
    config.sampleRate = uiSampleRate;
    m_bWaveform = ma_audio_buffer_init(&config, &m_Waveform) == MA_SUCCESS;
    m_bSound = m_bWaveform && ma_sound_init_from_data_source(pEngine, &m_Waveform, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &m_Sound) == MA_SUCCESS;
    if (m_bSound)
    {
      ma_sound_set_looping(&m_Sound, MA_TRUE);
      ma_sound_start(&m_Sound);
    }
  }

  void OnDeactivation() override
  {
    if (m_bSound)
      ma_sound_uninit(&m_Sound);
    if (m_bWaveform)
      ma_audio_buffer_uninit(&m_Waveform);
    ezFallbackGameState::OnDeactivation();
    m_pWorld.Clear();
  }

  void ProcessInput() override {} // The fixture must be independent of user input.

  void AfterWorldUpdate() override
  {
    ezFallbackGameState::AfterWorldUpdate();
    if (ezCommandLineUtils::GetGlobalInstance()->HasOption("-movie-test-controls"))
    {
      auto* pAudio = ezMiniAudioSingleton::GetSingleton();
      pAudio->SetMasterChannelVolume(m_uiFrame < 8 ? 0.5f : 1.0f);
      pAudio->SetMasterChannelMute(m_uiFrame >= 8 && m_uiFrame < 16);
      pAudio->SetMasterChannelPaused(m_uiFrame >= 16 && m_uiFrame < 24);
    }
    ++m_uiFrame;
    const float fTime = static_cast<float>(m_pWorld->GetClock().GetAccumulatedTime().GetSeconds());
    ezTransform transform = ezTransform::MakeIdentity();
    transform.m_vPosition.Set(4.0f, ezMath::Sin(ezAngle::MakeFromRadian(fTime * 2.0f)), 0.0f);
    ezDebugRenderer::DrawSolidBox(m_pWorld.Borrow(), ezBoundingBox::MakeFromCenterAndHalfExtents(ezVec3::MakeZero(), ezVec3(0.5f)), ezColor::Orange, transform);
    const int iStall = ezCommandLineUtils::GetGlobalInstance()->GetIntOption("-movie-test-stall", 0);
    if (iStall > 0)
      ezThreadUtils::Sleep(ezTime::MakeFromMilliseconds(iStall));
  }

private:
  ezUInt32 m_uiFrame = 0;
  ezUniquePtr<ezWorld> m_pWorld;
  ezDynamicArray<float> m_Samples;
  ma_audio_buffer m_Waveform = {};
  ma_sound m_Sound = {};
  bool m_bWaveform = false;
  bool m_bSound = false;
};

class ezMovieTestApplication : public ezGameApplication
{
public:
  ezMovieTestApplication()
    : ezGameApplication("MoviesPluginTest", nullptr)
  {
  }

protected:
  ezResult BeforeCoreSystemsStartup() override
  {
    m_sAppProjectPath = ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-project");
    return ezGameApplication::BeforeCoreSystemsStartup();
  }
  void Init_LoadProjectPlugins() override
  {
    if (ezPlugin::LoadPlugin("ezMoviesPlugin").Failed())
    {
      SetReturnCode(1);
      QuitApplication();
    }
  }
  ezUniquePtr<ezGameStateBase> CreateGameState() override
  {
    return EZ_DEFAULT_NEW(ezMovieTestState);
  }
};
EZ_APPLICATION_ENTRY_POINT(ezMovieTestApplication);
