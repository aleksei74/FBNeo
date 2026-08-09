#ifndef FBNEO_NAMCOS_GL_RASTER_H
#define FBNEO_NAMCOS_GL_RASTER_H

class NamcosPolyThreadPool;

struct NamcosGlRasterState
{
	UINT32 tpage;
	UINT32 textureWindowX;
	UINT32 textureWindowY;
	UINT32 textureWindowW;
	UINT32 textureWindowH;
	UINT32 drawX1;
	UINT32 drawY1;
	UINT32 drawX2;
	UINT32 drawY2;
	INT32 drawOffsetX;
	INT32 drawOffsetY;
	INT32 drawStp;
	INT32 checkStp;
	INT32 gpuType;
};

struct NamcosGlRasterPacket
{
	const UINT32 *words;
	UINT32 wordCount;
	UINT8 command;
	UINT16 *vram;
	UINT64 *vramGeneration;
	UINT64 *vramRowGeneration;
	NamcosPolyThreadPool *threadPool;
	NamcosGlRasterState state;
};

enum NamcosGlRasterPrimitiveType
{
	NAMCOS_GL_RASTER_NONE = 0,
	NAMCOS_GL_RASTER_FILL,
	NAMCOS_GL_RASTER_FLAT_POLYGON,
	NAMCOS_GL_RASTER_GOURAUD_POLYGON,
	NAMCOS_GL_RASTER_TEXTURED_POLYGON,
	NAMCOS_GL_RASTER_FLAT_RECTANGLE,
	NAMCOS_GL_RASTER_TEXTURED_RECTANGLE
};

struct NamcosGlRasterVertex
{
	INT32 x;
	INT32 y;
	UINT8 red;
	UINT8 green;
	UINT8 blue;
	UINT8 u;
	UINT8 v;
};

struct NamcosGlRasterPrimitive
{
	NamcosGlRasterPrimitiveType type;
	NamcosGlRasterVertex vertex[4];
	UINT32 vertexCount;
	INT32 width;
	INT32 height;
	INT32 semiTransparent;
	INT32 rawTexture;
	INT32 gouraud;
	UINT32 clut;
	UINT32 tpage;
};

struct NamcosGlRasterDrawVertex
{
	float x;
	float y;
	float red;
	float green;
	float blue;
	float u;
	float v;
};

struct NamcosGlRasterRect
{
	INT32 x1;
	INT32 y1;
	INT32 x2;
	INT32 y2;
};

struct NamcosGlRasterDirtyBounds
{
	INT32 x1;
	INT32 y1;
	INT32 x2;
	INT32 y2;
	NamcosGlRasterRect rects[8];
	INT32 rectCount;
	bool sparse;
	bool valid;

	NamcosGlRasterDirtyBounds() : x1(0), y1(0), x2(0), y2(0),
		rectCount(0), sparse(true), valid(false) {}

	void Reset()
	{
		rectCount = 0;
		sparse = true;
		valid = false;
	}

	void Include(INT32 left, INT32 top, INT32 right, INT32 bottom)
	{
		if (!valid) {
			x1 = left;
			y1 = top;
			x2 = right;
			y2 = bottom;
			rects[0].x1 = left;
			rects[0].y1 = top;
			rects[0].x2 = right;
			rects[0].y2 = bottom;
			rectCount = 1;
			sparse = true;
			valid = true;
			return;
		}
		if (left < x1) x1 = left;
		if (top < y1) y1 = top;
		if (right > x2) x2 = right;
		if (bottom > y2) y2 = bottom;
		if (!sparse) return;

		NamcosGlRasterRect merged = { left, top, right, bottom };
		for (INT32 i = 0; i < rectCount;) {
			const NamcosGlRasterRect &rect = rects[i];
			if (merged.x2 + 1 < rect.x1 || rect.x2 + 1 < merged.x1 ||
				merged.y2 + 1 < rect.y1 || rect.y2 + 1 < merged.y1) {
				i++;
				continue;
			}
			if (rect.x1 < merged.x1) merged.x1 = rect.x1;
			if (rect.y1 < merged.y1) merged.y1 = rect.y1;
			if (rect.x2 > merged.x2) merged.x2 = rect.x2;
			if (rect.y2 > merged.y2) merged.y2 = rect.y2;
			rects[i] = rects[--rectCount];
			i = 0;
		}
		if (rectCount == 8) {
			rectCount = 0;
			sparse = false;
			return;
		}
		rects[rectCount++] = merged;
	}

	INT32 GetReadbackRects(NamcosGlRasterRect *output, INT32 capacity) const
	{
		if (!valid || output == NULL || capacity <= 0) return 0;
		if (sparse && rectCount > 1 && rectCount <= capacity) {
			INT64 sparseArea = 0;
			for (INT32 i = 0; i < rectCount; i++) {
				sparseArea += (INT64)(rects[i].x2 - rects[i].x1 + 1) *
					(rects[i].y2 - rects[i].y1 + 1);
			}
			const INT64 boundingArea = (INT64)(x2 - x1 + 1) *
				(y2 - y1 + 1);
			if (sparseArea * 2 <= boundingArea) {
				memcpy(output, rects,
					(size_t)rectCount * sizeof(NamcosGlRasterRect));
				return rectCount;
			}
		}
		output[0].x1 = x1;
		output[0].y1 = y1;
		output[0].x2 = x2;
		output[0].y2 = y2;
		return 1;
	}
};

struct NamcosGlRasterUploadTracker
{
	UINT64 rowGeneration[1024];
	bool valid;

	NamcosGlRasterUploadTracker() : valid(false) {}

	void Reset()
	{
		valid = false;
	}

	void RememberAll(const UINT64 *source)
	{
		if (source == NULL) {
			valid = false;
			return;
		}
		memcpy(rowGeneration, source, sizeof(rowGeneration));
		valid = true;
	}

	void RememberRange(const UINT64 *source, INT32 first, INT32 rows)
	{
		if (source == NULL || first < 0 || rows <= 0 || first + rows > 1024)
			return;
		memcpy(rowGeneration + first, source + first,
			(size_t)rows * sizeof(UINT64));
		valid = true;
	}
};

struct NamcosGlRasterUploadSpan
{
	INT32 firstRow;
	INT32 rowCount;
};

static inline INT32 NamcosGlRasterBuildUploadSpans(
	const NamcosGlRasterUploadTracker *tracker, const UINT64 *rowGeneration,
	NamcosGlRasterUploadSpan *spans, INT32 capacity,
	INT32 *boundingFirst, INT32 *boundingRows)
{
	if (tracker == NULL || rowGeneration == NULL || spans == NULL ||
		capacity <= 0 || boundingFirst == NULL || boundingRows == NULL) {
		return -1;
	}

	INT32 first = 0;
	while (first < 1024 && tracker->rowGeneration[first] ==
		rowGeneration[first]) first++;
	if (first == 1024) {
		*boundingFirst = 0;
		*boundingRows = 0;
		return 0;
	}
	INT32 last = 1024;
	while (last > first && tracker->rowGeneration[last - 1] ==
		rowGeneration[last - 1]) last--;
	*boundingFirst = first;
	*boundingRows = last - first;

	INT32 spanCount = 0;
	INT32 changedRows = 0;
	INT32 row = first;
	while (row < last) {
		while (row < last && tracker->rowGeneration[row] ==
			rowGeneration[row]) row++;
		if (row == last) break;
		const INT32 spanFirst = row;
		while (row < last && tracker->rowGeneration[row] !=
			rowGeneration[row]) row++;
		if (spanCount == capacity) return -1;
		spans[spanCount].firstRow = spanFirst;
		spans[spanCount].rowCount = row - spanFirst;
		changedRows += row - spanFirst;
		spanCount++;
	}

	if (spanCount > 1 && changedRows * 2 > *boundingRows) return -1;
	return spanCount;
}

static inline UINT8 NamcosGlRasterExpand5(UINT32 value)
{
	value &= 0x1f;
	return (UINT8)((value << 3) | (value >> 2));
}

#if defined(LSB_FIRST)
struct NamcosGlRasterPackTable
{
	UINT32 values[0x10000];

	NamcosGlRasterPackTable()
	{
		for (UINT32 pixel = 0; pixel < 0x10000; pixel++) {
			values[pixel] = (UINT32)NamcosGlRasterExpand5(pixel) |
				((UINT32)NamcosGlRasterExpand5(pixel >> 5) << 8) |
				((UINT32)NamcosGlRasterExpand5(pixel >> 10) << 16) |
				((pixel & 0x8000) ? 0xff000000U : 0);
		}
	}
};

static inline const UINT32 *NamcosGlRasterGetPackTable()
{
	static NamcosGlRasterPackTable table;
	return table.values;
}
#endif

static inline void NamcosGlRasterPackVramPixel(UINT16 source,
	UINT8 *destination)
{
	destination[0] = NamcosGlRasterExpand5(source);
	destination[1] = NamcosGlRasterExpand5(source >> 5);
	destination[2] = NamcosGlRasterExpand5(source >> 10);
	destination[3] = (source & 0x8000) ? 0xff : 0x00;
}

static inline UINT16 NamcosGlRasterUnpackVramPixel(const UINT8 *source)
{
	const UINT16 red = (UINT16)((source[0] * 31 + 127) / 255);
	const UINT16 green = (UINT16)((source[1] * 31 + 127) / 255);
	const UINT16 blue = (UINT16)((source[2] * 31 + 127) / 255);
	return (UINT16)(red | (green << 5) | (blue << 10) |
		(source[3] >= 0x80 ? 0x8000 : 0));
}

static inline void NamcosGlRasterPackVram(const UINT16 *source,
	UINT8 *destination)
{
	#if defined(LSB_FIRST)
	const UINT32 *table = NamcosGlRasterGetPackTable();
	#endif
	for (UINT32 y = 0; y < 1024; y++) {
		UINT8 *row = destination + (size_t)(1023 - y) * 1024 * 4;
		for (UINT32 x = 0; x < 1024; x++) {
			#if defined(LSB_FIRST)
			((UINT32 *)row)[x] = table[source[(size_t)y * 1024 + x]];
			#else
			NamcosGlRasterPackVramPixel(source[(size_t)y * 1024 + x],
				row + x * 4);
			#endif
		}
	}
}

static inline void NamcosGlRasterPackVramRange(const UINT16 *source,
	UINT8 *destination, INT32 first, INT32 rows)
{
	#if defined(LSB_FIRST)
	const UINT32 *table = NamcosGlRasterGetPackTable();
	#endif
	for (INT32 yy = 0; yy < rows; yy++) {
		const UINT16 *input = source + (size_t)(first + rows - 1 - yy) * 1024;
		UINT8 *output = destination + (size_t)yy * 1024 * 4;
		for (INT32 x = 0; x < 1024; x++) {
			#if defined(LSB_FIRST)
			((UINT32 *)output)[x] = table[input[x]];
			#else
			NamcosGlRasterPackVramPixel(input[x], output + x * 4);
			#endif
		}
	}
}

static inline void NamcosGlRasterUnpackVram(const UINT8 *source,
	UINT16 *destination)
{
	for (UINT32 y = 0; y < 1024; y++) {
		const UINT8 *row = source + (size_t)(1023 - y) * 1024 * 4;
		for (UINT32 x = 0; x < 1024; x++) {
			destination[(size_t)y * 1024 + x] =
				NamcosGlRasterUnpackVramPixel(row + x * 4);
		}
	}
}

static inline void NamcosGlRasterUnpackVramRect(const UINT8 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height)
{
	for (INT32 yy = 0; yy < height; yy++) {
		const UINT8 *row = source + (size_t)(height - 1 - yy) * width * 4;
		UINT16 *output = destination + (size_t)(y + yy) * 1024 + x;
		for (INT32 xx = 0; xx < width; xx++) {
			output[xx] = NamcosGlRasterUnpackVramPixel(row + xx * 4);
		}
	}
}

static inline INT32 NamcosGlRasterSigned11(UINT32 value)
{
	value &= 0x7ff;
	return (value & 0x400) ? (INT32)value - 0x800 : (INT32)value;
}

static inline void NamcosGlRasterDecodeColor(UINT32 value,
	NamcosGlRasterVertex *vertex)
{
	vertex->red = (UINT8)(value & 0xff);
	vertex->green = (UINT8)((value >> 8) & 0xff);
	vertex->blue = (UINT8)((value >> 16) & 0xff);
}

static inline void NamcosGlRasterDecodePosition(UINT32 value,
	const NamcosGlRasterState *state, NamcosGlRasterVertex *vertex)
{
	vertex->x = NamcosGlRasterSigned11(value) + state->drawOffsetX;
	vertex->y = NamcosGlRasterSigned11(value >> 16) + state->drawOffsetY;
}

static inline bool NamcosGlRasterDecodePacket(
	const NamcosGlRasterPacket *packet, NamcosGlRasterPrimitive *primitive)
{
	if (packet == NULL || primitive == NULL || packet->words == NULL ||
		packet->wordCount == 0) {
		return false;
	}

	memset(primitive, 0, sizeof(*primitive));
	const UINT32 *words = packet->words;
	const UINT8 command = packet->command;
	primitive->semiTransparent = (words[0] & 0x02000000) != 0;

	if (command == 0x02 && packet->wordCount >= 3) {
		primitive->type = NAMCOS_GL_RASTER_FILL;
		primitive->vertexCount = 1;
		primitive->vertex[0].x = words[1] & 0x3ff;
		primitive->vertex[0].y = (words[1] >> 16) & 0x3ff;
		NamcosGlRasterDecodeColor(words[0], &primitive->vertex[0]);
		primitive->width = words[2] & 0xffff;
		primitive->height = words[2] >> 16;
		primitive->semiTransparent = 0;
		return primitive->width > 0 && primitive->height > 0 &&
			primitive->vertex[0].x + primitive->width <= 1024 &&
			primitive->vertex[0].y + primitive->height <= 1024;
	}

	if (command >= 0x20 && command <= 0x23 && packet->wordCount >= 4) {
		primitive->type = NAMCOS_GL_RASTER_FLAT_POLYGON;
		primitive->vertexCount = 3;
		NamcosGlRasterDecodeColor(words[0], &primitive->vertex[0]);
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterDecodePosition(words[i + 1], &packet->state,
				&primitive->vertex[i]);
			if (i != 0) NamcosGlRasterDecodeColor(words[0], &primitive->vertex[i]);
		}
		return true;
	}

	if (command >= 0x28 && command <= 0x2b && packet->wordCount >= 5) {
		primitive->type = NAMCOS_GL_RASTER_FLAT_POLYGON;
		primitive->vertexCount = 4;
		for (UINT32 i = 0; i < 4; i++) {
			NamcosGlRasterDecodePosition(words[i + 1], &packet->state,
				&primitive->vertex[i]);
			NamcosGlRasterDecodeColor(words[0], &primitive->vertex[i]);
		}
		return true;
	}

	if (command >= 0x30 && command <= 0x33 && packet->wordCount >= 6) {
		primitive->type = NAMCOS_GL_RASTER_GOURAUD_POLYGON;
		primitive->vertexCount = 3;
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterDecodeColor(words[i * 2], &primitive->vertex[i]);
			NamcosGlRasterDecodePosition(words[i * 2 + 1], &packet->state,
				&primitive->vertex[i]);
		}
		return true;
	}

	if (command >= 0x38 && command <= 0x3b && packet->wordCount >= 8) {
		primitive->type = NAMCOS_GL_RASTER_GOURAUD_POLYGON;
		primitive->vertexCount = 4;
		for (UINT32 i = 0; i < 4; i++) {
			NamcosGlRasterDecodeColor(words[i * 2], &primitive->vertex[i]);
			NamcosGlRasterDecodePosition(words[i * 2 + 1], &packet->state,
				&primitive->vertex[i]);
		}
		return true;
	}

	const bool texturedPolygon =
		(command >= 0x24 && command <= 0x27) ||
		(command >= 0x2c && command <= 0x2f) ||
		(command >= 0x34 && command <= 0x37) ||
		(command >= 0x3c && command <= 0x3f);
	if (texturedPolygon) {
		const bool gouraud = command >= 0x34;
		const bool quad = (command >= 0x2c && command <= 0x2f) ||
			(command >= 0x3c && command <= 0x3f);
		const UINT32 points = quad ? 4 : 3;
		const UINT32 neededWords = gouraud ? (quad ? 12 : 9) :
			(quad ? 9 : 7);
		if (packet->wordCount < neededWords) return false;

		primitive->type = NAMCOS_GL_RASTER_TEXTURED_POLYGON;
		primitive->vertexCount = points;
		primitive->rawTexture = (words[0] & 0x01000000) != 0;
		primitive->gouraud = gouraud;
		for (UINT32 i = 0; i < points; i++) {
			const UINT32 colorIndex = gouraud ? i * 3 : 0;
			const UINT32 positionIndex = gouraud ? i * 3 + 1 : i * 2 + 1;
			const UINT32 textureIndex = positionIndex + 1;
			NamcosGlRasterDecodeColor(words[colorIndex], &primitive->vertex[i]);
			NamcosGlRasterDecodePosition(words[positionIndex], &packet->state,
				&primitive->vertex[i]);
			primitive->vertex[i].u = (UINT8)(words[textureIndex] & 0xff);
			primitive->vertex[i].v = (UINT8)((words[textureIndex] >> 8) & 0xff);
			if (i == 0) primitive->clut = words[textureIndex] >> 16;
			if (i == 1) primitive->tpage = words[textureIndex] >> 16;
		}
		return true;
	}

	if (command >= 0x40 && command <= 0x43 && packet->wordCount >= 3) {
		NamcosGlRasterVertex first;
		NamcosGlRasterVertex last;
		memset(&first, 0, sizeof(first));
		memset(&last, 0, sizeof(last));
		NamcosGlRasterDecodePosition(words[1], &packet->state, &first);
		NamcosGlRasterDecodePosition(words[2], &packet->state, &last);
		if (first.x != last.x && first.y != last.y) return false;

		primitive->type = NAMCOS_GL_RASTER_FLAT_RECTANGLE;
		primitive->vertexCount = 1;
		primitive->vertex[0] = first;
		NamcosGlRasterDecodeColor(words[0], &primitive->vertex[0]);
		primitive->width = 1;
		primitive->height = 1;
		if (first.x != last.x) {
			primitive->width = first.x < last.x ? last.x - first.x :
				first.x - last.x;
			if (first.x > last.x) primitive->vertex[0].x = last.x + 1;
		} else if (first.y != last.y) {
			primitive->height = first.y < last.y ? last.y - first.y :
				first.y - last.y;
			if (first.y > last.y) primitive->vertex[0].y = last.y + 1;
		}
		return true;
	}

	if (command >= 0x60 && command <= 0x63 && packet->wordCount >= 3) {
		primitive->width = words[2] & 0xffff;
		primitive->height = words[2] >> 16;
	} else if (command >= 0x68 && command <= 0x6b) {
		primitive->width = primitive->height = 1;
	} else if (command >= 0x70 && command <= 0x73) {
		primitive->width = primitive->height = 8;
	} else if (command >= 0x78 && command <= 0x7b) {
		primitive->width = primitive->height = 16;
	} else if (command >= 0x64 && command <= 0x67 &&
		packet->wordCount >= 4) {
		primitive->width = words[3] & 0xffff;
		primitive->height = words[3] >> 16;
	} else if (command >= 0x6c && command <= 0x6f) {
		primitive->width = primitive->height = 1;
	} else if (command >= 0x74 && command <= 0x77) {
		primitive->width = primitive->height = 8;
	} else if (command >= 0x7c && command <= 0x7f) {
		primitive->width = primitive->height = 16;
	} else {
		return false;
	}

	if (packet->wordCount < 2 || primitive->width <= 0 ||
		primitive->height <= 0) {
		return false;
	}
	const bool texturedRectangle = (command & 0x04) != 0;
	primitive->type = texturedRectangle ? NAMCOS_GL_RASTER_TEXTURED_RECTANGLE :
		NAMCOS_GL_RASTER_FLAT_RECTANGLE;
	primitive->vertexCount = 1;
	NamcosGlRasterDecodeColor(words[0], &primitive->vertex[0]);
	NamcosGlRasterDecodePosition(words[1], &packet->state,
		&primitive->vertex[0]);
	if (texturedRectangle) {
		primitive->rawTexture = (words[0] & 0x01000000) != 0;
		primitive->clut = words[2] >> 16;
		primitive->tpage = packet->state.tpage;
		primitive->vertex[0].u = (UINT8)(words[2] & 0xff);
		primitive->vertex[0].v = (UINT8)((words[2] >> 8) & 0xff);
	}
	return true;
}

static inline void NamcosGlRasterCopyDrawVertex(
	NamcosGlRasterDrawVertex *destination,
	const NamcosGlRasterVertex *source)
{
	destination->x = (float)source->x;
	destination->y = (float)source->y;
	destination->red = (float)source->red / 255.0f;
	destination->green = (float)source->green / 255.0f;
	destination->blue = (float)source->blue / 255.0f;
	destination->u = (float)source->u;
	destination->v = (float)source->v;
}

static inline bool NamcosGlRasterTriangleIsOversized(
	const NamcosGlRasterVertex *a, const NamcosGlRasterVertex *b,
	const NamcosGlRasterVertex *c)
{
	const NamcosGlRasterVertex *vertices[3] = { a, b, c };
	for (INT32 i = 0; i < 3; i++) {
		const NamcosGlRasterVertex *first = vertices[i];
		const NamcosGlRasterVertex *second = vertices[(i + 1) % 3];
		INT32 dx = first->x - second->x;
		INT32 dy = first->y - second->y;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		if (dx > 1023 || dy > 1023) return true;
	}
	return false;
}

static inline UINT32 NamcosGlRasterBuildColorTriangles(
	const NamcosGlRasterPrimitive *primitive,
	NamcosGlRasterDrawVertex *vertices, UINT32 capacity)
{
	if (primitive == NULL || vertices == NULL || capacity < 6 ||
		(primitive->type != NAMCOS_GL_RASTER_FLAT_POLYGON &&
		 primitive->type != NAMCOS_GL_RASTER_GOURAUD_POLYGON &&
		 primitive->type != NAMCOS_GL_RASTER_FLAT_RECTANGLE &&
		 primitive->type != NAMCOS_GL_RASTER_FILL)) {
		return 0;
	}

	if (primitive->type == NAMCOS_GL_RASTER_FLAT_RECTANGLE ||
		primitive->type == NAMCOS_GL_RASTER_FILL) {
		NamcosGlRasterVertex corners[4];
		for (UINT32 i = 0; i < 4; i++) corners[i] = primitive->vertex[0];
		corners[1].x += primitive->width;
		corners[2].y += primitive->height;
		corners[3].x += primitive->width;
		corners[3].y += primitive->height;
		static const UINT8 order[6] = { 0, 1, 2, 1, 3, 2 };
		for (UINT32 i = 0; i < 6; i++) {
			NamcosGlRasterCopyDrawVertex(&vertices[i], &corners[order[i]]);
		}
		return 6;
	}

	if (primitive->vertexCount == 3) {
		if (NamcosGlRasterTriangleIsOversized(&primitive->vertex[0],
			&primitive->vertex[1], &primitive->vertex[2])) return 0;
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterCopyDrawVertex(&vertices[i], &primitive->vertex[i]);
		}
		return 3;
	}
	if (primitive->vertexCount == 4) {
		const bool cull0 = NamcosGlRasterTriangleIsOversized(
			&primitive->vertex[0], &primitive->vertex[1], &primitive->vertex[2]);
		const bool cull1 = NamcosGlRasterTriangleIsOversized(
			&primitive->vertex[1], &primitive->vertex[2], &primitive->vertex[3]);
		if (cull0 && cull1) return 0;
		const UINT8 *order;
		UINT32 count;
		static const UINT8 firstTriangle[3] = { 0, 1, 2 };
		static const UINT8 secondTriangle[3] = { 1, 2, 3 };
		static const UINT8 quadOrder[6] = { 0, 1, 3, 0, 3, 2 };
		if (cull0) {
			order = secondTriangle;
			count = 3;
		} else if (cull1) {
			order = firstTriangle;
			count = 3;
		} else {
			order = quadOrder;
			count = 6;
		}
		for (UINT32 i = 0; i < count; i++) {
			NamcosGlRasterCopyDrawVertex(&vertices[i],
				&primitive->vertex[order[i]]);
		}
		return count;
	}
	return 0;
}

static inline UINT32 NamcosGlRasterBuildTexturedTriangles(
	const NamcosGlRasterPrimitive *primitive,
	NamcosGlRasterDrawVertex *vertices, UINT32 capacity)
{
	if (primitive == NULL || vertices == NULL || capacity < 6 ||
		(primitive->type != NAMCOS_GL_RASTER_TEXTURED_POLYGON &&
		 primitive->type != NAMCOS_GL_RASTER_TEXTURED_RECTANGLE)) {
		return 0;
	}
	if (primitive->type == NAMCOS_GL_RASTER_TEXTURED_RECTANGLE) {
		NamcosGlRasterVertex corners[4];
		for (UINT32 i = 0; i < 4; i++) corners[i] = primitive->vertex[0];
		corners[1].x += primitive->width;
		corners[1].u = (UINT8)(corners[1].u + primitive->width);
		corners[2].y += primitive->height;
		corners[2].v = (UINT8)(corners[2].v + primitive->height);
		corners[3].x += primitive->width;
		corners[3].y += primitive->height;
		corners[3].u = (UINT8)(corners[3].u + primitive->width);
		corners[3].v = (UINT8)(corners[3].v + primitive->height);
		static const UINT8 rectangleOrder[6] = { 0, 1, 2, 1, 3, 2 };
		for (UINT32 i = 0; i < 6; i++) {
			NamcosGlRasterCopyDrawVertex(&vertices[i],
				&corners[rectangleOrder[i]]);
		}
		return 6;
	}
	if (primitive->vertexCount != 3 && primitive->vertexCount != 4) return 0;

	if (primitive->vertexCount == 3) {
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterCopyDrawVertex(&vertices[i], &primitive->vertex[i]);
		}
		return 3;
	}

	static const UINT8 order[6] = { 0, 1, 2, 1, 3, 2 };
	for (UINT32 i = 0; i < 6; i++) {
		NamcosGlRasterCopyDrawVertex(&vertices[i],
			&primitive->vertex[order[i]]);
	}
	return 6;
}

static inline bool NamcosGlRasterGetDrawBounds(
	const NamcosGlRasterPacket *packet,
	const NamcosGlRasterPrimitive *primitive,
	INT32 *x1, INT32 *y1, INT32 *x2, INT32 *y2)
{
	if (packet == NULL || primitive == NULL || primitive->vertexCount == 0 ||
		x1 == NULL || y1 == NULL || x2 == NULL || y2 == NULL) {
		return false;
	}

	INT32 left = primitive->vertex[0].x;
	INT32 top = primitive->vertex[0].y;
	INT32 right = left;
	INT32 bottom = top;
	if (primitive->type == NAMCOS_GL_RASTER_FILL ||
		primitive->type == NAMCOS_GL_RASTER_FLAT_RECTANGLE ||
		primitive->type == NAMCOS_GL_RASTER_TEXTURED_RECTANGLE) {
		right += primitive->width - 1;
		bottom += primitive->height - 1;
	} else {
		for (UINT32 i = 1; i < primitive->vertexCount; i++) {
			if (primitive->vertex[i].x < left) left = primitive->vertex[i].x;
			if (primitive->vertex[i].x > right) right = primitive->vertex[i].x;
			if (primitive->vertex[i].y < top) top = primitive->vertex[i].y;
			if (primitive->vertex[i].y > bottom) bottom = primitive->vertex[i].y;
		}
	}

	if (primitive->type != NAMCOS_GL_RASTER_FILL) {
		if (left < (INT32)packet->state.drawX1) left = packet->state.drawX1;
		if (top < (INT32)packet->state.drawY1) top = packet->state.drawY1;
		if (right > (INT32)packet->state.drawX2) right = packet->state.drawX2;
		if (bottom > (INT32)packet->state.drawY2) bottom = packet->state.drawY2;
	}
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > 1023) right = 1023;
	if (bottom > 1023) bottom = 1023;
	if (right < left || bottom < top) return false;

	*x1 = left;
	*y1 = top;
	*x2 = right;
	*y2 = bottom;
	return true;
}

static inline void NamcosGlRasterGetTextureState(
	const NamcosGlRasterPacket *packet,
	const NamcosGlRasterPrimitive *primitive, UINT32 *state)
{
	const UINT32 tpage = primitive->tpage;
	UINT32 texx = (tpage & 0x0f) << 6;
	UINT32 texy;
	UINT32 mode;
	UINT32 interleaved = 0;
	if (packet->state.gpuType == 2) {
		texy = ((tpage & 0x10) << 4) | ((tpage & 0x800) >> 2);
		mode = (tpage >> 7) & 3;
	} else {
		texy = (tpage & 0x60) << 3;
		mode = (tpage >> 9) & 3;
		interleaved = (tpage & 0x2000) != 0;
	}
	texx += packet->state.textureWindowX >>
		(mode == 0 ? 2 : (mode == 1 ? 1 : 0));
	texy += packet->state.textureWindowY;

	state[0] = texx;
	state[1] = texy;
	state[2] = mode;
	state[3] = interleaved;
	state[4] = packet->state.textureWindowW;
	state[5] = packet->state.textureWindowH;
	state[6] = (primitive->clut & 0x3f) << 4;
	state[7] = (primitive->clut >> 6) & 0x3ff;
}

static inline bool NamcosGlRasterIsVramCommand(UINT8 command)
{
	return command == 0x02 ||
		(command >= 0x20 && command <= 0x7f) || command == 0x80 ||
		command == 0xa0 || command == 0xc0;
}

static inline bool NamcosGlRasterCanSubmitCommand(UINT8 command)
{
	// Keep texturing and semi-transparency on the software
	// rasterizer. Small GPU batches cost more than their VRAM synchronization.
	// Bit 0 controls raw texture modulation and has no effect on untextured
	// polygons, so both encodings can share the hardware path.
	if (command == 0x02) return true;
	switch (command & 0xfe) {
		case 0x20:
		case 0x30:
		case 0x40:
		case 0x60:
		case 0x68:
		case 0x70:
		case 0x78:
			return true;
	}
	return false;
}

#endif
