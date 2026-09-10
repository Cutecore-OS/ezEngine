#include <EditorGreyBoxShared/EditorGreyBoxSharedPCH.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <GuiFoundation/PropertyGrid/Implementation/PropertyWidget.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyGridWidget.moc.h>
#include <ToolsFoundation/Object/DocumentObjectBase.h>
#include <ToolsFoundation/Reflection/PhantomRttiManager.h>
#include <QComboBox>

namespace
{
  ezHybridArray<ezGreyBoxFeatureCreator, 2> s_Creators;
  ezHybridArray<const ezRTTI*, 2> s_Types;
  ezGreyBoxGeometryProvider s_GeometryProvider = nullptr;
}

class ezQtGreyBoxCompositeWidget : public ezQtPropertyEditorEnumWidget
{
public:
  ~ezQtGreyBoxCompositeWidget()
  {
    for (auto* feature : m_Features)
      delete feature;
  }
  void SetSelection(const ezArrayPtr<ezPropertySelection>& items) override
  {
    ezQtPropertyEditorEnumWidget::SetSelection(items);
    for (auto* feature : m_Features)
      feature->SetSelection(items);
  }
  void SetReadOnly(bool readOnly = true) override
  {
    ezQtPropertyEditorEnumWidget::SetReadOnly(readOnly);
    for (auto* feature : m_Features)
      feature->SetReadOnly(readOnly);
  }
  bool IsPreparedToDie() const { return IsUndead(); }
  void CommitValue(const ezVariant& value) { BroadcastValueChanged(value); }
  void DisconnectDefaultEnum()
  {
    disconnect(m_pWidget, SIGNAL(currentIndexChanged(int)), this, SLOT(on_CurrentEnum_changed(int)));
  }
  void BeforeChange()
  {
    for (auto* feature : m_Features)
      feature->OnBeforeChange();
  }

protected:
  void OnInit() override
  {
    ezQtPropertyEditorEnumWidget::OnInit();
    for (auto creator : s_Creators)
    {
      auto* feature = creator(this);
      m_Features.PushBack(feature);
      feature->Initialize(m_pGrid, m_pObjectAccessor, m_pType, m_pProp, m_pWidget);
    }
  }
  void DoPrepareToDie() override
  {
    for (auto* feature : m_Features)
      feature->PrepareToDie();
    ezQtPropertyEditorEnumWidget::DoPrepareToDie();
  }

private:
  ezHybridArray<ezQtGreyBoxPropertyFeature*, 2> m_Features;
};

ezQtGreyBoxPropertyFeature::ezQtGreyBoxPropertyFeature(ezQtGreyBoxCompositeWidget* host)
  : QWidget(host), m_pHost(host)
{
}
void ezQtGreyBoxPropertyFeature::Initialize(ezQtPropertyGridWidget* grid, ezObjectAccessorBase* accessor,
  const ezRTTI* type, const ezAbstractProperty* property, QComboBox* combo)
{
  m_pGrid = grid;
  m_pObjectAccessor = accessor;
  m_pType = type;
  m_pProp = property;
  m_pWidget = combo;
  OnInit();
}
void ezQtGreyBoxPropertyFeature::SetSelection(const ezArrayPtr<ezPropertySelection>& items) { m_Items = items; }
void ezQtGreyBoxPropertyFeature::PrepareToDie()
{
  m_bUndead = true;
  DoPrepareToDie();
}
bool ezQtGreyBoxPropertyFeature::IsUndead() const { return m_bUndead || m_pHost->IsPreparedToDie(); }
void ezQtGreyBoxPropertyFeature::BroadcastValueChanged(const ezVariant& value) { m_pHost->CommitValue(value); }
void ezQtGreyBoxPropertyFeature::DisconnectDefaultEnum() { m_pHost->DisconnectDefaultEnum(); }
void ezQtGreyBoxPropertyFeature::BeforeChange() { m_pHost->BeforeChange(); }

namespace
{
  void RegisterType(const ezRTTI* type)
  {
    if (!type || (type->GetTypeName() != "ezGreyBoxShape" && type->GetTypeName() != "ezGreyBoxConeShape") || s_Types.Contains(type))
      return;
    s_Types.PushBack(type);
    ezQtPropertyGridWidget::GetFactory().RegisterCreator(type, [](const ezRTTI*) -> ezQtPropertyWidget*
      { return new ezQtGreyBoxCompositeWidget(); });
  }
  void TypeEvent(const ezPhantomRttiManagerEvent& e)
  {
    if (e.m_Type == ezPhantomRttiManagerEvent::Type::TypeRemoved)
    {
      if (s_Types.RemoveAndCopy(e.m_pChangedType))
        ezQtPropertyGridWidget::GetFactory().UnregisterCreator(e.m_pChangedType);
    }
    else
      RegisterType(e.m_pChangedType);
  }
}
void ezRegisterGreyBoxFeature(ezGreyBoxFeatureCreator creator)
{
  if (s_Creators.Contains(creator))
    return;
  s_Creators.PushBack(creator);
  if (s_Creators.GetCount() == 1)
  {
    ezPhantomRttiManager::s_Events.AddEventHandler(TypeEvent);
    RegisterType(ezRTTI::FindTypeByName("ezGreyBoxShape"));
    RegisterType(ezRTTI::FindTypeByName("ezGreyBoxConeShape"));
  }
}
void ezUnregisterGreyBoxFeature(ezGreyBoxFeatureCreator creator)
{
  if (!s_Creators.RemoveAndCopy(creator) || !s_Creators.IsEmpty())
    return;
  ezPhantomRttiManager::s_Events.RemoveEventHandler(TypeEvent);
  for (const auto* type : s_Types)
    ezQtPropertyGridWidget::GetFactory().UnregisterCreator(type);
  s_Types.Clear();
}
bool ezIsGreyBoxComponent(const ezDocumentObject* object)
{
  return object && (object->GetType()->GetTypeName() == "ezGreyBoxComponent" ||
    object->GetType()->GetTypeName() == "ezGreyBoxConeComponent");
}
void ezSetGreyBoxGeometryProvider(ezGreyBoxGeometryProvider provider) { s_GeometryProvider = provider; }
ezStatus ezBuildGreyBoxExtensionGeometry(ezObjectAccessorBase& accessor, const ezDocumentObject* object, ezGeometry& geometry)
{
  if (!s_GeometryProvider)
    return ezStatus("Enable Grey Boxing Extended to access this component's geometry.");
  return s_GeometryProvider(accessor, object, geometry);
}


