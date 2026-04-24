#!/bin/bash
# Build Template Project
cd "$(dirname "$0")/../.."

# Switch active project in platform_common.cpp
sed -i 's|^#include ".*project.cpp"|#include "templates/project_template/project.cpp"|' platform_common.cpp

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target window_glfw_linux
