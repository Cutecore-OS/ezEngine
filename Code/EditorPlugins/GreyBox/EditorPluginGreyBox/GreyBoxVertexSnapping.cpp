#include <EditorPluginGreyBox/EditorPluginGreyBoxPCH.h>

#include <EditorFramework/Assets/AssetDocument.h>
#include <EditorFramework/Document/GameObjectDocument.h>
#include <EditorFramework/DocumentWindow/EngineDocumentWindow.moc.h>
#include <EditorFramework/DocumentWindow/EngineViewWidget.moc.h>
#include <EditorFramework/EditTools/EditTool.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <EditorPluginGreyBox/GreyBoxVertexGizmo.h>
#include <EditorPluginGreyBox/GreyBoxVertexSnapping.h>
#include <EditorPluginGreyBox/GreyBoxVertices.h>
#include <Foundation/Containers/Map.h>
#include <ToolsFoundation/Document/DocumentManager.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>

#include <QApplication>
#include <QCursor>
#include <QKeyEvent>

namespace
{
  struct ResolvedComponent
  {
    const ezDocumentObject* m_pObject = nullptr;
    ezObjectAccessorBase* m_pAccessor = nullptr;
  };

  ResolvedComponent ResolveComponent(ezGameObjectDocument* pDocument, const ezUuid& guid)
  {
    if (!guid.IsValid())
      return {};
    auto resolve = [&guid](ezDocument* pCandidate) -> ResolvedComponent
    {
      const auto* pObject = pCandidate->GetObjectManager()->GetObject(guid);
      if (ezIsGreyBoxComponent(pObject))
        return {pObject, pCandidate->GetObjectAccessor()};
      return {};
    };

    auto result = resolve(pDocument);
    if (result.m_pObject != nullptr)
      return result;
    // Loaded sub-documents share the scene's engine world, and can supply target vertices.
    for (auto* pManager : ezDocumentManager::GetAllDocumentManagers())
    {
      for (auto* pCandidate : pManager->GetAllOpenDocuments())
      {
        if (pCandidate != pDocument && pCandidate->GetMainDocument() == pDocument->GetMainDocument())
        {
          result = resolve(pCandidate);
          if (result.m_pObject != nullptr)
            return result;
        }
      }
    }
    return {};
  }

  struct GeometryCache
  {
    ezUuid m_Component;
    ezVariantArray m_Properties;
    ezDynamicArray<ezVec3> m_Vertices;

    ezStatus Update(const ResolvedComponent& component)
    {
      const char* szProperties[] = {"Shape", "SizeNegX", "SizePosX", "SizeNegY", "SizePosY", "SizeNegZ", "SizePosZ",
        "Detail", "Curvature", "Thickness", "SlopedTop", "SlopedBottom", "BaseRadiusScale", "TopRadiusScale", "Sides", "HeightSegments", "ProfileCurve", "SmoothShading"};
      ezVariantArray properties;
      for (const char* szProperty : szProperties)
      {
        if (component.m_pObject->GetType()->FindPropertyByName(szProperty) == nullptr)
          continue;
        ezVariant value;
        EZ_SUCCEED_OR_RETURN(component.m_pAccessor->GetValueByName(component.m_pObject, szProperty, value));
        properties.PushBack(value);
      }
      if (m_Component == component.m_pObject->GetGuid() && properties == m_Properties)
        return ezStatus(EZ_SUCCESS);

      m_Component = ezUuid();
      EZ_SUCCEED_OR_RETURN(ezBuildGreyBoxVertices(*component.m_pAccessor, component.m_pObject, m_Vertices));
      m_Component = component.m_pObject->GetGuid();
      m_Properties = std::move(properties);
      return ezStatus(EZ_SUCCESS);
    }

    bool Contains(const ezVec3& position) const
    {
      for (const auto& vertex : m_Vertices)
      {
        if (vertex.IsEqual(position, 0.00001f))
          return true;
      }
      return false;
    }
  };

  class ezGreyBoxVertexSnapWindow
  {
  public:
    ezGreyBoxVertexSnapWindow(ezQtEngineDocumentWindow* pWindow, ezGameObjectDocument* pDocument)
      : m_pWindow(pWindow)
      , m_pDocument(pDocument)
    {
      m_Gizmo.SetOwner(pWindow, nullptr);
      m_Gizmo.m_GizmoEvents.AddEventHandler(ezMakeDelegate(&ezGreyBoxVertexSnapWindow::GizmoEventHandler, this));
    }

    bool IsEnabled() const
    {
      const auto* pTool = m_pDocument->GetActiveEditTool();
      return pTool != nullptr && pTool->GetDynamicRTTI()->GetTypeName() == "ezGreyBoxEditTool";
    }

    void Reset()
    {
      m_Source = {};
      m_HoveredComponent = ezUuid();
      m_Gizmo.SetVisible(false);
    }

    void HideForModifiers(ezQtEngineViewWidget* pView)
    {
      if (m_Gizmo.IsVisible())
      {
        m_Gizmo.SetVisible(false);
        // Do not let a cached vertex-handle hit become the starting point of Ctrl+draw.
        if (pView != nullptr)
          pView->ClearLastPickedObject();
      }
    }

    void Update()
    {
      if (!IsEnabled())
      {
        Reset();
        return;
      }
      const auto modifiers = QApplication::keyboardModifiers();
      const auto& interaction = ezQtEngineViewWidget::GetInteractionContext();
      auto* pView = interaction.m_pLastHoveredViewWidget;
      if (pView != nullptr && pView->GetDocumentWindow() != m_pWindow)
        pView = nullptr;
      if (modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::ShiftModifier))
      {
        HideForModifiers(pView);
        return;
      }
      if (ezEditorInputContext::IsAnyInputContextActive())
      {
        if (!m_Gizmo.IsActiveInputContext())
          m_Gizmo.SetVisible(false);
        return;
      }

      if (pView != nullptr && pView->rect().contains(pView->mapFromGlobal(QCursor::pos())))
      {
        const QPoint mouse = pView->mapFromGlobal(QCursor::pos());
        const auto& picked = pView->PickObject(static_cast<ezUInt16>(mouse.x()), static_cast<ezUInt16>(mouse.y()));
        // A vertex handle covers its own mesh in the picking buffer. Keep that mesh's markers.
        if (!m_Gizmo.OwnsHandle(picked.m_PickedOther))
          m_HoveredComponent = picked.m_PickedComponent;
      }
      else
        m_HoveredComponent = ezUuid();

      ezDynamicArray<ezGreyBoxVertexMarker> markers;
      if (m_Source.m_Component.IsValid())
      {
        const auto source = ResolveComponent(m_pDocument, m_Source.m_Component);
        ezTransform transform;
        if (source.m_pObject == nullptr || m_SourceCache.Update(source).Failed() ||
            !m_SourceCache.Contains(m_Source.m_vLocalPosition) ||
            m_pDocument->ComputeObjectTransformation(source.m_pObject->GetParent(), transform).Failed())
          m_Source = {};
        else
        {
          m_Source.m_vWorldPosition = transform.TransformPosition(m_Source.m_vLocalPosition);
          markers.PushBack(m_Source);
        }
      }

      const auto hovered = ResolveComponent(m_pDocument, m_HoveredComponent);
      ezTransform transform;
      if (hovered.m_pObject != nullptr && m_HoverCache.Update(hovered).Succeeded() &&
          m_pDocument->ComputeObjectTransformation(hovered.m_pObject->GetParent(), transform).Succeeded())
      {
        for (const auto& vertex : m_HoverCache.m_Vertices)
        {
          if (m_Source.m_Component == m_HoveredComponent && m_Source.m_vLocalPosition.IsEqual(vertex, 0.00001f))
            continue;
          ezGreyBoxVertexMarker marker;
          marker.m_Component = m_HoveredComponent;
          marker.m_vLocalPosition = vertex;
          marker.m_vWorldPosition = transform.TransformPosition(vertex);
          markers.PushBack(marker);
        }
      }
      m_Gizmo.SetMarkers(markers);
      m_Gizmo.SetVisible(!markers.IsEmpty());
    }

  private:
    void GizmoEventHandler(const ezGizmoEvent& e)
    {
      if (e.m_Type != ezGizmoEvent::Type::Interaction || !IsEnabled())
        return;
      const ezGreyBoxVertexMarker picked = m_Gizmo.GetPickedVertex();
      const auto target = ResolveComponent(m_pDocument, picked.m_Component);
      if (target.m_pObject == nullptr)
        return;
      const auto source = ResolveComponent(m_pDocument, m_Source.m_Component);
      if (source.m_pObject == nullptr || source.m_pObject->GetParent() == target.m_pObject->GetParent())
      {
        m_Source = picked;
        m_Source.m_bPinned = true;
        m_pWindow->ShowTemporaryStatusBarMsg("Source vertex selected. Click a vertex on another Grey Boxing object. Escape cancels.");
        return;
      }

      if (m_SourceCache.Update(source).Failed() || !m_SourceCache.Contains(m_Source.m_vLocalPosition))
      {
        m_Source = {};
        m_pWindow->ShowTemporaryStatusBarMsg("The source geometry changed. Select a source vertex again.");
        return;
      }

      // Moving a parent also moves its descendants, so those cannot be stationary snap targets.
      for (auto* pParent = target.m_pObject->GetParent(); pParent != nullptr; pParent = pParent->GetParent())
      {
        if (pParent == source.m_pObject->GetParent())
        {
          m_pWindow->ShowTemporaryStatusBarMsg("Cannot snap to a child of the source object.");
          return;
        }
      }
      ezTransform targetTransform;
      if (m_pDocument->ComputeObjectTransformation(target.m_pObject->GetParent(), targetTransform).Failed())
        return;
      const ezVec3 vTarget = targetTransform.TransformPosition(picked.m_vLocalPosition);
      const ezStatus status = ezSnapGreyBoxVertex(*source.m_pAccessor, source.m_pObject, m_Source.m_vLocalPosition, vTarget);
      if (status.Succeeded())
      {
        m_Source = {};
        m_pWindow->ShowTemporaryStatusBarMsg("Grey Boxing vertices snapped. Undo restores the previous position.");
      }
      else
        m_pWindow->ShowTemporaryStatusBarMsg(ezFmt("{}", status.GetMessageString()));
    }

    ezQtEngineDocumentWindow* m_pWindow;
    ezGameObjectDocument* m_pDocument;
    ezGreyBoxVertexGizmo m_Gizmo;
    ezGreyBoxVertexMarker m_Source;
    ezUuid m_HoveredComponent;
    GeometryCache m_HoverCache;
    GeometryCache m_SourceCache;
  };

  class ezQtGreyBoxVertexSnapping : public QObject
  {
  public:
    ezQtGreyBoxVertexSnapping()
    {
      qApp->installEventFilter(this);
      ezQtDocumentWindow::s_Events.AddEventHandler(ezMakeDelegate(&ezQtGreyBoxVertexSnapping::WindowEventHandler, this));
    }

    ~ezQtGreyBoxVertexSnapping()
    {
      if (qApp != nullptr)
        qApp->removeEventFilter(this);
      ezQtDocumentWindow::s_Events.RemoveEventHandler(ezMakeDelegate(&ezQtGreyBoxVertexSnapping::WindowEventHandler, this));
    }

  protected:
    bool eventFilter(QObject* pObject, QEvent* pEvent) override
    {
      auto* pView = qobject_cast<ezQtEngineViewWidget*>(pObject);
      if (pEvent->type() == QEvent::KeyPress)
      {
        auto* pKey = static_cast<QKeyEvent*>(pEvent);
        if (pKey->key() == Qt::Key_Control || pKey->key() == Qt::Key_Shift)
        {
          for (auto it = m_Windows.GetIterator(); it.IsValid(); ++it)
            it.Value()->HideForModifiers(pView);
        }
        else if (pKey->key() == Qt::Key_Escape && pView != nullptr)
        {
          auto it = m_Windows.Find(pView->GetDocumentWindow());
          if (it.IsValid())
            it.Value()->Reset();
        }
      }
      // Never consume modifier keys: the original draw-box input context retains Ctrl/Shift behavior.
      return QObject::eventFilter(pObject, pEvent);
    }

  private:
    void WindowEventHandler(const ezQtDocumentWindowEvent& e)
    {
      if (e.m_Type == ezQtDocumentWindowEvent::WindowClosing)
      {
        // Remove handles while their owning document and engine connection are still alive.
        for (auto it = m_Windows.GetIterator(); it.IsValid(); ++it)
        {
          if (it.Key() == e.m_pWindow)
          {
            m_Windows.Remove(it);
            break;
          }
        }
        return;
      }
      if (e.m_Type != ezQtDocumentWindowEvent::BeforeRedraw)
        return;
      auto* pWindow = qobject_cast<ezQtEngineDocumentWindow*>(e.m_pWindow);
      if (pWindow == nullptr)
        return;
      auto* pDocument = ezDynamicCast<ezGameObjectDocument*>(pWindow->GetDocument());
      if (pDocument == nullptr)
        return;
      auto it = m_Windows.Find(pWindow);
      if (!it.IsValid())
      {
        const auto* pTool = pDocument->GetActiveEditTool();
        if (pTool == nullptr || pTool->GetDynamicRTTI()->GetTypeName() != "ezGreyBoxEditTool")
          return;
        it = m_Windows.Insert(pWindow, EZ_DEFAULT_NEW(ezGreyBoxVertexSnapWindow, pWindow, pDocument));
      }
      it.Value()->Update();
    }

    ezMap<ezQtEngineDocumentWindow*, ezUniquePtr<ezGreyBoxVertexSnapWindow>> m_Windows;
  };

  ezQtGreyBoxVertexSnapping* s_pVertexSnapping = nullptr;
} // namespace

void ezRegisterGreyBoxVertexSnapping()
{
  if (qApp != nullptr && s_pVertexSnapping == nullptr)
    s_pVertexSnapping = new ezQtGreyBoxVertexSnapping();
}

void ezUnregisterGreyBoxVertexSnapping()
{
  delete s_pVertexSnapping;
  s_pVertexSnapping = nullptr;
}

