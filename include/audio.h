#ifndef AUDIO_H
#define AUDIO_H

typedef enum SoundEffect {
	SFX_MOVE_UP,
	SFX_MOVE_DOWN,
	SFX_DROP,
	SFX_CLEAR,
	SFX_SHOT,
	SFX_EXPLOSION
} SoundEffect;

void soundInit(void);
void soundFrame(void);

void playSoundEffect(SoundEffect effect);

#endif