#include "burnint.h"
#include "st0016_snd.h"

struct st0016_voice {
	UINT8 regs[0x20];
	UINT32 start;
	UINT32 end;
	UINT32 lpstart;
	UINT32 lpend;
	UINT16 freq;
	INT32 vol_l;
	INT32 vol_r;
	UINT8 flags;
	UINT32 pos;
	UINT32 frac;
	INT32 lponce;
	INT32 out;
};

struct st0016_chip {
	st0016_voice voice[8];
	UINT8 *rom;
	INT32 rom_len;
};

static st0016_chip *ic = NULL;

void st0016snd_init(UINT8 *rom, INT32 rom_len)
{
	ic = (st0016_chip*)BurnMalloc(sizeof(st0016_chip));
	memset(ic, 0, sizeof(st0016_chip));
	ic->rom = rom;
	ic->rom_len = rom_len;
}

void st0016snd_exit()
{
	if (ic) {
		BurnFree(ic);
		ic = NULL;
	}
}

void st0016snd_reset()
{
	if (ic) {
		for (INT32 v = 0; v < 8; v++) {
			memset(&ic->voice[v], 0, sizeof(st0016_voice));
		}
	}
}

void st0016snd_write(INT32 offset, UINT8 data)
{
	if (!ic) return;
	
	if (offset < 0x100) {
		INT32 v = offset >> 5;
		INT32 reg = offset & 0x1f;
		st0016_voice *voice = &ic->voice[v];
		
		voice->regs[reg] = data;
		
		switch (reg) {
			case 0x00:
			case 0x01:
			case 0x02:
				voice->start = (voice->regs[0x02] << 16) | (voice->regs[0x01] << 8) | voice->regs[0x00];
				break;
			case 0x04:
			case 0x05:
			case 0x06:
				voice->lpstart = (voice->regs[0x06] << 16) | (voice->regs[0x05] << 8) | voice->regs[0x04];
				break;
			case 0x08:
			case 0x09:
			case 0x0a:
				voice->lpend = (voice->regs[0x0a] << 16) | (voice->regs[0x09] << 8) | voice->regs[0x08];
				break;
			case 0x0c:
			case 0x0d:
			case 0x0e:
				voice->end = (voice->regs[0x0e] << 16) | (voice->regs[0x0d] << 8) | voice->regs[0x0c];
				break;
			case 0x10:
			case 0x11:
				voice->freq = (voice->regs[0x11] << 8) | voice->regs[0x10];
				break;
			case 0x14:
				voice->vol_l = (INT8)data;
				break;
			case 0x15:
				voice->vol_r = (INT8)data;
				break;
			case 0x16:
				if (data != voice->flags) {
					if (data != 0) {
						voice->pos = voice->start;
						voice->frac = 0;
						voice->lponce = 0;
					}
				}
				voice->flags = data;
				break;
		}
	}
}

UINT8 st0016snd_read(INT32 offset)
{
	if (!ic) return 0;
	if (offset < 0x100) {
		return ic->voice[offset >> 5].regs[offset & 0x1f];
	}
	return 0;
}

void st0016snd_update(INT16 *buffer, INT32 samples)
{
	if (!ic) return;
	if (nBurnSoundRate == 0) return;
	
	INT32 native_rate = 8000000 / 128; // ST-0016 sound clock 8MHz/128 = 62500Hz (renju)
	UINT32 freq_step[8];
	
	for (INT32 v = 0; v < 8; v++) {
		freq_step[v] = (UINT32)(((float)ic->voice[v].freq * native_rate) / nBurnSoundRate);
	}
	
	for (INT32 sampleind = 0; sampleind < samples; sampleind++) {
		INT32 dataL = 0;
		INT32 dataR = 0;
		
		for (INT32 v = 0; v < 8; v++) {
			st0016_voice *voice = &ic->voice[v];
			
			if (voice->flags & 0x06) {
				INT32 pos_mask = voice->pos & 0x1fffff;
				
				if (pos_mask < ic->rom_len) {
					voice->out = ((INT16)(INT8)ic->rom[pos_mask]) << 8;
				} else {
					voice->out = 0;
				}
				
				voice->frac += freq_step[v];
				voice->pos += (voice->frac >> 16);
				voice->frac &= 0xffff;
				
				if (voice->lponce) {
					if (voice->pos >= voice->lpend) {
						voice->pos = voice->lpstart;
					}
				} else {
					if (voice->pos >= voice->end) {
						if (voice->flags & 1) { // loop
							voice->pos = voice->lpstart;
							voice->lponce = 1;
						} else {
							voice->flags = 0;
							voice->pos = 0;
							voice->frac = 0;
							voice->out = 0;
							continue;
						}
					}
				}
				
				// MAME mixes (out*vol)>>8 normalised by (32768<<4), i.e. an extra /16.
				dataL += (voice->out * voice->vol_l) >> 12;
				dataR += (voice->out * voice->vol_r) >> 12;
			}
		}
		
		INT32 nLeftSample = BURN_SND_CLIP(dataL);
		INT32 nRightSample = BURN_SND_CLIP(dataR);
		
		buffer[0] = BURN_SND_CLIP(buffer[0] + nLeftSample);
		buffer[1] = BURN_SND_CLIP(buffer[1] + nRightSample);
		buffer += 2;
	}
}

INT32 st0016snd_scan(INT32 nAction)
{
	if (!ic) return 0;
	
	struct BurnArea ba;
	char szName[32];
	
	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		sprintf(szName, "st0016 voices");
		ba.Data		= ic->voice;
		ba.nLen		= sizeof(ic->voice);
		ba.nAddress = 0;
		ba.szName	= szName;
		BurnAcb(&ba);
	}
	
	return 0;
}
