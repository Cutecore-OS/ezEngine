#pragma once
#include <Foundation/Threading/Mutex.h>
#include <MiniAudio/miniaudio.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffect.h>

/// All buses reach this node before master output. Sidechains are measured before
/// any ducking, so rules do not depend on bus order or feed back into one another.
struct EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioMixerNode
{
  static constexpr ezUInt32 MaxGroups = 64;
  ma_node_base m_Node;
  ezResult Initialize(ma_engine* pEngine);
  ~ezMiniAudioMixerNode();
  void Configure(ezArrayPtr<const ezString> groups, ezArrayPtr<const ezMiniAudioDucker> duckers);

private:
  struct Rule
  {
    ezUInt64 m_Sources = 0, m_Targets = 0;
    float m_fThreshold = 0, m_fReduction = 0, m_fAttackStep = 0, m_fReleaseStep = 0, m_fEnvelope = 0;
  };
  static void Process(ma_node*, const float**, ma_uint32*, float**, ma_uint32*);
  ezMutex m_Mutex;
  ezDynamicArray<Rule> m_Rules;
  ezUInt32 m_uiChannels = 0, m_uiRate = 0;
  float m_Gains[MaxGroups]; // Audio-thread-owned, retained if a configuration update is in progress.
  bool m_bInitialized = false;
};
