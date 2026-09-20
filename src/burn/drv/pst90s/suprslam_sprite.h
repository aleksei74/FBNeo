#pragma once

// The visible span is smaller than the 512-pixel sprite wrap period.
static INT32 SuprslamWrapCoordinate(INT32 position, INT32 limit)
{
	return position >= limit ? position - 512 : position;
}

struct SuprslamSpriteTile {
	INT32 code, color, x, y, width, height, flipx, flipy;
	INT32 opaque;
};

static INT32 SuprslamBuildColumns(INT32 ox, INT32 size, INT32 zoom, INT32 flip,
	INT32 limit, INT32 *positions, INT32 *offsets)
{
	INT32 count = 0;
	const INT32 width = (zoom + 1) / 2;
	for (INT32 col = 0; col <= size; col++) {
		const INT32 x = SuprslamWrapCoordinate(ox + (flip ? size - col : col) * zoom / 2, limit);
		if (x >= limit || x + width <= 0) continue;
		positions[count] = x;
		offsets[count++] = col;
	}
	return count;
}

// 0: transparent, 1: mixed, 2: opaque. Decoded sprite ROM is immutable.
static UINT8 SuprslamTileCoverage(const UINT8 *pixels)
{
	INT32 transparent = 0;
	for (INT32 i = 0; i < 256; i++) transparent += pixels[i] == 15;
	return transparent == 256 ? 0 : (transparent == 0 ? 2 : 1);
}

static INT32 SuprslamZoomStep(INT32 extent)
{
	// zoom = 17..32 produces rounded extents of 9..16 pixels.
	// Keep integer truncation identical to RenderZoomedTile, including flips.
	static const INT32 steps[8] = {
		0x100000 / 9, 0x100000 / 10, 0x100000 / 11, 0x100000 / 12,
		0x100000 / 13, 0x100000 / 14, 0x100000 / 15, 0x100000 / 16
	};
	return steps[extent - 9];
}

static void SuprslamRenderTile(UINT16 *dest, INT32 stride, const UINT8 *gfx,
	const SuprslamSpriteTile &tile, INT32 minx, INT32 maxx, INT32 miny, INT32 maxy)
{
	const INT32 top = tile.y > miny ? tile.y : miny;
	const INT32 bottom = tile.y + tile.height < maxy ? tile.y + tile.height : maxy;
	// Row workers reject other bands before doing horizontal clipping.
	if (top >= bottom) return;
	const INT32 left = tile.x > minx ? tile.x : minx;
	const INT32 right = tile.x + tile.width < maxx ? tile.x + tile.width : maxx;
	if (left >= right) return;

	INT32 dy = SuprslamZoomStep(tile.height);
	INT32 starty = tile.flipy ? (tile.height - 1) * dy : 0;
	if (tile.flipy) dy = -dy;
	starty += (top - tile.y) * dy;
	const UINT8 *base = gfx + tile.code * 256;
	// A rounded width of 16 maps one source pixel to each destination pixel.
	// This also covers zoom 31, whose rounded sampling matches zoom 32.
	if (tile.width == 16) {
		if (tile.opaque) {
			const INT32 sourcex = tile.flipx ? 15 - (left - tile.x) : left - tile.x;
			const INT32 count = right - left;
			for (INT32 y = top; y < bottom; y++, starty += dy) {
				const UINT8 *src = base + (starty >> 16) * 16 + sourcex;
				UINT16 *out = dest + y * stride + left;
				INT32 x = 0;
				if (tile.flipx) {
					for (; x + 4 <= count; x += 4) {
						out[x + 0] = src[-x - 0] + tile.color;
						out[x + 1] = src[-x - 1] + tile.color;
						out[x + 2] = src[-x - 2] + tile.color;
						out[x + 3] = src[-x - 3] + tile.color;
					}
					for (; x < count; x++) out[x] = src[-x] + tile.color;
					continue;
				}
				for (; x + 4 <= count; x += 4) {
					out[x + 0] = src[x + 0] + tile.color;
					out[x + 1] = src[x + 1] + tile.color;
					out[x + 2] = src[x + 2] + tile.color;
					out[x + 3] = src[x + 3] + tile.color;
				}
				for (; x < count; x++) out[x] = src[x] + tile.color;
			}
			return;
		}
		const INT32 step = tile.flipx ? -1 : 1;
		const INT32 sourcex = tile.flipx ? 15 - (left - tile.x) : left - tile.x;
		for (INT32 y = top; y < bottom; y++, starty += dy) {
			const UINT8 *src = base + (starty >> 16) * 16;
			UINT16 *out = dest + y * stride;
			INT32 u = sourcex;
			for (INT32 x = left; x < right; x++, u += step) {
				const INT32 pixel = src[u];
				if (pixel != 15) out[x] = pixel + tile.color;
			}
		}
		return;
	}
	INT32 dx = SuprslamZoomStep(tile.width);
	INT32 startx = tile.flipx ? (tile.width - 1) * dx : 0;
	if (tile.flipx) dx = -dx;
	startx += (left - tile.x) * dx;
	for (INT32 y = top; y < bottom; y++, starty += dy) {
		const UINT8 *src = base + (starty >> 16) * 16;
		UINT16 *out = dest + y * stride;
		INT32 u = startx;
		if (tile.opaque) {
			for (INT32 x = left; x < right; x++, u += dx) {
				out[x] = src[u >> 16] + tile.color;
			}
		} else {
			for (INT32 x = left; x < right; x++, u += dx) {
				const INT32 pixel = src[u >> 16];
				if (pixel != 15) out[x] = pixel + tile.color;
			}
		}
	}
}
