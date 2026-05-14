#pragma once

#include <stdint.h>

int Port_Android_InitAudio(void);
void Port_Android_AudioPause(void);
void Port_Android_AudioResume(void);
void Port_Audio_Pause(void);
void Port_Audio_Resume(void);
void Port_Audio_Shutdown(void);
