#!/bin/bash
# Build script for Linux X11 version

g++ window.cpp  -lGL -lX11 -lGLEW -lassimp -o window