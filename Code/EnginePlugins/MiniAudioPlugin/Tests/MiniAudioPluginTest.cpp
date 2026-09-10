#include <Core/GameApplication/GameApplicationBase.h>
#include <Core/ResourceManager/ResourceTypeLoader.h>
#include <Core/World/World.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/MemoryStream.h>
#include <Foundation/Math/Random.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <Foundation/Utilities/AssetFileHeader.h>
#include <MiniAudioPlugin/Components/MiniAudioEffectVolumeComponent.h>
#include <MiniAudioPlugin/Components/MiniAudioListenerComponent.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundComponent.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundVolumeComponent.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffectNode.h>
#include <MiniAudioPlugin/Effects/MiniAudioMixerNode.h>
#include <MiniAudioPlugin/Effects/MiniAudioTimeStretch.h>
#include <MiniAudioPlugin/MiniAudioSingleton.h>
#include <MiniAudioPlugin/Resources/MiniAudioSoundResource.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>
#include <cmath>

#include <Core/Interfaces/PhysicsWorldModule.h>

class MiniAudioTestPhysics : public ezPhysicsWorldModuleInterface
{
  EZ_ADD_DYNAMIC_REFLECTION(MiniAudioTestPhysics, ezPhysicsWorldModuleInterface);
  EZ_DECLARE_WORLD_MODULE();

public:
  MiniAudioTestPhysics(ezWorld* world)
    : ezPhysicsWorldModuleInterface(world)
  {
  }
  mutable ezUInt32 m_uiQueries = 0;
  ezUInt32 m_uiBlockedRays = 0;
  ezGameObjectHandle m_hSource;
  mutable ezVec3 m_vLastEnd = ezVec3::MakeZero();
  bool m_bGeometricObstacle = false;
  float m_fObstacleX = 5, m_fObstacleHalfSize = 0.1f;
  float m_fForcedHitDistance = -1;
  mutable ezDynamicArray<ezVec3> m_Starts, m_Ends;
  mutable float m_fLastRayLength = 0;
  ezUInt32 GetCollisionLayerByName(ezStringView sName) const override { return 0; }
  ezUInt8 GetWeightCategoryByName(ezStringView sName) const override { return 0; }
  ezUInt8 GetImpulseTypeByName(ezStringView sName) const override { return 0; }
  bool Raycast(ezPhysicsCastResult& out_result, const ezVec3& vStart, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const override { return false; }
  bool RaycastAll(ezPhysicsCastResultArray& out_results, const ezVec3& vStart, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params) const override
  {
    ++m_uiQueries;
    m_vLastEnd = vStart + vDir * fDistance;
    m_Starts.PushBack(vStart);
    m_Ends.PushBack(m_vLastEnd);
    m_fLastRayLength = fDistance;
    EZ_TEST_INT(params.m_uiCollisionLayer, 7);
    EZ_TEST_BOOL(params.m_bIgnoreInitialOverlap);
    EZ_TEST_BOOL(params.m_ShapeTypes.IsSet(ezPhysicsShapeType::Static));
    EZ_TEST_BOOL(params.m_ShapeTypes.IsSet(ezPhysicsShapeType::Dynamic));
    out_results.m_Results.Clear();
    ezPhysicsCastResult selfHit = {};
    selfHit.m_hActorObject = m_hSource;
    selfHit.m_fDistance = fDistance * 0.25f;
    out_results.m_Results.PushBack(selfHit);
    bool bBlocked = (m_uiQueries - 1) % 9 < m_uiBlockedRays;
    float fHitDistance = m_fForcedHitDistance >= 0 ? m_fForcedHitDistance : fDistance * 0.5f;
    if (m_bGeometricObstacle)
    {
      fHitDistance = ezMath::Abs(vDir.x) > 0.0001f ? (m_fObstacleX - vStart.x) / vDir.x : -1;
      const ezVec3 vHit = vStart + vDir * fHitDistance;
      bBlocked = fHitDistance > 0 && fHitDistance < fDistance && ezMath::Abs(vHit.y) <= m_fObstacleHalfSize && ezMath::Abs(vHit.z) <= m_fObstacleHalfSize;
    }
    if (bBlocked)
    {
      ezPhysicsCastResult hit = {};
      hit.m_fDistance = fHitDistance;
      hit.m_vPosition = vStart + vDir * fHitDistance;
      out_results.m_Results.PushBack(hit);
    }
    return true;
  }
  bool SweepTestSphere(ezPhysicsCastResult& out_result, float fSphereRadius, const ezVec3& vStart, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const override { return false; }
  bool SweepTestBox(ezPhysicsCastResult& out_result, const ezVec3& vBoxExtents, const ezTransform& transform, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const override { return false; }
  bool SweepTestCapsule(ezPhysicsCastResult& out_result, float fCapsuleRadius, float fCapsuleHeight, const ezTransform& transform, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const override { return false; }
  bool SweepTestCylinder(ezPhysicsCastResult& out_result, float fCylinderRadius, float fCylinderHeight, const ezTransform& transform, const ezVec3& vDir, float fDistance, const ezPhysicsQueryParameters& params, ezPhysicsHitCollection collection = ezPhysicsHitCollection::Closest) const override { return false; }
  bool OverlapTestSphere(float fSphereRadius, const ezVec3& vPosition, const ezPhysicsQueryParameters& params) const override { return false; }
  bool OverlapTestBox(const ezVec3& vBoxExtents, const ezVec3& vPosition, const ezTransform& transform, const ezPhysicsQueryParameters& params) const override { return false; }
  bool OverlapTestCapsule(float fCapsuleRadius, float fCapsuleHeight, const ezTransform& transform, const ezPhysicsQueryParameters& params) const override { return false; }
  bool OverlapTestCylinder(float fCylinderRadius, float fCylinderHeight, const ezTransform& transform, const ezPhysicsQueryParameters& params) const override { return false; }
  void QueryShapesInSphere(ezPhysicsOverlapResultArray& out_results, float fSphereRadius, const ezVec3& vPosition, const ezPhysicsQueryParameters& params) const override {}
  void QueryShapesInBox(ezPhysicsOverlapResultArray& out_results, const ezVec3& vBoxExtents, const ezTransform& transform, const ezPhysicsQueryParameters& params) const override {}
  void QueryShapesInCapsule(ezPhysicsOverlapResultArray& out_results, float fCapsuleRadius, float fCapsuleHeight, const ezTransform& transform, const ezPhysicsQueryParameters& params) const override {}
  void QueryShapesInCylinder(ezPhysicsOverlapResultArray& out_results, float fCylinderRadius, float fCylinderHeight, const ezTransform& transform, const ezPhysicsQueryParameters& params) const override {}
  ezVec3 GetGravity() const override { return ezVec3::MakeZero(); }
};
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(MiniAudioTestPhysics, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;
EZ_IMPLEMENT_WORLD_MODULE(MiniAudioTestPhysics);

EZ_TESTFRAMEWORK_ENTRY_POINT("MiniAudioPluginTest", "MiniAudio Effects Tests")
EZ_CREATE_SIMPLE_TEST_GROUP(MiniAudio);

namespace
{
  constexpr ezUInt32 s_uiRate = 48000;

  // Render through the actual MiniAudio graph, including silence after source completion.
  void Render(ezArrayPtr<const ezMiniAudioEffectInstance> effects, ezArrayPtr<const float> input, ezDynamicArray<float>& output,
    ezUInt32 uiFrames = 48000, float fGroupVolume = 1.0f)
  {
    ma_engine engine;
    auto config = ma_engine_config_init();
    config.noDevice = MA_TRUE;
    config.channels = 2;
    config.sampleRate = s_uiRate;
    config.gainSmoothTimeInFrames = 0;
    if (!EZ_TEST_INT(ma_engine_init(&config, &engine), MA_SUCCESS))
      return;
    ma_sound_group group;
    EZ_TEST_INT(ma_sound_group_init(&engine, MA_SOUND_FLAG_NO_PITCH, nullptr, &group), MA_SUCCESS);
    ma_sound_group_set_volume(&group, fGroupVolume);
    {
      ezMiniAudioEffectNode node;
      EZ_TEST_BOOL(node.Initialize(&engine, &group).Succeeded());
      node.Configure(effects);
      auto bufferConfig = ma_audio_buffer_config_init(ma_format_f32, 2, input.GetCount() / 2, input.GetPtr(), nullptr);
      ma_audio_buffer buffer;
      EZ_TEST_INT(ma_audio_buffer_init(&bufferConfig, &buffer), MA_SUCCESS);
      ma_sound sound;
      EZ_TEST_INT(ma_sound_init_from_data_source(&engine, &buffer, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, nullptr, &sound), MA_SUCCESS);
      EZ_TEST_INT(ma_node_attach_output_bus(&sound, 0, &node.m_Node, 0), MA_SUCCESS);
      EZ_TEST_INT(ma_sound_start(&sound), MA_SUCCESS);
      output.SetCount(uiFrames * 2);
      // Irregular blocks exercise delay continuity across graph reads.
      for (ezUInt32 start = 0; start < uiFrames;)
      {
        const ezUInt32 count = ezMath::Min(137u, uiFrames - start);
        EZ_TEST_INT(ma_engine_read_pcm_frames(&engine, output.GetData() + start * 2, count, nullptr), MA_SUCCESS);
        start += count;
      }
      ma_sound_uninit(&sound);
      ma_audio_buffer_uninit(&buffer);
    }
    ma_sound_group_uninit(&group);
    ma_engine_uninit(&engine);
  }

  float Energy(ezArrayPtr<const float> data, ezUInt32 uiFirstFrame, ezUInt32 uiEndFrame)
  {
    float sum = 0;
    for (ezUInt32 i = uiFirstFrame; i < uiEndFrame; ++i)
      sum += data[i * 2] * data[i * 2];
    return sum;
  }

  class AudioTestApplication : public ezGameApplicationBase
  {
  public:
    AudioTestApplication()
      : ezGameApplicationBase("MiniAudioRuntimeTest")
    {
    }
    ezString FindProjectDirectory() const override { return {}; }
    void Init_SetupGraphicsDevice() override {}
    void Deinit_ShutdownGraphicsDevice() override {}
    void Run_WorldUpdateAndRender() override {}
  };
} // namespace

EZ_CREATE_SIMPLE_TEST(MiniAudio, SignalProcessing)
{
  const float impulse[] = {1.0f, 0.25f};
  ezMiniAudioEffectInstance effect;
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Delay;
  effect.m_Effect.m_fDelay = 10;
  effect.m_Effect.m_fFeedback = 0.5f;
  effect.m_Effect.m_fMix = 0.5f;
  ezDynamicArray<float> output;

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Delay, feedback, stereo independence and sound group gain")
  {
    Render(ezMakeArrayPtr(&effect, 1), impulse, output, 1600, 0.5f);
    EZ_TEST_FLOAT(output[0], 0.25f, 0.00001f);
    EZ_TEST_FLOAT(output[480 * 2], 0.25f, 0.00001f);
    EZ_TEST_FLOAT(output[480 * 2 + 1], 0.0625f, 0.00001f);
    EZ_TEST_FLOAT(output[960 * 2], 0.125f, 0.00001f);
    effect.m_fWeight = 0;
    Render(ezMakeArrayPtr(&effect, 1), impulse, output, 1600);
    EZ_TEST_FLOAT(output[0], 1, 0.00001f);
    EZ_TEST_FLOAT(Energy(output, 1, 1600), 0, 0.00001f);
    effect.m_fWeight = 1;
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Reverb and modulated delay produce finite tails after a one-frame source")
  {
    for (auto type : {ezMiniAudioEffectType::Reverb, ezMiniAudioEffectType::Chorus, ezMiniAudioEffectType::Flanger})
    {
      effect.m_Effect.m_Type = type;
      Render(ezMakeArrayPtr(&effect, 1), impulse, output);
      EZ_TEST_BOOL(Energy(output, 1, 48000) > 0.001f);
      EZ_TEST_BOOL(Energy(output, 36000, 48000) < Energy(output, 1, 12000));
      for (float value : output)
        EZ_TEST_BOOL(ezMath::IsFinite(value));
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Filter attenuation and parametric EQ gain")
  {
    ezDynamicArray<float> sine;
    sine.SetCount(48000 * 2);
    for (ezUInt32 i = 0; i < 48000; ++i)
      sine[i * 2] = sine[i * 2 + 1] = 0.1f * std::sin(2.0f * ezMath::Pi<float>() * 1000.0f * i / s_uiRate);
    const float reference = Energy(sine, 24000, 48000);
    effect.m_Effect.m_fMix = 1;
    for (auto type : {ezMiniAudioEffectType::HighPass, ezMiniAudioEffectType::LowPass, ezMiniAudioEffectType::ParametricEqualizer})
    {
      effect.m_Effect.m_Type = type;
      effect.m_Effect.m_fFrequency = type == ezMiniAudioEffectType::HighPass ? 10000.0f : 100.0f;
      if (type == ezMiniAudioEffectType::ParametricEqualizer)
      {
        effect.m_Effect.m_fFrequency = 1000;
        effect.m_Effect.m_fGain = 6;
      }
      Render(ezMakeArrayPtr(&effect, 1), sine, output);
      const float ratio = Energy(output, 24000, 48000) / reference;
      if (type == ezMiniAudioEffectType::ParametricEqualizer)
        EZ_TEST_FLOAT(ratio, std::pow(10.0f, 0.6f), 0.01f);
      else
        EZ_TEST_BOOL(ratio < 0.001f);
    }
  }
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, VolumesAndSerialization)
{
  ezStartup::StartupCoreSystems();
  {
    ezWorldDesc desc("MiniAudioVolumes");
    ezWorld world(desc);
    EZ_LOCK(world.GetWriteMarker());
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    objectDesc.m_LocalPosition = ezVec3(10, 20, 30);
    objectDesc.m_LocalRotation = ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisZ(), ezAngle::MakeFromDegree(45));
    objectDesc.m_LocalScaling = ezVec3(2, 3, -4);
    ezGameObject* pObject;
    world.CreateObject(objectDesc, pObject);
    ezMiniAudioEffectBoxComponent* pBox;
    ezMiniAudioEffectSphereComponent* pSphere;
    ezMiniAudioSoundComponent* pSound;
    ezMiniAudioEffectBoxComponent::CreateComponent(pObject, pBox);
    ezMiniAudioEffectSphereComponent::CreateComponent(pObject, pSphere);
    ezMiniAudioSoundComponent::CreateComponent(pObject, pSound);
    pObject->UpdateGlobalTransform();
    pBox->SetExtents(ezVec3(4));
    pSphere->SetRadius(2);
    auto point = [&](float x)
    { return pObject->GetGlobalTransform().TransformPosition(ezVec3(x, 0, 0)); };
    for (const auto* pVolume : {static_cast<ezMiniAudioEffectVolumeComponent*>(pBox), static_cast<ezMiniAudioEffectVolumeComponent*>(pSphere)})
    {
      EZ_TEST_FLOAT(pVolume->GetWeight(point(0)), 1, 0.00001f);
      EZ_TEST_FLOAT(pVolume->GetWeight(point(1.5f)), 0.5f, 0.00001f);
      EZ_TEST_FLOAT(pVolume->GetWeight(point(2)), 0, 0.00001f);
      EZ_TEST_FLOAT(pVolume->GetWeight(point(3)), 0, 0.00001f);
    }
    ezMiniAudioEffect effect;
    effect.m_Type = ezMiniAudioEffectType::Flanger;
    effect.m_fRate = 2.0f;
    pBox->m_Effects.PushBack(effect);
    pSphere->m_Effects.PushBack(effect);
    pSound->m_Effects.PushBack(effect);
    pSphere->m_ExcludeGroups.PushBack("NoZoneEffects");
    pBox->m_IncludeGroups.PushBack("NoZoneEffects");
    pSound->m_sGroup = "NoZoneEffects";
    pSphere->m_fFalloff = 0.75f;
    pBox->m_iPriority = 7;
    pSphere->m_bInterpolateByTime = true;
    pSphere->m_InterpolationDuration = ezTime::MakeFromSeconds(2.5);
    ezMiniAudioListenerComponent* pListener;
    ezMiniAudioListenerComponent::CreateComponent(pObject, pListener);
    ezMiniAudioGroupEffect listenerEffect;
    listenerEffect.m_sGroup = "Music";
    listenerEffect.m_Type = ezMiniAudioEffectType::Compressor;
    listenerEffect.m_fRatio = 8;
    pListener->m_Effects.PushBack(listenerEffect);
    pListener->m_bDucker = true;
    ezMiniAudioDucker ducker;
    ducker.m_SourceGroups.PushBack("Voice");
    ducker.m_SourceGroups.PushBack("Weapons");
    ducker.m_TargetGroups.PushBack("Music");
    ducker.m_fRelease = 900;
    pListener->m_Duckers.PushBack(ducker);

    ezDefaultMemoryStreamStorage storage;
    ezMemoryStreamWriter streamWriter(&storage);
    ezWorldWriter writer;
    writer.WriteWorld(streamWriter, world);
    ezMemoryStreamReader streamReader(&storage);
    ezWorldReader reader;
    EZ_TEST_BOOL(reader.ReadWorldDescription(streamReader).Succeeded());
    ezWorld restored(desc);
    EZ_LOCK(restored.GetWriteMarker());
    reader.InstantiateWorld(restored);
    auto box = restored.GetComponentManager<ezMiniAudioEffectBoxComponentManager>()->GetComponents();
    auto sphere = restored.GetComponentManager<ezMiniAudioEffectSphereComponentManager>()->GetComponents();
    auto sound = restored.GetComponentManager<ezMiniAudioSoundComponentManager>()->GetComponents();
    EZ_TEST_INT(box->m_iPriority, 7);
    EZ_TEST_BOOL(!box->m_bInterpolateByTime);
    EZ_TEST_BOOL(sphere->m_bInterpolateByTime);
    EZ_TEST_FLOAT(sphere->m_InterpolationDuration.GetSeconds(), 2.5, 0);
    EZ_TEST_FLOAT(sphere->m_fFalloff, 0.75f, 0);
    EZ_TEST_BOOL(sphere->m_ExcludeGroups.Contains("NoZoneEffects"));
    EZ_TEST_BOOL(box->m_IncludeGroups.Contains("NoZoneEffects"));
    EZ_TEST_BOOL(sound->m_sGroup == "NoZoneEffects");
    EZ_TEST_BOOL(box->m_Effects[0] == effect);
    EZ_TEST_BOOL(sphere->m_Effects[0] == effect);
    EZ_TEST_BOOL(sound->m_Effects[0] == effect);
    auto listener = restored.GetComponentManager<ezMiniAudioListenerComponentManager>()->GetComponents();
    EZ_TEST_BOOL(listener->m_bDucker);
    EZ_TEST_INT(listener->m_Duckers.GetCount(), 1);
    EZ_TEST_BOOL(listener->m_Duckers[0].m_SourceGroups == ducker.m_SourceGroups);
    EZ_TEST_BOOL(listener->m_Duckers[0].m_TargetGroups == ducker.m_TargetGroups);
    EZ_TEST_FLOAT(listener->m_Duckers[0].m_fRelease, 900, 0);
    EZ_TEST_INT(listener->m_Effects.GetCount(), 1);
    EZ_TEST_BOOL(listener->m_Effects[0] == listenerEffect);
    EZ_TEST_STRING(listener->m_Effects[0].m_sGroup, "Music");
  }
  ezStartup::ShutdownCoreSystems();
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, RuntimeRouting)
{
  AudioTestApplication application;
  ezStartup::StartupCoreSystems();
  ezStartup::StartupHighLevelSystems();
  auto* pAudio = ezMiniAudioSingleton::GetSingleton();
  if (!EZ_TEST_BOOL(pAudio->IsInitialized()))
  {
    ezStartup::ShutdownHighLevelSystems();
    ezStartup::ShutdownCoreSystems();
    return;
  }
  auto* pEngine = pAudio->GetEngine();
  EZ_TEST_INT(ma_engine_stop(pEngine), MA_SUCCESS);
  {
    ezWorldDesc desc("MiniAudioRuntime");
    ezWorld world(desc);
    EZ_LOCK(world.GetWriteMarker());
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    ezGameObject *pZoneObject, *pListenerObject, *pSourceObject;
    world.CreateObject(objectDesc, pZoneObject);
    world.CreateObject(objectDesc, pListenerObject);
    world.CreateObject(objectDesc, pSourceObject);
    ezMiniAudioEffectSphereComponent* pZone;
    ezMiniAudioListenerComponent* pListener;
    ezMiniAudioSoundComponent* pSource;
    ezMiniAudioEffectSphereComponent::CreateComponent(pZoneObject, pZone);
    ezMiniAudioListenerComponent::CreateComponent(pListenerObject, pListener);
    ezMiniAudioSoundComponent::CreateComponent(pSourceObject, pSource);
    ezMiniAudioEffect delay;
    delay.m_Type = ezMiniAudioEffectType::Delay;
    delay.m_fDelay = 10;
    delay.m_fFeedback = 0;
    pZone->m_Effects.PushBack(delay);
    pZone->SetRadius(2);
    pZone->m_ExcludeGroups.PushBack("NoZoneEffects");
    world.Update();
    pAudio->UpdateEffects();
    EZ_TEST_VEC3(pAudio->GetListenerPosition(), ezVec3::MakeZero(), 0.00001f);

    const ezUInt8 header[] = {'R', 'I', 'F', 'F', 36, 32, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
      1, 0, 1, 0, 128, 187, 0, 0, 0, 119, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 0, 32, 0, 0};
    ezDataBuffer data;
    data.SetCount(44 + 8192);
    ezMemoryUtils::ZeroFill(data.GetData(), data.GetCount());
    ezMemoryUtils::Copy(data.GetData(), header, 44);
    data[44 + 65] = 64;
    const ezUInt32 channels = ma_engine_get_channels(pEngine);
    const ezUInt32 delayFrames = ma_engine_get_sample_rate(pEngine) / 100;
    ezDynamicArray<float> samples;
    samples.SetCount(delayFrames * 3 * channels);
    for (ezUInt32 scenario = 0; scenario < 4; ++scenario)
    {
      if (scenario == 1)
        pSource->m_sGroup = "NoZoneEffects";
      if (scenario == 2)
      {
        pSource->m_sGroup.Clear();
        pListenerObject->SetLocalPosition(ezVec3(10, 0, 0));
        world.Update();
      }
      if (scenario == 3)
        pSource->m_Effects.PushBack(delay); // Personal effects still apply outside zones.
      auto* pInstance = pAudio->AllocateSoundInstance(data, &world, pSource->GetHandle(), nullptr);
      if (!EZ_TEST_BOOL(pInstance != nullptr))
        continue;
      ma_sound_set_spatialization_enabled(&pInstance->m_Sound, false);
      ma_sound_start(&pInstance->m_Sound);
      ma_engine_read_pcm_frames(pEngine, samples.GetData(), delayFrames * 3, nullptr);
      float dry = 0, echo = 0;
      for (ezUInt32 i = 0; i < delayFrames; ++i)
      {
        dry += ezMath::Abs(samples[i * channels]);
        echo += ezMath::Abs(samples[(delayFrames + i) * channels]);
      }
      EZ_TEST_BOOL(dry > 0.1f);
      EZ_TEST_FLOAT(echo, scenario == 0 || scenario == 3 ? 0.25f : 0.0f, 0.01f);
      pAudio->FreeSoundInstance(pInstance);
    }
    ezMiniAudioEffect speed;
    speed.m_Type = ezMiniAudioEffectType::Speed;
    speed.m_fSpeed = 0.1f;
    pSource->m_Effects.Clear();
    pSource->m_Effects.PushBack(speed);
    auto* pInstance = pAudio->AllocateSoundInstance(data, &world, pSource->GetHandle(), nullptr);
    EZ_TEST_FLOAT(ma_sound_get_pitch(&pInstance->m_Sound), 0.1f, 0.00001f);
    ma_sound_start(&pInstance->m_Sound);
    samples.SetCount(4096 * channels);
    ma_engine_read_pcm_frames(pEngine, samples.GetData(), 4096, nullptr);
    ma_uint64 cursor = 0;
    ma_sound_get_cursor_in_pcm_frames(&pInstance->m_Sound, &cursor);
    EZ_TEST_BOOL(cursor < 1500);
    EZ_TEST_BOOL(!ma_sound_at_end(&pInstance->m_Sound));
    pAudio->FreeSoundInstance(pInstance);

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Live listener falloff and group edits keep the existing graph")
    {
      // Three spaced impulses let us change groups during uninterrupted playback.
      // Seeking is deliberately avoided: MiniAudio retains resampler history on seek.
      ezDataBuffer continuousData;
      continuousData.SetCount(44 + 48000 * 2);
      ezMemoryUtils::ZeroFill(continuousData.GetData(), continuousData.GetCount());
      ezMemoryUtils::Copy(continuousData.GetData(), header, 44);
      ezRawMemoryStreamWriter riffSize(continuousData.GetData() + 4, 4);
      riffSize << ezUInt32(36 + 48000 * 2);
      ezRawMemoryStreamWriter dataSize(continuousData.GetData() + 40, 4);
      dataSize << ezUInt32(48000 * 2);
      for (ezUInt32 frame : {4800u, 19200u, 33600u})
        continuousData[44 + frame * 2 + 1] = 64;
      pSource->m_Effects.Clear();
      pListenerObject->SetLocalPosition(ezVec3::MakeZero());
      world.Update();
      pInstance = pAudio->AllocateSoundInstance(continuousData, &world, pSource->GetHandle(), nullptr);
      ma_sound_set_spatialization_enabled(&pInstance->m_Sound, false);
      auto* pOriginalNode = pInstance->m_pEffectNode;
      ma_sound_start(&pInstance->m_Sound);
      const ezUInt32 rate = ma_engine_get_sample_rate(pEngine);
      const ezUInt32 framesPerStep = rate * 3 / 10;
      samples.SetCount(framesPerStep * channels);
      for (ezUInt32 step = 0; step < 3; ++step)
      {
        if (step == 0)
        {
          pListenerObject->SetLocalPosition(ezVec3(1.5f, 0, 0));
          world.Update();
        }
        else if (step == 1)
          pSource->m_sGroup = "NoZoneEffects";
        else
          pSource->m_sGroup.Clear();
        pAudio->UpdateEffects();
        EZ_TEST_BOOL(pInstance->m_pEffectNode == pOriginalNode);
        ma_engine_read_pcm_frames(pEngine, samples.GetData(), framesPerStep, nullptr);
        float echo = 0;
        for (ezUInt32 i = rate * 105 / 1000; i < rate * 115 / 1000; ++i)
          echo += ezMath::Abs(samples[i * channels]);
        EZ_TEST_FLOAT(echo, step == 1 ? 0.0f : 0.125f, 0.01f);
      }
      pAudio->FreeSoundInstance(pInstance);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Per-component include and exclude groups on the same prefab object")
    {
      pListenerObject->SetLocalPosition(ezVec3::MakeZero());
      pZone->m_IncludeGroups.PushBack("Weapons");
      pSource->m_sGroup = "Weapons";
      ezMiniAudioSoundComponent* pOtherSource;
      ezMiniAudioSoundComponent::CreateComponent(pSourceObject, pOtherSource);
      world.Update();
      for (ezUInt32 step = 0; step < 3; ++step)
      {
        if (step == 2)
          pZone->m_ExcludeGroups.PushBack("Weapons"); // Exclusion wins over inclusion.
        auto* pTestSource = step == 1 ? pOtherSource : pSource;
        pInstance = pAudio->AllocateSoundInstance(data, &world, pTestSource->GetHandle(), nullptr);
        ma_sound_set_spatialization_enabled(&pInstance->m_Sound, false);
        ma_sound_start(&pInstance->m_Sound);
        ma_engine_read_pcm_frames(pEngine, samples.GetData(), delayFrames * 3, nullptr);
        float echo = 0;
        for (ezUInt32 i = delayFrames; i < delayFrames * 2; ++i)
          echo += ezMath::Abs(samples[i * channels]);
        EZ_TEST_FLOAT(echo, step == 0 ? 0.25f : 0.0f, 0.01f);
        pAudio->FreeSoundInstance(pInstance);
      }
      pZone->m_IncludeGroups.Clear();
      pSource->m_sGroup.Clear();
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Time interpolation is shared by all voices and keeps progressing in silence")
    {
      world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(0.25));
      pZone->SetActiveFlag(false);
      pZone->SetActiveFlag(true);
      pZone->m_bInterpolateByTime = true;
      pZone->m_InterpolationDuration = ezTime::MakeFromSeconds(1);
      pZone->m_Effects.Clear();
      pZone->m_Effects.PushBack(speed);
      pSource->m_Effects.Clear();
      pListenerObject->SetLocalPosition(ezVec3(10, 0, 0));
      world.Update();
      pAudio->UpdateEffects();
      EZ_TEST_FLOAT(pZone->GetEffectWeight(pAudio->GetListenerPosition()), 0, 0.00001f);
      pListenerObject->SetLocalPosition(ezVec3(1.99f, 0, 0)); // Barely inside; Falloff is ignored.
      for (ezUInt32 i = 0; i < 4; ++i)
      {
        world.Update();
        pAudio->UpdateEffects(); // No playing voices are needed for the transition.
      }
      EZ_TEST_FLOAT(pZone->GetEffectWeight(pAudio->GetListenerPosition()), 0.9f, 0.00001f);
      for (ezUInt32 i = 0; i < 2; ++i)
      {
        pInstance = pAudio->AllocateSoundInstance(data, &world, pSource->GetHandle(), nullptr);
        EZ_TEST_FLOAT(ma_sound_get_pitch(&pInstance->m_Sound), 0.19f, 0.00001f);
        pAudio->UpdateEffects(); // Repeated calls in a frame must not advance the interpolation.
        EZ_TEST_FLOAT(ma_sound_get_pitch(&pInstance->m_Sound), 0.19f, 0.00001f);
        pAudio->FreeSoundInstance(pInstance);
      }
      pListenerObject->SetLocalPosition(ezVec3(10, 0, 0));
      world.Update();
      pAudio->UpdateEffects();
      const float fAfterExit = 0.9f * std::pow(10.0f, -0.25f);
      EZ_TEST_FLOAT(pZone->GetEffectWeight(pAudio->GetListenerPosition()), fAfterExit, 0.00001f);
      pListenerObject->SetLocalPosition(ezVec3::MakeZero());
      world.Update();
      pAudio->UpdateEffects();
      EZ_TEST_FLOAT(pZone->GetEffectWeight(pAudio->GetListenerPosition()), 1 - (1 - fAfterExit) * std::pow(10.0f, -0.25f), 0.00001f);
      pZone->m_InterpolationDuration = ezTime::MakeZero();
      world.Update();
      pAudio->UpdateEffects();
      EZ_TEST_FLOAT(pZone->GetEffectWeight(pAudio->GetListenerPosition()), 1, 0.00001f);
      pZone->m_bInterpolateByTime = false;
      pZone->m_Effects.Clear();
      pZone->m_Effects.PushBack(delay);
      world.GetClock().SetFixedTimeStep();
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Sound volumes gate dry audio and effect tails")
    {
      pZone->SetActiveFlag(false);
      ezMiniAudioSoundBoxComponent* box;
      ezMiniAudioSoundSphereComponent* sphere;
      ezMiniAudioSoundBoxComponent::CreateComponent(pSourceObject, box);
      ezMiniAudioSoundSphereComponent::CreateComponent(pSourceObject, sphere);
      box->SetExtents(ezVec3(4));
      sphere->SetRadius(2);
      for (auto* source : {static_cast<ezMiniAudioSoundVolumeComponent*>(box), static_cast<ezMiniAudioSoundVolumeComponent*>(sphere)})
      {
        source->m_vCaptureOffset = ezVec3(0.5f, 1, 0);
        source->m_Effects.PushBack(delay);
        for (ezUInt32 step = 0; step < 3; ++step)
        {
          pListenerObject->SetLocalPosition(ezVec3(step == 0 ? 0 : step == 1 ? 1.5f
                                                                             : 3.0f,
            0, 0));
          world.Update();
          pAudio->UpdateEffects();
          pInstance = pAudio->AllocateSoundInstance(data, &world, source->GetHandle(), nullptr);
          const auto position = ma_sound_get_position(&pInstance->m_Sound);
          EZ_TEST_VEC3(ezVec3(position.x, position.y, position.z), source->GetSourcePosition(), 0.00001f);
          ma_sound_set_spatialization_enabled(&pInstance->m_Sound, false);
          ma_sound_start(&pInstance->m_Sound);
          ma_engine_read_pcm_frames(pEngine, samples.GetData(), delayFrames * 3, nullptr);
          float energy = 0;
          for (ezUInt32 f = 0; f < delayFrames * 3; ++f)
            energy += ezMath::Abs(samples[f * channels]);
          EZ_TEST_FLOAT(energy, step == 0 ? 0.5f : step == 1 ? 0.25f
                                                             : 0.0f,
            0.01f);
          pAudio->FreeSoundInstance(pInstance);
        }
      }
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Natural completion preserves the tail, then releases the instance")
    {
      pZone->SetActiveFlag(false);
      delay.m_fDelay = 200;
      pSource->m_Effects.PushBack(delay);
      pInstance = pAudio->AllocateSoundInstance(data, &world, pSource->GetHandle(), nullptr);
      ma_sound_set_spatialization_enabled(&pInstance->m_Sound, false);
      ma_sound_start(&pInstance->m_Sound);
      const ezUInt32 rate = ma_engine_get_sample_rate(pEngine);
      samples.SetCount(rate * channels);
      ma_engine_read_pcm_frames(pEngine, samples.GetData(), rate / 8, nullptr);
      EZ_TEST_BOOL(ma_sound_at_end(&pInstance->m_Sound));
      pAudio->SetMasterChannelPaused(true); // Keep hardware stopped while testing the game-thread update.
      pAudio->UpdateSound();
      EZ_TEST_BOOL(pInstance->m_bInUse && pInstance->m_bFinishedNotified);
      ma_engine_read_pcm_frames(pEngine, samples.GetData(), rate / 4, nullptr);
      float tail = 0;
      for (ezUInt32 i = 0; i < rate / 4; ++i)
        tail += ezMath::Abs(samples[i * channels]);
      EZ_TEST_FLOAT(tail, 0.25f, 0.01f);
      pAudio->UpdateSound();
      EZ_TEST_BOOL(!pInstance->m_bInUse);
      pAudio->SetMasterChannelPaused(false);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Editor listener override")
    {
      pAudio->SetListenerOverrideMode(true);
      pAudio->SetListener(-1, ezVec3(3, 4, 5), ezVec3::MakeAxisX(), ezVec3::MakeAxisZ(), ezVec3::MakeZero());
      world.Update();
      EZ_TEST_VEC3(pAudio->GetListenerPosition(), ezVec3(3, 4, 5), 0.00001f);
      pAudio->SetListenerOverrideMode(false);
    }
  }
  ezStartup::ShutdownHighLevelSystems();
  ezStartup::ShutdownCoreSystems();
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, PrefabPlayback)
{
  AudioTestApplication application;
  ezStartup::StartupCoreSystems();
  ezStartup::StartupHighLevelSystems();
  ezFileSystem::AddDataDirectory(MINI_AUDIO_TEST_DATA_DIR, "MiniAudioFixtures", "audiofixtures").IgnoreResult();
  {
    auto loader = EZ_DEFAULT_NEW(ezResourceLoaderFromMemory);
    ezMemoryStreamWriter writer(&loader->m_CustomData);
    writer << ezString("Generated Ogg tone");
    ezAssetFileHeader assetHeader;
    assetHeader.SetFileHashAndVersion(1, 2);
    assetHeader.Write(writer).IgnoreResult();
    writer << ezUInt8(2) << false;
    writer << 1.0f << 1.0f << 1.0f << 1.0f;           // Volume and pitch ranges.
    writer << false << 1.0f << 10.0f << 1.0f << 0.0f; // Spatialization, attenuation, Doppler.
    ezFileReader file;
    EZ_TEST_BOOL(file.Open(":audiofixtures/Tone.ogg").Succeeded());
    ezDataBuffer bytes;
    bytes.SetCount(static_cast<ezUInt32>(file.GetFileSize()));
    file.ReadBytes(bytes.GetData(), bytes.GetCount());
    writer << ezUInt32(1) << bytes.GetCount();
    writer.WriteBytes(bytes.GetData(), bytes.GetCount()).IgnoreResult();
    writer << ezString();
    auto sound = ezResourceManager::GetExistingResourceOrCreateAsync<ezMiniAudioSoundResource>("MiniAudio.PrefabTone", std::move(loader));
    ezResourceLock<ezMiniAudioSoundResource> resource(sound, ezResourceAcquireMode::BlockTillLoaded);
    EZ_TEST_BOOL(resource.GetAcquireResult() == ezResourceAcquireResult::Final);
    EZ_TEST_BOOL(resource->GetAudioData().GetCount() > bytes.GetCount());
    auto* audio = ezMiniAudioSingleton::GetSingleton();
    ma_engine_stop(audio->GetEngine());
    for (ezUInt32 scenario = 0; scenario < 6; ++scenario)
    {
      const float speed = scenario % 2 == 0 ? 1.0f : 0.1f;
      ezDefaultMemoryStreamStorage storage;
      {
        ezWorldDesc sourceDesc("Prefab source");
        ezWorld source(sourceDesc);
        EZ_LOCK(source.GetWriteMarker());
        source.SetWorldSimulationEnabled(false);
        ezGameObject* pObject;
        source.CreateObject(ezGameObjectDesc(), pObject);
        ezMiniAudioSoundComponent* pSound;
        if (scenario < 2)
          ezMiniAudioSoundComponent::CreateComponent(pObject, pSound);
        else if (scenario < 4)
        {
          ezMiniAudioSoundBoxComponent* box;
          ezMiniAudioSoundBoxComponent::CreateComponent(pObject, box);
          box->m_vCaptureOffset = ezVec3(2, 0, 0);
          pSound = box;
        }
        else
        {
          ezMiniAudioSoundSphereComponent* sphere;
          ezMiniAudioSoundSphereComponent::CreateComponent(pObject, sphere);
          sphere->m_vCaptureOffset = ezVec3(2, 0, 0);
          pSound = sphere;
        }
        auto* property = static_cast<const ezAbstractMemberProperty*>(pSound->GetDynamicRTTI()->FindPropertyByName("Sound"));
        ezReflectionUtils::SetMemberPropertyValue(property, pSound, resource->GetResourceID());
        pSound->m_sGroup = "NoZoneEffects";
        ezMiniAudioEffect effect;
        effect.m_Type = ezMiniAudioEffectType::Speed;
        effect.m_fSpeed = speed;
        pSound->m_Effects.PushBack(effect);
        ezMemoryStreamWriter streamWriter(&storage);
        ezWorldWriter worldWriter;
        worldWriter.WriteWorld(streamWriter, source);
      }
      ezWorldDesc desc("Instantiated sound prefab");
      ezWorld world(desc);
      EZ_LOCK(world.GetWriteMarker());
      ezMemoryStreamReader streamReader(&storage);
      ezWorldReader reader;
      EZ_TEST_BOOL(reader.ReadWorldDescription(streamReader).Succeeded());
      reader.InstantiatePrefab(world, ezTransform::MakeIdentity(), {});
      world.Update(); // Autoplay from OnSimulationStarted, as in a surface interaction.
      ezDynamicArray<ezMiniAudioSoundInstance*> instances;
      audio->GetSoundInstances(instances);
      if (EZ_TEST_INT(instances.GetCount(), 1))
      {
        auto* instance = instances[0];
        EZ_TEST_FLOAT(ma_sound_get_pitch(&instance->m_Sound), speed, 0.00001f);
        EZ_TEST_BOOL(instance->m_sGroup == "NoZoneEffects");
        const ezUInt32 rate = ma_engine_get_sample_rate(audio->GetEngine());
        const ezUInt32 channels = ma_engine_get_channels(audio->GetEngine());
        ezDynamicArray<float> samples;
        samples.SetCount(rate * channels);
        ma_engine_read_pcm_frames(audio->GetEngine(), samples.GetData(), rate, nullptr);
        ezUInt32 crossings = 0;
        for (ezUInt32 i = rate / 4; i < rate * 3 / 4; ++i)
        {
          if (samples[i * channels] <= 0 && samples[(i + 1) * channels] > 0)
            ++crossings;
        }
        // The one-second 1 kHz Ogg tone becomes a ten-second 100 Hz tone at Speed 0.1.
        EZ_TEST_FLOAT(static_cast<float>(crossings), 500.0f * speed, 2.0f);
        if (speed < 1)
        {
          EZ_TEST_BOOL(!ma_sound_at_end(&instance->m_Sound));
          ma_uint64 cursor = 0;
          ma_sound_get_cursor_in_pcm_frames(&instance->m_Sound, &cursor);
          EZ_TEST_FLOAT(static_cast<float>(cursor), 4800.0f, 1024.0f);
        }
      }
    }
  }
  ezFileSystem::RemoveDataDirectoryGroup("MiniAudioFixtures");
  ezStartup::ShutdownHighLevelSystems();
  ezStartup::ShutdownCoreSystems();
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, ExtendedEffects)
{
  ezMiniAudioEffectInstance effect;
  ezDynamicArray<float> input, output;
  input.SetCount(48000 * 2, 0.8f);
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Volume;
  effect.m_Effect.m_fVolume = 0.25f;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_FLOAT(output[1000], 0.2f, 0.00001f);
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Panner;
  effect.m_Effect.m_fPan = 1;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_FLOAT(output[1000], 0, 0.00001f);
  EZ_TEST_FLOAT(output[1001], 0.8f, 0.00001f);
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Limiter;
  effect.m_Effect.m_fThreshold = -12;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_FLOAT(output[1000], std::pow(10.0f, -12.0f / 20), 0.0001f);
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Compressor;
  effect.m_Effect.m_fRatio = 4;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  const float expected = 0.8f * std::pow(10.0f, (-12.0f - 20.0f * std::log10(0.8f)) * 0.75f / 20.0f);
  EZ_TEST_FLOAT(output[80000], expected, 0.0001f);
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Distortion;
  effect.m_Effect.m_fMix = 1;
  effect.m_Effect.m_fDrive = 5;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_FLOAT(output[1000], std::tanh(4.0f) / std::tanh(5.0f), 0.0001f);
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Bitcrusher;
  effect.m_Effect.m_uiBits = 2;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_FLOAT(output[1000], 1.0f, 0.0001f);
  effect.m_Effect.m_Type = ezMiniAudioEffectType::Muffling;
  effect.m_Effect.m_fVolume = 0.25f;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_FLOAT(output[80000], 0.2f, 0.0001f);
  for (ezUInt32 f = 0; f < 48000; ++f)
    input[f * 2] = input[f * 2 + 1] = 0.5f * std::sin(2.0f * ezMath::Pi<float>() * 10000.0f * f / 48000);
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_BOOL(Energy(output, 24000, 48000) < Energy(input, 24000, 48000) * 0.001f);
  for (auto& value : input)
    value = 0.8f;
  effect.m_Effect.m_Type = ezMiniAudioEffectType::BandPass;
  Render(ezMakeArrayPtr(&effect, 1), input, output);
  EZ_TEST_FLOAT(output[80000], 0, 0.00001f); // DC is rejected.
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, Ducker)
{
  ma_engine engine;
  auto config = ma_engine_config_init();
  config.noDevice = MA_TRUE;
  config.channels = 2;
  config.sampleRate = 48000;
  EZ_TEST_INT(ma_engine_init(&config, &engine), MA_SUCCESS);
  {
    ezMiniAudioMixerNode mixer;
    EZ_TEST_BOOL(mixer.Initialize(&engine).Succeeded());
    const ezString groups[] = {"Voice", "Music", "Ambience"};
    ezMiniAudioDucker rule;
    rule.m_SourceGroups.PushBack("Voice");
    rule.m_TargetGroups.PushBack("Music");
    rule.m_fAttack = 10;
    rule.m_fRelease = 20;
    mixer.Configure(groups, ezMakeArrayPtr(&rule, 1));
    float source[960], target[960], output[960];
    for (ezUInt32 f = 0; f < 480; ++f)
    {
      source[2 * f] = 0.5f;
      source[2 * f + 1] = 0;
      target[2 * f] = 0;
      target[2 * f + 1] = 0.5f;
    }
    const float* inputs[ezMiniAudioMixerNode::MaxGroups] = {};
    inputs[0] = source;
    inputs[1] = target;
    float* outputs[] = {output};
    auto process = [&]()
    {
      ma_uint32 frames = 480, outFrames = 480;
      mixer.m_Node.vtable->onProcess(&mixer.m_Node, inputs, &frames, outputs, &outFrames);
    };
    process();
    EZ_TEST_FLOAT(output[0], 0.5f, 0.00001f);
    EZ_TEST_FLOAT(output[479 * 2 + 1], 0.1f, 0.00001f);
    EZ_TEST_FLOAT(output[239 * 2 + 1], 0.3f, 0.001f);
    mixer.Configure(groups, ezMakeArrayPtr(&rule, 1)); // Updates preserve the envelope.
    process();
    EZ_TEST_FLOAT(output[1], 0.1f, 0.00001f);
    ezMemoryUtils::ZeroFill(source, 960);
    process();
    EZ_TEST_FLOAT(output[959], 0.3f, 0.001f);
    process();
    EZ_TEST_FLOAT(output[959], 0.5f, 0.001f);
    for (ezUInt32 f = 0; f < 480; ++f)
      source[f * 2] = 0.05f; // Below threshold.
    process();
    EZ_TEST_FLOAT(output[959], 0.5f, 0.00001f);
    // The same sidechain through MiniAudio's real multi-input graph.
    for (ezUInt32 f = 0; f < 480; ++f)
      source[f * 2] = 0.5f;
    ma_audio_buffer buffers[2];
    ma_sound sounds[2];
    for (ezUInt32 i = 0; i < 2; ++i)
    {
      auto bufferConfig = ma_audio_buffer_config_init(ma_format_f32, 2, 480, i == 0 ? source : target, nullptr);
      EZ_TEST_INT(ma_audio_buffer_init(&bufferConfig, &buffers[i]), MA_SUCCESS);
      EZ_TEST_INT(ma_sound_init_from_data_source(&engine, &buffers[i], MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &sounds[i]), MA_SUCCESS);
      ma_sound_set_looping(&sounds[i], true);
      EZ_TEST_INT(ma_node_attach_output_bus(&sounds[i], 0, &mixer.m_Node, i), MA_SUCCESS);
      ma_sound_start(&sounds[i]);
    }
    ma_engine_read_pcm_frames(&engine, output, 480, nullptr);
    ma_engine_read_pcm_frames(&engine, output, 480, nullptr);
    EZ_TEST_FLOAT(output[958], 0.5f, 0.00001f);
    EZ_TEST_FLOAT(output[959], 0.1f, 0.00001f);
    ma_sound_stop(&sounds[0]);
    for (ezUInt32 i = 0; i < 3; ++i)
      ma_engine_read_pcm_frames(&engine, output, 480, nullptr);
    EZ_TEST_FLOAT(output[959], 0.5f, 0.001f);
    for (ezUInt32 i = 0; i < 2; ++i)
    {
      ma_sound_uninit(&sounds[i]);
      ma_audio_buffer_uninit(&buffers[i]);
    }
  }
  ma_engine_uninit(&engine);
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, TimeStretch)
{
  // PCM WAV with a 440 Hz tone. Non-integer periods exercise waveform alignment.
  ezDefaultMemoryStreamStorage storage;
  ezMemoryStreamWriter writer(&storage);
  writer << ezUInt32(0x46464952) << ezUInt32(36 + 96000) << ezUInt32(0x45564157) << ezUInt32(0x20746d66);
  writer << ezUInt32(16) << ezUInt16(1) << ezUInt16(1) << ezUInt32(48000) << ezUInt32(96000) << ezUInt16(2) << ezUInt16(16);
  writer << ezUInt32(0x61746164) << ezUInt32(96000);
  for (ezUInt32 i = 0; i < 48000; ++i)
    writer << static_cast<ezInt16>(10000 * std::sin(2.0 * ezMath::Pi<double>() * 440 * i / 48000));
  ezDataBuffer wav;
  wav.SetCount(static_cast<ezUInt32>(storage.GetStorageSize64()));
  ezMemoryStreamReader reader(&storage);
  reader.ReadBytes(wav.GetData(), wav.GetCount());
  for (float tempo : {0.5f, 1.0f, 2.0f})
  {
    ma_decoder decoder;
    auto config = ma_decoder_config_init(ma_format_f32, 0, 0);
    EZ_TEST_INT(ma_decoder_init_memory(wav.GetData(), wav.GetCount(), &config, &decoder), MA_SUCCESS);
    {
      ezMiniAudioTimeStretch stretch;
      EZ_TEST_BOOL(stretch.Initialize(&decoder).Succeeded());
      stretch.SetTempo(tempo);
      ezDynamicArray<float> samples;
      samples.SetCount(100000);
      ma_uint64 count = 0;
      ma_data_source_read_pcm_frames(&stretch.m_Source, samples.GetData(), samples.GetCount(), &count);
      EZ_TEST_FLOAT(static_cast<float>(count), 48000.0f / tempo, 2.0f);
      ezUInt32 crossings = 0;
      for (ezUInt32 i = 4800; i < 19200; ++i)
        if (samples[i] <= 0 && samples[i + 1] > 0)
          ++crossings;
      EZ_TEST_FLOAT(static_cast<float>(crossings), 132, 3);
    }
    ma_decoder_uninit(&decoder);
  }
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, AssetAndListener)
{
  AudioTestApplication application;
  ezStartup::StartupCoreSystems();
  ezStartup::StartupHighLevelSystems();
  ezFileSystem::AddDataDirectory(MINI_AUDIO_TEST_DATA_DIR, "MiniAudioFixtures", "audiofixtures").IgnoreResult();
  {
    auto* audio = ezMiniAudioSingleton::GetSingleton();
    ma_engine_stop(audio->GetEngine());
    ezFileReader file;
    EZ_TEST_BOOL(file.Open(":audiofixtures/Tone.ogg").Succeeded());
    ezDataBuffer bytes;
    bytes.SetCount(static_cast<ezUInt32>(file.GetFileSize()));
    file.ReadBytes(bytes.GetData(), bytes.GetCount());
    auto loader = EZ_DEFAULT_NEW(ezResourceLoaderFromMemory);
    ezMemoryStreamWriter writer(&loader->m_CustomData);
    writer << ezString("Group and effect fixture");
    ezAssetFileHeader header;
    header.SetFileHashAndVersion(1, 3);
    header.Write(writer).IgnoreResult();
    writer << ezUInt8(3) << false << 1.0f << 1.0f << 1.0f << 1.0f;
    writer << false << 1.0f << 10.0f << 1.0f << 0.0f;
    writer << ezUInt32(3);
    for (ezUInt32 i = 0; i < 3; ++i)
    {
      writer << bytes.GetCount();
      writer.WriteBytes(bytes.GetData(), bytes.GetCount()).IgnoreResult();
    }
    ezMiniAudioEffect assetEffect;
    assetEffect.m_Type = ezMiniAudioEffectType::Volume;
    assetEffect.m_fVolume = 0.5f;
    writer << ezString("Music") << true;
    ezDynamicArray<ezMiniAudioEffect> assetEffects;
    assetEffects.PushBack(assetEffect);
    writer.WriteArray(assetEffects).IgnoreResult();
    auto handle = ezResourceManager::GetExistingResourceOrCreateAsync<ezMiniAudioSoundResource>("MiniAudio.GroupFixture", std::move(loader));
    ezResourceLock<ezMiniAudioSoundResource> resource(handle, ezResourceAcquireMode::BlockTillLoaded);
    EZ_TEST_BOOL(resource->GetGroup() == "Music");
    EZ_TEST_INT(resource->GetEffects().GetCount(), 1);
    ezRandom rng;
    rng.Initialize(1234);
    ezUInt32 last = ezInvalidIndex;
    for (ezUInt32 cycle = 0; cycle < 20; ++cycle)
    {
      ezUInt32 mask = 0;
      for (ezUInt32 i = 0; i < 3; ++i)
      {
        const auto selected = resource->SelectVariation(rng);
        EZ_TEST_BOOL(selected != last);
        EZ_TEST_BOOL((mask & (1 << selected)) == 0);
        mask |= 1 << selected;
        last = selected;
      }
      EZ_TEST_INT(mask, 7);
    }
    ezWorldDesc desc("Listener buses");
    ezWorld world(desc);
    EZ_LOCK(world.GetWriteMarker());
    ezGameObject* object;
    world.CreateObject(ezGameObjectDesc(), object);
    ezMiniAudioListenerComponent* listener;
    ezMiniAudioListenerComponent::CreateComponent(object, listener);
    ezMiniAudioSoundComponent* source;
    ezMiniAudioSoundComponent::CreateComponent(object, source);
    ezMiniAudioGroupEffect busEffect;
    busEffect.m_sGroup = "Music";
    busEffect.m_Type = ezMiniAudioEffectType::Volume;
    busEffect.m_fVolume = 0.25f;
    listener->m_Effects.PushBack(busEffect);
    world.Update();
    audio->UpdateEffects();
    float reference = 0;
    const ezUInt32 rate = ma_engine_get_sample_rate(audio->GetEngine());
    const ezUInt32 channels = ma_engine_get_channels(audio->GetEngine());
    ezDynamicArray<float> output;
    output.SetCount(rate / 2 * channels);
    for (ezUInt32 mode = 0; mode < 3; ++mode)
    {
      source->m_sGroup = mode == 1 ? "Weapons" : "";
      auto* instance = resource->InstantiateSound(&rng, &world, source->GetHandle());
      EZ_TEST_STRING(instance->m_sGroup, mode == 1 ? "Weapons" : "Music");
      EZ_TEST_INT(instance->m_AssetEffects.GetCount(), 1);
      if (mode == 2)
      {
        listener->SetActiveFlag(false);
        audio->UpdateEffects();
      }
      ma_sound_start(&instance->m_Sound);
      ma_engine_read_pcm_frames(audio->GetEngine(), output.GetData(), rate / 2, nullptr);
      float energy = 0;
      for (ezUInt32 f = rate / 4; f < rate / 2; ++f)
        energy += output[f * channels] * output[f * channels];
      if (mode == 0)
        reference = energy;
      else
        EZ_TEST_FLOAT(energy / reference, 16, 0.01f);
      audio->FreeSoundInstance(instance);
    }
    // Pitch changes frequency while preserving duration; tempo changes duration only.
    for (auto type : {ezMiniAudioEffectType::Pitch, ezMiniAudioEffectType::TimeStretch})
    {
      ezMiniAudioEffect effect;
      effect.m_Type = type;
      effect.m_fPitch = 12;
      effect.m_fSpeed = 0.5f;
      source->m_Effects.Clear();
      source->m_Effects.PushBack(effect);
      auto* instance = resource->InstantiateSound(&rng, &world, source->GetHandle());
      ma_sound_start(&instance->m_Sound);
      ma_engine_read_pcm_frames(audio->GetEngine(), output.GetData(), rate / 2, nullptr);
      ezUInt32 crossings = 0;
      for (ezUInt32 f = rate / 4; f < rate / 2 - 1; ++f)
        if (output[f * channels] <= 0 && output[(f + 1) * channels] > 0)
          ++crossings;
      EZ_TEST_FLOAT(static_cast<float>(crossings), type == ezMiniAudioEffectType::Pitch ? 500 : 250, 4);
      const ezUInt32 blocks = type == ezMiniAudioEffectType::Pitch ? 2 : 4;
      for (ezUInt32 block = 1; block < blocks; ++block)
      {
        EZ_TEST_BOOL(!ma_sound_at_end(&instance->m_Sound));
        ma_engine_read_pcm_frames(audio->GetEngine(), output.GetData(), rate / 2, nullptr);
      }
      ma_engine_read_pcm_frames(audio->GetEngine(), output.GetData(), rate / 2, nullptr);
      EZ_TEST_BOOL(ma_sound_at_end(&instance->m_Sound));
      audio->FreeSoundInstance(instance);
    }
  }
  {
    ezWorldDesc desc("Listener without sound components");
    ezWorld world(desc);
    EZ_LOCK(world.GetWriteMarker());
    ezGameObject* object;
    world.CreateObject(ezGameObjectDesc(), object);
    ezMiniAudioListenerComponent* listener;
    ezMiniAudioListenerComponent::CreateComponent(object, listener);
    listener->m_Effects.PushBack(ezMiniAudioGroupEffect());
    world.Update();
    ezMiniAudioSingleton::GetSingleton()->UpdateEffects();
  }
  ezMiniAudioSingleton::GetSingleton()->UpdateEffects();
  EZ_TEST_BOOL(ezMiniAudioSingleton::GetSingleton()->GetListenerEffects().IsEmpty());
  ezFileSystem::RemoveDataDirectoryGroup("MiniAudioFixtures");
  ezStartup::ShutdownHighLevelSystems();
  ezStartup::ShutdownCoreSystems();
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, SoundVolumesAndOcclusion)
{
  AudioTestApplication application;
  ezStartup::StartupCoreSystems();
  ezStartup::StartupHighLevelSystems();
  auto* audio = ezMiniAudioSingleton::GetSingleton();
  ma_engine_stop(audio->GetEngine());
  {
    ezWorldDesc desc("Sound source volumes");
    ezWorld world(desc);
    EZ_LOCK(world.GetWriteMarker());
    world.GetClock().SetFixedTimeStep(ezTime::MakeFromSeconds(0.25));
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    objectDesc.m_LocalPosition = ezVec3(10, 20, 30);
    objectDesc.m_LocalRotation = ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisZ(), ezAngle::MakeFromDegree(45));
    objectDesc.m_LocalScaling = ezVec3(2, 3, -4);
    ezGameObject* object;
    world.CreateObject(objectDesc, object);
    ezMiniAudioSoundBoxComponent* box;
    ezMiniAudioSoundSphereComponent* sphere;
    ezMiniAudioSoundBoxComponent::CreateComponent(object, box);
    ezMiniAudioSoundSphereComponent::CreateComponent(object, sphere);
    box->SetExtents(ezVec3(4));
    sphere->SetRadius(2);
    world.Update();
    const auto transform = object->GetGlobalTransform();
    const auto inside = transform.TransformPosition(ezVec3::MakeZero());
    const auto edge = transform.TransformPosition(ezVec3(1.5f, 0, 0));
    const auto outside = transform.TransformPosition(ezVec3(3, 0, 0));
    for (auto* source : {static_cast<ezMiniAudioSoundVolumeComponent*>(box), static_cast<ezMiniAudioSoundVolumeComponent*>(sphere)})
    {
      source->m_vCaptureOffset = ezVec3(1, 2, 3);
      EZ_TEST_VEC3(source->GetSourcePosition(), transform.TransformPosition(source->m_vCaptureOffset), 0.00001f);
      EZ_TEST_FLOAT(source->GetVolumeWeight(inside), 1, 0.00001f);
      EZ_TEST_FLOAT(source->GetVolumeWeight(edge), 0.5f, 0.00001f);
      EZ_TEST_FLOAT(source->GetVolumeWeight(outside), 0, 0.00001f);
      source->m_bInterpolateByTime = true;
      for (ezUInt32 i = 0; i < 4; ++i)
      {
        world.Update();
        source->UpdateTemporalWeight(edge);
      }
      EZ_TEST_FLOAT(source->GetVolumeWeight(edge), 0.9f, 0.00001f);
      for (ezUInt32 i = 0; i < 4; ++i)
      {
        world.Update();
        source->UpdateTemporalWeight(outside);
      }
      EZ_TEST_FLOAT(source->GetVolumeWeight(outside), 0.09f, 0.00001f);
      source->m_bInterpolateByTime = false;
      source->m_bUseOcclusion = true;
      source->m_uiOcclusionCollisionLayer = 7;
      source->SetNoGlobalPitch(true);
      EZ_TEST_FLOAT(source->GetOcclusion(inside), 0, 0); // No physics module: unobstructed.
    }
    auto* physics = world.GetOrCreateModule<MiniAudioTestPhysics>();
    EZ_TEST_BOOL(world.GetModuleReadOnly<ezPhysicsWorldModuleInterface>() == physics);
    physics->m_hSource = object->GetHandle();
    physics->m_uiBlockedRays = 6;
    box->m_fOcclusionThreshold = 0.5f;
    const float partialOcclusion = box->GetOcclusion(inside);
    EZ_TEST_BOOL(partialOcclusion > 0 && partialOcclusion < 1);
    const ezUInt32 queryCount = physics->m_uiQueries;
    box->GetOcclusion(inside);
    EZ_TEST_INT(physics->m_uiQueries, queryCount); // Reused by voices on the same component.
    world.Update();
    physics->m_uiBlockedRays = 9;
    for (ezUInt32 i = 0; i < 8; ++i)
    {
      world.Update();
      box->GetOcclusion(inside);
    }
    EZ_TEST_FLOAT(box->GetOcclusion(inside), 1, 0.00001f);
    // Render the component's automatic muffling through the real audio graph.
    const ezUInt8 wavHeader[] = {'R', 'I', 'F', 'F', 36, 32, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
      1, 0, 1, 0, 128, 187, 0, 0, 0, 119, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 0, 32, 0, 0};
    ezDataBuffer wav;
    wav.SetCount(44 + 8192);
    ezMemoryUtils::Copy(wav.GetData(), wavHeader, 44);
    for (ezUInt32 f = 0; f < 4096; ++f)
    {
      wav[44 + f * 2] = 0;
      wav[45 + f * 2] = 64;
    }
    audio->SetListener(0, inside, ezVec3::MakeAxisX(), ezVec3::MakeAxisZ(), ezVec3::MakeZero());
    auto* instance = audio->AllocateSoundInstance(wav, &world, box->GetHandle(), nullptr);
    EZ_TEST_FLOAT(instance->m_fOcclusion, 1, 0.00001f);
    EZ_TEST_FLOAT(instance->m_fVolumeWeight, 1, 0.00001f);
    ma_sound_set_spatialization_enabled(&instance->m_Sound, false);
    ma_sound_start(&instance->m_Sound);
    const ezUInt32 channels = ma_engine_get_channels(audio->GetEngine());
    ezDynamicArray<float> samples;
    samples.SetCount(2048 * channels);
    ma_engine_read_pcm_frames(audio->GetEngine(), samples.GetData(), 2048, nullptr);
    EZ_TEST_FLOAT(samples[1500 * channels], 0.125f, 0.0001f);
    audio->FreeSoundInstance(instance);
    box->m_fOcclusionThreshold = 1;
    EZ_TEST_FLOAT(box->GetOcclusion(inside), 0, 0);
    box->m_bUseOcclusion = false;
    EZ_TEST_FLOAT(box->GetOcclusion(inside), 0, 0);
    box->m_bUseOcclusion = true;
    box->m_fOcclusionThreshold = 0.5f;
    box->m_fOcclusionRadius = 2.0f;
    box->m_fOcclusionRange = 30.0f;

    ezDefaultMemoryStreamStorage storage;
    ezMemoryStreamWriter streamWriter(&storage);
    ezWorldWriter writer;
    writer.WriteWorld(streamWriter, world);
    ezMemoryStreamReader streamReader(&storage);
    ezWorldReader reader;
    EZ_TEST_BOOL(reader.ReadWorldDescription(streamReader).Succeeded());
    ezWorld restored(desc);
    EZ_LOCK(restored.GetWriteMarker());
    reader.InstantiateWorld(restored);
    auto restoredBox = restored.GetComponentManager<ezMiniAudioSoundBoxComponentManager>()->GetComponents();
    auto restoredSphere = restored.GetComponentManager<ezMiniAudioSoundSphereComponentManager>()->GetComponents();
    EZ_TEST_VEC3(restoredBox->GetExtents(), ezVec3(4), 0);
    EZ_TEST_FLOAT(restoredSphere->GetRadius(), 2, 0);
    EZ_TEST_VEC3(restoredBox->m_vCaptureOffset, box->m_vCaptureOffset, 0);
    EZ_TEST_BOOL(restoredBox->GetNoGlobalPitch());
    EZ_TEST_BOOL(restoredBox->m_bUseOcclusion);
    EZ_TEST_INT(restoredBox->m_uiOcclusionCollisionLayer, 7);
    EZ_TEST_FLOAT(restoredBox->m_fOcclusionThreshold, 0.5f, 0);
    EZ_TEST_FLOAT(restoredBox->m_fOcclusionRadius, 2, 0);
    EZ_TEST_FLOAT(restoredBox->m_fOcclusionRange, 30, 0);
  }
  ezStartup::ShutdownHighLevelSystems();
  ezStartup::ShutdownCoreSystems();
}

EZ_CREATE_SIMPLE_TEST(MiniAudio, OcclusionApertureAndRange)
{
  AudioTestApplication application;
  ezStartup::StartupCoreSystems();
  ezStartup::StartupHighLevelSystems();
  auto* audio = ezMiniAudioSingleton::GetSingleton();
  ma_engine_stop(audio->GetEngine());
  {
    ezWorldDesc desc("Occlusion aperture regression");
    ezWorld world(desc);
    EZ_LOCK(world.GetWriteMarker());
    world.GetClock().SetFixedTimeStep(ezTime::MakeFromMilliseconds(100));
    ezGameObject* object;
    ezGameObjectDesc objectDesc;
    objectDesc.m_bDynamic = true;
    world.CreateObject(objectDesc, object);
    ezMiniAudioSoundComponent* source;
    ezMiniAudioSoundComponent::CreateComponent(object, source);
    source->m_bUseOcclusion = true;
    source->m_uiOcclusionCollisionLayer = 7;
    source->m_fOcclusionRange = 20;
    auto* physics = world.GetOrCreateModule<MiniAudioTestPhysics>();
    physics->m_hSource = object->GetHandle();
    physics->m_bGeometricObstacle = true;
    const ezVec3 listener = ezVec3::MakeZero();
    const ezVec3 emitter(10, 0, 0);
    world.Update();
    for (float x : {0.1f, 5.0f, 9.9f})
    {
      physics->m_fObstacleX = x;
      world.Update();
      EZ_TEST_FLOAT(source->GetOcclusion(listener, emitter), 0, 0); // A small obstacle does not cover most of the source sphere.
    }
    EZ_TEST_BOOL(physics->m_Starts.GetCount() > 27);
    for (const auto& end : physics->m_Ends)
      EZ_TEST_FLOAT((end - emitter).GetLength(), 1, 0.00001f);
    EZ_TEST_BOOL(!physics->m_Starts[0].IsEqual(physics->m_Starts[1], 0.01f));
    EZ_TEST_BOOL(physics->m_fLastRayLength < 10);
    auto settle = [&](const ezVec3& position)
    {
      float result = 0;
      for (ezUInt32 i = 0; i < 20; ++i)
      {
        world.Update();
        result = source->GetOcclusion(position, emitter);
      }
      return result;
    };
    physics->m_fObstacleX = 5;
    physics->m_fObstacleHalfSize = 10;
    world.Update();
    const float attack = source->GetOcclusion(listener, emitter);
    EZ_TEST_BOOL(attack > 0 && attack < 1);                            // A newly blocking grille/wall cannot jump to full attenuation.
    EZ_TEST_FLOAT(source->GetOcclusion(listener, emitter), attack, 0); // Multiple voices do not advance time.
    EZ_TEST_FLOAT(settle(listener), 1, 0.00001f);
    physics->m_fObstacleHalfSize = 0.1f;
    source->m_fOcclusionRadius = 0;                                    // Point sampling can be explicitly requested.
    EZ_TEST_FLOAT(source->GetOcclusion(listener, emitter), 1, 0);
    source->m_fOcclusionRadius = 1;
    EZ_TEST_FLOAT(source->GetOcclusion(listener, emitter), 0, 0);      // Editing radius invalidates the cache immediately.
    physics->m_fObstacleHalfSize = 10;
    world.Update();
    EZ_TEST_FLOAT(settle(listener), 1, 0.00001f);
    EZ_TEST_FLOAT(source->GetOcclusion(ezVec3(9, 0, 0), emitter), 0, 0); // Sphere surface is protected, even behind a wall.
    const ezUInt32 protectedQueries = physics->m_uiQueries;
    EZ_TEST_FLOAT(source->GetOcclusion(ezVec3(9.5f, 0, 0), emitter), 0, 0);
    EZ_TEST_FLOAT(source->GetOcclusion(emitter, emitter), 0, 0);
    EZ_TEST_INT(physics->m_uiQueries, protectedQueries);
    physics->m_fObstacleX = 8.75f;
    EZ_TEST_FLOAT(source->GetOcclusion(ezVec3(8.5f, 0, 0), emitter), 0.5f, 0.00001f); // Halfway through the near fade.
    physics->m_fObstacleX = 5;
    EZ_TEST_FLOAT(settle(ezVec3(6, 0, 0)), 0, 0.00001f);
    EZ_TEST_FLOAT(source->GetOcclusion(ezVec3(1000, 0, 0), emitter), 0, 0);
    const ezUInt32 queries = physics->m_uiQueries;
    EZ_TEST_FLOAT(source->GetOcclusion(ezVec3(1001, 0, 0), emitter), 0, 0);
    EZ_TEST_INT(physics->m_uiQueries, queries);                   // Out of range: no rays and no stale attenuation.
    EZ_TEST_FLOAT(source->GetOcclusion(listener, emitter), 1, 0); // Re-entry refreshes immediately.
    physics->m_bGeometricObstacle = false;
    physics->m_uiBlockedRays = 9;
    physics->m_fForcedHitDistance = 2000;
    world.Update();
    const float release = source->GetOcclusion(listener, emitter);
    EZ_TEST_BOOL(release > 0 && release < 1);
    EZ_TEST_FLOAT(settle(listener), 0, 0.00001f); // Reject a backend hit beyond the source.
    physics->m_fForcedHitDistance = -1;
    source->m_fOcclusionRange = 10;
    EZ_TEST_FLOAT(source->GetOcclusion(listener, ezVec3(9, 0, 0)), 0.5f, 0.00001f);
    source->m_fOcclusionRange = 0;
    EZ_TEST_FLOAT(source->GetOcclusion(listener, emitter), 0, 0);
    // Detached voices query their actual playback point, not the moving component.
    object->SetLocalPosition(emitter);
    source->m_fOcclusionRange = 20;
    world.Update();
    audio->SetListener(0, listener, ezVec3::MakeAxisX(), ezVec3::MakeAxisZ(), ezVec3::MakeZero());
    const ezUInt8 header[] = {'R', 'I', 'F', 'F', 36, 32, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
      1, 0, 1, 0, 128, 187, 0, 0, 0, 119, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 0, 32, 0, 0};
    ezDataBuffer wav;
    wav.SetCount(44 + 8192);
    ezMemoryUtils::ZeroFill(wav.GetData(), wav.GetCount());
    ezMemoryUtils::Copy(wav.GetData(), header, 44);
    auto* instance = audio->AllocateSoundInstance(wav, &world, source->GetHandle(), nullptr);
    EZ_TEST_FLOAT(instance->m_fOcclusion, 1, 0);
    instance->m_hComponent.Invalidate();
    ma_sound_set_position(&instance->m_Sound, 30, 0, 0);
    audio->UpdateEffects();
    EZ_TEST_FLOAT(instance->m_fOcclusion, 0, 0);
    ma_sound_set_position(&instance->m_Sound, 10, 0, 0);
    audio->UpdateEffects();
    EZ_TEST_FLOAT(instance->m_fOcclusion, 1, 0);
    world.GetComponentManager<ezMiniAudioSoundComponentManager>()->DeleteComponent(source->GetHandle());
    audio->UpdateEffects();
    EZ_TEST_FLOAT(instance->m_fOcclusion, 0, 0); // Never retain a deleted component's occlusion forever.
    audio->FreeSoundInstance(instance);
  }
  ezStartup::ShutdownHighLevelSystems();
  ezStartup::ShutdownCoreSystems();
}
