ez_cmake_init()

ez_requires_miniaudio()

# Get the name of this folder as the project name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

# Keep the effects tests with MiniAudio without compiling them into the runtime plugin.
ez_glob_source_files(${CMAKE_CURRENT_SOURCE_DIR} MINI_AUDIO_SOURCES)
list(FILTER MINI_AUDIO_SOURCES EXCLUDE REGEX "/Tests/")
ez_create_target(LIBRARY ${PROJECT_NAME} NO_UNITY MANUAL_SOURCE_FILES ${MINI_AUDIO_SOURCES})

target_link_libraries(${PROJECT_NAME}
  PUBLIC
  Core
  RendererCore

  PRIVATE
  MiniAudio
)

ez_copy_plugin_bundle(${PROJECT_NAME} "MiniAudio")

add_subdirectory(Tests)
