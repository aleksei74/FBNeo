// copyright-holders:Aaron Giles
/*********************************************************

    Konami 054539 (TOP) PCM Sound Chip

    A lot of information comes from Amuse.
    Big thanks to them.

*********************************************************/

// Dec 3, 2017: anti-pop/anti-click code added: rampdown, dpcm table symmetricification, check last sample of 4bit dpcm for huge dc offset)
// Jan 19, 2018: added cubic resampling. -dink
// Aug 21-24, 2020: added digital delay/echo effect. -dink

#if defined (_MSC_VER)
#define _USE_MATH_DEFINES
#endif

#include "burnint.h"
#include <math.h>
#include "k054539.h"
#include "biquad.h"
#include "dtimer.h"

static INT32 nNumChips = 0;

typedef struct _k054539_interface k054539_interface;
struct _k054539_interface
{
	const char *rgnoverride;
	void (*apan)(double, double);
	void (*irq)(int);
};

struct k054539_channel {
	UINT32 pos;
	UINT32 pfrac;
	INT32 val;
	INT32 pval;
	double lvol;
	double rvol;
	INT32 delay_on;
};

struct k054539_info {
	k054539_interface intf;
	double voltab[256];
	double pantab[0xf];

	double k054539_gain[8];
	UINT8 k054539_posreg_latch[8][3];
	INT32 k054539_flags;

	UINT8 regs[0x230];

	// delay (echo)
	UINT8 *delay_ram;
	UINT32 delay_pos;
	UINT32 delay_size;
	double delay_decay;
	INT32 timer_state;

	INT32 cur_ptr;
	INT32 cur_limit;
	UINT8 *cur_zone;
	UINT8 *rom;
	UINT32 rom_size;
	UINT32 rom_mask;

	INT32 clock;

	dtimer irqtimer;

	double volume[2];
	INT32 output_dir[2];
	BIQ biquad[2];

	k054539_channel channels[8];
};

static k054539_info Chips[2];
static k054539_info *info;

#define DELAYRAM_SIZE	(0x4000 * 2)

// for resampling
static INT16 *soundbuf[2] = { NULL, NULL };
static UINT32 nSampleSize[2];
static INT32 nFractionalPosition[2];
static INT32 nPosition[2];

static inline UINT8 k054539_read_rom(k054539_info *chip, INT32 address)
{
	if (address < 0 || (UINT32)address >= chip->rom_size) {
		return 0;
	}

	return chip->rom[address];
}

static void timer_callback(INT32 chip)
{
	if (Chips[chip].regs[0x22f] & 0x20) {
		Chips[chip].timer_state ^= 1;
		if (Chips[chip].intf.irq) {
			Chips[chip].intf.irq(Chips[chip].timer_state);
		}
	}
}

void K054539SetIRQCallback(INT32 chip, void (*irq_cb)(int))
{
	info = &Chips[chip];
	info->intf.irq = irq_cb;

#if defined FBNEO_DEBUG
	if (chip != 0) {
		bprintf(0, _T("K054539SetIRQCallback(): timer only enabled for chip #0!\n"));
	}
#endif
}

void K054539SetFlags(INT32 chip, INT32 flags)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539SetFlags called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("K054539SetFlags called with invalid chip %x\n"), chip);
#endif

	info = &Chips[chip];
	info->k054539_flags = flags;
}

void K054539_set_gain(INT32 chip, INT32 channel, double gain)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539_set_gain called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("K054539_set_gain called with invalid chip %x\n"), chip);
#endif

	info = &Chips[chip];
	if (gain >= 0) info->k054539_gain[channel] = gain;
}

static INT32 k054539_regupdate()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539_regupdate called without init\n"));
#endif

	return !(info->regs[0x22f] & 0x80);
}

static void k054539_keyon(INT32 channel)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539_keyon called without init\n"));
#endif

	if(k054539_regupdate())
		info->regs[0x22c] |= 1 << channel;
}

static void k054539_keyoff(INT32 channel)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539_keyoff called without init\n"));
#endif

	if(k054539_regupdate())
		info->regs[0x22c] &= ~(1 << channel);
}

void K054539Write(INT32 chip, INT32 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539Write called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("K054539Write called with invalid chip %x\n"), chip);
#endif

	info = &Chips[chip];

	INT32 latch, offs, ch, pan;
	UINT8 *regbase, *regptr, *posptr;

	regbase = info->regs;
	latch = (info->k054539_flags & K054539_UPDATE_AT_KEYON) && (regbase[0x22f] & 1);

	if (latch && offset < 0x100)
	{
		offs = (offset & 0x1f) - 0xc;
		ch = offset >> 5;

		if (offs >= 0 && offs <= 2)
		{
			// latch writes to the position index registers
			info->k054539_posreg_latch[ch][offs] = data;
			return;
		}
	}

	else switch(offset)
	{
		case 0x13f:
			pan = data >= 0x11 && data <= 0x1f ? data - 0x11 : 0x18 - 0x11;
			if(info->intf.apan)
				info->intf.apan(info->pantab[pan], info->pantab[0xe - pan]);
		break;

		case 0x214:
			if (latch)
			{
				for(ch=0; ch<8; ch++)
				{
					if(data & (1<<ch))
					{
						posptr = &info->k054539_posreg_latch[ch][0];
						regptr = regbase + (ch<<5) + 0xc;

						// update the chip at key-on
						regptr[0] = posptr[0];
						regptr[1] = posptr[1];
						regptr[2] = posptr[2];

						k054539_keyon(ch);
					}
				}
			}
			else
			{
				for(ch=0; ch<8; ch++)
					if(data & (1<<ch))
						k054539_keyon(ch);
			}
		break;

		case 0x215:
			for(ch=0; ch<8; ch++)
				if(data & (1<<ch))
					k054539_keyoff(ch);
		break;

		case 0x227:
		{
			if (chip == 0) {
				INT32 cycles = (INT32)((1 / ((38 + data) * (info->clock/384.0/14400.0))) * 18432000 / 2);
				info->irqtimer.start(cycles, -1, 1, 1);
				info->timer_state = 0;
				if (info->intf.irq) info->intf.irq(info->timer_state);
			}
		}
		break;

		case 0x22d:
			if(regbase[0x22e] == 0x80) {
				INT32 addr = (info->cur_ptr & 0x3fff) | ((info->cur_ptr & 0x10000) >> 2);
				info->delay_ram[addr] = data;
			}
			info->cur_ptr = (info->cur_ptr + 1) & 0x1ffff;
		break;

		case 0x22e:
			info->cur_zone =
				data == 0x80 ? info->delay_ram :
				info->rom + 0x20000*data;
			info->cur_limit = data == 0x80 ? 0x4000 : 0x20000;
			info->cur_ptr = 0;
		break;

		case 0x22f:
			if (!(data & 0x20)) {
				info->timer_state = 0;
				if (info->intf.irq) info->intf.irq(info->timer_state);
			}
		break;

		default:
		break;
	}

	regbase[offset] = data;
}

UINT8 K054539Read(INT32 chip, INT32 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539Read called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("K054539Read called with invalid chip %x\n"), chip);
#endif

	info = &Chips[chip];

	switch(offset) {
	case 0x22d:
		if(info->regs[0x22f] & 0x10) {
			UINT8 res;
			if (info->regs[0x22e] == 0x80) {
				INT32 addr = (info->cur_ptr & 0x3fff) | ((info->cur_ptr & 0x10000) >> 2);
				res = info->delay_ram[addr];
			} else {
				res = k054539_read_rom(info, (0x20000 * info->regs[0x22e]) + info->cur_ptr);
			}
			info->cur_ptr = (info->cur_ptr + 1) & 0x1ffff;
			return res;
		} else
			return 0;

	case 0x22c:
		break;

	default:
		break;
	}

	return info->regs[offset];
}

void K054539Reset(INT32 chip)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539Reset called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("K054539Reset called with invalid chip %x\n"), chip);
#endif

	info = &Chips[chip];

	info->delay_pos = 0;
	info->delay_size = 0;
	info->delay_decay = 0.6;
	memset(info->delay_ram, 0, DELAYRAM_SIZE);

	if (chip == 0) {
		info->irqtimer.stop_retrig();
	}
	info->timer_state = 0;
	if (info->intf.irq) info->intf.irq(info->timer_state);

	info->cur_ptr = 0;
	info->cur_zone = info->rom;
	memset(info->regs, 0, sizeof(info->regs));
	memset(info->k054539_posreg_latch, 0, sizeof(info->k054539_posreg_latch));
	memset(info->channels, 0, sizeof(info->channels));
	memset(soundbuf[0], 0, (800 * sizeof(INT16) * 2) * 4);
	memset(soundbuf[1], 0, (800 * sizeof(INT16) * 2) * 4);
}

static void k054539_init_chip(INT32 clock, UINT8 *rom, INT32 nLen)
{
	memset(info->regs, 0, sizeof(info->regs));
	memset(info->k054539_posreg_latch, 0, sizeof(info->k054539_posreg_latch));
	info->k054539_flags |= K054539_UPDATE_AT_KEYON; // make it default until proven otherwise

	info->delay_ram = (UINT8*)BurnMalloc(DELAYRAM_SIZE);
	info->delay_pos = 0;
	info->delay_size = 0;
	info->delay_decay = 0.6;
	info->timer_state = 0;
	memset(info->delay_ram, 0, DELAYRAM_SIZE);

	info->cur_ptr = 0;

	info->rom = rom;
	info->rom_size = nLen;
	info->rom_mask = 0xffffffffU;
	for (INT32 i = 0; i < 32; i++) {
		if((1U<<i) >= info->rom_size) {
			info->rom_mask = (1U<<i) - 1;
			break;
		}
	}

	info->volume[BURN_SND_K054539_ROUTE_1] = 1.00;
	info->volume[BURN_SND_K054539_ROUTE_2] = 1.00;
	info->output_dir[BURN_SND_K054539_ROUTE_1] = BURN_SND_ROUTE_BOTH;
	info->output_dir[BURN_SND_K054539_ROUTE_2] = BURN_SND_ROUTE_BOTH;

	INT32 sample_rate = clock / 384;
	if (sample_rate <= 0) sample_rate = 48000;
	info->biquad[0].init(FILT_HIGHPASS, sample_rate, 500, 1.0, 0.0);
	info->biquad[1].init(FILT_LOWPASS, sample_rate, 12000, 1.0, 0.0);
}

void K054539SetApanCallback(INT32 chip, void (*ApanCB)(double, double))
{
	info = &Chips[chip];
	info->intf.apan = ApanCB;
}

void K054539Init(INT32 chip, INT32 clock, UINT8 *rom, INT32 nLen)
{
	DebugSnd_K054539Initted = 1;
	
	INT32 i;

	memset(&Chips[chip], 0, sizeof(k054539_info));

	info = &Chips[chip];

	info->clock = clock;

	if (chip == 0) {
		timerInit();
		info->irqtimer.init(chip, timer_callback);
		timerAdd(info->irqtimer);
	}

	if (nBurnSoundRate) nSampleSize[chip] = (UINT32)(clock / 384) * (1 << 16) / nBurnSoundRate;
	nFractionalPosition[chip] = 0;
	nPosition[chip] = 0;

	for (i = 0; i < 8; i++)
		info->k054539_gain[i] = 1.0;

	info->k054539_flags = K054539_RESET_FLAGS;

	for(i=0; i<256; i++)
		info->voltab[i] = pow(10.0, (-36.0 * (double)i / (double)0x40) / 20.0) / 4.0;

	for(i=0; i<0xf; i++)
		info->pantab[i] = sqrt((double)i)  / sqrt((double)0xe);

	k054539_init_chip(clock, rom, nLen);

	if (soundbuf[0] == NULL) soundbuf[0] = (INT16*)BurnMalloc((800 * sizeof(INT16) * 2) * 4);
	if (soundbuf[1] == NULL) soundbuf[1] = (INT16*)BurnMalloc((800 * sizeof(INT16) * 2) * 4);

	nNumChips = chip;
}

void K054539SetRoute(INT32 chip, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539SetRoute called without init\n"));
	if (chip >nNumChips) bprintf(PRINT_ERROR, _T("K054539SetRoute called with invalid chip %x\n"), chip);
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("K054539SetRoute called with invalid index %i\n"), nIndex);
#endif

	info = &Chips[chip];
	
	info->volume[nIndex] = nVolume;
	info->output_dir[nIndex] = nRouteDir;
}

void K054539Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539Exit called without init\n"));
#endif

	if (!DebugSnd_K054539Initted) return;

	BurnFree (soundbuf[0]);
	BurnFree (soundbuf[1]);
	soundbuf[0] = NULL;
	soundbuf[1] = NULL;

	timerExit();

	for (INT32 i = 0; i < 2; i++) {
		info = &Chips[i];
		BurnFree (info->delay_ram);

		info->biquad[0].exit();
		info->biquad[1].exit();
	}
	
	DebugSnd_K054539Initted = 0;
	nNumChips = 0;
}

void K054539Update(INT32 chip, INT16 *outputs, INT32 samples_len)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539Update called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("K054539Update called with invalid chip %x\n"), chip);
#endif

	info = &Chips[chip];
#define VOL_CAP 1.80

	static const INT16 dpcm[16] = {
		0 * 0x100,   1 * 0x100,   2 * 0x100,   4 * 0x100,  8 * 0x100, 16 * 0x100, 32 * 0x100, 64 * 0x100,
		0 * 0x100, -64 * 0x100, -32 * 0x100, -16 * 0x100, -8 * 0x100, -4 * 0x100, -2 * 0x100, -1 * 0x100
	};
	INT16 *delay_line = (INT16 *)(info->delay_ram);

	if(!(info->regs[0x22f] & 1))
		return;

	// re-sampleizer pt.1
	INT32 nSamplesNeeded = (INT32)(((UINT64)nFractionalPosition[chip] + (UINT64)nSampleSize[chip] * samples_len) >> 16) + 8;

	INT16 *pBufL = soundbuf[chip] + 0 * 4096 + 5 + nPosition[chip];
	INT16 *pBufR = soundbuf[chip] + 1 * 4096 + 5 + nPosition[chip];

	for (INT32 sample = 0; sample < (nSamplesNeeded - nPosition[chip]); sample++) {
		double lval, rval;

		if(!(info->k054539_flags & K054539_DISABLE_REVERB))
			lval = rval = delay_line[info->delay_pos];
		else
			lval = rval = 0;
		delay_line[info->delay_pos] = 0;

		for(INT32 ch=0; ch<8; ch++) {
			if(info->regs[0x22c] & (1<<ch)) {
				UINT8 *base1 = info->regs + 0x20*ch;
				UINT8 *base2 = info->regs + 0x200 + 0x2*ch;
				struct k054539_channel *chan = info->channels + ch;

				INT32 delta = base1[0x00] | (base1[0x01] << 8) | (base1[0x02] << 16);

				INT32 vol = base1[0x03];

				INT32 bval = vol + base1[0x04];
				if (bval > 255) bval = 255;

				INT32 pan = base1[0x05];
				// DJ Main: 81-87 right, 88 middle, 89-8f left
				if (pan >= 0x81 && pan <= 0x8f)
					pan -= 0x81;
				else if (pan >= 0x11 && pan <= 0x1f)
					pan -= 0x11;
				else
					pan = 0x18 - 0x11;

				double cur_gain = info->k054539_gain[ch];

				double lvol = info->voltab[vol] * info->pantab[pan] * cur_gain;
				if (lvol > VOL_CAP)
					lvol = VOL_CAP;

				double rvol = info->voltab[vol] * info->pantab[0xe - pan] * cur_gain;
				if (rvol > VOL_CAP)
					rvol = VOL_CAP;

				double rbvol = info->voltab[bval] * cur_gain / 2;
				if (rbvol > VOL_CAP)
					rbvol = VOL_CAP;

				INT32 rdelta = (base1[0x06] | (base1[0x07] << 8)) >> 3;
				rdelta = (rdelta + info->delay_pos) & 0x3fff;

				INT32 cur_pos = (base1[0x0c] | (base1[0x0d] << 8) | (base1[0x0e] << 16));

				INT32 fdelta, pdelta;
				if(base2[0] & 0x20) {
					delta = -delta;
					fdelta = +0x10000;
					pdelta = -1;
				} else {
					fdelta = -0x10000;
					pdelta = +1;
				}

				INT32 cur_pfrac, cur_val, cur_pval;
				if(cur_pos != (INT32)chan->pos) {
					chan->pos = cur_pos;
					cur_pfrac = 0;
					cur_val = 0;
					cur_pval = 0;
				} else {
					cur_pfrac = chan->pfrac;
					cur_val = chan->val;
					cur_pval = chan->pval;
				}

				switch(base2[0] & 0xc) {
				case 0x0: { // 8bit pcm
					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = (INT16)(k054539_read_rom(info, cur_pos) << 8);
						if(cur_val == (INT16)0x8000 && (base2[1] & 1)) {
							cur_pos = (base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16));
							cur_val = (INT16)(k054539_read_rom(info, cur_pos) << 8);
						}
						if(cur_val == (INT16)0x8000) {
							k054539_keyoff(ch);
							cur_val = 0;
							break;
						}
					}
					break;
				}

				case 0x4: { // 16bit pcm lsb first
					pdelta <<= 1;

					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = (INT16)(k054539_read_rom(info, cur_pos) | k054539_read_rom(info, cur_pos + 1)<<8);
						if(cur_val == (INT16)0x8000 && (base2[1] & 1)) {
							cur_pos = (base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16));
							cur_val = (INT16)(k054539_read_rom(info, cur_pos) | k054539_read_rom(info, cur_pos + 1)<<8);
						}
						if(cur_val == (INT16)0x8000) {
							k054539_keyoff(ch);
							cur_val = 0;
							break;
						}
					}
					break;
				}

				case 0x8: { // 4bit dpcm
					cur_pos <<= 1;
					cur_pfrac <<= 1;
					if(cur_pfrac & 0x10000) {
						cur_pfrac &= 0xffff;
						cur_pos |= 1;
					}

					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = k054539_read_rom(info, cur_pos >> 1);
						if(cur_val == 0x88 && (base2[1] & 1)) {
							cur_pos = (base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16)) << 1;
							cur_val = k054539_read_rom(info, cur_pos >> 1);
						}
						if(cur_val == 0x88) {
							k054539_keyoff(ch);
							cur_val = 0;
							break;
						}
						if(cur_pos & 1)
							cur_val >>= 4;
						else
							cur_val &= 15;
						cur_val = cur_pval + dpcm[cur_val];
						if(cur_val < -32768)
							cur_val = -32768;
						else if(cur_val > 32767)
							cur_val = 32767;
					}

					cur_pfrac >>= 1;
					if(cur_pos & 1)
						cur_pfrac |= 0x8000;
					cur_pos >>= 1;
					break;
				}
				default:
					//LOG(("Unknown sample type %x for channel %d\n", base2[0] & 0xc, ch));
					break;
				}
				lval += cur_val * lvol;
				rval += cur_val * rvol;
				delay_line[(rdelta + info->delay_pos) & 0x1fff] += (INT16)(cur_val * rbvol);

				chan->lvol = lvol;
				chan->rvol = rvol;

				chan->pos = cur_pos;
				chan->pfrac = cur_pfrac;
				chan->pval = cur_pval;
				chan->val = cur_val;

				if(k054539_regupdate()) {
					base1[0x0c] = cur_pos     & 0xff;
					base1[0x0d] = cur_pos>> 8 & 0xff;
					base1[0x0e] = cur_pos>>16 & 0xff;
				}
			} else { // channel: off
				// get rampy to remove dc offset clicks -dink [Dec. 1, 2017]
				struct k054539_channel *chan = info->channels + ch;

				if (chan->val > 0) {
					chan->val -= ((chan->val >  4) ? 4 : 1);
				} else if (chan->val < 0) {
					chan->val += ((chan->val < -4) ? 4 : 1);
				}

				lval += chan->val * chan->lvol;
				rval += chan->val * chan->rvol;
			}
		}
		info->delay_pos = (info->delay_pos + 1) & 0x1fff;

		if (info->k054539_flags & K054539_REVERSE_STEREO) {
			double temp = rval;
			rval = lval;
			lval = temp;
		}

		INT32 nLeftSample = 0, nRightSample = 0;

		if ((info->output_dir[BURN_SND_K054539_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(lval * info->volume[BURN_SND_K054539_ROUTE_1]);
		}
		if ((info->output_dir[BURN_SND_K054539_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(rval * info->volume[BURN_SND_K054539_ROUTE_1]);
		}

		if ((info->output_dir[BURN_SND_K054539_ROUTE_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(lval * info->volume[BURN_SND_K054539_ROUTE_2]);
		}
		if ((info->output_dir[BURN_SND_K054539_ROUTE_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(rval * info->volume[BURN_SND_K054539_ROUTE_2]);
		}

		pBufL[0] = BURN_SND_CLIP(nLeftSample);  pBufL++;
		pBufR[0] = BURN_SND_CLIP(nRightSample); pBufR++;
	}

	pBufL = soundbuf[chip] + 0 * 4096 + 5;
	pBufR = soundbuf[chip] + 1 * 4096 + 5;

	for (INT32 i = (nFractionalPosition[chip] & 0xFFFF0000) >> 15; i < (samples_len << 1); i += 2, nFractionalPosition[chip] += nSampleSize[chip]) {
		INT32 nLeftSample[4] = {0, 0, 0, 0};
		INT32 nRightSample[4] = {0, 0, 0, 0};
		INT32 nTotalLeftSample, nTotalRightSample;

		nLeftSample[0] += (INT32)(pBufL[(nFractionalPosition[chip] >> 16) - 3]);
		nLeftSample[1] += (INT32)(pBufL[(nFractionalPosition[chip] >> 16) - 2]);
		nLeftSample[2] += (INT32)(pBufL[(nFractionalPosition[chip] >> 16) - 1]);
		nLeftSample[3] += (INT32)(pBufL[(nFractionalPosition[chip] >> 16) - 0]);

		nRightSample[0] += (INT32)(pBufR[(nFractionalPosition[chip] >> 16) - 3]);
		nRightSample[1] += (INT32)(pBufR[(nFractionalPosition[chip] >> 16) - 2]);
		nRightSample[2] += (INT32)(pBufR[(nFractionalPosition[chip] >> 16) - 1]);
		nRightSample[3] += (INT32)(pBufR[(nFractionalPosition[chip] >> 16) - 0]);

		nTotalLeftSample = INTERPOLATE4PS_16BIT((nFractionalPosition[chip] >> 4) & 0x0fff, nLeftSample[0], nLeftSample[1], nLeftSample[2], nLeftSample[3]);
		nTotalRightSample = INTERPOLATE4PS_16BIT((nFractionalPosition[chip] >> 4) & 0x0fff, nRightSample[0], nRightSample[1], nRightSample[2], nRightSample[3]);

		nTotalLeftSample = BURN_SND_CLIP(nTotalLeftSample);
		nTotalRightSample = BURN_SND_CLIP(nTotalRightSample);

		outputs[i + 0] = BURN_SND_CLIP(outputs[i + 0] + nTotalLeftSample);
		outputs[i + 1] = BURN_SND_CLIP(outputs[i + 1] + nTotalRightSample);
	}

	INT32 nExtraSamples = nSamplesNeeded - (nFractionalPosition[chip] >> 16);
	if (nExtraSamples < 0) {
		nExtraSamples = 0;
	}

	for (INT32 i = -4; i < nExtraSamples; i++) {
		pBufL[i] = pBufL[(nFractionalPosition[chip] >> 16) + i];
		pBufR[i] = pBufR[(nFractionalPosition[chip] >> 16) + i];
	}

	nFractionalPosition[chip] &= 0xFFFF;
	nPosition[chip] = nExtraSamples;
	//  -4 -3 -2 -1 0 +1 +2 +3 +4
}

void K054539Scan(INT32 nAction, INT32 *)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K054539Initted) bprintf(PRINT_ERROR, _T("K054539Scan called without init\n"));
#endif

	struct BurnArea ba;
	char szName[32];

	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}

	for (INT32 i = 0; i < nNumChips+1; i++) {
		info = &Chips[i];

		memset(&ba, 0, sizeof(ba));
		sprintf(szName, "K054539 Latch %d", i);
		ba.Data		= info->k054539_posreg_latch;
		ba.nLen		= sizeof(info->k054539_posreg_latch);
		ba.nAddress = 0;
		ba.szName	= szName;
		BurnAcb(&ba);

		sprintf(szName, "K054539 Regs # %d", i);
		ba.Data		= info->regs;
		ba.nLen		= sizeof(info->regs);
		ba.nAddress = 0;
		ba.szName	= szName;
		BurnAcb(&ba);

		sprintf(szName, "K054539 DelayRam # %d", i);
		ba.Data		= info->delay_ram;
		ba.nLen		= DELAYRAM_SIZE;
		ba.nAddress = 0;
		ba.szName	= szName;
		BurnAcb(&ba);

		sprintf(szName, "K054539 Channels # %d", i);
		ba.Data		= &info->channels;
		ba.nLen		= sizeof(info->channels);
		ba.nAddress = 0;
		ba.szName	= szName;
		BurnAcb(&ba);

		SCAN_VAR(info->delay_pos);
		SCAN_VAR(info->delay_size);
		SCAN_VAR(info->delay_decay);
		SCAN_VAR(info->timer_state);
		SCAN_VAR(info->cur_ptr);
		SCAN_VAR(info->cur_limit);

		if (i == 0) {
			timerScan();
		}

		if (nAction & ACB_WRITE) {
			INT32 data = info->regs[0x22e];
			info->cur_zone =
				data == 0x80 ? info->delay_ram :
				info->rom + 0x20000*data;
			info->cur_limit = data == 0x80 ? 0x4000 : 0x20000;

			if (~nAction & ACB_RUNAHEAD) {
				for (INT32 chip = 0; chip < 2; chip++) {
					nFractionalPosition[chip] = 0;
					nPosition[chip] = 0;
				}

				memset(soundbuf[0], 0, (800 * sizeof(INT16) * 2) * 4);
				memset(soundbuf[1], 0, (800 * sizeof(INT16) * 2) * 4);
			}
		}
	}
}
