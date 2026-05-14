#include "port_android_audio.h"
#include "port_m4a_backend.h"
#include "main.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "TMC-Audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BYTES_PER_FRAME (sizeof(int16_t) * CHANNELS)
#define BUFFER_FRAMES 1024
#define BUFFER_COUNT 4

static SLObjectItf sEngineObj = NULL;
static SLEngineItf sEngine = NULL;
static SLObjectItf sOutputMixObj = NULL;
static SLObjectItf sPlayerObj = NULL;
static SLBufferQueueItf sBufferQueue = NULL;
static SLPlayItf sPlay = NULL;

static int16_t sBuffers[BUFFER_COUNT][BUFFER_FRAMES * CHANNELS];
static int sCurrentBuf = 0;
static volatile int sAudioRunning = 0;

extern Main gMain;

extern bool Port_M4A_Backend_Init(uint32_t sampleRate);
extern void Port_M4A_Backend_Render(int16_t* buffer, uint32_t numFrames, bool mute);
extern void Port_M4A_Backend_Shutdown(void);

static void buffer_queue_callback(SLBufferQueueItf caller, void* context) {
    (void)context;
    if (!sAudioRunning) return;

    int16_t* buf = sBuffers[sCurrentBuf];
    sCurrentBuf = (sCurrentBuf + 1) % BUFFER_COUNT;

    Port_M4A_Backend_Render(buf, BUFFER_FRAMES, gMain.muteAudio);
    static int prevL = 0, prevR = 0;
    for (int i = 0; i < BUFFER_FRAMES; i++) {
        int l = buf[i * 2];
        int r = buf[i * 2 + 1];
        l = l - prevL + (prevL >> 2);
        r = r - prevR + (prevR >> 2);
        if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
        if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
        buf[i * 2] = (int16_t)l;
        buf[i * 2 + 1] = (int16_t)r;
        prevL = l; prevR = r;
    }

    SLresult res = (*caller)->Enqueue(caller, buf, BUFFER_FRAMES * BYTES_PER_FRAME);
    if (res != SL_RESULT_SUCCESS) {
        LOGE("BufferQueue Enqueue failed: %d", res);
    }
}

int Port_Android_InitAudio(void) {
    SLresult res;

    res = slCreateEngine(&sEngineObj, 0, NULL, 0, NULL, NULL);
    if (res != SL_RESULT_SUCCESS) { LOGE("slCreateEngine failed"); return 0; }

    res = (*sEngineObj)->Realize(sEngineObj, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) { LOGE("Engine Realize failed"); return 0; }

    res = (*sEngineObj)->GetInterface(sEngineObj, SL_IID_ENGINE, &sEngine);
    if (res != SL_RESULT_SUCCESS) { LOGE("GetInterface ENGINE failed"); return 0; }

    const SLInterfaceID mixIids[] = { SL_IID_VOLUME };
    const SLboolean mixReq[] = { SL_BOOLEAN_FALSE };
    res = (*sEngine)->CreateOutputMix(sEngine, &sOutputMixObj, 1, mixIids, mixReq);
    if (res != SL_RESULT_SUCCESS) { LOGE("CreateOutputMix failed"); return 0; }

    res = (*sOutputMixObj)->Realize(sOutputMixObj, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) { LOGE("OutputMix Realize failed"); return 0; }

    if (!Port_M4A_Backend_Init(SAMPLE_RATE)) {
        LOGE("M4A backend init failed");
        return 0;
    }

    SLDataLocator_AndroidSimpleBufferQueue locBuf = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, BUFFER_COUNT
    };
    SLDataFormat_PCM fmtPcm = {
        SL_DATAFORMAT_PCM, CHANNELS, SAMPLE_RATE * 1000,
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource audioSrc = { &locBuf, &fmtPcm };

    SLDataLocator_OutputMix locOut = { SL_DATALOCATOR_OUTPUTMIX, sOutputMixObj };
    SLDataSink audioSnk = { &locOut, NULL };

    const SLInterfaceID ids[] = { SL_IID_BUFFERQUEUE };
    const SLboolean req[] = { SL_BOOLEAN_TRUE };
    res = (*sEngine)->CreateAudioPlayer(sEngine, &sPlayerObj, &audioSrc, &audioSnk, 1, ids, req);
    if (res != SL_RESULT_SUCCESS) { LOGE("CreateAudioPlayer failed: %d", res); return 0; }

    res = (*sPlayerObj)->Realize(sPlayerObj, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) { LOGE("Player Realize failed"); return 0; }

    res = (*sPlayerObj)->GetInterface(sPlayerObj, SL_IID_PLAY, &sPlay);
    if (res != SL_RESULT_SUCCESS) { LOGE("GetInterface PLAY failed"); return 0; }

    res = (*sPlayerObj)->GetInterface(sPlayerObj, SL_IID_BUFFERQUEUE, &sBufferQueue);
    if (res != SL_RESULT_SUCCESS) { LOGE("GetInterface BUFFERQUEUE failed"); return 0; }

    res = (*sBufferQueue)->RegisterCallback(sBufferQueue, buffer_queue_callback, NULL);
    if (res != SL_RESULT_SUCCESS) { LOGE("RegisterCallback failed"); return 0; }

    sAudioRunning = 1;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        memset(sBuffers[i], 0, BUFFER_FRAMES * BYTES_PER_FRAME);
        (*sBufferQueue)->Enqueue(sBufferQueue, sBuffers[i], BUFFER_FRAMES * BYTES_PER_FRAME);
    }

    res = (*sPlay)->SetPlayState(sPlay, SL_PLAYSTATE_PLAYING);
    if (res != SL_RESULT_SUCCESS) { LOGE("SetPlayState PLAYING failed"); return 0; }

    LOGI("Audio initialized: %d Hz %d ch", SAMPLE_RATE, CHANNELS);
    return 1;
}

void Port_Audio_Pause(void) {
    if (sPlay) (*sPlay)->SetPlayState(sPlay, SL_PLAYSTATE_PAUSED);
}

void Port_Audio_Resume(void) {
    if (sPlay) (*sPlay)->SetPlayState(sPlay, SL_PLAYSTATE_PLAYING);
}

void Port_Audio_Shutdown(void) {
    sAudioRunning = 0;
    if (sPlayerObj) { (*sPlayerObj)->Destroy(sPlayerObj); sPlayerObj = NULL; }
    if (sOutputMixObj) { (*sOutputMixObj)->Destroy(sOutputMixObj); sOutputMixObj = NULL; }
    if (sEngineObj) { (*sEngineObj)->Destroy(sEngineObj); sEngineObj = NULL; }
    Port_M4A_Backend_Shutdown();
}

void Port_Audio_Reset(void) {
    Port_M4A_Backend_Reset();
}

void Port_Audio_OnFifoWrite(uint32_t addr, uint32_t value) {
    (void)addr; (void)value;
}
