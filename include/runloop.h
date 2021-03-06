#ifndef RUNLOOP_H
#define RUNLOOP_H

#include <gba_types.h>

typedef struct Runloop {
	void (*init)(u32 framecount);
	void (*deinit)(void);
	void (*frame)(u32 framecount);
} Runloop;

void setRunloop(Runloop* runloop);
void incrementRunloop(void);

#endif