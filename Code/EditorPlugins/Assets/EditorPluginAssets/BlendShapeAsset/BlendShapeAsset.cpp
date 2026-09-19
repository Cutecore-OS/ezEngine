#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorFramework/GUI/ExposedParameters.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <Foundation/Utilities/Progress.h>
#include <ModelImporter2/ModelImporter.h>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>
#include <RendererCore/Meshes/MeshResourceDescriptor.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeAssetDocument, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezBlendShapeAssetDocument::ezBlendShapeAssetDocument(ezStringView sDocumentPath)
  : ezSimpleAssetDocument<ezEditableBlendShapes>(sDocumentPath, ezAssetDocEngineConnection::Simple, true)
{
}

ezBlendShapeAssetDocument::~ezBlendShapeAssetDocument() = default;

void ezBlendShapeAssetDocument::PropertyMetaStateEventHandler(ezPropertyMetaStateEvent& e)
{
}

ezStatus ezBlendShapeAssetDocument::WriteResource(ezStreamWriter& inout_stream, const ezEditableBlendShapes& blendShapes, ezUInt16* out_pNumChannels) const
{
  ezBlendShapeResourceDescriptor desc;
  blendShapes.FillResourceDescriptor(desc);

  if (out_pNumChannels != nullptr)
  {
    *out_pNumChannels = static_cast<ezUInt16>(desc.m_Channels.GetCount());
  }

  EZ_SUCCEED_OR_RETURN(desc.Serialize(inout_stream));

  return ezStatus(EZ_SUCCESS);
}

void ezBlendShapeAssetDocument::UpdateAssetDocumentInfo(ezAssetDocumentInfo* pInfo) const
{
  SUPER::UpdateAssetDocumentInfo(pInfo);

  auto pProp = GetProperties();

  // Expose blend shape names so components can dynamically reflect them
  ezExposedParameters* pExposedParams = EZ_DEFAULT_NEW(ezExposedParameters);
  for (const auto& ch : pProp->m_Channels)
  {
    ezExposedParameter* param = EZ_DEFAULT_NEW(ezExposedParameter);
    param->m_sName = ch.m_sName;
    param->m_DefaultValue = ch.m_fDefaultWeight;
    pExposedParams->m_Parameters.PushBack(param);
  }

  pInfo->m_MetaInfo.PushBack(pExposedParams);
}

ezTransformStatus ezBlendShapeAssetDocument::InternalTransformAsset(ezStreamWriter& stream, ezStringView sOutputTag, const ezPlatformProfile* pAssetProfile, const ezAssetFileHeader& AssetHeader, ezBitflags<ezTransformFlags> transformFlags)
{
  m_bIsTransforming = true;
  EZ_SCOPE_EXIT(m_bIsTransforming = false);

  ezProgressRange range("Transforming Asset", 2, false);

  ezEditableBlendShapes* pProp = GetProperties();

  ezStringBuilder sAbsFilename = pProp->m_sSourceFile;

  if (sAbsFilename.IsEmpty())
  {
    range.BeginNextStep("Writing Result");
    EZ_SUCCEED_OR_RETURN(WriteResource(stream, *pProp));
  }
  else
  {
    if (!ezQtEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(sAbsFilename))
    {
      return ezStatus(ezFmt("Couldn't make path absolute: '{0}'", sAbsFilename));
    }

    ezUniquePtr<ezModelImporter2::Importer> pImporter = ezModelImporter2::RequestImporterForFileType(sAbsFilename);
    if (pImporter == nullptr)
      return ezStatus("No known importer for this file type.");

    range.BeginNextStep("Importing Source File");

    ezEditableBlendShapes newBlendShapes;

    ezMeshResourceDescriptor baseMeshDesc;

    ezModelImporter2::ImportOptions opt;
    opt.m_sSourceFile = sAbsFilename;
    opt.m_pMeshOutput = &baseMeshDesc;
    opt.m_pBlendShapesOutput = &newBlendShapes;

    EZ_SUCCEED_OR_RETURN(pImporter->Import(opt));

    pProp->m_MeshBufferDesc = std::move(newBlendShapes.m_MeshBufferDesc);
    pProp->m_Channels = std::move(newBlendShapes.m_Channels);

    range.BeginNextStep("Writing Result");
    EZ_SUCCEED_OR_RETURN(WriteResource(stream, *pProp));
  }

  return ezStatus(EZ_SUCCESS);
}

ezTransformStatus ezBlendShapeAssetDocument::InternalCreateThumbnail(const ThumbnailInfo& ThumbnailInfo)
{
  if (!GetProperties()->m_sPreviewMesh.IsEmpty())
  {
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "PreviewMesh";
    msg.m_sPayload = GetProperties()->m_sPreviewMesh;
    SendMessageToEngine(&msg);
  }

  return ezAssetDocument::RemoteCreateThumbnail(ThumbnailInfo);
}
