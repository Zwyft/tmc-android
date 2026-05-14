#pragma once

#include <stdint.h>

void Port_Android_TouchDown(int id, int x, int y);
void Port_Android_TouchMove(int id, int x, int y);
void Port_Android_TouchUp(int id, int x, int y);
void Port_Android_KeyDown(int keyCode);
void Port_Android_KeyUp(int keyCode);
uint16_t Port_Android_GetKeyInput(void);
void Port_Android_SetScreenSize(int w, int h);
