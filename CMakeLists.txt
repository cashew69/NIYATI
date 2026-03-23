cmake_minimum_required(VERSION 3.10)
project(NIYATI)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NIYATI_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/core
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/core/gl
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/dependancies
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/dependancies/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/user
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
    add_executable(window_win32 platforms/windows/winmain.cpp ${IMGUI_SOURCES})

    target_include_directories(window_win32 PRIVATE
        ${NIYATI_INCLUDES}
        engine/dependancies/glew/include
        engine/dependancies/assimp/include
    )
    target_link_directories(window_win32 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/dependancies/glew/lib/Release/x64
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/dependancies/assimp/lib/x64
    )
    target_link_libraries(window_win32 opengl32 glew32 assimp-vc143-mt)

else()
    # Editor Build (GLFW + ImGui)
    add_executable(window_glfw_linux platforms/editor/glfwmain.cpp ${IMGUI_SOURCES})
    target_include_directories(window_glfw_linux PRIVATE ${NIYATI_INCLUDES})
    target_compile_definitions(window_glfw_linux PRIVATE HAS_IMGUI)
    target_link_libraries(window_glfw_linux GL GLEW glfw assimp)

    # Raw Linux Build (X11)
    add_executable(window_x11 platforms/linux/linmain.cpp)
    target_include_directories(window_x11 PRIVATE ${NIYATI_INCLUDES})
    target_link_libraries(window_x11 GL X11 GLEW assimp)
endif()
