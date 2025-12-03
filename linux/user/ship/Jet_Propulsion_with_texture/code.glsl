#version 460 core

// precision highp float;
// precision highp sampler2D;

in vec2 uv;

out vec4 out_color;


// size of the canvas in pixels
uniform vec2 u_resolution;

uniform float u_time;

uniform vec4 u_mouse;

uniform sampler2D u_textures[16];


#pragma endregion
vec4 fragColor;  // Modern GLSL output (required for #330)
float u_speed = 0.5f;

void main() {

    float scrolledV = uv.y + u_time * u_speed;
    float noiseValue = texture(u_textures[0], vec2(uv.x, scrolledV)).r;
    float tailFade = pow(uv.y, 2.0);
    float finalAlpha = noiseValue * tailFade * 1.5;
    vec4 transparentBlack = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 flameColor = vec4(1.643, 0.545, 0.992, 1.000);
    
    out_color = mix(transparentBlack, flameColor, finalAlpha);;
}
