#include <Core/World/World.h>
#include <SpringBonePlugin/Components/SpringBoneComponent.h>
#include <TestFramework/Framework/TestFramework.h>

#ifdef SPRINGBONE_EDITOR_TESTS
#  include <ToolsFoundation/Document/Document.h>
#  include <ToolsFoundation/Object/DocumentObjectMirror.h>
#  include <ToolsFoundation/Object/ObjectAccessorBase.h>

class SpringBoneTestDocument : public ezDocument
{
public:
  SpringBoneTestDocument()
    : ezDocument("SpringBoneMirrorTest.ezScene", EZ_DEFAULT_NEW(ezDocumentObjectManager))
  {
  }
  ezDocumentInfo* CreateDocumentInfo() override { return EZ_DEFAULT_NEW(ezDocumentInfo); }
};
#endif

void TestSpringBoneEditorMirror(ezSpringBoneComponent& spring, ezWorld& world)
{
#ifdef SPRINGBONE_EDITOR_TESTS
  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Editor nested settings mirror and Undo/Redo during simulation")
  {
    SpringBoneTestDocument document;
    auto* accessor = document.GetObjectAccessor();
    ezUuid id;
    accessor->StartTransaction("Add spring bone");
    EZ_TEST_BOOL(accessor->AddObjectByName(document.GetObjectManager()->GetRootObject(), "Children", -1, ezGetStaticRTTI<ezSpringBoneComponent>(), id).Succeeded());
    accessor->FinishTransaction();
    const auto* object = document.GetObjectManager()->GetObject(id);
    ezVariant settingsId;
    EZ_TEST_BOOL(accessor->GetValueByName(object, "Settings", settingsId).Succeeded());
    const auto* settings = document.GetObjectManager()->GetObject(settingsId.Get<ezUuid>());

    // Bind the document root to an existing world component, as the engine-side mirror does.
    // Changes must travel through the nested Settings property path, without resetting Jolt.
    ezRttiConverterContext context;
    context.RegisterObject(id, ezGetStaticRTTI<ezSpringBoneComponent>(), &spring);
    ezDocumentObjectMirror mirror;
    mirror.InitSender(document.GetObjectManager());
    mirror.InitReceiver(&context);
    accessor->StartTransaction("Change angular sway");
    EZ_TEST_BOOL(accessor->SetValueByName(settings, "SwayAngles", ezVec3(0, 25, 0)).Succeeded());
    accessor->FinishTransaction();
    EZ_TEST_BOOL(spring.m_Settings.m_vSwayAngles == ezVec3(0, 25, 0));
    for (ezUInt32 i = 0; i < 120; ++i)
      world.Update();
    EZ_TEST_BOOL(spring.GetSimulatedPose()[2].GetTranslationVector().z < 0.9f);
    EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
    EZ_TEST_BOOL(spring.m_Settings.m_vSwayAngles.IsZero());
    for (ezUInt32 i = 0; i < 180; ++i)
      world.Update();
    EZ_TEST_FLOAT(spring.GetSimulatedPose()[2].GetTranslationVector().z, 1, 0.01f);
    EZ_TEST_BOOL(document.GetCommandHistory()->Redo().Succeeded());
    EZ_TEST_BOOL(spring.m_Settings.m_vSwayAngles == ezVec3(0, 25, 0));
    for (ezUInt32 i = 0; i < 120; ++i)
      world.Update();
    EZ_TEST_BOOL(spring.GetSimulatedPose()[2].GetTranslationVector().z < 0.9f);
    mirror.DeInit();
    context.UnregisterObject(id);
  }
#endif
}
