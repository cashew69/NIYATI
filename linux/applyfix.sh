#!/bin/bash

# ============================================================
# Transform System Installation Script
# ============================================================
# This script will update your project files to use the
# Transform system and fix all compilation errors.
#
# Run from the outputs/linux directory:
#   bash apply_fixes.sh /path/to/your/project/linux
# ============================================================

# Check if target directory is provided
if [ -z "$1" ]; then
    echo "Usage: bash apply_fixes.sh /run/media/cashew/NewVolume/Win-LinSync/MyProjects/Fun/NIYATI/linux/"
    echo ""
    echo "Example:"
    echo "  bash apply_fixes.sh /run/media/cashew/NewVolume/Win-LinSync/MyProjects/Fun/NIYATI/linux"
    exit 1
fi

TARGET_DIR="$1"

# Check if target directory exists
if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' does not exist!"
    exit 1
fi

echo "============================================================"
echo "Transform System Installation"
echo "============================================================"
echo "Target directory: $TARGET_DIR"
echo ""

# Backup original files
echo "[1/5] Creating backups..."
BACKUP_DIR="$TARGET_DIR/backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR/core/gl"

if [ -f "$TARGET_DIR/core/gl/structs.h" ]; then
    cp "$TARGET_DIR/core/gl/structs.h" "$BACKUP_DIR/core/gl/"
    echo "  ✓ Backed up structs.h"
fi

if [ -f "$TARGET_DIR/core/gl/modelloading.cpp" ]; then
    cp "$TARGET_DIR/core/gl/modelloading.cpp" "$BACKUP_DIR/core/gl/"
    echo "  ✓ Backed up modelloading.cpp"
fi

if [ -f "$TARGET_DIR/core/gl/renderer.cpp" ]; then
    cp "$TARGET_DIR/core/gl/renderer.cpp" "$BACKUP_DIR/core/gl/"
    echo "  ✓ Backed up renderer.cpp"
fi

if [ -f "$TARGET_DIR/common.h" ]; then
    cp "$TARGET_DIR/common.h" "$BACKUP_DIR/"
    echo "  ✓ Backed up common.h"
fi

echo "  Backups saved to: $BACKUP_DIR"
echo ""

# Copy engine directory
echo "[2/5] Installing Transform system..."
if [ ! -d "$TARGET_DIR/engine" ]; then
    mkdir -p "$TARGET_DIR/engine"
fi

cp -r engine/* "$TARGET_DIR/engine/"
echo "  ✓ Copied engine/transform.h"
echo "  ✓ Copied engine/transform.cpp"
echo "  ✓ Copied engine documentation"
echo ""

# Update structs.h
echo "[3/5] Updating structs.h..."
mkdir -p "$TARGET_DIR/core/gl"
cp core/gl/structs.h "$TARGET_DIR/core/gl/structs.h"
echo "  ✓ Updated core/gl/structs.h (Mesh now uses Transform*)"
echo ""

# Update modelloading.cpp
echo "[4/5] Updating modelloading.cpp..."
cp core/gl/modelloading.cpp "$TARGET_DIR/core/gl/modelloading.cpp"
echo "  ✓ Updated core/gl/modelloading.cpp"
echo ""

# Update renderer.cpp
echo "[5/5] Updating renderer.cpp..."
cp core/gl/renderer.cpp "$TARGET_DIR/core/gl/renderer.cpp"
echo "  ✓ Updated core/gl/renderer.cpp"
echo ""

# Check if common.h needs updating
echo "============================================================"
echo "IMPORTANT: Update common.h manually"
echo "============================================================"
echo ""
echo "You need to add these lines to your common.h:"
echo ""
echo "1. After '#include \"vmath.h\"' and 'using namespace vmath;', add:"
echo "   #include \"engine/transform.h\""
echo ""
echo "2. After '#include \"core/gl/structs.h\"', add:"
echo "   #include \"engine/transform.cpp\""
echo ""
echo "Your common.h should look like:"
echo ""
echo "  #include \"vmath.h\""
echo "  using namespace vmath;"
echo "  // ... OpenGL includes ..."
echo "  #include \"engine/transform.h\"         // ← ADD THIS"
echo "  #include \"core/gl/structs.h\""
echo "  // ... other includes ..."
echo "  #include \"engine/transform.cpp\"       // ← ADD THIS"
echo "  #include \"core/gl/shaders.cpp\""
echo ""
echo "A reference common.h is provided in the outputs folder."
echo ""
echo "============================================================"
echo "Installation Complete!"
echo "============================================================"
echo ""
echo "Next steps:"
echo "1. Update your common.h as shown above"
echo "2. Run: cd $TARGET_DIR && sh build.sh"
echo ""
echo "If you have issues, restore from: $BACKUP_DIR"
echo ""
