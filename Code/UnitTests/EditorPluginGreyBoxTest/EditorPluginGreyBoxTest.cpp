#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/World/GameObject.h>
#include <Core/World/World.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <EditorPluginGreyBox/GreyBoxPivot.h>
#include <EditorPluginGreyBox/GreyBoxPivotGizmo.h>
#include <EditorPluginGreyBoxExtended/GreyBoxShape.h>
#include <EditorPluginGreyBox/GreyBoxVertexGizmo.h>
#include <EditorPluginGreyBox/GreyBoxVertices.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/MemoryStream.h>
#include <Foundation/Math/Transform.h>
#include <GameEngine/Gameplay/GreyBoxComponent.h>
#include <GreyBoxPlugin/Components/GreyBoxConeComponent.h>
#include <GuiFoundation/PropertyGrid/Implementation/PropertyWidget.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyGridWidget.moc.h>
#include <RendererCore/Meshes/CpuMeshResource.h>
#include <RendererCore/Meshes/MeshComponent.h>
#include <RendererCore/Utils/WorldGeoExtractionUtil.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>
#include <ToolsFoundation/Document/Document.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>

#include <GuiFoundation/PropertyGrid/Implementation/TypeWidget.moc.h>
#include <GuiFoundation/Widgets/CollapsibleGroupBox.moc.h>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

EZ_TESTFRAMEWORK_ENTRY_POINT("EditorPluginGreyBoxTest", "Grey Boxing Tools Tests")
EZ_CREATE_SIMPLE_TEST_GROUP(GreyBox);

namespace
{
  class ezGreyBoxTestDocument : public ezDocument
  {
  public:
    ezGreyBoxTestDocument()
      : ezDocument("GreyBoxPivotTest.ezScene", EZ_DEFAULT_NEW(ezDocumentObjectManager))
    {
    }
    ezDocumentInfo* CreateDocumentInfo() override { return EZ_DEFAULT_NEW(ezDocumentInfo); }
  };

  const ezDocumentObject* Add(ezObjectAccessorBase& accessor, const ezDocumentObject* pParent, const char* szProperty, const ezRTTI* pType)
  {
    ezUuid id;
    EZ_TEST_BOOL(accessor.AddObjectByName(pParent, szProperty, -1, pType, id).Succeeded());
    return accessor.GetObject(id);
  }

  ezVec3 GetCorner(ezObjectAccessorBase& accessor, const ezDocumentObject* pComponent, bool bPositive)
  {
    return ezVec3(accessor.GetByName<float>(pComponent, bPositive ? "SizePosX" : "SizeNegX"),
             accessor.GetByName<float>(pComponent, bPositive ? "SizePosY" : "SizeNegY"),
             accessor.GetByName<float>(pComponent, bPositive ? "SizePosZ" : "SizeNegZ")) *
           (bPositive ? 1.0f : -1.0f);
  }

  ezTransform GetGlobalTransform(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject)
  {
    if (pObject == nullptr || pObject->GetType() != ezGetStaticRTTI<ezGameObject>())
      return ezTransform::MakeIdentity();
    const ezTransform local(accessor.GetByName<ezVec3>(pObject, "LocalPosition"),
      accessor.GetByName<ezQuat>(pObject, "LocalRotation"),
      accessor.GetByName<ezVec3>(pObject, "LocalScaling") * accessor.GetByName<float>(pObject, "LocalUniformScaling"));
    return GetGlobalTransform(accessor, pObject->GetParent()) * local;
  }

  ezVec3 ToWorld(ezObjectAccessorBase& accessor, const ezDocumentObject* pObject, const ezVec3& vPoint)
  {
    return GetGlobalTransform(accessor, pObject).TransformPosition(vPoint);
  }
} // namespace

EZ_CREATE_SIMPLE_TEST(GreyBox, RecalculatePosition)
{
  int argc = 1;
  char name[] = "GreyBoxTest";
  char* argv[] = {name, nullptr};
  ezUniquePtr<QApplication> application;
  if (qApp == nullptr)
    application = EZ_DEFAULT_NEW(QApplication, argc, argv);
  ezStartup::StartupCoreSystems();
  EZ_TEST_BOOL(ezPlugin::LoadPlugin("ezEditorPluginGreyBox").Succeeded());
  EZ_TEST_BOOL(ezPlugin::LoadPlugin("ezEditorPluginGreyBoxExtended").Succeeded());
  {
    ezGreyBoxTestDocument document;
    auto& accessor = *document.GetObjectAccessor();
    accessor.StartTransaction("Setup");
    auto pParent = Add(accessor, document.GetObjectManager()->GetRootObject(), "Children", ezGetStaticRTTI<ezGameObject>());
    auto pOwner = Add(accessor, pParent, "Children", ezGetStaticRTTI<ezGameObject>());
    auto pBox = Add(accessor, pOwner, "Components", ezGetStaticRTTI<ezGreyBoxComponent>());
    auto pSibling = Add(accessor, pOwner, "Components", ezGetStaticRTTI<ezGreyBoxComponent>());
    auto pChild = Add(accessor, pOwner, "Children", ezGetStaticRTTI<ezGameObject>());
    accessor.SetValueByName(pBox, "SizeNegX", 36.0f).AssertSuccess();
    accessor.SetValueByName(pBox, "SizePosX", -34.0f).AssertSuccess();
    accessor.SetValueByName(pBox, "SizeNegY", -10.0f).AssertSuccess();
    accessor.SetValueByName(pBox, "SizePosY", 14.0f).AssertSuccess();
    accessor.SetValueByName(pBox, "SizeNegZ", 2.0f).AssertSuccess();
    accessor.SetValueByName(pBox, "SizePosZ", 6.0f).AssertSuccess();
    accessor.SetValueByName(pChild, "LocalPosition", ezVec3(2, 4, 6)).AssertSuccess();
    accessor.SetValueByName(pOwner, "LocalPosition", ezVec3(5, 7, 9)).AssertSuccess();
    accessor.SetValueByName(pOwner, "LocalRotation", ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisZ(), ezAngle::MakeFromDegree(37))).AssertSuccess();
    accessor.SetValueByName(pOwner, "LocalScaling", ezVec3(-2, 3, 0.5f)).AssertSuccess();
    accessor.SetValueByName(pOwner, "LocalUniformScaling", 2.0f).AssertSuccess();
    accessor.SetValueByName(pParent, "LocalPosition", ezVec3(100, -20, 7)).AssertSuccess();
    accessor.SetValueByName(pParent, "LocalRotation", ezQuat::MakeFromAxisAndAngle(ezVec3::MakeAxisX(), ezAngle::MakeFromDegree(23))).AssertSuccess();
    accessor.SetValueByName(pParent, "LocalScaling", ezVec3(1, 2, 3)).AssertSuccess();
    accessor.FinishTransaction();

    const ezVec3 vMin = ToWorld(accessor, pOwner, GetCorner(accessor, pBox, false));
    const ezVec3 vMax = ToWorld(accessor, pOwner, GetCorner(accessor, pBox, true));
    const ezVec3 vSibling = ToWorld(accessor, pOwner, GetCorner(accessor, pSibling, false));
    const ezVec3 vChild = ToWorld(accessor, pChild, ezVec3::MakeZero());
    const ezDocumentObject* selection[] = {pBox, pSibling};

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "All 27 anchors, rotation, signed scale, parent, children, sibling and Undo/Redo")
    {
      for (int z = 0; z < 3; ++z)
      {
        for (int y = 0; y < 3; ++y)
        {
          for (int x = 0; x < 3; ++x)
          {
            const ezVec3 vAnchor(x * 0.5f, y * 0.5f, z * 0.5f);
            EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, selection, vAnchor).Succeeded());
            EZ_TEST_BOOL(ToWorld(accessor, pOwner, GetCorner(accessor, pBox, false)).IsEqual(vMin, 0.001f));
            EZ_TEST_BOOL(ToWorld(accessor, pOwner, GetCorner(accessor, pBox, true)).IsEqual(vMax, 0.001f));
            EZ_TEST_BOOL(ToWorld(accessor, pOwner, GetCorner(accessor, pSibling, false)).IsEqual(vSibling, 0.001f));
            EZ_TEST_BOOL(ToWorld(accessor, pChild, ezVec3::MakeZero()).IsEqual(vChild, 0.001f));
            const ezVec3 vLocalMin = GetCorner(accessor, pBox, false);
            const ezVec3 vLocalMax = GetCorner(accessor, pBox, true);
            EZ_TEST_BOOL((vLocalMin + (vLocalMax - vLocalMin).CompMul(vAnchor)).IsZero(0.0001f));
            if (x == 0)
            {
              EZ_TEST_FLOAT(vLocalMin.x, 0, 0.0001f);
              EZ_TEST_FLOAT(vLocalMax.x, 2, 0.0001f);
            }
            EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
            EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 36, 0);
            EZ_TEST_BOOL(accessor.GetByName<ezVec3>(pOwner, "LocalPosition") == ezVec3(5, 7, 9));
            EZ_TEST_BOOL(document.GetCommandHistory()->Redo().Succeeded());
            EZ_TEST_BOOL(ToWorld(accessor, pOwner, GetCorner(accessor, pBox, false)).IsEqual(vMin, 0.001f));
            EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
          }
        }
      }
    }


    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Cone conversion preserves identity, properties, vertices, pivot and Undo")
    {
      const ezUuid id = pBox->GetGuid();
      const ezUuid ids[] = {id};
      const ezVec3 oldPosition = accessor.GetByName<ezVec3>(pOwner, "LocalPosition");
      EZ_TEST_BOOL(ezSetGreyBoxShape(accessor, ids, 13).Succeeded());
      const auto* pCone = accessor.GetObject(id);
      EZ_TEST_STRING(pCone->GetType()->GetTypeName(), "ezGreyBoxConeComponent");
      EZ_TEST_BOOL(pCone->GetParent() == pOwner);
      EZ_TEST_FLOAT(accessor.GetByName<float>(pCone, "SizeNegX"), 36, 0);
      EZ_TEST_BOOL(accessor.GetByName<ezVec3>(pOwner, "LocalPosition") == oldPosition);
      ezDynamicArray<ezVec3> vertices;
      EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, pCone, vertices).Succeeded());
      EZ_TEST_INT(vertices.GetCount(), 32 * 8 + 1);
      const ezTransform beforePivot = GetGlobalTransform(accessor, pOwner);
      const ezVec3 worldVertex = beforePivot.TransformPosition(vertices[0]);
      const ezDocumentObject* cones[] = {pCone};
      EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, cones, ezVec3(0.5f)).Succeeded());
      ezDynamicArray<ezVec3> movedVertices;
      EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, pCone, movedVertices).Succeeded());
      EZ_TEST_INT(vertices.GetCount(), movedVertices.GetCount());
      // Translation can reorder equal floating-point keys in the sorted vertex list.
      for (const auto& vertex : vertices)
      {
        const ezVec3 expected = beforePivot.TransformPosition(vertex);
        bool found = false;
        for (const auto& moved : movedVertices)
          found |= ToWorld(accessor, pOwner, moved).IsEqual(expected, 0.001f);
        EZ_TEST_BOOL(found);
      }
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      const ezVec3 target = worldVertex + ezVec3(3, 5, 7);
      EZ_TEST_BOOL(ezSnapGreyBoxVertex(accessor, pCone, vertices[0], target).Succeeded());
      EZ_TEST_BOOL(ToWorld(accessor, pOwner, vertices[0]).IsEqual(target, 0.001f));
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());

      ezQtPropertyGridWidget grid(nullptr, &document, false);
      const auto* pShape = pCone->GetType()->FindPropertyByName("Shape");
      auto* pWidget = ezQtPropertyGridWidget::CreatePropertyWidget(pShape);
      pWidget->Init(&grid, &accessor, pCone->GetType(), pShape);
      ezPropertySelection item;
      item.m_pObject = pCone;
      pWidget->SetSelection(ezMakeArrayPtr(&item, 1));
      auto* pCombo = pWidget->findChild<QComboBox*>("GreyBoxShape");
      EZ_TEST_BOOL(pCombo != nullptr);
      if (pCombo)
      {
        EZ_TEST_INT(pCombo->count(), 14);
        EZ_TEST_INT(pCombo->currentData().toLongLong(), 13);
        pCombo->setCurrentIndex(pCombo->findData(static_cast<qlonglong>(ezGreyBoxShape::StairsPosX)));
        QApplication::processEvents();
        EZ_TEST_STRING(accessor.GetObject(id)->GetType()->GetTypeName(), "ezGreyBoxConeComponent");
        EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      }
      pWidget->PrepareToDie();
      delete pWidget;
      {
        ezQtTypeWidget typeWidget(nullptr, &grid, &accessor, pCone->GetType(),
          "Shape;SizeNegX;SizePosX;SizeNegY;SizePosY;SizeNegZ;SizePosZ;BaseRadiusScale;TopRadiusScale;Sides;HeightSegments;ProfileCurve;SmoothShading;GenerateCollision;UseAsOccluder", nullptr);
        typeWidget.SetSelection(ezMakeArrayPtr(&item, 1));
        EZ_TEST_BOOL(typeWidget.findChild<QWidget*>("GreyBoxPivotControls") != nullptr);
        typeWidget.resize(480, typeWidget.sizeHint().height());
        typeWidget.setAttribute(Qt::WA_DontShowOnScreen);
        typeWidget.show();
        QApplication::processEvents();
        typeWidget.grab().save("H:/ezEngine/Workspace/greybox-cone-properties.png");
        typeWidget.PrepareToDie();
      }
      EZ_TEST_BOOL(ezSetGreyBoxShape(accessor, ids, ezGreyBoxShape::StairsPosX).Succeeded());
      EZ_TEST_STRING(accessor.GetObject(id)->GetType()->GetTypeName(), "ezGreyBoxConeComponent");
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      EZ_TEST_STRING(accessor.GetObject(id)->GetType()->GetTypeName(), "ezGreyBoxConeComponent");
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      EZ_TEST_BOOL(accessor.GetObject(id) == pBox);
    }


    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Cone world serialization, CPU export and static collision")
    {
      ezWorldDesc desc("ConeTest");
      ezWorld world(desc);
      EZ_LOCK(world.GetWriteMarker());
      ezGameObjectDesc objectDesc;
      objectDesc.m_bActiveFlag = false;
      objectDesc.m_LocalPosition = ezVec3(3, 5, 7);
      ezGameObject* pObject = nullptr;
      world.CreateObject(objectDesc, pObject);
      ezGreyBoxConeComponent* pCone = nullptr;
      ezGreyBoxConeComponent::CreateComponent(pObject, pCone);
      pCone->SetSides(7);
      pCone->SetHeightSegments(3);
      pCone->SetTopRadiusScale(0.4f);
      pCone->SetProfileCurve(-0.6f);
      pCone->SetColor(ezColor::Orange);
      pCone->SetCustomData(ezVec4(1, 2, 3, 4));
      pCone->SetUseAsOccluder(false);
      ezMaterialResourceDescriptor materialDesc;
      materialDesc.m_sSurface.Assign("ConeTestSurface");
      const auto material = ezResourceManager::CreateResource<ezMaterialResource>("ConeTestMaterial", std::move(materialDesc));
      pCone->SetMaterial(material);
      ezGreyBoxConeComponent* pColumn = nullptr;
      ezGreyBoxConeComponent::CreateComponent(pObject, pColumn);
      pColumn->SetShape(ezGreyBoxConeShape::Column);
      pColumn->SetDetail(11);
      pColumn->SetSmoothShading(false);
      ezDefaultMemoryStreamStorage storage;
      ezMemoryStreamWriter writer(&storage);
      ezWorldWriter worldWriter;
      worldWriter.WriteWorld(writer, world);
      ezMemoryStreamReader reader(&storage);
      ezWorldReader worldReader;
      EZ_TEST_BOOL(worldReader.ReadWorldDescription(reader).Succeeded());
      ezWorldDesc restoredDesc("ConeRestored");
      ezWorld restoredWorld(restoredDesc);
      EZ_LOCK(restoredWorld.GetWriteMarker());
      worldReader.InstantiateWorld(restoredWorld);
      auto it = restoredWorld.GetComponentManager<ezGreyBoxConeComponentManager>()->GetComponents();
      EZ_TEST_BOOL(it.IsValid());
      if (it.IsValid())
      {
        EZ_TEST_BOOL(it->GetSettings().GetHash() == pCone->GetSettings().GetHash());
        EZ_TEST_BOOL(it->GetColor() == ezColor::Orange);
        EZ_TEST_BOOL(it->GetCustomData() == ezVec4(1, 2, 3, 4));
        EZ_TEST_BOOL(it->GetMaterial() == material);
        EZ_TEST_BOOL(!it->GetUseAsOccluder());
        EZ_TEST_BOOL(it->GetGenerateCollision());
        EZ_TEST_BOOL(!it->GetMesh().IsValid());
        ++it;
        EZ_TEST_BOOL(it.IsValid());
        if (it.IsValid())
        {
          EZ_TEST_INT(it->GetShape().GetValue(), ezGreyBoxConeShape::Column);
          EZ_TEST_INT(it->GetDetail(), 11);
          EZ_TEST_BOOL(!it->GetSmoothShading());
        }
      }
      ezWorldGeoExtractionUtil::MeshObjectList meshes;
      ezMsgExtractGeometry extract;
      extract.m_pMeshObjects = &meshes;
      pCone->OnMsgExtractGeometry(extract);
      EZ_TEST_INT(meshes.GetCount(), 1);
      if (!meshes.IsEmpty())
      {
        ezResourceLock<ezCpuMeshResource> mesh(meshes[0].m_hMeshResource, ezResourceAcquireMode::BlockTillLoaded);
        EZ_TEST_BOOL(mesh->GetDescriptor().MeshBufferDesc().GetPrimitiveCount() > 0);
      }
      ezMsgBuildStaticMesh collision;
      ezSmcDescription meshDescription;
      collision.m_pStaticMeshDescription = &meshDescription;
      pCone->OnBuildStaticMesh(collision);
      EZ_TEST_BOOL(!meshDescription.m_Triangles.IsEmpty());
      EZ_TEST_INT(meshDescription.m_SubMeshes.GetCount(), 1);
      EZ_TEST_INT(meshDescription.m_Surfaces.GetCount(), 1);
      pCone->SetGenerateCollision(false);
      extract.m_Mode = ezWorldGeoExtractionUtil::ExtractionMode::CollisionMesh;
      meshes.Clear();
      pCone->OnMsgExtractGeometry(extract);
      EZ_TEST_BOOL(meshes.IsEmpty());
      meshDescription.m_Triangles.Clear();
      pCone->OnBuildStaticMesh(collision);
      EZ_TEST_BOOL(meshDescription.m_Triangles.IsEmpty());
    }


    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Native shading toggle preserves all shapes and their geometry")
    {
      for (ezInt64 shape = 0; shape <= 12; ++shape)
      {
        accessor.StartTransaction("Native shape");
        auto* object = Add(accessor, pParent, "Children", ezGetStaticRTTI<ezGameObject>());
        auto* native = Add(accessor, object, "Components", ezGetStaticRTTI<ezGreyBoxComponent>());
        for (const char* size : {"SizeNegX", "SizePosX", "SizeNegY", "SizePosY", "SizeNegZ", "SizePosZ"})
          accessor.SetValueByName(native, size, 2.0f).AssertSuccess();
        accessor.SetValueByName(native, "Shape", shape).AssertSuccess();
        accessor.SetValueByName(native, "Curvature", ezAngle::MakeFromDegree(185)).AssertSuccess();
        accessor.SetValueByName(native, "Thickness", 0.3f).AssertSuccess();
        accessor.SetValueByName(native, "Detail", 12u).AssertSuccess();
        accessor.FinishTransaction();
        EZ_TEST_BOOL(ezGetGreyBoxSmoothShading(accessor, native) == (shape >= 5));
        const ezUuid id = native->GetGuid();
        const ezUuid ids[] = {id};
        ezDynamicArray<ezVec3> before, after;
        EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, native, before).Succeeded());
        EZ_TEST_BOOL(ezSetGreyBoxSmoothShading(accessor, ids, false).Succeeded());
        const auto* extended = accessor.GetObject(id);
        EZ_TEST_STRING(extended->GetType()->GetTypeName(), "ezGreyBoxConeComponent");
        EZ_TEST_INT(accessor.GetByName<ezInt64>(extended, "Shape"), shape);
        EZ_TEST_BOOL(!accessor.GetByName<bool>(extended, "SmoothShading"));
        EZ_TEST_INT(accessor.GetByName<ezUInt32>(extended, "Detail"), 12);
        EZ_TEST_FLOAT(accessor.GetByName<float>(extended, "Thickness"), 0.3f, 0);
        EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, extended, after).Succeeded());
        if (shape == 5)
        {
          ezQtPropertyGridWidget grid(nullptr, &document, false);
          ezQtTypeWidget typeWidget(nullptr, &grid, &accessor, extended->GetType(),
            "Shape;SizeNegX;SizePosX;SizeNegY;SizePosY;SizeNegZ;SizePosZ;BaseRadiusScale;TopRadiusScale;Sides;HeightSegments;ProfileCurve;SmoothShading;Detail;Curvature;Thickness;SlopedTop;SlopedBottom;GenerateCollision;UseAsOccluder", nullptr);
          ezPropertySelection item;
          item.m_pObject = extended;
          typeWidget.SetSelection(ezMakeArrayPtr(&item, 1));
          auto* shading = typeWidget.findChild<QCheckBox*>("GreyBoxSmoothShading");
          EZ_TEST_BOOL(shading != nullptr);
          if (shading)
          {
            EZ_TEST_BOOL(!shading->isChecked());
            const auto* label = typeWidget.findChild<QLabel*>("GreyBoxSmoothShadingLabel");
            EZ_TEST_BOOL(label != nullptr);
            if (label)
            {
              auto* layout = qobject_cast<QGridLayout*>(label->parentWidget()->layout());
              EZ_TEST_BOOL(layout != nullptr);
              if (layout)
              {
                int labelRow, labelColumn, rowSpan, columnSpan, controlRow, controlColumn;
                layout->getItemPosition(layout->indexOf(label), &labelRow, &labelColumn, &rowSpan, &columnSpan);
                EZ_TEST_INT(labelColumn, 0);
                EZ_TEST_INT(columnSpan, 1);
                layout->getItemPosition(layout->indexOf(shading->parentWidget()), &controlRow, &controlColumn, &rowSpan, &columnSpan);
                EZ_TEST_INT(controlColumn, 2);
                EZ_TEST_INT(columnSpan, 1);
                EZ_TEST_INT(controlRow, labelRow);
              }
            }
            auto* group = shading->parentWidget();
            while (group && !qobject_cast<ezQtCollapsibleGroupBox*>(group))
              group = group->parentWidget();
            EZ_TEST_BOOL(group != nullptr);
            if (group)
              EZ_TEST_BOOL(qobject_cast<ezQtCollapsibleGroupBox*>(group)->GetTitle() == "Misc");
          }
          typeWidget.resize(480, typeWidget.sizeHint().height());
          typeWidget.setAttribute(Qt::WA_DontShowOnScreen);
          typeWidget.show();
          QApplication::processEvents();
          typeWidget.grab().save("H:/ezEngine/Workspace/greybox-column-properties.png");
          typeWidget.PrepareToDie();
        }
        EZ_TEST_BOOL(before == after);
        EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
        EZ_TEST_BOOL(accessor.GetObject(id) == native);
        EZ_TEST_BOOL(document.GetCommandHistory()->Redo().Succeeded());
        EZ_TEST_BOOL(ezSetGreyBoxSmoothShading(accessor, ids, true).Succeeded());
        EZ_TEST_BOOL(accessor.GetByName<bool>(accessor.GetObject(id), "SmoothShading"));
        EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, accessor.GetObject(id), after).Succeeded());
        EZ_TEST_BOOL(before == after);
      }
      const ezUuid invalidSelection[] = {pBox->GetGuid(), pOwner->GetGuid()};
      EZ_TEST_BOOL(ezSetGreyBoxSmoothShading(accessor, invalidSelection, false).Failed());
      EZ_TEST_BOOL(accessor.GetObject(pBox->GetGuid()) == pBox);
    }


    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Shape dropdown synchronizes shading with runtime and Undo")
    {
      accessor.StartTransaction("Shape switching fixture");
      auto* owner = Add(accessor, pParent, "Children", ezGetStaticRTTI<ezGameObject>());
      auto* object = Add(accessor, owner, "Components", ezGetStaticRTTI<ezGreyBoxConeComponent>());
      accessor.FinishTransaction();
      const ezUuid ids[] = {object->GetGuid()};
      ezQtPropertyGridWidget grid(nullptr, &document, false);
      const auto* property = object->GetType()->FindPropertyByName("Shape");
      auto* widget = ezQtPropertyGridWidget::CreatePropertyWidget(property);
      widget->Init(&grid, &accessor, object->GetType(), property);
      ezPropertySelection item;
      item.m_pObject = object;
      widget->SetSelection(ezMakeArrayPtr(&item, 1));
      auto* combo = widget->findChild<QComboBox*>("GreyBoxShape");
      auto* check = widget->findChild<QCheckBox*>("GreyBoxSmoothShading");
      EZ_TEST_BOOL(combo != nullptr && check != nullptr);
      if (combo && check)
      {
        ezGreyBoxConeComponent runtime;
        for (ezInt64 shape : {0, 5, 1, 13, 4, 0})
        {
          const ezInt64 oldShape = accessor.GetByName<ezInt64>(object, "Shape");
          const bool oldSmooth = accessor.GetByName<bool>(object, "SmoothShading");
          combo->setCurrentIndex(combo->findData(static_cast<qlonglong>(shape)));
          QApplication::processEvents();
          ezGreyBoxConeSettings settings;
          EZ_TEST_BOOL(ezReadGreyBoxConeSettings(accessor, object, settings).Succeeded());
          const bool expected = settings.GetDefaultSmoothShading();
          EZ_TEST_INT(settings.m_uiShape, shape);
          EZ_TEST_BOOL(settings.m_bSmoothShading == expected);
          EZ_TEST_BOOL(check->isChecked() == expected);

          // Replay the reflected property updates, including different delivery orders.
          runtime.SetSmoothShading(!expected);
          runtime.SetShape(static_cast<ezGreyBoxConeShape::Enum>(shape));
          EZ_TEST_BOOL(runtime.GetSmoothShading() != expected);
          runtime.SetSmoothShading(expected);
          runtime.SetShape(static_cast<ezGreyBoxConeShape::Enum>(shape));
          EZ_TEST_BOOL(runtime.GetSmoothShading() == expected);
          EZ_TEST_BOOL(runtime.GetSettings().GetHash() == settings.GetHash());

          EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
          EZ_TEST_INT(accessor.GetByName<ezInt64>(object, "Shape"), oldShape);
          EZ_TEST_BOOL(accessor.GetByName<bool>(object, "SmoothShading") == oldSmooth);
          EZ_TEST_BOOL(check->isChecked() == oldSmooth);
          EZ_TEST_BOOL(document.GetCommandHistory()->Redo().Succeeded());
          EZ_TEST_BOOL(check->isChecked() == expected);
          EZ_TEST_BOOL(ezSetGreyBoxSmoothShading(accessor, ids, !expected).Succeeded());
          EZ_TEST_BOOL(check->isChecked() != expected);
        }
      }
      widget->PrepareToDie();
      delete widget;
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Invalid multi-selection rolls back earlier objects")
    {
      const ezDocumentObject* invalid[] = {pBox, pChild};
      EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, invalid, ezVec3(0.5f)).Failed());
      EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 36, 0);
      EZ_TEST_BOOL(accessor.GetByName<ezVec3>(pOwner, "LocalPosition") == ezVec3(5, 7, 9));
      EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, selection, ezVec3(2)).Failed());
    }



    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Parent and child selected together, then repeated recalculation")
    {
      accessor.StartTransaction("Add child grey box");
      auto pChildBox = Add(accessor, pChild, "Components", ezGetStaticRTTI<ezGreyBoxComponent>());
      accessor.SetValueByName(pChildBox, "SizeNegX", -3.0f).AssertSuccess();
      accessor.SetValueByName(pChildBox, "SizePosX", 5.0f).AssertSuccess();
      accessor.FinishTransaction();
      const ezVec3 vChildBoxMin = ToWorld(accessor, pChild, GetCorner(accessor, pChildBox, false));
      const ezDocumentObject* hierarchy[] = {pBox, pChildBox};
      EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, hierarchy, ezVec3(0.5f)).Succeeded());
      EZ_TEST_BOOL(ToWorld(accessor, pOwner, GetCorner(accessor, pBox, false)).IsEqual(vMin, 0.001f));
      EZ_TEST_BOOL(ToWorld(accessor, pChild, GetCorner(accessor, pChildBox, false)).IsEqual(vChildBoxMin, 0.001f));
      const ezVec3 vPosition = accessor.GetByName<ezVec3>(pOwner, "LocalPosition");
      EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, hierarchy, ezVec3(0.5f)).Succeeded());
      EZ_TEST_BOOL(accessor.GetByName<ezVec3>(pOwner, "LocalPosition") == vPosition);
      // A no-op must not add another undo step.
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 36, 0);
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Uncompensated components and singular parent transforms roll back")
    {
      accessor.StartTransaction("Add other component");
      Add(accessor, pOwner, "Components", ezGetStaticRTTI<ezMeshComponent>());
      accessor.FinishTransaction();
      EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, selection, ezVec3(0.5f)).Failed());
      EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 36, 0);
      EZ_TEST_BOOL(ToWorld(accessor, pChild, ezVec3::MakeZero()).IsEqual(vChild, 0.001f));
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());

      accessor.StartTransaction("Set singular parent scale");
      accessor.SetValueByName(pParent, "LocalScaling", ezVec3(0, 1, 1)).AssertSuccess();
      accessor.FinishTransaction();
      EZ_TEST_BOOL(ezRecalculateGreyBoxPivot(accessor, selection, ezVec3(0.5f)).Failed());
      EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 36, 0);
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
    }


    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Actual shape vertices, including every stair tread")
    {
      ezDynamicArray<ezVec3> vertices;
      EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, pBox, vertices).Succeeded());
      EZ_TEST_INT(vertices.GetCount(), 8);
      const ezVec3 vMinimum = GetCorner(accessor, pBox, false);
      const ezVec3 vMaximum = GetCorner(accessor, pBox, true);
      auto contains = [&vertices](const ezVec3& position)
      {
        for (const auto& vertex : vertices)
        {
          if (vertex.IsEqual(position, 0.0001f))
            return true;
        }
        return false;
      };

      accessor.StartTransaction("Four steps");
      accessor.SetValueByName(pBox, "Shape", static_cast<ezInt64>(ezGreyBoxShape::StairsPosX)).AssertSuccess();
      accessor.SetValueByName(pBox, "Detail", ezUInt32(4)).AssertSuccess();
      accessor.SetValueByName(pBox, "Curvature", ezAngle::MakeFromDegree(0)).AssertSuccess();
      accessor.FinishTransaction();
      EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, pBox, vertices).Succeeded());
      EZ_TEST_BOOL(vertices.GetCount() > 8);
      for (ezUInt32 i = 0; i < 4; ++i)
      {
        const float x0 = vMinimum.x + (vMaximum.x - vMinimum.x) * i / 4.0f;
        const float x1 = vMinimum.x + (vMaximum.x - vMinimum.x) * (i + 1) / 4.0f;
        const float z = vMinimum.z + (vMaximum.z - vMinimum.z) * (i + 1) / 4.0f;
        EZ_TEST_BOOL(contains(ezVec3(x0, vMinimum.y, z)));
        EZ_TEST_BOOL(contains(ezVec3(x1, vMinimum.y, z)));
        EZ_TEST_BOOL(contains(ezVec3(x0, vMaximum.y, z)));
        EZ_TEST_BOOL(contains(ezVec3(x1, vMaximum.y, z)));
      }
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());

      for (ezInt64 shape = ezGreyBoxShape::Box; shape <= ezGreyBoxShape::SpiralStairs; ++shape)
      {
        accessor.StartTransaction("Shape");
        accessor.SetValueByName(pBox, "Shape", shape).AssertSuccess();
        accessor.SetValueByName(pBox, "Detail", ezUInt32(16)).AssertSuccess();
        accessor.SetValueByName(pBox, "Curvature", ezAngle::MakeFromDegree(95)).AssertSuccess();
        accessor.FinishTransaction();
        EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, pBox, vertices).Succeeded());
        EZ_TEST_BOOL(!vertices.IsEmpty());
        for (ezUInt32 i = 0; i < vertices.GetCount(); ++i)
        {
          EZ_TEST_BOOL(vertices[i].IsValid());
          for (ezUInt32 j = i + 1; j < vertices.GetCount(); ++j)
            EZ_TEST_BOOL(vertices[i] != vertices[j]);
        }
        if (shape >= ezGreyBoxShape::RampPosX && shape <= ezGreyBoxShape::RampNegY)
          EZ_TEST_INT(vertices.GetCount(), 6);
        if (shape == ezGreyBoxShape::Column)
          EZ_TEST_BOOL(vertices.GetCount() >= 32);
        EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      }

      accessor.StartTransaction("Rounded curvature");
      accessor.SetValueByName(pBox, "Shape", static_cast<ezInt64>(ezGreyBoxShape::StairsPosX)).AssertSuccess();
      accessor.SetValueByName(pBox, "Curvature", ezAngle::MakeFromDegree(93)).AssertSuccess();
      accessor.FinishTransaction();
      ezDynamicArray<ezVec3> rounded;
      EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, pBox, rounded).Succeeded());
      accessor.StartTransaction("Rounded curvature reference");
      accessor.SetValueByName(pBox, "Curvature", ezAngle::MakeFromDegree(95)).AssertSuccess();
      accessor.FinishTransaction();
      EZ_TEST_BOOL(ezBuildGreyBoxVertices(accessor, pBox, vertices).Succeeded());
      EZ_TEST_BOOL(vertices == rounded);
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Vertex-to-vertex translation with transformed parents and Undo/Redo")
    {
      const ezVec3 vSource = GetCorner(accessor, pBox, false);
      const ezVec3 vTarget(31, -47, 19);
      const ezTransform original = GetGlobalTransform(accessor, pOwner);
      const ezVec3 vOriginalChild = ToWorld(accessor, pChild, ezVec3::MakeZero());
      EZ_TEST_BOOL(ezSnapGreyBoxVertex(accessor, pBox, vSource, vTarget).Succeeded());
      const ezTransform snapped = GetGlobalTransform(accessor, pOwner);
      EZ_TEST_BOOL(snapped.TransformPosition(vSource).IsEqual(vTarget, 0.001f));
      EZ_TEST_BOOL(snapped.m_qRotation.IsEqualRotation(original.m_qRotation, ezMath::DefaultEpsilon<float>()));
      EZ_TEST_BOOL(snapped.m_vScale == original.m_vScale);
      EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 36, 0);
      EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizePosX"), -34, 0);
      EZ_TEST_BOOL(ToWorld(accessor, pChild, ezVec3::MakeZero()).IsEqual(vOriginalChild + vTarget - original.TransformPosition(vSource), 0.001f));
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      EZ_TEST_BOOL(GetGlobalTransform(accessor, pOwner).m_vPosition.IsEqual(original.m_vPosition, 0.001f));
      EZ_TEST_BOOL(document.GetCommandHistory()->Redo().Succeeded());
      EZ_TEST_BOOL(ToWorld(accessor, pOwner, vSource).IsEqual(vTarget, 0.001f));
      EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Vertex markers are pickable, and Ctrl/Shift pass through")
    {
      ezGreyBoxVertexGizmo gizmo;
      ezGreyBoxVertexMarker marker;
      marker.m_Component = pBox->GetGuid();
      marker.m_vLocalPosition = GetCorner(accessor, pBox, false);
      marker.m_vWorldPosition = ToWorld(accessor, pOwner, marker.m_vLocalPosition);
      gizmo.SetMarkers(ezMakeArrayPtr(&marker, 1));
      gizmo.SetVisible(true);
      gizmo.ConfigureInteraction(&gizmo.GetHandle(0), nullptr, marker.m_vWorldPosition, ezVec2I32(640, 480));
      for (const auto modifiers : {Qt::KeyboardModifiers(Qt::ControlModifier), Qt::KeyboardModifiers(Qt::ShiftModifier),
             Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier)})
      {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, modifiers);
        EZ_TEST_BOOL(gizmo.MousePressEvent(&press) == ezEditorInput::MayBeHandledByOthers);
        EZ_TEST_BOOL(!gizmo.IsActiveInputContext());
      }
      QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      EZ_TEST_BOOL(gizmo.MousePressEvent(&press) == ezEditorInput::WasExclusivelyHandled);
      EZ_TEST_BOOL(gizmo.GetPickedVertex().m_Component == pBox->GetGuid());
      EZ_TEST_BOOL(gizmo.GetPickedVertex().m_vLocalPosition == marker.m_vLocalPosition);
      QMouseEvent release(QEvent::MouseButtonRelease, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
      EZ_TEST_BOOL(gizmo.MouseReleaseEvent(&release) == ezEditorInput::WasExclusivelyHandled);
      EZ_TEST_BOOL(!gizmo.IsActiveInputContext());
      marker.m_bPinned = true;
      gizmo.SetMarkers(ezMakeArrayPtr(&marker, 1));
      EZ_TEST_BOOL(gizmo.GetHandle(0).GetTransformation().m_vPosition == marker.m_vWorldPosition);
      gizmo.SetVisible(false);
      EZ_TEST_BOOL(gizmo.MousePressEvent(&press) == ezEditorInput::MayBeHandledByOthers);
      gizmo.SetVisible(true);
      EZ_TEST_INT(gizmo.GetMarkerCount(), 1);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Clickable gizmo anchors preserve geometry and release input capture")
    {
      ezGreyBoxPivotGizmo gizmo;
      const ezTransform transform = GetGlobalTransform(accessor, pOwner);
      gizmo.SetBounds(-GetCorner(accessor, pBox, false), GetCorner(accessor, pBox, true));
      gizmo.SetTransformation(transform);
      gizmo.SetVisible(true);
      ezUInt32 uiPicks = 0;
      gizmo.m_GizmoEvents.AddEventHandler([&uiPicks](const ezGizmoEvent& event)
        {
          if (event.m_Type == ezGizmoEvent::Type::Interaction)
            ++uiPicks; });
      for (ezUInt32 i = 0; i < 27; ++i)
      {
        const ezVec3 vAnchor = ezGreyBoxPivotGizmo::GetAnchor(i);
        const ezVec3 vLocalPoint = GetCorner(accessor, pBox, false) +
                                   (GetCorner(accessor, pBox, true) - GetCorner(accessor, pBox, false)).CompMul(vAnchor);
        EZ_TEST_BOOL(gizmo.GetHandle(i).GetTransformation().m_vPosition.IsEqual(transform.TransformPosition(vLocalPoint), 0.001f));
        gizmo.ConfigureInteraction(&gizmo.GetHandle(i), nullptr, ezVec3::MakeZero(), ezVec2I32(640, 480));
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        EZ_TEST_BOOL(gizmo.MousePressEvent(&press) == ezEditorInput::WasExclusivelyHandled);
        EZ_TEST_INT(gizmo.GetSelectedAnchor(), i);
        EZ_TEST_BOOL(gizmo.IsActiveInputContext());
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        EZ_TEST_BOOL(gizmo.MouseReleaseEvent(&release) == ezEditorInput::WasExclusivelyHandled);
        EZ_TEST_BOOL(!gizmo.IsActiveInputContext());
      }
      EZ_TEST_INT(uiPicks, 27);
      EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 36, 0);
      gizmo.SetVisible(false);
      QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      EZ_TEST_BOOL(gizmo.MousePressEvent(&press) == ezEditorInput::MayBeHandledByOthers);
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Controls are inside a collapsible Tools group")
    {
      ezQtPropertyGridWidget grid(nullptr, &document, false);
      ezQtTypeWidget typeWidget(nullptr, &grid, &accessor, pBox->GetType(),
        "Shape;SizeNegX;SizePosX;SizeNegY;SizePosY;SizeNegZ;SizePosZ;Detail;Curvature;Thickness;SlopedTop;SlopedBottom;GenerateCollision;UseAsOccluder", nullptr);
      ezPropertySelection item;
      item.m_pObject = pBox;
      typeWidget.SetSelection(ezMakeArrayPtr(&item, 1));
      auto pControls = typeWidget.findChild<QWidget*>("GreyBoxPivotControls");
      EZ_TEST_BOOL(pControls != nullptr);
      if (pControls)
      {
        auto pLayout = qobject_cast<QGridLayout*>(typeWidget.layout());
        int row, column, rowSpan, columnSpan;
        pLayout->getItemPosition(pLayout->indexOf(pControls), &row, &column, &rowSpan, &columnSpan);
        EZ_TEST_INT(row, pLayout->rowCount() - 1);
        EZ_TEST_INT(columnSpan, 3);
        EZ_TEST_BOOL(pControls->parentWidget() == &typeWidget);
        auto pTools = qobject_cast<ezQtCollapsibleGroupBox*>(pControls);
        EZ_TEST_BOOL(pTools != nullptr);
        if (pTools != nullptr)
        {
          EZ_TEST_BOOL(pTools->GetTitle() == "Tools");
          pTools->SetCollapseState(true);
          EZ_TEST_BOOL(pTools->GetContent()->isHidden());
          pTools->SetCollapseState(false);
          EZ_TEST_BOOL(!pTools->GetContent()->isHidden());
        }
      }
      typeWidget.resize(480, typeWidget.sizeHint().height());
      typeWidget.setAttribute(Qt::WA_DontShowOnScreen);
      typeWidget.show();
      QApplication::processEvents();
      typeWidget.grab().save("H:/ezEngine/Workspace/greybox-property-panel.png");
      typeWidget.PrepareToDie();
    }

    EZ_TEST_BLOCK(ezTestBlock::Enabled, "Registered Shape widget exposes button and 27 anchors")
    {
      ezQtPropertyGridWidget grid(nullptr, &document, false);
      const auto* pShape = pBox->GetType()->FindPropertyByName("Shape");
      auto* pWidget = ezQtPropertyGridWidget::CreatePropertyWidget(pShape);
      pWidget->Init(&grid, &accessor, pBox->GetType(), pShape);
      ezPropertySelection item;
      item.m_pObject = pBox;
      pWidget->SetSelection(ezMakeArrayPtr(&item, 1));
      QPushButton* pRecalculate = nullptr;
      for (auto* pButton : pWidget->findChildren<QPushButton*>())
      {
        if (pButton->text() == "Recalculate Position")
          pRecalculate = pButton;
      }
      EZ_TEST_BOOL(pRecalculate != nullptr);
      bool bFoundAnchors = false;
      for (auto* pCombo : pWidget->findChildren<QComboBox*>())
        bFoundAnchors |= pCombo->count() == 27;
      EZ_TEST_BOOL(bFoundAnchors);
      if (pRecalculate)
      {
        pRecalculate->click();
        EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizeNegX"), 1, 0.0001f);
        EZ_TEST_FLOAT(accessor.GetByName<float>(pBox, "SizePosX"), 1, 0.0001f);
        EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
      }
      auto* pSmooth = pWidget->findChild<QCheckBox*>("GreyBoxSmoothShading");
      EZ_TEST_BOOL(pSmooth != nullptr);
      if (pSmooth)
      {
        const ezUuid id = pBox->GetGuid();
        EZ_TEST_BOOL(!pSmooth->isChecked());
        pSmooth->click();
        QApplication::processEvents();
        const auto* extended = accessor.GetObject(id);
        EZ_TEST_STRING(extended->GetType()->GetTypeName(), "ezGreyBoxConeComponent");
        EZ_TEST_BOOL(accessor.GetByName<bool>(extended, "SmoothShading"));
        EZ_TEST_BOOL(document.GetCommandHistory()->Undo().Succeeded());
        EZ_TEST_BOOL(accessor.GetObject(id) == pBox);
      }
      pWidget->PrepareToDie();
      delete pWidget;
    }
  }
  ezStartup::ShutdownCoreSystems();
}

