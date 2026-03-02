#!/bin/bash
# Build Example 01: Simple Triangle
cd "$(dirname "$0")/../.."

# Switch active project in platform_common.cpp
sed -i 's|^#include "examples/.*|#include "examples/01_triangle/project.cpp"|' platform_common.cpp

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build build --target window_glfw_linux
