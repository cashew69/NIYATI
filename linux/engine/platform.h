#ifndef _NIYATI_PLATFORM_H_
#define _NIYATI_PLATFORM_H_

// Platform abstraction layer
// These functions are implemented per-platform in each platform's main file.
// Engine/user code should call these instead of GLFW/X11 APIs directly.

// Returns elapsed time since application start, in seconds.
float platformGetTime(void);

// Returns the current framebuffer size in pixels.
void platformGetFramebufferSize(int* width, int* height);

#endif // _NIYATI_PLATFORM_H_
