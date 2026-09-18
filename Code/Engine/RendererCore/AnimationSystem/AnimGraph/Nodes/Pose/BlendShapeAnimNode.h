#pragma once

#include <RendererCore/AnimationSystem/AnimGraph/AnimGraphNode.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraphPins.h>

class EZ_RENDERERCORE_DLL ezBlendShapeAnimNode : public ezAnimGraphNode
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeAnimNode, ezAnimGraphNode);

  //////////////////////////////////////////////////////////////////////////
  // ezAnimGraphNode

protected:
  virtual ezResult SerializeNode(ezStreamWriter& stream) const override;
  virtual ezResult DeserializeNode(ezStreamReader& stream) override;

  virtual void Step(ezAnimController& ref_controller, ezAnimGraphInstance& ref_graph, ezTime tDiff, const ezSkeletonResource* pSkeleton, ezGameObject* pTarget) const override;

  //////////////////////////////////////////////////////////////////////////
  // ezBlendShapeAnimNode

public:
  ezBlendShapeAnimNode();
  ~ezBlendShapeAnimNode();

  void SetShapeName(const char* szShape);
  const char* GetShapeName() const;

  float m_fWeight = 0.0f;

private:
  ezHashedString m_sShapeName;

  ezAnimGraphNumberInputPin m_InWeight;
  ezAnimGraphLocalPoseInputPin m_InPose;
  ezAnimGraphLocalPoseOutputPin m_OutPose;
};
