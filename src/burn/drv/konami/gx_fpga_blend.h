// SPDX-License-Identifier: GPL-3.0-or-later
// GX blend arithmetic from rtl/video/gx_mixer.v alpha_of/blendch.
// Software adaptation, 2026-11-04. See gx_esc_provenance.md.
#ifndef FBNEO_GX_FPGA_BLEND_H
#define FBNEO_GX_FPGA_BLEND_H
#include <stdint.h>
static inline unsigned GxFpgaAlpha(unsigned mix, unsigned r13, unsigned r14)
{
	if (!mix)
		return 255;
	unsigned setting = ((mix & 2) ? r14 : r13) >> ((mix & 1) ? 0 : 8);
	unsigned level = setting & 31, add = setting & 32;
	if (!add)
		level = 31 - level;
	return (add ? 0x100 : 0) | ((level << 3) | (level >> 2));
}
static inline uint32_t GxFpgaBlend(uint32_t dst, uint32_t src, unsigned setting)
{
	uint32_t result = 0;
	for (unsigned shift = 0; shift < 24; shift += 8) {
		int d = (dst >> shift) & 255, s = (src >> shift) & 255;
		int p = ((setting & 0x100) ? s : s - d) * (int)(setting & 255) + ((setting & 0x100) ? s : 0);
		// Explicit floor division matches RTL signed p[18:8], including negatives.
		int value = d + (p < 0 ? -((-p + 255) / 256) : p / 256);
		if (value < 0)
			value = 0;
		if (value > 255)
			value = 255;
		result |= (uint32_t)value << shift;
	}
	return result;
}
#endif
