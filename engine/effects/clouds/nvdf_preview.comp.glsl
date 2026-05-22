#version 460 core

// Copies a single slice of a 3D R8 texture into a 2D RGBA8 preview image
// so the editor can hand it to ImGui::Image (which needs a 2D texture).

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba8, binding = 0) writeonly uniform image2D u_preview;

uniform sampler3D u_nvdf;
uniform float u_slice;  // 0..1 normalised position along u_axis
uniform int   u_axis;   // 0 = slice along X, 1 = along Y, 2 = along Z

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 sz = imageSize(u_preview);
    if (px.x >= sz.x || px.y >= sz.y) return;

    vec2 uv = (vec2(px) + 0.5) / vec2(sz);
    vec3 spl;
    if      (u_axis == 0) spl = vec3(u_slice, uv.x, uv.y);
    else if (u_axis == 1) spl = vec3(uv.x, u_slice, uv.y);
    else                  spl = vec3(uv.x, uv.y, u_slice);

    float d = texture(u_nvdf, spl).r;
    imageStore(u_preview, px, vec4(d, d, d, 1.0));
}
