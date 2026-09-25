#include <RendererCore/RendererCorePCH.h>
#include <RendererCore/AnimationSystem/AnimGraph/Nodes/BlendShapes/BlendShapeAnimNode.h>
#include <Core/World/World.h>
#include <Foundation/Reflection/Reflection.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimController.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraphInstance.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeWeightAnimNode, 1, ezRTTIDefaultAllocator<ezBlendShapeWeightAnimNode>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Shape", m_sShape),
    EZ_MEMBER_PROPERTY("Weight", m_fWeight)->AddAttributes(new ezClampValueAttribute(-1.0f, 1.0f)),
    EZ_MEMBER_PROPERTY("InPose", m_InPose)->AddAttributes(new ezHiddenAttribute()),
    EZ_MEMBER_PROPERTY("InWeight", m_InWeight)->AddAttributes(new ezHiddenAttribute()),
    EZ_MEMBER_PROPERTY("OutPose", m_OutPose)->AddAttributes(new ezHiddenAttribute()),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Blend Shapes"),
    new ezTitleAttribute("Blend Shape: {Shape}"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezResult ezBlendShapeWeightAnimNode::SerializeNode(ezStreamWriter& inout_stream) const
{
  inout_stream.WriteVersion(1);
  EZ_SUCCEED_OR_RETURN(SUPER::SerializeNode(inout_stream));
  inout_stream << m_sShape;
  inout_stream << m_fWeight;
  EZ_SUCCEED_OR_RETURN(m_InPose.Serialize(inout_stream));
  EZ_SUCCEED_OR_RETURN(m_InWeight.Serialize(inout_stream));
  return m_OutPose.Serialize(inout_stream);
}

ezResult ezBlendShapeWeightAnimNode::DeserializeNode(ezStreamReader& inout_stream)
{
  inout_stream.ReadVersion(1);
  EZ_SUCCEED_OR_RETURN(SUPER::DeserializeNode(inout_stream));
  inout_stream >> m_sShape;
  inout_stream >> m_fWeight;
  EZ_SUCCEED_OR_RETURN(m_InPose.Deserialize(inout_stream));
  EZ_SUCCEED_OR_RETURN(m_InWeight.Deserialize(inout_stream));
  return m_OutPose.Deserialize(inout_stream);
}

void ezBlendShapeWeightAnimNode::Step(ezAnimController& inout_controller, ezAnimGraphInstance& inout_graph, ezTime diff, const ezSkeletonResource* pSkeleton, ezGameObject* pTarget) const
{
  if (!m_InPose.IsConnected() || !m_OutPose.IsConnected())
    return;
  auto pOut = inout_controller.AddPinDataLocalTransforms();
  auto pIn = m_InPose.GetPose(inout_controller, inout_graph);
  if (pIn == nullptr)
    return;
  const ezUInt16 uiIndex = pOut->m_uiOwnIndex;
  *pOut = *pIn;
  pOut->m_uiOwnIndex = uiIndex;
  if (!m_sShape.IsEmpty())
  {
    ezStringBuilder name;
    ezBlendShapeResourceDescriptor::MakeCurveName(m_sShape, name);
    ezHashedString hash;
    hash.Assign(name);
    ezAnimGraphCustomCurveData* pCurve = nullptr;
    for (auto& curve : pOut->m_CustomCurveValues)
      if (curve.m_sName == hash)
        pCurve = &curve;
    if (pCurve == nullptr)
      pCurve = &pOut->m_CustomCurveValues.ExpandAndGetRef();
    pCurve->m_sName = hash;
    pCurve->m_fValue = ezBlendShapeResourceDescriptor::SanitizeWeight(static_cast<float>(m_InWeight.GetNumber(inout_graph, m_fWeight)), 0);
  }
  m_OutPose.SetPose(inout_graph, pOut);
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_AnimationSystem_AnimGraph_Nodes_BlendShapes_BlendShapeAnimNode);
