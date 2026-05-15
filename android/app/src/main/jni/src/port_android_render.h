#pragma once

#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

int Port_Android_InitRenderer(ANativeWindow* window);
void Port_Android_ShutdownRenderer(void);
void Port_Android_SwapBuffers(void);
void Port_Android_GetWindowSize(int* w, int* h);
