#version 330 core
in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uIconTex;
uniform vec3      uTintColor;

void main() {
    vec4 s = texture(uIconTex, vTexCoord);
    if (s.a < 0.05) discard;
    FragColor = vec4(s.rgb * uTintColor, s.a);
}
