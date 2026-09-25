#pragma once

#include <Foundation/Threading/Mutex.h>
#include <Foundation/Types/UniquePtr.h>
#include <MiniAudio/miniaudio.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffect.h>

/// A per-source graph node. All buffers are prepared on the game thread.
/// The audio callback never allocates or waits for the configuration lock.
struct EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioEffectNode
{
  ma_node_base m_Node; // Must be first: MiniAudio passes this address to Process().

  ezMiniAudioEffectNode();
  ~ezMiniAudioEffectNode();
  ezResult Initialize(ma_engine* pEngine, ma_node* pDestination);
  void Configure(ezArrayPtr<const ezMiniAudioEffectInstance> effects);
  float GetTailSeconds() const { return m_fTailSeconds; }

private:
  struct Stage;
  static void Process(ma_node* pNode, const float** pInputs, ma_uint32* pInputFrames, float** pOutputs, ma_uint32* pOutputFrames);

  ezMutex m_Mutex;
  ezDynamicArray<ezUniquePtr<Stage>> m_Stages;
  ezUInt32 m_uiChannels = 0;
  ezUInt32 m_uiSampleRate = 0;
  bool m_bInitialized = false;
  float m_fTailSeconds = 0.0f;
};
