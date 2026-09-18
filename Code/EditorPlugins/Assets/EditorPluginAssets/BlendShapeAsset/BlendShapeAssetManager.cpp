#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAssetManager.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeAssetDocumentManager, 1, ezRTTIDefaultAllocator<ezBlendShapeAssetDocumentManager>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

ezBlendShapeAssetDocumentManager::ezBlendShapeAssetDocumentManager()
{
  ezDocumentManager::s_Events.AddEventHandler(ezMakeDelegate(&ezBlendShapeAssetDocumentManager::OnDocumentManagerEvent, this));

  m_DocTypeDesc.m_sDocumentTypeName = "BlendShapes";
  m_DocTypeDesc.m_sFileExtension = "ezBlendShapeAsset";
  m_DocTypeDesc.m_sIcon = ":/AssetIcons/Mesh.svg";
  m_DocTypeDesc.m_sAssetCategory = "Animation";
  m_DocTypeDesc.m_pDocumentType = ezGetStaticRTTI<ezBlendShapeAssetDocument>();
  m_DocTypeDesc.m_pManager = this;
  m_DocTypeDesc.m_CompatibleTypes.PushBack("CompatibleAsset_Mesh_BlendShapes");

  m_DocTypeDesc.m_sResourceFileExtension = "ezBinBlendShapes";
  m_DocTypeDesc.m_AssetDocumentFlags = ezAssetDocumentFlags::SupportsThumbnail | ezAssetDocumentFlags::AutoTransformOnSave;
}

ezBlendShapeAssetDocumentManager::~ezBlendShapeAssetDocumentManager()
{
  ezDocumentManager::s_Events.RemoveEventHandler(ezMakeDelegate(&ezBlendShapeAssetDocumentManager::OnDocumentManagerEvent, this));
}

void ezBlendShapeAssetDocumentManager::OnDocumentManagerEvent(const ezDocumentManager::Event& e)
{
}

void ezBlendShapeAssetDocumentManager::InternalCreateDocument(
  ezStringView sDocumentTypeName, ezStringView sPath, bool bCreateNewDocument, ezDocument*& out_pDocument, const ezDocumentObject* pOpenContext)
{
  out_pDocument = new ezBlendShapeAssetDocument(sPath);
}

void ezBlendShapeAssetDocumentManager::InternalGetSupportedDocumentTypes(ezDynamicArray<const ezDocumentTypeDescriptor*>& inout_DocumentTypes) const
{
  inout_DocumentTypes.PushBack(&m_DocTypeDesc);
}
