#include "port_android_rom.h"
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "TMC-ROM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static AAssetManager* sAssetMgr = NULL;
static char sFilesDir[512] = {0};
static char sRomPath[640] = {0};
static int sRomFound = 0;

void Port_Android_SetAssetManager(AAssetManager* mgr) {
    sAssetMgr = mgr;
}

void Port_Android_SetFilesDir(const char* path) {
    strncpy(sFilesDir, path, sizeof(sFilesDir) - 1);
}

static int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static int copy_asset_to_file(const char* asset_name, const char* out_path) {
    AAsset* asset = AAssetManager_open(sAssetMgr, asset_name, AASSET_MODE_BUFFER);
    if (!asset) return 0;

    off_t size = AAsset_getLength(asset);
    const void* data = AAsset_getBuffer(asset);
    if (!data) { AAsset_close(asset); return 0; }

    FILE* f = fopen(out_path, "wb");
    if (!f) { AAsset_close(asset); return 0; }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    AAsset_close(asset);

    return written == (size_t)size;
}

const char* Port_Android_FindRom(void) {
    if (sRomFound) return sRomPath;

    const char* candidates[] = {
        "baserom.gba", "baserom_eu.gba", "baserom_jp.gba",
        "tmc.gba", "tmc_eu.gba", "tmc_jp.gba",
        NULL
    };

    /* Check files dir first */
    for (int i = 0; candidates[i]; i++) {
        snprintf(sRomPath, sizeof(sRomPath), "%s/%s", sFilesDir, candidates[i]);
        if (file_exists(sRomPath)) {
            sRomFound = 1;
            LOGI("Found ROM: %s", sRomPath);
            return sRomPath;
        }
    }

    /* Try extracting from APK assets */
    for (int i = 0; candidates[i]; i++) {
        snprintf(sRomPath, sizeof(sRomPath), "%s/%s", sFilesDir, candidates[i]);
        if (copy_asset_to_file(candidates[i], sRomPath)) {
            sRomFound = 1;
            LOGI("Extracted ROM from assets: %s", sRomPath);
            return sRomPath;
        }
    }

    LOGE("No ROM found! Place baserom.gba in files dir or APK assets");
    sRomPath[0] = '\0';
    return NULL;
}
