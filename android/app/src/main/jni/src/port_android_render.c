#include "port_android_render.h"
#include <android/log.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "TMC-Render"
#define DBG(...) do { \
    FILE* _f = fopen("/storage/emulated/0/Android/data/org.tmc/files/debug.log", "a"); \
    if (_f) { fprintf(_f, "[GL] " __VA_ARGS__); fprintf(_f, "\n"); fclose(_f); } \
} while(0)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static EGLDisplay sEglDisplay = EGL_NO_DISPLAY;
static EGLContext sEglContext = EGL_NO_CONTEXT;
static EGLSurface sEglSurface = EGL_NO_SURFACE;
static int sWinW = 240, sWinH = 160;

static const char* kVertexShader =
    "#version 300 es\n"
    "in vec2 aPos;\n"
    "in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

static const char* kFragmentShader =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 vTexCoord;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uTexture;\n"
    "void main() {\n"
    "    fragColor = texture(uTexture, vTexCoord);\n"
    "}\n";

static GLuint sProgram = 0;
static GLuint sVAO = 0;
static GLuint sVBO = 0;
static GLuint sTexture = 0;
static GLint sUniformTex = -1;

extern uint32_t* virtuappu_frame_buffer;

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGE("Shader compile error: %s", log);
        return 0;
    }
    return shader;
}

int Port_Android_InitRenderer(ANativeWindow* window) {
    sEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (sEglDisplay == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return 0;
    }

    EGLint major, minor;
    if (!eglInitialize(sEglDisplay, &major, &minor)) {
        LOGE("eglInitialize failed");
        return 0;
    }
    LOGI("EGL version %d.%d", major, minor);

    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(sEglDisplay, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed");
        return 0;
    }

    sEglSurface = eglCreateWindowSurface(sEglDisplay, config, window, NULL);
    if (sEglSurface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        return 0;
    }

    EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    sEglContext = eglCreateContext(sEglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    if (sEglContext == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed");
        return 0;
    }

    if (!eglMakeCurrent(sEglDisplay, sEglSurface, sEglSurface, sEglContext)) {
        LOGE("eglMakeCurrent failed");
        return 0;
    }

    eglQuerySurface(sEglDisplay, sEglSurface, EGL_WIDTH, &sWinW);
    eglQuerySurface(sEglDisplay, sEglSurface, EGL_HEIGHT, &sWinH);
    LOGI("Window: %dx%d", sWinW, sWinH);

    glViewport(0, 0, sWinW, sWinH);
    glClearColor(0, 0, 0, 1);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) return 0;

    sProgram = glCreateProgram();
    glAttachShader(sProgram, vs);
    glAttachShader(sProgram, fs);
    glLinkProgram(sProgram);
    GLint ok = 0;
    glGetProgramiv(sProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(sProgram, sizeof(log), NULL, log);
        LOGE("Program link error: %s", log);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    float verts[] = {
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
    };

    glGenVertexArrays(1, &sVAO);
    glGenBuffers(1, &sVBO);
    glBindVertexArray(sVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &sTexture);
    glBindTexture(GL_TEXTURE_2D, sTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    sUniformTex = glGetUniformLocation(sProgram, "uTexture");
    glUseProgram(sProgram);
    glUniform1i(sUniformTex, 0);

    return 1;
}

void Port_Android_ShutdownRenderer(void) {
    if (sTexture) glDeleteTextures(1, &sTexture);
    if (sVBO) glDeleteBuffers(1, &sVBO);
    if (sVAO) glDeleteVertexArrays(1, &sVAO);
    if (sProgram) glDeleteProgram(sProgram);
    if (sEglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(sEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (sEglContext != EGL_NO_CONTEXT) eglDestroyContext(sEglDisplay, sEglContext);
        if (sEglSurface != EGL_NO_SURFACE) eglDestroySurface(sEglDisplay, sEglSurface);
        eglTerminate(sEglDisplay);
    }
    sEglDisplay = EGL_NO_DISPLAY;
    sEglContext = EGL_NO_CONTEXT;
    sEglSurface = EGL_NO_SURFACE;
}

void Port_Android_SwapBuffers(void) {
    eglSwapBuffers(sEglDisplay, sEglSurface);
}

void Port_Android_GetWindowSize(int* w, int* h) {
    if (w) *w = sWinW;
    if (h) *h = sWinH;
}

/* Called from port_ppu.c: present the rendered frame */
void Port_PresentFrameGL(void) {
    DBG("glClear...");
    glClear(GL_COLOR_BUFFER_BIT);
    DBG("glBindTexture...");
    glBindTexture(GL_TEXTURE_2D, sTexture);
    DBG("glTexImage2D...");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 virtuappu_frame_buffer);
    DBG("glBindVertexArray...");
    glBindVertexArray(sVAO);
    DBG("glDrawArrays...");
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    DBG("SwapBuffers...");
    Port_Android_SwapBuffers();
    DBG("PresentFrameGL done");
}
