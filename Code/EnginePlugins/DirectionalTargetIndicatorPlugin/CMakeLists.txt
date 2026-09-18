ez_cmake_init()

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)
ez_create_target(LIBRARY ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PUBLIC RendererCore)
target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../Data/Base"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../Data/Plugins/DirectionalTargetIndicatorPlugin"
)
ez_copy_plugin_bundle(${PROJECT_NAME} "DirectionalTargetIndicator")
