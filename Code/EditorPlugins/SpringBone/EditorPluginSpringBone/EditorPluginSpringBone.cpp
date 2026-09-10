#include <EditorPluginSpringBone/EditorPluginSpringBonePCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/GUI/ExposedParameters.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/Strings/TranslationLookup.h>
#include <GuiFoundation/PropertyGrid/PropertyMetaState.h>
#include <GuiFoundation/UIServices/DynamicStringEnum.h>
#include <ToolsFoundation/Document/Document.h>
#include <ToolsFoundation/Object/DocumentObjectBase.h>
#include <ToolsFoundation/Selection/SelectionManager.h>

namespace
{
  struct SettingHelp
  {
    const char* m_szProperty;
    const char* m_szHelp;
  };

  static const SettingHelp s_SettingHelp[] = {
    {"Mass", "Mass of one simulated bone in kilograms."},
    {"Stiffness", "Angular spring frequency in Hz. Higher values return to the animation target faster."},
    {"Damping", "Spring damping ratio. Start at 1 for a quick return with little overshoot."},
    {"RootStiffness", "Transverse translation spring frequency in Hz. Requires a nonzero MaxDistance."},
    {"LengthStiffness", "Translation spring frequency along the bone in Hz. Requires a nonzero MaxDistance."},
    {"Softening", "Reduces spring frequencies, down to 10 percent at 1."},
    {"Influence", "Blend between animation (0) and simulated pose (1). Bodies continue simulating at 0."},
    {"MaxMotorForce", "Maximum motor force in N and torque in Nm. Limits how closely the body follows its target."},
    {"MaxDistance", "Translation limit per axis in meters at object scale 1. Zero pins the base. Linear force at a pinned pivot cannot rotate the bone; use SwayAngles or a ForceApplicationOffset."},
    {"MaxAngle", "Angular limits relative to the initial joint pose. Limits and locked axes take priority over sway and animation targets."},
    {"LockRotationX", "Lock X rotation in the initial joint frame."},
    {"LockRotationY", "Lock Y rotation in the initial joint frame."},
    {"LockRotationZ", "Lock Z rotation in the initial joint frame."},
    {"GravityFactor", "Multiplier of Jolt world gravity. Zero disables gravity."},
    {"AirDrag", "Linear and angular resistance to motion, in inverse seconds."},
    {"Friction", "Additional angular damping and joint friction. Bone Surface determines contact friction."},
    {"ExternalAcceleration", "Constant world-space linear acceleration in m/s^2. Uses ForceApplicationOffset."},
    {"SwayAngles", "Desired angular amplitudes XYZ in degrees, added to the local animation rotation. Works with a pinned base and without Bone Shapes. Actual rotation depends on motors, limits and collisions. Zero disables this target motion."},
    {"SwayAcceleration", "Oscillating linear acceleration XYZ in owner coordinates, in m/s^2. This is a force, not an angular amplitude. With a pinned base, use a nonzero ForceApplicationOffset to produce torque when the center of mass lies at the pivot."},
    {"SwayAngularAcceleration", "Oscillating angular acceleration XYZ in local bone coordinates, in deg/s^2. Converted to torque using body inertia. Actual motion depends on stiffness and limits."},
    {"SwayFrequency", "Base frequency in Hz. Changes preserve the current phase. Zero freezes the waveform; set amplitudes to zero to stop applying it."},
    {"SwayPhase", "Common phase offset in radians."},
    {"SwayFrequencyScale", "Independent XYZ frequency multipliers. Changes preserve phase continuity."},
    {"SwayAxisPhase", "Additional XYZ phase offsets in radians."},
    {"SwayBonePhase", "Phase offset per skeleton joint index in radians, before the XYZ multipliers."},
    {"ForceApplicationOffset", "Force application point relative to the center of mass, in local bone axes and meters. Applies to external acceleration, linear sway and wind. Creates torque without adding a collider. At zero, linear accelerations act at the center of mass; wind automatically acts at the midpoint of the longest child segment. Terminal bones use their parent segment length."},
    {"WindInfluence", "Influence of the world's wind, including Simple Wind and Wind Volumes. Independent of self motion."},
    {"WindFlutter", "Additional wind flutter. Requires wind; self motion works without wind."},
    {"Settings", "All displayed values directly control simulation and apply live. An override completely replaces the common settings for its bone."},
    {"BoneOverrides", "Each entry fully replaces Settings for its bone. Enabled can include bones outside the selected subtree. Last matching entry wins."},
    {"SelfCollision", "Collide simulated Bone Shapes, except directly connected parent and child."},
    {"CollideWithSkeleton", "Collide with Bone Shapes of animated bones. Bones without shapes have no collision geometry."},
  };

  void PropertyMetaState(ezPropertyMetaStateEvent& e)
  {
    const ezStringView type = e.m_pObject->GetType()->GetTypeName();
    if (type != "ezSpringBoneSettings" && type != "ezSpringBoneComponent" && type != "ezSpringBoneOverride")
      return;
    for (const auto& help : s_SettingHelp)
      if (e.m_pObject->GetType()->FindPropertyByName(help.m_szProperty))
      {
        ezStringBuilder key("SpringBone.", help.m_szProperty);
        (*e.m_pPropertyStates)[help.m_szProperty].m_sNewLabelText = key;
      }
  }

  void RegisterSettingHelp()
  {
    // Use Foundation's storage implementation so no plugin vtable remains after unloading.
    ezUniquePtr<ezTranslatorStorage> translator = EZ_DEFAULT_NEW(ezTranslatorStorage);
    for (const auto& help : s_SettingHelp)
    {
      ezStringBuilder key("SpringBone.", help.m_szProperty);
      const ezUInt64 hash = ezHashingUtils::StringHash(key);
      translator->StoreTranslation(help.m_szProperty, hash, ezTranslationUsage::Default);
      translator->StoreTranslation(help.m_szHelp, hash, ezTranslationUsage::Tooltip);
    }
    ezTranslationLookup::AddTranslator(std::move(translator));
  }

  void CollectBoneNames(const ezDocumentObject* pObject, ezDynamicStringEnum& names)
  {
    if (pObject->GetType()->GetTypeName() == "ezSpringBoneComponent"_ezsv)
    {
      const ezVariant value = pObject->GetTypeAccessor().GetValue("Skeleton");
      if (value.IsA<ezString>())
      {
        const auto asset = ezAssetCurator::GetSingleton()->FindSubAsset(value.Get<ezString>());
        if (asset)
        {
          if (const auto* parameters = asset->m_pAssetInfo->m_Info->GetMetaInfo<ezExposedParameters>())
            for (const auto* parameter : parameters->m_Parameters)
              if (parameter->m_sName != "<root-transform>")
                names.AddValidValue(parameter->m_sName, true);
        }
      }
    }
    for (const auto* child : pObject->GetChildren())
      CollectBoneNames(child, names);
  }

  void RefreshBoneNames(ezDynamicStringEnum::RefreshValuesEvent& e)
  {
    if (e.m_sEnumName != "SpringBoneNames"_ezsv)
      return;
    e.m_pEnum->Clear();
    if (e.m_pDocument == nullptr)
      return;
    for (const auto* selected : e.m_pDocument->GetSelectionManager()->GetSelection())
    {
      // Selection can be a game object, the component itself, or a nested settings object.
      const ezDocumentObject* object = selected;
      while (object->GetParent() && object->GetType()->GetTypeName() != "ezGameObject"_ezsv)
        object = object->GetParent();
      CollectBoneNames(object, *e.m_pEnum);
    }
    e.m_pEnum->SortValues();
  }
} // namespace

EZ_PLUGIN_ON_LOADED()
{
  RegisterSettingHelp();
  ezPropertyMetaState::GetSingleton()->m_Events.AddEventHandler(PropertyMetaState);
  ezDynamicStringEnum::s_RefreshValuesEvent.AddEventHandler(RefreshBoneNames);
}

EZ_PLUGIN_ON_UNLOADED()
{
  ezPropertyMetaState::GetSingleton()->m_Events.RemoveEventHandler(PropertyMetaState);
  ezDynamicStringEnum::s_RefreshValuesEvent.RemoveEventHandler(RefreshBoneNames);
}
