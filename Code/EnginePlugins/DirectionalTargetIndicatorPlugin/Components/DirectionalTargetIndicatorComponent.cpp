#include <DirectionalTargetIndicatorPlugin/DirectionalTargetIndicatorPluginPCH.h>

#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <DirectionalTargetIndicatorPlugin/Components/DirectionalTargetIndicatorComponent.h>
#include <DirectionalTargetIndicatorPlugin/Rendering/DirectionalTargetIndicatorRenderer.h>
#include <DirectionalTargetIndicatorPlugin/Rendering/DirectionalTargetIndicatorUtils.h>
#include <RendererCore/Pipeline/RenderDataManager.h>
#include <RendererCore/Pipeline/View.h>

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_TYPE(ezDirectionalTargetIndicatorElement, ezNoBase, 1, ezRTTIDefaultAllocator<ezDirectionalTargetIndicatorElement>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_MEMBER_PROPERTY("Texture", m_hTexture)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Texture_2D")),
    EZ_MEMBER_PROPERTY("Size", m_vSize)->AddAttributes(new ezDefaultValueAttribute(ezVec2(48.0f)), new ezClampValueAttribute(ezVec2(0.0f), ezVariant()), new ezSuffixAttribute(" px")),
    EZ_MEMBER_PROPERTY("Rotate", m_bRotate)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_MEMBER_PROPERTY("Offset", m_vOffset)->AddAttributes(new ezSuffixAttribute(" px")),
  }
  EZ_END_PROPERTIES;
}
EZ_END_STATIC_REFLECTED_TYPE;

EZ_BEGIN_COMPONENT_TYPE(ezDirectionalTargetIndicatorComponent, 2, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_RESOURCE_MEMBER_PROPERTY("MarkerTexture", m_hMarkerTexture)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Texture_2D")),
    EZ_MEMBER_PROPERTY("MarkerSize", m_vMarkerSize)->AddAttributes(new ezDefaultValueAttribute(ezVec2(48.0f)), new ezClampValueAttribute(ezVec2(0.0f), ezVariant()), new ezSuffixAttribute(" px")),
    EZ_MEMBER_PROPERTY("CaptureOffset", m_vCaptureOffset),
    EZ_MEMBER_PROPERTY("ShowOffscreenIndicator", m_bShowOffscreenIndicator)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_MEMBER_PROPERTY("ScreenMargin", m_fScreenMargin)->AddAttributes(new ezHiddenAttribute()),
    EZ_MEMBER_PROPERTY("ScreenOffset", m_fScreenOffset)->AddAttributes(new ezDefaultValueAttribute(1.5f), new ezClampValueAttribute(0.0f, 50.0f), new ezSuffixAttribute(" %")),
    EZ_MEMBER_PROPERTY("LeftOffset", m_fLeftOffset)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(0.0f, 50.0f), new ezSuffixAttribute(" %")),
    EZ_MEMBER_PROPERTY("RightOffset", m_fRightOffset)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(0.0f, 50.0f), new ezSuffixAttribute(" %")),
    EZ_MEMBER_PROPERTY("TopOffset", m_fTopOffset)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(0.0f, 50.0f), new ezSuffixAttribute(" %")),
    EZ_MEMBER_PROPERTY("BottomOffset", m_fBottomOffset)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(0.0f, 50.0f), new ezSuffixAttribute(" %")),
    EZ_MEMBER_PROPERTY("CornerRadius", m_fCornerRadius)->AddAttributes(new ezDefaultValueAttribute(5.0f), new ezClampValueAttribute(0.0f, 50.0f), new ezSuffixAttribute(" %")),
    EZ_ARRAY_MEMBER_PROPERTY("OffscreenElements", m_OffscreenElements),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgExtractRenderData, OnMsgExtractRenderData),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Rendering"),
    new ezTransformManipulatorAttribute("CaptureOffset"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_COMPONENT_TYPE;
// clang-format on

ezResult ezDirectionalTargetIndicatorElement::Serialize(ezStreamWriter& inout_stream) const
{
  inout_stream << m_hTexture;
  inout_stream << m_vSize;
  inout_stream << m_bRotate;
  inout_stream << m_vOffset;
  return EZ_SUCCESS;
}

ezResult ezDirectionalTargetIndicatorElement::Deserialize(ezStreamReader& inout_stream)
{
  inout_stream >> m_hTexture;
  inout_stream >> m_vSize;
  inout_stream >> m_bRotate;
  inout_stream >> m_vOffset;
  return EZ_SUCCESS;
}

ezDirectionalTargetIndicatorComponent::ezDirectionalTargetIndicatorComponent() = default;
ezDirectionalTargetIndicatorComponent::~ezDirectionalTargetIndicatorComponent() = default;

void ezDirectionalTargetIndicatorComponent::SerializeComponent(ezWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s << m_hMarkerTexture;
  s << m_vMarkerSize;
  s << m_vCaptureOffset;
  s << m_bShowOffscreenIndicator;
  s << m_fScreenMargin;
  s.WriteArray(m_OffscreenElements).IgnoreResult();
  s << m_fScreenOffset;
  s << m_fLeftOffset;
  s << m_fRightOffset;
  s << m_fTopOffset;
  s << m_fBottomOffset;
  s << m_fCornerRadius;
}

void ezDirectionalTargetIndicatorComponent::DeserializeComponent(ezWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();
  s >> m_hMarkerTexture;
  s >> m_vMarkerSize;
  s >> m_vCaptureOffset;
  s >> m_bShowOffscreenIndicator;
  s >> m_fScreenMargin;
  s.ReadArray(m_OffscreenElements).IgnoreResult();
  if (inout_stream.GetComponentTypeVersion(GetStaticRTTI()) >= 2)
  {
    s >> m_fScreenOffset;
    s >> m_fLeftOffset;
    s >> m_fRightOffset;
    s >> m_fTopOffset;
    s >> m_fBottomOffset;
    s >> m_fCornerRadius;
  }
  else
  {
    m_fScreenOffset = 0.0f;
    m_fCornerRadius = 0.0f;
  }
}

ezResult ezDirectionalTargetIndicatorComponent::GetLocalBounds(ezBoundingBoxSphere& out_bounds, bool& out_bAlwaysVisible, ezMsgUpdateLocalBounds& ref_msg)
{
  // Off-screen and occluded targets must still be extracted.
  out_bAlwaysVisible = true;
  return EZ_SUCCESS;
}

void ezDirectionalTargetIndicatorComponent::OnMsgExtractRenderData(ezMsgExtractRenderData& ref_msg) const
{
  const auto& view = *ref_msg.m_pView;
  if (ref_msg.m_OverrideCategory != ezInvalidRenderDataCategory ||
      view.GetCameraUsageHint() == ezCameraUsageHint::Shadow || view.GetCameraUsageHint() == ezCameraUsageHint::Reflection)
    return;

  const ezVec2 vViewportSize(view.GetViewport().width, view.GetViewport().height);
  if (vViewportSize.x <= 0.0f || vViewportSize.y <= 0.0f)
    return;

  const ezVec3 vTarget = GetOwner()->GetGlobalTransform().TransformPosition(m_vCaptureOffset);
  const ezVec4 vClip = view.GetViewProjectionMatrix() * vTarget.GetAsVec4(1.0f);
  if (!vClip.IsValid())
    return;

  const bool bInFront = (vTarget - view.GetCamera()->GetCenterPosition()).Dot(view.GetCamera()->GetCenterDirForwards()) > 0.0001f;
  const bool bOnScreen = bInFront && ezMath::Abs(vClip.w) > 0.000001f &&
                         ezMath::Abs(vClip.x) <= ezMath::Abs(vClip.w) && ezMath::Abs(vClip.y) <= ezMath::Abs(vClip.w);
  if (bOnScreen ? !m_hMarkerTexture.IsValid() : (!m_bShowOffscreenIndicator || m_OffscreenElements.IsEmpty()))
    return;

  using namespace ezDirectionalTargetIndicatorUtils;
  const ezVec2 vDirection = GetScreenDirection(vClip, vViewportSize);
  // Maps the texture's upward direction (0, -1) to the direction of the target.
  const ezVec2 vRotation(-vDirection.y, vDirection.x);
  ezVec2 vAnchor;
  if (bOnScreen)
  {
    vAnchor = ezVec2(vClip.x / vClip.w, -vClip.y / vClip.w).CompMul(vViewportSize) * 0.5f;
  }
  else
  {
    ezVec2 vExtent = ezVec2::MakeZero();
    for (const auto& element : m_OffscreenElements)
    {
      if (!element.m_hTexture.IsValid() || element.m_vSize.x <= 0.0f || element.m_vSize.y <= 0.0f)
        continue;

      const ezVec2 vRot = element.m_bRotate ? vRotation : ezVec2(1.0f, 0.0f);
      const ezVec2 vX = Rotate(ezVec2(element.m_vSize.x * 0.5f, 0.0f), vRot);
      const ezVec2 vY = Rotate(ezVec2(0.0f, element.m_vSize.y * 0.5f), vRot);
      const ezVec2 vOffset = Rotate(element.m_vOffset, vRot);
      vExtent = vExtent.CompMax(vX.Abs() + vY.Abs() + vOffset.Abs());
    }

    const ezVec2 vLegacyInset(ezMath::Max(0.0f, m_fScreenMargin));
    const ezVec4 vOffsets(m_fLeftOffset, m_fRightOffset, m_fTopOffset, m_fBottomOffset);
    vAnchor = GetRoundedEdgePosition(vDirection, vViewportSize, vExtent + vLegacyInset, m_fScreenOffset, vOffsets, m_fCornerRadius);
  }

  auto pData = ref_msg.m_pRenderDataManager->CreateRenderDataForThisFrame<ezDirectionalTargetIndicatorRenderData>(GetOwner());
  pData->m_vGlobalPosition = vTarget;
  const ezVec2 vPixelToClip(2.0f / vViewportSize.x, -2.0f / vViewportSize.y);
  auto addQuad = [&](const ezTexture2DResourceHandle& hTexture, const ezVec2& vSize, const ezVec2& vOffset, const ezVec2& vRot)
  {
    if (!hTexture.IsValid() || !vSize.IsValid() || !vOffset.IsValid() || vSize.x <= 0.0f || vSize.y <= 0.0f)
      return;

    auto& quad = pData->m_Quads.ExpandAndGetRef();
    quad.m_hTexture = hTexture;
    quad.m_vCenter = (vAnchor + Rotate(vOffset, vRot)).CompMul(vPixelToClip);
    quad.m_vAxisX = Rotate(ezVec2(vSize.x, 0.0f), vRot).CompMul(vPixelToClip);
    quad.m_vAxisY = Rotate(ezVec2(0.0f, vSize.y), vRot).CompMul(vPixelToClip);
  };

  if (bOnScreen)
    addQuad(m_hMarkerTexture, m_vMarkerSize, ezVec2::MakeZero(), ezVec2(1.0f, 0.0f));
  else
  {
    for (const auto& element : m_OffscreenElements)
      addQuad(element.m_hTexture, element.m_vSize, element.m_vOffset, element.m_bRotate ? vRotation : ezVec2(1.0f, 0.0f));
  }

  if (!pData->m_Quads.IsEmpty())
    ref_msg.AddRenderData(pData, ezDefaultRenderDataCategories::GUI, ezRenderData::Caching::Never);
}

// Keep old editor scenes visually unchanged; the hidden pixel margin remains serialized.
#include <Foundation/Serialization/AbstractObjectGraph.h>
#include <Foundation/Serialization/GraphPatch.h>

class ezDirectionalTargetIndicatorComponentPatch_1_2 : public ezGraphPatch
{
public:
  ezDirectionalTargetIndicatorComponentPatch_1_2()
    : ezGraphPatch("ezDirectionalTargetIndicatorComponent", 2)
  {
  }

  virtual void Patch(ezGraphPatchContext& ref_context, ezAbstractObjectGraph* pGraph, ezAbstractObjectNode* pNode) const override
  {
    pNode->AddProperty("ScreenOffset", 0.0f);
    pNode->AddProperty("CornerRadius", 0.0f);
  }
};

ezDirectionalTargetIndicatorComponentPatch_1_2 g_ezDirectionalTargetIndicatorComponentPatch_1_2;

EZ_STATICLINK_FILE(DirectionalTargetIndicatorPlugin, DirectionalTargetIndicatorPlugin_Components_DirectionalTargetIndicatorComponent);
