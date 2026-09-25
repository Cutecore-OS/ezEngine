#include <EditorPluginGreyBox/EditorPluginGreyBoxPCH.h>

#include <EditorFramework/Document/GameObjectDocument.h>
#include <EditorFramework/DocumentWindow/EngineDocumentWindow.moc.h>
#include <EditorFramework/EditTools/EditTool.h>
#include <EditorPluginGreyBox/GreyBoxPivot.h>
#include <EditorPluginGreyBox/GreyBoxPivotGizmo.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <EditorPluginGreyBox/GreyBoxVertexSnapping.h>
#include <Foundation/Strings/TranslationLookup.h>
#include <GuiFoundation/PropertyGrid/Implementation/PropertyWidget.moc.h>
#include <GuiFoundation/PropertyGrid/Implementation/TypeWidget.moc.h>
#include <GuiFoundation/PropertyGrid/ManipulatorManager.h>
#include <GuiFoundation/PropertyGrid/PropertyGridWidget.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyMetaState.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>
#include <GuiFoundation/Widgets/CollapsibleGroupBox.moc.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>
#include <ToolsFoundation/Reflection/PhantomRttiManager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
  // The Shape enum supplies an editor-only extension point without changing component reflection.
  // Once the type widget is built, attach our controls below its last property group (Misc).
  class ezQtGreyBoxToolsFeature : public ezQtGreyBoxPropertyFeature
  {
  public:
    using ezQtGreyBoxPropertyFeature::ezQtGreyBoxPropertyFeature;
    void OnBeforeChange() override { StopPicking(); }

    ~ezQtGreyBoxToolsFeature()
    {
      StopPicking();
      delete m_pControls;
    }

    void SetSelection(const ezArrayPtr<ezPropertySelection>& items) override
    {
      StopPicking();
      ezQtGreyBoxPropertyFeature::SetSelection(items);
      if (!m_pControls)
        return;

      for (QWidget* pParent = parentWidget(); pParent != nullptr; pParent = pParent->parentWidget())
      {
        if (auto pTypeWidget = qobject_cast<ezQtTypeWidget*>(pParent))
        {
          if (m_pControls->parentWidget() != pTypeWidget)
          {
            auto pLayout = qobject_cast<QGridLayout*>(pTypeWidget->layout());
            m_pControls->setParent(pTypeWidget);
            pLayout->addWidget(m_pControls, pLayout->rowCount(), 0, 1, 3);
            m_pGrid->SetCollapseState(qobject_cast<ezQtCollapsibleGroupBox*>(m_pControls.data()));
            m_pControls->show();
          }
          break;
        }
      }
      m_pPick->setEnabled(items.GetCount() == 1 && GetEngineWindow() != nullptr);
    }

    void SetReadOnly(bool bReadOnly = true) override
    {
      ezQtGreyBoxPropertyFeature::SetReadOnly(bReadOnly);
      if (m_pControls)
        m_pControls->setEnabled(!bReadOnly);
      if (bReadOnly)
        StopPicking();
    }

  protected:
    void OnInit() override
    {
      ezQtGreyBoxPropertyFeature::OnInit();
      if (m_pType->GetTypeName() != "ezGreyBoxComponent" && m_pType->GetTypeName() != "ezGreyBoxConeComponent")
        return;

      auto pTools = new ezQtCollapsibleGroupBox(this);
      pTools->SetTitle("Tools");
      connect(pTools, &ezQtGroupBoxBase::CollapseStateChanged, m_pGrid, &ezQtPropertyGridWidget::OnCollapseStateChanged);
      m_pControls = pTools;
      m_pControls->setObjectName("GreyBoxPivotControls");
      m_pControls->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
      auto pToolsLayout = new QVBoxLayout(pTools->GetContent());
      pToolsLayout->setContentsMargins(0, 0, 0, 0);

      auto pControlsLayout = new QHBoxLayout();
      pToolsLayout->addLayout(pControlsLayout);
      pControlsLayout->setContentsMargins(0, 4, 0, 0);
      auto pButton = new QPushButton("Recalculate Position", m_pControls);
      pButton->setToolTip("Move the object origin to the selected point without moving the Grey Boxing geometry. Supports Undo/Redo.");
      pControlsLayout->addWidget(pButton);
      m_pAnchor = new QComboBox(m_pControls);
      m_pAnchor->setObjectName("GreyBoxPivotAnchor");
      m_pAnchor->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
      m_pAnchor->setMinimumContentsLength(10);
      m_pAnchor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      m_pAnchor->setToolTip("Pivot point in the local bounding box. Coordinates are X / Y / Z.");
      pControlsLayout->addWidget(m_pAnchor, 1);
      m_pAnchor->addItem("Center", 13);
      m_pAnchor->addItem("Min corner", 0);
      m_pAnchor->addItem("Max corner", 26);
      const char* szCoordinates[] = {"Min", "Center", "Max"};
      for (int z = 0; z < 3; ++z)
      {
        for (int y = 0; y < 3; ++y)
        {
          for (int x = 0; x < 3; ++x)
          {
            const int iAnchor = x + 3 * y + 9 * z;
            if (iAnchor == 0 || iAnchor == 13 || iAnchor == 26)
              continue;
            const int iCenters = (x == 1) + (y == 1) + (z == 1);
            const char* szKind = iCenters == 2 ? "Face" : iCenters == 1 ? "Edge"
                                                                        : "Corner";
            m_pAnchor->addItem(QString("%1: %2 / %3 / %4").arg(szKind).arg(szCoordinates[x]).arg(szCoordinates[y]).arg(szCoordinates[z]), iAnchor);
          }
        }
      }
      m_pPick = new QToolButton(m_pControls);
      m_pPick->setObjectName("GreyBoxPickPivot");
      m_pPick->setIcon(QIcon(":/GuiFoundation/Icons/Cursor.svg"));
      m_pPick->setCheckable(true);
      m_pPick->setToolTip("Pick a pivot point in the viewport. Yellow: corners; purple: center; RGB: face and edge centers. Select one Grey Boxing component.");
      pControlsLayout->addWidget(m_pPick);

      connect(m_pPick, &QToolButton::toggled, this, [this](bool bChecked)
        {
          if (bChecked)
            StartPicking();
          else
            StopPicking(); });
      connect(m_pAnchor, &QComboBox::currentIndexChanged, this, [this](int)
        {
          if (m_pGizmo)
            m_pGizmo->SetSelectedAnchor(m_pAnchor->currentData().toUInt()); });
      connect(pButton, &QPushButton::clicked, this, [this]()
        {
          if (IsUndead())
            return;
          ezHybridArray<const ezDocumentObject*, 8> components;
          for (const auto& item : m_Items)
            components.PushBack(item.m_pObject);
          const ezVec3 vAnchor = ezGreyBoxPivotGizmo::GetAnchor(m_pAnchor->currentData().toUInt());
          const ezStatus status = ezRecalculateGreyBoxPivot(*m_pObjectAccessor, components, vAnchor);
          UpdateGizmo();
          ezQtUiServices::MessageBoxStatus(status, "Could not recalculate Grey Boxing position."); });
    }

    void DoPrepareToDie() override
    {
      StopPicking();
      if (m_pControls)
        m_pControls->setEnabled(false);
      ezQtGreyBoxPropertyFeature::DoPrepareToDie();
    }

  private:
    ezQtEngineDocumentWindow* GetEngineWindow() const
    {
      auto pDocument = m_pGrid->GetDocument();
      if (!pDocument->GetDynamicRTTI()->IsDerivedFrom<ezGameObjectDocument>())
        return nullptr;
      return qobject_cast<ezQtEngineDocumentWindow*>(ezQtDocumentWindow::FindWindowByDocument(pDocument->GetMainDocument()));
    }

    void StartPicking()
    {
      auto pWindow = GetEngineWindow();
      if (IsUndead() || m_Items.GetCount() != 1 || pWindow == nullptr)
      {
        StopPicking();
        return;
      }

      // Temporarily leave the transform tool so its handles cannot cover the center anchor.
      m_pToolDocument = ezDynamicCast<ezGameObjectDocument*>(pWindow->GetDocument());
      if (m_pToolDocument != nullptr && m_pToolDocument->GetActiveEditTool() != nullptr)
      {
        m_pPreviousEditTool = m_pToolDocument->GetActiveEditTool()->GetDynamicRTTI();
        m_pToolDocument->SetActiveEditTool(nullptr);
      }
      // Prevent the resize handles from overlapping the anchor handles.
      ezManipulatorManager::GetSingleton()->ClearActiveManipulator(m_pGrid->GetDocument());
      m_pGizmo = EZ_DEFAULT_NEW(ezGreyBoxPivotGizmo);
      m_pGizmo->SetOwner(pWindow, nullptr);
      m_pGizmo->SetSelectedAnchor(m_pAnchor->currentData().toUInt());
      m_pGizmo->m_GizmoEvents.AddEventHandler(ezMakeDelegate(&ezQtGreyBoxToolsFeature::GizmoEventHandler, this));
      m_pGrid->GetDocument()->GetSelectionManager()->m_Events.AddEventHandler(
        ezMakeDelegate(&ezQtGreyBoxToolsFeature::SelectionEventHandler, this), m_SelectionUnsubscriber);
      if (m_pGrid->GetDocument()->GetMainDocument() != m_pGrid->GetDocument())
        m_pGrid->GetDocument()->GetMainDocument()->GetSelectionManager()->m_Events.AddEventHandler(
          ezMakeDelegate(&ezQtGreyBoxToolsFeature::SelectionEventHandler, this), m_MainSelectionUnsubscriber);
      ezManipulatorManager::GetSingleton()->m_Events.AddEventHandler(
        ezMakeDelegate(&ezQtGreyBoxToolsFeature::ManipulatorEventHandler, this), m_ManipulatorUnsubscriber);
      ezQtDocumentWindow::s_Events.AddEventHandler(
        ezMakeDelegate(&ezQtGreyBoxToolsFeature::DocumentWindowEventHandler, this), m_WindowUnsubscriber);
      UpdateGizmo();
      if (m_pGizmo)
        m_pGizmo->SetVisible(true);
    }

    void StopPicking()
    {
      m_SelectionUnsubscriber.Unsubscribe();
      m_MainSelectionUnsubscriber.Unsubscribe();
      m_ManipulatorUnsubscriber.Unsubscribe();
      m_WindowUnsubscriber.Unsubscribe();
      if (m_pGizmo)
        m_pGizmo->SetVisible(false);
      m_pGizmo.Clear();
      if (m_pPreviousEditTool != nullptr && m_pToolDocument != nullptr && m_pToolDocument->GetActiveEditTool() == nullptr)
        m_pToolDocument->SetActiveEditTool(m_pPreviousEditTool);
      m_pPreviousEditTool = nullptr;
      m_pToolDocument = nullptr;
      if (m_pPick && m_pPick->isChecked())
      {
        const QSignalBlocker blocker(m_pPick);
        m_pPick->setChecked(false);
      }
    }

    void UpdateGizmo()
    {
      if (!m_pGizmo || m_Items.IsEmpty())
        return;
      if (m_pToolDocument != nullptr && m_pToolDocument->GetActiveEditTool() != nullptr)
      {
        StopPicking();
        return;
      }
      const auto* pObject = m_Items[0].m_pObject;
      ezVec3 vNegative, vPositive;
      const char* szNegative[] = {"SizeNegX", "SizeNegY", "SizeNegZ"};
      const char* szPositive[] = {"SizePosX", "SizePosY", "SizePosZ"};
      for (ezUInt32 i = 0; i < 3; ++i)
      {
        vNegative.GetData()[i] = m_pObjectAccessor->GetByName<float>(pObject, szNegative[i]);
        vPositive.GetData()[i] = m_pObjectAccessor->GetByName<float>(pObject, szPositive[i]);
      }
      if (!vNegative.IsValid() || !vPositive.IsValid())
      {
        StopPicking();
        return;
      }
      auto pDocument = static_cast<const ezGameObjectDocument*>(m_pGrid->GetDocument());
      m_pGizmo->SetBounds(vNegative, vPositive);
      m_pGizmo->SetTransformation(pDocument->GetGlobalTransform(pObject->GetParent()));
    }

    void GizmoEventHandler(const ezGizmoEvent& e)
    {
      if (e.m_Type == ezGizmoEvent::Type::Interaction)
        m_pAnchor->setCurrentIndex(m_pAnchor->findData(m_pGizmo->GetSelectedAnchor()));
    }

    void SelectionEventHandler(const ezSelectionManagerEvent& e) { StopPicking(); }

    void DocumentWindowEventHandler(const ezQtDocumentWindowEvent& e)
    {
      if (e.m_pWindow->GetDocument() != m_pGrid->GetDocument()->GetMainDocument())
        return;
      if (e.m_Type == ezQtDocumentWindowEvent::BeforeRedraw)
        UpdateGizmo();
      else if (e.m_Type == ezQtDocumentWindowEvent::WindowClosing)
      {
        m_pPreviousEditTool = nullptr;
        StopPicking();
      }
    }

    void ManipulatorEventHandler(const ezManipulatorManagerEvent& e)
    {
      if (e.m_pDocument == m_pGrid->GetDocument() && (e.m_pManipulator != nullptr || e.m_bHideManipulators))
        StopPicking();
    }





    QPointer<QWidget> m_pControls;
    QPointer<QComboBox> m_pAnchor;
    QPointer<QToolButton> m_pPick;
    ezGameObjectDocument* m_pToolDocument = nullptr;
    const ezRTTI* m_pPreviousEditTool = nullptr;
    ezEvent<const ezQtDocumentWindowEvent&>::Unsubscriber m_WindowUnsubscriber;
    ezUniquePtr<ezGreyBoxPivotGizmo> m_pGizmo;
    ezCopyOnBroadcastEvent<const ezSelectionManagerEvent&>::Unsubscriber m_SelectionUnsubscriber;
    ezCopyOnBroadcastEvent<const ezSelectionManagerEvent&>::Unsubscriber m_MainSelectionUnsubscriber;
    ezCopyOnBroadcastEvent<const ezManipulatorManagerEvent&>::Unsubscriber m_ManipulatorUnsubscriber;
  };


  ezQtGreyBoxPropertyFeature* CreateTools(ezQtGreyBoxCompositeWidget* host) { return new ezQtGreyBoxToolsFeature(host); }
}

EZ_PLUGIN_ON_LOADED()
{
  ezRegisterGreyBoxVertexSnapping();
  ezRegisterGreyBoxFeature(CreateTools);
}
EZ_PLUGIN_ON_UNLOADED()
{
  ezUnregisterGreyBoxFeature(CreateTools);
  ezUnregisterGreyBoxVertexSnapping();
}

