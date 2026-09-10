#pragma once

#include <GreyBoxPlugin/Geometry/ConeGeometry.h>
#include <RendererCore/Meshes/MeshComponentBase.h>
#include <RendererCore/Rasterizer/RasterizerObject.h>

class ezMeshResourceDescriptor;
struct ezMsgBuildStaticMesh;
struct ezMsgExtractGeometry;
struct ezMsgExtractOccluderData;

struct EZ_GREYBOXPLUGIN_DLL ezGreyBoxConeShape
{
  using StorageType = ezUInt8;
  enum Enum
  {
    Box,
    RampPosX,
    RampNegX,
    RampPosY,
    RampNegY,
    Column,
    StairsPosX,
    StairsNegX,
    StairsPosY,
    StairsNegY,
    ArchX,
    ArchY,
    SpiralStairs,
    Cone = 13,
    Default = Cone
  };
};
EZ_DECLARE_REFLECTABLE_TYPE(EZ_GREYBOXPLUGIN_DLL, ezGreyBoxConeShape);

using ezGreyBoxConeComponentManager = ezComponentManager<class ezGreyBoxConeComponent, ezBlockStorageType::Compact>;

/// Procedural cone, pyramid or frustum. Uses the standard mesh renderer and static collision export.
class EZ_GREYBOXPLUGIN_DLL ezGreyBoxConeComponent : public ezMeshComponentBase
{
  EZ_DECLARE_COMPONENT_TYPE(ezGreyBoxConeComponent, ezMeshComponentBase, ezGreyBoxConeComponentManager);

public:
  ezGreyBoxConeComponent() = default;
  ~ezGreyBoxConeComponent() = default;
  void SerializeComponent(ezWorldWriter& stream) const override;
  void DeserializeComponent(ezWorldReader& stream) override;
  ezEnum<ezGreyBoxConeShape> GetShape() const { return static_cast<ezGreyBoxConeShape::Enum>(m_Settings.m_uiShape); }
  void SetShape(ezEnum<ezGreyBoxConeShape> shape);

  float GetSizeNegX() const { return m_Settings.m_vNegative.x; }
  void SetSizeNegX(float value);
  float GetSizePosX() const { return m_Settings.m_vPositive.x; }
  void SetSizePosX(float value);
  float GetSizeNegY() const { return m_Settings.m_vNegative.y; }
  void SetSizeNegY(float value);
  float GetSizePosY() const { return m_Settings.m_vPositive.y; }
  void SetSizePosY(float value);
  float GetSizeNegZ() const { return m_Settings.m_vNegative.z; }
  void SetSizeNegZ(float value);
  float GetSizePosZ() const { return m_Settings.m_vPositive.z; }
  void SetSizePosZ(float value);
  float GetBaseRadiusScale() const { return m_Settings.m_fBaseRadiusScale; }
  void SetBaseRadiusScale(float value);
  float GetTopRadiusScale() const { return m_Settings.m_fTopRadiusScale; }
  void SetTopRadiusScale(float value);
  ezUInt32 GetSides() const { return m_Settings.m_uiSides; }
  void SetSides(ezUInt32 value);
  ezUInt32 GetHeightSegments() const { return m_Settings.m_uiHeightSegments; }
  void SetHeightSegments(ezUInt32 value);
  float GetProfileCurve() const { return m_Settings.m_fProfileCurve; }
  void SetProfileCurve(float value);
  bool GetSmoothShading() const { return m_Settings.m_bSmoothShading; }
  void SetSmoothShading(bool value);

  ezUInt32 GetDetail() const { return m_Settings.m_uiDetail; }
  void SetDetail(ezUInt32 value);
  ezAngle GetCurvature() const { return m_Settings.m_Curvature; }
  void SetCurvature(ezAngle value);
  float GetThickness() const { return m_Settings.m_fThickness; }
  void SetThickness(float value);
  bool GetSlopedTop() const { return m_Settings.m_bSlopedTop; }
  void SetSlopedTop(bool value);
  bool GetSlopedBottom() const { return m_Settings.m_bSlopedBottom; }
  void SetSlopedBottom(bool value);
  void SetMaterial(ezMaterialResourceHandle material) { ezMeshComponentBase::SetMaterial(0, material); }
  ezMaterialResourceHandle GetMaterial() const { return ezMeshComponentBase::GetMaterial(0); }
  bool GetGenerateCollision() const { return m_bGenerateCollision; }
  void SetGenerateCollision(bool value) { m_bGenerateCollision = value; }
  bool GetUseAsOccluder() const { return m_bUseAsOccluder; }
  void SetUseAsOccluder(bool value);
  const ezGreyBoxConeSettings& GetSettings() const { return m_Settings; }
  void OnBuildStaticMesh(ezMsgBuildStaticMesh& msg) const;
  void OnMsgExtractGeometry(ezMsgExtractGeometry& msg) const;
  void OnMsgExtractOccluderData(ezMsgExtractOccluderData& msg) const;

protected:
  void OnActivated() override;
  ezResult GetLocalBounds(ezBoundingBoxSphere& bounds, bool& alwaysVisible, ezMsgUpdateLocalBounds& msg) override;

private:
  void GeometryChanged();
  void RebuildMesh();
  bool BuildDescriptor(ezMeshResourceDescriptor& descriptor) const;
  ezGreyBoxConeSettings m_Settings;
  bool m_bGenerateCollision = true;
  bool m_bUseAsOccluder = true;
  mutable ezSharedPtr<const ezRasterizerObject> m_pOccluder;
};
