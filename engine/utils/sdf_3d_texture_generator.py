"""
SDF 3D Texture Generator for Volumetric Clouds
===============================================
Blender Addon — generates a Signed Distance Field (SDF) 3D texture from any
mesh and exports it for use in OpenGL / GLSL volumetric cloud shaders.

Install:
  Edit → Preferences → Add-ons → Install → select this .py file → Enable

Usage:
  3D Viewport → Sidebar (N) → SDF Tools tab

Export formats
--------------
  .raw   – raw float32 binary (load directly as GL_TEXTURE_3D)
  .f3d   – custom binary with header (resolution + world bounds embedded)
  PNG    – one grayscale PNG per Z-slice  (16-bit, for debugging / DCC tools)
  .npy   – NumPy array dump (for further Python processing)

A companion GLSL snippet + JSON metadata file is always written alongside the
chosen format so you can paste the shader code straight into your renderer.

GLSL usage in your shader
--------------------------
  uniform sampler3D u_sdf;
  uniform vec3      u_sdfBoundsMin;
  uniform vec3      u_sdfBoundsMax;

  float sampleSDF(vec3 worldPos) {
      vec3 uvw = (worldPos - u_sdfBoundsMin) /
                 (u_sdfBoundsMax - u_sdfBoundsMin);
      return texture(u_sdf, clamp(uvw, 0.0, 1.0)).r;
  }

  // negative  → inside the cloud shape
  // positive  → outside
  float density = max(0.0, -sampleSDF(pos));
"""

# ──────────────────────────────────────────────────────────────────────────────
#  Addon metadata
# ──────────────────────────────────────────────────────────────────────────────
bl_info = {
    "name":        "SDF 3D Texture Generator – Volumetric Clouds",
    "author":      "Claude / Anthropic",
    "version":     (1, 3, 0),
    "blender":     (3, 0, 0),
    "location":    "View3D › Sidebar › SDF Tools",
    "description": "Bake a Signed Distance Field 3D texture from any mesh "
                   "and export it for OpenGL / GLSL volumetric rendering.",
    "warning":     "Large resolutions (≥ 128³) can take several minutes.",
    "doc_url":     "",
    "category":    "Render",
}

# ──────────────────────────────────────────────────────────────────────────────
#  Imports
# ──────────────────────────────────────────────────────────────────────────────
import bpy
import bmesh
import os
import json
import struct
import math
import time
import textwrap

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

from bpy.props import (
    BoolProperty, EnumProperty, FloatProperty,
    IntProperty, PointerProperty, StringProperty,
)
from bpy.types import Operator, Panel, PropertyGroup


# ──────────────────────────────────────────────────────────────────────────────
#  Constants
# ──────────────────────────────────────────────────────────────────────────────
F3D_MAGIC   = b"F3DX"       # magic bytes for .f3d header
F3D_VERSION = 1


# ══════════════════════════════════════════════════════════════════════════════
#  SDF COMPUTATION  (pure Python + numpy, no external dependencies)
# ══════════════════════════════════════════════════════════════════════════════

def _build_bvh(obj: bpy.types.Object) -> tuple:
    """
    Return (BVHTree, evaluated_mesh) for *obj* in world space.
    Applies all modifiers and the object matrix.
    """
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj  = obj.evaluated_get(depsgraph)
    me        = eval_obj.to_mesh()

    # Transform vertices to world space
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.transform(bm, matrix=obj.matrix_world, verts=bm.verts)

    tree = BVHTree.FromBMesh(bm)
    bm.free()
    eval_obj.to_mesh_clear()
    return tree


def _sign_from_ray(tree: BVHTree, point: Vector) -> float:
    """
    Determine inside/outside sign for *point* by casting a ray along +X.
    Returns -1.0 (inside) or +1.0 (outside).
    """
    direction = Vector((1.0, 0.0, 0.0))
    hits = 0
    origin = point.copy()
    epsilon = 1e-5

    # Walk ray, counting surface crossings
    for _ in range(64):                          # cap at 64 crossings
        loc, _normal, _idx, dist = tree.ray_cast(origin, direction)
        if loc is None:
            break
        hits += 1
        origin = loc + direction * epsilon        # step past hit surface

    return -1.0 if (hits % 2 == 1) else 1.0


def compute_sdf_grid(
    obj:        bpy.types.Object,
    resolution: tuple,          # (rx, ry, rz)
    padding:    float = 0.05,   # fraction of bbox diagonal
    use_sign:   bool  = True,
    operator    = None,         # bpy.types.Operator for progress report
) -> tuple:
    """
    Core SDF baker.

    Returns
    -------
    sdf_grid : np.ndarray shape (rz, ry, rx) dtype float32
        Negative values → inside mesh, positive → outside.
    world_min : Vector
    world_max : Vector
    """
    rx, ry, rz = resolution

    # ── world-space bounding box ──────────────────────────────────────────────
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    w_min = Vector((min(c.x for c in corners),
                    min(c.y for c in corners),
                    min(c.z for c in corners)))
    w_max = Vector((max(c.x for c in corners),
                    max(c.y for c in corners),
                    max(c.z for c in corners)))

    diag = (w_max - w_min).length
    pad  = padding * diag

    w_min -= Vector((pad, pad, pad))
    w_max += Vector((pad, pad, pad))

    span = w_max - w_min

    if operator:
        operator.report({'INFO'}, f"SDF bounds: {w_min} → {w_max}")

    # ── build BVH ─────────────────────────────────────────────────────────────
    tree = _build_bvh(obj)

    # ── grid coordinates ──────────────────────────────────────────────────────
    # Centres of voxels
    xs = np.linspace(w_min.x + span.x / (2 * rx),
                     w_max.x - span.x / (2 * rx), rx, dtype=np.float32)
    ys = np.linspace(w_min.y + span.y / (2 * ry),
                     w_max.y - span.y / (2 * ry), ry, dtype=np.float32)
    zs = np.linspace(w_min.z + span.z / (2 * rz),
                     w_max.z - span.z / (2 * rz), rz, dtype=np.float32)

    sdf = np.zeros((rz, ry, rx), dtype=np.float32)

    t0      = time.time()
    total   = rz * ry * rx
    done    = 0
    last_pct = -1

    for iz, z in enumerate(zs):
        for iy, y in enumerate(ys):
            for ix, x in enumerate(xs):
                pt = Vector((x, y, z))

                # Nearest point on surface
                loc, normal, _face_idx, _dist = tree.find_nearest(pt)

                if loc is None:
                    # Should not happen for a closed mesh inside the bbox
                    sdf[iz, iy, ix] = 0.0
                else:
                    dist_val = (pt - loc).length

                    if use_sign:
                        # Sign from surface normal direction (fast, works for
                        # manifold/closed meshes).  Falls back to ray-cast for
                        # ambiguous cases (dist ≈ 0 or open meshes).
                        if dist_val < 1e-6:
                            sign = -1.0
                        else:
                            to_surface = loc - pt
                            dot = to_surface.dot(normal)
                            if abs(dot) < 1e-4 * dist_val:
                                sign = _sign_from_ray(tree, pt)
                            else:
                                sign = -1.0 if dot > 0 else 1.0

                        sdf[iz, iy, ix] = sign * dist_val
                    else:
                        sdf[iz, iy, ix] = dist_val

                done += 1

            # ── progress report every 1 % ────────────────────────────────────
            pct = int(done * 100 / total)
            if pct != last_pct:
                last_pct = pct
                elapsed  = time.time() - t0
                eta      = (elapsed / max(done, 1)) * (total - done)
                msg = f"SDF bake: {pct}%  (ETA {eta:.0f}s)"
                if operator:
                    operator.report({'INFO'}, msg)

    elapsed = time.time() - t0
    if operator:
        operator.report({'INFO'}, f"SDF bake complete in {elapsed:.1f}s")

    return sdf, w_min, w_max


# ══════════════════════════════════════════════════════════════════════════════
#  EXPORT HELPERS
# ══════════════════════════════════════════════════════════════════════════════

def _normalize(sdf: np.ndarray) -> np.ndarray:
    """Map SDF values to [0, 1]:  0.5 = surface, <0.5 = inside, >0.5 = outside."""
    mn, mx = float(sdf.min()), float(sdf.max())
    span = mx - mn if mx != mn else 1.0
    return ((sdf - mn) / span).astype(np.float32)


def export_raw(sdf: np.ndarray, path: str, normalize: bool) -> str:
    """Export as raw binary float32 (X-major, i.e. innermost axis = X)."""
    data = _normalize(sdf) if normalize else sdf
    # Layout for OpenGL: Z-slices, each slice = rows of Y, each row = X values
    # numpy default C-order: [z, y, x] → already correct
    data.tofile(path)
    return path


def export_f3d(
    sdf: np.ndarray, path: str,
    w_min: Vector, w_max: Vector,
    normalize: bool,
) -> str:
    """
    Custom .f3d binary format.

    Header (64 bytes):
        4B  magic  "F3DX"
        4B  version (uint32 LE)
        4B  res_x  (uint32 LE)
        4B  res_y
        4B  res_z
        4B  channel_count  (1 = float32 R)
        4B  data_type      (0 = float32)
        4B  flags          (bit0 = normalised)
       24B  world AABB     (6× float32 LE: minX minY minZ maxX maxY maxZ)
        8B  padding
    Data: res_x * res_y * res_z * float32 (C-order: [z,y,x])
    """
    data   = _normalize(sdf) if normalize else sdf
    rz, ry, rx = data.shape
    flags  = 1 if normalize else 0

    with open(path, "wb") as f:
        # Header
        f.write(F3D_MAGIC)
        f.write(struct.pack("<I", F3D_VERSION))
        f.write(struct.pack("<III", rx, ry, rz))
        f.write(struct.pack("<II", 1, 0))           # channels, dtype
        f.write(struct.pack("<I", flags))
        f.write(struct.pack("<6f",
                            w_min.x, w_min.y, w_min.z,
                            w_max.x, w_max.y, w_max.z))
        f.write(b"\x00" * 8)                        # padding → 64 B total
        # Data
        data.tofile(f)

    return path


def export_png_slices(
    sdf: np.ndarray, out_dir: str, base_name: str, normalize: bool
) -> list:
    """
    Export each Z-slice as a 16-bit grayscale PNG.
    Requires Blender's internal image API.
    """
    data   = _normalize(sdf) if normalize else np.clip((sdf + 1) / 2, 0, 1)
    rz, ry, rx = data.shape

    paths = []
    for iz in range(rz):
        slice_name = f"{base_name}_slice_{iz:04d}"
        img = bpy.data.images.new(slice_name, width=rx, height=ry,
                                  float_buffer=True, is_data=True)
        img.colorspace_settings.name = "Non-Color"
        img.file_format = "PNG"
        img.use_generated_float = True

        # Blender image pixels are RGBA, bottom-to-top
        slice_data = data[iz]             # shape (ry, rx)
        # Flip Y for Blender convention
        flipped = slice_data[::-1, :]     # shape (ry, rx)

        # Build RGBA array
        pixels = np.zeros((ry * rx * 4,), dtype=np.float32)
        flat   = flipped.flatten()
        pixels[0::4] = flat              # R
        pixels[1::4] = flat              # G
        pixels[2::4] = flat              # B
        pixels[3::4] = 1.0               # A

        img.pixels.foreach_set(pixels.tolist())

        out_path = os.path.join(out_dir, f"{slice_name}.png")
        img.filepath_raw = out_path
        img.save()
        bpy.data.images.remove(img)
        paths.append(out_path)

    return paths


def export_npy(sdf: np.ndarray, path: str, normalize: bool) -> str:
    """Export as NumPy .npy file."""
    data = _normalize(sdf) if normalize else sdf
    np.save(path, data)
    return path


def export_nvdf(
    sdf:           np.ndarray,   # float32 [z, y, x] — negative inside
    path:          str,
    w_min:         Vector,
    w_max:         Vector,
    feather_voxels: float = 2.0,  # edge softness in voxel units
    density_scale:  float = 1.0,  # multiplier before uint8 clamp
) -> str:
    """
    Export as engine-native .nvdf (Nubis Voxel Density Field).

    Binary layout (matches nvdf_generator.cpp / vcloud_texture.cpp exactly):
    ┌──────────────────────────────────────────────────────────────┐
    │  Offset  Size  Field                                         │
    │       0     4  magic  = 'N','V','D','F'                      │
    │       4     4  version  (uint32 LE) = 1                      │
    │       8     4  width    (uint32 LE) = X (innermost in voxels)│
    │      12     4  height   (uint32 LE) = Y  ← cloud height axis │
    │      16     4  depth    (uint32 LE) = Z                      │
    │      20     4  format   (uint32 LE) = 0  (R8 uint8)          │
    │      24     W×H×D  voxels uint8, row-major X (X fastest)     │
    └──────────────────────────────────────────────────────────────┘

    Axis remapping (Blender → engine):
        Blender X → texture X (width,  S coord) — horizontal
        Blender Z → texture Y (height, T coord) — cloud height axis  ← KEY
        Blender Y → texture Z (depth,  R coord) — horizontal

    The engine's cloud_compute.comp.glsl samples the NVDF as
        nvdfUVW = vec3(uvxz.x, effH, uvxz.y)
    where T (=Y) carries the normalised cloud height (effH).  Exporting with
    Blender Z mapped to texture Y ensures a Z-up Blender mesh aligns with
    the engine's vertical axis without re-orienting the mesh.

    Engine GL wrap modes (set by vcVCloud_LoadNVDF / vcCloud_LoadNVDF):
        WRAP_S (X) = GL_REPEAT          ← horizontal tiling
        WRAP_T (Y) = GL_CLAMP_TO_EDGE   ← height axis (effH already clamped)
        WRAP_R (Z) = GL_REPEAT          ← horizontal tiling

    SDF → density conversion (mirrors the engine's density sampling):
        feather  = feather_voxels * voxel_world_size
        t        = clamp((sdf + feather) / (2 * feather), 0, 1)
        density  = 1.0 − smoothstep(t)    → 1 inside, 0 outside
        stored   = uint8(density × densityScale × 255)
    """
    rz, ry, rx = sdf.shape

    # Blender Z (height) becomes texture Y; Blender Y becomes texture Z.
    # Transpose from (rz, ry, rx) to (ry, rz, rx) so GL sees:
    #   depth = ry (Blender Y, horizontal), height = rz (Blender Z, vertical)
    sdf_reoriented = np.ascontiguousarray(np.transpose(sdf, (1, 0, 2)))
    # texture dimensions after reorientation
    tex_w, tex_h, tex_d = rx, rz, ry   # width=rx, height=rz(Blender Z), depth=ry

    # Feather in world units — use the height span (Blender Z / texture Y).
    vox_size_h = (w_max.z - w_min.z) / rz if rz > 0 else 1e-6
    feather    = max(feather_voxels * vox_size_h, 1e-6)

    # Smoothstep SDF → density:
    #   sdf ≤ -feather  →  t=0  →  density=1.0  (deep inside)
    #   sdf =  0        →  t=0.5 → density=0.5  (on surface)
    #   sdf ≥ +feather  →  t=1  →  density=0.0  (outside)
    t         = np.clip((sdf_reoriented + feather) / (2.0 * feather), 0.0, 1.0)
    smooth    = t * t * (3.0 - 2.0 * t)
    density   = (1.0 - smooth) * density_scale

    # Quantize to R8 uint8
    voxels = (density * 255.0 + 0.5).clip(0, 255).astype(np.uint8)

    with open(path, "wb") as f:
        f.write(b"NVDF")
        # version=1, width=tex_w, height=tex_h(Blender Z), depth=tex_d, format=0
        f.write(struct.pack("<IIIII", 1, tex_w, tex_h, tex_d, 0))
        f.write(voxels.tobytes())

    return path


def write_metadata(
    path: str,
    obj_name: str,
    resolution: tuple,
    w_min: Vector,
    w_max: Vector,
    padding: float,
    normalize: bool,
    export_fmt: str,
    nvdf_feather_voxels: float = 2.0,
    nvdf_density_scale:  float = 1.0,
) -> str:
    """Write a companion JSON metadata file."""
    rx, ry, rz = resolution

    meta = {
        "source_object":  obj_name,
        "resolution":     {"x": rx, "y": ry, "z": rz},
        "world_bounds": {
            "min": [w_min.x, w_min.y, w_min.z],
            "max": [w_max.x, w_max.y, w_max.z],
        },
        "padding_fraction":  padding,
        "normalized":        normalize,
        "export_format":     export_fmt,
    }

    if export_fmt == "nvdf":
        vox_size = (w_max.x - w_min.x) / rx
        meta.update({
            "nvdf_format_id":        0,
            "nvdf_format_name":      "R8 uint8 density",
            "nvdf_feather_voxels":   nvdf_feather_voxels,
            "nvdf_feather_world":    nvdf_feather_voxels * vox_size,
            "nvdf_density_scale":    nvdf_density_scale,
            "nvdf_encoding":         "0=empty, 255=full density",
            "nvdf_gl_internal_fmt":  "GL_R8",
            "nvdf_gl_pixel_fmt":     "GL_RED",
            "nvdf_gl_pixel_type":    "GL_UNSIGNED_BYTE",
            "nvdf_wrap_s":           "GL_REPEAT         (X — horizontal tile)",
            "nvdf_wrap_t":           "GL_CLAMP_TO_EDGE  (Y — cloud height, effH clamped)",
            "nvdf_wrap_r":           "GL_REPEAT         (Z — horizontal tile)",
            "nvdf_axis_note":        "Blender Z (up) → texture Y (T/height). Blender Y → texture Z (R/depth).",
            "engine_loader":         "vcVCloud_LoadNVDF(path)  [format=0 branch]",
            "engine_gl_upload":
                f"glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // required for R8\n"
                f"glTexStorage3D(GL_TEXTURE_3D, 1, GL_R8, {rx}, {rz}, {ry});\n"
                f"glTexSubImage3D(GL_TEXTURE_3D, 0, 0,0,0, {rx},{rz},{ry}, "
                f"GL_RED, GL_UNSIGNED_BYTE, data);\n"
                f"glPixelStorei(GL_UNPACK_ALIGNMENT, 4);",
        })
    else:
        meta.update({
            "channel":          "R float32  (negative=inside, positive=outside)",
            "opengl_format":    "GL_R32F / GL_TEXTURE_3D",
            "opengl_load_hint": (
                f"glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, "
                f"{rx}, {ry}, {rz}, 0, GL_RED, GL_FLOAT, data);"
            ),
        })

    with open(path, "w") as f:
        json.dump(meta, f, indent=2)
    return path


def write_glsl_snippet(path: str, w_min: Vector, w_max: Vector) -> str:
    """Write a ready-to-paste GLSL snippet."""

    snippet = textwrap.dedent(f"""\
    // ──────────────────────────────────────────────────────────────────────────
    //  SDF 3D Texture — GLSL usage snippet
    //  Generated by the Blender SDF 3D Texture Generator addon
    // ──────────────────────────────────────────────────────────────────────────

    // --- Uniforms (set from your application) ---------------------------------
    uniform sampler3D u_sdf;
    // Tip: bind with GL_LINEAR for smooth density; GL_NEAREST for crisp edges.

    // World-space AABB baked into this SDF
    const vec3 SDF_BOUNDS_MIN = vec3({w_min.x:.6f}, {w_min.y:.6f}, {w_min.z:.6f});
    const vec3 SDF_BOUNDS_MAX = vec3({w_max.x:.6f}, {w_max.y:.6f}, {w_max.z:.6f});

    // --- Helper: sample SDF at a world-space position -------------------------
    float sampleSDF(vec3 worldPos) {{
        vec3 uvw = (worldPos - SDF_BOUNDS_MIN)
                 / (SDF_BOUNDS_MAX - SDF_BOUNDS_MIN);
        // Optional: return a large positive distance outside the bbox
        if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0))))
            return 1.0;
        return texture(u_sdf, uvw).r;
    }}

    // --- Basic cloud density from SDF ----------------------------------------
    //  sdf < 0 → inside shape → density > 0
    //  sdf > 0 → outside      → density = 0
    float cloudDensity(vec3 worldPos) {{
        float sdf = sampleSDF(worldPos);
        return max(0.0, -sdf);
    }}

    // --- Soft density with a feathered edge ----------------------------------
    //  feather: distance over which the edge fades (world units)
    float cloudDensitySoft(vec3 worldPos, float feather) {{
        float sdf = sampleSDF(worldPos);
        return 1.0 - smoothstep(-feather, feather, sdf);
    }}

    // --- Approximate surface normal from SDF gradient (central differences) --
    vec3 sdfGradient(vec3 worldPos) {{
        float span = (SDF_BOUNDS_MAX.x - SDF_BOUNDS_MIN.x);
        float e    = span * 0.004;                // small offset (~0.4 % of box)
        float dx = sampleSDF(worldPos + vec3(e, 0, 0))
                 - sampleSDF(worldPos - vec3(e, 0, 0));
        float dy = sampleSDF(worldPos + vec3(0, e, 0))
                 - sampleSDF(worldPos - vec3(0, e, 0));
        float dz = sampleSDF(worldPos + vec3(0, 0, e))
                 - sampleSDF(worldPos - vec3(0, 0, e));
        return normalize(vec3(dx, dy, dz));
    }}

    // --- Ray-march skeleton for volumetric cloud (simplified) ----------------
    //
    //  Call this from your fragment or compute shader.
    //
    //  rayOrigin, rayDir   : camera ray in world space
    //  tMin, tMax          : ray enter/exit distance
    //  numSteps            : march quality (64–256)
    //  absorption          : controls opacity build-up
    //  Returns: vec4(RGB colour, transmittance)  — composite over background
    //
    vec4 raymarchCloud(
        vec3  rayOrigin,
        vec3  rayDir,
        float tMin,
        float tMax,
        int   numSteps,
        float absorption
    ) {{
        float stepSize    = (tMax - tMin) / float(numSteps);
        float transmit    = 1.0;
        vec3  accumulated = vec3(0.0);

        for (int i = 0; i < numSteps; ++i) {{
            float t   = tMin + (float(i) + 0.5) * stepSize;
            vec3  pos = rayOrigin + t * rayDir;

            float density = cloudDensitySoft(pos, 0.05);
            if (density < 0.001) continue;

            // Simple single-scattering colour (replace with your lighting)
            vec3 cloudColour = vec3(1.0, 0.98, 0.96);

            float alpha   = 1.0 - exp(-absorption * density * stepSize);
            accumulated  += transmit * alpha * cloudColour;
            transmit     *= 1.0 - alpha;

            if (transmit < 0.01) break;   // early out — fully opaque
        }}

        return vec4(accumulated, 1.0 - transmit);
    }}

    // --- Application-side OpenGL upload (C++ pseudo-code) --------------------
    //
    //  FILE *f = fopen("output.raw", "rb");
    //  fread(data, sizeof(float), RX * RY * RZ, f);
    //
    //  GLuint tex;
    //  glGenTextures(1, &tex);
    //  glBindTexture(GL_TEXTURE_3D, tex);
    //  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    //  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
    //               RX, RY, RZ, 0, GL_RED, GL_FLOAT, data);
    //
    // ──────────────────────────────────────────────────────────────────────────
    """)

    with open(path, "w") as f:
        f.write(snippet)
    return path


# ══════════════════════════════════════════════════════════════════════════════
#  PROPERTIES
# ══════════════════════════════════════════════════════════════════════════════

class SDF_Properties(PropertyGroup):

    source_object: PointerProperty(
        name        = "Source Mesh",
        description = "Mesh object to bake the SDF from",
        type        = bpy.types.Object,
        poll        = lambda self, obj: obj.type == 'MESH',
    )

    resolution_x: IntProperty(
        name    = "Resolution X",
        default = 64, min = 4, max = 512,
        description = "Voxel grid resolution along X",
    )
    resolution_y: IntProperty(
        name    = "Resolution Y",
        default = 64, min = 4, max = 512,
    )
    resolution_z: IntProperty(
        name    = "Resolution Z",
        default = 64, min = 4, max = 512,
    )

    resolution_preset: EnumProperty(
        name  = "Preset",
        items = [
            ("custom",   "Custom",   ""),
            ("32",       "32³",      ""),
            ("64",       "64³",      "Fast preview"),
            ("128",      "128³",     "Good quality  (~2 min)"),
            ("256",      "256³",     "High quality  (~15 min)"),
        ],
        default = "64",
        update  = lambda self, ctx: _apply_preset(self),
    )

    padding: FloatProperty(
        name        = "Padding",
        description = "Extra space around mesh as fraction of bounding-box diagonal",
        default = 0.05, min = 0.0, max = 0.5, subtype = 'FACTOR',
    )

    use_sign: BoolProperty(
        name        = "Signed SDF",
        description = "Compute true signed distance (negative inside, positive outside). "
                      "Disable for unsigned distance only.",
        default = True,
    )

    normalize: BoolProperty(
        name        = "Normalise to [0, 1]",
        description = "Remap values to [0, 1] — required for 8/16-bit PNG slices, "
                      "optional for float formats",
        default = False,
    )

    export_format: EnumProperty(
        name  = "Export Format",
        items = [
            ("nvdf", "NVDF (engine native ★)",
                     "Engine-native R8 density field — direct drop-in for nvdf_generator / "
                     "vcVCloud_LoadBC6HNVDF.  Converts SDF → uint8 density automatically."),
            ("raw",  "RAW float32",    "Raw binary float32 — load directly as GL_TEXTURE_3D"),
            ("f3d",  "F3D (with header)",
                     "Custom binary with AABB header embedded"),
            ("png",  "PNG slices",     "16-bit grayscale, one PNG per Z-slice"),
            ("npy",  "NumPy (.npy)",   "NumPy array — for Python post-processing"),
        ],
        default = "nvdf",
    )

    nvdf_feather_voxels: FloatProperty(
        name        = "Edge Feather (voxels)",
        description = "Soft transition width at cloud surface, in voxel units. "
                      "Mirrors erosion/smoothing done by the Nubis density function. "
                      "0 = hard binary edge,  2–4 = typical soft cloud edge.",
        default = 2.0, min = 0.0, max = 16.0,
    )

    nvdf_density_scale: FloatProperty(
        name        = "Density Scale",
        description = "Multiplier applied before uint8 quantisation. "
                      "Matches u_densityScale in nvdf_generate.comp.glsl. "
                      "1.0 = surface maps to 127, core to 255.",
        default = 1.0, min = 0.1, max = 4.0,
    )

    output_dir: StringProperty(
        name        = "Output Directory",
        description = "Folder to write the SDF texture and companion files",
        default     = "//sdf_output",
        subtype     = "DIR_PATH",
    )

    output_name: StringProperty(
        name        = "File Base Name",
        description = "Base file name (no extension)",
        default     = "cloud_sdf",
    )

    # Internal — filled after a successful bake so the preview knows where to look
    last_bake_path: StringProperty(default = "")


def _apply_preset(props):
    p = props.resolution_preset
    if p in ("32", "64", "128", "256"):
        r = int(p)
        props.resolution_x = r
        props.resolution_y = r
        props.resolution_z = r


# ══════════════════════════════════════════════════════════════════════════════
#  OPERATORS
# ══════════════════════════════════════════════════════════════════════════════

class SDF_OT_Generate(Operator):
    """Bake SDF from mesh and export 3D texture"""
    bl_idname  = "sdf.generate"
    bl_label   = "Bake & Export SDF"
    bl_options = {"REGISTER"}

    def execute(self, context):
        props = context.scene.sdf_props

        # ── validation ────────────────────────────────────────────────────────
        obj = props.source_object
        if obj is None:
            self.report({'ERROR'}, "No source mesh selected.")
            return {'CANCELLED'}
        if obj.type != 'MESH':
            self.report({'ERROR'}, "Source object must be a mesh.")
            return {'CANCELLED'}

        resolution = (props.resolution_x, props.resolution_y, props.resolution_z)
        total_vox  = props.resolution_x * props.resolution_y * props.resolution_z
        if total_vox > 256 ** 3:
            self.report({'ERROR'},
                        f"Resolution {resolution} exceeds 256³ safety limit. "
                        "Reduce and try again.")
            return {'CANCELLED'}

        # ── output paths ──────────────────────────────────────────────────────
        out_dir = bpy.path.abspath(props.output_dir)
        os.makedirs(out_dir, exist_ok=True)
        base    = props.output_name
        ext_map = {"raw": ".raw", "f3d": ".f3d", "png": "", "npy": ".npy"}

        self.report({'INFO'},
                    f"Baking SDF {resolution} from '{obj.name}'  →  {out_dir}")

        # ── bake ──────────────────────────────────────────────────────────────
        sdf, w_min, w_max = compute_sdf_grid(
            obj         = obj,
            resolution  = resolution,
            padding     = props.padding,
            use_sign    = props.use_sign,
            operator    = self,
        )

        # ── export ────────────────────────────────────────────────────────────
        fmt = props.export_format
        nm  = props.normalize

        if fmt == "nvdf":
            out_path = os.path.join(out_dir, base + ".nvdf")
            export_nvdf(
                sdf            = sdf,
                path           = out_path,
                w_min          = w_min,
                w_max          = w_max,
                feather_voxels = props.nvdf_feather_voxels,
                density_scale  = props.nvdf_density_scale,
            )

        elif fmt == "raw":
            out_path = os.path.join(out_dir, base + ".raw")
            export_raw(sdf, out_path, nm)

        elif fmt == "f3d":
            out_path = os.path.join(out_dir, base + ".f3d")
            export_f3d(sdf, out_path, w_min, w_max, nm)

        elif fmt == "png":
            png_dir  = os.path.join(out_dir, base + "_slices")
            os.makedirs(png_dir, exist_ok=True)
            out_path = png_dir
            export_png_slices(sdf, png_dir, base, normalize=True)  # PNG must be [0,1]

        elif fmt == "npy":
            out_path = os.path.join(out_dir, base + ".npy")
            export_npy(sdf, out_path, nm)

        # ── companion files ───────────────────────────────────────────────────
        meta_path = os.path.join(out_dir, base + "_meta.json")
        glsl_path = os.path.join(out_dir, base + "_shader.glsl")

        write_metadata(meta_path, obj.name, resolution, w_min, w_max,
                       props.padding, nm, fmt,
                       nvdf_feather_voxels = props.nvdf_feather_voxels,
                       nvdf_density_scale  = props.nvdf_density_scale)
        write_glsl_snippet(glsl_path, w_min, w_max)

        props.last_bake_path = out_dir

        self.report({'INFO'},
                    f"✓ SDF exported to:  {out_path}\n"
                    f"  Meta:   {meta_path}\n"
                    f"  GLSL:   {glsl_path}")
        return {'FINISHED'}


class SDF_OT_OpenOutputDir(Operator):
    """Open the output directory in the OS file manager"""
    bl_idname = "sdf.open_output_dir"
    bl_label  = "Open Output Folder"

    def execute(self, context):
        props   = context.scene.sdf_props
        out_dir = bpy.path.abspath(props.output_dir)
        if not os.path.isdir(out_dir):
            self.report({'ERROR'}, f"Directory does not exist: {out_dir}")
            return {'CANCELLED'}

        import subprocess, sys
        if sys.platform == "win32":
            os.startfile(out_dir)
        elif sys.platform == "darwin":
            subprocess.Popen(["open", out_dir])
        else:
            subprocess.Popen(["xdg-open", out_dir])
        return {'FINISHED'}


class SDF_OT_SetFromActive(Operator):
    """Set source mesh from the active object"""
    bl_idname = "sdf.set_from_active"
    bl_label  = "Use Active Object"

    def execute(self, context):
        obj = context.active_object
        if obj and obj.type == 'MESH':
            context.scene.sdf_props.source_object = obj
            self.report({'INFO'}, f"Source set to '{obj.name}'")
        else:
            self.report({'WARNING'}, "Active object is not a mesh.")
        return {'FINISHED'}


class SDF_OT_EstimateTime(Operator):
    """Show a rough time estimate for the current settings"""
    bl_idname = "sdf.estimate_time"
    bl_label  = "Estimate Bake Time"

    def execute(self, context):
        props   = context.scene.sdf_props
        voxels  = props.resolution_x * props.resolution_y * props.resolution_z
        # Rough benchmark: ~50 k voxels / second on a modern CPU
        secs    = voxels / 50_000
        mins    = secs / 60
        msg = (f"{props.resolution_x}×{props.resolution_y}×{props.resolution_z} "
               f"= {voxels:,} voxels  ≈  {secs:.0f}s ({mins:.1f} min)")
        self.report({'INFO'}, msg)
        return {'FINISHED'}


# ══════════════════════════════════════════════════════════════════════════════
#  PANEL
# ══════════════════════════════════════════════════════════════════════════════

class SDF_PT_MainPanel(Panel):
    bl_label       = "SDF 3D Texture"
    bl_idname      = "SDF_PT_main"
    bl_space_type  = "VIEW_3D"
    bl_region_type = "UI"
    bl_category    = "SDF Tools"

    def draw(self, context):
        layout = self.layout
        props  = context.scene.sdf_props

        # ── Source ────────────────────────────────────────────────────────────
        box = layout.box()
        box.label(text="Source Mesh", icon="MESH_DATA")
        row = box.row(align=True)
        row.prop(props, "source_object", text="")
        row.operator("sdf.set_from_active", text="", icon="EYEDROPPER")

        # ── Resolution ────────────────────────────────────────────────────────
        box = layout.box()
        box.label(text="Grid Resolution", icon="GRID")
        box.prop(props, "resolution_preset", text="Preset")

        col = box.column(align=True)
        col.prop(props, "resolution_x", text="X")
        col.prop(props, "resolution_y", text="Y")
        col.prop(props, "resolution_z", text="Z")
        col.enabled = (props.resolution_preset == "custom")

        box.operator("sdf.estimate_time", text="Estimate Bake Time",
                     icon="SORTTIME")

        # ── Options ───────────────────────────────────────────────────────────
        box = layout.box()
        box.label(text="Options", icon="TOOL_SETTINGS")
        box.prop(props, "padding", slider=True)
        box.prop(props, "use_sign")
        box.prop(props, "normalize")

        # ── Export ────────────────────────────────────────────────────────────
        box = layout.box()
        box.label(text="Export", icon="EXPORT")
        box.prop(props, "export_format")

        # NVDF-specific options
        if props.export_format == "nvdf":
            sub = box.box()
            sub.label(text="NVDF Options", icon="SETTINGS")
            sub.prop(props, "nvdf_feather_voxels", slider=True)
            sub.prop(props, "nvdf_density_scale",  slider=True)
            sub.label(text="→ R8 density  (0=empty, 255=full)", icon="INFO")

        box.prop(props, "output_dir")
        box.prop(props, "output_name")

        # ── Action buttons ────────────────────────────────────────────────────
        layout.separator()
        row = layout.row()
        row.scale_y = 1.8
        row.operator("sdf.generate", icon="RENDER_RESULT")

        if props.last_bake_path:
            layout.operator("sdf.open_output_dir",
                            text="Open Output Folder", icon="FILE_FOLDER")


class SDF_PT_HelpPanel(Panel):
    bl_label       = "Usage Guide"
    bl_idname      = "SDF_PT_help"
    bl_space_type  = "VIEW_3D"
    bl_region_type = "UI"
    bl_category    = "SDF Tools"
    bl_options     = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        col    = layout.column(align=True)

        lines = [
            "1. Select a closed/manifold mesh.",
            "2. Choose resolution (64³ = fast preview).",
            "3. Format: NVDF (★) for your engine.",
            "4. Hit 'Bake & Export SDF'.",
            "5. Load in engine:",
            "   vcVCloud_LoadBC6HNVDF(path)",
            "   (format=0 / R8 branch)",
            "",
            "NVDF density encoding:",
            "  0   = empty (outside mesh)",
            "  255 = full density (inside)",
            "  Surface at ~127 (feathered)",
            "",
            "Feather = edge softness in voxels",
            "  0   = hard binary edge",
            "  2-4 = typical cloud softness",
            "",
            "Wrap modes (engine native):",
            "  X,Y = GL_REPEAT (tiling)",
            "  Z   = GL_CLAMP_TO_EDGE (height)",
        ]
        for ln in lines:
            col.label(text=ln)


# ══════════════════════════════════════════════════════════════════════════════
#  REGISTER / UNREGISTER
# ══════════════════════════════════════════════════════════════════════════════

_classes = (
    SDF_Properties,
    SDF_OT_Generate,
    SDF_OT_OpenOutputDir,
    SDF_OT_SetFromActive,
    SDF_OT_EstimateTime,
    SDF_PT_MainPanel,
    SDF_PT_HelpPanel,
)


def register():
    for cls in _classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.sdf_props = PointerProperty(type=SDF_Properties)


def unregister():
    for cls in reversed(_classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.sdf_props


if __name__ == "__main__":
    register()
