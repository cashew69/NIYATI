# Complete Guide to Volumetric Effects and Raymarching

## Table of Contents
1. [Basic Concepts](#basic-concepts)
2. [Raymarching](#raymarching)
3. [Volumetric Effects](#volumetric-effects)
4. [3D Textures](#3d-textures)
5. [OpenGL Setup](#opengl-setup)
6. [Putting It All Together](#putting-it-all-together)

---

## Basic Concepts

### What is a Volumetric Effect?

A volumetric effect is something that fills up a space in 3D. Think of it like fog, smoke, or clouds. Instead of drawing flat pictures on the outside, we need to show what happens inside the space too.

**Simple example:** If you look at a regular wall in a game, you only draw the outside surface. But for clouds, the camera can be inside them, and you need to show what the inside looks like.

### How Computers Draw Volumetric Things

Normally, a computer draws 3D shapes by:
1. Deciding what color each pixel on the screen should be
2. Finding what 3D object is closest to the camera
3. Drawing that object's color

For volumetric effects, we do something different:
1. We shoot an imaginary ray from the camera through each pixel
2. We walk along that ray step by step
3. At each step, we ask: "Is there fog/cloud/smoke here?"
4. We combine all these answers to get the final color

---

## Raymarching

### What is Raymarching?

Raymarching is a technique where we:
1. Shoot a ray (imaginary line) from the camera through a pixel
2. Walk along this ray in small steps
3. At each step, we check if there is something (like cloud density)
4. We collect information from each step and combine it

**Like walking through fog:**
- Imagine you are walking through fog
- Every step you take, the fog blocks a little bit of light
- After many steps, the light is mostly blocked
- Your eye sees a dark cloud

### The Basic Raymarching Loop

```
For each pixel on screen:
  Create a ray starting from camera pointing through that pixel
  
  Walk along the ray in steps:
    For each step:
      Check: Is there cloud/fog/smoke here?
      If yes:
        - Remember how much light is blocked
        - Add light from sun hitting the cloud
        - Add bounced light (scattering)
      Move to next step
  
  Combine all the light information
  Draw the final color
```

### Ray-AABB Intersection

Before we can march, we need to know: "Where do I start and stop walking?"

AABB stands for "Axis-Aligned Bounding Box" — a simple box made from 3 numbers (minimum position) and 3 numbers (maximum position).

**Why?** Because clouds only exist in a specific box. Outside the box, there is nothing. So we:
1. Start at the camera
2. Find where the ray enters the box (tNear)
3. Find where the ray exits the box (tFar)
4. Walk only between these two distances

```
Example in code:
  Box is from (0,0,0) to (500,500,500)
  Camera at (250, 250, -100)
  Ray shooting forward
  
  Ray enters box at distance 100 (tNear = 100)
  Ray exits box at distance 600 (tFar = 600)
  
  So we march from distance 100 to distance 600
```

### Step Sizes and Adaptive Marching

If we take the same step size everywhere, we waste time:
- In empty space: Why take tiny steps? Nothing is there anyway.
- In dense cloud: We need tiny steps to see details.

**Adaptive marching:** Take bigger steps in empty space, smaller steps in cloud.

Example:
```
Step 1: Empty space → take 20 units forward
Step 2: Empty space → take 20 units forward
Step 3: Found cloud! Go back and take 2 units forward
Step 4: In cloud → take 2 units forward
Step 5: In cloud → take 2 units forward
```

### Jitter

If we always start at the exact same position, we see lines and stripes (called "banding").

**Solution:** Add randomness. Each frame, start the march from a slightly different position. When many frames blend together, the stripes disappear.

**Where does the randomness come from?**
- Blue noise: A special noise pattern that looks random but is actually organized in a smart way. It helps the eye blend frames faster than regular random noise.

---

## Volumetric Effects

### Cloud Density

The most important question in raymarching is: "How much cloud is at this position?"

We answer this by combining several things:

#### 1. Height Profile

Clouds have different shapes depending on height:
- **Stratus clouds:** Flat layer, like a blanket
- **Cumulus clouds:** Tall with flat bottom, like floating mountains
- **Stratocumulus clouds:** In between

The height profile is a simple function that says:
- At 0% height: How cloudy? (0 = empty, 1 = fully cloudy)
- At 50% height: How cloudy?
- At 100% height: How cloudy?

For cumulus clouds:
- Bottom (0%): Empty
- Lower area (10%): Empty (flat base)
- Middle (40%): Very cloudy (main body)
- Top (80%): Cloudy (rounded top)
- Very top (100%): Empty (clouds don't go to infinity)

#### 2. Coverage

Not the whole sky is cloudy. We need a coverage value that says:
- 0.0 = clear sky everywhere
- 0.5 = half the sky has clouds
- 1.0 = totally cloudy

We can use simple coverage everywhere, or use a 2D map to say which areas are cloudy.

#### 3. Noise

Real clouds are not smooth blobs. They have bumpy surfaces with detailed edges.

We use 3D noise textures to add this detail:
- **Base noise:** Large wrinkles and bumps (the main cloud shape)
- **Detail noise:** Tiny wrinkles on the surface (fine details)

The noise values range from 0 to 1, and we use them to:
- Make solid cloud cores (value near 1)
- Make wisps at edges (value near 0)

#### 4. Combining Everything

Final density = (Height profile) × (Coverage) × (Base noise) × (Detail noise × (erosion strength))

Example:
```
Height = 1.0 (nice middle of cumulus)
Coverage = 0.8 (80% of sky has cloud)
Base noise = 0.7 (pretty cloudy)
Detail noise = 0.5 (medium wispy details)

Final = 1.0 × 0.8 × 0.7 × 0.5 = 0.28
```

### Light and Scattering

When light hits cloud particles, it bounces around (scatters). This is why clouds are bright on the sunny side.

#### The Sun Direction

We need to know where the sun is. We can:
1. Use a fixed direction (simple)
2. Use a sun object in the scene (realistic)

#### Shadow Light Marching

To know how much light reaches a point in the cloud:
1. Starting from that point
2. Walk toward the sun
3. Count how much cloud blocks the light
4. This number is called "light density"

We use fewer steps here than the main march because:
- We do not need the same detail quality
- It would be too slow to do full-detail march for every point

Example:
```
Main march: 96 steps to show details
Light march: 4 steps (enough to know if dark or bright)
```

#### Multiple Scattering

When light bounces multiple times inside a cloud, the math gets complicated. We use a trick:

"Multiple Scattering Approximation" = combination of:
- Light bouncing once (main light)
- Light bouncing twice (medium darkness)
- Light bouncing three times (extra darkness)

```
Final brightness = (1×bounce) × 0.8 + (2×bounce) × 0.3 + (3×bounce) × 0.1
```

#### Powder Effect (Beer-Powder)

In real clouds:
- The sunny side is very bright
- The shadow side is very dark
- The edges have a bright white glow (powder effect)

We create this by:
1. Calculating base brightness (from sun light)
2. Making the edges brighter (powder term)
3. Making the inside darker (shadows)

This happens mostly when looking toward the sun.

#### Phase Function

When light bounces off particles, it bounces more in some directions than others.

**Henyey-Greenstein phase function** = a math formula that says:
- Looking toward sun: More light bounces toward you
- Looking away from sun: Less light bounces toward you
- Looking to the side: Medium light

We use a blend of two phase functions to make realistic scattering.

### Compositing (Combining with Scene)

After raymarching, we need to put the clouds into the final image:

1. **Color:** The raymarched cloud color
2. **Alpha (transparency):** How see-through it is
3. **Position:** Use depth from raymarching to know if cloud is in front or behind objects

We use alpha blending:
```
Final pixel = (Cloud color × Cloud alpha) + (Scene color × (1 - Cloud alpha))
```

---

## 3D Textures

### What is a 3D Texture?

A 2D texture is a grid of colors:
```
2×2 texture (super small):
┌─────┬─────┐
│ Red │Blue │
├─────┼─────┤
│Green│White│
└─────┴─────┘

Each color is one texel (texture pixel)
```

A 3D texture is a cube of values:
```
2×2×2 texture:
┌─────────────────────┐
│  Top layer (z=1):   │   ┌─────┬─────┐
│                     │   │ Val1│ Val2│
│                     │   ├─────┼─────┤
│                     │   │ Val3│ Val4│
│                     │   └─────┴─────┘
│  Bottom layer (z=0):│   ┌─────┬─────┐
│                     │   │ Val5│ Val6│
│                     │   ├─────┼─────┤
│                     │   │ Val7│ Val8│
│                     │   └─────┴─────┘
└─────────────────────┘
```

### Why 3D Textures for Clouds?

Because we need a value at every 3D position:
- Density at (10, 20, 30)
- Density at (15, 25, 35)
- Density at (100, 50, 200)

A 3D texture stores these values in a compact form that the GPU can access quickly.

### Generating 3D Textures (CPU-side)

The generation process:

#### Step 1: Generate Value Noise

Value noise is a smooth random pattern. We create it by:
1. Dividing 3D space into a grid (lattice)
2. Assigning a random value to each corner
3. For any point between corners, blend the nearby corners

```
Visual example (2D for simplicity):
  (0,1)=0.7      (1,1)=0.3
     ·─────────────·
     │      ?      │  For point (0.5, 0.5):
     │   (0.5,0.5) │  Blend corners: 0.25 for each
     ·─────────────·
  (0,0)=0.2      (1,0)=0.8
```

#### Step 2: Stack Multiple Octaves (FBM)

FBM = Fractional Brownian Motion. We layer multiple noise functions:
- Layer 1 (large): Low-frequency (big bumps)
- Layer 2 (medium): Medium-frequency (medium bumps)
- Layer 3 (small): High-frequency (tiny bumps)

Combining them:
```
FBM = 0.5×(large) + 0.25×(medium) + 0.125×(small)

The larger contribution from big bumps gives fluffy-looking clouds.
```

#### Step 3: Create Worley Noise (Cellular Noise)

Worley noise creates a cellular pattern, like a honeycomb:
1. Divide space into cells
2. In each cell, place a random point (feature point)
3. For any position, find the nearest feature point
4. The value = 1 - (distance to nearest point)

Result: Center of cell = 1 (bright), edge of cell = 0 (dark)

```
Visual example:
  · = feature point
  1.0 at point, 0.0 at edge, smooth between

  ·               ·
       (cloudy)
      (edges)     ·
```

#### Step 4: Combine into Perlin-Worley

This is the secret sauce for clouds:
- FBM Perlin = smooth, billowy (but featureless)
- Worley = sharp, defined edges (but harsh)

Combined = smooth clouds with natural edges

Formula:
```
Perlin-Worley = Worley + (FBM × (1 - Worley))

This makes the Perlin fill the center of Worley cells.
```

#### Step 5: Create Multi-Octave Worley for Erosion

We create several Worley textures at different frequencies (4 cells, 8 cells, 16 cells) and pack them into texture channels:
- Channel R: Perlin-Worley (main cloud)
- Channel G: Worley 4 cells (erosion layer 1)
- Channel B: Worley 8 cells (erosion layer 2)
- Channel A: Worley 16 cells (erosion layer 3)

In the shader, we blend these:
```
Erosion = G × 0.625 + B × 0.25 + A × 0.125

Then we subtract erosion from the main cloud:
Final = Perlin-Worley - Erosion

This creates wispy edges instead of solid spheres.
```

### Texture Sampling

When we need a value at position (x, y, z), the GPU does:

1. **Convert to texture coordinates** (0 to 1 range)
2. **Find the 8 surrounding values** (corners of the cube we are in)
3. **Blend them together** (trilinear interpolation)

The blending is smooth so we do not see grid lines.

Example:
```
Want value at (x=0.3, y=0.7, z=0.5)
Texture coordinate = (0.3, 0.7, 0.5)

8 surrounding corners:
  (0,0,0)=0.2  (1,0,0)=0.4
  (0,1,0)=0.6  (1,1,0)=0.8
  (0,0,1)=0.1  (1,0,1)=0.3
  (0,1,1)=0.5  (1,1,1)=0.7

Blend using weights:
  x=0.3 → blend 70% with x=0, 30% with x=1
  y=0.7 → blend 30% with y=0, 70% with y=1
  z=0.5 → blend 50% with z=0, 50% with z=1

Final ≈ 0.425
```

### Texture Formats

Different formats store values differently:

#### RGBA8 (8 bits per channel)
- R, G, B, A each store 0-255
- Good for: Most noise textures (Perlin-Worley, Worley)
- Size: 4 bytes per value
- Quality: Good enough for cloud details

#### RGB8 (8 bits per channel, no alpha)
- R, G, B each store 0-255
- Good for: Detail erosion textures
- Size: 3 bytes per value
- Quality: Fine for high-frequency details

#### BC6H (Compressed, HDR)
- Compressed format (6 bits per block of pixels)
- Good for: Pre-baked density fields (takes less storage)
- Quality: Very high (HDR precision)
- Speed: Extremely fast (hardware decompresses automatically)

#### R32F (Float, 32 bits)
- Stores floating-point number (any value, not just 0-255)
- Good for: Scene depth, final cloud output
- Quality: Excellent precision
- Speed: Slower than integer formats

### Mipmap Levels

Mipmaps are smaller versions of the texture:

```
Original: 64×64×64
Mip 1:    32×32×32
Mip 2:    16×16×16
Mip 3:     8×8×8
```

When we sample far away, we use a smaller mip:
- Looking close: Use full resolution
- Looking far: Use smaller mip (faster, looks correct from distance)

In the light march (few steps), we use mip level 1:
```glsl
textureLod(u_noiseBase, uv, 1.0);  // Use mip 1, not full resolution
```

This makes light marching fast.

---

## OpenGL Setup

### What OpenGL Needs

OpenGL is the graphics library. For volumetric effects, we need:

1. **Shaders** — Programs that run on GPU
2. **Textures** — Data storage on GPU
3. **Buffers** — More data storage (for sphere positions)
4. **Rendering state** — Settings for how to draw

### Compute Shaders

A compute shader is a special type of program that does general computation on the GPU.

Unlike vertex/fragment shaders that process one vertex or pixel at a time, a compute shader can:
- Organize threads into groups
- Share data between threads
- Write anywhere in memory (not just one output pixel)

**Our setup:**
```glsl
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
```

This says: "Create 8×8=64 threads per group, arranged in a 2D grid."

The GPU will:
1. Launch many groups of 64 threads
2. Cover the entire output image (e.g., 1920×1080 pixels)
3. Each thread handles one or a few pixels

### Texture Creation

We create textures in this order:

#### Step 1: Create CPU Data

```cpp
// Allocate memory on CPU
unsigned char* data = (unsigned char*)malloc(64*64*64*4);
// Fill with noise values (GPU is fast and happens once at startup or on change)
// See noise_gen.comp.glsl and weather_gen.comp.glsl for GPU implementation.
// Baking is now handled by GPU compute shaders:
// vcCloud_BakeBaseNoiseGPU(id, N);
```


#### Step 2: Create GPU Texture

```cpp
// Create OpenGL texture object
GLuint id;
glGenTextures(1, &id);

// Bind it (make it active)
glBindTexture(GL_TEXTURE_3D, id);

// Upload data to GPU
glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8,
             64, 64, 64,  // dimensions
             0,           // border
             GL_RGBA,     // format of data
             GL_UNSIGNED_BYTE,  // type
             data);       // pointer to data

// Create mipmaps automatically
glGenerateMipmap(GL_TEXTURE_3D);

// Set filtering (how to blend)
glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// Set wrapping (what happens at edges)
glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
```

#### Step 3: Bind to Shader

In the compute shader:
```glsl
uniform sampler3D u_noiseBase;
```

In C++:
```cpp
// Set which texture unit (0, 1, 2, etc.)
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_3D, s_NoiseTexBase);

// Tell shader to use texture unit 0
GLint loc = glGetUniformLocation(program, "u_noiseBase");
glUniform1i(loc, 0);
```

### Shader Storage Buffer Objects (SSBO)

SSBOs are for storing large arrays of data that shaders can read.

**Our use:** Storing sphere positions (up to 256 spheres × 4 floats each)

```cpp
// Create buffer
GLuint buffer;
glGenBuffers(1, &buffer);
glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);

// Upload data
float* sphereData = ...;  // array of sphere positions
int bytes = 256 * 4 * sizeof(float);
glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, sphereData, GL_DYNAMIC_DRAW);

// Bind to shader binding point
glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
```

In the shader:
```glsl
layout(std430, binding = 1) readonly buffer CloudSpheresBuffer {
  vec4 spheres[];
};

// Now we can read: spheres[0], spheres[1], etc.
```

### Image Load/Store

Normally, textures are read-only in shaders. But we need to write the final cloud color.

For this, we use image load/store:

```glsl
layout(rgba16f, binding = 0) writeonly uniform image2D u_outputImage;

// In compute shader:
imageStore(u_outputImage, pixel, color);
```

The `rgba16f` means:
- RGBA: Four channels (Red, Green, Blue, Alpha)
- 16f: 16-bit floating-point (very precise, good for accumulation)

### Rendering the Final Result

After compute shader writes to the output texture, we need to display it:

1. **Create a full-screen quad** (actually a triangle, but covers whole screen)
2. **Sample the cloud texture** in a fragment shader
3. **Composite with the scene** (if needed)

The vertex shader:
```glsl
const vec2 verts[3] = vec2[](
    vec2(-1.0, -1.0),  // Bottom-left
    vec2( 3.0, -1.0),  // Bottom-right (bigger to cover)
    vec2(-1.0,  3.0)   // Top-left (bigger to cover)
);
gl_Position = vec4(verts[gl_VertexID], u_depth, 1.0);
```

The fragment shader:
```glsl
uniform sampler2D u_cloudTex;
void main() {
    vec4 cloud = texture(u_cloudTex, vTexCoord);
    if (cloud.a < 0.004) discard;  // Skip transparent pixels
    FragColor = cloud;
}
```

### Uniforms

Uniforms are values passed from CPU to GPU for every frame:

```cpp
// Get location (once, at initialization)
GLint loc = glGetUniformLocation(program, "u_cameraPos");

// Set value (every frame)
glUniform3f(loc, camera.x, camera.y, camera.z);
```

Common uniform types:
- `glUniform1f` — one float
- `glUniform3f` — three floats (vec3)
- `glUniformMatrix4fv` — 4×4 matrix

We use many uniforms:
- Camera position, view/projection matrices
- Cloud parameters (density, coverage, noise scale)
- Sun direction, colors
- Time (for animation)

---

## Putting It All Together

### The Complete Flow

```
Frame start:
  │
  ├─ Update uniforms (camera, time, parameters)
  │
  ├─ Bind textures to GPU
  │    - Base noise (64^3)
  │    - Detail noise (32^3)
  │    - Blue noise (jitter)
  │    - History (previous frame)
  │    - Weather map (coverage/type)
  │    - Scene depth (occlusion)
  │
  ├─ Bind SSBO (sphere positions)
  │
  ├─ Bind output texture
  │
  ├─ Dispatch compute shader
  │    - One thread per pixel (or group of pixels)
  │    - Each thread:
  │      - Creates a camera ray
  │      - Finds where ray enters/exits cloud volume
  │      - Marches along ray in steps:
  │        • Sample density at current position
  │        • If dense enough:
  │          - Sample shadows (light march)
  │          - Calculate scattering
  │          - Accumulate light and color
  │        • If empty:
  │          - Take bigger step (adaptive)
  │      - Blend with previous frame history (TAA)
  │      - Write result to output texture
  │
  ├─ Render full-screen quad
  │    - Samples cloud texture
  │    - Blends with scene (alpha compositing)
  │    - Displays result
  │
  └─ Frame end
```

### Key Parameters to Control

#### Cloud Shape
- `cloudType` — 0=stratus, 0.5=stratocumulus, 1=cumulus
- `noiseScale` — How big the bumps are (frequency)
- `detailScale` — How fine the erosion details are
- `coverage` — What percentage of sky has clouds

#### Density
- `densityScale` — Overall brightness/darkness (multiply)
- `absorption` — How much light gets blocked
- `erosion` — How much detail erodes the solid parts

#### Light
- `sunColor` — What color is the sun
- `sunIntensity` — How bright the sun is
- `ambientStrength` — Background light level
- `scatterG` — How much light scatters toward viewer

#### Appearance
- `cloudColorTop` — Color at cloud top (where sun hits)
- `cloudColorBottom` — Color at cloud bottom (shadow side)
- `silverLining` — Glow around edges when backlit

#### Performance
- `maxSteps` — How many march steps (more = slower but detailed)
- `stepSize` — How big each step is (bigger = faster but less detail)
- `adaptiveFactor` — How much to grow step in empty space
- `renderScale` — What resolution to render at (0.5 = half resolution)

#### Temporal Anti-Aliasing (TAA)
- `enableTAA` — Smooth out jitter artifacts
- `taaBlend` — How much to blend with previous frame
- `frameIndex` — Internal counter for different samples each frame

### Simple Volumetric Effect Recipe

To create a new volumetric effect (fog, dust, etc.):

1. **Create your density function** (replaces `cloudDensity`)
   - Input: 3D position
   - Output: density (0 = empty, 1 = solid)

2. **Set up the same raymarching loop**
   - Walk along ray from camera
   - Sample density at each step
   - Accumulate color

3. **Add lighting** (optional but recommended)
   - For each step with density > 0
   - March toward light source
   - Calculate how much light reaches that point

4. **Composite the result**
   - Write to output texture
   - Blend with scene

5. **Create noise textures**
   - Generate 3D textures for your effect
   - Can be different from clouds (smoke, dust, etc.)

---

## Optimization Tricks

### Adaptive Stepping
Instead of always taking the same size step, adapt to the scene:
- Empty space: Take big steps to skip ahead
- Dense region: Take small steps for detail
- Result: Fewer total steps needed

### Dual-Pass Rendering
Render near and far regions separately at different resolutions:
- Near (e.g., 480×270): Full detail
- Far (e.g., 960×540): Less detail, bigger steps
- Composite together
- Saves cost without losing visible quality

### Checkerboard Rendering
Only render 1 of 4 pixels per frame (checkerboard pattern):
- Frame 1: Pixels at (even x, even y)
- Frame 2: Pixels at (odd x, even y)
- Frame 3: Pixels at (even x, odd y)
- Frame 4: Pixels at (odd x, odd y)
- Use TAA to fill in missing pixels from history
- Result: 4× fewer ray marches (but fills in over 4 frames)

### Mip-level Selection for Light Marching
Light marching does not need full detail, so:
- Main march: Sample full-resolution texture
- Light march: Sample mip level 1 (1/8 the data)
- Result: Faster shadow calculation

### Weather Map Fast-Path
Before sampling expensive 3D texture:
- Check 2D weather map coverage
- If coverage near 0: Skip 3D texture, take big step
- If coverage high: Sample 3D texture
- Result: Clear sky regions are very fast

---

## Common Mistakes to Avoid

### Clamp Bounds Reversed
On AMD GPUs, `clamp(x, lo, hi)` where lo > hi returns 0 (undefined behavior). Always use:
```glsl
clamp(value, min(lo, hi), max(lo, hi))
```

### Texture Slab Visibility
When rays march nearly parallel to one texture axis, they sample the same slice for many steps. The trilinear boundary shows as a flat plane.

Fix: Apply sub-texel UVW jitter that varies smoothly in world space:
```glsl
vec3 jitter = vec3(
    sin(p.x*0.073 + p.z*0.051),
    sin(p.y*0.079 + p.x*0.063),
    sin(p.z*0.067 + p.y*0.057)
) * 0.018;
```

### Jitter-Related Shimmering at Distance
Animated jitter (frame-varying) causes shimmer when step sizes are large because different frames sample completely different parts of the volume.

Fix: Use static (world-anchored) jitter at distance, switch to animated jitter up close:
```glsl
float blend = smoothstep(switchDist*0.5, switchDist*1.5, distance);
finalJitter = mix(animated, static, blend);
```

### Depth Reprojection for TAA
Using the cloud box midpoint for TAA reprojection causes ghosting when thin clouds are near the camera.

Fix: Use the actual first-hit distance (where density first exceeds threshold):
```glsl
// Better: use first intersection point
if (outFirstHit > 0.0) {
    vec2 prevUV = reprojectUV(cameraPos + rayDir * outFirstHit);
    history = texture(u_historyTex, prevUV);
}
```

### NVDF Tiling Patterns
When using a pre-baked density texture, the same pattern repeats across the world.

Fixes:
1. Rotate each entity's sample coordinates
2. Add per-entity phase offset
3. Apply height-width modulation to vary shape
4. Domain-warp with low-frequency noise

---

## Summary

Volumetric effects work by:

1. **Raymarching:** Walk along rays from the camera, sampling density at each step
2. **Density calculation:** Combine height profile + coverage + noise to get density
3. **Lighting:** Calculate how much light reaches each step point (shadow marching)
4. **Scattering:** Model how light bounces inside the volume
5. **Composition:** Accumulate color and transparency along the ray
6. **3D Textures:** Store noise patterns that define the cloud shape
7. **Optimization:** Use adaptive steps, checkerboarding, and other tricks to make it fast

The same concept works for fog, smoke, dust, fire, or any volumetric effect — you just change the density function and the lighting model.
