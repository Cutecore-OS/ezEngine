#include <EditorPluginGreyBox/EditorPluginGreyBoxPCH.h>

#include <EditorPluginGreyBox/GreyBoxPivot.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <Foundation/Containers/Set.h>
#include <Foundation/Math/Quat.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>

namespace
{
  constexpr const char* s_szNegative[] = {"SizeNegX", "SizeNegY", "SizeNegZ"};
  constexpr const char* s_szPositive[] = {"SizePosX", "SizePosY", "SizePosZ"};

  bool IsGreyBox(const ezDocumentObject* pObject)
  {
    return ezIsGreyBoxComponent(pObject);
  }

  template <typename T>
  ezStatus Read(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject, ezStringView sProperty, T& out_value)
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, sProperty, value));
    if (!value.IsA<T>())
      return ezStatus("Unexpected property type.");
    out_value = value.Get<T>();
    return ezStatus(EZ_SUCCESS);
  }

  ezStatus ReadBounds(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject, ezVec3& out_vNegative, ezVec3& out_vPositive)
  {
    for (ezUInt32 i = 0; i < 3; ++i)
    {
      EZ_SUCCEED_OR_RETURN(Read(accessor, pObject, s_szNegative[i], out_vNegative.GetData()[i]));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pObject, s_szPositive[i], out_vPositive.GetData()[i]));
    }
    if (!out_vNegative.IsValid() || !out_vPositive.IsValid())
      return ezStatus("Grey Boxing sizes must be finite.");
    return ezStatus(EZ_SUCCESS);
  }

  ezStatus Recalculate(ezObjectAccessorBase& accessor, const ezDocumentObject* pComponent, const ezVec3& vAnchor)
  {
    const ezDocumentObject* pOwner = pComponent->GetParent();
    if (pOwner == nullptr || pOwner->GetType()->GetTypeName() != "ezGameObject")
      return ezStatus("Grey Boxing must belong to a game object.");

    ezVec3 vNegative, vPositive;
    EZ_SUCCEED_OR_RETURN(ReadBounds(accessor, pComponent, vNegative, vPositive));
    // Negative sizes are distances from the origin, not signed minimum coordinates.
    const ezVec3 vOffset = -vNegative + (vNegative + vPositive).CompMul(vAnchor);
    if (vOffset.IsZero())
      return ezStatus(EZ_SUCCESS);

    ezVec3 vPosition, vScale;
    ezQuat qRotation;
    float fUniformScale;
    EZ_SUCCEED_OR_RETURN(Read(accessor, pOwner, "LocalPosition", vPosition));
    EZ_SUCCEED_OR_RETURN(Read(accessor, pOwner, "LocalRotation", qRotation));
    EZ_SUCCEED_OR_RETURN(Read(accessor, pOwner, "LocalScaling", vScale));
    EZ_SUCCEED_OR_RETURN(Read(accessor, pOwner, "LocalUniformScaling", fUniformScale));
    // ezTransform composes scale and rotation separately (without shear). Account for
    // ancestor scale before converting the world-space pivot displacement back to parent space.
    ezVec3 vParentScale(1.0f);
    for (auto pAncestor = pOwner->GetParent(); pAncestor != nullptr && pAncestor->GetType()->GetTypeName() == "ezGameObject"; pAncestor = pAncestor->GetParent())
    {
      ezVec3 vAncestorScale;
      float fAncestorUniformScale;
      EZ_SUCCEED_OR_RETURN(Read(accessor, pAncestor, "LocalScaling", vAncestorScale));
      EZ_SUCCEED_OR_RETURN(Read(accessor, pAncestor, "LocalUniformScaling", fAncestorUniformScale));
      vParentScale = vParentScale.CompMul(vAncestorScale) * fAncestorUniformScale;
    }
    if (!vParentScale.IsValid() || vParentScale.x == 0 || vParentScale.y == 0 || vParentScale.z == 0)
      return ezStatus("The parent transform must have finite, non-zero scale.");
    vPosition += (qRotation * vParentScale.CompMul(vScale).CompMul(vOffset) * fUniformScale).CompDiv(vParentScale);
    if (!vPosition.IsValid())
      return ezStatus("Recalculated position must be finite.");

    for (const ezDocumentObject* pChild : pOwner->GetChildren())
    {
      if (IsGreyBox(pChild))
      {
        EZ_SUCCEED_OR_RETURN(ReadBounds(accessor, pChild, vNegative, vPositive));
        vNegative += vOffset;
        vPositive -= vOffset;
        if (!vNegative.IsValid() || !vPositive.IsValid())
          return ezStatus("Recalculated sizes must be finite.");
        for (ezUInt32 i = 0; i < 3; ++i)
        {
          EZ_SUCCEED_OR_RETURN(accessor.SetValueByName(pChild, s_szNegative[i], vNegative.GetData()[i]));
          EZ_SUCCEED_OR_RETURN(accessor.SetValueByName(pChild, s_szPositive[i], vPositive.GetData()[i]));
        }
      }
      else if (pChild->GetParentProperty() == "Children")
      {
        ezVec3 vChildPosition;
        EZ_SUCCEED_OR_RETURN(Read(accessor, pChild, "LocalPosition", vChildPosition));
        vChildPosition -= vOffset;
        if (!vChildPosition.IsValid())
          return ezStatus("Recalculated child position must be finite.");
        EZ_SUCCEED_OR_RETURN(accessor.SetValueByName(pChild, "LocalPosition", vChildPosition));
      }
      else
      {
        return ezStatus("This object has other components whose positions cannot be compensated automatically. Move them to a child object first.");
      }
    }

    return accessor.SetValueByName(pOwner, "LocalPosition", vPosition);
  }
} // namespace

ezStatus ezRecalculateGreyBoxPivot(ezObjectAccessorBase& accessor,
  ezArrayPtr<const ezDocumentObject* const> components, const ezVec3& vAnchor)
{
  if (!vAnchor.IsValid() || vAnchor.CompMin(ezVec3::MakeZero()) != ezVec3::MakeZero() || vAnchor.CompMax(ezVec3(1)) != ezVec3(1))
    return ezStatus("Pivot anchor must be between zero and one.");
  if (components.IsEmpty())
    return ezStatus("Select a Grey Boxing component.");

  accessor.StartTransaction("Recalculate Grey Boxing Position");
  ezSet<const ezDocumentObject*> owners;
  for (const ezDocumentObject* pComponent : components)
  {
    ezStatus status(EZ_SUCCESS);
    if (!IsGreyBox(pComponent))
      status = ezStatus("Select only Grey Boxing components.");
    else if (!owners.Contains(pComponent->GetParent()))
    {
      owners.Insert(pComponent->GetParent());
      status = Recalculate(accessor, pComponent, vAnchor);
    }

    if (status.Failed())
    {
      accessor.CancelTransaction();
      return status;
    }
  }
  accessor.FinishTransaction();
  return ezStatus(EZ_SUCCESS);
}

