#!/usr/bin/env python3
"""
Run after cmake to extend build/compile_commands.json with entries for
.cpp files that are #included (unity-build style) so clangd can analyse
them with the right flags.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.abspath(__file__))


def find_unity_includes(path, visited=None):
    """Return absolute paths of all .cpp files #included from path, recursively."""
    if visited is None:
        visited = set()
    if path in visited or not os.path.exists(path):
        return set()
    visited.add(path)

    try:
        text = open(path, errors="ignore").read()
    except OSError:
        return set()

    result = set()
    src_dir = os.path.dirname(path)
    for m in re.finditer(r'#\s*include\s+"([^"]+\.cpp)"', text):
        inc = m.group(1)
        for base in (src_dir, ROOT):
            candidate = os.path.normpath(os.path.join(base, inc))
            if os.path.exists(candidate):
                result.add(candidate)
                result |= find_unity_includes(candidate, visited)
                break
    return result


def strip_output_flag(cmd):
    return re.sub(r"\s+-o\s+\S+", "", cmd)


def main():
    build_db = os.path.join(ROOT, "build", "compile_commands.json")
    out_db   = build_db  # write back into build/ where clangd looks

    if not os.path.exists(build_db):
        sys.exit(f"Not found: {build_db}\nRun cmake first.")

    entries = json.load(open(build_db))
    known   = {e["file"] for e in entries}

    # glfwmain.cpp is the richest translation unit (HAS_IMGUI + all includes)
    template = next((e for e in entries if "glfwmain.cpp" in e["file"]), entries[0])
    base_cmd = strip_output_flag(template["command"])

    # Collect unity-included .cpp files reachable from source files and headers
    unity: set = set()
    for e in entries:
        unity |= find_unity_includes(e["file"])
    for dirpath, dirs, files in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d != "build"]
        for f in files:
            if f.endswith((".h", ".hpp")):
                unity |= find_unity_includes(os.path.join(dirpath, f))

    added = 0
    for path in sorted(unity):
        if path not in known:
            cmd = re.sub(r"-c\s+\S+", f"-c {path}", base_cmd)
            entries.append({"directory": template["directory"], "command": cmd, "file": path})
            known.add(path)
            added += 1
            print(f"  + {os.path.relpath(path, ROOT)}")

    json.dump(entries, open(out_db, "w"), indent=2)
    print(f"\nWrote {len(entries)} entries ({added} unity files) → compile_commands.json")


if __name__ == "__main__":
    main()
