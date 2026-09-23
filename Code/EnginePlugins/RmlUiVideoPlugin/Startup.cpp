#include <RmlUiVideoPlugin/RmlUiVideoPluginPCH.h>

#include <Foundation/Configuration/Startup.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUiPlugin/RmlUiSingleton.h>
#include <RmlUiVideoPlugin/VideoElement.h>

// Factory keeps a non-owning pointer until Rml::Shutdown(), including document destruction.
static Rml::ElementInstancerGeneric<ezRmlUiVideoElement> s_VideoInstancer;
static Rml::ElementInstancer* s_pPreviousInstancer = nullptr;

// clang-format off
EZ_BEGIN_SUBSYSTEM_DECLARATION(RmlUiVideo, RmlUiVideoPlugin)
  BEGIN_SUBSYSTEM_DEPENDENCIES
    "RmlUiPlugin"
  END_SUBSYSTEM_DEPENDENCIES

  ON_HIGHLEVELSYSTEMS_STARTUP
  {
    if (ezRmlUi::GetSingleton() != nullptr)
    {
      s_pPreviousInstancer = Rml::Factory::GetElementInstancer("video");
      Rml::Factory::RegisterElementInstancer("video", &s_VideoInstancer);
    }
  }
  ON_HIGHLEVELSYSTEMS_SHUTDOWN
  {
    Rml::Factory::RegisterElementInstancer("video", s_pPreviousInstancer);
    s_pPreviousInstancer = nullptr;
  }
EZ_END_SUBSYSTEM_DECLARATION;
// clang-format on

EZ_STATICLINK_FILE(RmlUiVideoPlugin, RmlUiVideoPlugin_Startup);
