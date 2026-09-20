#pragma once

// 32 colors per block keeps deferred palette tracking small (514 bytes).
class GxPaletteUpdates
{
public:
	GxPaletteUpdates() : count(0) { for (int i = 0; i < 256; i++) dirty[i] = 0; }
	void Mark(UINT32 address)
	{
		const UINT32 block = (address & 0x7fff) >> 7;
		if (!dirty[block]) {
			dirty[block] = 1;
			blocks[count++] = (UINT8)block;
		}
	}
	void Update(const UINT32 *ram, UINT32 *palette, bool full)
	{
		// Every block is queued: convert in memory order instead of write order.
		if (count == 256) full = true;
		if (full) {
			for (int i = 0; i < 0x2000; i++) palette[i] = ram[i] & 0x00ffffff;
			// A full conversion consumes every dirty block, regardless of queue order.
			for (int i = 0; i < 256; i++) dirty[i] = 0;
			count = 0;
			return;
		}
		for (int n = 0; n < count; n++) {
			const int block = blocks[n];
			for (int i = block * 32; i < (block + 1) * 32; i++) palette[i] = ram[i] & 0x00ffffff;
			dirty[block] = 0;
		}
		count = 0;
	}
private:
	UINT8 dirty[256], blocks[256];
	UINT16 count;
};
