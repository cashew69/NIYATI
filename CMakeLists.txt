cmake_minimum_required(VERSION 3.10)
project(NIYATI)

# Generate compile_commands.json for editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    .
    engine
    engine/dependancies/imgui
)

set(IMGUI_SOURCES
    engine/dependancies/imgui/imgui.cpp
    engine/dependancies/imgui/imgui_draw.cpp
    engine/dependancies/imgui/imgui_tables.cpp
    engine/dependancies/imgui/imgui_widgets.cpp
    engine/dependancies/imgui/imgui_impl_glfw.cpp
    engine/dependancies/imgui/imgui_impl_opengl3.cpp
)

if(WIN32)
    add_executable(window_win32 WIN32 platforms/windows/winmain.cpp)

    target_include_directories(window_win32 PRIVATE
        .
        engine
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/dependancies/glew/include
        "C:/Program Files/Assimp/include"
    )
    target_link_directories(window_win32 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/dependancies/glew/lib/Release/x64
        "C:/Program Files/Assimp/lib/x64"
    )
    target_link_libraries(window_win32 opengl32 glew32 assimp-vc143-mt)

else()
    # GLFW editor window (with ImGui)
    add_executable(window_glfw_linux
        platforms/editor/glfwmain.cpp
        ${IMGUI_SOURCES}
    )
    target_compile_definitions(window_glfw_linux PRIVATE HAS_IMGUI)
    target_link_libraries(window_glfw_linux GL GLEW glfw assimp)

    # X11 window (no ImGui)
    add_executable(window_x11 platforms/linux/linmain.cpp)
    target_link_libraries(window_x11 GL X11 GLEW assimp)
endif()
