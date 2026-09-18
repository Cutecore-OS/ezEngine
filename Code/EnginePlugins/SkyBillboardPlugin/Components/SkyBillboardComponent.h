#pragma once

#include <RendererCore/Meshes/MeshComponentBase.h>
#include <RendererCore/Textures/Texture2DResource.h>
#include <SkyBillboardPlugin/SkyBillboardPluginDLL.h>

using ezSkyBillboardComponentManager = ezComponentManager<class ezSkyBillboardComponent, ezBlockStorageType::Compact>;

/// Draws a texture at infinity, behind scene geometry. Only the owner's rotation is used.
/// A flat image faces local +X with local +Z up; a cubemap uses the standard skybox orientation.
class EZ_SKYBILLBOARDPLUGIN_DLL ezSkyBillboardComponent : public ezRenderComponent
{
  EZ_DECLARE_COMPONENT_TYPE(ezSkyBillboardComponent, ezRenderComponent, ezSkyBillboardComponentManager);

public:
  ezSkyBillboardComponent();
  ~ezSkyBillboardComponent();

  virtual void SerializeComponent(ezWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(ezWorldReader& inout_stream) override;
  virtual ezResult GetLocalBounds(ezBoundingBoxSphere& ref_bounds, bool& ref_bAlwaysVisible, ezMsgUpdateLocalBounds& ref_msg) override;

  EZ_ADD_RESOURCEHANDLE_ACCESSORS_WITH_SETTER(Texture, m_hTexture, SetTexture);
  void SetTexture(const ezTexture2DResourceHandle& hTexture);
  const ezTexture2DResourceHandle& GetTexture() const { return m_hTexture; }

  EZ_ADD_RESOURCEHANDLE_ACCESSORS_WITH_SETTER(CubeMap, m_hCubeMap, SetCubeMap);
  void SetCubeMap(const ezTextureCubeResourceHandle& hCubeMap);
  const ezTextureCubeResourceHandle& GetCubeMap() const { return m_hCubeMap; }

  void SetUseCubeMap(bool bUseCubeMap);
  bool GetUseCubeMap() const { return m_bUseCubeMap; }

  /// Vertical angular size of the flat image, in degrees. Does not affect cubemaps.
  void SetSize(float fSize);
  float GetSize() const { return m_fSize; }
  /// Width divided by height of the flat image.
  void SetAspectRatio(float fAspectRatio);
  float GetAspectRatio() const { return m_fAspectRatio; }
  void SetColor(const ezColor& color);
  const ezColor& GetColor() const { return m_Color; }
  void SetGlowColor(const ezColor& color);
  const ezColor& GetGlowColor() const { return m_GlowColor; }
  void SetGlowIntensity(float fIntensity);
  float GetGlowIntensity() const { return m_fGlowIntensity; }
  void SetOpacity(float fOpacity);
  float GetOpacity() const { return m_fOpacity; }
  void SetAdditive(bool bAdditive);
  bool GetAdditive() const { return m_bAdditive; }
  /// Higher layers are drawn over lower layers. Use distinct values for overlapping images.
  void SetLayer(ezUInt16 uiLayer);
  ezUInt16 GetLayer() const { return m_uiLayer; }

protected:
  virtual void Initialize() override;
  virtual void OnDeactivated() override;

private:
  void OnMsgExtractRenderData(ezMsgExtractRenderData& msg) const;
  void UpdateMaterial();

  ezTexture2DResourceHandle m_hTexture;
  ezTextureCubeResourceHandle m_hCubeMap;
  ezMeshResourceHandle m_hMesh;
  ezMaterialResourceHandle m_hMaterial;
  mutable ezInstanceDataOffset m_InstanceDataOffset;
  ezColor m_Color = ezColor::White;
  ezColor m_GlowColor = ezColor::White;
  float m_fSize = 10.0f;
  float m_fAspectRatio = 1.0f;
  float m_fGlowIntensity = 0.0f;
  float m_fOpacity = 1.0f;
  ezUInt16 m_uiLayer = 0;
  bool m_bUseCubeMap = false;
  bool m_bAdditive = false;
};
