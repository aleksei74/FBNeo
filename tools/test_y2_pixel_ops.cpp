// Standalone exact scalar/NEON colour comparison.
#include "../src/burn/drv/sega/y2_pixel_ops.h"
#include <stdio.h>
#include <stdlib.h>

static uint32_t rng = 0xb720cdef;
static uint32_t Random() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

int main()
{
	using namespace Y2PixelOps;
	for (unsigned a = 0; a < 256; a++)
		for (unsigned b = 0; b < 256; b++) {
			uint32_t src = (a * 0x01010101u), dst = (b * 0x01010101u);
			if (Modulate(src, dst) != ModulateScalar(src, dst)) return 1;
			for (unsigned mode = 0; mode < 2; mode++) {
				const unsigned factors[] = {0, 1, 127, 128, 254, 255};
				for (unsigned sf : factors) for (unsigned df : factors)
					if (BlendAlpha(src, dst, sf, df, mode) != BlendScalar(src, dst, sf, df, mode)) return 2;
			}
		}
	for (unsigned i = 0; i < 1000000; i++) {
		uint32_t src = Random(), dst = Random();
		unsigned sf = Random() & 255, df = Random() & 255;
		if (Modulate(src, dst) != ModulateScalar(src, dst)) return 3;
		for (unsigned mode = 0; mode < 2; mode++)
			if (BlendAlpha(src, dst, sf, df, mode) != BlendScalar(src, dst, sf, df, mode)) return 4;
	}
	printf("Y2 exact pixel arithmetic: PASS (NEON %d)\n", Y2_PIXEL_NEON);
}
