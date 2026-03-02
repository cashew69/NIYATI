#!/bin/bash
# Build Example 03: Terrain + PBR + Skybox
cd "$(dirname "$0")/../.."

# Switch active project in platform_common.cpp
sed -i 's|^#include "examples/.*|#include "examples/03_terrain_pbr/project.cpp"|' platform_common.cpp

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build build --target window_glfw_linux
cmake --build build --target window_x11
