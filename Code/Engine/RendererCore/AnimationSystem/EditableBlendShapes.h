#pragma once

#include <RendererCore/RendererCoreDLL.h>

#include <Foundation/Reflection/Reflection.h>
#include <Foundation/Strings/HashedString.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>

class EZ_RENDERERCORE_DLL ezEditableBlendShapeChannel : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezEditableBlendShapeChannel, ezReflectedClass);

public:
  ezString m_sName;
  float m_fDefaultWeight = 0.0f;
  float m_fMinWeight = 0.0f;
  float m_fMaxWeight = 1.0f;
  ezDynamicArray<ezBlendShapeVertexDelta> m_Deltas;
};

class EZ_RENDERERCORE_DLL ezEditableBlendShapes : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezEditableBlendShapes, ezReflectedClass);

public:
  ezEditableBlendShapes();
  ~ezEditableBlendShapes();

  ezString m_sSourceFile;
  ezString m_sPreviewMesh;
  ezMeshBufferResourceDescriptor m_MeshBufferDesc;
  ezDynamicArray<ezEditableBlendShapeChannel> m_Channels;

  void FillResourceDescriptor(ezBlendShapeResourceDescriptor& ref_desc) const;
};
