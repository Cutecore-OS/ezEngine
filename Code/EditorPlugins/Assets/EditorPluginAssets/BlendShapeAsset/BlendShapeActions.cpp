#include <EditorPluginAssets/EditorPluginAssetsPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorPluginAssets/BlendShapeAsset/BlendShapeAsset.h>
#include <GuiFoundation/Action/ActionManager.h>
#include <GuiFoundation/Action/ActionMapManager.h>
#include <GuiFoundation/Action/BaseActions.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>
#include <QTimer>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>



class ezImportBlendShapesAction : public ezButtonAction
{
  EZ_ADD_DYNAMIC_REFLECTION(ezImportBlendShapesAction, ezButtonAction);

public:
  ezImportBlendShapesAction(const ezActionContext& context, const char* name)
    : ezButtonAction(context, name, false, ":/AssetIcons/Animated_Mesh.svg")
  {
    m_sName = "Import Blend Shapes";
  }
  void Execute(const ezVariant&) override
  {
    auto* mesh = ezDynamicCast<ezAnimatedMeshAssetDocument*>(m_Context.m_pDocument);
    if (mesh == nullptr)
      return;
    ezDynamicArray<ezDocument*> documents;
    const auto status = ezImportBlendShapes(*mesh, documents);
    if (status.Failed())
    {
      ezQtUiServices::GetSingleton()->MessageBoxStatus(status, "Importing blend shapes failed.");
      return;
    }
    if (documents.IsEmpty())
    {
      ezQtUiServices::GetSingleton()->MessageBoxInformation("The selected Animated Mesh source contains no imported blend shapes.");
      return;
    }
    // Save the mesh settings before the companion, whose transform tracks this file.
    documents.InsertAt(0, mesh);
    for (auto* document : documents)
    {
      if (document->SaveDocument().Failed())
        return;
      ezAssetCurator::GetSingleton()->NotifyOfFileChange(document->GetDocumentPath());
    }
    for (auto* document : documents)
    {
      auto* asset = ezDynamicCast<ezAssetDocument*>(document);
      if (asset == nullptr)
        continue;
      const auto result = ezAssetCurator::GetSingleton()->TransformAsset(asset->GetGuid(), ezTransformFlags::TriggeredManually);
      if (result.Failed())
        ezLog::Error("{}", result.m_sMessage);
    }
  }
};
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezImportBlendShapesAction, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;
static ezActionDescriptorHandle s_ImportShapes;


namespace
{
  /// Keep the exposed-parameter source in sync with the normal Animated Mesh, including undo/redo and pasted objects.
  class PoseBinding : public QObject
  {
  public:
    PoseBinding(ezDocument* document)
      : m_pDocument(document)
    {
      document->GetObjectManager()->m_StructureEvents.AddEventHandler(ezMakeDelegate(&PoseBinding::OnStructure, this));
      document->GetObjectManager()->m_PropertyEvents.AddEventHandler(ezMakeDelegate(&PoseBinding::OnProperty, this));
      Schedule();
    }
    ~PoseBinding()
    {
      m_pDocument->GetObjectManager()->m_StructureEvents.RemoveEventHandler(ezMakeDelegate(&PoseBinding::OnStructure, this));
      m_pDocument->GetObjectManager()->m_PropertyEvents.RemoveEventHandler(ezMakeDelegate(&PoseBinding::OnProperty, this));
    }

  private:
    void OnStructure(const ezDocumentObjectStructureEvent&) { Schedule(); }
    void OnProperty(const ezDocumentObjectPropertyEvent& event)
    {
      if (event.m_sProperty == "Mesh")
        Schedule();
    }
    void Schedule()
    {
      if (m_bPending)
        return;
      m_bPending = true;
      QTimer::singleShot(16, this, [this]()
        { m_bPending = false; Synchronize(); });
    }
    void Synchronize()
    {
      if (m_pDocument->GetCommandHistory()->IsInTransaction())
      {
        Schedule();
        return;
      }
      ezDynamicArray<const ezDocumentObject*> poses;
      auto visit = [&](const ezDocumentObject* object, auto&& recurse) -> void
      {
        if (object->GetType()->GetTypeName() == ezStringView("ezBlendShapePoseComponent"))
          poses.PushBack(object);
        for (const auto* child : object->GetChildren())
          recurse(child, recurse);
      };
      visit(m_pDocument->GetObjectManager()->GetRootObject(), visit);
      auto* manager = m_pDocument->GetObjectManager();
      for (const auto* pose : poses)
      {
        ezVariant mesh("");
        for (const auto* sibling : pose->GetParent()->GetChildren())
          if (sibling->GetType()->GetTypeName() == ezStringView("ezAnimatedMeshComponent"))
          {
            mesh = sibling->GetTypeAccessor().GetValue("Mesh");
            break;
          }
        if (pose->GetTypeAccessor().GetValue("Mesh") == mesh)
          continue;
        // This is a derived editor binding, not an editable choice. Emit the normal property
        // event for exposed parameters and mirroring without adding an undo step or clearing redo.
        manager->SetValue(manager->GetObject(pose->GetGuid()), "Mesh", mesh).AssertSuccess();
      }
    }
    ezDocument* m_pDocument;
    bool m_bPending = false;
  };
  ezMap<ezDocument*, PoseBinding*> s_Bindings;
  void OnDocument(const ezDocumentManager::Event& event)
  {
    if (event.m_Type == ezDocumentManager::Event::Type::DocumentOpened)
      s_Bindings[event.m_pDocument] = new PoseBinding(event.m_pDocument);
    else if (event.m_Type == ezDocumentManager::Event::Type::DocumentClosing)
    {
      auto it = s_Bindings.Find(event.m_pDocument);
      if (it.IsValid())
      {
        delete it.Value();
        s_Bindings.Remove(it);
      }
    }
  }
} // namespace

void ezRegisterBlendShapeActions()
{
  s_ImportShapes = EZ_REGISTER_ACTION_0("BlendShapes.Import", ezActionScope::Document, "Animation", "", ezImportBlendShapesAction);
  ezActionMapManager::GetActionMap("AnimatedMeshAssetToolBar")->MapAction(s_ImportShapes, "", 20.0f);
  ezActionMapManager::GetActionMap("AnimatedMeshAssetMenuBar")->MapAction(s_ImportShapes, "G.Asset", 20.0f);
  ezDocumentManager::s_Events.AddEventHandler(OnDocument);
  for (auto* manager : ezDocumentManager::GetAllDocumentManagers())
    for (auto* document : manager->GetAllOpenDocuments())
      s_Bindings[document] = new PoseBinding(document);
}
void ezUnregisterBlendShapeActions()
{
  ezDocumentManager::s_Events.RemoveEventHandler(OnDocument);
  for (auto it : s_Bindings)
    delete it.Value();
  s_Bindings.Clear();
  ezActionManager::UnregisterAction(s_ImportShapes);
}
