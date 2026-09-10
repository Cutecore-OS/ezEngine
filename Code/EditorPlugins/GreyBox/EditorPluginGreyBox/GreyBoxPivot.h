#pragma once

#include <EditorPluginGreyBox/EditorPluginGreyBoxDLL.h>
#include <Foundation/Math/Vec3.h>
#include <Foundation/Types/ArrayPtr.h>
#include <Foundation/Types/Status.h>

class ezDocumentObject;
class ezObjectAccessorBase;

/// Editor-only operation. vAnchor selects a point in the bounds: 0 = minimum, 0.5 = center, 1 = maximum.
/// All changes, including compensation for children and sibling grey boxes, form one undo transaction.
EZ_EDITORPLUGINGREYBOX_DLL ezStatus ezRecalculateGreyBoxPivot(ezObjectAccessorBase& accessor,
  ezArrayPtr<const ezDocumentObject* const> components, const ezVec3& vAnchor);
