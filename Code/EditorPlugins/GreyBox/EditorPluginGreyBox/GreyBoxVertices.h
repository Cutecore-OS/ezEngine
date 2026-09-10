#pragma once

#include <EditorPluginGreyBox/EditorPluginGreyBoxDLL.h>
#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Math/Vec3.h>
#include <Foundation/Types/Status.h>

class ezDocumentObject;
class ezObjectAccessorBase;

/// Uses the component's full-detail geometry builder, including all stair treads and curved shapes.
EZ_EDITORPLUGINGREYBOX_DLL ezStatus ezBuildGreyBoxVertices(ezObjectAccessorBase& accessor,
  const ezDocumentObject* pComponent, ezDynamicArray<ezVec3>& out_vertices);

/// Translate the owning object so that its local vertex reaches vWorldTarget. One undo transaction.
EZ_EDITORPLUGINGREYBOX_DLL ezStatus ezSnapGreyBoxVertex(ezObjectAccessorBase& accessor,
  const ezDocumentObject* pComponent, const ezVec3& vLocalSource, const ezVec3& vWorldTarget);
