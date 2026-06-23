#ifndef ST0016_SND_H
#define ST0016_SND_H

#include "burnint.h"

#ifdef __cplusplus
extern "C" {
#endif

void st0016snd_init(UINT8 *rom, INT32 rom_len);
void st0016snd_exit();
void st0016snd_reset();
void st0016snd_write(INT32 offset, UINT8 data);
UINT8 st0016snd_read(INT32 offset);
void st0016snd_update(INT16 *buffer, INT32 samples);
INT32 st0016snd_scan(INT32 nAction);

#ifdef __cplusplus
}
#endif

#endif // ST0016_SND_H
