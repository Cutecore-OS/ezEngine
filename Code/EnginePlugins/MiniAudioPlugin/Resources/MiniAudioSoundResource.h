#pragma once

#include <Core/ResourceManager/Resource.h>
#include <Foundation/Threading/Mutex.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffect.h>

class ezRandom;
class ezWorld;
struct ezMiniAudioSoundInstance;
struct ezComponentHandle;

using ezMiniAudioSoundResourceHandle = ezTypedResourceHandle<class ezMiniAudioSoundResource>;

struct EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioSoundResourceDescriptor
{
  // empty, these types of resources must be loaded from file
};

class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioSoundResource : public ezResource
{
  EZ_ADD_DYNAMIC_REFLECTION(ezMiniAudioSoundResource, ezResource);
  EZ_RESOURCE_DECLARE_COMMON_CODE(ezMiniAudioSoundResource);
  EZ_RESOURCE_DECLARE_CREATEABLE(ezMiniAudioSoundResource, ezMiniAudioSoundResourceDescriptor);

public:
  ezMiniAudioSoundResource();
  ~ezMiniAudioSoundResource();

  const ezDataBuffer& GetAudioData() const;
  const ezDataBuffer& GetAudioData(ezRandom& ref_rng) const;

  const ezDynamicArray<ezMiniAudioEffect>& GetEffects() const { return m_Effects; }
  ezStringView GetGroup() const { return m_sSoundGroup; }
  ezUInt32 SelectVariation(ezRandom& rng) const;

  bool GetLoop() const { return m_bLoop; }
  float GetVolume(ezRandom& ref_rng) const;
  float GetPitch(ezRandom& ref_rng) const;
  bool GetSpatialize() const { return m_bSpatialize; }
  float GetDopplerFactor() const { return m_fDopplerFactor; }
  float GetMinDistance() const { return m_fMinDistance; }
  float GetMaxDistance() const { return m_fMaxDistance; }
  float GetRolloff() const { return m_fRolloff; }

  /// Instantiates the sound, all arguments are optional.
  ezMiniAudioSoundInstance* InstantiateSound(ezRandom* pRng, ezWorld* pWorld, const ezComponentHandle& hComponent);

private:
  virtual ezResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual ezResourceLoadDesc UpdateContent(ezStreamReader* pStream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

private:
  ezHybridArray<ezDataBuffer, 1> m_AudioData;

  ezDynamicArray<ezMiniAudioEffect> m_Effects;
  bool m_bRandomWithoutRepeats = false;
  mutable ezMutex m_VariationMutex;
  mutable ezDynamicArray<ezUInt32> m_ShuffleBag;
  mutable ezUInt32 m_uiLastVariation = ezInvalidIndex;
  ezString m_sSoundGroup;
  bool m_bLoop = false;
  float m_fMinVolume = 1.0f;
  float m_fMaxVolume = 1.0f;
  float m_fMinDistance = 1.0f;
  float m_fMaxDistance = 10.0f;
  float m_fMinPitch = 1.0f;
  float m_fMaxPitch = 1.0f;
  bool m_bSpatialize = true;
  float m_fDopplerFactor = 1.0f;
  float m_fRolloff = 1.0f;
};
