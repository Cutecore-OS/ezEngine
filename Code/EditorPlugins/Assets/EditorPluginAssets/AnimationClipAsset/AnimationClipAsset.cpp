#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorFramework/Assets/AssetBrowserDlg.moc.h>
#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorPluginAssets/AnimationClipAsset/AnimationClipAsset.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <Foundation/Utilities/AssetInfoFile.h>
#include <Foundation/Utilities/Progress.h>
#include <GuiFoundation/PropertyGrid/PropertyMetaState.h>
#include <GuiFoundation/UIServices/DynamicStringEnum.h>
#include <ModelImporter2/ModelImporter.h>
#include <RendererCore/AnimationSystem/AnimationClipResource.h>
#include <RendererCore/AnimationSystem/EditableSkeleton.h>
#include <ToolsFoundation/Object/ObjectCommandAccessor.h>

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_STATIC_REFLECTED_ENUM(ezRootMotionSource, 1)
  EZ_ENUM_CONSTANTS(ezRootMotionSource::None, ezRootMotionSource::Constant)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationClipCurveData, 2, ezRTTIDefaultAllocator<ezAnimationClipCurveData>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Name", m_sName)->AddAttributes(new ezDynamicStringEnumAttribute("CustomAnimCurveNames")),
    EZ_MEMBER_PROPERTY("OverrideSource", m_bOverrideSource),
    EZ_MEMBER_PROPERTY("Curve", m_Curve)->AddAttributes(new ezHiddenAttribute()),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_STATIC_REFLECTED_ENUM(ezAdditiveAnimationReference, 1)
  EZ_ENUM_CONSTANTS(ezAdditiveAnimationReference::FirstKeyFrame, ezAdditiveAnimationReference::LastKeyFrame)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationClipAssetProperties, 5, ezRTTIDefaultAllocator<ezAnimationClipAssetProperties>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("File", m_sSourceFile)->AddAttributes(new ezFileBrowserAttribute("Select Animation", ezFileBrowserAttribute::MeshesWithAnimations), new ezRequiredAttribute()),
    EZ_MEMBER_PROPERTY("PreviewMesh", m_sPreviewMesh)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_Skinned", ezDependencyFlags::Transform | ezDependencyFlags::Thumbnail)),
    // \see ezAnimationClipAssetDocument::OnRefreshDynamicStringEnum()
    EZ_MEMBER_PROPERTY("UseAnimationClip", m_sAnimationClipToExtract)->AddAttributes(new ezDynamicStringEnumAttribute("AnimationClipsInSourceFile")),
    EZ_MEMBER_PROPERTY("FirstFrame", m_uiFirstFrame),
    EZ_MEMBER_PROPERTY("NumFrames", m_uiNumFrames),
    EZ_MEMBER_PROPERTY("Additive", m_bAdditive),
    EZ_MEMBER_PROPERTY("BasePreviewAnim", m_sPreviewAnim)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Keyframe_Animation", ezDependencyFlags::Thumbnail)),
    EZ_ENUM_MEMBER_PROPERTY("AdditiveReference", ezAdditiveAnimationReference, m_AdditiveReference),
    EZ_ENUM_MEMBER_PROPERTY("RootMotion", ezRootMotionSource, m_RootMotionMode),
    EZ_MEMBER_PROPERTY("ConstantRootMotion", m_vConstantRootMotion),
    EZ_MEMBER_PROPERTY("RootMotionDistance", m_fConstantRootMotionLength),
    EZ_MEMBER_PROPERTY("AdjustScale", m_fAnimationPositionScale)->AddAttributes(new ezDefaultValueAttribute(1.0f), new ezClampValueAttribute(0.0001f, 10000.0f), new ezGroupAttribute("Adjustments")),
    EZ_ARRAY_MEMBER_PROPERTY("Curves", m_Curves),
    EZ_MEMBER_PROPERTY("ImportBlendShapes", m_bImportBlendShapes)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_ARRAY_MEMBER_PROPERTY("BlendShapes", m_BlendShapes)->AddAttributes(new ezContainerAttribute(false, false, false)),
    EZ_MEMBER_PROPERTY("EventTrack", m_EventTrack)->AddAttributes(new ezHiddenAttribute()),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationClipAssetDocument, 8, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezAnimationClipAssetProperties::ezAnimationClipAssetProperties() = default;
ezAnimationClipAssetProperties::~ezAnimationClipAssetProperties() = default;

void ezAnimationClipAssetDocument::OnRefreshDynamicStringEnum(ezDynamicStringEnum::RefreshValuesEvent& e)
{
  if (e.m_sEnumName != "AnimationClipsInSourceFile"_ezsv)
    return;

  e.m_pEnum->Clear();

  if (e.m_pDocument == nullptr)
    return;

  const ezAssetCurator::ezLockedSubAsset asset = ezAssetCurator::GetSingleton()->GetSubAsset(e.m_pDocument->GetGuid());

  if (!asset.isValid())
    return;

  const ezAssetInfoFile* pInfo = asset->m_pAssetInfo->GetTransformInfo();

  if (pInfo == nullptr)
    return;

  const ezVariant clips = pInfo->GetValue(ezAssetInfoFile::Keys::AvailableClips);

  if (!clips.IsA<ezVariantArray>())
    return;

  for (const ezVariant& clip : clips.Get<ezVariantArray>())
  {
    e.m_pEnum->AddValidValue(clip.ConvertTo<ezString>());
  }
}

void ezAnimationClipAssetProperties::PropertyMetaStateEventHandler(ezPropertyMetaStateEvent& e)
{
  if (e.m_pObject->GetTypeAccessor().GetType() == ezGetStaticRTTI<ezAnimationClipCurveData>())
  {
    if (e.m_pObject->GetParentProperty() == "BlendShapes")
      (*e.m_pPropertyStates)["Name"].m_Visibility = ezPropertyUiState::Disabled;
    (*e.m_pPropertyStates)["OverrideSource"].m_Visibility = e.m_pObject->GetParentProperty() == "BlendShapes" ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
    return;
  }

  if (e.m_pObject->GetTypeAccessor().GetType() != ezGetStaticRTTI<ezAnimationClipAssetProperties>())
    return;

  auto& props = *e.m_pPropertyStates;

  const bool bAdditive = e.m_pObject->GetTypeAccessor().GetValue("Additive").ConvertTo<bool>();
  props["AdditiveReference"].m_Visibility = bAdditive ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
  props["BasePreviewAnim"].m_Visibility = bAdditive ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;

  const ezInt64 motionType = e.m_pObject->GetTypeAccessor().GetValue("RootMotion").ConvertTo<ezInt64>();
  props["ConstantRootMotion"].m_Visibility = ezPropertyUiState::Invisible;
  props["RootMotionDistance"].m_Visibility = ezPropertyUiState::Invisible;

  switch (motionType)
  {
    case ezRootMotionSource::Constant:
      props["ConstantRootMotion"].m_Visibility = ezPropertyUiState::Default;
      props["RootMotionDistance"].m_Visibility = ezPropertyUiState::Default;
      break;

    default:
      break;
  }
}

ezAnimationClipAssetDocument::ezAnimationClipAssetDocument(ezStringView sDocumentPath)
  : ezSimpleAssetDocument<ezAnimationClipAssetProperties>(sDocumentPath, ezAssetDocEngineConnection::Simple, true)
{
}

void ezAnimationClipAssetDocument::SetCommonAssetUiState(ezCommonAssetUiState::Enum state, double value)
{
  switch (state)
  {
    case ezCommonAssetUiState::SimulationSpeed:
      m_fSimulationSpeed = value;
      break;
    default:
      break;
  }

  // handles standard booleans and broadcasts the event
  return SUPER::SetCommonAssetUiState(state, value);
}

double ezAnimationClipAssetDocument::GetCommonAssetUiState(ezCommonAssetUiState::Enum state) const
{
  switch (state)
  {
    case ezCommonAssetUiState::SimulationSpeed:
      return m_fSimulationSpeed;
    default:
      break;
  }

  return SUPER::GetCommonAssetUiState(state);
}

ezTransformStatus ezAnimationClipAssetDocument::InternalTransformAsset(ezStreamWriter& stream, ezStringView sOutputTag, const ezPlatformProfile* pAssetProfile, const ezAssetFileHeader& AssetHeader, ezBitflags<ezTransformFlags> transformFlags)
{
  ezProgressRange range("Transforming Asset", 2, false);

  ezAnimationClipAssetProperties* pProp = GetProperties();

  ezAnimationClipResourceDescriptor desc;

  range.BeginNextStep("Importing Animations");

  ezStringBuilder sAbsFilename = pProp->m_sSourceFile;
  if (!ezQtEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(sAbsFilename))
  {
    return ezStatus(ezFmt("Could not make path absolute: '{0};", sAbsFilename));
  }

  ezUniquePtr<ezModelImporter2::Importer> pImporter = ezModelImporter2::RequestImporterForFileType(sAbsFilename);
  if (pImporter == nullptr)
    return ezStatus("No known importer for this file type.");

  ezEditableSkeleton skeleton;

  ezModelImporter2::ImportOptions opt;
  opt.m_sSourceFile = sAbsFilename;
  // opt.m_pSkeletonOutput = &skeleton; // TODO: may be needed later to optimize the clip
  opt.m_pAnimationOutput = &desc;
  opt.m_bAdditiveAnimation = pProp->m_bAdditive;
  opt.m_AdditiveReference = (pProp->m_AdditiveReference == ezAdditiveAnimationReference::FirstKeyFrame) ? ezModelImporter2::AdditiveReference::FirstKeyFrame : ezModelImporter2::AdditiveReference::LastKeyFrame;
  opt.m_sAnimationToImport = pProp->m_sAnimationClipToExtract;
  opt.m_uiFirstAnimKeyframe = pProp->m_uiFirstFrame;
  opt.m_uiNumAnimKeyframes = pProp->m_uiNumFrames;
  opt.m_fAnimationPositionScale = pProp->m_fAnimationPositionScale;

  const ezResult res = pImporter->Import(opt);

  if (res.Succeeded())
  {
    if (pProp->m_RootMotionMode == ezRootMotionSource::Constant)
    {
      desc.m_vConstantRootMotion = pProp->m_vConstantRootMotion;

      if (pProp->m_fConstantRootMotionLength > 0.0f)
      {
        desc.m_vConstantRootMotion.SetLength(pProp->m_fConstantRootMotionLength, 0.01f).IgnoreResult();
      }
    }

    // copy named custom curves
    desc.m_CustomCurves.SetCount(pProp->m_Curves.GetCount());
    for (ezUInt32 i = 0; i < pProp->m_Curves.GetCount(); ++i)
    {
      desc.m_CustomCurves[i].m_sName.Assign(pProp->m_Curves[i].m_sName);
      pProp->m_Curves[i].m_Curve.ConvertToRuntimeData(desc.m_CustomCurves[i].m_Curve);
      desc.m_CustomCurves[i].m_Curve.SortControlPoints();
      desc.m_CustomCurves[i].m_Curve.CreateLinearApproximation();
    }

    {
      EZ_SUCCEED_OR_RETURN(ezTransformBlendShapeCurves(*this, sAbsFilename, desc, transformFlags.IsSet(ezTransformFlags::BackgroundProcessing)));
      pProp = GetProperties();
    }

    range.BeginNextStep("Writing Result");

    pProp->m_EventTrack.ConvertToRuntimeData(desc.m_EventTrack);

    EZ_SUCCEED_OR_RETURN(desc.Serialize(stream));
  }

  // Fills the drop down of the 'UseAnimationClip' property, so that a clip can be picked without
  // opening the source file again.
  if (!pImporter->m_OutputAnimationNames.IsEmpty())
  {
    ezVariantArray clipNames;
    clipNames.Reserve(pImporter->m_OutputAnimationNames.GetCount());

    for (const auto& sName : pImporter->m_OutputAnimationNames)
    {
      clipNames.PushBack(ezVariant(sName));
    }

    GetTransformInfo().SetValue(ezAssetInfoFile::Keys::AvailableClips, ezVariant(clipNames));
  }

  if (res.Failed())
    return ezStatus("Model importer was unable to read this asset.");

  return ezStatus(EZ_SUCCESS);
}

ezTransformStatus ezAnimationClipAssetDocument::InternalCreateThumbnail(const ThumbnailInfo& ThumbnailInfo)
{
  // the preview mesh is an editor side only option, so the thumbnail context doesn't know anything about this
  // until we explicitly tell it about the mesh
  // without sending this here, thumbnails would remain black for assets transformed in the background
  if (!GetProperties()->m_sPreviewMesh.IsEmpty())
  {
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "PreviewMesh";
    msg.m_sPayload = GetProperties()->m_sPreviewMesh;
    SendMessageToEngine(&msg);
  }
  if (!GetProperties()->m_sPreviewAnim.IsEmpty())
  {
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "PreviewAnim";
    msg.m_sPayload = GetProperties()->m_sPreviewAnim;
    SendMessageToEngine(&msg);
  }

  ezStatus status = ezAssetDocument::RemoteCreateThumbnail(ThumbnailInfo);
  return status;
}

ezUuid ezAnimationClipAssetDocument::InsertEventTrackCpAt(ezInt64 iTickX, const char* szValue)
{
  ezObjectCommandAccessor accessor(GetCommandHistory());
  ezObjectAccessorBase& acc = accessor;
  acc.StartTransaction("Insert Event");

  const ezAbstractProperty* pTrackProp = ezGetStaticRTTI<ezAnimationClipAssetProperties>()->FindPropertyByName("EventTrack");
  ezUuid trackGuid = accessor.Get<ezUuid>(GetPropertyObject(), pTrackProp);

  ezUuid newObjectGuid;
  EZ_VERIFY(acc.AddObjectByName(accessor.GetObject(trackGuid), "ControlPoints", -1, ezGetStaticRTTI<ezEventTrackControlPointData>(), newObjectGuid).Succeeded(), "");
  const ezDocumentObject* pCPObj = accessor.GetObject(newObjectGuid);
  EZ_VERIFY(acc.SetValueByName(pCPObj, "Tick", iTickX).Succeeded(), "");
  EZ_VERIFY(acc.SetValueByName(pCPObj, "Event", szValue).Succeeded(), "");

  acc.FinishTransaction();

  return newObjectGuid;
}

//////////////////////////////////////////////////////////////////////////

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationClipAssetDocumentGenerator, 1, ezRTTIDefaultAllocator<ezAnimationClipAssetDocumentGenerator>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

ezAnimationClipAssetDocumentGenerator::ezAnimationClipAssetDocumentGenerator()
{
  AddSupportedFileType("fbx");
  AddSupportedFileType("gltf");
  AddSupportedFileType("glb");
}

ezAnimationClipAssetDocumentGenerator::~ezAnimationClipAssetDocumentGenerator() = default;

void ezAnimationClipAssetDocumentGenerator::GetImportModes(ezStringView sAbsInputFile, ezDynamicArray<ezAssetDocumentGenerator::ImportMode>& out_modes) const
{
  {
    ezAssetDocumentGenerator::ImportMode& info = out_modes.ExpandAndGetRef();
    info.m_Priority = ezAssetDocGeneratorPriority::Undecided;
    info.m_sName = "AnimationClipImport_Single";
    info.m_sIcon = ":/AssetIcons/Animation_Clip.svg";
  }

  {
    ezAssetDocumentGenerator::ImportMode& info = out_modes.ExpandAndGetRef();
    info.m_Priority = ezAssetDocGeneratorPriority::Undecided;
    info.m_sName = "AnimationClipImport_All";
    info.m_sIcon = ":/AssetIcons/Animation_Clip.svg";
  }
}

bool ezAnimationClipAssetDocumentGenerator::NeedsImport(ezStringView sInputFileAbs, ezStringView sMode) const
{
  // In this mode the clip documents are named after the animations inside the file, which are only
  // known after parsing it. Always import, the loop over the clips skips the ones that exist.
  if (sMode == "AnimationClipImport_All")
    return true;

  return SUPER::NeedsImport(sInputFileAbs, sMode);
}

ezStatus ezAnimationClipAssetDocumentGenerator::Generate(ezStringView sInputFileAbs, ezStringView sMode, ezDynamicArray<ezDocument*>& out_generatedDocuments)
{
  const ezStringBuilder sOutFile = GetImportTargetPath(sInputFileAbs);

  auto pApp = ezQtEditorApp::GetSingleton();

  ezStringBuilder sInputFileRel = sInputFileAbs;
  pApp->MakePathDataDirectoryRelative(sInputFileRel);

  ezStringBuilder title;
  title.SetFormat("Select Preview Mesh for Animation Clip '{}'", sInputFileAbs.GetFileName());

  ezStringBuilder sPreviewMesh;

  // The preview mesh is only used for previewing the clip in the editor, so leaving it empty is fine.
  // Without a user there is nobody to close this dialog, which would block the editor indefinitely.
  if (!pApp->IsInUnattendedMode())
  {
    ezQtAssetBrowserDlg dlg(nullptr, ezUuid::MakeInvalid(), "CompatibleAsset_Mesh_Skinned", title);
    if (dlg.exec() != 0)
    {
      if (dlg.GetSelectedAssetGuid().IsValid())
      {
        ezConversionUtils::ToString(dlg.GetSelectedAssetGuid(), sPreviewMesh);
      }
    }
  }

  if (sMode == "AnimationClipImport_Single")
  {
    ezDocument* pDoc = pApp->CreateDocument(sOutFile, ezDocumentFlags::None);
    if (pDoc == nullptr)
      return ezStatus("Could not create target document");

    out_generatedDocuments.PushBack(pDoc);

    ezAnimationClipAssetDocument* pAssetDoc = ezDynamicCast<ezAnimationClipAssetDocument*>(pDoc);

    auto& accessor = pAssetDoc->GetPropertyObject()->GetTypeAccessor();
    accessor.SetValue("File", sInputFileRel.GetView());
    accessor.SetValue("PreviewMesh", sPreviewMesh.GetView());

    ezLog::Success("Imported animation clip: '{}'", sOutFile);

    return ezStatus(EZ_SUCCESS);
  }

  if (sMode == "AnimationClipImport_All")
  {
    ezModelImporter2::ImportOptions opt;
    opt.m_sSourceFile = sInputFileAbs;

    ezUniquePtr<ezModelImporter2::Importer> pImporter = ezModelImporter2::RequestImporterForFileType(opt.m_sSourceFile);
    if (pImporter == nullptr)
      return ezStatus("No known importer for this file type.");

    if (pImporter->Import(opt).Failed())
      return ezStatus("Failed to import asset.");

    ezStringBuilder sFilename;
    ezStringBuilder sOutFile2;

    for (const auto& clip : pImporter->m_OutputAnimationNames)
    {
      ezPathUtils::MakeValidFilename(clip, '-', sFilename);
      sFilename.ReplaceAll(" ", "-");
      sFilename.Prepend(sOutFile.GetFileName(), "_");

      sOutFile2 = sOutFile;
      sOutFile2.ChangeFileName(sFilename);

      if (ezOSFile::ExistsFile(sOutFile2))
      {
        ezLog::Info("Skipping animation clip import, file has been imported before: '{}'", sOutFile2);
        continue;
      }

      ezDocument* pDoc = pApp->CreateDocument(sOutFile2, ezDocumentFlags::None);
      if (pDoc == nullptr)
        return ezStatus("Could not create target document");

      out_generatedDocuments.PushBack(pDoc);

      ezAnimationClipAssetDocument* pAssetDoc = ezDynamicCast<ezAnimationClipAssetDocument*>(pDoc);

      auto& accessor = pAssetDoc->GetPropertyObject()->GetTypeAccessor();
      accessor.SetValue("File", sInputFileRel.GetView());
      accessor.SetValue("UseAnimationClip", clip);
      accessor.SetValue("PreviewMesh", sPreviewMesh.GetView());

      ezLog::Success("Imported animation clip: '{}'", sOutFile2);
    }

    return ezStatus(EZ_SUCCESS);
  }

  EZ_ASSERT_NOT_IMPLEMENTED;
  return ezStatus(EZ_FAILURE);
}

ezStatus ezAnimationClipAssetDocument::RefreshBlendShapeCurves()
{
  const auto* properties = GetProperties();
  ezAnimationClipResourceDescriptor clip;
  clip.SetDuration(ezTime::MakeFromSeconds(1));
  ezStringBuilder source = properties->m_sSourceFile;
  if (!source.IsEmpty())
  {
    if (!source.IsAbsolutePath() && !ezQtEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(source))
      return ezStatus("Cannot resolve the animation source.");
    auto importer = ezModelImporter2::RequestImporterForFileType(source);
    if (importer == nullptr)
      return ezStatus("No importer for the animation source.");
    ezModelImporter2::ImportOptions options;
    options.m_sSourceFile = source;
    options.m_pAnimationOutput = &clip;
    options.m_sAnimationToImport = properties->m_sAnimationClipToExtract;
    options.m_uiFirstAnimKeyframe = properties->m_uiFirstFrame;
    options.m_uiNumAnimKeyframes = properties->m_uiNumFrames;
    if (importer->Import(options).Failed())
    {
      // A model may have shapes but no animation yet. Its Preview Mesh must still
      // expose those names while the user configures the clip's animation source.
      if (properties->m_sPreviewMesh.IsEmpty())
        return ezStatus("Could not read the animation source.");
      source.Clear();
      double duration = 1.0;
      for (const auto& curve : properties->m_BlendShapes)
        for (const auto& point : curve.m_Curve.m_ControlPoints)
          duration = ezMath::Max(duration, point.GetTickAsTime().GetSeconds());
      clip.SetDuration(ezTime::MakeFromSeconds(duration));
    }
  }
  const auto status = ezTransformBlendShapeCurves(*this, source, clip, false);
  return status.Succeeded() ? ezStatus(EZ_SUCCESS) : ezStatus(status.m_sMessage.GetView());
}

void ezAnimationClipAssetDocument::SetBlendShapeCurves(const ezDynamicArray<ezAnimationClipCurveData>& curves)
{
  auto* accessor = GetObjectAccessor();
  accessor->StartTransaction("Update imported blend shape curves");
  auto resize = [&](const ezDocumentObject* parent, const char* property, ezUInt32 count, const ezRTTI* type)
  {
    while (parent->GetTypeAccessor().GetCount(property) > static_cast<ezInt32>(count))
    {
      const auto guid = parent->GetTypeAccessor().GetValue(property, parent->GetTypeAccessor().GetCount(property) - 1).Get<ezUuid>();
      accessor->RemoveObject(accessor->GetObject(guid)).AssertSuccess();
    }
    while (parent->GetTypeAccessor().GetCount(property) < static_cast<ezInt32>(count))
    {
      ezUuid guid;
      accessor->AddObjectByName(parent, property, -1, type, guid).AssertSuccess();
    }
  };
  resize(GetPropertyObject(), "BlendShapes", curves.GetCount(), ezGetStaticRTTI<ezAnimationClipCurveData>());
  for (ezUInt32 i = 0; i < curves.GetCount(); ++i)
  {
    const auto* namedCurve = accessor->GetObject(GetPropertyObject()->GetTypeAccessor().GetValue("BlendShapes", i).Get<ezUuid>());
    accessor->SetValueByName(namedCurve, "Name", curves[i].m_sName).AssertSuccess();
    accessor->SetValueByName(namedCurve, "OverrideSource", curves[i].m_bOverrideSource).AssertSuccess();
    const auto* curve = accessor->GetObject(namedCurve->GetTypeAccessor().GetValue("Curve").Get<ezUuid>());
    const auto& keys = curves[i].m_Curve.m_ControlPoints;
    resize(curve, "ControlPoints", keys.GetCount(), ezGetStaticRTTI<ezCurveControlPointData>());
    for (ezUInt32 k = 0; k < keys.GetCount(); ++k)
    {
      const auto* point = accessor->GetObject(curve->GetTypeAccessor().GetValue("ControlPoints", k).Get<ezUuid>());
      accessor->SetValueByName(point, "Tick", keys[k].m_iTick).AssertSuccess();
      accessor->SetValueByName(point, "Value", keys[k].m_fValue).AssertSuccess();
      accessor->SetValueByName(point, "LeftTangent", keys[k].m_LeftTangent).AssertSuccess();
      accessor->SetValueByName(point, "RightTangent", keys[k].m_RightTangent).AssertSuccess();
      accessor->SetValueByName(point, "LeftTangentMode", keys[k].m_LeftTangentMode.GetValue()).AssertSuccess();
      accessor->SetValueByName(point, "RightTangentMode", keys[k].m_RightTangentMode.GetValue()).AssertSuccess();
      accessor->SetValueByName(point, "Linked", keys[k].m_bTangentsLinked).AssertSuccess();
    }
  }
  accessor->FinishTransaction();
}
