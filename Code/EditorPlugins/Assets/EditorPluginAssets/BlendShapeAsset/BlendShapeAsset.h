#pragma once
#include <EditorFramework/Assets/AssetDocumentManager.h>
#include <EditorFramework/Assets/SimpleAssetDocument.h>
#include <EditorPluginAssets/AnimatedMeshAsset/AnimatedMeshAsset.h>
#include <EditorPluginAssets/AnimationClipAsset/AnimationClipAsset.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeImport.h>

class EZ_EDITORPLUGINASSETS_DLL ezBlendShapeAssetProperties : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeAssetProperties, ezReflectedClass);

public:
  ezString m_sMesh;
};

/// Companion of an ordinary Animated Mesh asset. Import settings are taken from that mesh.
class EZ_EDITORPLUGINASSETS_DLL ezBlendShapeAssetDocument : public ezSimpleAssetDocument<ezBlendShapeAssetProperties>
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeAssetDocument, ezSimpleAssetDocument<ezBlendShapeAssetProperties>);

public:
  ezBlendShapeAssetDocument(ezStringView sPath);

protected:
  virtual void UpdateAssetDocumentInfo(ezAssetDocumentInfo* pInfo) const override;
  virtual ezTransformStatus InternalTransformAsset(ezStreamWriter& stream, ezStringView sOutputTag, const ezPlatformProfile* pProfile, const ezAssetFileHeader& header, ezBitflags<ezTransformFlags> flags) override;
};

class EZ_EDITORPLUGINASSETS_DLL ezBlendShapeAssetManager : public ezAssetDocumentManager
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeAssetManager, ezAssetDocumentManager);

public:
  ezBlendShapeAssetManager();
  ~ezBlendShapeAssetManager();

private:
  void OnDocumentEvent(const ezDocumentManager::Event& e);
  virtual void InternalCreateDocument(ezStringView sType, ezStringView sPath, bool bCreate, ezDocument*& out_pDocument, const ezDocumentObject* pContext) override;
  virtual void InternalGetSupportedDocumentTypes(ezDynamicArray<const ezDocumentTypeDescriptor*>& inout_types) const override;
  virtual bool GeneratesProfileSpecificAssets() const override { return false; }
  ezAssetDocumentTypeDescriptor m_Type;
};

void ezRegisterBlendShapeActions();
void ezUnregisterBlendShapeActions();
EZ_EDITORPLUGINASSETS_DLL ezStatus ezImportBlendShapes(ezAssetDocument& document, ezDynamicArray<ezDocument*>& documents);
EZ_EDITORPLUGINASSETS_DLL ezStatus ezPrepareBlendShapes(const ezModelImporter2::ImportOptions& options, ezMeshResourceDescriptor& mesh);
EZ_EDITORPLUGINASSETS_DLL void ezExposeBlendShapes(const ezAssetDocument& document, ezAssetDocumentInfo& info);
EZ_EDITORPLUGINASSETS_DLL ezTransformStatus ezTransformBlendShapeCurves(ezAssetDocument& document, ezStringView file, ezAnimationClipResourceDescriptor& clip, bool background);
EZ_EDITORPLUGINASSETS_DLL ezStatus ezGetMeshBlendShapeTargets(ezStringView mesh, ezDynamicArray<ezBlendShapeTarget>& targets);
EZ_EDITORPLUGINASSETS_DLL ezStatus ezMergeBlendShapeClipCurves(const ezAnimationClipAssetProperties& properties, ezStringView sFile,
  ezAnimationClipResourceDescriptor& inout_clip, ezDynamicArray<ezAnimationClipCurveData>& out_editorCurves);
