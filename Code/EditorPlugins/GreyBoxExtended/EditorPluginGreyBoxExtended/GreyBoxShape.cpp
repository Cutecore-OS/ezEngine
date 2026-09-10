#include <EditorPluginGreyBoxExtended/EditorPluginGreyBoxExtendedPCH.h>

#include <EditorPluginGreyBoxExtended/GreyBoxShape.h>
#include <GreyBoxPlugin/Components/GreyBoxConeComponent.h>
#include <ToolsFoundation/Document/Document.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>

ezStatus ezReadGreyBoxConeSettings(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject, ezGreyBoxConeSettings& out_settings)
{
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "SizeNegX", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_vNegative.x = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "SizePosX", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_vPositive.x = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "SizeNegY", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_vNegative.y = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "SizePosY", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_vPositive.y = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "SizeNegZ", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_vNegative.z = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "SizePosZ", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_vPositive.z = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "BaseRadiusScale", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_fBaseRadiusScale = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "TopRadiusScale", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_fTopRadiusScale = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "Sides", value));
    if (!value.CanConvertTo<ezUInt32>())
      return ezStatus("Invalid cone property.");
    out_settings.m_uiSides = value.ConvertTo<ezUInt32>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "HeightSegments", value));
    if (!value.CanConvertTo<ezUInt32>())
      return ezStatus("Invalid cone property.");
    out_settings.m_uiHeightSegments = value.ConvertTo<ezUInt32>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "ProfileCurve", value));
    if (!value.CanConvertTo<float>())
      return ezStatus("Invalid cone property.");
    out_settings.m_fProfileCurve = value.ConvertTo<float>();
  }
  {
    ezVariant value;
    EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, "SmoothShading", value));
    if (!value.CanConvertTo<bool>())
      return ezStatus("Invalid cone property.");
    out_settings.m_bSmoothShading = value.ConvertTo<bool>();
  }
  out_settings.m_uiShape = accessor.GetByName<ezUInt8>(pObject, "Shape");
  out_settings.m_uiDetail = accessor.GetByName<ezUInt32>(pObject, "Detail");
  out_settings.m_Curvature = accessor.GetByName<ezAngle>(pObject, "Curvature");
  out_settings.m_fThickness = accessor.GetByName<float>(pObject, "Thickness");
  out_settings.m_bSlopedTop = accessor.GetByName<bool>(pObject, "SlopedTop");
  out_settings.m_bSlopedBottom = accessor.GetByName<bool>(pObject, "SlopedBottom");
  return ezStatus(EZ_SUCCESS);
}

namespace
{
  ezStatus SetShape(ezObjectAccessorBase& accessor, const ezUuid& guid, ezInt64 iShape, bool bForcePlugin = false)
  {
    const auto* pObject = accessor.GetObject(guid);
    if (!ezIsGreyBoxComponent(pObject))
      return ezStatus("Select only Grey Boxing components.");
    const bool bCone = iShape == ezGreyBoxConeShape::Cone;
    if (pObject->GetType()->GetTypeName() == "ezGreyBoxConeComponent" || (!bCone && !bForcePlugin))
      return accessor.SetValueByName(pObject, "Shape", iShape);

    const ezRTTI* pTargetType = ezRTTI::FindTypeByName("ezGreyBoxConeComponent");
    if (pTargetType == nullptr)
      return ezStatus("Enable the Grey Boxing Tools plugin first.");

    // Copy shared reflected properties, including component flags. Shape-specific settings
    // stay in the undo command for the old component and are restored when undoing.
    ezHybridArray<const ezAbstractProperty*, 32> properties;
    pTargetType->GetAllProperties(properties);
    ezVariantDictionary values;
    for (const auto* pProperty : properties)
    {
      if (pProperty->GetCategory() != ezPropertyCategory::Member || pProperty->GetPropertyName() == "Shape" ||
          pProperty->GetFlags().IsSet(ezPropertyFlags::ReadOnly))
        continue;
      if (pObject->GetType()->FindPropertyByName(pProperty->GetPropertyName()) == nullptr)
        continue;
      ezVariant value;
      EZ_SUCCEED_OR_RETURN(accessor.GetValueByName(pObject, pProperty->GetPropertyName(), value));
      values.Insert(pProperty->GetPropertyName(), value);
    }
    const auto* pParent = pObject->GetParent();
    const ezString parentProperty = pObject->GetParentProperty();
    const ezVariant index = pObject->GetPropertyIndex();
    EZ_SUCCEED_OR_RETURN(accessor.RemoveObject(pObject));
    ezUuid id = guid;
    EZ_SUCCEED_OR_RETURN(accessor.AddObjectByName(pParent, parentProperty, index, pTargetType, id));
    const auto* pReplacement = accessor.GetObject(id);
    for (auto it = values.GetIterator(); it.IsValid(); ++it)
      EZ_SUCCEED_OR_RETURN(accessor.SetValueByName(pReplacement, it.Key(), it.Value()));
    return accessor.SetValueByName(pReplacement, "Shape", iShape);
  }
} // namespace

ezStatus ezSetGreyBoxShape(ezObjectAccessorBase& accessor, ezArrayPtr<const ezUuid> components, ezInt64 iShape)
{
  if (components.IsEmpty() || iShape < 0 || iShape > ezGreyBoxConeShape::Cone)
    return ezStatus("Invalid Grey Boxing shape or selection.");
  if (accessor.GetObjectManager()->GetDocument()->IsReadOnly())
    return ezStatus("The document is read-only.");
  accessor.StartTransaction("Change Grey Boxing Shape");
  for (const auto& guid : components)
  {
    auto change = [&]() -> ezStatus
    {
      const auto* original = accessor.GetObject(guid);
      const bool changed = ezIsGreyBoxComponent(original) && accessor.GetByName<ezInt64>(original, "Shape") != iShape;
      EZ_SUCCEED_OR_RETURN(SetShape(accessor, guid, iShape));
      const auto* object = accessor.GetObject(guid);
      if (changed && object->GetType()->GetTypeName() == "ezGreyBoxConeComponent")
      {
        ezGreyBoxConeSettings settings;
        EZ_SUCCEED_OR_RETURN(ezReadGreyBoxConeSettings(accessor, object, settings));
        // Both document properties must change in the same transaction. Runtime setters
        // must not silently change another reflected property behind the editor's back.
        EZ_SUCCEED_OR_RETURN(accessor.SetValueByName(object, "SmoothShading", settings.GetDefaultSmoothShading()));
      }
      return ezStatus(EZ_SUCCESS);
    };
    const ezStatus status = change();
    if (status.Failed())
    {
      accessor.CancelTransaction();
      return status;
    }
  }
  accessor.FinishTransaction();
  return ezStatus(EZ_SUCCESS);
}

ezStatus ezSetGreyBoxSmoothShading(ezObjectAccessorBase& accessor, ezArrayPtr<const ezUuid> components, bool bSmooth)
{
  if (components.IsEmpty() || accessor.GetObjectManager()->GetDocument()->IsReadOnly())
    return ezStatus("Select editable Grey Boxing components.");
  accessor.StartTransaction("Change Grey Boxing Smooth Shading");
  for (const auto& guid : components)
  {
    auto change = [&]() -> ezStatus
    {
      const auto* pObject = accessor.GetObject(guid);
      if (!ezIsGreyBoxComponent(pObject))
        return ezStatus("Select only Grey Boxing components.");
      if (pObject->GetType()->GetTypeName() == "ezGreyBoxComponent")
      {
        const ezInt64 shape = accessor.GetByName<ezInt64>(pObject, "Shape");
        EZ_SUCCEED_OR_RETURN(SetShape(accessor, guid, shape, true));
      }
      return accessor.SetValueByName(accessor.GetObject(guid), "SmoothShading", bSmooth);
    };
    const ezStatus status = change();
    if (status.Failed())
    {
      accessor.CancelTransaction();
      return status;
    }
  }
  accessor.FinishTransaction();
  return ezStatus(EZ_SUCCESS);
}

bool ezGetGreyBoxSmoothShading(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject)
{
  if (!ezIsGreyBoxComponent(pObject))
    return false;
  if (pObject->GetType()->GetTypeName() == "ezGreyBoxConeComponent")
    return accessor.GetByName<bool>(pObject, "SmoothShading");
  ezGreyBoxConeSettings settings;
  settings.m_uiShape = accessor.GetByName<ezUInt8>(pObject, "Shape");
  settings.m_vNegative = ezVec3(accessor.GetByName<float>(pObject, "SizeNegX"), accessor.GetByName<float>(pObject, "SizeNegY"), accessor.GetByName<float>(pObject, "SizeNegZ"));
  settings.m_vPositive = ezVec3(accessor.GetByName<float>(pObject, "SizePosX"), accessor.GetByName<float>(pObject, "SizePosY"), accessor.GetByName<float>(pObject, "SizePosZ"));
  settings.m_uiDetail = accessor.GetByName<ezUInt32>(pObject, "Detail");
  settings.m_Curvature = accessor.GetByName<ezAngle>(pObject, "Curvature");
  settings.m_fThickness = accessor.GetByName<float>(pObject, "Thickness");
  settings.m_bSlopedTop = accessor.GetByName<bool>(pObject, "SlopedTop");
  settings.m_bSlopedBottom = accessor.GetByName<bool>(pObject, "SlopedBottom");
  return settings.GetDefaultSmoothShading();
}

