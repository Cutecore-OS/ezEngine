#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/Assets/AssetStatusIndicator.moc.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyGridWidget.moc.h>
#include <GuiFoundation/DockPanels/DocumentPanel.moc.h>
#include <GuiFoundation/ContainerWindow/ContainerWindow.moc.h>
#include <ads/DockManager.h>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QToolBar>
#include <QVBoxLayout>

namespace
{
  class BlendShapeWindow : public ezQtDocumentWindow
  {
  public:
    BlendShapeWindow(ezBlendShapeAssetDocument* document)
      : ezQtDocumentWindow(document)
    {
      auto* toolbar = addToolBar("Asset");
      auto* save = toolbar->addAction("Save");
      save->setShortcut(QKeySequence::Save);
      connect(save, &QAction::triggered, this, [this]()
        { SaveDocument().LogFailure(); });
      auto* transform = toolbar->addAction("Transform");
      connect(transform, &QAction::triggered, this, [document]()
        { const auto status = ezAssetCurator::GetSingleton()->TransformAsset(document->GetGuid(), ezTransformFlags::TriggeredManually); if (status.Failed()) ezLog::Error("{}", status.m_sMessage); });
      auto* panel = new QWidget(this);
      auto* layout = new QVBoxLayout(panel);
      auto* description = new QLabel("Companion data for the Animated Mesh below. Edit weights with Blend Shape Pose Component; edit animations in the normal Animation Clip window.", panel);
      description->setWordWrap(true);
      layout->addWidget(description);
      layout->addWidget(new ezQtAssetStatusIndicator(document));
      auto* grid = new ezQtPropertyGridWidget(panel, document, false);
      ezDeque<const ezDocumentObject*> selection;
      selection.PushBack(document->GetPropertyObject());
      grid->SetSelection(selection);
      layout->addWidget(grid);
      auto* targets = new QTableWidget(panel);
      targets->setObjectName("BlendShapeTargets");
      targets->setColumnCount(2);
      targets->setHorizontalHeaderLabels({"Blend Shape", "Default Weight"});
      targets->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
      targets->setEditTriggers(QAbstractItemView::NoEditTriggers);
      ezDynamicArray<ezBlendShapeTarget> names;
      const auto status = ezGetMeshBlendShapeTargets(document->GetProperties()->m_sMesh, names);
      if (status.Succeeded())
      {
        targets->setRowCount(names.GetCount());
        for (ezUInt32 i = 0; i < names.GetCount(); ++i)
        {
          targets->setItem(i, 0, new QTableWidgetItem(ezMakeQString(names[i].m_sName.GetView())));
          targets->setItem(i, 1, new QTableWidgetItem(QString::number(names[i].m_fDefaultWeight)));
        }
      }
      else
        layout->addWidget(new QLabel(ezMakeQString(status.GetMessageString()), panel));
      layout->addWidget(targets);

      // The document window owns a CDockManager as its central widget. Replacing it
      // through QMainWindow deletes that manager after queued Qt events are processed.
      auto* dock = new ezQtDocumentPanel(GetContainerWindow()->GetDockManager(), this, document);
      dock->setObjectName("BlendShapeAssetDockWidget");
      dock->setWindowTitle("Blend Shapes");
      dock->setWidget(panel, ads::CDockWidget::ForceNoScrollArea);
      m_pDockManager->setCentralWidget(dock);
      FinishWindowCreation();
    }
  };
} // namespace

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezBlendShapeAssetManager, 1, ezRTTIDefaultAllocator<ezBlendShapeAssetManager>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

ezBlendShapeAssetManager::ezBlendShapeAssetManager()
{
  ezDocumentManager::s_Events.AddEventHandler(ezMakeDelegate(&ezBlendShapeAssetManager::OnDocumentEvent, this));
  m_Type.m_sDocumentTypeName = "Blend Shapes";
  m_Type.m_sFileExtension = "ezBlendShapeAsset";
  m_Type.m_sResourceFileExtension = "ezBinBlendShape";
  m_Type.m_sIcon = ":/AssetIcons/Animated_Mesh.svg";
  m_Type.m_sAssetCategory = "Animation";
  m_Type.m_pDocumentType = ezGetStaticRTTI<ezBlendShapeAssetDocument>();
  m_Type.m_pManager = this;
  m_Type.m_CompatibleTypes.PushBack("CompatibleAsset_BlendShape");
}
ezBlendShapeAssetManager::~ezBlendShapeAssetManager()
{
  ezDocumentManager::s_Events.RemoveEventHandler(ezMakeDelegate(&ezBlendShapeAssetManager::OnDocumentEvent, this));
}
void ezBlendShapeAssetManager::OnDocumentEvent(const ezDocumentManager::Event& e)
{
  if (e.m_Type == ezDocumentManager::Event::Type::DocumentWindowRequested && e.m_pDocument->GetDynamicRTTI() == ezGetStaticRTTI<ezBlendShapeAssetDocument>())
    new BlendShapeWindow(static_cast<ezBlendShapeAssetDocument*>(e.m_pDocument)); // NOLINT: owned by the document window framework.
}
void ezBlendShapeAssetManager::InternalCreateDocument(ezStringView, ezStringView path, bool, ezDocument*& document, const ezDocumentObject*)
{
  document = new ezBlendShapeAssetDocument(path);
}
void ezBlendShapeAssetManager::InternalGetSupportedDocumentTypes(ezDynamicArray<const ezDocumentTypeDescriptor*>& types) const
{
  types.PushBack(&m_Type);
}
