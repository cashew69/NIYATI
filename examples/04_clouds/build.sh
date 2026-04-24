#!/bin/bash
# Build Example 04: Volumetric Clouds
cd "$(dirname "$0")/../.."

# Switch active project in platform_common.cpp
sed -i 's|^#include "examples/.*|#include "examples/04_clouds/project.cpp"|' platform_common.cpp

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target window_glfw_linux
