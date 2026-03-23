#!/bin/bash
# Build Example 02: 3D Pyramid
cd "$(dirname "$0")/../.."

# Switch active project in platform_common.cpp
sed -i 's|^#include "examples/.*|#include "examples/02_pyramid/project.cpp"|' platform_common.cpp

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build build --target window_glfw_linux
