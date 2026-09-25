#include <EditorPluginGreyBox/EditorPluginGreyBoxPCH.h>

#include <Core/Graphics/Geometry.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <EditorPluginGreyBox/GreyBoxVertices.h>
#include <Foundation/Math/Transform.h>
#include <GameEngine/Gameplay/GreyBoxComponent.h>
#include <ToolsFoundation/Document/Document.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>

namespace
{
  template <typename T>
  ezStatus Read(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject, const char* szProperty, T& out_value)
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, szProperty, value));
    if (!value.CanConvertTo<T>())
      return ezStatus("Invalid Grey Boxing property type.");
    out_value = value.ConvertTo<T>();
    return ezStatus(EZ_SUCCESS);
  }

  // A detached, editor-only geometry adapter. Reuse the protected component builder rather than
  // duplicating its shape algorithms; no world, component registration or GPU resources are created.
  class ezGreyBoxGeometryBuilder : public ezGreyBoxComponent
  {
  public:
    ezStatus Build(ezObjectAccessorBase& accessor, const ezDocumentObject* pComponent, ezGeometry& out_geometry)
    {
      ezInt64 iShape;
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "Shape", iShape));
      if (iShape < ezGreyBoxShape::Box || iShape > ezGreyBoxShape::SpiralStairs)
        return ezStatus("Unknown Grey Boxing shape.");
      m_Shape = static_cast<ezGreyBoxShape::Enum>(iShape);
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SizeNegX", m_fSizeNegX));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SizePosX", m_fSizePosX));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SizeNegY", m_fSizeNegY));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SizePosY", m_fSizePosY));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SizeNegZ", m_fSizeNegZ));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SizePosZ", m_fSizePosZ));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "Detail", m_uiDetail));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "Curvature", m_Curvature));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "Thickness", m_fThickness));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SlopedTop", m_bSlopedTop));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pComponent, "SlopedBottom", m_bSlopedBottom));
      const ezVec3 vNegative(m_fSizeNegX, m_fSizeNegY, m_fSizeNegZ);
      const ezVec3 vPositive(m_fSizePosX, m_fSizePosY, m_fSizePosZ);
      if (!vNegative.IsValid() || !vPositive.IsValid() || !(vNegative + vPositive).IsValid() ||
          !ezMath::IsFinite(m_Curvature.GetRadian()) || !ezMath::IsFinite(m_fThickness))
        return ezStatus("Grey Boxing geometry must be finite.");
      // Apply the component's own curvature rounding before invoking its geometry builder.
      SetCurvature(m_Curvature);
      if (m_uiDetail == 0 || m_uiDetail > 4096)
        return ezStatus("Grey Boxing Detail is outside the supported range.");
      BuildGeometry(out_geometry, m_Shape, false);
      return ezStatus(EZ_SUCCESS);
    }
  };

  ezStatus GetTransform(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject, ezTransform& out_transform)
  {
    out_transform = ezTransform::MakeIdentity();
    if (pObject == nullptr || pObject->GetType()->GetTypeName() != "ezGameObject")
      return ezStatus(EZ_SUCCESS);
    ezTransform parent, local;
    float fUniformScale;
    EZ_SUCCEED_OR_RETURN(GetTransform(accessor, pObject->GetParent(), parent));
    EZ_SUCCEED_OR_RETURN(Read(accessor, pObject, "LocalPosition", local.m_vPosition));
    EZ_SUCCEED_OR_RETURN(Read(accessor, pObject, "LocalRotation", local.m_qRotation));
    EZ_SUCCEED_OR_RETURN(Read(accessor, pObject, "LocalScaling", local.m_vScale));
    EZ_SUCCEED_OR_RETURN(Read(accessor, pObject, "LocalUniformScaling", fUniformScale));
    local.m_vScale *= fUniformScale;
    out_transform = parent * local;
    return ezStatus(EZ_SUCCESS);
  }
} // namespace

ezStatus ezBuildGreyBoxVertices(ezObjectAccessorBase& accessor, const ezDocumentObject* pComponent, ezDynamicArray<ezVec3>& out_vertices)
{
  out_vertices.Clear();
  if (!ezIsGreyBoxComponent(pComponent))
    return ezStatus("Select a Grey Boxing component.");
  ezGreyBoxGeometryBuilder builder;
  ezGeometry geometry;
  if (pComponent->GetType()->GetTypeName() == "ezGreyBoxConeComponent")
  {
    EZ_SUCCEED_OR_RETURN(ezBuildGreyBoxExtensionGeometry(accessor, pComponent, geometry));
  }
  else
  {
    EZ_SUCCEED_OR_RETURN(builder.Build(accessor, pComponent, geometry));
  }
  for (const auto& vertex : geometry.GetVertices())
  {
    if (!vertex.m_vPosition.IsValid())
      return ezStatus("The generated geometry contains an invalid vertex.");
    out_vertices.PushBack(vertex.m_vPosition);
  }

  // Hard normals and UV seams duplicate positions. Sort once when the shape changes rather
  // than comparing every pair; high-detail stairs can contain thousands of split vertices.
  out_vertices.Sort([](const ezVec3& a, const ezVec3& b)
    {
      if (a.x != b.x)
        return a.x < b.x;
      if (a.y != b.y)
        return a.y < b.y;
      return a.z < b.z; });
  ezUInt32 uiUnique = 0;
  for (ezUInt32 i = 0; i < out_vertices.GetCount(); ++i)
  {
    if (uiUnique == 0 || out_vertices[i] != out_vertices[uiUnique - 1])
      out_vertices[uiUnique++] = out_vertices[i];
  }
  out_vertices.SetCount(uiUnique);
  return ezStatus(EZ_SUCCESS);
}

ezStatus ezSnapGreyBoxVertex(ezObjectAccessorBase& accessor, const ezDocumentObject* pComponent,
  const ezVec3& vLocalSource, const ezVec3& vWorldTarget)
{
  if (!ezIsGreyBoxComponent(pComponent) ||
      pComponent->GetParent() == nullptr || pComponent->GetParent()->GetType()->GetTypeName() != "ezGameObject")
    return ezStatus("The source must belong to a Grey Boxing object.");
  if (!vLocalSource.IsValid() || !vWorldTarget.IsValid())
    return ezStatus("Vertex positions must be finite.");
  if (accessor.GetObjectManager()->GetDocument()->IsReadOnly())
    return ezStatus("The source document is read-only.");

  const auto* pOwner = pComponent->GetParent();
  ezTransform world, parent;
  EZ_SUCCEED_OR_RETURN(GetTransform(accessor, pOwner, world));
  EZ_SUCCEED_OR_RETURN(GetTransform(accessor, pOwner->GetParent(), parent));
  if (!parent.m_vScale.IsValid() || parent.m_vScale.x == 0 || parent.m_vScale.y == 0 || parent.m_vScale.z == 0)
    return ezStatus("The parent transform must have finite, non-zero scale.");
  ezVec3 vPosition;
  EZ_SUCCEED_OR_RETURN(Read(accessor, pOwner, "LocalPosition", vPosition));
  const ezVec3 vDelta = vWorldTarget - world.TransformPosition(vLocalSource);
  vPosition += (parent.m_qRotation.GetInverse() * vDelta).CompDiv(parent.m_vScale);
  if (!vPosition.IsValid())
    return ezStatus("The resulting position is invalid.");

  accessor.StartTransaction("Snap Grey Boxing Vertices");
  const ezStatus status = accessor.SetValueByName(pOwner, "LocalPosition", vPosition);
  if (status.Failed())
    accessor.CancelTransaction();
  else
    accessor.FinishTransaction();
  return status;
}

