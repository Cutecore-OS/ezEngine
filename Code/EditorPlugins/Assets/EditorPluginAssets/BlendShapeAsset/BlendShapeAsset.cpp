#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <EditorFramework/GUI/ExposedParameters.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <Foundation/IO/OSFile.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeAssetProperties, 2, ezRTTIDefaultAllocator<ezBlendShapeAssetProperties>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Mesh", m_sMesh)->AddAttributes(new ezAssetBrowserAttribute("CompatibleAsset_Mesh_Skinned", ezDependencyFlags::None), new ezRequiredAttribute()),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeAssetDocument, 2, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

namespace
{
  ezStatus GetOptions(const ezAnimatedMeshAssetProperties& properties, ezModelImporter2::ImportOptions& out_options)
  {
    ezStringBuilder file(properties.m_sMeshFile);
    if (!file.IsAbsolutePath() && !ezQtEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(file))
      return ezStatus("Cannot resolve the Animated Mesh source.");
    out_options.m_sSourceFile = file;
    out_options.m_bImportSkinningData = true;
    out_options.m_bRecomputeNormals = properties.m_bRecalculateNormals;
    out_options.m_bRecomputeTangents = properties.m_bRecalculateTangents;
    out_options.m_bNormalizeWeights = properties.m_bNormalizeWeights;
    out_options.m_bHighPrecision = true;
    out_options.m_MeshVertexColorConversion = properties.m_VertexColorConversion;
    auto tags = [](ezStringView text, ezDynamicArray<ezString>& output)
    {
      ezHybridArray<ezStringView, 8> parts;
      text.Split(false, parts, ";");
      for (auto part : parts)
      {
        part.Trim();
        output.PushBack(part);
      }
    };
    tags(properties.m_sMeshIncludeTags, out_options.m_MeshIncludeTags);
    tags(properties.m_sMeshExcludeTags, out_options.m_MeshExcludeTags);
    if (properties.m_bSimplifyMesh)
      return ezStatus("Blend shapes require an authored LOD; automatic mesh simplification changes target correspondence.");
    return ezStatus(EZ_SUCCESS);
  }

  ezAnimatedMeshAssetDocument* OpenMesh(ezStringView sMesh)
  {
    if (ezConversionUtils::IsStringUuid(sMesh))
    {
      const auto guid = ezConversionUtils::ConvertStringToUuid(sMesh);
      for (auto* manager : ezDocumentManager::GetAllDocumentManagers())
        for (auto* document : manager->GetAllOpenDocuments())
          if (document->GetGuid() == guid)
            return ezDynamicCast<ezAnimatedMeshAssetDocument*>(document);
    }
    ezString path;
    {
      const auto asset = ezAssetCurator::GetSingleton()->FindSubAsset(sMesh);
      if (!asset.isValid())
        return nullptr;
      path = asset->m_pAssetInfo->m_Path.GetAbsolutePath();
    }
    return ezDynamicCast<ezAnimatedMeshAssetDocument*>(ezQtEditorApp::GetSingleton()->OpenDocument(path, ezDocumentFlags::None));
  }

  ezStatus ImportCompanion(ezAssetDocument& document, ezDynamicArray<ezDocument*>& documents)
  {
    auto& mesh = static_cast<ezAnimatedMeshAssetDocument&>(document);
    ezModelImporter2::ImportOptions options;
    EZ_SUCCEED_OR_RETURN(GetOptions(*mesh.GetProperties(), options));
    ezBlendShapeResourceDescriptor shapes;
    EZ_SUCCEED_OR_RETURN(ezImportBlendShapeMesh(options, shapes));
    if (shapes.m_Targets.IsEmpty())
      return ezStatus(EZ_SUCCESS);
    ezStringBuilder path(document.GetDocumentPath());
    path.ChangeFileExtension("ezBlendShapeAsset");
    auto* app = ezQtEditorApp::GetSingleton();
    const bool exists = ezOSFile::ExistsFile(path);
    auto* companion = ezDynamicCast<ezBlendShapeAssetDocument*>(exists ? app->OpenDocument(path, ezDocumentFlags::None) : app->CreateDocument(path, ezDocumentFlags::None));
    if (companion == nullptr)
      return ezStatus("Could not create the Blend Shapes companion asset.");
    ezStringBuilder meshGuid, shapesGuid;
    ezConversionUtils::ToString(mesh.GetGuid(), meshGuid);
    ezConversionUtils::ToString(companion->GetGuid(), shapesGuid);
    auto* accessor = companion->GetObjectAccessor();
    accessor->StartTransaction("Associate Animated Mesh");
    accessor->SetValueByName(companion->GetPropertyObject(), "Mesh", meshGuid.GetView()).AssertSuccess();
    accessor->FinishTransaction();
    accessor = mesh.GetObjectAccessor();
    accessor->StartTransaction("Assign default blend shapes");
    accessor->SetValueByName(mesh.GetPropertyObject(), "DefaultBlendShapes", shapesGuid.GetView()).AssertSuccess();
    accessor->FinishTransaction();
    if (!documents.Contains(companion))
      documents.PushBack(companion);
    return ezStatus(EZ_SUCCESS);
  }

  ezStatus PrepareMesh(const ezModelImporter2::ImportOptions& options, ezMeshResourceDescriptor& mesh)
  {
    ezBlendShapeResourceDescriptor shapes;
    EZ_SUCCEED_OR_RETURN(ezImportBlendShapeMesh(options, shapes));
    mesh.SetBounds(shapes.m_Mesh.GetBounds());
    mesh.m_fMaxBoneVertexOffset = shapes.m_Mesh.m_fMaxBoneVertexOffset;
    return ezStatus(EZ_SUCCESS);
  }

  void ExposeShapes(const ezAssetDocument& document, ezAssetDocumentInfo& info)
  {
    const auto& mesh = static_cast<const ezAnimatedMeshAssetDocument&>(document);
    if (mesh.GetProperties()->m_sDefaultBlendShapes.IsEmpty())
      return;
    ezModelImporter2::ImportOptions options;
    if (GetOptions(*mesh.GetProperties(), options).Failed())
      return;
    ezDynamicArray<ezBlendShapeTarget> targets;
    if (options.m_sSourceFile.HasExtension("glb") || options.m_sSourceFile.HasExtension("gltf"))
    {
      if (ezReadBlendShapeGltfNames(options.m_sSourceFile, targets, &options).Failed())
        return;
    }
    else
    {
      ezBlendShapeResourceDescriptor shapes;
      if (ezImportBlendShapeMesh(options, shapes).Failed())
        return;
      targets = std::move(shapes.m_Targets);
    }
    ezExposedParameters* parameters = EZ_DEFAULT_NEW(ezExposedParameters);
    for (const auto& target : targets)
    {
      ezExposedParameter* parameter = EZ_DEFAULT_NEW(ezExposedParameter);
      parameter->m_sName = target.m_sName.GetView();
      parameter->m_DefaultValue = target.m_fDefaultWeight;
      parameter->m_Attributes.PushBack(EZ_DEFAULT_NEW(ezClampValueAttribute, -1.0f, 1.0f));
      parameters->m_Parameters.PushBack(parameter);
    }
    info.m_MetaInfo.PushBack(parameters);
  }

  void CopyCurve(const ezCurve1D& source, ezSingleCurveData& destination)
  {
    destination.m_ControlPoints.Clear();
    for (ezUInt32 i = 0; i < source.GetNumControlPoints(); ++i)
    {
      const auto& cp = source.GetControlPoint(i);
      auto& key = destination.m_ControlPoints.ExpandAndGetRef();
      key.m_iTick = static_cast<ezInt64>(ezMath::Round(cp.m_Position.x * 4800.0));
      key.m_fValue = cp.m_Position.y;
      key.m_LeftTangent = cp.m_LeftTangent;
      key.m_RightTangent = cp.m_RightTangent;
      key.m_LeftTangentMode = cp.m_TangentModeLeft;
      key.m_RightTangentMode = cp.m_TangentModeRight;
      key.m_bTangentsLinked = false;
      if (i > 0 && destination.m_ControlPoints[i - 1].m_iTick == key.m_iTick && key.m_iTick > 0)
        --destination.m_ControlPoints[i - 1].m_iTick;
    }
  }
  bool EqualCurves(const ezDynamicArray<ezAnimationClipCurveData>& a, const ezDynamicArray<ezAnimationClipCurveData>& b)
  {
    if (a.GetCount() != b.GetCount())
      return false;
    for (ezUInt32 i = 0; i < a.GetCount(); ++i)
    {
      if (a[i].m_sName != b[i].m_sName || a[i].m_bOverrideSource != b[i].m_bOverrideSource)
        return false;
      const auto& x = a[i].m_Curve.m_ControlPoints;
      const auto& y = b[i].m_Curve.m_ControlPoints;
      if (x.GetCount() != y.GetCount())
        return false;
      for (ezUInt32 j = 0; j < x.GetCount(); ++j)
        if (x[j].m_iTick != y[j].m_iTick || x[j].m_fValue != y[j].m_fValue || x[j].m_LeftTangent != y[j].m_LeftTangent || x[j].m_RightTangent != y[j].m_RightTangent ||
            x[j].m_LeftTangentMode != y[j].m_LeftTangentMode || x[j].m_RightTangentMode != y[j].m_RightTangentMode || x[j].m_bTangentsLinked != y[j].m_bTangentsLinked)
          return false;
    }
    return true;
  }
  ezTransformStatus TransformCurves(ezAssetDocument& document, ezStringView file, ezAnimationClipResourceDescriptor& clip, bool bBackgroundProcessing)
  {
    auto& doc = static_cast<ezAnimationClipAssetDocument&>(document);
    ezDynamicArray<ezAnimationClipCurveData> curves;
    EZ_SUCCEED_OR_RETURN(ezMergeBlendShapeClipCurves(*doc.GetProperties(), file, clip, curves));
    if (!EqualCurves(curves, doc.GetProperties()->m_BlendShapes))
    {
      // Like imported skeleton joints, editable keys must be saved by the editor process.
      if (bBackgroundProcessing)
        return ezTransformStatus(ezTransformResult::NeedsImport);
      doc.SetBlendShapeCurves(curves);
    }
    return ezStatus(EZ_SUCCESS);
  }
} // namespace

ezStatus ezGetMeshBlendShapeTargets(ezStringView meshId, ezDynamicArray<ezBlendShapeTarget>& targets)
{
  targets.Clear();
  if (meshId.IsEmpty())
    return ezStatus(EZ_SUCCESS);
  auto* mesh = OpenMesh(meshId);
  if (mesh == nullptr)
    return ezStatus("The Animated Mesh asset is unavailable.");
  ezModelImporter2::ImportOptions options;
  EZ_SUCCEED_OR_RETURN(GetOptions(*mesh->GetProperties(), options));
  if (options.m_sSourceFile.HasExtension("gltf") || options.m_sSourceFile.HasExtension("glb"))
    return ezReadBlendShapeGltfNames(options.m_sSourceFile, targets, &options);
  ezBlendShapeResourceDescriptor shapes;
  EZ_SUCCEED_OR_RETURN(ezImportBlendShapeMesh(options, shapes));
  targets = std::move(shapes.m_Targets);
  return ezStatus(EZ_SUCCESS);
}

ezStatus ezMergeBlendShapeClipCurves(const ezAnimationClipAssetProperties& properties, ezStringView file,
  ezAnimationClipResourceDescriptor& clip, ezDynamicArray<ezAnimationClipCurveData>& editorCurves)
{
  editorCurves.Clear();
  ezAnimationClipResourceDescriptor imported;
  if (properties.m_bImportBlendShapes && (file.HasExtension("glb") || file.HasExtension("gltf")))
  {
    EZ_SUCCEED_OR_RETURN(ezReadBlendShapeGltfCurves(file, properties.m_sAnimationClipToExtract, imported));
    // Match the native importer's tick-based trimming, including its clamped first frame.
    Assimp::Importer importer;
    ezStringBuilder path(file);
    const auto* scene = importer.ReadFile(path, 0);
    double start = 0;
    if (scene)
      for (ezUInt32 i = 0; i < scene->mNumAnimations; ++i)
      {
        const auto& animation = *scene->mAnimations[i];
        if (!properties.m_sAnimationClipToExtract.IsEmpty() && properties.m_sAnimationClipToExtract != animation.mName.C_Str())
          continue;
        const auto frames = static_cast<ezUInt32>(ezMath::Max(1.0, animation.mDuration));
        if (animation.mTicksPerSecond > 0)
          start = ezMath::Min(properties.m_uiFirstFrame, frames - 1) / animation.mTicksPerSecond;
        break;
      }
    const double end = start + clip.GetDuration().GetSeconds();
    for (auto& curve : imported.m_CustomCurves)
    {
      if (start > 0 || properties.m_uiNumFrames > 0)
      {
        ezCurve1D cropped;
        auto add = [&](double time)
        {
          auto& cp = cropped.AddControlPoint(time - start);
          cp.m_Position.y = curve.m_Curve.Evaluate(time);
          cp.m_TangentModeLeft = ezCurveTangentMode::Linear;
          cp.m_TangentModeRight = ezCurveTangentMode::Linear;
        };
        add(start);
        for (const auto& point : curve.m_Curve.GetLinearApproximation())
          if (point.x > start && point.x < end)
            add(point.x);
        add(end);
        curve.m_Curve = std::move(cropped);
        curve.m_Curve.SortControlPoints();
        curve.m_Curve.CreateLinearApproximation();
      }
    }
    ezDynamicArray<ezBlendShapeTarget> defaults;
    EZ_SUCCEED_OR_RETURN(ezReadBlendShapeGltfNames(file, defaults));
    for (const auto& target : defaults)
    {
      ezStringBuilder name;
      ezBlendShapeResourceDescriptor::MakeCurveName(target.m_sName.GetView(), name);
      bool found = false;
      for (const auto& curve : imported.m_CustomCurves)
        found |= curve.m_sName.GetView() == name;
      if (!found)
      {
        auto& curve = imported.m_CustomCurves.ExpandAndGetRef();
        curve.m_sName.Assign(name);
        curve.m_Curve.AddControlPoint(0).m_Position.y = target.m_fDefaultWeight;
        curve.m_Curve.SortControlPoints();
        curve.m_Curve.CreateLinearApproximation();
      }
    }
  }
  // Animation files often contain only skeleton tracks. The selected mesh still
  // supplies all available morph names so they can be keyed without inventing names.
  ezDynamicArray<ezBlendShapeTarget> meshTargets;
  EZ_SUCCEED_OR_RETURN(ezGetMeshBlendShapeTargets(properties.m_sPreviewMesh, meshTargets));
  for (const auto& target : meshTargets)
  {
    ezStringBuilder name;
    ezBlendShapeResourceDescriptor::MakeCurveName(target.m_sName.GetView(), name);
    bool found = false;
    for (const auto& curve : imported.m_CustomCurves)
      found |= curve.m_sName.GetView() == name;
    if (found)
      continue;
    auto& curve = imported.m_CustomCurves.ExpandAndGetRef();
    curve.m_sName.Assign(name);
    curve.m_Curve.AddControlPoint(0).m_Position.y = target.m_fDefaultWeight;
    if (clip.GetDuration().IsPositive())
      curve.m_Curve.AddControlPoint(clip.GetDuration().GetSeconds()).m_Position.y = target.m_fDefaultWeight;
    curve.m_Curve.SortControlPoints();
    curve.m_Curve.CreateLinearApproximation();
  }
  for (const auto& source : imported.m_CustomCurves)
  {
    auto& curve = editorCurves.ExpandAndGetRef();
    ezStringView name = source.m_sName.GetView();
    name.Shrink(11, 0);
    curve.m_sName = name;
    CopyCurve(source.m_Curve, curve.m_Curve);
  }
  ezHashSet<ezString> authored;
  for (const auto& curve : properties.m_BlendShapes)
  {
    if (!curve.m_bOverrideSource)
      continue;
    if (curve.m_sName.IsEmpty() || authored.Contains(curve.m_sName))
      return ezStatus("Blend shape names must be non-empty and unique.");
    authored.Insert(curve.m_sName);
    ezAnimationClipCurveData* destination = nullptr;
    for (auto& existing : editorCurves)
      if (existing.m_sName == curve.m_sName)
        destination = &existing;
    if (destination == nullptr)
      destination = &editorCurves.ExpandAndGetRef();
    *destination = curve;
  }
  for (const auto& curve : clip.m_CustomCurves)
    if (curve.m_sName.GetView().StartsWith("BlendShape/"))
      return ezStatus("Use the Blend Shapes tab for morph curves, not the ordinary Curves tab.");
  for (const auto& curve : editorCurves)
  {
    auto& output = clip.m_CustomCurves.ExpandAndGetRef();
    ezStringBuilder name;
    ezBlendShapeResourceDescriptor::MakeCurveName(curve.m_sName, name);
    output.m_sName.Assign(name);
    if (!curve.m_bOverrideSource)
    {
      for (const auto& source : imported.m_CustomCurves)
        if (source.m_sName == output.m_sName)
          output.m_Curve = source.m_Curve;
    }
    else
    {
      for (const auto& key : curve.m_Curve.m_ControlPoints)
        if (!ezMath::IsFinite(key.m_fValue) || !key.m_LeftTangent.IsValid() || !key.m_RightTangent.IsValid() || key.m_iTick < 0 || key.GetTickAsTime() > clip.GetDuration())
          return ezStatus("Blend shape keys must be finite and within the clip duration.");
      curve.m_Curve.ConvertToRuntimeData(output.m_Curve);
      output.m_Curve.SortControlPoints();
      output.m_Curve.CreateLinearApproximation();
    }
  }
  return ezStatus(EZ_SUCCESS);
}

ezBlendShapeAssetDocument::ezBlendShapeAssetDocument(ezStringView path)
  : ezSimpleAssetDocument(path, ezAssetDocEngineConnection::None)
{
}

ezTransformStatus ezBlendShapeAssetDocument::InternalTransformAsset(ezStreamWriter& stream, ezStringView, const ezPlatformProfile*, const ezAssetFileHeader&, ezBitflags<ezTransformFlags>)
{
  auto* mesh = OpenMesh(GetProperties()->m_sMesh);
  if (mesh == nullptr)
    return ezStatus("The associated Animated Mesh asset is unavailable. Reimport the Animated Mesh with Import blend shapes enabled.");
  ezModelImporter2::ImportOptions options;
  EZ_SUCCEED_OR_RETURN(GetOptions(*mesh->GetProperties(), options));
  ezBlendShapeResourceDescriptor shapes;
  EZ_SUCCEED_OR_RETURN(ezImportBlendShapeMesh(options, shapes));
  shapes.m_sMesh = GetProperties()->m_sMesh;
  return ezStatus(shapes.Save(stream));
}

ezStatus ezImportBlendShapes(ezAssetDocument& document, ezDynamicArray<ezDocument*>& documents) { return ImportCompanion(document, documents); }
ezStatus ezPrepareBlendShapes(const ezModelImporter2::ImportOptions& options, ezMeshResourceDescriptor& mesh) { return PrepareMesh(options, mesh); }
void ezExposeBlendShapes(const ezAssetDocument& document, ezAssetDocumentInfo& info) { ExposeShapes(document, info); }
ezTransformStatus ezTransformBlendShapeCurves(ezAssetDocument& document, ezStringView file, ezAnimationClipResourceDescriptor& clip, bool background) { return TransformCurves(document, file, clip, background); }

void ezBlendShapeAssetDocument::UpdateAssetDocumentInfo(ezAssetDocumentInfo* pInfo) const
{
  SUPER::UpdateAssetDocumentInfo(pInfo);
  if (auto* mesh = OpenMesh(GetProperties()->m_sMesh))
  {
    // Depend on the settings file, not the mesh resource: the mesh packages this companion.
    // A resource dependency here would create a cycle through the curator's package hash.
    ezStringBuilder path(mesh->GetDocumentPath());
    ezQtEditorApp::GetSingleton()->MakePathDataDirectoryRelative(path);
    pInfo->m_TransformDependencies.Insert(path);
    for (const auto& dependency : mesh->GetAssetDocumentInfo()->m_TransformDependencies)
      pInfo->m_TransformDependencies.Insert(dependency);
  }
}
