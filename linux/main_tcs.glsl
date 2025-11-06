#version 460 core

layout(vertices = 4) out;

in vec3 vNormal[];
in vec2 vTexCoord[];

out vec3 tcNormal[];
out vec2 tcTexCoord[];

void main(void)
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcTexCoord[gl_InvocationID] = vTexCoord[gl_InvocationID];
    tcNormal[gl_InvocationID] = vNormal[gl_InvocationID];

    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = 16.0;
        gl_TessLevelOuter[1] = 16.0;
        gl_TessLevelOuter[2] = 16.0;
        gl_TessLevelOuter[3] = 16.0;

        gl_TessLevelInner[0] = 16.0;
        gl_TessLevelInner[1] = 16.0;
    }
}
