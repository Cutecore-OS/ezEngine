#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/CodeGen/CppProject.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <EditorPluginMovies/EditorPluginMoviesPCH.h>
#include <EditorPluginScene/Scene/SceneDocument.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <GuiFoundation/Action/ActionManager.h>
#include <GuiFoundation/Action/ActionMapManager.h>
#include <GuiFoundation/Action/BaseActions.h>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <ToolsFoundation/Project/ToolsProject.h>
#include <cmath>

EZ_PLUGIN_DEPENDENCY(ezEditorPluginScene);

class ezRenderMovieAction : public ezButtonAction
{
  EZ_ADD_DYNAMIC_REFLECTION(ezRenderMovieAction, ezButtonAction);

public:
  ezRenderMovieAction(const ezActionContext& context, const char* szName)
    : ezButtonAction(context, szName, false, ":/EditorPluginMovies/RenderMovie.svg")
  {
    auto* pScene = ezDynamicCast<ezSceneDocument*>(context.m_pDocument);
    SetEnabled(pScene && !pScene->IsPrefab());
  }

  void Execute(const ezVariant& value) override
  {
    auto* pScene = ezDynamicCast<ezSceneDocument*>(m_Context.m_pDocument);
    if (!pScene || pScene->IsPrefab())
      return;
    auto* pParent = m_Context.m_pWindow;
    if (pScene->GetGameMode() != GameMode::Off)
    {
      QMessageBox::information(pParent, "Render Movie", "Stop scene simulation before rendering a movie.");
      return;
    }

    QDialog dialog(pParent);
    dialog.setWindowTitle("Render Movie");
    auto* pForm = new QFormLayout(&dialog);
    QComboBox resolution;
    resolution.addItems({"1920 x 1080 (16:9)", "3840 x 2160 (16:9)", "1280 x 720 (16:9)", "1080 x 1920 (9:16)", "1080 x 1080 (1:1)", "Custom"});
    QSpinBox width, height, fps, bitrate;
    width.setRange(16, 8192);
    width.setSingleStep(2);
    width.setValue(1920);
    height.setRange(16, 8192);
    height.setSingleStep(2);
    height.setValue(1080);
    fps.setRange(1, 240);
    fps.setValue(30);
    fps.setSuffix(" fps");
    bitrate.setRange(1, 1000);
    bitrate.setValue(20);
    bitrate.setSuffix(" Mbps");
    QDoubleSpinBox duration;
    duration.setRange(0.01, 86400);
    duration.setDecimals(3);
    duration.setValue(10);
    duration.setSuffix(" s");
    QComboBox format, encoder;
    format.addItem("MP4 (AAC audio)", "mp4");
    format.addItem("Matroska (lossless PCM audio)", "matroska");
    encoder.addItem("Auto (GPU, then software)", "auto");
    encoder.addItem("NVIDIA H.264 (NVENC)", "h264_nvenc");
    encoder.addItem("AMD H.264 (AMF)", "h264_amf");
    encoder.addItem("Intel H.264 (Quick Sync)", "h264_qsv");
    encoder.addItem("Software MPEG-4", "mpeg4");
    QLabel timing;
    auto updateTiming = [&]()
    {
      const auto frames = std::max(1LL, std::llround(duration.value() * fps.value()));
      timing.setText(QString("%1 frames / %2 s (rounded to a whole frame)").arg(frames).arg(static_cast<double>(frames) / fps.value(), 0, 'f', 6));
    };
    QObject::connect(&duration, &QDoubleSpinBox::valueChanged, &dialog, updateTiming);
    QObject::connect(&fps, &QSpinBox::valueChanged, &dialog, updateTiming);
    bool bSettingPreset = false;
    QObject::connect(&resolution, &QComboBox::currentIndexChanged, &dialog, [&](int i)
      {
      const int sizes[][2] = {{1920, 1080}, {3840, 2160}, {1280, 720}, {1080, 1920}, {1080, 1080}};
      if (i < 5)
      {
        bSettingPreset = true;
        width.setValue(sizes[i][0]);
        height.setValue(sizes[i][1]);
        bSettingPreset = false;
      } });
    auto customResolution = [&]()
    {
      if (!bSettingPreset)
        resolution.setCurrentIndex(5);
    };
    QObject::connect(&width, &QSpinBox::valueChanged, &dialog, customResolution);
    QObject::connect(&height, &QSpinBox::valueChanged, &dialog, customResolution);
    QSettings settings;
    settings.beginGroup("RenderMovie");
    fps.setValue(settings.value("fps", 30).toInt());
    duration.setValue(settings.value("duration", 10.0).toDouble());
    bitrate.setValue(settings.value("bitrate", 20).toInt());
    pForm->addRow("Resolution / aspect ratio", &resolution);
    pForm->addRow("Width", &width);
    pForm->addRow("Height", &height);
    pForm->addRow("Frame rate", &fps);
    pForm->addRow("Video bitrate", &bitrate);
    pForm->addRow("Format", &format);
    pForm->addRow("Encoder", &encoder);
    pForm->addRow("Track duration", &duration);
    pForm->addRow(&timing);
    QLabel note("Renders the scene's active game camera from the beginning.\nSound: MiniAudio. GPU availability depends on the installed driver.\nMP4 / MKV: H.264 on GPU; Auto may use MPEG-4 in software.");
    pForm->addRow(&note);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    pForm->addRow(&buttons);
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&]()
      {
      if ((width.value() & 1) || (height.value() & 1))
        QMessageBox::warning(&dialog, "Render Movie", "Width and height must be even numbers.");
      else
        dialog.accept(); });
    updateTiming();
    if (dialog.exec() != QDialog::Accepted)
      return;
    settings.setValue("fps", fps.value());
    settings.setValue("duration", duration.value());
    settings.setValue("bitrate", bitrate.value());
    const QString extension = format.currentIndex() == 0 ? "mp4" : "mkv";
    QString output = QFileDialog::getSaveFileName(pParent, "Render Movie", QString("Movie.") + extension, QString("Movie (*.%1)").arg(extension));
    if (output.isEmpty())
      return;
    if (!output.endsWith("." + extension, Qt::CaseInsensitive))
      output += "." + extension;
    // Publish only a complete movie. Never replace an existing file, including races during rendering.
    if (QFileInfo::exists(output))
    {
      QMessageBox::warning(pParent, "Render Movie", "Choose a new filename. The destination already exists.");
      return;
    }
    const auto pluginConfig = ezQtEditorApp::GetSingleton()->GetRuntimePluginConfig(false);
    bool bMiniAudio = false, bMovies = false, bFmod = false;
    for (const auto& plugin : pluginConfig.m_Plugins)
    {
      bMiniAudio |= plugin.m_sAppDirRelativePath == "ezMiniAudioPlugin";
      bMovies |= plugin.m_sAppDirRelativePath == "ezMoviesPlugin";
      bFmod |= plugin.m_sAppDirRelativePath == "ezFmodPlugin";
    }
    if (!bMiniAudio || !bMovies || bFmod)
    {
      QMessageBox::warning(pParent, "Render Movie", "Enable Movies and MiniAudio in the project plugin settings. FMOD audio is not supported by this exporter.");
      return;
    }
    if (ezCppProject::EnsureCppPluginReady().Failed() || pScene->SaveDocument().Failed())
      return;
    if (ezQtEditorApp::GetSingleton()->GetRuntimePluginConfig(false).Save().Failed())
    {
      QMessageBox::warning(pParent, "Render Movie", "Cannot save the runtime plugin configuration.");
      return;
    }
    auto* pCurator = ezAssetCurator::GetSingleton();
    if (pCurator->TransformAllAssets().Failed())
    {
      QMessageBox::warning(pParent, "Render Movie", "Asset transformation failed. See the editor log.");
      return;
    }
    if (pScene->ExportScene(false).Failed())
    {
      QMessageBox::warning(pParent, "Render Movie", "Scene export failed. See the editor log.");
      return;
    }
    QTemporaryDir temporary(QFileInfo(output).absolutePath() + "/.ezMovie-XXXXXX");
    if (!temporary.isValid())
    {
      QMessageBox::warning(pParent, "Render Movie", "Cannot create a temporary output directory.");
      return;
    }
    const QString temporaryMovie = temporary.path() + "/Movie." + extension;
    const QString windowConfig = temporary.path() + "/Window.ddl";
    QFile windowFile(windowConfig);
    const QByteArray windowDescription =
      "WindowDesc { string %Title{\"Render Movie\"} string %Mode{\"Window\"} "
      "Vec2u %Resolution{uint32{640,360}} bool %ClipMouseCursor{false} "
      "bool %ShowMouseCursor{true} bool %SetForegroundOnInit{false} }\n";
    if (!windowFile.open(QIODevice::WriteOnly) || windowFile.write(windowDescription) != windowDescription.size())
    {
      QMessageBox::warning(pParent, "Render Movie", "Cannot write the temporary player window configuration.");
      return;
    }
    windowFile.close();
    const auto frames = std::max(1LL, std::llround(duration.value() * fps.value()));
    ezStringBuilder sScene = pScene->GetAssetDocumentManager()->GetAbsoluteOutputFileName(pScene->GetAssetDocumentTypeDescriptor(), pScene->GetDocumentPath(), "");
    ezStringBuilder sDataDir = pCurator->FindDataDirectoryForAsset(pScene->GetDocumentPath());
    if (sScene.MakeRelativeTo(sDataDir).Failed())
      return;
    QStringList arguments;
    arguments << "-project" << ezToolsProject::GetSingleton()->GetProjectDirectory().GetData()
              << "-scene" << sScene.GetData() << "-profile" << ezString(pCurator->GetActiveAssetProfile()->GetConfigName()).GetData()
              << "-wnd" << windowConfig
              << "-movie-output" << temporaryMovie << "-movie-width" << QString::number(width.value())
              << "-movie-height" << QString::number(height.value()) << "-movie-fps" << QString::number(fps.value())
              << "-movie-frames" << QString::number(frames) << "-movie-bitrate" << QString::number(bitrate.value() * 1000000)
              << "-movie-format" << format.currentData().toString() << "-movie-encoder" << encoder.currentData().toString();
    if (ezCommandLineUtils::GetGlobalInstance()->HasOption("-renderer"))
      arguments << "-renderer" << ezString(ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-renderer")).GetData();
    QProcess process;
    process.setWorkingDirectory(QCoreApplication::applicationDirPath());
    process.setProcessChannelMode(QProcess::MergedChannels);
    QProgressDialog progress("Loading scene and preparing the movie encoder...", "Cancel", 0, static_cast<int>(frames), pParent);
    progress.setWindowTitle("Render Movie");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    QByteArray log;
    bool bSuccess = false;
    QObject::connect(&process, &QProcess::readyReadStandardOutput, &progress, [&]()
      {
      log += process.readAllStandardOutput();
      log = log.right(65536); });
    QObject::connect(&process, &QProcess::finished, &progress, [&](int code, QProcess::ExitStatus status)
      {
      bSuccess = code == 0 && status == QProcess::NormalExit && QFileInfo(temporaryMovie).size() > 0;
      progress.done(QDialog::Accepted); });
    QElapsedTimer startupTimer;
    startupTimer.start();
    bool bStartupTimeout = false;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &progress, [&]()
      {
      if (startupTimer.elapsed() > 310000 && !QFileInfo::exists(temporaryMovie + ".progress"))
      {
        bStartupTimeout = true;
        process.kill();
        return;
      }
      QFile file(temporaryMovie + ".progress");
      if (file.open(QIODevice::ReadOnly))
      {
        bool bOk = false;
        int frame = file.readAll().toInt(&bOk);
        if (bOk)
        {
          progress.setValue(frame);
          progress.setLabelText(QString("Rendering frame %1 / %2").arg(frame).arg(frames));
        }
      } });
    timer.start(250);
    process.start(QCoreApplication::applicationDirPath() + "/ezPlayer", arguments);
    if (!process.waitForStarted(10000))
    {
      QMessageBox::warning(pParent, "Render Movie", "Cannot start ezPlayer: " + process.errorString());
      return;
    }
    progress.exec();
    timer.stop();
    if (process.state() != QProcess::NotRunning)
    {
      process.kill();
      process.waitForFinished(5000);
      return;
    }
    log += process.readAllStandardOutput();
    QFile movieLog(temporaryMovie + ".log");
    if (movieLog.open(QIODevice::ReadOnly))
      log += movieLog.readAll();
    if (bStartupTimeout)
      log += "\nTimed out waiting for the movie renderer to start.";
    if (!bSuccess)
    {
      QMessageBox::warning(pParent, "Render Movie", "The movie was not published. Check the encoder, Movies/MiniAudio plugin selection and destination permissions.\n\n" + QString::fromUtf8(log.right(6000)));
      return;
    }
    if (!QFile::rename(temporaryMovie, output))
    {
      temporary.setAutoRemove(false);
      QMessageBox::warning(pParent, "Render Movie", "Cannot publish the movie. The completed file is preserved at:\n" + temporaryMovie);
      return;
    }
    movieLog.close();
    QFile::copy(temporaryMovie + ".log", output + ".log");
    QMessageBox::information(pParent, "Render Movie", "Movie saved to:\n" + output);
  }
};

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezRenderMovieAction, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;

static ezActionDescriptorHandle s_hRenderMovie;
EZ_PLUGIN_ON_LOADED()
{
  s_hRenderMovie = EZ_REGISTER_ACTION_0("Render Movie", ezActionScope::Document, "Scene", "", ezRenderMovieAction);
  for (const char* szMap : {"EditorPluginScene_DocumentToolBar", "EditorPluginScene_Scene2ToolBar"})
  {
    if (auto* pMap = ezActionMapManager::GetActionMap(szMap))
      pMap->MapAction(s_hRenderMovie, "SceneCategory", 3.5f);
  }
}
EZ_PLUGIN_ON_UNLOADED()
{
  ezActionManager::UnregisterAction(s_hRenderMovie);
}
