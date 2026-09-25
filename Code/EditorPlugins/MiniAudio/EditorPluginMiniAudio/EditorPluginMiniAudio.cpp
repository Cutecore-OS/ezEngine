#include <EditorPluginMiniAudio/EditorPluginMiniAudioPCH.h>

#include <EditorFramework/Actions/AssetActions.h>
#include <EditorFramework/Actions/ProjectActions.h>
#include <EditorPluginMiniAudio/Actions/MiniAudioActions.h>
#include <EditorPluginMiniAudio/Preferences/MiniAudioPreferences.h>
#include <EditorPluginMiniAudio/SoundAsset/MiniAudioSoundAsset.h>
#include <GuiFoundation/Action/CommandHistoryActions.h>
#include <GuiFoundation/Action/DocumentActions.h>
#include <GuiFoundation/Action/StandardMenus.h>
#include <GuiFoundation/PropertyGrid/PropertyMetaState.h>
#include <GuiFoundation/UIServices/DynamicStringEnum.h>
#include <MiniAudioPlugin/Components/MiniAudioEffectVolumeComponent.h>
#include <MiniAudioPlugin/Components/MiniAudioSoundVolumeComponent.h>
#include <MiniAudioPlugin/Effects/MiniAudioEffect.h>
#include <ToolsFoundation/Object/DocumentObjectBase.h>

#include <EditorFramework/DocumentWindow/EngineDocumentWindow.moc.h>
#include <EditorFramework/Gizmos/SphereGizmo.h>
#include <EditorFramework/Manipulators/ManipulatorAdapterRegistry.h>
#include <ToolsFoundation/Object/ObjectAccessorBase.h>

// The stock sphere adapter scales with the owner and has no capture offset.
// Keep the audio-specific behavior local to MiniAudio.
class ezMiniAudioOcclusionManipulatorAdapter : public ezManipulatorAdapter
{
protected:
  virtual void Finalize() override
  {
    auto* pDocument = m_pObject->GetDocumentObjectManager()->GetDocument()->GetMainDocument();
    auto* pWindow = qobject_cast<ezQtEngineDocumentWindow*>(ezQtDocumentWindow::FindWindowByDocument(pDocument));
    EZ_ASSERT_DEV(pWindow != nullptr, "Audio manipulators require an engine document window.");
    m_Gizmo.SetOwner(pWindow, nullptr);
    m_Gizmo.m_GizmoEvents.AddEventHandler(ezMakeDelegate(&ezMiniAudioOcclusionManipulatorAdapter::GizmoEventHandler, this));
  }

  virtual void Update() override { UpdateGizmoTransform(); }

  virtual void UpdateGizmoTransform() override
  {
    auto* pAccessor = GetObjectAccessor();
    ezTransform transform = GetObjectTransform();
    if (const auto* pOffset = GetProperty("CaptureOffset"))
      transform.m_vPosition = transform.TransformPosition(pAccessor->Get<ezVec3>(m_pObject, pOffset));
    transform.m_vScale = ezVec3(1);
    m_Gizmo.SetTransformation(transform);
    const float fRadius = pAccessor->Get<float>(m_pObject, GetProperty("OcclusionRadius"));
    m_Gizmo.SetOuterSphere(ezMath::IsFinite(fRadius) ? ezMath::Max(0.0f, fRadius) : 1.0f);
    m_Gizmo.SetVisible(m_bManipulatorIsVisible && pAccessor->Get<bool>(m_pObject, GetProperty("UseOcclusion")));
  }

  void GizmoEventHandler(const ezGizmoEvent& e)
  {
    switch (e.m_Type)
    {
      case ezGizmoEvent::Type::BeginInteractions:
        BeginTemporaryInteraction();
        break;
      case ezGizmoEvent::Type::CancelInteractions:
        CancelTemporayInteraction();
        break;
      case ezGizmoEvent::Type::EndInteractions:
        EndTemporaryInteraction();
        break;
      case ezGizmoEvent::Type::Interaction:
        ChangeProperties("OcclusionRadius", m_Gizmo.GetOuterRadius());
        break;
    }
  }

  ezSphereGizmo m_Gizmo;
};

static void ToolsProjectEventHandler(const ezToolsProjectEvent& e);

static void EffectPropertyMetaStateEventHandler(ezPropertyMetaStateEvent& e)
{
  if (e.m_pObject->GetTypeAccessor().GetType()->IsDerivedFrom(ezGetStaticRTTI<ezMiniAudioSoundComponent>()))
  {
    const bool bOcclusion = e.m_pObject->GetTypeAccessor().GetValue("UseOcclusion").ConvertTo<bool>();
    (*e.m_pPropertyStates)["OcclusionRadius"].m_Visibility = bOcclusion ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
    (*e.m_pPropertyStates)["OcclusionRange"].m_Visibility = bOcclusion ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
    (*e.m_pPropertyStates)["OcclusionThreshold"].m_Visibility = bOcclusion ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
    (*e.m_pPropertyStates)["OcclusionCollisionLayer"].m_Visibility = bOcclusion ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
  }
  if (e.m_pObject->GetTypeAccessor().GetType()->IsDerivedFrom(ezGetStaticRTTI<ezMiniAudioEffectVolumeComponent>()) ||
      e.m_pObject->GetTypeAccessor().GetType()->IsDerivedFrom(ezGetStaticRTTI<ezMiniAudioSoundVolumeComponent>()))
  {
    const bool bByTime = e.m_pObject->GetTypeAccessor().GetValue("InterpolateByTime").ConvertTo<bool>();
    (*e.m_pPropertyStates)["Falloff"].m_Visibility = bByTime ? ezPropertyUiState::Invisible : ezPropertyUiState::Default;
    (*e.m_pPropertyStates)["InterpolationDuration"].m_Visibility = bByTime ? ezPropertyUiState::Default : ezPropertyUiState::Invisible;
    return;
  }
  if (!e.m_pObject->GetTypeAccessor().GetType()->IsDerivedFrom(ezGetStaticRTTI<ezMiniAudioEffect>()))
    return;

  const auto type = e.m_pObject->GetTypeAccessor().GetValue("Type").ConvertTo<ezInt64>();
  auto show = [&](const char* szProperty, bool bVisible)
  { (*e.m_pPropertyStates)[szProperty].m_Visibility = bVisible ? ezPropertyUiState::Default : ezPropertyUiState::Invisible; };
  const bool bReverb = type == ezMiniAudioEffectType::Reverb;
  const bool bDelay = type == ezMiniAudioEffectType::Delay;
  const bool bModulation = type == ezMiniAudioEffectType::Chorus || type == ezMiniAudioEffectType::Flanger;
  const bool bFilter = type == ezMiniAudioEffectType::Muffling || type == ezMiniAudioEffectType::BandPass || type == ezMiniAudioEffectType::HighPass || type == ezMiniAudioEffectType::LowPass || type == ezMiniAudioEffectType::ParametricEqualizer;
  const bool bDynamics = type == ezMiniAudioEffectType::Compressor || type == ezMiniAudioEffectType::Limiter;
  show("Mix", type != ezMiniAudioEffectType::Speed && type != ezMiniAudioEffectType::Pitch && type != ezMiniAudioEffectType::TimeStretch && type != ezMiniAudioEffectType::Volume && type != ezMiniAudioEffectType::Panner && !bDynamics);
  show("Delay", bDelay);
  show("Feedback", bDelay || bModulation);
  show("Decay", bReverb);
  show("RoomSize", bReverb);
  show("Damping", bReverb);
  show("Rate", bModulation);
  show("Depth", bModulation);
  show("Frequency", bFilter);
  show("Q", bFilter);
  show("Gain", type == ezMiniAudioEffectType::ParametricEqualizer);
  show("Speed", type == ezMiniAudioEffectType::Speed || type == ezMiniAudioEffectType::TimeStretch);
  show("Volume", type == ezMiniAudioEffectType::Volume || type == ezMiniAudioEffectType::Muffling);
  show("Pitch", type == ezMiniAudioEffectType::Pitch);
  show("Threshold", bDynamics);
  show("Ratio", type == ezMiniAudioEffectType::Compressor);
  show("AttackTime", type == ezMiniAudioEffectType::Compressor);
  show("ReleaseTime", bDynamics);
  show("Drive", type == ezMiniAudioEffectType::Distortion);
  show("Bits", type == ezMiniAudioEffectType::Bitcrusher);
  show("Downsample", type == ezMiniAudioEffectType::Bitcrusher);
  show("Pan", type == ezMiniAudioEffectType::Panner);
}

void OnLoadPlugin()
{
  ezManipulatorAdapterRegistry::GetSingleton()->m_Factory.RegisterCreator(ezGetStaticRTTI<ezMiniAudioOcclusionManipulatorAttribute>(),
    [](const ezRTTI*) -> ezManipulatorAdapter*
    { return EZ_DEFAULT_NEW(ezMiniAudioOcclusionManipulatorAdapter); });
  ezToolsProject::GetSingleton()->s_Events.AddEventHandler(ToolsProjectEventHandler);
  ezPropertyMetaState::GetSingleton()->m_Events.AddEventHandler(ezMiniAudioSoundAssetProperties::PropertyMetaStateEventHandler);

  ezPropertyMetaState::GetSingleton()->m_Events.AddEventHandler(EffectPropertyMetaStateEventHandler);

  // Mesh
  {
    // Menu Bar
    ezActionMapManager::RegisterActionMap("MiniAudioSoundAssetMenuBar", "AssetMenuBar");

    // Tool Bar
    {
      ezActionMapManager::RegisterActionMap("MiniAudioSoundAssetToolBar", "AssetToolbar");
    }
  }

  // Scene
  {
    // Menu Bar
    {
      ezMiniAudioActions::RegisterActions();
      ezMiniAudioActions::MapPluginMenuActions("AssetMenuBar");
      ezMiniAudioActions::MapMenuActions("EditorPluginScene_DocumentMenuBar");
      ezMiniAudioActions::MapMenuActions("EditorPluginScene_Scene2MenuBar");
      ezMiniAudioActions::MapToolbarActions("EditorPluginScene_DocumentToolBar");
      ezMiniAudioActions::MapToolbarActions("EditorPluginScene_Scene2ToolBar");
    }
  }
}

void OnUnloadPlugin()
{
  ezManipulatorAdapterRegistry::GetSingleton()->m_Factory.UnregisterCreator(ezGetStaticRTTI<ezMiniAudioOcclusionManipulatorAttribute>());
  ezPropertyMetaState::GetSingleton()->m_Events.RemoveEventHandler(EffectPropertyMetaStateEventHandler);
  ezMiniAudioActions::UnregisterActions();
  ezToolsProject::GetSingleton()->s_Events.RemoveEventHandler(ToolsProjectEventHandler);
  ezPropertyMetaState::GetSingleton()->m_Events.RemoveEventHandler(ezMiniAudioSoundAssetProperties::PropertyMetaStateEventHandler);
}

static void ToolsProjectEventHandler(const ezToolsProjectEvent& e)
{
  if (e.m_Type == ezToolsProjectEvent::Type::ProjectOpened)
  {
    ezMiniAudioProjectPreferences* pPreferences = ezPreferences::QueryPreferences<ezMiniAudioProjectPreferences>();
    pPreferences->SyncCVars();
    ezDynamicStringEnum::GetDynamicEnum("MiniAudioSoundGroups").AddValidValue("Default", true);
  }
}

EZ_PLUGIN_ON_LOADED()
{
  OnLoadPlugin();
}

EZ_PLUGIN_ON_UNLOADED()
{
  OnUnloadPlugin();
}
