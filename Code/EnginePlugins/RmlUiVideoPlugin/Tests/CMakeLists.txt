ez_cmake_init()
ez_requires(EZ_BUILD_UNITTESTS)
ez_create_target(APPLICATION RmlUiVideoPluginTest NO_PCH NO_UNITY)
# Keep test folders from colliding with the runtime project names in Visual Studio.
set_property(TARGET RmlUiVideoPluginTest PROPERTY FOLDER "UnitTests/Plugins")
target_link_libraries(RmlUiVideoPluginTest PRIVATE TestFramework RmlUiVideoPlugin)
ez_ci_add_test(RmlUiVideoPluginTest)
target_compile_definitions(RmlUiVideoPluginTest PRIVATE RMLUI_VIDEO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Data")
