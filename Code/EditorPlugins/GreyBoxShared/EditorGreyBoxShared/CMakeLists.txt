ez_cmake_init()
ez_requires_editor()
ez_create_target(LIBRARY EditorGreyBoxShared)
ez_link_target_qt(TARGET EditorGreyBoxShared COMPONENTS Core Gui Widgets)
target_link_libraries(EditorGreyBoxShared PUBLIC EditorFramework)

