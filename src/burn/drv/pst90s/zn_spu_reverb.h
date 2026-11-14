// license:BSD-3-Clause
// copyright-holders:pSXAuthor, R. Belmont
// Preset-based reverb matches MAME 0.289 spu_tables.cpp / spureverb.cpp.
// Included inside ZnSpuAudio; the delay/filter history is save-state data.
struct ReverbParams {
	float band_pole, band_gain, comb_delay[2][4], comb_gain, allpass_delay, allpass_gain;
};
struct ReverbPreset {
	const char *name;
	UINT16 param[32];
	ReverbParams cfg;
};
static ReverbPreset reverbPresets[] = {
	{
		"Reverb off",
		{
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
		}
	},

	{
		"Room",
		{
			0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0x0000, 0x0000, 0xBA80,
			0x5800, 0x5300, 0x04D6, 0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF,
			0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x01B4, 0x0136, 0x00B8, 0x005C, 0x8000, 0x8000
		},
		{ // made up from studio small      [used by BOF3]
			-0.3112f,
			0.7832f,

			{ { 20.7f, 31.3f, 40.1711f, 51.6803f },
			  { 21.7025f, 30.2f, 42.6655f, 48.6691f } },
			0.8f,

			0.0f,
			0.0f
		}
	},

	{
		"Studio Small",
		{
			0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00,
			0x5280, 0x4EC0, 0x03E4, 0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266,
			0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135, 0x01D2, 0x00B7,
			0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, 0x8000, 0x8000
		},
		{ // made up from studio large      [used by Fighters Impact]
			-0.3112f,
			0.7832f,

			{ { 24.7f, 44.3f, 49.1711f, 59.6803f },
			  { 27.7025f, 38.2f, 51.6655f, 59.6691f } },
			0.6462f,

			0.0f,
			0.0f
		}
	},

	{
		"Studio Medium",
		{
			0x00B1, 0x007F, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xB4C0,
			0x5280, 0x4EC0, 0x0904, 0x076B, 0x0824, 0x065F, 0x07A2, 0x0616,
			0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305, 0x0462, 0x02B7,
			0x042F, 0x0265, 0x0264, 0x01B2, 0x0100, 0x0080, 0x8000, 0x8000
		},
		{ // made up from studio large
			-0.3112f,
			0.7832f,

			{ { 37.7f, 62.3f, 82.1711f, 71.6803f },
			  { 43.7025f, 62.2f, 85.6655f, 78.6691f } },
			0.6462f,

			0.0f,
			0.0f
		}
	},

	{
		"Studio Large",
		{
			0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680,
			0x5680, 0x52C0, 0x0DFB, 0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973,
			0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0, 0x06EF, 0x03D2,
			0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000
		},
		{ // tuned from xenogears ost
			-0.3112f,
			0.7832f,

			{ { 37.7f, 62.3f, 82.1711f, 71.6803f },
			  { 43.7025f, 62.2f, 85.6655f, 78.6691f } },
			0.8462f,

			0.0f,
			0.0f
		}
	},

	{
		"Hall",
		{
			0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000,
			0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1,
			0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD,
			0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000
		},
		{ // made up
			-0.4222f,
			0.8889f,

			{ { 20.37f, 79.63f, 107.40f, 94.44f },
			  { 31.47f, 72.22f, 116.66f, 105.55f } },
			0.8889f,

			0.0f,
			0.0f
		}
	},

	{
		"Space Echo",
		{
			0x033D, 0x0231, 0x7E00, 0x5000, 0xB400, 0xB000, 0x4C00, 0xB000,
			0x6000, 0x5400, 0x1ED6, 0x1A31, 0x1D14, 0x183B, 0x1BC2, 0x16B2,
			0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D, 0x11F6, 0x0C5D,
			0x1056, 0x0AE1, 0x0AE0, 0x07A2, 0x0464, 0x0232, 0x8000, 0x8000
		},
		{ // made up
			0.3951f,
			0.7552f,

			{
			  { 36.7138f, 134.6171f, 24.4758f, 194.0596f },
			  {   54.1932f, 90.9070f, 29.7174f, 200.0000f },
			},

			0.9301f,

			109.2563f,
			0.4222f
		}
	},

	{
		"Echo",
		{
			0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x8100,
			0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
			0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000
		}
	},

	{
		"Delay",
		{
			0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
			0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
			0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000,
		}
	},

	{
		"Half Echo",
		{
			0x0017, 0x0013, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0x8500,
			0x5F80, 0x54C0, 0x0371, 0x02AF, 0x02E5, 0x01DF, 0x02B0, 0x01D7,
			0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1, 0x011F, 0x0059,
			0x01A0, 0x00E3, 0x0058, 0x0040, 0x0028, 0x0014, 0x8000, 0x8000
		},
		{ // made up from space echo
			0.3951f,
			0.7552f,

			{
			  { 36.7138f, 134.6171f, 24.4758f, 194.0596f },
			  {   54.1932f, 90.9070f, 29.7174f, 200.0000f },
			},

			0.7301f,

			109.2563f,
			0.7222f
		}
	},

	{ nullptr }
};

//
//
//

struct ReverbHistory {
	INT16 y[2][4][65536], x[2][65536], ax[2][65536], ay[2][65536], bx[2][2];
	INT32 position, preset, dirty;
};
static ReverbHistory reverb;
static void ReverbSample(INT32 *dry, const INT32 *wet)
{
	if (reverb.dirty) {
		for (INT32 i = 0; reverbPresets[i].name; i++) {
			if (!memcmp(reverbPresets[i].param, ZnSpuRegs + 0x1c0 / 2, 64)) {
				reverb.preset = i;
				break;
			}
		}
		reverb.dirty = 0;
	}
	const ReverbParams &p = reverbPresets[reverb.preset].cfg;
	if (p.band_gain <= 0) {
		for (INT32 c = 0; c < 2; c++)
			dry[c] = Clip(dry[c] + wet[c]);
		return;
	}
	INT32 pos = reverb.position;
	INT32 pole = (INT32)(p.band_pole * 32767), gain = (INT32)(p.band_gain * 32767);
	INT32 combGain = (INT32)(p.comb_gain * 32767);
	INT32 apDelay = (INT32)((p.allpass_delay / 1000.0f) * 44100) & ~3;
	INT32 apGain = (INT32)(p.allpass_gain * 32767);
	for (INT32 c = 0; c < 2; c++) {
		INT32 input = wet[c] + reverb.bx[c][1] + ((pole * reverb.x[c][(pos - 1) & 65535]) >> 15);
		reverb.x[c][pos] = Clip((input * gain) >> 15);
		reverb.bx[c][1] = reverb.bx[c][0];
		reverb.bx[c][0] = wet[c];
		INT32 value = 0;
		for (INT32 f = 0; f < 4; f++) {
			INT32 delay = (INT32)((p.comb_delay[c][f] / 1000.0f) * 44100) & ~3;
			INT32 index = (pos - delay) & 65535;
			reverb.y[c][f][pos] = Clip(reverb.x[c][index] + ((combGain * reverb.y[c][f][index]) >> 15));
			value += reverb.y[c][f][pos];
		}
		value >>= 2;
		if (apDelay) {
			reverb.ax[c][pos] = value;
			INT32 index = (pos - apDelay) & 65535;
			value = Clip(((apGain * (reverb.ay[c][index] - reverb.x[c][pos])) >> 15) + reverb.ax[c][index]);
			reverb.ay[c][pos] = value;
		}
		dry[c] = Clip(dry[c] + wet[c] + ((value * (INT16)ZnSpuRegs[0x184 / 2 + c]) >> 15));
	}
	reverb.position = (pos + 1) & 65535;
}
