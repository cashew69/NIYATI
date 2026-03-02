#!/bin/bash

# Function to print usage
usage() {
    echo "Usage: $0 [option]"
    echo "Options:"
    echo "  --glfw-lin   Build for Linux using GLFW"
    echo "  --glfw-win   Build for Windows using GLFW (Cross-compile)"
    echo "  --x11        Build for Linux using X11 (Native)"
    echo "  --win32      Build for Windows using Win32 API (Cross-compile)"
    echo "  --clean      Clean build artifacts"
    exit 1
}

# Check if arguments are provided
if [ $# -eq 0 ]; then
    usage
fi

# Common Include Paths
INC_ENGINE="-I engine"
INC_ROOT="-I ."
INC_IMGUI="-I engine/dependancies/imgui"

# ImGui Sources (for GLFW builds)
IMGUI_SOURCES="engine/dependancies/imgui/imgui.cpp \
               engine/dependancies/imgui/imgui_draw.cpp \
               engine/dependancies/imgui/imgui_tables.cpp \
               engine/dependancies/imgui/imgui_widgets.cpp \
               engine/dependancies/imgui/imgui_impl_glfw.cpp \
               engine/dependancies/imgui/imgui_impl_opengl3.cpp"

case "$1" in
    --glfw-lin)
        echo "Building for Linux (GLFW)..."
        g++ platforms/editor/glfwmain.cpp \
            $IMGUI_SOURCES \
            -o window_glfw_linux \
            $INC_IMGUI $INC_ROOT $INC_ENGINE \
            -lGL -lGLEW -lglfw -lassimp
        if [ $? -eq 0 ]; then
            echo "Build Successful: window_glfw_linux"
        else
            echo "Build Failed"
        fi
        ;;

    --glfw-win)
        echo "Building for Windows (GLFW)..."
        x86_64-w64-mingw32-g++ platforms/editor/glfwmain.cpp gui.cpp \
            $IMGUI_SOURCES \
            -o window_glfw.exe \
            $INC_IMGUI $INC_ROOT $INC_ENGINE \
            -lglew32 -lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32 -lassimp
        if [ $? -eq 0 ]; then
            echo "Build Successful: window_glfw.exe"
        else
            echo "Build Failed"
        fi
        ;;

    --x11)
        echo "Building for Linux (X11)..."
        g++ platforms/linux/linmain.cpp \
            -o window_x11 \
            $INC_ROOT $INC_ENGINE \
            -lGL -lX11 -lGLEW -lassimp
        if [ $? -eq 0 ]; then
            echo "Build Successful: window_x11"
        else
            echo "Build Failed"
        fi
        ;;

    --win32)
        echo "Building for Windows (Win32)..."
        x86_64-w64-mingw32-g++ platforms/windows/winmain.cpp \
            -o window_win32.exe \
            $INC_ROOT $INC_ENGINE \
            -lglew32 -lopengl32 -lgdi32 -luser32 -lkernel32 -lassimp
        if [ $? -eq 0 ]; then
            echo "Build Successful: window_win32.exe"
        else
            echo "Build Failed"
        fi
        ;;
    
    --clean)
        echo "Cleaning up..."
        rm -f window_glfw_linux window_x11 window_glfw.exe window_win32.exe window a.out
        echo "Cleaned."
        ;;

    *)
        usage
        ;;
esac
