#pragma once
#include <EditorPluginAssets/EditorPluginAssetsDLL.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <Foundation/Types/Status.h>
#include <ModelImporter2/Importer/Importer.h>
#include <RendererCore/AnimationSystem/AnimationClipResource.h>

/// Uses the same importer/options/remap as the Animated Mesh asset.
EZ_EDITORPLUGINASSETS_DLL ezStatus ezImportBlendShapeMesh(const ezModelImporter2::ImportOptions& options, ezBlendShapeResourceDescriptor& out_desc);
EZ_EDITORPLUGINASSETS_DLL ezStatus ezImportBlendShapeMesh(ezStringView sFile, bool bImportShapes, ezBlendShapeResourceDescriptor& out_desc);
EZ_EDITORPLUGINASSETS_DLL ezStatus ezReadBlendShapeGltfCurves(ezStringView sFile, ezStringView sAnimation, ezAnimationClipResourceDescriptor& inout_desc);
EZ_EDITORPLUGINASSETS_DLL ezStatus ezReadBlendShapeGltfNames(ezStringView sFile, ezDynamicArray<ezBlendShapeTarget>& out_targets, const ezModelImporter2::ImportOptions* pOptions = nullptr);
