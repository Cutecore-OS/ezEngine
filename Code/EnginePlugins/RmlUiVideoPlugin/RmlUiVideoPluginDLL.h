#pragma once

#include <Foundation/Basics.h>
#include <RmlUiPlugin/RmlUiPluginDLL.h>

#if EZ_ENABLED(EZ_COMPILE_ENGINE_AS_DLL)
#  ifdef BUILDSYSTEM_BUILDING_RMLUIVIDEOPLUGIN_LIB
#    define EZ_RMLUIVIDEOPLUGIN_DLL EZ_DECL_EXPORT
#  else
#    define EZ_RMLUIVIDEOPLUGIN_DLL EZ_DECL_IMPORT
#  endif
#else
#  define EZ_RMLUIVIDEOPLUGIN_DLL
#endif
