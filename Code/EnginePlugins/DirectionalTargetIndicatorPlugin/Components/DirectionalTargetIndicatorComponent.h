#pragma once

#include <DirectionalTargetIndicatorPlugin/DirectionalTargetIndicatorPluginDLL.h>
#include <RendererCore/Components/RenderComponent.h>
#include <RendererCore/Pipeline/RenderData.h>
#include <RendererCore/Textures/Texture2DResource.h>

/// A layer of the off-screen indicator. Unrotated arrows should point upwards.
struct EZ_DIRECTIONALTARGETINDICATORPLUGIN_DLL ezDirectionalTargetIndicatorElement
{
  ezTexture2DResourceHandle m_hTexture;
  ezVec2 m_vSize = ezVec2(48.0f);
  bool m_bRotate = true;
  ezVec2 m_vOffset = ezVec2::MakeZero(); ///< Pixels, applied before rotation about the shared anchor.

  ezResult Serialize(ezStreamWriter& inout_stream) const;
  ezResult Deserialize(ezStreamReader& inout_stream);
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_DIRECTIONALTARGETINDICATORPLUGIN_DLL, ezDirectionalTargetIndicatorElement);

using ezDirectionalTargetIndicatorComponentManager = ezComponentManager<class ezDirectionalTargetIndicatorComponent, ezBlockStorageType::Compact>;

/// Attach to a target to display a fixed pixel-size marker, or a layered arrow when outside the view.
/// Indicators are overlays: scene geometry does not occlude them.
class EZ_DIRECTIONALTARGETINDICATORPLUGIN_DLL ezDirectionalTargetIndicatorComponent : public ezRenderComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezDirectionalTargetIndicatorComponent, ezRenderComponent, ezDirectionalTargetIndicatorComponentManager);

public:
  ezDirectionalTargetIndicatorComponent();
  ~ezDirectionalTargetIndicatorComponent();

  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;
  virtual ezResult GetLocalBounds(ezBoundingBoxSphere& out_bounds, bool& out_bAlwaysVisible, ezMsgUpdateLocalBounds& ref_msg) override;

  ezTexture2DResourceHandle m_hMarkerTexture;
  ezVec2 m_vMarkerSize = ezVec2(48.0f);
  ezVec3 m_vCaptureOffset = ezVec3::MakeZero();
  bool m_bShowOffscreenIndicator = true;
  float m_fScreenMargin = 0.0f; ///< Legacy pixel inset, retained for version 1 scenes.
  float m_fScreenOffset = 1.5f; ///< Percentage of the corresponding viewport dimension.
  float m_fLeftOffset = 0.0f;
  float m_fRightOffset = 0.0f;
  float m_fTopOffset = 0.0f;
  float m_fBottomOffset = 0.0f;
  float m_fCornerRadius = 5.0f; ///< Percentage of the shorter viewport dimension.
  ezHybridArray<ezDirectionalTargetIndicatorElement, 3> m_OffscreenElements;

protected:
  void OnMsgExtractRenderData(ezMsgExtractRenderData& ref_msg) const;
};
