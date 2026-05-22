#version 460 core
uniform float u_depth;
out vec2 vTexCoord;

// Fullscreen triangle. z is controllable via u_depth.
//   z = 0.99  (far plane)  -> clouds render behind everything (classic mode).
//   z = -0.99 (near plane) -> clouds render over everything (shader-occlusion mode).
void main() {
    const vec2 verts[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(verts[gl_VertexID], u_depth, 1.0);
    vTexCoord   = verts[gl_VertexID] * 0.5 + 0.5;
}
