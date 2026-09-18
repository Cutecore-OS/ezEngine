#pragma once

#include <GrassPlugin/GrassPluginDLL.h>
#include <RendererCore/Meshes/CustomMeshComponent.h>

using ezGrassPatchComponentManager = ezComponentManager<class ezGrassPatchComponent, ezBlockStorageType::FreeList>;

/// Internal render proxy. Each patch participates independently in the world's visibility queries.
/// Created by ezGrassComponent and excluded from scene serialization.
class EZ_GRASSPLUGIN_DLL ezGrassPatchComponent : public ezCustomMeshComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezGrassPatchComponent, ezCustomMeshComponent, ezGrassPatchComponentManager);

public:
  float m_fViewDistance = 200.0f;
  ezUInt32 m_uiSelectionId = 0;

  bool IsWithinViewDistance(const ezVec3& vCameraPosition) const;

private:
  void OnMsgExtractRenderData(ezMsgExtractRenderData& ref_msg) const;
};
