#pragma once
#include <RendererCore/RendererCoreDLL.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraphNode.h>

/// Adds or replaces one morph curve on a pose. Place before Pose Result.
class EZ_RENDERERCORE_DLL ezBlendShapeWeightAnimNode : public ezAnimGraphNode
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeWeightAnimNode, ezAnimGraphNode);

public:
  ezString m_sShape;
  float m_fWeight = 0;

protected:
  virtual ezResult SerializeNode(ezStreamWriter& inout_stream) const override;
  virtual ezResult DeserializeNode(ezStreamReader& inout_stream) override;
  virtual void Step(ezAnimController& inout_controller, ezAnimGraphInstance& inout_graph, ezTime diff, const ezSkeletonResource* pSkeleton, ezGameObject* pTarget) const override;

private:
  ezAnimGraphLocalPoseInputPin m_InPose;
  ezAnimGraphNumberInputPin m_InWeight;
  ezAnimGraphLocalPoseOutputPin m_OutPose;
};
