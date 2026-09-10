#include <Core/ResourceManager/ResourceManager.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>

EZ_TESTFRAMEWORK_ENTRY_POINT("DirectionalTargetIndicatorPluginTest", "Directional Target Indicator Plugin Tests")
EZ_CREATE_SIMPLE_TEST_GROUP(DirectionalTargetIndicator);

void TestDirectionalTargetIndicatorMaterial(ezGALDevice* pDevice);

EZ_CREATE_SIMPLE_TEST(DirectionalTargetIndicator, RenderingAndSerialization)
{
  ezStartup::StartupCoreSystems();
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Base", "DirectionalTargetIndicatorTests", "base").Succeeded());
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Plugins/DirectionalTargetIndicatorPlugin", "DirectionalTargetIndicatorTests", "sky").Succeeded());
  ezGALDeviceCreationDescription desc;
  ezGALDevice* pDevice = ezGALDeviceFactory::CreateDevice("DX11", ezFoundation::GetDefaultAllocator(), desc);
  if (!EZ_TEST_BOOL(pDevice != nullptr && pDevice->Init().Succeeded()))
    return;
  ezGALDevice::SetDefaultDevice(pDevice);
  ezStartup::StartupHighLevelSystems();
  TestDirectionalTargetIndicatorMaterial(pDevice);
  ezStartup::ShutdownHighLevelSystems();
  ezResourceManager::FreeAllUnusedResources();
  pDevice->Shutdown().IgnoreResult();
  EZ_DEFAULT_DELETE(pDevice);
  ezFileSystem::RemoveDataDirectoryGroup("DirectionalTargetIndicatorTests");
  ezStartup::ShutdownCoreSystems();
}

#include <DirectionalTargetIndicatorPlugin/Rendering/DirectionalTargetIndicatorUtils.h>

EZ_CREATE_SIMPLE_TEST(DirectionalTargetIndicator, RoundedScreenBoundary)
{
  using namespace ezDirectionalTargetIndicatorUtils;
  const ezVec2 vSize(1000, 500);
  const ezVec2 vExtent(10, 10);
  const ezVec4 vSides(1, 2, 3, 4);
  // Common 5% plus each side. Layer extents are included as well.
  EZ_TEST_VEC2(GetRoundedEdgePosition(ezVec2(1, 0), vSize, vExtent, 5, vSides, 0), ezVec2(420, -2.5f), 0.001f);
  EZ_TEST_VEC2(GetRoundedEdgePosition(ezVec2(-1, 0), vSize, vExtent, 5, vSides, 0), ezVec2(-430, -2.5f), 0.001f);
  EZ_TEST_VEC2(GetRoundedEdgePosition(ezVec2(0, -1), vSize, vExtent, 5, vSides, 0), ezVec2(-5, -200), 0.001f);
  EZ_TEST_VEC2(GetRoundedEdgePosition(ezVec2(0, 1), vSize, vExtent, 5, vSides, 0), ezVec2(-5, 195), 0.001f);

  for (float fX : {-1.0f, 1.0f})
  {
    for (float fY : {-1.0f, 1.0f})
    {
      const ezVec2 vDirection = ezVec2(fX, fY).GetNormalized();
      const ezVec2 vPoint = GetRoundedEdgePosition(vDirection, ezVec2(100), ezVec2::MakeZero(), 0, ezVec4::MakeZero(), 20);
      const float fExpected = 30.0f + 20.0f / ezMath::Sqrt(2.0f);
      EZ_TEST_VEC2(vPoint, ezVec2(fX, fY) * fExpected, 0.001f);
      // Same proportions at twice the resolution, including on non-square viewports.
      const ezVec2 vBase = GetRoundedEdgePosition(vDirection, vSize, vExtent, 5, vSides, 10);
      EZ_TEST_VEC2(GetRoundedEdgePosition(vDirection, vSize * 2.0f, vExtent * 2.0f, 5, vSides, 10), vBase * 2.0f, 0.001f);
    }
  }
  const ezVec2 vDiagonal = ezVec2(1, 1).GetNormalized();
  EZ_TEST_VEC2(GetRoundedEdgePosition(vDiagonal, ezVec2(100), ezVec2::MakeZero(), 0, ezVec4::MakeZero(), 50), vDiagonal * 50.0f, 0.001f);
  EZ_TEST_VEC2(GetRoundedEdgePosition(vDiagonal, ezVec2(100), ezVec2(1000), 50, ezVec4(50), 50), ezVec2::MakeZero(), 0.001f);
  // The straight-to-arc join remains continuous.
  const ezVec2 vJoin = ezVec2(50, 30).GetNormalized();
  EZ_TEST_VEC2(GetRoundedEdgePosition(vJoin, ezVec2(100), ezVec2::MakeZero(), 0, ezVec4::MakeZero(), 20), ezVec2(50, 30), 0.001f);
}
