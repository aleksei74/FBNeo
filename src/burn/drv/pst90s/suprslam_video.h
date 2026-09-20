#pragma once

// Skip unchanged 32-byte blocks before checking individual two-byte tiles.
template<typename MarkTile>
static void SuprslamMarkBackgroundChanges(const UINT8 *previous, const UINT8 *current, MarkTile mark)
{
	for (INT32 block = 0; block < 0x2000; block += 32) {
		if (memcmp(previous + block, current + block, 32) == 0) continue;
		for (INT32 offset = block; offset < block + 32; offset += 2) {
			if (previous[offset] != current[offset] || previous[offset + 1] != current[offset + 1])
				mark(offset / 2);
		}
	}
}

// Recognize the two-row RANKING title, not general sprite priority/bank settings.
// The caller supplies native-endian tile words through read_tile.
template<typename ReadTile>
static bool SuprslamRankingScreen(int screen_bank, int bg_bank, int sprite_ctrl, ReadTile read_tile)
{
	if (screen_bank != 0x1000 || bg_bank != 0x2000 || !(sprite_ctrl & 8)) return false;
	static const unsigned short title[] = {
		0xf04c, 0xf04d, 0xf04e, 0xf04f, 0xf048, 0xf049, 0xf050,
		0xf051, 0xf046, 0xf047, 0xf048, 0xf049, 0xf052, 0xf053
	};
	// POINT RANKING and SLAMDUNK RANKING use different title columns.
	for (int column = 19; column <= 22; column += 3) {
		int tile = 0;
		for (; tile < 14; tile++) {
			if (read_tile(2 * 64 + column + tile) != title[tile] ||
				read_tile(3 * 64 + column + tile) != title[tile] + 0x24) break;
		}
		if (tile == 14) return true;
	}
	return false;
}
