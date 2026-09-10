#include <EditorPluginGreyBoxExtended/EditorPluginGreyBoxExtendedPCH.h>
#include <Core/Graphics/Geometry.h>
#include <EditorGreyBoxShared/GreyBoxIntegration.h>
#include <EditorPluginGreyBoxExtended/GreyBoxShape.h>
#include <GreyBoxPlugin/Geometry/ConeGeometry.h>
#include <Foundation/Strings/TranslationLookup.h>
#include <GuiFoundation/PropertyGrid/Implementation/TypeWidget.moc.h>
#include <GuiFoundation/PropertyGrid/PropertyMetaState.h>
#include <GuiFoundation/UIServices/UIServices.moc.h>
#include <GuiFoundation/Widgets/CollapsibleGroupBox.moc.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QTimer>

namespace
{
  class ezQtGreyBoxShapeWidget : public ezQtGreyBoxPropertyFeature
  {
  public:
    using ezQtGreyBoxPropertyFeature::ezQtGreyBoxPropertyFeature;
    ~ezQtGreyBoxShapeWidget()
    {
      m_ShadingUnsubscriber.Unsubscribe();
      delete m_pSmoothLabel;
      delete m_pSmoothRow;
    }
    void SetSelection(const ezArrayPtr<ezPropertySelection>& items) override
    {
      ezQtGreyBoxPropertyFeature::SetSelection(items);
      UpdateSmoothState();
      if (!m_pSmoothRow) return;
      for (QWidget* parent = parentWidget(); parent; parent = parent->parentWidget())
      {
        if (auto* pTypeWidget = qobject_cast<ezQtTypeWidget*>(parent))
        {
          if (m_pType->GetTypeName() == "ezGreyBoxConeComponent")
          {
            bool bHasCone = false;
            for (const auto& item : items)
              bHasCone |= m_pObjectAccessor->GetByName<ezInt64>(item.m_pObject, "Shape") == 13;
            for (auto* pGroup : pTypeWidget->findChildren<ezQtCollapsibleGroupBox*>())
              if (pGroup->GetTitle() == "Cone")
                pGroup->setVisible(bHasCone);
          }
          for (auto* pGroup : pTypeWidget->findChildren<ezQtCollapsibleGroupBox*>())
          {
            if (pGroup->GetTitle() == "Misc" && m_pSmoothRow->parentWidget() != pGroup->GetContent())
            {
              auto* pLayout = qobject_cast<QGridLayout*>(pGroup->GetContent()->layout());
              if (pLayout)
              {
                m_pSmoothRow->setParent(pGroup->GetContent());
                const int row = pLayout->rowCount();
                m_pSmoothLabel->setParent(pGroup->GetContent());
                pLayout->addWidget(m_pSmoothLabel, row, 0);
                pLayout->addWidget(m_pSmoothRow, row, 2);
                m_pSmoothLabel->show();
                m_pSmoothRow->show();
              }
            }
          }

          break;
        }
      }
    }
    void SetReadOnly(bool readOnly = true) override
    {
      if (m_pSmoothRow) m_pSmoothRow->setEnabled(!readOnly);
      if (m_pSmoothLabel) m_pSmoothLabel->setEnabled(!readOnly);
    }
  protected:
    void OnInit() override
    {
      if (m_pType->GetTypeName() != "ezGreyBoxComponent" && m_pType->GetTypeName() != "ezGreyBoxConeComponent")
        return;
      DisconnectDefaultEnum();
      {
        const QSignalBlocker blocker(m_pWidget);
        m_pWidget->clear();
        const auto* pNativeShape = ezRTTI::FindTypeByName("ezGreyBoxShape");
        if (pNativeShape != nullptr)
        {
          for (ezUInt32 i = 1; i < pNativeShape->GetProperties().GetCount(); ++i)
          {
            const auto* pProperty = pNativeShape->GetProperties()[i];
            if (pProperty->GetCategory() != ezPropertyCategory::Constant)
              continue;
            const auto* pConstant = static_cast<const ezAbstractConstantProperty*>(pProperty);
            ezStringView name = pProperty->GetPropertyName();
            name.TrimWordStart("ezGreyBoxShape::");
            m_pWidget->addItem(QString::fromUtf8(name.GetStartPointer(), name.GetElementCount()), pConstant->GetConstant().ConvertTo<ezInt64>());
          }
        }
        m_pWidget->addItem("Cone", static_cast<qlonglong>(13));
        m_pWidget->setObjectName("GreyBoxShape");
      }
      connect(m_pWidget, &QComboBox::currentIndexChanged, this, [this](int index)
        {
          if (IsUndead() || index < 0)
            return;
          const ezInt64 shape = m_pWidget->itemData(index).toLongLong();
          if (m_pType->GetTypeName() == "ezGreyBoxComponent" && shape != 13)
          {
            BroadcastValueChanged(shape);
            return;
          }
          ezDynamicArray<ezUuid> ids;
          for (const auto& item : m_Items)
            ids.PushBack(item.m_pObject->GetGuid());
          // Replacing a component rebuilds the property grid. Leave the combobox signal first.
          QTimer::singleShot(0, this, [this, ids, shape]()
            {
              if (IsUndead())
                return;
              BeforeChange();
              auto* pAccessor = m_pObjectAccessor;
              const ezStatus status = ezSetGreyBoxShape(*pAccessor, ids, shape);
              ezQtUiServices::MessageBoxStatus(status, "Could not change Grey Boxing shape.");
            }); });


      {
        m_pSmoothRow = new QWidget(this);
        auto* pRowLayout = new QHBoxLayout(m_pSmoothRow);
        pRowLayout->setContentsMargins(0, 0, 0, 0);
        auto* pSmooth = new QCheckBox(m_pSmoothRow);
        m_pSmoothLabel = new QLabel("Smooth Shading", this);
        m_pSmoothLabel->setObjectName("GreyBoxSmoothShadingLabel");
        m_pSmoothLabel->setBuddy(pSmooth);
        m_pSmooth = pSmooth;
        pSmooth->setObjectName("GreyBoxSmoothShading");

        pSmooth->setToolTip("Smooth surface lighting without changing geometry. The initial state follows the native shape. Disable for flat polygon faces. Supports Undo.");
        pRowLayout->addWidget(pSmooth);
        connect(pSmooth, &QCheckBox::toggled, this, [this](bool smooth)
          {
            ezDynamicArray<ezUuid> ids;
            for (const auto& item : m_Items)
              ids.PushBack(item.m_pObject->GetGuid());
            QTimer::singleShot(0, this, [this, ids, smooth]()
              {
                if (IsUndead())
                  return;
                BeforeChange();
                const ezStatus status = ezSetGreyBoxSmoothShading(*m_pObjectAccessor, ids, smooth);
                ezQtUiServices::MessageBoxStatus(status, "Could not change Grey Boxing shading.");
              }); });
      }
      m_pObjectAccessor->GetObjectManager()->m_PropertyEvents.AddEventHandler(
        ezMakeDelegate(&ezQtGreyBoxShapeWidget::ShadingPropertyEvent, this), m_ShadingUnsubscriber);

    }
    void DoPrepareToDie() override
    {
      m_ShadingUnsubscriber.Unsubscribe();
      SetReadOnly(true);
    }
  private:
    void UpdateSmoothState()
    {
      const auto& items = m_Items;
      if (m_pSmooth && !items.IsEmpty())
      {
        const QSignalBlocker blocker(m_pSmooth);
        const bool first = ezGetGreyBoxSmoothShading(*m_pObjectAccessor, items[0].m_pObject);
        bool mixed = false;
        for (const auto& item : items)
          mixed |= ezGetGreyBoxSmoothShading(*m_pObjectAccessor, item.m_pObject) != first;
        m_pSmooth->setTristate(mixed);
        m_pSmooth->setCheckState(mixed ? Qt::PartiallyChecked : first ? Qt::Checked
                                                                      : Qt::Unchecked);
      }
    }

    void ShadingPropertyEvent(const ezDocumentObjectPropertyEvent& e)
    {
      if (IsUndead())
        return;
      for (const auto& item : m_Items)
        if (e.m_pObject == item.m_pObject)
        {
          UpdateSmoothState();
          return;
        }
    }

    ezCopyOnBroadcastEvent<const ezDocumentObjectPropertyEvent&>::Unsubscriber m_ShadingUnsubscriber;
    QPointer<QLabel> m_pSmoothLabel;
    QPointer<QWidget> m_pSmoothRow;
    QPointer<QCheckBox> m_pSmooth;
  };
  void ExtendedPropertyMetaState(ezPropertyMetaStateEvent& e)
  {
    if (e.m_pObject->GetType()->GetTypeName() != "ezGreyBoxConeComponent")
      return;
    (*e.m_pPropertyStates)["SmoothShading"].m_Visibility = ezPropertyUiState::Invisible;
    const ezInt64 shape = e.m_pObject->GetTypeAccessor().GetValue("Shape").ConvertTo<ezInt64>();
    auto& props = *e.m_pPropertyStates;
    for (const char* name : {"BaseRadiusScale", "TopRadiusScale", "Sides", "HeightSegments", "ProfileCurve"})
      props[name].m_Visibility = shape == 13 ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
    for (const char* name : {"Detail", "Curvature", "Thickness", "SlopedTop", "SlopedBottom"})
      props[name].m_Visibility = ezPropertyUiState::Invisible;
    if (shape >= 5 && shape <= 12)
      props["Detail"].m_Visibility = ezPropertyUiState::Default;
    if (shape >= 6 && shape <= 12)
      props["Curvature"].m_Visibility = ezPropertyUiState::Default;
    if (shape >= 10 && shape <= 12)
      props["Thickness"].m_Visibility = ezPropertyUiState::Default;
    if ((shape >= 6 && shape <= 9) || shape == 12)
    {
      props["SlopedTop"].m_Visibility = ezPropertyUiState::Default;
      props["Detail"].m_sNewLabelText = "Steps";
    }
    if (shape == 12)
      props["SlopedBottom"].m_Visibility = ezPropertyUiState::Default;
  }

  void RegisterConeTranslations()
  {
    // Foundation owns this storage, so no plugin code remains in its virtual table after unload.
    ezUniquePtr<ezTranslatorStorage> translator = EZ_DEFAULT_NEW(ezTranslatorStorage);
    translator->StoreTranslation("Grey Boxing Extended", ezHashingUtils::StringHash("ezGreyBoxConeComponent"), ezTranslationUsage::Default);
    struct PropertyText
    {
      const char* m_szName;
      const char* m_szLabel;
      const char* m_szTooltip;
    };
    const PropertyText properties[] = {
      {"Shape", "Shape", "Choose a native Grey Boxing shape or a procedural cone. Switching component types supports Undo."},
      {"SizeNegX", "Size Neg X", "Distance from the local origin to the negative X side of the nominal size box."},
      {"SizePosX", "Size Pos X", "Distance from the local origin to the positive X side of the nominal size box."},
      {"SizeNegY", "Size Neg Y", "Distance from the local origin to the negative Y side of the nominal size box."},
      {"SizePosY", "Size Pos Y", "Distance from the local origin to the positive Y side of the nominal size box."},
      {"SizeNegZ", "Size Neg Z", "Distance from the local origin to the base along negative Z."},
      {"SizePosZ", "Size Pos Z", "Distance from the local origin to the top along positive Z."},
      {"BaseRadiusScale", "Base Radius Scale", "Base radius relative to half the Size X/Y dimensions. 1 is full size; 0 makes an inverted apex."},
      {"TopRadiusScale", "Top Radius Scale", "0 makes a point. Increase for a capped frustum; match the base radius for a column."},
      {"Sides", "Sides", "Number of sides around the cone (3-128). Use 4 for a square pyramid or 32 and above for a round cone."},
      {"HeightSegments", "Height Segments", "Subdivisions along Z (1-64). Use more segments to resolve the profile curve."},
      {"ProfileCurve", "Profile Curve", "Negative values pull the sides inward; positive values bulge outward. End radii stay fixed. Requires at least 2 height segments."},
      {"SmoothShading", "Smooth Shading", "Use smooth cone normals or the original engine normals for native shapes, preserving hard edges and caps. Disable for flat polygon faces."},
      {"GenerateCollision", "Generate Collision", "Include the cone in static collision generation."},
      {"UseAsOccluder", "Use As Occluder", "Allow this cone to occlude objects behind it."},
      {"Detail", "Detail", "Number of column sides, arch segments or stair steps."},
      {"Curvature", "Curvature", "Curvature in degrees, rounded to the engine's 5-degree increments."},
      {"Thickness", "Thickness", "Thickness used by the native geometry generator."},
      {"SlopedTop", "Sloped Top", "Use a sloped top surface."},
      {"SlopedBottom", "Sloped Bottom", "Use a sloped bottom surface."},
      {"Material", "Material", "Material for the cone; uses the Grey Boxing pattern when empty."},
      {"Color", "Color", "Tint color."},
      {"CustomData", "Custom Data", "Additional values passed to the material shader."},
    };
    for (const auto& property : properties)
    {
      ezStringBuilder key("ezGreyBoxConeComponent::", property.m_szName);
      const ezUInt64 hash = ezHashingUtils::StringHash(key);
      translator->StoreTranslation(property.m_szLabel, hash, ezTranslationUsage::Default);
      translator->StoreTranslation(property.m_szTooltip, hash, ezTranslationUsage::Tooltip);
    }
    ezTranslationLookup::AddTranslator(std::move(translator));
  }


  ezQtGreyBoxPropertyFeature* CreateExtended(ezQtGreyBoxCompositeWidget* host) { return new ezQtGreyBoxShapeWidget(host); }
  ezStatus BuildExtendedGeometry(ezObjectAccessorBase& accessor, const ezDocumentObject* object, ezGeometry& geometry)
  {
    ezGreyBoxConeSettings settings;
    EZ_SUCCEED_OR_RETURN(ezReadGreyBoxConeSettings(accessor, object, settings));
    return settings.BuildGeometry(geometry).Succeeded() ? ezStatus(EZ_SUCCESS) : ezStatus("Invalid Grey Boxing Extended geometry.");
  }
}
EZ_PLUGIN_ON_LOADED()
{
  RegisterConeTranslations();
  ezPropertyMetaState::GetSingleton()->m_Events.AddEventHandler(ExtendedPropertyMetaState);
  ezRegisterGreyBoxFeature(CreateExtended);
  ezSetGreyBoxGeometryProvider(BuildExtendedGeometry);
}
EZ_PLUGIN_ON_UNLOADED()
{
  ezSetGreyBoxGeometryProvider(nullptr);
  ezUnregisterGreyBoxFeature(CreateExtended);
  ezPropertyMetaState::GetSingleton()->m_Events.RemoveEventHandler(ExtendedPropertyMetaState);
}

