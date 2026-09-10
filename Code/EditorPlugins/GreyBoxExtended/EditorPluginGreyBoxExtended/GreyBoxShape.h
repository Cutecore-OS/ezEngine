#pragma once

#include <EditorPluginGreyBoxExtended/EditorPluginGreyBoxExtendedDLL.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <Foundation/Types/ArrayPtr.h>
#include <Foundation/Types/Status.h>
#include <Foundation/Types/Uuid.h>

class ezDocumentObject;
class ezObjectAccessorBase;
struct ezGreyBoxConeSettings;

EZ_EDITORPLUGINGREYBOXEXTENDED_DLL ezStatus ezReadGreyBoxConeSettings(ezObjectAccessorBase& accessor,
  const ezDocumentObject* pObject, ezGreyBoxConeSettings& out_settings);

/// Switches between native shapes and the plugin cone, preserving identity and common properties.
/// The complete selection is changed in one undo transaction.
EZ_EDITORPLUGINGREYBOXEXTENDED_DLL ezStatus ezSetGreyBoxShape(ezObjectAccessorBase& accessor,
  ezArrayPtr<const ezUuid> components, ezInt64 iShape);

/// Native shading stays untouched until overridden; conversion and normals are one undo transaction.
EZ_EDITORPLUGINGREYBOXEXTENDED_DLL ezStatus ezSetGreyBoxSmoothShading(ezObjectAccessorBase& accessor,
  ezArrayPtr<const ezUuid> components, bool bSmooth);

EZ_EDITORPLUGINGREYBOXEXTENDED_DLL bool ezGetGreyBoxSmoothShading(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject);

