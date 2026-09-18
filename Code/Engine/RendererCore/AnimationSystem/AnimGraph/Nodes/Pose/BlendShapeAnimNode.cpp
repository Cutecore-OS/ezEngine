#include <RendererCore/RendererCorePCH.h>

#include <Core/World/GameObject.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimController.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraph.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraphInstance.h>
#include <RendererCore/AnimationSystem/AnimGraph/Nodes/Pose/BlendShapeAnimNode.h>
#include <RendererCore/AnimationSystem/Declarations.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeAnimNode, 1, ezRTTIDefaultAllocator<ezBlendShapeAnimNode>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("ShapeName", GetShapeName, SetShapeName),
    EZ_MEMBER_PROPERTY("Weight", m_fWeight)->AddAttributes(new ezDefaultValueAttribute(0.0f), new ezClampValueAttribute(0.0f, 1.0f)),

    EZ_MEMBER_PROPERTY("InWeight", m_InWeight)->AddAttributes(new ezHiddenAttribute()),
    EZ_MEMBER_PROPERTY("InPose", m_InPose)->AddAttributes(new ezHiddenAttribute()),
    EZ_MEMBER_PROPERTY("OutPose", m_OutPose)->AddAttributes(new ezHiddenAttribute()),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Pose Modification"),
    new ezColorAttribute(ezColorScheme::DarkUI(ezColorScheme::Teal)),
    new ezTitleAttribute("Blend Shape: '{ShapeName}'"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezBlendShapeAnimNode::ezBlendShapeAnimNode() = default;
ezBlendShapeAnimNode::~ezBlendShapeAnimNode() = default;

ezResult ezBlendShapeAnimNode::SerializeNode(ezStreamWriter& stream) const
{
  stream.WriteVersion(1);

  EZ_SUCCEED_OR_RETURN(SUPER::SerializeNode(stream));

  stream << m_sShapeName;
  stream << m_fWeight;

  EZ_SUCCEED_OR_RETURN(m_InWeight.Serialize(stream));
  EZ_SUCCEED_OR_RETURN(m_InPose.Serialize(stream));
  EZ_SUCCEED_OR_RETURN(m_OutPose.Serialize(stream));

  return EZ_SUCCESS;
}

ezResult ezBlendShapeAnimNode::DeserializeNode(ezStreamReader& stream)
{
  const auto version = stream.ReadVersion(1);
  EZ_IGNORE_UNUSED(version);

  EZ_SUCCEED_OR_RETURN(SUPER::DeserializeNode(stream));

  stream >> m_sShapeName;
  stream >> m_fWeight;

  EZ_SUCCEED_OR_RETURN(m_InWeight.Deserialize(stream));
  EZ_SUCCEED_OR_RETURN(m_InPose.Deserialize(stream));
  EZ_SUCCEED_OR_RETURN(m_OutPose.Deserialize(stream));

  return EZ_SUCCESS;
}

void ezBlendShapeAnimNode::Step(ezAnimController& ref_controller, ezAnimGraphInstance& ref_graph, ezTime tDiff, const ezSkeletonResource* pSkeleton, ezGameObject* pTarget) const
{
  const float fWeight = static_cast<float>(m_InWeight.GetNumber(ref_graph, m_fWeight));

  if (m_InPose.IsConnected() && m_OutPose.IsConnected())
  {
    if (auto pCurrentLocalTransforms = m_InPose.GetPose(ref_controller, ref_graph))
    {
      ezAnimGraphPinDataLocalTransforms* pLocalTransforms = ref_controller.AddPinDataLocalTransforms();

      // Re-query pointer as AddPinDataLocalTransforms could have reallocated
      pCurrentLocalTransforms = m_InPose.GetPose(ref_controller, ref_graph);

      pLocalTransforms->m_CommandID = pCurrentLocalTransforms->m_CommandID;
      pLocalTransforms->m_pWeights = pCurrentLocalTransforms->m_pWeights;
      pLocalTransforms->m_fOverallWeight = pCurrentLocalTransforms->m_fOverallWeight;
      pLocalTransforms->m_bUseRootMotion = pCurrentLocalTransforms->m_bUseRootMotion;
      pLocalTransforms->m_vRootMotion = pCurrentLocalTransforms->m_vRootMotion;
      pLocalTransforms->m_CustomCurveValues = pCurrentLocalTransforms->m_CustomCurveValues;

      bool bFound = false;
      for (auto& cv : pLocalTransforms->m_CustomCurveValues)
      {
        if (cv.m_sName == m_sShapeName)
        {
          cv.m_fValue = fWeight;
          bFound = true;
          break;
        }
      }

      if (!bFound && !m_sShapeName.IsEmpty())
      {
        auto& cv = pLocalTransforms->m_CustomCurveValues.ExpandAndGetRef();
        cv.m_sName = m_sShapeName;
        cv.m_fValue = fWeight;
      }

      m_OutPose.SetPose(ref_graph, pLocalTransforms);
    }
  }

  if (pTarget != nullptr && !m_sShapeName.IsEmpty())
  {
    ezMsgSetBlendShapeWeight msg;
    msg.m_sShapeName = m_sShapeName;
    msg.m_fWeight = fWeight;
    pTarget->SendMessage(msg);
  }
}

void ezBlendShapeAnimNode::SetShapeName(const char* szShape)
{
  m_sShapeName.Assign(szShape);
}

const char* ezBlendShapeAnimNode::GetShapeName() const
{
  return m_sShapeName.GetData();
}

EZ_STATICLINK_FILE(RendererCore, RendererCore_AnimationSystem_AnimGraph_Nodes_Pose_BlendShapeAnimNode);
