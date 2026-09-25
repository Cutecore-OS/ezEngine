#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorFramework/Assets/AssetStatusIndicator.moc.h>
#include <EditorFramework/DocumentWindow/OrbitCamViewWidget.moc.h>
#include <EditorFramework/InputContexts/OrbitCameraContext.h>
#include <EditorPluginAssets/AnimationClipAsset/AnimationClipAssetWindow.moc.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <Foundation/Algorithm/HashingUtils.h>
#include <GuiFoundation/ActionViews/MenuBarActionMapView.moc.h>
#include <GuiFoundation/ActionViews/ToolBarActionMapView.moc.h>
#include <GuiFoundation/DockPanels/DocumentPanel.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyGridWidget.moc.h>
#include <GuiFoundation/Widgets/EventTrackEditorWidget.moc.h>
#include <GuiFoundation/Widgets/TimeScrubberWidget.moc.h>
#include <ToolsFoundation/Object/ObjectCommandAccessor.h>
#include <QListWidget>
#include <QSignalBlocker>
#include <QSplitter>
#include <RendererCore/AnimationSystem/BlendShapeResource.h>

namespace
{
  QWidget* CreateNamedCurvePanel(ezQtCurve1DEditorWidget* editor, QListWidget*& names,
    ezCurveGroupData& curves, const char* objectName)
  {
    auto* splitter = new QSplitter(Qt::Horizontal, editor->parentWidget());
    names = new QListWidget(splitter);
    names->setObjectName(objectName);
    names->setSelectionMode(QAbstractItemView::ExtendedSelection);
    names->setMinimumWidth(130);
    names->setToolTip("Select a curve by name to select its control points. Ctrl selects multiple curves.");
    splitter->addWidget(names);
    splitter->addWidget(editor);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({190, 650});
    QObject::connect(names, &QListWidget::itemSelectionChanged, editor, [editor, names, &curves]()
    {
      {
        QSignalBlocker block(editor->CurveEdit);
        editor->CurveEdit->ClearSelection();
        for (auto* item : names->selectedItems())
        {
          const auto curve = static_cast<ezUInt32>(names->row(item));
          if (curve >= curves.m_Curves.GetCount())
            continue;
          for (ezUInt32 point = 0; point < curves.m_Curves[curve]->m_ControlPoints.GetCount(); ++point)
            editor->CurveEdit->SetSelected({static_cast<ezUInt16>(curve), static_cast<ezUInt16>(point)}, true);
        }
      }
      editor->CurveEdit->SelectionChangedEvent();
    });
    QObject::connect(editor->CurveEdit, &ezQtCurveEditWidget::SelectionChangedEvent, names, [editor, names]()
    {
      QSignalBlocker block(names);
      names->clearSelection();
      for (const auto& point : editor->CurveEdit->GetSelection())
        if (point.m_uiCurve < names->count())
          names->item(point.m_uiCurve)->setSelected(true);
    });
    QObject::connect(editor, &ezQtCurve1DEditorWidget::CpMovedEvent, names, [names](ezUInt32 curve, ezUInt32, ezInt64, double)
    {
      if (curve < static_cast<ezUInt32>(names->count()))
        names->scrollToItem(names->item(curve));
    });
    QObject::connect(editor->CurveEdit, &ezQtCurveEditWidget::MoveCurveEvent, names, [names](ezInt32 curve, double)
    {
      QSignalBlocker block(names);
      names->clearSelection();
      if (curve >= 0 && curve < names->count())
        names->item(curve)->setSelected(true);
    });
    return splitter;
  }

  void UpdateCurveNames(QListWidget* names, const ezDynamicArray<ezAnimationClipCurveData>& curves)
  {
    if (names == nullptr)
      return;
    QSignalBlocker block(names);
    while (names->count() > static_cast<int>(curves.GetCount()))
      delete names->takeItem(names->count() - 1);
    for (ezUInt32 i = 0; i < curves.GetCount(); ++i)
    {
      if (i == static_cast<ezUInt32>(names->count()))
        names->addItem(new QListWidgetItem());
      auto* item = names->item(i);
      const auto name = ezMakeQString(curves[i].m_sName);
      item->setText(name.isEmpty() ? QString("Curve %1").arg(i + 1) : name);
      item->setToolTip(item->text());
      QPixmap swatch(12, 12);
      const auto color = curves[i].m_Curve.m_CurveColor;
      swatch.fill(QColor(color.r, color.g, color.b));
      item->setIcon(QIcon(swatch));
    }
  }
}

ezQtAnimationClipAssetDocumentWindow::ezQtAnimationClipAssetDocumentWindow(ezAnimationClipAssetDocument* pDocument)
  : ezQtEngineDocumentWindow(pDocument)
  , m_Clock("AssetClip")
{
  // Menu Bar
  {
    ezQtMenuBarActionMapView* pMenuBar = static_cast<ezQtMenuBarActionMapView*>(menuBar());
    ezActionContext context;
    context.m_sMapping = "AnimationClipAssetMenuBar";
    context.m_pDocument = pDocument;
    context.m_pWindow = this;
    pMenuBar->SetActionContext(context);
  }

  // Tool Bar
  {
    ezQtToolBarActionMapView* pToolBar = new ezQtToolBarActionMapView("Toolbar", this);
    ezActionContext context;
    context.m_sMapping = "AnimationClipAssetToolBar";
    context.m_pDocument = pDocument;
    context.m_pWindow = this;
    pToolBar->SetActionContext(context);
    pToolBar->setObjectName("AnimationClipAssetWindowToolBar");
    addToolBar(pToolBar);
  }

  // 3D View
  ezQtViewWidgetContainer* pContainer = nullptr;
  {
    SetTargetFramerate(25);

    m_ViewConfig.m_Camera.LookAt(ezVec3(-1.6f, 0, 0), ezVec3(0, 0, 0), ezVec3(0, 0, 1));
    m_ViewConfig.ApplyPerspectiveSetting(90);

    m_pViewWidget = new ezQtOrbitCamViewWidget(this, &m_ViewConfig);
    m_pViewWidget->ConfigureRelative(ezVec3(0, 0, 1), ezVec3(5.0f), ezVec3(5, -2, 3), 2.0f);
    AddViewWidget(m_pViewWidget);
    pContainer = new ezQtViewWidgetContainer(GetContainerWindow()->GetDockManager(), this, m_pViewWidget, "AnimationClipAssetViewToolBar");
    m_pDockManager->setCentralWidget(pContainer);
  }

  // Property Grid
  {
    ezQtDocumentPanel* pPropertyPanel = new ezQtDocumentPanel(GetContainerWindow()->GetDockManager(), this, pDocument);
    pPropertyPanel->setObjectName("AnimationClipAssetDockWidget");
    pPropertyPanel->setWindowTitle("Animation Clip Properties");
    pPropertyPanel->show();

    ezQtPropertyGridWidget* pPropertyGrid = new ezQtPropertyGridWidget(pPropertyPanel, pDocument);

    QWidget* pWidget = new QWidget();
    pWidget->setObjectName("Group");
    pWidget->setLayout(new QVBoxLayout());
    pWidget->setContentsMargins(0, 0, 0, 0);

    pWidget->layout()->setContentsMargins(0, 0, 0, 0);
    pWidget->layout()->addWidget(new ezQtAssetStatusIndicator(GetDocument()));
    pWidget->layout()->addWidget(pPropertyGrid);

    pPropertyPanel->setWidget(pWidget, ads::CDockWidget::ForceNoScrollArea);

    m_pDockManager->addDockWidgetTab(ads::RightDockWidgetArea, pPropertyPanel);

    pDocument->GetSelectionManager()->SetSelection(pDocument->GetObjectManager()->GetRootObject()->GetChildren()[0]);
  }

  // Time Scrubber
  {
    m_pTimeScrubber = new ezQtTimeScrubberWidget(pContainer);
    m_pTimeScrubber->SetDuration(ezTime::MakeFromSeconds(1));

    pContainer->GetLayout()->addWidget(m_pTimeScrubber);

    connect(m_pTimeScrubber, &ezQtTimeScrubberWidget::ScrubberPosChangedEvent, this, &ezQtAnimationClipAssetDocumentWindow::OnScrubberPosChangedEvent);
  }

  // Event Track Panel
  {
    m_pEventTrackPanel = new ezQtDocumentPanel(GetContainerWindow()->GetDockManager(), this, pDocument);
    m_pEventTrackPanel->setObjectName("AnimClipEventTrackDockWidget");
    m_pEventTrackPanel->setWindowTitle("Event Track");
    m_pEventTrackPanel->show();

    m_pEventTrackEditor = new ezQtEventTrackEditorWidget(m_pEventTrackPanel);
    m_pEventTrackPanel->setWidget(m_pEventTrackEditor);

    m_pDockManager->addDockWidgetTab(ads::BottomDockWidgetArea, m_pEventTrackPanel);

    UpdateEventTrackEditor();
  }

  // Event track editor events
  {
    connect(m_pEventTrackEditor, &ezQtEventTrackEditorWidget::InsertCpEvent, this, &ezQtAnimationClipAssetDocumentWindow::onEventTrackInsertCpAt);
    connect(m_pEventTrackEditor, &ezQtEventTrackEditorWidget::CpMovedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onEventTrackCpMoved);
    connect(m_pEventTrackEditor, &ezQtEventTrackEditorWidget::CpDeletedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onEventTrackCpDeleted);

    connect(m_pEventTrackEditor, &ezQtEventTrackEditorWidget::BeginOperationEvent, this, &ezQtAnimationClipAssetDocumentWindow::onEventTrackBeginOperation);
    connect(m_pEventTrackEditor, &ezQtEventTrackEditorWidget::EndOperationEvent, this, &ezQtAnimationClipAssetDocumentWindow::onEventTrackEndOperation);
    connect(m_pEventTrackEditor, &ezQtEventTrackEditorWidget::BeginCpChangesEvent, this, &ezQtAnimationClipAssetDocumentWindow::onEventTrackBeginCpChanges);
    connect(m_pEventTrackEditor, &ezQtEventTrackEditorWidget::EndCpChangesEvent, this, &ezQtAnimationClipAssetDocumentWindow::onEventTrackEndCpChanges);
  }

  // curve editor
  {
    m_pCurveEditPanel = new ezQtDocumentPanel(GetContainerWindow()->GetDockManager(), this, pDocument);
    m_pCurveEditPanel->setObjectName("AnimClipCustomCurvesPanel");
    m_pCurveEditPanel->setWindowTitle("Curves");
    m_pCurveEditPanel->show();

    m_pCurveEditor = new ezQtCurve1DEditorWidget(this);
    m_pCurveEditPanel->setWidget(CreateNamedCurvePanel(m_pCurveEditor, m_pCurveNames, m_Curves, "AnimationCurveNames"));

    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::InsertCpEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveInsertCpAt);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::CpMovedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveCpMoved);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::CpDeletedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveCpDeleted);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::TangentMovedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveTangentMoved);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::TangentLinkEvent, this, &ezQtAnimationClipAssetDocumentWindow::onLinkCurveTangents);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::CpTangentModeEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveTangentModeChanged);

    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::BeginOperationEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveBeginOperation);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::EndOperationEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveEndOperation);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::BeginCpChangesEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveBeginCpChanges);
    connect(m_pCurveEditor, &ezQtCurve1DEditorWidget::EndCpChangesEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveEndCpChanges);

    m_pDockManager->addDockWidgetTab(ads::BottomDockWidgetArea, m_pCurveEditPanel);

    UpdateCurveEditor();
  }

  // Morph tracks use the native curve editor and document transactions.
  {
    m_pBlendShapePanel = new ezQtDocumentPanel(GetContainerWindow()->GetDockManager(), this, pDocument);
    m_pBlendShapePanel->setObjectName("AnimClipBlendShapeCurvesPanel");
    m_pBlendShapePanel->setWindowTitle("Blend Shapes");
    m_pBlendShapePanel->show();

    m_pBlendShapeEditor = new ezQtCurve1DEditorWidget(this);
    m_pBlendShapePanel->setWidget(CreateNamedCurvePanel(m_pBlendShapeEditor, m_pBlendShapeNames, m_BlendShapeCurves, "BlendShapeCurveNames"));

    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::InsertCpEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveInsertCpAt);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::CpMovedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveCpMoved);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::CpDeletedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveCpDeleted);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::TangentMovedEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveTangentMoved);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::TangentLinkEvent, this, &ezQtAnimationClipAssetDocumentWindow::onLinkCurveTangents);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::CpTangentModeEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveTangentModeChanged);

    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::BeginOperationEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveBeginOperation);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::EndOperationEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveEndOperation);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::BeginCpChangesEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveBeginCpChanges);
    connect(m_pBlendShapeEditor, &ezQtCurve1DEditorWidget::EndCpChangesEvent, this, &ezQtAnimationClipAssetDocumentWindow::onCurveEndCpChanges);

    if (auto* pArea = m_pEventTrackPanel->dockAreaWidget())
      m_pDockManager->addDockWidgetTabToArea(m_pBlendShapePanel, pArea);
    else
      m_pDockManager->addDockWidgetTab(ads::BottomDockWidgetArea, m_pBlendShapePanel);

    UpdateCurveEditor();
  }

  FinishWindowCreation();

  GetAnimationClipDocument()->m_CommonAssetUiChangeEvent.AddEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::CommonAssetUiEventHandler, this));
  GetDocument()->GetCommandHistory()->m_Events.AddEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::CommandHistoryEventHandler, this));
  pDocument->GetObjectManager()->m_StructureEvents.AddEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::StructureEventHandler, this));
  pDocument->GetObjectManager()->m_PropertyEvents.AddEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::BlendShapePropertyEventHandler, this));
  ScheduleBlendShapeRefresh();
}

ezQtAnimationClipAssetDocumentWindow::~ezQtAnimationClipAssetDocumentWindow()
{
  GetAnimationClipDocument()->m_CommonAssetUiChangeEvent.RemoveEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::CommonAssetUiEventHandler, this));
  GetDocument()->GetCommandHistory()->m_Events.RemoveEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::CommandHistoryEventHandler, this));
  GetDocument()->GetObjectManager()->m_StructureEvents.RemoveEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::StructureEventHandler, this));
  GetDocument()->GetObjectManager()->m_PropertyEvents.RemoveEventHandler(ezMakeDelegate(&ezQtAnimationClipAssetDocumentWindow::BlendShapePropertyEventHandler, this));
}

ezAnimationClipAssetDocument* ezQtAnimationClipAssetDocumentWindow::GetAnimationClipDocument()
{
  return static_cast<ezAnimationClipAssetDocument*>(GetDocument());
}

void ezQtAnimationClipAssetDocumentWindow::ExtractRootMotionFromFeet()
{
  ezSimpleDocumentConfigMsgToEngine msg;
  msg.m_sWhatToDo = "ExtractRootMotionFromFeet";

  GetDocument()->SendMessageToEngine(&msg);
}

void ezQtAnimationClipAssetDocumentWindow::SendRedrawMsg()
{
  // do not try to redraw while the process is crashed, it is obviously futile
  if (ezEditorEngineProcessConnection::GetSingleton()->IsProcessCrashed())
    return;

  {
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "PlaybackPos";
    msg.m_PayloadValue = (double)(m_PlaybackPosition.GetSeconds() / m_ClipDuration.GetSeconds());
    GetDocument()->SendMessageToEngine(&msg);
  }

  {
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "PreviewMesh";
    msg.m_sPayload = GetAnimationClipDocument()->GetProperties()->m_sPreviewMesh;
    GetDocument()->SendMessageToEngine(&msg);
  }
  {
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "PreviewAnim";
    msg.m_sPayload = GetAnimationClipDocument()->GetProperties()->m_sPreviewAnim;
    GetDocument()->SendMessageToEngine(&msg);
  }

  {
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "SimulationSpeed";

    if (GetAnimationClipDocument()->GetCommonAssetUiState(ezCommonAssetUiState::Pause) != 0.0f)
      msg.m_PayloadValue = 0.0;
    else
      msg.m_PayloadValue = GetAnimationClipDocument()->GetCommonAssetUiState(ezCommonAssetUiState::SimulationSpeed);

    GetEditorEngineConnection()->SendMessage(&msg);
  }

  // Preview the in-memory curves; dragging never saves or transforms the asset.
  {
    ezVariantDictionary weights;
    const auto& shapes = GetAnimationClipDocument()->GetProperties()->m_BlendShapes;
    for (ezUInt32 i = 0; i < ezMath::Min(shapes.GetCount(), m_BlendShapePreviewCurves.GetCount()); ++i)
    {
      ezStringBuilder name;
      ezBlendShapeResourceDescriptor::MakeCurveName(shapes[i].m_sName, name);
      weights.Insert(name, static_cast<float>(m_BlendShapePreviewCurves[i].Evaluate(m_PlaybackPosition.GetSeconds())));
    }
    ezSimpleDocumentConfigMsgToEngine msg;
    msg.m_sWhatToDo = "BlendShapePreview";
    msg.m_PayloadValue = weights;
    GetDocument()->SendMessageToEngine(&msg);
  }

  for (auto pView : m_ViewWidgets)
  {
    pView->SetEnablePicking(false);
    pView->UpdateCameraInterpolation();
    pView->SyncToEngine();
  }

  QueryObjectBBox();
}

void ezQtAnimationClipAssetDocumentWindow::QueryObjectBBox(ezInt32 iPurpose /*= 0*/)
{
  ezQuerySelectionBBoxMsgToEngine msg;
  msg.m_uiViewID = 0xFFFFFFFF;
  msg.m_iPurpose = iPurpose;
  GetDocument()->SendMessageToEngine(&msg);
}

void ezQtAnimationClipAssetDocumentWindow::UpdateEventTrackEditor()
{
  auto* pDoc = GetAnimationClipDocument();

  m_pEventTrackEditor->SetData(pDoc->GetProperties()->m_EventTrack, m_ClipDuration.GetSeconds());
}

static ezColorGammaUB GetColorForCurveName(ezStringView sName, ezInt32 iOffset)
{
  return ezColorScheme::LightUI(static_cast<ezColorScheme::Enum>((iOffset + ezHashingUtils::StringHash(sName)) % ezColorScheme::Count));
}

void ezQtAnimationClipAssetDocumentWindow::UpdateCurveEditor()
{
  auto* pDoc = GetAnimationClipDocument();

  m_Curves.Clear();
  m_Curves.m_bOwnsData = false;

  auto& curves = pDoc->GetProperties()->m_Curves;

  ezInt32 iOffset = 0;
  for (auto& namedCurve : curves)
  {
    namedCurve.m_Curve.m_CurveColor = GetColorForCurveName(namedCurve.m_sName, iOffset);
    m_Curves.m_Curves.PushBack(&namedCurve.m_Curve);
    ++iOffset;
  }

  m_pCurveEditor->SetCurveExtents(0.0f, m_ClipDuration.GetSeconds(), true, true);
  m_pCurveEditor->SetCurves(m_Curves);
  UpdateCurveNames(m_pCurveNames, curves);
  if (m_pBlendShapeEditor)
  {
    m_BlendShapeCurves.Clear();
    m_BlendShapeCurves.m_bOwnsData = false;
    ezInt32 iShapeColor = 0;
    for (auto& curve : pDoc->GetProperties()->m_BlendShapes)
    {
      curve.m_Curve.m_CurveColor = GetColorForCurveName(curve.m_sName, iShapeColor++);
      m_BlendShapeCurves.m_Curves.PushBack(&curve.m_Curve);
    }
    m_pBlendShapeEditor->SetCurveExtents(0.0f, m_ClipDuration.GetSeconds(), true, true);
    m_pBlendShapeEditor->SetCurves(m_BlendShapeCurves);
    UpdateCurveNames(m_pBlendShapeNames, pDoc->GetProperties()->m_BlendShapes);
    m_BlendShapePreviewCurves.SetCount(m_BlendShapeCurves.m_Curves.GetCount());
    for (ezUInt32 i = 0; i < m_BlendShapePreviewCurves.GetCount(); ++i)
    {
      auto& curve = m_BlendShapePreviewCurves[i];
      m_BlendShapeCurves.ConvertToRuntimeData(i, curve);
      curve.SortControlPoints();
      curve.CreateLinearApproximation();
    }
  }
}

void ezQtAnimationClipAssetDocumentWindow::InternalRedraw()
{
  if (m_pTimeScrubber == nullptr)
    return;

  if (m_ClipDuration.IsPositive())
  {
    m_Clock.Update();

    const double fSpeed = GetAnimationClipDocument()->GetCommonAssetUiState(ezCommonAssetUiState::SimulationSpeed);

    if (GetAnimationClipDocument()->GetCommonAssetUiState(ezCommonAssetUiState::Pause) == 0)
    {
      m_PlaybackPosition += m_Clock.GetTimeDiff() * fSpeed;
    }

    if (m_PlaybackPosition > m_ClipDuration)
    {
      if (GetAnimationClipDocument()->GetCommonAssetUiState(ezCommonAssetUiState::Loop) != 0)
      {
        m_PlaybackPosition -= m_ClipDuration;
      }
      else
      {
        m_PlaybackPosition = m_ClipDuration;
      }
    }
  }

  m_PlaybackPosition = ezMath::Clamp(m_PlaybackPosition, ezTime::MakeZero(), m_ClipDuration);
  m_pTimeScrubber->SetScrubberPosition(m_PlaybackPosition);
  m_pEventTrackEditor->SetScrubberPosition(m_PlaybackPosition);
  m_pCurveEditor->SetScrubberPosition(m_PlaybackPosition);
  if (m_pBlendShapeEditor)
    m_pBlendShapeEditor->SetScrubberPosition(m_PlaybackPosition);

  ezEditorInputContext::UpdateActiveInputContext();
  SendRedrawMsg();
  ezQtEngineDocumentWindow::InternalRedraw();
}

void ezQtAnimationClipAssetDocumentWindow::ProcessMessageEventHandler(const ezEditorEngineDocumentMsg* pMsg0)
{
  if (auto pMsg = ezDynamicCast<const ezQuerySelectionBBoxResultMsgToEditor*>(pMsg0))
  {
    const ezQuerySelectionBBoxResultMsgToEditor* pMessage = static_cast<const ezQuerySelectionBBoxResultMsgToEditor*>(pMsg);

    if (pMessage->m_vCenter.IsValid() && pMessage->m_vHalfExtents.IsValid())
    {
      m_pViewWidget->SetOrbitVolume(pMessage->m_vCenter, pMessage->m_vHalfExtents.CompMax(ezVec3(0.1f)));
    }
    else
    {
      // try again
      QueryObjectBBox(pMessage->m_iPurpose);
    }

    return;
  }

  if (auto pMsg = ezDynamicCast<const ezSimpleDocumentConfigMsgToEditor*>(pMsg0))
  {
    if (pMsg->m_sWhatToDo == "ClipDuration")
    {
      const ezTime newDuration = pMsg->m_PayloadValue.Get<ezTime>();

      if (m_ClipDuration != newDuration)
      {
        m_ClipDuration = newDuration;

        m_pTimeScrubber->SetDuration(m_ClipDuration);

        UpdateEventTrackEditor();
        UpdateCurveEditor();

        m_pEventTrackEditor->FrameCurve();
        m_pCurveEditor->FrameCurve();
        if (m_pBlendShapeEditor)
          m_pBlendShapeEditor->FrameCurve();
      }

      return;
    }
    else if (pMsg->m_sWhatToDo == "ExtractRootMotionFromFeet")
    {
      const ezVec4 vResult = pMsg->m_PayloadValue.Get<ezVec4>();

      auto pPropObj = GetAnimationClipDocument()->GetPropertyObject();

      ezObjectCommandAccessor acc(GetAnimationClipDocument()->GetCommandHistory());
      acc.StartTransaction("Extract Root Motion From Feet");

      acc.SetValueByName(pPropObj, "RootMotion", (ezInt32)ezRootMotionSource::Constant).AssertSuccess();
      acc.SetValueByName(pPropObj, "ConstantRootMotion", vResult.GetAsVec3()).AssertSuccess();
      acc.SetValueByName(pPropObj, "RootMotionDistance", vResult.w).AssertSuccess();

      acc.FinishTransaction();

      GetAnimationClipDocument()->GetCommandHistory();

      return;
    }
    else if (pMsg->m_sWhatToDo == "ReportError")
    {
      QString text = ezMakeQString(pMsg->m_sPayload);
      QTimer::singleShot(1, [=]()
        {
          // we have to break out of this callback, and execute the UI stuff on the main thread
          ezQtUiServices::MessageBoxInformation(ezFmt(text.toUtf8().data()));
          //
        });
      return;
    }
  }

  ezQtEngineDocumentWindow::ProcessMessageEventHandler(pMsg0);
}

void ezQtAnimationClipAssetDocumentWindow::CommonAssetUiEventHandler(const ezCommonAssetUiState& e)
{
  ezQtEngineDocumentWindow::CommonAssetUiEventHandler(e);

  if (e.m_State == ezCommonAssetUiState::Restart)
  {
    m_PlaybackPosition = ezTime::MakeFromSeconds(-1);
  }
}

void ezQtAnimationClipAssetDocumentWindow::OnScrubberPosChangedEvent(ezUInt64 uiNewScrubberTickPos)
{
  if (m_pTimeScrubber == nullptr || m_ClipDuration.IsZeroOrNegative())
    return;

  m_PlaybackPosition = ezTime::MakeFromSeconds(uiNewScrubberTickPos / 4800.0);
}

void ezQtAnimationClipAssetDocumentWindow::onEventTrackInsertCpAt(ezInt64 tickX, QString value)
{
  auto* pDoc = GetAnimationClipDocument();
  pDoc->InsertEventTrackCpAt(tickX, value.toUtf8().data());
}

void ezQtAnimationClipAssetDocumentWindow::onEventTrackCpMoved(ezUInt32 cpIdx, ezInt64 iTickX)
{
  iTickX = ezMath::Max<ezInt64>(iTickX, 0);

  auto* pDoc = GetAnimationClipDocument();

  ezObjectCommandAccessor accessor(pDoc->GetCommandHistory());

  const ezAbstractProperty* pTrackProp = ezGetStaticRTTI<ezAnimationClipAssetProperties>()->FindPropertyByName("EventTrack");
  const ezUuid trackGuid = accessor.Get<ezUuid>(pDoc->GetPropertyObject(), pTrackProp);
  const ezDocumentObject* pTrackObj = accessor.GetObject(trackGuid);

  const ezVariant cpGuid = pTrackObj->GetTypeAccessor().GetValue("ControlPoints", cpIdx);

  ezSetObjectPropertyCommand cmdSet;
  cmdSet.m_Object = cpGuid.Get<ezUuid>();

  cmdSet.m_sProperty = "Tick";
  cmdSet.m_NewValue = iTickX;
  pDoc->GetCommandHistory()->AddCommand(cmdSet).AssertSuccess();
}

void ezQtAnimationClipAssetDocumentWindow::onEventTrackCpDeleted(ezUInt32 cpIdx)
{
  auto* pDoc = GetAnimationClipDocument();

  ezObjectCommandAccessor accessor(pDoc->GetCommandHistory());

  const ezAbstractProperty* pTrackProp = ezGetStaticRTTI<ezAnimationClipAssetProperties>()->FindPropertyByName("EventTrack");
  const ezUuid trackGuid = accessor.Get<ezUuid>(pDoc->GetPropertyObject(), pTrackProp);
  const ezDocumentObject* pTrackObj = accessor.GetObject(trackGuid);

  const ezVariant cpGuid = pTrackObj->GetTypeAccessor().GetValue("ControlPoints", cpIdx);

  if (!cpGuid.IsValid())
    return;

  ezRemoveObjectCommand cmdSet;
  cmdSet.m_Object = cpGuid.Get<ezUuid>();
  pDoc->GetCommandHistory()->AddCommand(cmdSet).AssertSuccess();
}

void ezQtAnimationClipAssetDocumentWindow::onEventTrackBeginOperation(QString name)
{
  ezCommandHistory* history = GetDocument()->GetCommandHistory();
  history->BeginTemporaryCommands("Modify Events");
}

void ezQtAnimationClipAssetDocumentWindow::onEventTrackEndOperation(bool commit)
{
  ezCommandHistory* history = GetDocument()->GetCommandHistory();

  if (commit)
    history->FinishTemporaryCommands();
  else
    history->CancelTemporaryCommands();
}

void ezQtAnimationClipAssetDocumentWindow::onEventTrackBeginCpChanges(QString name)
{
  GetDocument()->GetCommandHistory()->StartTransaction(name.toUtf8().data());
}

void ezQtAnimationClipAssetDocumentWindow::onEventTrackEndCpChanges()
{
  GetDocument()->GetCommandHistory()->FinishTransaction();

  UpdateEventTrackEditor();
}

/// Returns the ezSingleCurveData document object for Curves[uiCurveIdx].m_Curve
const char* ezQtAnimationClipAssetDocumentWindow::GetEditedCurveProperty() const
{
  return sender() == m_pBlendShapeEditor ? "BlendShapes" : "Curves";
}

static const ezDocumentObject* GetCurveSubObject(ezAnimationClipAssetDocument* pDoc, ezUInt32 uiCurveIdx, const char* szProperty)
{
  const ezVariant namedCurveGuid = pDoc->GetPropertyObject()->GetTypeAccessor().GetValue(szProperty, uiCurveIdx);
  const ezDocumentObject* pNamedCurve = pDoc->GetObjectManager()->GetObject(namedCurveGuid.Get<ezUuid>());
  if (ezStringUtils::IsEqual(szProperty, "BlendShapes"))
    pDoc->GetObjectAccessor()->SetValueByName(pNamedCurve, "OverrideSource", true).AssertSuccess();
  const ezVariant curveGuid = pNamedCurve->GetTypeAccessor().GetValue("Curve");
  return pDoc->GetObjectManager()->GetObject(curveGuid.Get<ezUuid>());
}

void ezQtAnimationClipAssetDocumentWindow::onCurveInsertCpAt(ezUInt32 uiCurveIdx, ezInt64 tickX, double newPosY)
{
  auto* pDoc = static_cast<ezAnimationClipAssetDocument*>(GetDocument());

  ezCommandHistory* history = pDoc->GetCommandHistory();

  // If there is no curve at uiCurveIdx yet, add a new named curve entry
  while (pDoc->GetPropertyObject()->GetTypeAccessor().GetCount(GetEditedCurveProperty()) <= static_cast<ezInt32>(uiCurveIdx))
  {
    ezAddObjectCommand cmdAdd;
    cmdAdd.m_Parent = pDoc->GetPropertyObject()->GetGuid();
    cmdAdd.m_sParentProperty = GetEditedCurveProperty();
    cmdAdd.m_pType = ezGetStaticRTTI<ezAnimationClipCurveData>();
    cmdAdd.m_Index = -1;
    cmdAdd.m_NewObjectGuid = ezUuid::MakeUuid();
    history->AddCommand(cmdAdd).AssertSuccess();
  }

  const ezDocumentObject* pCurveObj = GetCurveSubObject(pDoc, uiCurveIdx, GetEditedCurveProperty());

  ezAddObjectCommand cmdAdd;
  cmdAdd.m_Parent = pCurveObj->GetGuid();
  cmdAdd.m_NewObjectGuid = ezUuid::MakeUuid();
  cmdAdd.m_sParentProperty = "ControlPoints";
  cmdAdd.m_pType = ezGetStaticRTTI<ezCurveControlPointData>();
  cmdAdd.m_Index = -1;

  history->AddCommand(cmdAdd).AssertSuccess();

  ezSetObjectPropertyCommand cmdSet;
  cmdSet.m_Object = cmdAdd.m_NewObjectGuid;

  cmdSet.m_sProperty = "Tick";
  cmdSet.m_NewValue = tickX;
  history->AddCommand(cmdSet).AssertSuccess();

  cmdSet.m_sProperty = "Value";
  cmdSet.m_NewValue = newPosY;
  history->AddCommand(cmdSet).AssertSuccess();

  cmdSet.m_sProperty = "LeftTangent";
  cmdSet.m_NewValue = ezVec2(-0.1f, 0.0f);
  history->AddCommand(cmdSet).AssertSuccess();

  cmdSet.m_sProperty = "RightTangent";
  cmdSet.m_NewValue = ezVec2(+0.1f, 0.0f);
  history->AddCommand(cmdSet).AssertSuccess();
}

void ezQtAnimationClipAssetDocumentWindow::onCurveCpMoved(ezUInt32 curveIdx, ezUInt32 cpIdx, ezInt64 iTickX, double newPosY)
{
  iTickX = ezMath::Max<ezInt64>(iTickX, 0);

  auto* pDoc = static_cast<ezAnimationClipAssetDocument*>(GetDocument());

  const ezDocumentObject* pCurveObj = GetCurveSubObject(pDoc, curveIdx, GetEditedCurveProperty());
  const ezVariant cpGuid = pCurveObj->GetTypeAccessor().GetValue("ControlPoints", cpIdx);

  ezSetObjectPropertyCommand cmdSet;
  cmdSet.m_Object = cpGuid.Get<ezUuid>();

  cmdSet.m_sProperty = "Tick";
  cmdSet.m_NewValue = iTickX;
  GetDocument()->GetCommandHistory()->AddCommand(cmdSet).AssertSuccess();

  cmdSet.m_sProperty = "Value";
  cmdSet.m_NewValue = newPosY;
  GetDocument()->GetCommandHistory()->AddCommand(cmdSet).AssertSuccess();
}


void ezQtAnimationClipAssetDocumentWindow::onCurveCpDeleted(ezUInt32 curveIdx, ezUInt32 cpIdx)
{
  auto* pDoc = static_cast<ezAnimationClipAssetDocument*>(GetDocument());

  const ezDocumentObject* pCurveObj = GetCurveSubObject(pDoc, curveIdx, GetEditedCurveProperty());
  const ezVariant cpGuid = pCurveObj->GetTypeAccessor().GetValue("ControlPoints", cpIdx);

  if (!cpGuid.IsValid())
    return;

  ezRemoveObjectCommand cmdSet;
  cmdSet.m_Object = cpGuid.Get<ezUuid>();
  GetDocument()->GetCommandHistory()->AddCommand(cmdSet).AssertSuccess();
}


void ezQtAnimationClipAssetDocumentWindow::onCurveTangentMoved(ezUInt32 curveIdx, ezUInt32 cpIdx, float newPosX, float newPosY, bool rightTangent)
{
  auto* pDoc = static_cast<ezAnimationClipAssetDocument*>(GetDocument());

  const ezDocumentObject* pCurveObj = GetCurveSubObject(pDoc, curveIdx, GetEditedCurveProperty());
  const ezVariant cpGuid = pCurveObj->GetTypeAccessor().GetValue("ControlPoints", cpIdx);

  ezSetObjectPropertyCommand cmdSet;
  cmdSet.m_Object = cpGuid.Get<ezUuid>();

  // clamp tangents to one side
  if (rightTangent)
    newPosX = ezMath::Max(newPosX, 0.0f);
  else
    newPosX = ezMath::Min(newPosX, 0.0f);

  cmdSet.m_sProperty = rightTangent ? "RightTangent" : "LeftTangent";
  cmdSet.m_NewValue = ezVec2(newPosX, newPosY);
  GetDocument()->GetCommandHistory()->AddCommand(cmdSet).AssertSuccess();
}


void ezQtAnimationClipAssetDocumentWindow::onLinkCurveTangents(ezUInt32 curveIdx, ezUInt32 cpIdx, bool bLink)
{
  auto* pDoc = static_cast<ezAnimationClipAssetDocument*>(GetDocument());

  const ezDocumentObject* pCurveObj = GetCurveSubObject(pDoc, curveIdx, GetEditedCurveProperty());
  const ezVariant cpGuid = pCurveObj->GetTypeAccessor().GetValue("ControlPoints", cpIdx);

  ezSetObjectPropertyCommand cmdLink;
  cmdLink.m_Object = cpGuid.Get<ezUuid>();
  cmdLink.m_sProperty = "Linked";
  cmdLink.m_NewValue = bLink;
  GetDocument()->GetCommandHistory()->AddCommand(cmdLink).AssertSuccess();

  if (bLink)
  {
    const auto& curves = sender() == m_pBlendShapeEditor ? pDoc->GetProperties()->m_BlendShapes : pDoc->GetProperties()->m_Curves;
    const ezVec2 leftTangent = curves[curveIdx].m_Curve.m_ControlPoints[cpIdx].m_LeftTangent;
    const ezVec2 rightTangent = -leftTangent;

    onCurveTangentMoved(curveIdx, cpIdx, rightTangent.x, rightTangent.y, true);
  }
}


void ezQtAnimationClipAssetDocumentWindow::onCurveTangentModeChanged(ezUInt32 curveIdx, ezUInt32 cpIdx, bool rightTangent, int mode)
{
  auto* pDoc = static_cast<ezAnimationClipAssetDocument*>(GetDocument());

  const ezDocumentObject* pCurveObj = GetCurveSubObject(pDoc, curveIdx, GetEditedCurveProperty());
  const ezVariant cpGuid = pCurveObj->GetTypeAccessor().GetValue("ControlPoints", cpIdx);

  ezSetObjectPropertyCommand cmd;
  cmd.m_Object = cpGuid.Get<ezUuid>();
  cmd.m_sProperty = rightTangent ? "RightTangentMode" : "LeftTangentMode";
  cmd.m_NewValue = mode;
  GetDocument()->GetCommandHistory()->AddCommand(cmd).AssertSuccess();
}


void ezQtAnimationClipAssetDocumentWindow::onCurveBeginOperation(QString name)
{
  ezCommandHistory* history = GetDocument()->GetCommandHistory();
  history->BeginTemporaryCommands(name.toUtf8().data());
}

void ezQtAnimationClipAssetDocumentWindow::onCurveEndOperation(bool commit)
{
  ezCommandHistory* history = GetDocument()->GetCommandHistory();

  if (commit)
    history->FinishTemporaryCommands();
  else
    history->CancelTemporaryCommands();

  UpdateCurveEditor();
}

void ezQtAnimationClipAssetDocumentWindow::onCurveBeginCpChanges(QString name)
{
  GetDocument()->GetCommandHistory()->StartTransaction(name.toUtf8().data());
}

void ezQtAnimationClipAssetDocumentWindow::onCurveEndCpChanges()
{
  GetDocument()->GetCommandHistory()->FinishTransaction();

  UpdateCurveEditor();
}

void ezQtAnimationClipAssetDocumentWindow::OnAfterDocumentLayoutRestored()
{
  if (m_pBlendShapePanel && m_pBlendShapePanel->isClosed())
  {
    if (auto* pArea = m_pEventTrackPanel->dockAreaWidget())
      m_pDockManager->addDockWidgetTabToArea(m_pBlendShapePanel, pArea);
    else
      m_pDockManager->addDockWidgetTab(ads::BottomDockWidgetArea, m_pBlendShapePanel);
  }

  // ADS flags dock widgets not found in the saved layout as "unassigned" (closed, detached from all dock areas).
  // Re-add the panel to its default location.
  if (m_pCurveEditPanel->isClosed())
  {
    if (auto* pArea = m_pEventTrackPanel->dockAreaWidget())
    {
      m_pDockManager->addDockWidgetTabToArea(m_pCurveEditPanel, pArea);
    }
    else
    {
      m_pDockManager->addDockWidgetTab(ads::BottomDockWidgetArea, m_pCurveEditPanel);
    }
  }
}

void ezQtAnimationClipAssetDocumentWindow::StructureEventHandler(const ezDocumentObjectStructureEvent& e)
{
  switch (e.m_EventType)
  {
    case ezDocumentObjectStructureEvent::Type::AfterReset:
    case ezDocumentObjectStructureEvent::Type::AfterObjectAdded:
    case ezDocumentObjectStructureEvent::Type::AfterObjectRemoved:
    case ezDocumentObjectStructureEvent::Type::AfterObjectMoved2:
      UpdateCurveEditor();
      break;

    default:
      break;
  }
}

void ezQtAnimationClipAssetDocumentWindow::CommandHistoryEventHandler(const ezCommandHistoryEvent& e)
{
  // also listen to TransactionCanceled, which is sent when a no-op happens (e.g. asset transform with no change)
  // because the event track data object may still get replaced, and we have to get the new pointer
  if (e.m_Type == ezCommandHistoryEvent::Type::TransactionEnded || e.m_Type == ezCommandHistoryEvent::Type::UndoEnded ||
      e.m_Type == ezCommandHistoryEvent::Type::RedoEnded ||
      e.m_Type == ezCommandHistoryEvent::Type::TransactionCanceled)
  {
    UpdateEventTrackEditor();
    UpdateCurveEditor();
  }
}

void ezQtAnimationClipAssetDocumentWindow::ScheduleBlendShapeRefresh()
{
  if (m_bBlendShapeRefreshPending)
    return;
  m_bBlendShapeRefreshPending = true;
  QTimer::singleShot(0, this, [this]()
  {
    m_bBlendShapeRefreshPending = false;
    if (GetDocument()->GetCommandHistory()->IsInTransaction())
    {
      ScheduleBlendShapeRefresh();
      return;
    }
    const auto status = GetAnimationClipDocument()->RefreshBlendShapeCurves();
    if (status.Failed())
      ShowTemporaryStatusBarMsg(ezFmt("Blend Shapes: {}", status.GetMessageString()));
    UpdateCurveEditor();
  });
}

void ezQtAnimationClipAssetDocumentWindow::BlendShapePropertyEventHandler(const ezDocumentObjectPropertyEvent& e)
{
  if (e.m_pObject != GetAnimationClipDocument()->GetPropertyObject())
    return;
  if (e.m_sProperty == "File" || e.m_sProperty == "PreviewMesh" || e.m_sProperty == "UseAnimationClip" ||
      e.m_sProperty == "FirstFrame" || e.m_sProperty == "NumFrames" || e.m_sProperty == "ImportBlendShapes")
    ScheduleBlendShapeRefresh();
}
