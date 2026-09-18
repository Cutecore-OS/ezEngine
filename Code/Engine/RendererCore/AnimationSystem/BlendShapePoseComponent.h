#pragma once

#include <Core/World/ComponentManager.h>
#include <Foundation/Containers/ArrayMap.h>
#include <Foundation/Types/RangeView.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <RendererCore/AnimationSystem/Declarations.h>

class ezBlendShapePoseComponentManager : public ezComponentManager<class ezBlendShapePoseComponent, ezBlockStorageType::Compact>
{
public:
  using SUPER = ezComponentManager<ezBlendShapePoseComponent, ezBlockStorageType::Compact>;

  ezBlendShapePoseComponentManager(ezWorld* pWorld)
    : SUPER(pWorld)
  {
  }

  void Update(const ezWorldModule::UpdateContext& context);
  void EnqueueUpdate(ezComponentHandle hComponent);

private:
  mutable ezMutex m_Mutex;
  ezDeque<ezComponentHandle> m_RequireUpdate;

protected:
  virtual void Initialize() override;
};

//////////////////////////////////////////////////////////////////////////

/// Used in conjunction with an ezAnimatedMeshComponent to set blend shape weights (morph targets / shape keys).
///
/// Automatically exposes all blend shapes found in the assigned blend shape asset as editable properties.
/// Changes can be tweaked in the editor and will be applied to the animated mesh in real time.
class EZ_RENDERERCORE_DLL ezBlendShapePoseComponent : public ezComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezBlendShapePoseComponent, ezComponent, ezBlendShapePoseComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // ezComponent

public:
  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;

protected:
  virtual void OnActivated() override;
  virtual void OnSimulationStarted() override;

  //////////////////////////////////////////////////////////////////////////
  // ezBlendShapePoseComponent

public:
  ezBlendShapePoseComponent();
  ~ezBlendShapePoseComponent();

  /// Sets the blend shape resource to use.
  void SetBlendShapes(const ezBlendShapeResourceHandle& hResource);
  const ezBlendShapeResourceHandle& GetBlendShapes() const { return m_hBlendShapes; }

  const ezRangeView<const char*, ezUInt32> GetWeights() const;
  void SetWeight(const char* szKey, const ezVariant& value);
  void RemoveWeight(const char* szKey);
  bool GetWeight(const char* szKey, ezVariant& out_value) const;

  /// Sets the weight for a specific shape key (AngelScript / scriptable).
  void SetBlendShapeWeight(ezStringView sShapeName, float fWeight);

  /// Gets the weight of a specific shape key (AngelScript / scriptable).
  float GetBlendShapeWeight(ezStringView sShapeName) const;

  /// Resets all weights to zero (AngelScript / scriptable).
  void ResetWeights();

  /// Instructs the component to apply the weights to the animated mesh again.
  void ResendWeights();

protected:
  void Update();
  void SendWeights();

  void OnMsgAnimationCurveValue(ezMsgAnimationCurveValue& msg);
  void OnMsgSetBlendShapeWeight(ezMsgSetBlendShapeWeight& msg);

  ezUInt8 m_uiResendWeights = 0;
  ezBlendShapeResourceHandle m_hBlendShapes;
  ezArrayMap<ezHashedString, float> m_Weights;
};
