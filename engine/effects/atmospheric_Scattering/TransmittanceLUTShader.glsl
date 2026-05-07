#version 430 core

// 1. Set up our 8x8 thread group
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// 2. Bind the blank 16-bit texture we created in C++
layout(rgba16f, binding = 0) writeonly uniform image2D transmittanceLUT;

void main() {
    // 3. Get the exact X and Y pixel coordinate this thread is working on
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    // ... Atmospheric ray marching math will go here ...
    vec4 final_transmittance = vec4(1.0, 0.0, 0.0, 1.0); // Placeholder

    // 4. Save the calculated value directly into that specific pixel 💾
    imageStore(transmittanceLUT, pixel_coords, final_transmittance);
}
