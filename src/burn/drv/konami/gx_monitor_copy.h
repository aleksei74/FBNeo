#pragma once
#include <cstring>

static inline void GxCopyMonitor(UINT32 *target, const UINT32 *source,
	INT32 width, INT32 height, INT32 pitch, INT32 xoffset, INT32 yoffset)
{
	source += yoffset * pitch + xoffset;
	// Measured bulk-copy gains on x86 did not carry over to x64.
	if (sizeof(void*) == 4 && pitch == width) {
		memcpy(target, source, (size_t)width * height * sizeof(UINT32));
		return;
	}
	for (INT32 y = 0; y < height; y++) {
		memcpy(target, source, width * sizeof(UINT32));
		target += width;
		source += pitch;
	}
}
