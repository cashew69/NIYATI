#version 460 core

layout(vertices = 4) out;

in vec3 FragPos[];
in vec3 Normal[];
in vec2 TexCoord[];

out vec3 tcFragPos[];
out vec3 tcNormal[];
out vec2 tcTexCoord[];

uniform mat4 uView;
uniform float uTessLevelInner; // minimum tess level (far patches)
uniform float uTessLevelOuter; // maximum tess level (close patches)

void main(void)
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcFragPos[gl_InvocationID]  = FragPos[gl_InvocationID];
    tcTexCoord[gl_InvocationID] = TexCoord[gl_InvocationID];
    tcNormal[gl_InvocationID]   = Normal[gl_InvocationID];

    if (gl_InvocationID == 0)
    {
        const float MIN_DISTANCE = 100.0;
        const float MAX_DISTANCE = 2000.0;

        // True eye-space distance — correct for patches far to the side
        vec4 eyeSpacePos00 = uView * vec4(FragPos[0], 1.0);
        vec4 eyeSpacePos01 = uView * vec4(FragPos[1], 1.0);
        vec4 eyeSpacePos10 = uView * vec4(FragPos[2], 1.0);
        vec4 eyeSpacePos11 = uView * vec4(FragPos[3], 1.0);

        float distance00 = clamp((length(eyeSpacePos00.xyz) - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);
        float distance01 = clamp((length(eyeSpacePos01.xyz) - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);
        float distance10 = clamp((length(eyeSpacePos10.xyz) - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);
        float distance11 = clamp((length(eyeSpacePos11.xyz) - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0, 1.0);

        // At distance 0 (close) → uTessLevelOuter (max quality)
        // At distance 1 (far)   → uTessLevelInner (min quality)
        float tessLevel0 = mix(uTessLevelOuter, uTessLevelInner, min(distance10, distance00));
        float tessLevel1 = mix(uTessLevelOuter, uTessLevelInner, min(distance00, distance01));
        float tessLevel2 = mix(uTessLevelOuter, uTessLevelInner, min(distance01, distance11));
        float tessLevel3 = mix(uTessLevelOuter, uTessLevelInner, min(distance11, distance10));

        gl_TessLevelOuter[0] = tessLevel0;
        gl_TessLevelOuter[1] = tessLevel1;
        gl_TessLevelOuter[2] = tessLevel2;
        gl_TessLevelOuter[3] = tessLevel3;

        gl_TessLevelInner[0] = max(tessLevel1, tessLevel3);
        gl_TessLevelInner[1] = max(tessLevel0, tessLevel2);
    }
}
