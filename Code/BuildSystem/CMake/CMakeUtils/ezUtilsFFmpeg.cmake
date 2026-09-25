# Use one pinned SDK for the Windows video plugins. Explicit SDK paths are kept.
function(ez_resolve_ffmpeg_sdk ROOT_VARIABLE)
  if(NOT "${${ROOT_VARIABLE}}" STREQUAL "" OR NOT WIN32)
    return()
  endif()

  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    message(FATAL_ERROR "Set ${ROOT_VARIABLE} to an FFmpeg SDK for this architecture.")
  endif()

  set(PACKAGE "ffmpeg-n7.1.1-57-g1b48158a23-win64-gpl-shared-7.1")
  set(SDK_DIR "${CMAKE_SOURCE_DIR}/Workspace/shared/ffmpeg")
  set(ARCHIVE "${SDK_DIR}/${PACKAGE}.zip")
  set(SDK_ROOT "${SDK_DIR}/${PACKAGE}")
  if(NOT EXISTS "${SDK_ROOT}/.ez-sdk-ready")
    file(MAKE_DIRECTORY "${SDK_DIR}")
    message(STATUS "Preparing FFmpeg SDK for video plugins (first run only)")
    file(DOWNLOAD
      "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-08-31-13-00/${PACKAGE}.zip"
      "${ARCHIVE}"
      EXPECTED_HASH SHA256=bb4ca47dd93aff34655be37b336f481f82332f05d9c4cebea1536b784868adde
      TLS_VERIFY ON SHOW_PROGRESS STATUS DOWNLOAD_STATUS)
    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_CODE)
    if(NOT DOWNLOAD_CODE EQUAL 0)
      message(FATAL_ERROR "FFmpeg download failed: ${DOWNLOAD_STATUS}. Set ${ROOT_VARIABLE} to a local SDK to build offline.")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${ARCHIVE}" DESTINATION "${SDK_DIR}")
    if(NOT EXISTS "${SDK_ROOT}/include/libavcodec/avcodec.h" OR NOT EXISTS "${SDK_ROOT}/lib/avcodec.lib")
      message(FATAL_ERROR "FFmpeg archive does not contain the expected development SDK.")
    endif()
    file(TOUCH "${SDK_ROOT}/.ez-sdk-ready")
  endif()
  set(${ROOT_VARIABLE} "${SDK_ROOT}" CACHE PATH "FFmpeg SDK with include, lib and bin directories" FORCE)
endfunction()
