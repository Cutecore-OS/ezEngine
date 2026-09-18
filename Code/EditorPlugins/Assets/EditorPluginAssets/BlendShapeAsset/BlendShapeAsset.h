#pragma once

#include <EditorFramework/Assets/AssetDocumentGenerator.h>
#include <EditorFramework/Assets/SimpleAssetDocument.h>
#include <RendererCore/AnimationSystem/EditableBlendShapes.h>

struct ezPropertyMetaStateEvent;
class ezBlendShapeAssetDocument;

struct ezBlendShapeAssetEvent
{
  enum Type
  {
    RenderStateChanged,
    Transformed,
  };

  ezBlendShapeAssetDocument* m_pDocument = nullptr;
  Type m_Type;
};

class ezBlendShapeAssetDocument : public ezSimpleAssetDocument<ezEditableBlendShapes>
{
  EZ_ADD_DYNAMIC_REFLECTION(ezBlendShapeAssetDocument, ezSimpleAssetDocument<ezEditableBlendShapes>);

public:
  ezBlendShapeAssetDocument(ezStringView sDocumentPath);
  ~ezBlendShapeAssetDocument();

  static void PropertyMetaStateEventHandler(ezPropertyMetaStateEvent& e);

  ezStatus WriteResource(ezStreamWriter& inout_stream, const ezEditableBlendShapes& blendShapes, ezUInt16* out_pNumChannels = nullptr) const;

  bool m_bIsTransforming = false;

  virtual ezManipulatorSearchStrategy GetManipulatorSearchStrategy() const override
  {
    return ezManipulatorSearchStrategy::SelectedObject;
  }

  const ezEvent<const ezBlendShapeAssetEvent&>& Events() const { return m_Events; }

protected:
  virtual void UpdateAssetDocumentInfo(ezAssetDocumentInfo* pInfo) const override;
  virtual ezTransformStatus InternalTransformAsset(ezStreamWriter& stream, ezStringView sOutputTag, const ezPlatformProfile* pAssetProfile,
    const ezAssetFileHeader& AssetHeader, ezBitflags<ezTransformFlags> transformFlags) override;
  virtual ezTransformStatus InternalCreateThumbnail(const ThumbnailInfo& ThumbnailInfo) override;

  ezEvent<const ezBlendShapeAssetEvent&> m_Events;
};
