ez_cmake_init()
ez_requires_rmlui()

option(EZ_BUILD_RMLUI_VIDEO "Build the RmlUi video plugin (requires FFmpeg development libraries)" OFF)
ez_requires(EZ_BUILD_RMLUI_VIDEO)
set(EZ_RMLUI_VIDEO_FFMPEG_ROOT "" CACHE PATH "FFmpeg SDK root containing include, lib and bin")

find_path(RMLUI_VIDEO_FFMPEG_INCLUDE_DIR libavformat/avformat.h
  HINTS "${EZ_RMLUI_VIDEO_FFMPEG_ROOT}/include" REQUIRED)
foreach(COMPONENT avformat avcodec avutil swscale)
  find_library(RMLUI_VIDEO_${COMPONENT}_LIBRARY NAMES ${COMPONENT}
    HINTS "${EZ_RMLUI_VIDEO_FFMPEG_ROOT}/lib" REQUIRED)
endforeach()

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)
ez_glob_source_files(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)
list(FILTER SOURCES EXCLUDE REGEX "/Tests/")
ez_create_target(LIBRARY ${PROJECT_NAME} NO_UNITY MANUAL_SOURCE_FILES ${SOURCES})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${RMLUI_VIDEO_FFMPEG_INCLUDE_DIR}")
target_link_libraries(${PROJECT_NAME} PUBLIC RmlUiPlugin PRIVATE
  ${RMLUI_VIDEO_avformat_LIBRARY} ${RMLUI_VIDEO_avcodec_LIBRARY}
  ${RMLUI_VIDEO_avutil_LIBRARY} ${RMLUI_VIDEO_swscale_LIBRARY})

# Copy the supplied SDK runtime next to the plugin. System packages use the platform loader.
if(WIN32 AND EZ_RMLUI_VIDEO_FFMPEG_ROOT)
  file(GLOB FFMPEG_DLLS "${EZ_RMLUI_VIDEO_FFMPEG_ROOT}/bin/*.dll")
  foreach(DLL ${FFMPEG_DLLS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
  endforeach()
endif()
ez_copy_plugin_bundle(${PROJECT_NAME} "RmlUiVideo")
add_subdirectory(Tests)
