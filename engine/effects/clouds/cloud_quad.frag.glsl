#version 460 core
in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D u_cloudTex;

void main() {
    vec4 cloud = texture(u_cloudTex, vTexCoord);
    if (cloud.a < 0.004) discard;
    FragColor = cloud;
}
