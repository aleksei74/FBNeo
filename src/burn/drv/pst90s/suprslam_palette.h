#pragma once

#include <cstring>

struct SuprslamPaletteChanges {
	unsigned char dirty[2048];
	UINT16 entries[2048];
	INT32 count;
	void Clear() {
		if (count <= 8) {
			for (INT32 i = 0; i < count; i++) dirty[entries[i]] = 0;
		} else {
			std::memset(dirty, 0, sizeof(dirty));
		}
		count = 0;
	}
	void Mark(INT32 index) {
		if (dirty[index]) return;
		dirty[index] = 1;
		entries[count++] = (UINT16)index;
	}
};
