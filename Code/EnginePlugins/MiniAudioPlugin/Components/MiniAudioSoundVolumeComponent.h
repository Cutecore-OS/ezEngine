#pragma once

#include <Core/World/World.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundComponent.h>

struct ezMsgUpdateLocalBounds;

/// A sound source whose audible output is confined to a listener volume.
class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioSoundVolumeComponent : public ezMiniAudioSoundComponent
{
  EZ_DECLARE_ABSTRACT_COMPONENT_TYPE(ezMiniAudioSoundVolumeComponent, ezMiniAudioSoundComponent);

public:
  virtual void SerializeComponent(ezWorldWriter& stream) const override;
  virtual void DeserializeComponent(ezWorldReader& stream) override;
  virtual ezVec3 GetSourcePosition() const override;
  virtual float GetVolumeWeight(const ezVec3& vListener) const override;
  virtual float GetWeight(const ezVec3& vListener) const = 0;
  void UpdateTemporalWeight(const ezVec3& vListener);
  void Update();

  bool m_bInterpolateByTime = false;                           // [ property ]
  float m_fFalloff = 1.0f;                                     // [ property ]
  ezTime m_InterpolationDuration = ezTime::MakeFromSeconds(1); // [ property ]
  ezVec3 m_vCaptureOffset = ezVec3::MakeZero();                // [ property ]

protected:
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;
  bool GetLocalPosition(const ezVec3& vGlobalPosition, ezVec3& out_vLocal) const;
  float CalculateWeight(float fDistanceInside) const;

private:
  ezUInt32 m_uiLastWeightUpdate = ezInvalidIndex;
  float m_fTemporalWeight = 0;
};

using ezMiniAudioSoundBoxComponentManager = ezComponentManagerSimple<class ezMiniAudioSoundBoxComponent, ezComponentUpdateType::WhenSimulating, ezBlockStorageType::FreeList, ezWorldUpdatePhase::PostTransform>;

class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioSoundBoxComponent : public ezMiniAudioSoundVolumeComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezMiniAudioSoundBoxComponent, ezMiniAudioSoundVolumeComponent, ezMiniAudioSoundBoxComponentManager);

public:
  virtual void SerializeComponent(ezWorldWriter& stream) const override;
  virtual void DeserializeComponent(ezWorldReader& stream) override;
  virtual float GetWeight(const ezVec3& vListener) const override;
  void SetExtents(const ezVec3& vExtents);
  const ezVec3& GetExtents() const { return m_vExtents; }

protected:
  void OnUpdateLocalBounds(ezMsgUpdateLocalBounds& msg) const;
  ezVec3 m_vExtents = ezVec3(10);
};

using ezMiniAudioSoundSphereComponentManager = ezComponentManagerSimple<class ezMiniAudioSoundSphereComponent, ezComponentUpdateType::WhenSimulating, ezBlockStorageType::FreeList, ezWorldUpdatePhase::PostTransform>;

class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioSoundSphereComponent : public ezMiniAudioSoundVolumeComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezMiniAudioSoundSphereComponent, ezMiniAudioSoundVolumeComponent, ezMiniAudioSoundSphereComponentManager);

public:
  virtual void SerializeComponent(ezWorldWriter& stream) const override;
  virtual void DeserializeComponent(ezWorldReader& stream) override;
  virtual float GetWeight(const ezVec3& vListener) const override;
  void SetRadius(float fRadius);
  float GetRadius() const { return m_fRadius; }

protected:
  void OnUpdateLocalBounds(ezMsgUpdateLocalBounds& msg) const;
  float m_fRadius = 5;
};
