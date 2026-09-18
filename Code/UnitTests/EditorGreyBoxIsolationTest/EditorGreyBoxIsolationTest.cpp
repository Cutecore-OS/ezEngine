#include <Core/Graphics/Geometry.h>
#include <Core/World/GameObject.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/Utilities/CommandLineUtils.h>
#include <GameEngine/Gameplay/GreyBoxComponent.h>
#include <GuiFoundation/PropertyGrid/Implementation/PropertyWidget.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyGridWidget.moc.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>
#include <ToolsFoundation/Document/Document.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>

EZ_TESTFRAMEWORK_ENTRY_POINT("EditorGreyBoxIsolationTest", "Grey Boxing Plugin Isolation")
EZ_CREATE_SIMPLE_TEST_GROUP(GreyBoxIsolation);

namespace
{
  class TestDocument : public ezDocument
  {
  public:
    TestDocument() : ezDocument("GreyBoxIsolation.ezScene", EZ_DEFAULT_NEW(ezDocumentObjectManager)) {}
    ezDocumentInfo* CreateDocumentInfo() override { return EZ_DEFAULT_NEW(ezDocumentInfo); }
  };
}

EZ_CREATE_SIMPLE_TEST(GreyBoxIsolation, IndependentBundles)
{
  const ezString mode = ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-plugins", 0, "tools");
  const bool tools = mode == "tools" || mode == "both" || mode == "reverse";
  const bool extended = mode == "extended" || mode == "both" || mode == "reverse";
  EZ_TEST_BOOL(tools || extended);
  int argc = 1;
  char name[] = "GreyBoxIsolation";
  char* argv[] = {name, nullptr};
  ezUniquePtr<QApplication> app;
  if (!qApp) app = EZ_DEFAULT_NEW(QApplication, argc, argv);
  ezStartup::StartupCoreSystems();

  // This executable links neither editor plugin nor the procedural runtime plugin.
  EZ_TEST_BOOL(ezRTTI::FindTypeByName("ezGreyBoxConeComponent") == nullptr);
  if (extended && mode == "reverse")
    EZ_TEST_BOOL(ezPlugin::LoadPlugin("ezEditorPluginGreyBoxExtended").Succeeded());
  if (tools)
    EZ_TEST_BOOL(ezPlugin::LoadPlugin("ezEditorPluginGreyBox").Succeeded());
  if (extended && mode != "reverse")
    EZ_TEST_BOOL(ezPlugin::LoadPlugin("ezEditorPluginGreyBoxExtended").Succeeded());

  {
    TestDocument document;
    auto& accessor = *document.GetObjectAccessor();
    accessor.StartTransaction("Fixture");
    ezUuid owner, component;
    accessor.AddObjectByName(document.GetObjectManager()->GetRootObject(), "Children", -1, ezGetStaticRTTI<ezGameObject>(), owner).AssertSuccess();
    accessor.AddObjectByName(accessor.GetObject(owner), "Components", -1, ezGetStaticRTTI<ezGreyBoxComponent>(), component).AssertSuccess();
    accessor.FinishTransaction();
    const auto* object = accessor.GetObject(component);
    ezQtPropertyGridWidget grid(nullptr, &document, false);
    const auto* property = object->GetType()->FindPropertyByName("Shape");
    auto* widget = ezQtPropertyGridWidget::CreatePropertyWidget(property);
    widget->Init(&grid, &accessor, object->GetType(), property);
    ezPropertySelection selection;
    selection.m_pObject = object;
    widget->SetSelection(ezMakeArrayPtr(&selection, 1));
    EZ_TEST_BOOL((widget->findChild<QWidget*>("GreyBoxPivotControls") != nullptr) == tools);
    EZ_TEST_BOOL((widget->findChild<QCheckBox*>("GreyBoxSmoothShading") != nullptr) == extended);
    bool shapeFound = false;
    for (auto* combo : widget->findChildren<QComboBox*>())
      shapeFound |= combo->count() == (extended ? 14 : 13);
    EZ_TEST_BOOL(shapeFound);
    widget->PrepareToDie();
    delete widget;

    const auto* coneType = ezRTTI::FindTypeByName("ezGreyBoxConeComponent");
    EZ_TEST_BOOL((coneType != nullptr) == extended);
    if (coneType)
    {
      accessor.StartTransaction("Cone");
      ezUuid cone;
      accessor.AddObjectByName(accessor.GetObject(owner), "Components", -1, coneType, cone).AssertSuccess();
      accessor.FinishTransaction();
      ezGeometry geometry;
      EZ_TEST_BOOL(ezBuildGreyBoxExtensionGeometry(accessor, accessor.GetObject(cone), geometry).Succeeded());
      EZ_TEST_BOOL(!geometry.GetVertices().IsEmpty());
    }
  }
  ezStartup::ShutdownCoreSystems();
}

