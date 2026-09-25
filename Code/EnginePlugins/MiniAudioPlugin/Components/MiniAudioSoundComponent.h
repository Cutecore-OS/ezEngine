#pragma once

#include <Core/ResourceManager/ResourceHandle.h>
#include <Core/World/Component.h>
#include <Core/World/ComponentManager.h>
#include <Foundation/Types/TagSet.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffect.h>

/// Sphere manipulator whose center follows the audio emitter and whose radius is in world meters.
class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioOcclusionManipulatorAttribute : public ezSphereManipulatorAttribute
{
  EZ_ADD_DYNAMIC_REFLECTION(ezMiniAudioOcclusionManipulatorAttribute, ezSphereManipulatorAttribute);

public:
  ezMiniAudioOcclusionManipulatorAttribute()
    : ezSphereManipulatorAttribute("OcclusionRadius")
  {
  }
};

struct ezMiniAudioSoundInstance;
using ezMiniAudioSoundResourceHandle = ezTypedResourceHandle<class ezMiniAudioSoundResource>;

class ezMiniAudioSoundComponentManager : public ezComponentManager<class ezMiniAudioSoundComponent, ezBlockStorageType::FreeList>
{
public:
  ezMiniAudioSoundComponentManager(ezWorld* pWorld);

  virtual void Initialize() override;
  virtual void Deinitialize() override;

private:
  friend class ezMiniAudioSoundComponent;

  void UpdateEvents(const ezWorldModule::UpdateContext& context);

  ezUInt32 m_uiFirstComponentIndex = 0;
};

//////////////////////////////////////////////////////////////////////////

class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioSoundComponent : public ezComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezMiniAudioSoundComponent, ezComponent, ezMiniAudioSoundComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // ezComponent

public:
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;

protected:
  virtual void OnSimulationStarted() override;
  virtual void OnDeactivated() override;


  //////////////////////////////////////////////////////////////////////////
  // ezMiniAudioComponent

private:
  friend class ezComponentManagerSimple<class ezMiniAudioSoundComponent, ezComponentUpdateType::WhenSimulating>;


  //////////////////////////////////////////////////////////////////////////
  // ezMiniAudioSoundComponent

public:
  ezMiniAudioSoundComponent();
  ~ezMiniAudioSoundComponent();

  void SetPaused(bool b);                                // [ property ]
  bool GetPaused() const { return m_bPaused; }           // [ property ]

  void SetPitch(float f);                                // [ property ]
  float GetPitch() const { return m_fPitch; }            // [ property ]

  void SetVolume(float f);                               // [ property ]
  float GetVolume() const { return m_fComponentVolume; } // [ property ]

  /// If set, the global game speed does not affect the pitch of this event.
  ///
  /// This is important for global sounds, such as music or UI effects, so that they always play at their regular speed,
  /// even when the game is in slow motion.
  void SetNoGlobalPitch(bool bEnable); // [ property ]
  bool GetNoGlobalPitch() const;       // [ property ]

  /// Empty inherits the sound asset group.
  ezString m_sGroup;                       // [ property ]

  bool m_bUseOcclusion = false;            // [ property ]
  float m_fOcclusionThreshold = 0.5f;      // [ property ]
  ezUInt8 m_uiOcclusionCollisionLayer = 0; // [ property ]
  /// World-space source sphere radius. Listeners inside are never occluded.
  float m_fOcclusionRadius = 1.0f; // [ property ]
  /// Maximum source-listener distance for occlusion. Zero disables it.
  float m_fOcclusionRange = 50.0f; // [ property ]
  virtual ezVec3 GetSourcePosition() const;
  virtual float GetVolumeWeight(const ezVec3& vListener) const { return 1.0f; }
  float GetOcclusion(const ezVec3& vListener) const;
  float GetOcclusion(const ezVec3& vListener, const ezVec3& vSource) const;

  ezDynamicArray<ezMiniAudioEffect> m_Effects;             // [ property ]

  ezEnum<ezOnComponentFinishedAction2> m_OnFinishedAction; // [ property ]

  /// Makes the sound play.
  ///
  /// If it was not yet playing, it starts playing a new sound.
  /// If it was already playing, but paused, playback is resumed.
  /// If it was already playing, there is no change.
  void Play(); // [ scriptable ]

  /// If a sound is playing, it pauses at the current play position.
  ///
  /// Call Play() to resume playing.
  void Pause(); // [ scriptable ]

  /// Interrupts the sound playback abruptly.
  void Stop(); // [ scriptable ]

  /// Stops the sound, by fading it out over a short period.
  void FadeOut(ezTime fadeDuration); // [ scriptable ]

  /// Plays a completely new sound at the location of this component and with all its current properties.
  ///
  /// Pitch, volume, position and direction are copied to the new sound instance.
  /// The new sound then plays to the end and cannot be controlled through this component any further.
  /// No sound should be playing on this component, when using this function.
  void StartOneShot();                                    // [ scriptable ]

protected:
  void OnMsgDeleteGameObject(ezMsgDeleteGameObject& msg); // [ msg handler ]

  void Update();
  void UpdateParameters(ezMiniAudioSoundInstance* pInstance, float fVolume, float fPitch) const;

  friend class ezMiniAudioSingleton;
  void SoundFinished();

  ezMiniAudioSoundResourceHandle m_hSound;
  float m_fComponentVolume = 1.0f;
  float m_fPitch = 1.0f;
  float m_fResourceVolume = 1.0f;
  float m_fResourcePitch = 1.0f;
  bool m_bPaused = false;

  ezMiniAudioSoundInstance* m_pInstance = nullptr;

private:
  mutable ezTime m_LastOcclusionQuery = ezTime::MakeFromSeconds(-1);
  mutable float m_fOcclusion = 0.0f;
  mutable float m_fSmoothedOcclusion = 0.0f;
  mutable ezTime m_LastOcclusionSmoothing = ezTime::MakeFromSeconds(-1);
  mutable ezUInt8 m_uiLastOcclusionLayer = 0;
  mutable float m_fLastOcclusionRadius = -1, m_fLastOcclusionRange = -1;
  mutable ezVec3 m_vLastOcclusionSource = ezVec3::MakeZero();
  mutable ezVec3 m_vLastOcclusionListener = ezVec3::MakeZero();
};
