#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/Assets/AssetDocumentGenerator.h>
#include <EditorFramework/DocumentWindow/EngineViewWidget.moc.h>
#include <EditorFramework/IPC/EngineProcessConnection.h>
#include <EditorFramework/PropertyGrid/ExposedParametersPropertyWidget.moc.h>
#include <EditorPluginAssets/AnimatedMeshAsset/AnimatedMeshAsset.h>
#include <EditorPluginAssets/AnimationClipAsset/AnimationClipAsset.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <EditorPluginScene/Scene/Scene2Document.h>
#include <EditorTest/TestClass/TestClass.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/IO/OSFile.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.moc.h>
#include <GuiFoundation/Widgets/Curve1DEditorWidget.moc.h>
#include <GuiFoundation/Widgets/TimeScrubberWidget.moc.h>
#include <QApplication>
#include <QFile>
#include <QTableWidget>
#include <QListWidget>
#include <QPointer>
#include <ads/DockManager.h>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <RendererCore/AnimationSystem/AnimationClipResource.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>

// The linked asset managers register Qt icons during core startup.
EZ_TESTFRAMEWORK_ENTRY_POINT_CODE_INJECTION
EZ_APPLICATION_ENTRY_POINT_CODE_INJECTION
int main(int argc, char** argv)
{
  QApplication application(argc, argv);
  application.setProperty("Shared", 1);
  ezTestSetup::InitTestFramework("BlendShapeEditorTest", "Blend Shape Tests", argc, const_cast<const char**>(argv));
  while (ezTestSetup::RunTests() == ezTestAppRun::Continue)
  {
  }
  const ezInt32 failures = ezTestSetup::GetFailedTestCount();
  ezTestSetup::DeInitTestFramework();
  return failures;
}


class ezBlendShapeEditorTest : public ezEditorTest
{
public:
  const char* GetTestName() const override { return "Native Blend Shape Workflow"; }
  void SetupSubTests() override { AddSubTest("Import, pose and native clip", 0); }

  ezTestAppRun RunSubTest(ezInt32, ezUInt32) override
  {
    const auto regressionProject = ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-regressionProject");
    if (!regressionProject.IsEmpty())
      return RunSceneRegression(regressionProject);
    auto* app = m_pApplication->m_pEditorApp;
    ezStringBuilder path(BLEND_SHAPE_TEST_OUTPUT, "/");
    path.AppendFormat("{}", ezUuid::MakeUuid());
    EZ_TEST_BOOL(ezOSFile::CreateDirectoryStructure(path).Succeeded());
    m_sProjectPath = path;
    QCoreApplication::setApplicationName(ezMakeQString(path));
    path.AppendPath("ezProject");
    if (!EZ_TEST_BOOL(app->CreateOrOpenProject(true, path).Succeeded()))
      return ezTestAppRun::Quit;
    EZ_TEST_BOOL(!app->GetPluginBundles().m_Plugins.Contains("BlendShape"));
    app->WritePluginSelectionStateDDL();
    ezEditorEngineProcessConnection::GetSingleton()->SetPluginConfig(app->GetRuntimePluginConfig(true));
    EZ_TEST_BOOL(ezEditorEngineProcessConnection::GetSingleton()->RestartProcess().Succeeded());
    ProcessEvents(20);
    ezStringBuilder source(m_sProjectPath, "/Skinned.gltf");
    EZ_TEST_BOOL(ezOSFile::CopyFile(BLEND_SHAPE_TEST_DATA "/Skinned.gltf", source).Succeeded());
    auto* type = ezRTTI::FindTypeByName("ezAnimatedMeshAssetDocumentGenerator");
    if (!EZ_TEST_BOOL(type != nullptr))
      return ezTestAppRun::Quit;
    ezUniquePtr<ezAssetDocumentGenerator> generator = type->GetAllocator()->Allocate<ezAssetDocumentGenerator>();
    bool checkboxSeen = false;
    QTimer dialogDriver;
    QObject::connect(&dialogDriver, &QTimer::timeout, [&]()
      {
      for (auto* widget : QApplication::topLevelWidgets())
      {
        auto* morphs = widget->findChild<QCheckBox*>("ImportBlendShapes");
        auto* clips = widget->findChild<QCheckBox*>("ImportAnimClips");
        if (morphs == nullptr || clips == nullptr || !widget->isVisible()) continue;
        checkboxSeen = morphs->isVisible();
        morphs->setChecked(true);
        clips->setChecked(true);
        ezStringBuilder screenshot(m_sProjectPath, "/Import.png");
        widget->grab().save(ezMakeQString(screenshot));
        dialogDriver.stop();
        widget->findChild<QDialogButtonBox*>("Buttons")->button(QDialogButtonBox::Ok)->click();
        return;
      } });
    dialogDriver.start(20);
    ezDynamicArray<ezDocument*> documents;
    ezQtUiServices::SetHeadless(false);
    const auto importStatus = generator->Generate(source, "AnimatedMeshImport", documents);
    ezQtUiServices::SetHeadless(true);
    dialogDriver.stop();
    if (!EZ_TEST_BOOL_MSG(importStatus.Succeeded(), "%s", importStatus.GetMessageString().GetData()))
      return ezTestAppRun::Quit;
    EZ_TEST_BOOL(checkboxSeen);
    ezAnimatedMeshAssetDocument* mesh = nullptr;
    ezAnimationClipAssetDocument* clip = nullptr;
    ezAssetDocument* shapes = nullptr;
    ezAssetDocument* skeleton = nullptr;
    for (auto* doc : documents)
    {
      EZ_TEST_STATUS(doc->SaveDocument());
      ezAssetCurator::GetSingleton()->NotifyOfFileChange(doc->GetDocumentPath());
      if (auto* candidate = ezDynamicCast<ezAnimatedMeshAssetDocument*>(doc))
        mesh = candidate;
      if (auto* candidate = ezDynamicCast<ezAnimationClipAssetDocument*>(doc))
        if (candidate->GetProperties()->m_sAnimationClipToExtract == "Linear")
          clip = candidate;
      if (doc->GetDynamicRTTI()->GetTypeName() == ezStringView("ezBlendShapeAssetDocument"))
        shapes = static_cast<ezAssetDocument*>(doc);
      if (doc->GetDynamicRTTI()->GetTypeName() == ezStringView("ezSkeletonAssetDocument"))
        skeleton = static_cast<ezAssetDocument*>(doc);
    }
    if (!EZ_TEST_BOOL(mesh && clip && shapes && skeleton))
      return ezTestAppRun::Quit;
    ezStringBuilder shapeId, meshId;
    ezConversionUtils::ToString(shapes->GetGuid(), shapeId);
    ezConversionUtils::ToString(mesh->GetGuid(), meshId);
    EZ_TEST_STRING(mesh->GetProperties()->m_sDefaultBlendShapes, shapeId);
    EZ_TEST_STRING(clip->GetProperties()->m_sPreviewMesh, meshId);
    ezAnimationClipResourceDescriptor backgroundClip;
    backgroundClip.SetDuration(ezTime::MakeFromSeconds(1));
    const auto backgroundStatus = ezTransformBlendShapeCurves(*clip, source, backgroundClip, true);
    EZ_TEST_BOOL(backgroundStatus.m_Result == ezTransformResult::NeedsImport);
    EZ_TEST_INT(clip->GetProperties()->m_BlendShapes.GetCount(), 0);
    // Open before any foreground transform: names and imported curves must appear automatically.
    app->OpenDocument(clip->GetDocumentPath(), ezDocumentFlags::RequestWindow);
    WaitFrames(15);
    EZ_TEST_INT(clip->GetProperties()->m_BlendShapes.GetCount(), 2);
    const auto* blendShapeProperty = ezGetStaticRTTI<ezAnimationClipAssetProperties>()->FindPropertyByName("BlendShapes");
    const auto* shapeContainer = blendShapeProperty->GetAttributeByType<ezContainerAttribute>();
    if (EZ_TEST_BOOL(shapeContainer != nullptr))
    {
      EZ_TEST_BOOL(!shapeContainer->CanAdd());
      EZ_TEST_BOOL(!shapeContainer->CanDelete());
    }

    ProcessEvents(30);
    EZ_TEST_BOOL(ezAssetCurator::GetSingleton()->GetSubAsset(shapes->GetGuid()).isValid());
    for (auto* doc : {skeleton, static_cast<ezAssetDocument*>(mesh), shapes, static_cast<ezAssetDocument*>(clip)})
    {
      const auto status = doc->TransformAsset(ezTransformFlags::TriggeredManually);
      EZ_TEST_BOOL_MSG(status.Succeeded(), "%s: %s", ezString(doc->GetDocumentPath()).GetData(), status.m_sMessage.GetData());
      if (status.Failed())
      {
        const auto asset = ezAssetCurator::GetSingleton()->GetSubAsset(doc->GetGuid());
        if (asset.isValid())
        {
          for (const auto& dep : asset->m_pAssetInfo->m_MissingTransformDeps)
            ezLog::Info("Missing transform dependency: {}", dep);
          for (const auto& dep : asset->m_pAssetInfo->m_MissingPackageDeps)
            ezLog::Info("Missing package dependency: {}", dep);
        }
      }
      EZ_TEST_STATUS(doc->SaveDocument());
      ezAssetCurator::GetSingleton()->NotifyOfFileChange(doc->GetDocumentPath());
    }
    // Regression: opening the companion must keep the document dock manager alive.
    app->OpenDocument(shapes->GetDocumentPath(), ezDocumentFlags::RequestWindow);
    WaitFrames(40);
    auto* shapesWindow = ezQtDocumentWindow::FindWindowByDocument(shapes);
    if (!EZ_TEST_BOOL(shapesWindow != nullptr))
      return ezTestAppRun::Quit;
    QPointer<ads::CDockManager> shapeDock = qobject_cast<ads::CDockManager*>(shapesWindow->centralWidget());
    EZ_TEST_BOOL(!shapeDock.isNull());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    WaitFrames(20);
    EZ_TEST_BOOL(!shapeDock.isNull());
    auto* targetTable = shapesWindow->findChild<QTableWidget*>("BlendShapeTargets");
    if (EZ_TEST_BOOL(targetTable != nullptr))
      EZ_TEST_INT(targetTable->rowCount(), 2);
    QMetaObject::invokeMethod(shapesWindow, "SlotRestoreDocumentLayout", Qt::DirectConnection);
    EZ_TEST_STATUS(shapes->SaveDocument());
    ezString shapePath = shapes->GetDocumentPath();
    shapes->GetDocumentManager()->CloseDocument(shapes);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // Assets saved by the former optional plugin must resolve to the native type.
    QFile legacyAsset(QString::fromUtf8(shapePath.GetData()));
    if (EZ_TEST_BOOL(legacyAsset.open(QIODevice::ReadOnly)))
    {
      QByteArray contents = legacyAsset.readAll();
      legacyAsset.close();
      contents.replace("ezEditorPluginAssets", "ezEditorPluginBlendShape");
      if (EZ_TEST_BOOL(legacyAsset.open(QIODevice::WriteOnly | QIODevice::Truncate)))
      {
        EZ_TEST_INT(legacyAsset.write(contents), contents.size());
        legacyAsset.close();
      }
    }
    shapes = ezDynamicCast<ezAssetDocument*>(app->OpenDocument(shapePath, ezDocumentFlags::RequestWindow));
    if (!EZ_TEST_BOOL(shapes != nullptr))
      return ezTestAppRun::Quit;
    WaitFrames(20);
    shapesWindow = ezQtDocumentWindow::FindWindowByDocument(shapes);
    if (!EZ_TEST_BOOL(shapesWindow != nullptr))
      return ezTestAppRun::Quit;
    EZ_TEST_BOOL(qobject_cast<ads::CDockManager*>(shapesWindow->centralWidget()) != nullptr);

    EZ_TEST_INT(clip->GetProperties()->m_BlendShapes.GetCount(), 2);
    for (const auto& curve : clip->GetProperties()->m_BlendShapes)
    {
      EZ_TEST_INT(curve.m_Curve.m_ControlPoints.GetCount(), 2);
      EZ_TEST_DOUBLE(curve.m_Curve.Evaluate(2400), curve.m_sName == "Face/Smile" ? 0.5 : -0.5, 0.001);
    }
    const auto* parameters = mesh->GetAssetDocumentInfo()->GetMetaInfo<ezExposedParameters>();
    if (EZ_TEST_BOOL(parameters != nullptr))
    {
      EZ_TEST_BOOL(parameters->Find("Face/Smile") != nullptr);
      EZ_TEST_BOOL(parameters->Find("Face/Blink") != nullptr);
    }
    ezStringBuilder plainSource(m_sProjectPath, "/SkeletonOnly.gltf");
    EZ_TEST_BOOL(ezOSFile::CopyFile(BLEND_SHAPE_TEST_DATA "/SkeletonOnly.gltf", plainSource).Succeeded());
    ezStringBuilder plainPath(m_sProjectPath, "/SkeletonOnly.ezAnimationClipAsset");
    auto* plainClip = ezDynamicCast<ezAnimationClipAssetDocument*>(app->CreateDocument(plainPath, ezDocumentFlags::None));
    if (!EZ_TEST_BOOL(plainClip != nullptr))
      return ezTestAppRun::Quit;
    auto* plainAccessor = plainClip->GetObjectAccessor();
    plainAccessor->StartTransaction("Set animation source");
    EZ_TEST_STATUS(plainAccessor->SetValueByName(plainClip->GetPropertyObject(), "File", "SkeletonOnly.gltf"));
    EZ_TEST_STATUS(plainAccessor->SetValueByName(plainClip->GetPropertyObject(), "UseAnimationClip", "Linear"));
    plainAccessor->FinishTransaction();
    EZ_TEST_STATUS(plainClip->SaveDocument());
    app->OpenDocument(plainPath, ezDocumentFlags::RequestWindow);
    WaitFrames(10);
    EZ_TEST_INT(plainClip->GetProperties()->m_BlendShapes.GetCount(), 0);
    plainAccessor->StartTransaction("Select a mesh with morph targets");
    EZ_TEST_STATUS(plainAccessor->SetValueByName(plainClip->GetPropertyObject(), "PreviewMesh", meshId.GetView()));
    plainAccessor->FinishTransaction();
    WaitFrames(15);
    EZ_TEST_INT(plainClip->GetProperties()->m_BlendShapes.GetCount(), 2);
    for (const auto& curve : plainClip->GetProperties()->m_BlendShapes)
    {
      EZ_TEST_BOOL(curve.m_sName == "Face/Smile" || curve.m_sName == "Face/Blink");
      EZ_TEST_DOUBLE(curve.m_Curve.Evaluate(2400), 0, 0.001);
    }
    // Refreshing the available names must preserve edited keys and override state.
    auto authored = plainClip->GetProperties()->m_BlendShapes;
    authored[0].m_bOverrideSource = true;
    authored[0].m_Curve.m_ControlPoints[0].m_fValue = 0.7;
    plainClip->SetBlendShapeCurves(authored);
    EZ_TEST_STATUS(plainClip->RefreshBlendShapeCurves());
    EZ_TEST_BOOL(plainClip->GetProperties()->m_BlendShapes[0].m_bOverrideSource);
    EZ_TEST_DOUBLE(plainClip->GetProperties()->m_BlendShapes[0].m_Curve.m_ControlPoints[0].m_fValue, 0.7, 0.001);
    EZ_TEST_STATUS(plainClip->SaveDocument());

    // The command also upgrades an already imported native mesh without duplicating other assets.
    mesh->GetObjectAccessor()->StartTransaction("Remove shape assignment");
    EZ_TEST_STATUS(mesh->GetObjectAccessor()->SetValueByName(mesh->GetPropertyObject(), "DefaultBlendShapes", ""));
    mesh->GetObjectAccessor()->FinishTransaction();
    ExecuteDocumentAction("BlendShapes.Import", mesh);
    EZ_TEST_STRING(mesh->GetProperties()->m_sDefaultBlendShapes, shapeId);
    EZ_TEST_BOOL(ezAssetCurator::GetSingleton()->WriteAssetTables(nullptr, true).Succeeded());
    EZ_TEST_BOOL(ezEditorEngineProcessConnection::GetSingleton()->RestartProcess().Succeeded());
    WaitFrames(10);
    ezStringBuilder scenePath(m_sProjectPath, "/Morph.ezScene");
    auto* scene = ezDynamicCast<ezScene2Document*>(app->CreateDocument(scenePath, ezDocumentFlags::RequestWindow));
    if (!EZ_TEST_BOOL(scene != nullptr))
      return ezTestAppRun::Quit;
    const auto* object = CreateGameObject(scene, nullptr, "Animated Mesh with Shape Pose");
    auto* accessor = scene->GetObjectAccessor();
    accessor->StartTransaction("Add ordinary mesh and shape pose");
    ezUuid meshComponentId, poseId;
    EZ_TEST_STATUS(accessor->AddObjectByName(object, "Components", -1, ezRTTI::FindTypeByName("ezAnimatedMeshComponent"), meshComponentId));
    EZ_TEST_STATUS(accessor->AddObjectByName(object, "Components", -1, ezRTTI::FindTypeByName("ezBlendShapePoseComponent"), poseId));
    const auto* pose = accessor->GetObject(poseId);
    EZ_TEST_STATUS(accessor->SetValueByName(accessor->GetObject(meshComponentId), "Mesh", meshId.GetView()));
    accessor->FinishTransaction();
    const auto undoCount = scene->GetCommandHistory()->GetUndoStackSize();
    ProcessEvents(40);
    EZ_TEST_INT(scene->GetCommandHistory()->GetUndoStackSize(), undoCount);
    EZ_TEST_STATUS(scene->GetCommandHistory()->Undo());
    ProcessEvents(40);
    EZ_TEST_BOOL(accessor->GetObject(poseId) == nullptr);
    EZ_TEST_STATUS(scene->GetCommandHistory()->Redo());
    ProcessEvents(40);
    pose = accessor->GetObject(poseId);
    if (!EZ_TEST_BOOL(pose != nullptr))
      return ezTestAppRun::Quit;
    EZ_TEST_INT(scene->GetCommandHistory()->GetUndoStackSize(), undoCount);
    EZ_TEST_STRING(pose->GetTypeAccessor().GetValue("Mesh").ConvertTo<ezString>(), meshId);
    const auto* weights = pose->GetType()->FindPropertyByName("Weights");
    ezExposedParameterCommandAccessor proxy(accessor, weights, pose->GetType()->FindPropertyByName("Mesh"));
    ezDynamicArray<ezVariant> keys;
    EZ_TEST_STATUS(proxy.GetKeys(pose, weights, keys));
    EZ_TEST_INT(keys.GetCount(), 2);
    ezVariant weight;
    EZ_TEST_STATUS(proxy.GetValue(pose, weights, weight, "Face/Smile"));
    EZ_TEST_FLOAT(weight.ConvertTo<float>(), 0, 0);
    accessor->StartTransaction("Preview smile");
    EZ_TEST_STATUS(proxy.SetValue(pose, weights, 0.75f, "Face/Smile"));
    accessor->FinishTransaction();
    EZ_TEST_STATUS(proxy.GetValue(pose, weights, weight, "Face/Smile"));
    EZ_TEST_FLOAT(weight.ConvertTo<float>(), 0.75f, 0);
    scene->GetSelectionManager()->SetSelection(object);
    EZ_TEST_STATUS(scene->SaveDocument());
    app->OpenDocument(clip->GetDocumentPath(), ezDocumentFlags::RequestWindow);
    WaitFrames(10);
    if (auto* window = ezQtDocumentWindow::FindWindowByDocument(clip))
    {
      window->EnsureVisible();
      clip->SetCommonAssetUiState(ezCommonAssetUiState::Pause, 1.0f);
      WaitFrames(20);
      auto* panel = window->findChild<QWidget*>("AnimClipBlendShapeCurvesPanel");
      if (!EZ_TEST_BOOL(panel != nullptr))
        return ezTestAppRun::Quit;
      auto* editor = panel->findChild<ezQtCurve1DEditorWidget*>();
      if (!EZ_TEST_BOOL(editor != nullptr))
        return ezTestAppRun::Quit;
      auto* names = panel->findChild<QListWidget*>("BlendShapeCurveNames");
      if (!EZ_TEST_BOOL(names != nullptr))
        return ezTestAppRun::Quit;
      EZ_TEST_INT(names->count(), 2);
      for (int i = 0; i < names->count(); ++i)
      {
        EZ_TEST_STRING(names->item(i)->text().toUtf8().constData(), clip->GetProperties()->m_BlendShapes[i].m_sName);
        EZ_TEST_BOOL(!names->item(i)->icon().isNull());
      }
      names->setCurrentRow(1);
      EZ_TEST_INT(editor->CurveEdit->GetSelection().GetCount(), 2);
      for (const auto& selected : editor->CurveEdit->GetSelection())
        EZ_TEST_INT(selected.m_uiCurve, 1);
      editor->CurveEdit->SetSelection({0, 0});
      EZ_TEST_BOOL(names->item(0)->isSelected());
      EZ_TEST_BOOL(!names->item(1)->isSelected());
      // The ordinary Curves panel uses the same named selection UI.
      auto* clipAccessor = clip->GetObjectAccessor();
      clipAccessor->StartTransaction("Add named custom curve");
      ezUuid customCurve;
      EZ_TEST_STATUS(clipAccessor->AddObjectByName(clip->GetPropertyObject(), "Curves", -1, ezGetStaticRTTI<ezAnimationClipCurveData>(), customCurve));
      EZ_TEST_STATUS(clipAccessor->SetValueByName(clipAccessor->GetObject(customCurve), "Name", "Speed"));
      clipAccessor->FinishTransaction();
      auto* dockManager = qobject_cast<ads::CDockManager*>(window->centralWidget());
      auto* customPanel = dockManager ? dockManager->findDockWidget("AnimClipCustomCurvesPanel") : nullptr;
      auto* customNames = customPanel ? customPanel->findChild<QListWidget*>("AnimationCurveNames") : nullptr;
      if (EZ_TEST_BOOL(customNames != nullptr))
      {
        EZ_TEST_INT(customNames->count(), 1);
        EZ_TEST_STRING(customNames->item(0)->text().toUtf8().constData(), "Speed");
      }
      EZ_TEST_STATUS(clip->SaveDocument());
      QFile savedAsset(QString::fromUtf8(clip->GetDocumentPath().GetStartPointer(), clip->GetDocumentPath().GetElementCount()));
      EZ_TEST_BOOL(savedAsset.open(QIODevice::ReadOnly));
      const auto savedBytes = savedAsset.readAll();
      savedAsset.close();
      auto* scrubber = window->findChild<ezQtTimeScrubberWidget*>();
      if (!EZ_TEST_BOOL(scrubber != nullptr))
        return ezTestAppRun::Quit;
      scrubber->ScrubberPosChangedEvent(2400);
      WaitFrames(10);
      QImage before;
      CapturePreview(window, "Before.png", before);
      ezUInt32 smile = clip->GetProperties()->m_BlendShapes[0].m_sName == "Face/Smile" ? 0 : 1;
      editor->BeginOperationEvent("Drag imported smile");
      editor->BeginCpChangesEvent("Edit imported smile");
      editor->CpMovedEvent(smile, 0, 0, -1.0);
      editor->CpMovedEvent(smile, 1, 4800, -1.0);
      editor->EndCpChangesEvent();
      EZ_TEST_BOOL(clip->GetProperties()->m_BlendShapes[smile].m_bOverrideSource);
      EZ_TEST_DOUBLE(clip->GetProperties()->m_BlendShapes[smile].m_Curve.Evaluate(2400), -1.0, 0.001);
      EZ_TEST_BOOL(!clip->GetProperties()->m_BlendShapes[1 - smile].m_bOverrideSource);
      // Capture while the drag operation is still open, without saving or transforming.
      WaitFrames(20);
      QImage live;
      CapturePreview(window, "LiveDrag.png", live);
      EZ_TEST_BOOL_MSG(ChangedPixels(before, live) > 300, "Unsaved drag did not update the preview");
      editor->EndOperationEvent(true);
      EZ_TEST_STATUS(clip->GetCommandHistory()->Undo());
      WaitFrames(15);
      QImage undone;
      CapturePreview(window, "LiveUndo.png", undone);
      EZ_TEST_BOOL_MSG(ChangedPixels(before, undone) < 100, "Undo did not restore the live preview");
      EZ_TEST_BOOL(!clip->GetProperties()->m_BlendShapes[smile].m_bOverrideSource);
      EZ_TEST_DOUBLE(clip->GetProperties()->m_BlendShapes[smile].m_Curve.Evaluate(2400), 0.5, 0.001);
      EZ_TEST_STATUS(clip->GetCommandHistory()->Redo());
      EZ_TEST_DOUBLE(clip->GetProperties()->m_BlendShapes[smile].m_Curve.Evaluate(2400), -1.0, 0.001);
      EZ_TEST_BOOL(clip->IsModified());
      EZ_TEST_BOOL(savedAsset.open(QIODevice::ReadOnly));
      EZ_TEST_BOOL(savedAsset.readAll() == savedBytes);
      savedAsset.close();
      WaitFrames(20);
      QImage after;
      CapturePreview(window, "After.png", after);
      ezUInt32 changedPixels = 0;
      EZ_TEST_BOOL(before.size() == after.size());
      for (int y = 0; y < before.height(); ++y)
        for (int x = 0; x < before.width(); ++x)
        {
          const auto a = before.pixelColor(x, y);
          const auto b = after.pixelColor(x, y);
          if (ezMath::Abs(a.red() - b.red()) + ezMath::Abs(a.green() - b.green()) + ezMath::Abs(a.blue() - b.blue()) > 60)
            ++changedPixels;
        }
      EZ_TEST_BOOL_MSG(changedPixels > 300, "Morph edit changed only %u pixels", changedPixels);
      ezStringBuilder screenshot(m_sProjectPath, "/AnimationClip.png");
      window->grab().save(ezMakeQString(screenshot));
    }
    if (auto* window = ezQtDocumentWindow::FindWindowByDocument(scene))
    {
      window->EnsureVisible();
      WaitFrames(10);
      ezStringBuilder screenshot(m_sProjectPath, "/Pose.png");
      window->grab().save(ezMakeQString(screenshot));
    }
    return ezTestAppRun::Quit;
  }

private:
  static ezUInt32 ChangedPixels(const QImage& before, const QImage& after)
  {
    if (before.size() != after.size())
      return ezInvalidIndex;
    ezUInt32 changed = 0;
    for (int y = 0; y < before.height(); ++y)
      for (int x = 0; x < before.width(); ++x)
      {
        const auto a = before.pixelColor(x, y);
        const auto b = after.pixelColor(x, y);
        if (ezMath::Abs(a.red() - b.red()) + ezMath::Abs(a.green() - b.green()) + ezMath::Abs(a.blue() - b.blue()) > 60)
          ++changed;
      }
    return changed;
  }

  // Run only on a disposable copy: the clip is edited to exercise both preview paths.
  ezTestAppRun RunSceneRegression(ezStringView project)
  {
    auto* app = m_pApplication->m_pEditorApp;
    m_sProjectPath = project;
    QCoreApplication::setApplicationName(ezMakeQString(project));
    const char* bundles[] = {"AI", "AngelScript", "Fmod", "GameComponents", "Jolt", "Kraut", "Particles", "ProcGen", "RmlUi", "Terrain", "VisualScript"};
    for (const char* name : bundles)
      for (const auto& dll : app->GetPluginBundles().m_Plugins[name].m_EditorPlugins)
        EZ_TEST_BOOL(ezPlugin::LoadPlugin(dll).Succeeded());
    ezStringBuilder projectFile(project, "/ezProject");
    if (!EZ_TEST_BOOL(app->CreateOrOpenProject(false, projectFile).Succeeded()))
      return ezTestAppRun::Quit;
    for (const char* name : bundles)
      app->GetPluginBundles().m_Plugins[name].m_bSelected = true;
    auto* engine = ezEditorEngineProcessConnection::GetSingleton();
    engine->SetPluginConfig(app->GetRuntimePluginConfig(true));
    EZ_TEST_BOOL(engine->RestartProcess().Succeeded());
    for (const char* name : {"Main", "Animation"})
    {
      ezStringBuilder scenePath(project, "/Scenes/", name, ".ezScene");
      auto* document = app->OpenDocument(scenePath, ezDocumentFlags::RequestWindow);
      if (!EZ_TEST_BOOL(document != nullptr))
        return ezTestAppRun::Quit;
      WaitFrames(60);
      if (!EZ_TEST_BOOL(!engine->IsProcessCrashed()))
        return ezTestAppRun::Quit;
      auto* window = ezQtDocumentWindow::FindWindowByDocument(document);
      if (!EZ_TEST_BOOL(window != nullptr))
        return ezTestAppRun::Quit;
      window->EnsureVisible();
      for (auto* view : window->findChildren<ezQtEngineViewWidget*>())
      {
        view->m_pViewConfig->m_RenderMode = ezViewRenderMode::Default;
        view->m_pViewConfig->m_Perspective = ezSceneViewPerspective::Perspective;
        view->m_pViewConfig->ApplyPerspectiveSetting(70.0f);
        view->InterpolateCameraTo(ezVec3(12, -12, 8), ezVec3(-12, 12, -6).GetNormalized(), 70.0f, nullptr, true);
      }
      QImage capture;
      ezStringBuilder filename(name, ".png");
      ezUInt32 visiblePixels = 0;
      for (ezUInt32 attempt = 0; attempt < 6 && visiblePixels < 1024; ++attempt)
      {
        WaitFrames(120);
        CapturePreview(window, filename, capture);
        visiblePixels = 0;
        for (int y = 0; y < capture.height(); ++y)
          for (int x = 0; x < capture.width(); ++x)
            if (capture.pixelColor(x, y).lightness() > 10)
              ++visiblePixels;
      }
      EZ_TEST_BOOL_MSG(visiblePixels >= 1024, "%s scene did not render visible content", name);
    }
    ezStringBuilder clipPath(project, "/Animations/Pete/Wave.ezAnimationClipAsset");
    auto* clip = ezDynamicCast<ezAnimationClipAssetDocument*>(app->OpenDocument(clipPath, ezDocumentFlags::RequestWindow));
    if (!EZ_TEST_BOOL(clip != nullptr))
      return ezTestAppRun::Quit;
    auto* accessor = clip->GetObjectAccessor();
    accessor->StartTransaction("Test rest pose preview");
    EZ_TEST_STATUS(accessor->SetValueByName(clip->GetPropertyObject(), "UseAnimationClip", "Rest Pose"));
    EZ_TEST_STATUS(accessor->SetValueByName(clip->GetPropertyObject(), "Additive", false));
    EZ_TEST_STATUS(accessor->SetValueByName(clip->GetPropertyObject(), "BasePreviewAnim", ""));
    accessor->FinishTransaction();
    EZ_TEST_BOOL(ezAssetCurator::GetSingleton()->TransformAsset(clip->GetGuid(), ezTransformFlags::TriggeredManually).Succeeded());
    auto* window = ezQtDocumentWindow::FindWindowByDocument(clip);
    if (!EZ_TEST_BOOL(window != nullptr))
      return ezTestAppRun::Quit;
    window->EnsureVisible();
    clip->SetCommonAssetUiState(ezCommonAssetUiState::Pause, 1.0f);
    WaitFrames(40);
    QImage rest;
    CapturePreview(window, "PeteRest.png", rest);
    accessor->StartTransaction("Test additive preview without base");
    EZ_TEST_STATUS(accessor->SetValueByName(clip->GetPropertyObject(), "Additive", true));
    accessor->FinishTransaction();
    EZ_TEST_BOOL(ezAssetCurator::GetSingleton()->TransformAsset(clip->GetGuid(), ezTransformFlags::TriggeredManually).Succeeded());
    WaitFrames(40);
    QImage additive;
    CapturePreview(window, "PeteAdditive.png", additive);
    if (EZ_TEST_BOOL(rest.size() == additive.size()) && !rest.isNull())
    {
      ezUInt32 differences = 0;
      for (int y = 0; y < rest.height(); ++y)
        for (int x = 0; x < rest.width(); ++x)
        {
          const auto a = rest.pixelColor(x, y);
          const auto b = additive.pixelColor(x, y);
          if (ezMath::Abs(a.red() - b.red()) + ezMath::Abs(a.green() - b.green()) + ezMath::Abs(a.blue() - b.blue()) > 60)
            ++differences;
        }
      EZ_TEST_BOOL_MSG(differences < 2000, "Additive identity changed the rest pose preview by %u pixels", differences);
    }
    EZ_TEST_BOOL(!engine->IsProcessCrashed());
    return ezTestAppRun::Quit;
  }

  void CapturePreview(ezQtDocumentWindow* window, const char* name, QImage& image)
  {
    ezStringBuilder path(m_sProjectPath, "/", name);
    ezOSFile::DeleteFile(path).IgnoreResult();
    image = QImage();
    window->CreateImageCapture(path);
    for (ezUInt32 i = 0; i < 500; ++i)
    {
      ProcessEvents();
      if (image.load(ezMakeQString(path)))
        break;
      QThread::msleep(10);
    }
    EZ_TEST_BOOL(!image.isNull());
  }
};
static ezBlendShapeEditorTest s_BlendShapeEditorTest;
