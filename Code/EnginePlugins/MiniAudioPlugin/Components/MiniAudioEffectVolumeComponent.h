#pragma once

#include <Core/World/World.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffect.h>

struct ezMsgUpdateLocalBounds;

/// Shared effects and exclusion settings for listener volumes.
class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioEffectVolumeComponent : public ezComponent
{
  EZ_DECLARE_ABSTRACT_COMPONENT_TYPE(ezMiniAudioEffectVolumeComponent, ezComponent);

public:
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;

  /// Width of the transition inside the boundary, in local units. Zero gives a hard boundary.
  float m_fFalloff = 1.0f; // [ property ]
  /// Use elapsed world time instead of distance to the boundary.
  bool m_bInterpolateByTime = false; // [ property ]
  /// Time to cover 90% of the remaining distance to the target, like Volume Sampler.
  ezTime m_InterpolationDuration = ezTime::MakeFromSeconds(1.0); // [ property ]
  /// Updates the shared listener weight once per world update, even without playing voices.
  void UpdateTemporalWeight(const ezVec3& vListener);
  float GetEffectWeight(const ezVec3& vListener) const;

  /// Lower priorities are processed first when zones overlap.
  ezInt32 m_iPriority = 0;                     // [ property ]
  ezDynamicArray<ezMiniAudioEffect> m_Effects; // [ property ]
  /// Empty includes every group. Exclusions take precedence.
  ezDynamicArray<ezString> m_IncludeGroups; // [ property ]
  ezDynamicArray<ezString> m_ExcludeGroups; // [ property ]
  virtual float GetWeight(const ezVec3& vGlobalPosition) const = 0;

protected:
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;
  bool GetLocalPosition(const ezVec3& vGlobalPosition, ezVec3& out_vLocalPosition) const;

  float CalculateWeight(float fDistanceInside) const;

private:
  float m_fTemporalWeight = 0.0f;
  ezUInt32 m_uiLastWeightUpdate = ezInvalidIndex;
};

using ezMiniAudioEffectBoxComponentManager = ezComponentManager<class ezMiniAudioEffectBoxComponent, ezBlockStorageType::Compact>;

/// Oriented box listener volume, with dimensions in local space.
class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioEffectBoxComponent : public ezMiniAudioEffectVolumeComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezMiniAudioEffectBoxComponent, ezMiniAudioEffectVolumeComponent, ezMiniAudioEffectBoxComponentManager);

public:
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;
  virtual float GetWeight(const ezVec3& vGlobalPosition) const override;
  void SetExtents(const ezVec3& vExtents);
  const ezVec3& GetExtents() const { return m_vExtents; }

protected:
  void OnUpdateLocalBounds(ezMsgUpdateLocalBounds& ref_msg) const;
  ezVec3 m_vExtents = ezVec3(10.0f);
};

using ezMiniAudioEffectSphereComponentManager = ezComponentManager<class ezMiniAudioEffectSphereComponent, ezBlockStorageType::Compact>;

/// Sphere listener volume. Non-uniform object scale produces an ellipsoid.
class EZ_MINIAUDIOPLUGIN_DLL ezMiniAudioEffectSphereComponent : public ezMiniAudioEffectVolumeComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezMiniAudioEffectSphereComponent, ezMiniAudioEffectVolumeComponent, ezMiniAudioEffectSphereComponentManager);

public:
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;
  virtual float GetWeight(const ezVec3& vGlobalPosition) const override;
  void SetRadius(float fRadius);
  float GetRadius() const { return m_fRadius; }

protected:
  void OnUpdateLocalBounds(ezMsgUpdateLocalBounds& ref_msg) const;
  float m_fRadius = 5.0f;
};
