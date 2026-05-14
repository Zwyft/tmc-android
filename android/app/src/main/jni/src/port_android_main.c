#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include "gba/io_reg.h"
#include "main.h"
#include "port_config.h"
#include "port_android_render.h"
#include "port_android_audio.h"
#include "port_android_input.h"
#include "port_rom.h"
#include "port_android_rom.h"
#include "port_ppu.h"

#define LOG_TAG "TMC"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static JavaVM* gJvm = NULL;
static ANativeWindow* gWindow = NULL;
static AAssetManager* gAssetMgr = NULL;
static char gFilesDir[512] = {0};
static char gApkPath[1024] = {0};
static volatile int gWindowWidth = 0, gWindowHeight = 0;
static volatile int gRunning = 0;
static volatile int gPaused = 0;
static pthread_t gGameThread;
static sem_t gInitSem;

extern void Port_Config_Load(const char* path);

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    gJvm = vm;
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeSetSurface(JNIEnv* env, jobject thiz, jobject surface) {
    gWindow = ANativeWindow_fromSurface(env, surface);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeSetAssetManager(JNIEnv* env, jobject thiz, jobject mgr) {
    gAssetMgr = AAssetManager_fromJava(env, mgr);
    Port_Android_SetAssetManager(gAssetMgr);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeSetFilesDir(JNIEnv* env, jobject thiz, jstring path) {
    const char* utf = (*env)->GetStringUTFChars(env, path, NULL);
    strncpy(gFilesDir, utf, sizeof(gFilesDir) - 1);
    (*env)->ReleaseStringUTFChars(env, path, utf);
    Port_Android_SetFilesDir(gFilesDir);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeSetApkPath(JNIEnv* env, jobject thiz, jstring path) {
    const char* utf = (*env)->GetStringUTFChars(env, path, NULL);
    strncpy(gApkPath, utf, sizeof(gApkPath) - 1);
    (*env)->ReleaseStringUTFChars(env, path, utf);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeOnResize(JNIEnv* env, jobject thiz, jint w, jint h) {
    gWindowWidth = w;
    gWindowHeight = h;
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeOnSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    ANativeWindow_release(gWindow);
    gWindow = NULL;
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeOnPause(JNIEnv* env, jobject thiz) {
    gPaused = 1;
    Port_Audio_Pause();
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeOnResume(JNIEnv* env, jobject thiz) {
    gPaused = 0;
    Port_Audio_Resume();
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeTouchDown(JNIEnv* env, jobject thiz, jint id, jint x, jint y) {
    Port_Android_TouchDown(id, x, y);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeTouchMove(JNIEnv* env, jobject thiz, jint id, jint x, jint y) {
    Port_Android_TouchMove(id, x, y);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeTouchUp(JNIEnv* env, jobject thiz, jint id, jint x, jint y) {
    Port_Android_TouchUp(id, x, y);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeKeyDown(JNIEnv* env, jobject thiz, jint keyCode) {
    Port_Android_KeyDown((int)keyCode);
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeKeyUp(JNIEnv* env, jobject thiz, jint keyCode) {
    Port_Android_KeyUp((int)keyCode);
}

static void* game_thread_func(void* arg) {
    JNIEnv* env;
    (*gJvm)->AttachCurrentThread(gJvm, &env, NULL);

    LOGI("Game thread started");

    *(u16*)(gIoMem + REG_OFFSET_KEYINPUT) = 0x03FF;

    char config_path[640];
    snprintf(config_path, sizeof(config_path), "%s/config.json", gFilesDir);
    Port_Config_Load(config_path);

    if (!Port_Android_InitRenderer(gWindow)) {
        LOGE("Failed to init renderer");
        sem_post(&gInitSem);
        return NULL;
    }

    if (!Port_Android_InitAudio()) {
        LOGE("Failed to init audio, continuing muted");
        gMain.muteAudio = 1;
    }

    const char* romPath = Port_Android_FindRom();
    if (!romPath) {
        LOGE("No baserom.gba found! Place it in %s/", gFilesDir);
        sem_post(&gInitSem);
        return NULL;
    }

    Port_LoadRom(romPath);
    Port_PPU_Init(NULL);

    sem_post(&gInitSem);
    LOGI("Port initialized, entering AgbMain");

    gRunning = 1;
    AgbMain();

    Port_Audio_Shutdown();
    Port_Android_ShutdownRenderer();
    LOGI("Game thread exiting");

    (*gJvm)->DetachCurrentThread(gJvm);
    return NULL;
}

JNIEXPORT void JNICALL Java_org_tmc_TMCActivity_nativeMain(JNIEnv* env, jobject thiz) {
    sem_init(&gInitSem, 0, 0);
    pthread_create(&gGameThread, NULL, game_thread_func, NULL);
    sem_wait(&gInitSem);
    sem_destroy(&gInitSem);
}

extern void AgbMain(void);
extern void Port_PPU_Init(SDL_Window* window);
extern void Port_Audio_Shutdown(void);
extern void Port_Audio_Pause(void);
extern void Port_Audio_Resume(void);
