#ifndef _NIYATI_PLATFORM_H_
#define _NIYATI_PLATFORM_H_

// Platform abstraction layer
// These functions are implemented per-platform in each platform's main file.
// Engine/user code should call these instead of GLFW/X11 APIs directly.

// Returns elapsed time since application start, in seconds.
float platformGetTime(void);

// Returns the current framebuffer size in pixels.
void platformGetFramebufferSize(int* width, int* height);

// Sets the swap interval (0 = uncapped, 1 = vsync)
void platformSetSwapInterval(int interval);

// ============================================================================
// DELTA TIME
// g_DeltaTime  — seconds since last frame (clamped to 0.25s to guard against spikes)
// g_Time       — accumulated time since start (affected by g_UseDeltaTime logic)
// g_UseDeltaTime — when false, g_DeltaTime is always 1/60 (fixed-step feel)
// Handled automatically by display() in platforms/engine_loop.cpp.
// ============================================================================
extern float g_DeltaTime;
extern float g_Time;
extern bool  g_UseDeltaTime;

#endif // _NIYATI_PLATFORM_H_
