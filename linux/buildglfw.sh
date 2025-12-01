#!/bin/bash
# Build script for GLFW version

g++ glfwmain.cpp \
    imgui/imgui.cpp \
    imgui/imgui_draw.cpp \
    imgui/imgui_tables.cpp \
    imgui/imgui_widgets.cpp \
    imgui/imgui_impl_glfw.cpp \
    imgui/imgui_impl_opengl3.cpp \
    -o window \
    -I./imgui \
    -lGL -lGLEW -lglfw -lassimp