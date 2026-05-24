# Atmospheric Scattering — Complete Technical Guide

Based on the Hillaire 2020 implementation in this engine (`engine/effects/atmospheric_Scattering/sky_atmosphere_node.cpp`).

---

## 1. High Level Overview

The sky is not a painted texture. It is the result of light from the sun bouncing around inside a sphere of gas that wraps the planet. When you look at the sky, every pixel is the result of light that:
1. Came from the sun
2. Hit gas molecules somewhere between you and the edge of the atmosphere
3. Bounced toward your eye

The engine computes this using three textures called LUTs (Look-Up Tables). Each one precomputes one expensive calculation so it does not have to be redone for every pixel every frame.

```
SUN
 │
 ▼  travels through atmosphere
┌──────────────────���─────────────┐  ← Top of atmosphere (6420 km from planet center)
│                                │
│   ← light bounces here →      │
│                                │
│  CAMERA                        │
└────────────────���───────────────┘  ← Ground (6360 km from planet center)
```

**The three LUTs and what they store:**

| LUT | Size | Rebuilt | Stores |
|-----|------|---------|--------|
| Transmittance LUT | 256×64 | Only when params change | How much sunlight survives from any point to the top of atmosphere |
| Multi-Scatter LUT | 32×32 | Only when params change | How much light bounces MORE than once before reaching your eye |
| Sky-View LUT | 200×200 | Every frame | The final color of the sky in every direction |

The final render is one fullscreen triangle that reads from the Sky-View LUT.

---

## 2. The Physics

### 2.1 What is in the atmosphere

The atmosphere has three things that affect light:

**Rayleigh scattering** — caused by tiny gas molecules (nitrogen, oxygen). These molecules are much smaller than the wavelength of light. They scatter short wavelengths (blue) much more than long wavelengths (red). This is why the sky is blue during the day and red at sunset (the sun's light travels a longer path and all the blue gets scattered away before it reaches you).

**Mie scattering** — caused by larger particles: dust, aerosols, water droplets. These scatter all wavelengths roughly equally. This produces the white-grey haze around the sun and near the horizon.

**Ozone absorption** — the ozone layer sits at about 25 km altitude. It absorbs certain wavelengths (mainly red and some green), which gives the sky a deeper blue at the zenith.

### 2.2 The single concept everything builds on: Optical Depth

When light travels through a medium, it gets weaker. How much weaker depends on:
- How dense the medium is
- How far the light travels through it

**Optical depth** is the total amount of "stuff" the light passed through. It is a number. Zero means nothing in the way. Large means a lot in the way.

```
Transmittance = exp(-optical_depth)
```

This is the fraction of light that survives. If optical depth is 0, transmittance is 1.0 (all light passes). If optical depth is 3.0, transmittance is exp(-3) ≈ 0.05 (5% passes).

This one equation runs the entire sky system.

---

## 3. The Math — Every Piece

### 3.1 Density Functions

The atmosphere gets thinner as you go up. The density at any altitude is:

```
density_rayleigh(h) = exp(h / H_R)   where H_R = -8 km (scale height)
density_mie(h)      = exp(h / H_M)   where H_M = -1.2 km
density_ozone(h)    = max(0, 1 - |h - 25| / 15)    (tent centered at 25 km)
```

`h` is altitude in km above the surface. The scale height tells you how fast the gas thins out.

**What exp(h / H) means:**
- At h = 0 km: density = exp(0) = 1.0 (full density at sea level)
- At h = 8 km (Rayleigh scale height): density = exp(-1) ≈ 0.37 (37% remains)
- At h = 16 km: density = exp(-2) ≈ 0.14
- At h = 80 km: density = exp(-10) ≈ 0.0000454 (essentially zero)

The negative scale height (H_R = -0.125 when written as a multiplier instead of -8) means density falls off exponentially with altitude. The atmosphere is thick near the ground and thin at the top.

**In your engine's GLSL:**
```glsl
float hKm      = max(0.0, h);
float rayleigh = exp(uRayleighExpScale * hKm);   // uRayleighExpScale = -0.125 = 1/(-8)
float mie      = exp(uMieDensityExpScale * hKm); // uMieDensityExpScale = -0.8333 = 1/(-1.2)
float ozone    = max(0.0, 1.0 - abs(hKm - 25.0) / 15.0);
```

---

### 3.2 Extinction and Scattering Coefficients

At any point in the atmosphere, light is lost by two processes:
- **Scattering** — light is redirected away from its path (but it goes somewhere else)
- **Absorption** — light is converted to heat (truly lost)

Together they are called **extinction**:

```
extinction = scattering + absorption
```

For each type:

```
σ_rayleigh = β_R × density_rayleigh(h)     β_R = (5.802, 13.558, 33.1) × 10⁻³ /km
σ_mie      = β_M × density_mie(h)          β_M = 3.996 × 10⁻³ /km (scattering only)
α_mie      = α_M × density_mie(h)          α_M = 4.4 × 10⁻³ /km (absorption)
σ_ozone    = β_O × density_ozone(h)        β_O = (0.650, 1.881, 0.085) × 10⁻³ /km
```

Rayleigh scattering has different values for R, G, B — that's the vec3. Blue (33.1) is scattered 5.7× more than red (5.802). This ratio is directly why the sky is blue.

**Why the units `/km`?** Because when you integrate (sum up) along a path measured in km, the units cancel and you get a dimensionless optical depth number.

**In your engine:**
```glsl
vec3 rayleighExt = uRayleighScattering * density.x;              // vec3 × float
vec3 mieExt      = (vec3(uMieScattering) + vec3(uMieAbsorption)) * density.y;
vec3 ozoneExt    = uAbsorptionExtinction * density.z;
extinction = rayleighExt + mieExt + ozoneExt;
```

---

### 3.3 Optical Depth (the integral)

To find out how much light survives from point A to point B, you walk along the path from A to B in small steps, add up the extinction at each step:

```
optical_depth = ∫₀ᴸ extinction(pos(t)) dt
```

The integral sign ∫ means "sum up infinitely many infinitely small pieces." In practice, you do it with a loop (numerical integration):

```
optical_depth ≈ Σᵢ extinction(pos_i) × dt
```

where `dt` is the step size and `pos_i = start + direction × t_i`.

**In your engine (Transmittance LUT shader):**
```glsl
const int STEPS = 40;
float dt = tMax / float(STEPS);
vec3 opticalDepth = vec3(0.0);

for (int i = 0; i < STEPS; i++) {
    float t   = (float(i) + 0.5) * dt;   // sample at center of each step
    vec3  pos = orig + dir * t;
    float alt = length(pos) - uBotR;
    vec3  dens = sampleAtmosDensity(alt);
    opticalDepth += extinctionFromDensity(dens) * dt;
}
vec3 transmittance = exp(-opticalDepth);
```

The `+ 0.5` in `(i + 0.5)` places the sample at the midpoint of each step instead of the start. This is called the midpoint rule and it is more accurate than sampling at step starts.

---

### 3.4 Phase Functions

When light scatters off a particle, it does not go equally in all directions. The phase function describes the probability distribution of scatter directions.

**Rayleigh phase function:**
```
P_R(θ) = (3 / 16π) × (1 + cos²θ)
```

`θ` is the angle between the incoming light direction and the scatter direction. `cos θ = dot(viewDir, sunDir)`.

- At θ = 0° (forward scatter, looking toward sun): `P = 3/16π × 2 = 0.119`
- At θ = 90° (side): `P = 3/16π × 1 = 0.0597`
- At θ = 180° (backscatter, looking away from sun): `P = 3/16π × 2 = 0.119` (same as forward)

Rayleigh scattering is symmetric forward/backward.

**Mie phase function (Henyey-Greenstein):**
```
P_M(θ, g) = [3(1-g²)(1+cos²θ)] / [8π(2+g²)(1 + g² - 2g cosθ)^(3/2)]
```

`g` is the anisotropy parameter (0 = isotropic, 1 = fully forward). Your engine uses `g = 0.8`.

At g = 0.8, forward scatter is MUCH stronger than backscatter. This is why the sky is bright around the sun (Mie forward scattering peak) and why haze appears as a white glow around the sun direction.

**In your engine:**
```glsl
float phaseRayleigh(float cosTheta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta*cosTheta);
}

float phaseMie(float cosTheta, float g) {
    float g2  = g*g;
    float num = 3.0 * (1.0 - g2) * (1.0 + cosTheta*cosTheta);
    float den = 8.0 * PI * (2.0 + g2) * pow(abs(1.0 + g2 - 2.0*g*cosTheta), 1.5);
    return num / max(den, 1e-7);
}
```

The `max(den, 1e-7)` prevents divide-by-zero when `cosTheta ≈ 1` and `g ≈ 1`.

---

### 3.5 The Rendering Equation (the full thing)

The luminance (brightness) you see when looking in direction `v` from position `x` is:

```
L(x, v) = ∫₀ᵀ T(x, x+tv) × [σ_R×P_R(θ) + σ_M×P_M(θ)] × T(x+tv, sun) × L_sun  dt
```

Breaking this down term by term:

| Term | What it means |
|------|--------------|
| `∫₀ᵀ ... dt` | Sum along the ray from camera to top of atmosphere |
| `T(x, x+tv)` | How much light survives from camera to this sample point |
| `σ_R × P_R(θ)` | Rayleigh scattering coefficient × probability of scatter toward camera |
| `σ_M × P_M(θ)` | Same for Mie |
| `T(x+tv, sun)` | How much sunlight survives from sample point to the sun (through atmosphere) |
| `L_sun` | Brightness of the sun |

**In plain language:** For each point along the view ray, you ask "how much sunlight arrives here, and how much of that sunlight bounces toward my eye, and how much of that surviving light makes it back to my eye without being absorbed."

**Transmittance shorthand:** `T(A, B) = exp(-∫ₐᴮ extinction(t) dt)`

This is the optical depth integral from earlier, wrapped in exp(-).

---

### 3.6 Multiple Scattering

The equation above only handles light that bounces once (sun → atmosphere → eye). In reality light bounces many times. Hillaire 2020 approximates all bounces beyond the first with a single number called `Ψ_ms` (psi_ms).

```
L_multi(x, v) = ∫₀ᵀ T(x, x+tv) × [σ_R + σ_M] × Ψ_ms(x+tv, sun_dir) dt
```

`Ψ_ms` does not have a phase function (it uses isotropic = 1/4π, averaged over all directions) because after many bounces, light has lost its directional preference.

The Multi-Scatter LUT computes `Ψ_ms` using 64 sphere directions (Fibonacci spiral) and 20 ray-march steps per direction. The geometric series approximation `Ψ = L_2nd / (1 - f_ms)` sums up all orders of scattering at once.

---

### 3.7 Transmittance LUT Parameterization

The LUT is 256×64. Each pixel (u, v) maps to a (view_height, view_angle) pair using Hillaire's nonlinear remapping. The goal is to distribute LUT pixels so more resolution goes to the interesting regions (near the horizon, near the ground).

```
H = sqrt(topR² - botR²)       ← distance from ground to "horizon point" on atmosphere sphere
ρ = H × v                     ← parameterizes height
viewH = sqrt(ρ² + botR²)

dMin = topR - viewH
dMax = ρ + H
d = dMin + u × (dMax - dMin)
cosAngle = (H² - ρ² - d²) / (2 × viewH × d)
```

This looks complex but it is just a mapping that packs more UV pixels near the atmospheric horizon where accuracy matters most. When you sample the LUT, you run the same math in reverse.

---

### 3.8 Sky-View LUT Parameterization

The Sky-View LUT is 200×200. Each pixel maps to a view direction using nonlinear latitude:

```
If v < 0.5 (below horizon):
    t = 1 - 2v
    latitude = -(π/2) × t²

If v ≥ 0.5 (above horizon):
    t = 2v - 1
    latitude = (π/2) × t²
```

The squaring `t²` compresses the zenith (top of sky, rarely interesting) and expands the horizon region (where most scattering color variation happens).

At v = 0.5: latitude = 0 → horizontal direction (the horizon).
At v = 1.0: latitude = π/2 → straight up (the zenith).
At v = 0.0: latitude = -π/2 → straight down.

**When you look up the LUT** in the final render, you invert this mapping:
```
latitude = asin(dir.y)
t = sqrt(|latitude| / (π/2))
v = 0.5 + 0.5 × sign(latitude) × t      (above horizon)
v = 0.5 × (1 - t)                        (below horizon)
```

---

## 4. Visual Impact of Each Parameter

### Rayleigh Scattering `β_R = (5.802, 13.558, 33.1) × 10⁻³ /km`

The ratio between the three channels determines sky color. Blue (33.1) ÷ Red (5.802) = 5.7. Physically this is why the sky is blue.

| Change | Visual Result |
|--------|--------------|
| Increase all equally | Sky gets more opaque, hazier |
| Increase red channel | Sky becomes more purple/violet |
| Increase blue channel | Sky becomes more saturated blue |
| Decrease all | Sky becomes thin and dark, like on Mars (red planet has thin atmosphere) |
| Set to zero | No Rayleigh = no sky color, sun only |

### Mie Scattering `β_M = 3.996 × 10⁻³ /km`, Anisotropy `g = 0.8`

Mie controls the haze/glow around the sun and the white horizon layer.

| Change | Visual Result |
|--------|--------------|
| Increase `β_M` | Thicker white haze, less visible sky color, foggy day |
| Decrease `β_M` | Crisp sky, sun is harsher, less atmospheric depth |
| Increase `g` toward 1.0 | Sun glow becomes very tight and intense |
| Decrease `g` toward 0.0 | Sun glow spreads across the whole sky |
| Set `g = 0` | Completely uniform haze, no sun direction preference |

### Ozone Absorption `β_O = (0.650, 1.881, 0.085) × 10⁻³ /km`

Absorbs red and green at 25 km altitude. Without ozone, the sky zenith would be a lighter, less saturated blue.

| Change | Visual Result |
|--------|--------------|
| Increase | Deeper blue at zenith |
| Set to zero | Sky becomes lighter blue, more cyan |

### Scale Heights `H_R = 8 km`, `H_M = 1.2 km`

| Change | Visual Result |
|--------|--------------|
| Increase `H_R` | Rayleigh effects visible at higher altitude, atmosphere looks thicker |
| Decrease `H_R` | Thin atmosphere, Rayleigh effects concentrated near ground |
| Increase `H_M` | Mie haze extends higher, hazier sky overall |
| Decrease `H_M` | Mie haze stays at ground level |

### Camera Height

The camera height in atmosphere space changes what you see dramatically:
- Near ground: horizon is at eye level, you see the full sky dome
- At 10 km altitude: horizon curves below eye level (airplane view)
- At 100 km: atmosphere is thin above you, you see the curve of the planet

### Sun Direction `uSunDir.y`

This is the most powerful visual parameter.
- `y = 1.0` (straight up): blue sky, short light paths, minimal reddening
- `y = 0.1` (near horizon): orange/red sky, long light paths through dense atmosphere
- `y = 0.0` (on horizon): sunset, maximum reddening, Mie creates horizon glow
- `y < 0` (below horizon): night, sky is dark, only scattered light remains

---

## 5. How it Maps to GLSL

### 5.1 Ray-Sphere Intersection

Math: find t where `|orig + t×dir|² = R²`

```
|orig|² + 2t(orig·dir) + t² = R²
t² + 2bt + c = 0      where b = orig·dir, c = |orig|² - R²
discriminant = b² - c
t = -b ± sqrt(discriminant)
```

**In GLSL:**
```glsl
float raySphereIntersectNearest(vec3 orig, vec3 dir, float R) {
    float b  = dot(orig, dir);
    float c  = dot(orig, orig) - R*R;
    float d  = b*b - c;
    if (d < 0.0) return -1.0;     // no intersection
    float sd = sqrt(d);
    float t1 = -b - sd;            // near intersection
    float t2 = -b + sd;            // far intersection
    if (t1 > 0.0) return t1;       // both ahead, return nearest
    if (t2 > 0.0) return t2;       // t1 behind, return far (camera inside sphere)
    return -1.0;                   // both behind
}
```

**Why different from math:** The standard quadratic is `t = (-b ± sqrt(b²-ac)) / 2a`. Here `a = 1` (since `dir` is normalized, `dot(dir,dir) = 1`) so `a` drops out. The `2` also disappears because `b` in code already absorbed the factor of 2 from the quadratic: in `dot(orig, dir)`, the factor of 2 from `2t(orig·dir)` is folded into the definition of `b` in the code. This is a common optimization.

### 5.2 Compute Shaders

The LUTs are generated by compute shaders. Each thread computes one pixel.

```glsl
layout(local_size_x = 8, local_size_y = 8) in;
// ...
ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
ivec2 size  = imageSize(uTransmittanceLUT);
if (coord.x >= size.x || coord.y >= size.y) return;  // boundary guard
```

`gl_GlobalInvocationID.xy` is the pixel coordinate. The `local_size_x = 8, local_size_y = 8` means threads run in 8×8 blocks (64 threads per block). You dispatch `ceil(width/8) × ceil(height/8)` blocks.

**Why 8×8?** GPUs process threads in warps of 32-64. 8×8 = 64 fits exactly one warp, which is the most efficient tile size for 2D work.

**Why the boundary guard?** When image size is not divisible by 8, you dispatch slightly more threads than pixels. The guard prevents those extra threads from writing out of bounds.

**Writing to image vs sampling from texture:**
- `layout(rgba16f, binding = 0) writeonly uniform image2D` — write-only access, for output
- `uniform sampler2D` — read-only with filtering, for input
- You cannot use `texture()` on an image2D. You use `imageStore()` to write, `imageLoad()` to read (though in these shaders, the output is always image2D and input is always sampler2D).

### 5.3 imageStore vs texture

```glsl
// Writing (compute shader output):
imageStore(uTransmittanceLUT, coord, vec4(transmittance, 1.0));

// Reading (sampling with bilinear interpolation):
vec3 val = texture(uTransmittanceLUT, uv).rgb;
```

`imageStore` takes an `ivec2` pixel coordinate. `texture` takes a `vec2` UV in [0,1]. The `texture` call uses the GPU's built-in bilinear filtering hardware — it blends neighboring pixels smoothly. `imageStore`/`imageLoad` are exact pixel access with no filtering.

### 5.4 The glMemoryBarrier

```glsl
glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
```

After a compute shader writes to a texture, the GPU may not have flushed those writes to memory before the next shader reads them. This barrier forces all writes to be visible before continuing. Without it, the next shader might read stale data.

### 5.5 exp(-opticalDepth) as a vec3

In math, optical depth and transmittance are scalar for a single wavelength. In GLSL you use vec3 to handle R, G, B simultaneously:

```glsl
vec3 opticalDepth = vec3(0.0);
// ...
opticalDepth += extinctionFromDensity(dens) * dt;  // vec3 accumulation
vec3 transmittance = exp(-opticalDepth);            // exp() applied component-wise
```

`exp(vec3)` in GLSL applies `exp()` to each component independently. This is a GLSL feature (component-wise math on vectors) that has no direct equivalent in standard math notation. The math would write three separate equations for R, G, B; GLSL handles all three in one line.

### 5.6 The midpoint sampling `(i + 0.3)`

```glsl
float newT = ((i + 0.3)/numSteps) * tMax;
```

Instead of `+ 0.5` (true midpoint), the Shadertoy reference and your engine use `+ 0.3`. This shifts samples slightly toward the near end of each step. It is a minor accuracy tweak for the specific density profiles of Earth's atmosphere — slightly better results without costing any performance. You can change this to `0.5` and the difference is negligible.

### 5.7 Fibonacci Sphere Sampling (Multi-Scatter LUT)

```glsl
vec3 fibonacciSphere(int i, int n) {
    float theta = acos(1.0 - 2.0*(float(i)+0.5)/float(n));
    float phi   = TWO_PI * float(i) / GOLDEN_RATIO;
    return vec3(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi));
}
```

This generates n points uniformly distributed on a sphere. The golden ratio `φ = 1.618...` is used because it creates a spiral pattern that avoids clustering. Regular grid sampling on a sphere creates poles with dense points and equator with sparse points. Fibonacci spiral avoids this.

**Math:** `theta` is the polar angle (0 at north pole, π at south). `phi` is the azimuth (0 to 2π around the equator). The conversion to Cartesian is `(sin θ cos φ, cos θ, sin θ sin φ)` — standard spherical coordinates.

---

## 6. GLSL Specifics

### 6.1 Component-wise operations on vectors

Everything in GLSL that works on float also works on vec2, vec3, vec4 automatically:

```glsl
vec3 a = vec3(1.0, 2.0, 3.0);
vec3 b = exp(-a);      // = vec3(exp(-1), exp(-2), exp(-3))
vec3 c = a * b;        // component-wise multiply
vec3 d = a / b;        // component-wise divide
float e = dot(a, b);   // dot product = a.x*b.x + a.y*b.y + a.z*b.z
```

This is different from math where you would need explicit summation or matrix notation.

### 6.2 The fullscreen triangle trick

```glsl
vec2 pos;
if      (gl_VertexID == 0) { pos = vec2(-1.0, -1.0); }
else if (gl_VertexID == 1) { pos = vec2( 3.0, -1.0); }
else                        { pos = vec2(-1.0,  3.0); }
```

You draw 3 vertices with no VBO. `gl_VertexID` is the vertex index (0, 1, 2). The triangle is so large (-1 to 3) that it covers the entire screen when clipped. This is faster than a quad (4 vertices, 2 triangles, 6 index entries) and avoids the diagonal seam where two triangles meet.

### 6.3 Reconstructing the world-space ray direction

```glsl
vec4 ndcNear = vec4(pos, -1.0, 1.0);
vec4 wNear   = uInvViewProj * ndcNear;
wNear /= wNear.w;

vec4 ndcFar  = vec4(pos,  1.0, 1.0);
vec4 wFar    = uInvViewProj * ndcFar;
wFar  /= wFar.w;

vRayDir = normalize(wFar.xyz - wNear.xyz);
```

NDC (Normalized Device Coordinates) go from -1 to +1 in X and Y, and -1 to +1 in Z (near to far). The inverse view-projection matrix converts NDC back to world space. By taking two points on the same screen position at different depths (Z = -1 near, Z = 1 far) and subtracting, you get the ray direction in world space. The camera translation cancels out in the subtraction.

**Why divide by `.w`?** The perspective projection packs the divide into W. `wNear.w` is not 1 after the matrix multiply — you must divide through to get actual 3D world coordinates.

### 6.4 glDepthMask(GL_FALSE) for the sky

```cpp
glDepthMask(GL_FALSE);   // do not write to depth buffer
glDepthFunc(GL_LEQUAL);  // but do read from it
```

The sky draws at depth 0.9999. It reads the depth buffer to check if geometry is in front of it, but it does not write its own depth. This way, geometry rendered after the sky overwrites it correctly. If the sky wrote depth, nearby geometry rendered later would fail the depth test against the sky's 0.9999 and disappear.

---

## 7. How to Modify the Math

### 7.1 Change the planet

```glsl
float groundRadiusMM = 6.360;       // Earth radius
float atmosphereRadiusMM = 6.460;   // 100 km atmosphere
```

For Mars: `groundRadius = 3.390`, `atmosphereRadius = 3.440` (50 km atmosphere), reduce all scattering coefficients by 10× (thinner air).  
For a gas giant: set `atmosphereRadius` very large (6.360 + 5.0), and increase scattering coefficients dramatically.

### 7.2 Change sky color

The Rayleigh scattering ratio controls color. The ratio Blue:Red is what matters, not the absolute values. To make an alien orange sky:

```glsl
// Swap: make red scatter more than blue
vec3 rayleighScatteringBase = vec3(33.1, 13.558, 5.802) * 1e-3; // red > blue
```

To make a purple sky (sci-fi atmosphere), boost both red and blue, reduce green:
```glsl
vec3 rayleighScatteringBase = vec3(25.0, 8.0, 30.0) * 1e-3;
```

### 7.3 Change haze density

Mie scattering controls visible haze. Clear day vs foggy day:

```glsl
// Clear day (low haze):
float mieScatteringBase = 1.0e-3;   // ÷4 from default

// Heavy haze (Los Angeles smog):
float mieScatteringBase = 15.0e-3;  // ×4 from default
```

### 7.4 Change sun position

`uSunDir` is a normalized vec3. The Y component is the height above the horizon:
- Noon: `vec3(0, 1, 0)` — straight up
- Sunrise/sunset: `vec3(sin(azimuth), 0.05, -cos(azimuth))` — just above horizon
- Dusk: `vec3(sin(azimuth), -0.1, -cos(azimuth))` — just below horizon

### 7.5 Increase render quality

Transmittance LUT steps (currently 40):
- 20 steps: barely noticeable quality loss, 2× faster to compute
- 80 steps: noticeably smoother, useful for high-altitude scenes
- 40 is the sweet spot for real-time

Sky-View LUT resolution (currently 200×200):
- 128×128: faster, slightly more banding near horizon
- 256×256: smoother but recomputes every frame
- 200×200 is the sweet spot (recommended by the Shadertoy reference)

### 7.6 Change the tonemapping

The current chain:

```glsl
lum *= uExposure;                         // scale
lum  = pow(max(lum, 0.0), vec3(1.3));    // lift darks
lum /= smoothstep(0,0.2,sunDir.y)*2+0.15; // auto-expose
lum  = jodieReinhard(lum);                // tonemap
lum  = pow(lum, vec3(1.0/2.2));           // gamma
```

**Tune `uExposure`:** Lower → darker sky, higher → brighter sky. Default should be ~20.0 to match the Shadertoy reference.

**Remove the `pow(1.3)` curve:** Set it to `pow(lum, vec3(1.0))` (no-op) for a physically linear response. The sky will look darker at sunrise.

**Remove the auto-exposure divider:** Replace with a constant like `lum /= 1.0`. Sunsets will look much darker than noon, like real life (but less cinematic).

**Different tonemapper:** Replace `jodieReinhard` with ACES approximate:
```glsl
vec3 aces(vec3 x) {
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14), 0.0, 1.0);
}
```
ACES gives more contrast in the midtones and a characteristic "filmic" S-curve.

### 7.7 Add a second light source (moon)

The sky-view LUT only integrates one sun direction. To add a moon:
1. Add `uniform vec3 uMoonDir` and `uniform float uMoonIntensity`
2. In the Sky-View compute shader, run a second integration loop with the moon direction
3. Scale by `uMoonIntensity` (typically 0.0002× the sun)

The transmittance math is identical — just replace `uSunDir` with `uMoonDir` in the scattering integral.

---

## Quick Reference

```
transmittance = exp(-∫ extinction dt)         survival fraction of light
extinction = scattering + absorption          total loss per km
optical_depth = ∫ extinction dt               accumulated loss (dimensionless)
phase function = P(θ)                         probability distribution of scatter angle
L(view) = ∫ T_cam × σ × P × T_sun × L_sun dt final luminance equation
```

```
Rayleigh scatters blue >> red     → blue sky
Mie scatters all wavelengths      → white haze around sun
Ozone absorbs red at 25 km        → deep blue zenith
exp(-x) with x large → 0         → no light survives
exp(-x) with x small → 1         → light passes through
```

```
LUT: precompute expensive integrals → fast lookup at render time
Transmittance LUT: 256×64 → T(height, angle)
Multi-Scatter LUT: 32×32  → Ψ(height, sun_angle)
Sky-View LUT: 200×200     → L(longitude, latitude) per frame
```
