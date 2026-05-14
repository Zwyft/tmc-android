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
#include <signal.h>
#include <unistd.h>

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

/* File-based debug log — readable via `cat /sdcard/Android/data/org.tmc/files/debug.log` */
static FILE* sDebugLog = NULL;
#define DBG(...) do { \
    __android_log_print(ANDROID_LOG_INFO, "TMC", __VA_ARGS__); \
    if (sDebugLog) { fprintf(sDebugLog, __VA_ARGS__); fprintf(sDebugLog, "\n"); fflush(sDebugLog); } \
} while(0)

static JavaVM* gJvm = NULL;
static ANativeWindow* gWindow = NULL;
static AAssetManager* gAssetMgr = NULL;
static char gFilesDir[512] = {0};
static char gRomPath[640] = {0};
static volatile int gWindowWidth = 0, gWindowHeight = 0;
static volatile int gRunning = 0;
static volatile int gPaused = 0;
static pthread_t gGameThread;
static sem_t gInitSem;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    gJvm = vm;
    return JNI_VERSION_1_6;
}

/* ── GameActivity JNI calls ───────────────────────────────────────── */

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeSetSurface(JNIEnv* env, jobject thiz, jobject surface) {
    gWindow = ANativeWindow_fromSurface(env, surface);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeSetAssetManager(JNIEnv* env, jobject thiz, jobject mgr) {
    gAssetMgr = AAssetManager_fromJava(env, mgr);
    Port_Android_SetAssetManager(gAssetMgr);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeSetFilesDir(JNIEnv* env, jobject thiz, jstring path) {
    const char* utf = (*env)->GetStringUTFChars(env, path, NULL);
    strncpy(gFilesDir, utf, sizeof(gFilesDir) - 1);
    (*env)->ReleaseStringUTFChars(env, path, utf);
    Port_Android_SetFilesDir(gFilesDir);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeSetApkPath(JNIEnv* env, jobject thiz, jstring path) {
    const char* utf = (*env)->GetStringUTFChars(env, path, NULL);
    strncpy(gFilesDir + 256, utf, sizeof(gFilesDir) - 257);
    (*env)->ReleaseStringUTFChars(env, path, utf);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeSetRomPath(JNIEnv* env, jobject thiz, jstring path) {
    const char* utf = (*env)->GetStringUTFChars(env, path, NULL);
    strncpy(gRomPath, utf, sizeof(gRomPath) - 1);
    (*env)->ReleaseStringUTFChars(env, path, utf);
    LOGI("ROM path set: %s", gRomPath);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeOnResize(JNIEnv* env, jobject thiz, jint w, jint h) {
    gWindowWidth = w;
    gWindowHeight = h;
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeOnSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    ANativeWindow_release(gWindow);
    gWindow = NULL;
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeOnPause(JNIEnv* env, jobject thiz) {
    gPaused = 1;
    Port_Audio_Pause();
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeOnResume(JNIEnv* env, jobject thiz) {
    gPaused = 0;
    Port_Audio_Resume();
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeTouchDown(JNIEnv* env, jobject thiz, jint id, jint x, jint y) {
    Port_Android_TouchDown(id, x, y);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeTouchMove(JNIEnv* env, jobject thiz, jint id, jint x, jint y) {
    Port_Android_TouchMove(id, x, y);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeTouchUp(JNIEnv* env, jobject thiz, jint id, jint x, jint y) {
    Port_Android_TouchUp(id, x, y);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeKeyDown(JNIEnv* env, jobject thiz, jint keyCode) {
    Port_Android_KeyDown((int)keyCode);
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeKeyUp(JNIEnv* env, jobject thiz, jint keyCode) {
    Port_Android_KeyUp((int)keyCode);
}

/* Force CMake rebuild on next compile */
/* ── Signal handler for native crash logging ─────────────────────── */

static void crash_handler(int sig) {
    switch (sig) {
        case SIGSEGV: LOGI(">>> CRASH: SIGSEGV (null pointer or invalid memory access)"); break;
        case SIGABRT: LOGI(">>> CRASH: SIGABRT (assertion failed or abort())"); break;
        case SIGFPE:  LOGI(">>> CRASH: SIGFPE (divide by zero)"); break;
        case SIGILL:  LOGI(">>> CRASH: SIGILL (illegal instruction)"); break;
        default:      LOGI(">>> CRASH: signal %d", sig); break;
    }
    _exit(1);
}

/* ── Android-compatible fatal error ───────────────────────────────── */

void Android_FatalError(const char* title, const char* msg) {
    LOGE("FATAL: %s - %s", title, msg);
    /* Can't show a dialog here (no Java env in this thread), so just log it */
}

/* ── Game thread ──────────────────────────────────────────────────── */

static void* game_thread_func(void* arg) {
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGILL, crash_handler);

    JNIEnv* env;
    (*gJvm)->AttachCurrentThread(gJvm, &env, NULL);

    /* Open debug log file */
    {
        char log_path[640];
        snprintf(log_path, sizeof(log_path), "%s/debug.log", gFilesDir);
        sDebugLog = fopen(log_path, "w");
        if (sDebugLog) DBG("[init] debug.log opened");
    }

    DBG("[init] Game thread started");
    DBG("[init] filesDir: %s", gFilesDir);
    DBG("[init] romPath: %s", gRomPath);

    *(u16*)(gIoMem + REG_OFFSET_KEYINPUT) = 0x03FF;

    char config_path[640];
    snprintf(config_path, sizeof(config_path), "%s/config.json", gFilesDir);
    Port_Config_Load(config_path);
    DBG("[init] config loaded");

    DBG("[init] init renderer...");
    if (!Port_Android_InitRenderer(gWindow)) {
        DBG("[init] FAILED renderer");
        sem_post(&gInitSem);
        return NULL;
    }
    DBG("[init] renderer OK");

    DBG("[init] init audio...");
    if (!Port_Android_InitAudio()) {
        DBG("[init] FAILED audio, muting");
        gMain.muteAudio = 1;
    }
    DBG("[init] audio OK");

    const char* romPath = gRomPath[0] != '\0' ? gRomPath : Port_Android_FindRom();
    DBG("[init] romPath resolved: '%s'", romPath ? romPath : "NULL");
    if (!romPath || romPath[0] == '\0') {
        DBG("[init] FATAL: no ROM path");
        sem_post(&gInitSem);
        return NULL;
    }

    DBG("[init] verifying ROM file...");
    {
        FILE* test = fopen(romPath, "rb");
        if (!test) {
            DBG("[init] FATAL: cannot open ROM: %s", romPath);
            sem_post(&gInitSem);
            return NULL;
        }
        fseek(test, 0, SEEK_END);
        long sz = ftell(test);
        fclose(test);
        DBG("[init] ROM OK: %ld bytes", sz);
    }

    DBG("[init] calling Port_LoadRom...");
    Port_LoadRom(romPath);
    DBG("[init] Port_LoadRom returned");

    DBG("[init] calling Port_PPU_Init...");
    Port_PPU_Init(NULL);
    DBG("[init] Port_PPU_Init returned");

    sem_post(&gInitSem);

    DBG("[init] entering AgbMain...");
    gRunning = 1;
    AgbMain();
    DBG("[init] AgbMain returned (unexpected)");

    Port_Audio_Shutdown();
    Port_Android_ShutdownRenderer();
    DBG("[init] Game thread exiting");
    if (sDebugLog) fclose(sDebugLog);

    (*gJvm)->DetachCurrentThread(gJvm);
    return NULL;
}

JNIEXPORT void JNICALL Java_org_tmc_GameActivity_nativeMain(JNIEnv* env, jobject thiz) {
    sem_init(&gInitSem, 0, 0);
    pthread_create(&gGameThread, NULL, game_thread_func, NULL);
    sem_wait(&gInitSem);
    sem_destroy(&gInitSem);
}
