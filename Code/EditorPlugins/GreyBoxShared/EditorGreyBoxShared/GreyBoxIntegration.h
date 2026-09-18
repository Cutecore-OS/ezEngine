#pragma once
#include <EditorGreyBoxShared/EditorGreyBoxSharedDLL.h>
#include <GuiFoundation/PropertyGrid/PropertyBaseWidget.moc.h>
#include <Foundation/Types/Status.h>

class QComboBox;
class ezGeometry;
class ezQtGreyBoxCompositeWidget;

/// A feature contributed by an independently enabled editor plugin.
class EZ_EDITORGREYBOXSHARED_DLL ezQtGreyBoxPropertyFeature : public QWidget
{
public:
  explicit ezQtGreyBoxPropertyFeature(ezQtGreyBoxCompositeWidget* pHost);
  virtual ~ezQtGreyBoxPropertyFeature() = default;
  void Initialize(ezQtPropertyGridWidget* grid, ezObjectAccessorBase* accessor,
    const ezRTTI* type, const ezAbstractProperty* property, QComboBox* combo);
  virtual void SetSelection(const ezArrayPtr<ezPropertySelection>& items);
  virtual void SetReadOnly(bool bReadOnly = true) {}
  void PrepareToDie();
  virtual void OnBeforeChange() {}

protected:
  virtual void OnInit() {}
  virtual void DoPrepareToDie() {}
  bool IsUndead() const;
  void BroadcastValueChanged(const ezVariant& value);
  void DisconnectDefaultEnum();
  void BeforeChange();
  ezQtPropertyGridWidget* m_pGrid = nullptr;
  ezObjectAccessorBase* m_pObjectAccessor = nullptr;
  const ezRTTI* m_pType = nullptr;
  const ezAbstractProperty* m_pProp = nullptr;
  QComboBox* m_pWidget = nullptr;
  ezHybridArray<ezPropertySelection, 8> m_Items;

private:
  ezQtGreyBoxCompositeWidget* m_pHost;
  bool m_bUndead = false;
};

using ezGreyBoxFeatureCreator = ezQtGreyBoxPropertyFeature* (*)(ezQtGreyBoxCompositeWidget*);
EZ_EDITORGREYBOXSHARED_DLL void ezRegisterGreyBoxFeature(ezGreyBoxFeatureCreator creator);
EZ_EDITORGREYBOXSHARED_DLL void ezUnregisterGreyBoxFeature(ezGreyBoxFeatureCreator creator);
EZ_EDITORGREYBOXSHARED_DLL bool ezIsGreyBoxComponent(const ezDocumentObject* object);

using ezGreyBoxGeometryProvider = ezStatus (*)(ezObjectAccessorBase&, const ezDocumentObject*, ezGeometry&);
EZ_EDITORGREYBOXSHARED_DLL void ezSetGreyBoxGeometryProvider(ezGreyBoxGeometryProvider provider);
EZ_EDITORGREYBOXSHARED_DLL ezStatus ezBuildGreyBoxExtensionGeometry(ezObjectAccessorBase& accessor,
  const ezDocumentObject* object, ezGeometry& geometry);

