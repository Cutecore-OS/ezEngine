#include <Core/ResourceManager/ResourceManager.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <TestFramework/Framework/TestFramework.h>
#include <TestFramework/Utilities/TestSetup.h>

EZ_TESTFRAMEWORK_ENTRY_POINT("SkyBillboardPluginTest", "Sky Billboard Plugin Tests")
EZ_CREATE_SIMPLE_TEST_GROUP(SkyBillboard);

void TestSkyBillboardMaterial(ezGALDevice* pDevice);

EZ_CREATE_SIMPLE_TEST(SkyBillboard, RenderingAndSerialization)
{
  ezStartup::StartupCoreSystems();
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Base", "SkyBillboardTests", "base").Succeeded());
  EZ_TEST_BOOL(ezFileSystem::AddDataDirectory(">sdk/Data/Plugins/SkyBillboardPlugin", "SkyBillboardTests", "sky").Succeeded());
  ezGALDeviceCreationDescription desc;
  ezGALDevice* pDevice = ezGALDeviceFactory::CreateDevice("DX11", ezFoundation::GetDefaultAllocator(), desc);
  if (!EZ_TEST_BOOL(pDevice != nullptr && pDevice->Init().Succeeded()))
    return;
  ezGALDevice::SetDefaultDevice(pDevice);
  ezStartup::StartupHighLevelSystems();
  TestSkyBillboardMaterial(pDevice);
  ezStartup::ShutdownHighLevelSystems();
  ezResourceManager::FreeAllUnusedResources();
  pDevice->Shutdown().IgnoreResult();
  EZ_DEFAULT_DELETE(pDevice);
  ezFileSystem::RemoveDataDirectoryGroup("SkyBillboardTests");
  ezStartup::ShutdownCoreSystems();
}
