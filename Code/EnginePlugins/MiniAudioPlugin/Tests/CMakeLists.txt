ez_cmake_init()
ez_requires(EZ_BUILD_UNITTESTS)
ez_requires_miniaudio()

ez_create_target(APPLICATION MiniAudioPluginTest NO_PCH NO_UNITY)
# Keep test folders from colliding with the runtime project names in Visual Studio.
set_property(TARGET MiniAudioPluginTest PROPERTY FOLDER "UnitTests/Plugins")
target_link_libraries(MiniAudioPluginTest PRIVATE TestFramework MiniAudioPlugin MiniAudio)
ez_ci_add_test(MiniAudioPluginTest)

target_compile_definitions(MiniAudioPluginTest PRIVATE MINI_AUDIO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Data")
