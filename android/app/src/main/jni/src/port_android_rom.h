#pragma once

#include <android/asset_manager.h>

void Port_Android_SetAssetManager(AAssetManager* mgr);
void Port_Android_SetFilesDir(const char* path);
const char* Port_Android_FindRom(void);
