#ifndef __ANIMATION_H__
#define __ANIMATION_H__

#include "common.h"

#define STEP_COUNT 20

void *on_idle(void);

void *on_listen(void);

void *on_speak(void);

void *on_mute(void);

void *on_unmute(void);

void *on_disabled(void);

#endif
