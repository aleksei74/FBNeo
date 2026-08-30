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
	NAMCOS_GL_RASTER_TEXTURED_RECTANGLE,
	NAMCOS_GL_RASTER_VRAM_COPY
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
	INT32 sourceX;
	INT32 sourceY;
	INT32 semiTransparent;
	INT32 rawTexture;
	INT32 gouraud;
	UINT32 clut;
	UINT32 tpage;
};

struct NamcosGlRasterDrawVertex
{
	INT16 x;
	INT16 y;
	UINT8 red;
	UINT8 green;
	UINT8 blue;
	UINT8 u;
	UINT8 v;
	UINT8 padding;
	INT16 textureState0;
	INT16 textureState1;
	INT16 textureState2;
	INT16 textureState3;
	UINT8 textureControl0;
	UINT8 textureControl1;
	UINT8 textureControl2;
	UINT8 textureControl3;
};

static inline void NamcosGlRasterSetVertexStates(
	NamcosGlRasterDrawVertex *vertices, UINT32 count, UINT8 state,
	const UINT32 *textureState, bool textured, bool vramCopy)
{
	NamcosGlRasterDrawVertex stateVertex;
	stateVertex.padding = state;
	stateVertex.textureState0 = 0;
	stateVertex.textureState1 = 0;
	stateVertex.textureState2 = 0;
	stateVertex.textureState3 = 0;
	stateVertex.textureControl0 = 0;
	stateVertex.textureControl1 = 0;
	stateVertex.textureControl2 = 0;
	stateVertex.textureControl3 = 0;
	if (vramCopy) {
		stateVertex.textureState0 = (INT16)textureState[0];
		stateVertex.textureState1 = (INT16)textureState[1];
		stateVertex.textureState2 = (INT16)textureState[2];
		stateVertex.textureState3 = (INT16)textureState[3];
	} else if (textured) {
		stateVertex.textureState0 = (INT16)textureState[0];
		stateVertex.textureState1 = (INT16)textureState[1];
		stateVertex.textureState2 = (INT16)textureState[6];
		stateVertex.textureState3 = (INT16)textureState[7];
		stateVertex.textureControl0 = (UINT8)textureState[2];
		stateVertex.textureControl1 = (UINT8)textureState[3];
		// Texture-window masks always have their low three bits set.  Send only
		// the variable high five bits so GLSL 1.20/ES 2.0 do not divide both
		// masks for every fragment.
		stateVertex.textureControl2 = (UINT8)(textureState[4] >> 3);
		stateVertex.textureControl3 = (UINT8)(textureState[5] >> 3);
	}
	const size_t stateOffset = offsetof(NamcosGlRasterDrawVertex, padding);
	const size_t stateBytes = sizeof(NamcosGlRasterDrawVertex) - stateOffset;
	for (UINT32 i = 0; i < count; i++) {
		memcpy((UINT8 *)(vertices + i) + stateOffset,
			(UINT8 *)&stateVertex + stateOffset, stateBytes);
	}
}

struct NamcosGlRasterRect
{
	INT32 x1;
	INT32 y1;
	INT32 x2;
	INT32 y2;
};

static const INT32 NAMCOS_GL_RASTER_DIRTY_RECTS = 32;
static const INT32 NAMCOS_GL_RASTER_UPLOAD_SPANS = 16;

struct NamcosGlRasterDirtyBounds
{
	INT32 x1;
	INT32 y1;
	INT32 x2;
	INT32 y2;
	NamcosGlRasterRect rects[NAMCOS_GL_RASTER_DIRTY_RECTS];
	UINT32 tileRows[32];
	INT32 rectCount;
	bool sparse;
	bool tilesFull;
	bool valid;

	NamcosGlRasterDirtyBounds() : x1(0), y1(0), x2(0), y2(0),
		rectCount(0), sparse(true), tilesFull(false), valid(false)
	{
		memset(tileRows, 0, sizeof(tileRows));
	}

	void Reset()
	{
		memset(tileRows, 0, sizeof(tileRows));
		rectCount = 0;
		sparse = true;
		tilesFull = false;
		valid = false;
	}

	void MarkTiles(INT32 left, INT32 top, INT32 right, INT32 bottom)
	{
		if (tilesFull) return;
		if (right < left || bottom < top || right < 0 || bottom < 0 ||
			left > 1023 || top > 1023) return;
		if (left < 0) left = 0;
		if (top < 0) top = 0;
		if (right > 1023) right = 1023;
		if (bottom > 1023) bottom = 1023;
		const INT32 tileX1 = left >> 5;
		const INT32 tileX2 = right >> 5;
		const INT32 tileY1 = top >> 5;
		const INT32 tileY2 = bottom >> 5;
		UINT32 mask = 0xffffffffU << tileX1;
		if (tileX2 < 31) mask &= (1U << (tileX2 + 1)) - 1;
		for (INT32 y = tileY1; y <= tileY2; y++) tileRows[y] |= mask;
		if (tileX1 == 0 && tileX2 == 31 && tileY1 == 0 && tileY2 == 31) {
			tilesFull = true;
		}
	}

	void RebuildTiles()
	{
		memset(tileRows, 0, sizeof(tileRows));
		tilesFull = false;
		for (INT32 i = 0; i < rectCount; i++) {
			MarkTiles(rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
		}
	}

	bool TilesIntersect(INT32 left, INT32 top, INT32 right, INT32 bottom) const
	{
		if (right < left || bottom < top || right < 0 || bottom < 0 ||
			left > 1023 || top > 1023) return false;
		if (left < 0) left = 0;
		if (top < 0) top = 0;
		if (right > 1023) right = 1023;
		if (bottom > 1023) bottom = 1023;
		const INT32 tileX1 = left >> 5;
		const INT32 tileX2 = right >> 5;
		const INT32 tileY1 = top >> 5;
		const INT32 tileY2 = bottom >> 5;
		UINT32 mask = 0xffffffffU << tileX1;
		if (tileX2 < 31) mask &= (1U << (tileX2 + 1)) - 1;
		for (INT32 y = tileY1; y <= tileY2; y++) {
			if (tileRows[y] & mask) return true;
		}
		return false;
	}

	void AppendSparseRect(const NamcosGlRasterRect &incoming)
	{
		if (rectCount < NAMCOS_GL_RASTER_DIRTY_RECTS) {
			rects[rectCount++] = incoming;
			return;
		}

		// Keep a conservative sparse union at capacity.  Collapsing directly to
		// the bounding rectangle can turn a few scattered draws into a nearly
		// full-VRAM readback.  Merge the pair with the least added area instead.
		INT32 bestFirst = 0;
		INT32 bestSecond = 1;
		INT64 bestExtra = 0x7fffffffffffffffLL;
		NamcosGlRasterRect bestMerged = rects[0];
		NamcosGlRasterRect candidates[NAMCOS_GL_RASTER_DIRTY_RECTS + 1];
		INT64 candidateAreas[NAMCOS_GL_RASTER_DIRTY_RECTS + 1];
		for (INT32 i = 0; i < rectCount; i++) candidates[i] = rects[i];
		candidates[rectCount] = incoming;
		for (INT32 i = 0; i <= rectCount; i++) {
			candidateAreas[i] =
				(INT64)(candidates[i].x2 - candidates[i].x1 + 1) *
				(candidates[i].y2 - candidates[i].y1 + 1);
		}
		for (INT32 first = 0; first <= rectCount; first++) {
			const NamcosGlRasterRect &firstRect = candidates[first];
			for (INT32 second = first + 1; second <= rectCount; second++) {
				const NamcosGlRasterRect &secondRect = candidates[second];
				NamcosGlRasterRect merged = firstRect;
				if (secondRect.x1 < merged.x1) merged.x1 = secondRect.x1;
				if (secondRect.y1 < merged.y1) merged.y1 = secondRect.y1;
				if (secondRect.x2 > merged.x2) merged.x2 = secondRect.x2;
				if (secondRect.y2 > merged.y2) merged.y2 = secondRect.y2;
				const INT64 mergedArea = (INT64)(merged.x2 - merged.x1 + 1) *
					(merged.y2 - merged.y1 + 1);
				const INT64 extra = mergedArea - candidateAreas[first] -
					candidateAreas[second];
				if (extra < bestExtra) {
					bestExtra = extra;
					bestFirst = first;
					bestSecond = second;
					bestMerged = merged;
				}
			}
		}

		if (bestSecond == rectCount) {
			rects[bestFirst] = bestMerged;
		} else {
			rects[bestFirst] = bestMerged;
			rects[bestSecond] = incoming;
		}
	}

	void Include(INT32 left, INT32 top, INT32 right, INT32 bottom)
	{
		MarkTiles(left, top, right, bottom);
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
		if (sparse && rectCount > 0) {
			const NamcosGlRasterRect &recent = rects[rectCount - 1];
			if (left >= recent.x1 && top >= recent.y1 && right <= recent.x2 &&
				bottom <= recent.y2) return;
		}
		// Only search older sparse rectangles when the new draw was already
		// inside the accumulated bounds. Draws extending those bounds cannot
		// be fully covered by an existing rectangle.
		if (sparse && left >= x1 && top >= y1 && right <= x2 && bottom <= y2) {
			for (INT32 i = rectCount - 2; i >= 0; i--) {
				const NamcosGlRasterRect &rect = rects[i];
				if (left >= rect.x1 && top >= rect.y1 && right <= rect.x2 &&
					bottom <= rect.y2) return;
			}
		}
		// A draw covering the accumulated bounds also covers every sparse
		// rectangle, so the exact dirty union becomes this one rectangle.
		if (left <= x1 && top <= y1 && right >= x2 && bottom >= y2) {
			x1 = left;
			y1 = top;
			x2 = right;
			y2 = bottom;
			rects[0] = { left, top, right, bottom };
			rectCount = 1;
			sparse = true;
			return;
		}
		const bool disjointFromBounds = right + 1 < x1 || x2 + 1 < left ||
			bottom + 1 < y1 || y2 + 1 < top;
		if (left < x1) x1 = left;
		if (top < y1) y1 = top;
		if (right > x2) x2 = right;
		if (bottom > y2) y2 = bottom;
		if (!sparse) return;
		if (disjointFromBounds) {
			const NamcosGlRasterRect incoming = { left, top, right, bottom };
			AppendSparseRect(incoming);
			return;
		}

		NamcosGlRasterRect merged = { left, top, right, bottom };
		INT64 mergedArea = (INT64)(right - left + 1) * (bottom - top + 1);
		for (INT32 i = 0; i < rectCount;) {
			const NamcosGlRasterRect &rect = rects[i];
			if (merged.x2 + 1 < rect.x1 || rect.x2 + 1 < merged.x1 ||
				merged.y2 + 1 < rect.y1 || rect.y2 + 1 < merged.y1) {
				i++;
				continue;
			}
			NamcosGlRasterRect candidate = merged;
			if (rect.x1 < candidate.x1) candidate.x1 = rect.x1;
			if (rect.y1 < candidate.y1) candidate.y1 = rect.y1;
			if (rect.x2 > candidate.x2) candidate.x2 = rect.x2;
			if (rect.y2 > candidate.y2) candidate.y2 = rect.y2;
			const INT64 rectArea = (INT64)(rect.x2 - rect.x1 + 1) *
				(rect.y2 - rect.y1 + 1);
			const INT64 candidateArea = (INT64)(candidate.x2 - candidate.x1 + 1) *
				(candidate.y2 - candidate.y1 + 1);
			if (candidateArea > (mergedArea + rectArea) * 2) {
				i++;
				continue;
			}
			merged = candidate;
			mergedArea = candidateArea;
			rects[i] = rects[--rectCount];
			i = 0;
		}
		AppendSparseRect(merged);
	}

	INT32 GetReadbackRects(NamcosGlRasterRect *output, INT32 capacity,
		INT64 callCost = 16384) const
	{
		// Readback stalls the GPU, so avoiding a call is worth more than the
		// equivalent texture-to-texture copy operation.
		return GetCopyRects(output, capacity, callCost);
	}

	INT32 GetCopyRects(NamcosGlRasterRect *output, INT32 capacity,
		INT64 callCost = 4096) const
	{
		if (!valid || output == NULL || capacity <= 0) return 0;
		if (!sparse || rectCount <= 1 || rectCount > capacity) {
			output[0] = { x1, y1, x2, y2 };
			return 1;
		}
		if (rectCount == 2) {
			const INT64 firstArea = (INT64)(rects[0].x2 - rects[0].x1 + 1) *
				(rects[0].y2 - rects[0].y1 + 1);
			const INT64 secondArea = (INT64)(rects[1].x2 - rects[1].x1 + 1) *
				(rects[1].y2 - rects[1].y1 + 1);
			NamcosGlRasterRect merged = rects[0];
			if (rects[1].x1 < merged.x1) merged.x1 = rects[1].x1;
			if (rects[1].y1 < merged.y1) merged.y1 = rects[1].y1;
			if (rects[1].x2 > merged.x2) merged.x2 = rects[1].x2;
			if (rects[1].y2 > merged.y2) merged.y2 = rects[1].y2;
			const INT64 mergedArea = (INT64)(merged.x2 - merged.x1 + 1) *
				(merged.y2 - merged.y1 + 1);
			if (mergedArea + callCost < firstArea + secondArea + callCost * 2) {
				output[0] = merged;
				return 1;
			}
			output[0] = rects[0];
			output[1] = rects[1];
			return 2;
		}

		NamcosGlRasterRect working[NAMCOS_GL_RASTER_DIRTY_RECTS];
		NamcosGlRasterRect best[NAMCOS_GL_RASTER_DIRTY_RECTS];
		INT64 workingArea[NAMCOS_GL_RASTER_DIRTY_RECTS];
		memcpy(working, rects,
			(size_t)rectCount * sizeof(NamcosGlRasterRect));
		memcpy(best, rects,
			(size_t)rectCount * sizeof(NamcosGlRasterRect));
		INT32 count = rectCount;
		INT32 bestCount = count;
		INT64 area = 0;
		for (INT32 i = 0; i < count; i++) {
			workingArea[i] = (INT64)(working[i].x2 - working[i].x1 + 1) *
				(working[i].y2 - working[i].y1 + 1);
			area += workingArea[i];
		}
		// A copy call is considerably more expensive than a few thousand pixels,
		// especially on GLES drivers. Track the cheapest greedy merge level.
		INT64 bestCost = area + (INT64)count * callCost;
		while (count > 1) {
			INT32 bestFirst = 0;
			INT32 bestSecond = 1;
			INT64 bestExtra = 0x7fffffffffffffffLL;
			INT64 bestMergedArea = workingArea[0];
			NamcosGlRasterRect bestMerged = working[0];
			for (INT32 first = 0; first < count - 1; first++) {
				for (INT32 second = first + 1; second < count; second++) {
					NamcosGlRasterRect merged = working[first];
					if (working[second].x1 < merged.x1) merged.x1 = working[second].x1;
					if (working[second].y1 < merged.y1) merged.y1 = working[second].y1;
					if (working[second].x2 > merged.x2) merged.x2 = working[second].x2;
					if (working[second].y2 > merged.y2) merged.y2 = working[second].y2;
					const INT64 mergedArea =
						(INT64)(merged.x2 - merged.x1 + 1) *
						(merged.y2 - merged.y1 + 1);
					const INT64 extra = mergedArea - workingArea[first] -
						workingArea[second];
					if (extra < bestExtra) {
						bestExtra = extra;
						bestFirst = first;
						bestSecond = second;
						bestMerged = merged;
						bestMergedArea = mergedArea;
					}
				}
			}
			working[bestFirst] = bestMerged;
			workingArea[bestFirst] = bestMergedArea;
			--count;
			working[bestSecond] = working[count];
			workingArea[bestSecond] = workingArea[count];
			area += bestExtra;
			const INT64 cost = area + (INT64)count * callCost;
			if (cost < bestCost) {
				bestCost = cost;
				bestCount = count;
				memcpy(best, working,
					(size_t)count * sizeof(NamcosGlRasterRect));
			}
		}
		memcpy(output, best,
			(size_t)bestCount * sizeof(NamcosGlRasterRect));
		return bestCount;
	}

	bool Intersects(INT32 left, INT32 top, INT32 right, INT32 bottom) const
	{
		if (!valid || right < x1 || x2 < left || bottom < y1 || y2 < top)
			return false;
		if (!sparse || rectCount <= 1 ||
			(left <= x1 && top <= y1 && right >= x2 && bottom >= y2)) return true;
		if (!TilesIntersect(left, top, right, bottom)) return false;
		for (INT32 i = rectCount - 1; i >= 0; i--) {
			if (right >= rects[i].x1 && rects[i].x2 >= left &&
				bottom >= rects[i].y1 && rects[i].y2 >= top) return true;
		}
		return false;
	}

	void Exclude(INT32 left, INT32 top, INT32 right, INT32 bottom)
	{
		if (!Intersects(left, top, right, bottom)) return;
		if (left <= x1 && top <= y1 && right >= x2 && bottom >= y2) {
			Reset();
			return;
		}

		NamcosGlRasterRect source[NAMCOS_GL_RASTER_DIRTY_RECTS];
		const INT32 sourceCount = sparse ? rectCount : 1;
		if (sparse) {
			memcpy(source, rects,
				(size_t)sourceCount * sizeof(NamcosGlRasterRect));
		} else {
			source[0].x1 = x1;
			source[0].y1 = y1;
			source[0].x2 = x2;
			source[0].y2 = y2;
		}

		NamcosGlRasterRect remaining[NAMCOS_GL_RASTER_DIRTY_RECTS];
		INT32 remainingCount = 0;
		for (INT32 i = 0; i < sourceCount; i++) {
			const NamcosGlRasterRect &rect = source[i];
			const INT32 clipX1 = left > rect.x1 ? left : rect.x1;
			const INT32 clipY1 = top > rect.y1 ? top : rect.y1;
			const INT32 clipX2 = right < rect.x2 ? right : rect.x2;
			const INT32 clipY2 = bottom < rect.y2 ? bottom : rect.y2;
			if (clipX1 > clipX2 || clipY1 > clipY2) {
				if (remainingCount == NAMCOS_GL_RASTER_DIRTY_RECTS) return;
				remaining[remainingCount++] = rect;
				continue;
			}

			NamcosGlRasterRect split[4];
			INT32 splitCount = 0;
			if (rect.y1 < clipY1) {
				split[splitCount++] = { rect.x1, rect.y1, rect.x2, clipY1 - 1 };
			}
			if (clipY2 < rect.y2) {
				split[splitCount++] = { rect.x1, clipY2 + 1, rect.x2, rect.y2 };
			}
			if (rect.x1 < clipX1) {
				split[splitCount++] = { rect.x1, clipY1, clipX1 - 1, clipY2 };
			}
			if (clipX2 < rect.x2) {
				split[splitCount++] = { clipX2 + 1, clipY1, rect.x2, clipY2 };
			}
			if (remainingCount + splitCount > NAMCOS_GL_RASTER_DIRTY_RECTS) return;
			for (INT32 part = 0; part < splitCount; part++) {
				remaining[remainingCount++] = split[part];
			}
		}

		if (remainingCount == 0) {
			Reset();
			return;
		}
		memcpy(rects, remaining,
			(size_t)remainingCount * sizeof(NamcosGlRasterRect));
		rectCount = remainingCount;
		sparse = true;
		x1 = rects[0].x1;
		y1 = rects[0].y1;
		x2 = rects[0].x2;
		y2 = rects[0].y2;
		for (INT32 i = 1; i < rectCount; i++) {
			if (rects[i].x1 < x1) x1 = rects[i].x1;
			if (rects[i].y1 < y1) y1 = rects[i].y1;
			if (rects[i].x2 > x2) x2 = rects[i].x2;
			if (rects[i].y2 > y2) y2 = rects[i].y2;
		}
		RebuildTiles();
		valid = true;
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
			(size_t)rows * sizeof(rowGeneration[0]));
		valid = true;
	}

	void SetAndRememberRange(UINT64 *destination, UINT64 generation,
		INT32 first, INT32 rows)
	{
		if (destination == NULL || first < 0 || rows <= 0 ||
			first + rows > 1024) return;
		for (INT32 row = first; row < first + rows; row++) {
			destination[row] = generation;
			rowGeneration[row] = generation;
		}
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
	INT32 *boundingFirst, INT32 *boundingRows, INT32 callCostRows = 8)
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

	// Retain the largest unchanged gaps as transfer boundaries.  This gives
	// the minimum uploaded row count for the bounded number of GL calls;
	// filling the span array in row order can otherwise merge a large tail.
	const INT32 maxSpans = capacity < NAMCOS_GL_RASTER_UPLOAD_SPANS ?
		capacity : NAMCOS_GL_RASTER_UPLOAD_SPANS;
	INT32 selectedGapFirst[NAMCOS_GL_RASTER_UPLOAD_SPANS - 1];
	INT32 selectedGapRows[NAMCOS_GL_RASTER_UPLOAD_SPANS - 1];
	INT32 selectedCount = 0;
	INT32 row = first;
	while (row < last) {
		while (row < last && tracker->rowGeneration[row] !=
			rowGeneration[row]) row++;
		const INT32 gapFirst = row;
		while (row < last && tracker->rowGeneration[row] ==
			rowGeneration[row]) row++;
		if (row == last) break;
		const INT32 gapRows = row - gapFirst;
		if (gapRows <= callCostRows || maxSpans <= 1) continue;

		INT32 insert = selectedCount;
		if (selectedCount == maxSpans - 1) {
			if (gapRows <= selectedGapRows[selectedCount - 1]) continue;
			insert--;
		} else {
			selectedCount++;
		}
		while (insert > 0 && gapRows > selectedGapRows[insert - 1]) {
			selectedGapRows[insert] = selectedGapRows[insert - 1];
			selectedGapFirst[insert] = selectedGapFirst[insert - 1];
			insert--;
		}
		selectedGapRows[insert] = gapRows;
		selectedGapFirst[insert] = gapFirst;
	}

	// The selected gaps are ranked by size above. Sort the small retained set
	// by row and construct spans directly instead of scanning all 1024 rows a
	// second time.
	for (INT32 i = 1; i < selectedCount; i++) {
		const INT32 gapFirst = selectedGapFirst[i];
		const INT32 gapRows = selectedGapRows[i];
		INT32 insert = i;
		while (insert > 0 && gapFirst < selectedGapFirst[insert - 1]) {
			selectedGapFirst[insert] = selectedGapFirst[insert - 1];
			selectedGapRows[insert] = selectedGapRows[insert - 1];
			insert--;
		}
		selectedGapFirst[insert] = gapFirst;
		selectedGapRows[insert] = gapRows;
	}
	INT32 spanCount = 0;
	INT32 spanFirst = first;
	for (INT32 gap = 0; gap < selectedCount; gap++) {
		spans[spanCount].firstRow = spanFirst;
		spans[spanCount].rowCount = selectedGapFirst[gap] - spanFirst;
		spanCount++;
		spanFirst = selectedGapFirst[gap] + selectedGapRows[gap];
	}
	spans[spanCount].firstRow = spanFirst;
	spans[spanCount].rowCount = last - spanFirst;
	spanCount++;

	INT32 uploadRows = *boundingRows;
	for (INT32 gap = 0; gap < selectedCount; gap++) {
		uploadRows -= selectedGapRows[gap];
	}

	if (spanCount > 1 && uploadRows + spanCount * callCostRows >=
		*boundingRows + callCostRows) return -1;
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

static inline void NamcosGlRasterCopyColor(NamcosGlRasterVertex *destination,
	const NamcosGlRasterVertex *source)
{
	destination->red = source->red;
	destination->green = source->green;
	destination->blue = source->blue;
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

	if (command == 0x80 && packet->wordCount >= 4) {
		const INT32 sourceX = words[1] & 0x3ff;
		const INT32 sourceY = (words[1] >> 16) & 0x3ff;
		const INT32 destinationX = words[2] & 0x3ff;
		const INT32 destinationY = (words[2] >> 16) & 0x3ff;
		const INT32 width = words[3] & 0xffff;
		const INT32 height = words[3] >> 16;
		if (width <= 0 || height <= 0 || sourceX + width > 1024 ||
			sourceY + height > 1024 || destinationX + width > 1024 ||
			destinationY + height > 1024) return false;
		const bool overlaps = sourceX < destinationX + width &&
			destinationX < sourceX + width && sourceY < destinationY + height &&
			destinationY < sourceY + height;
		if (overlaps) return false;
		primitive->type = NAMCOS_GL_RASTER_VRAM_COPY;
		primitive->vertexCount = 1;
		primitive->vertex[0].x = destinationX;
		primitive->vertex[0].y = destinationY;
		primitive->vertex[0].red = 0;
		primitive->vertex[0].green = 0;
		primitive->vertex[0].blue = 0;
		primitive->sourceX = sourceX;
		primitive->sourceY = sourceY;
		primitive->width = width;
		primitive->height = height;
		primitive->semiTransparent = 0;
		return true;
	}

	if (command >= 0x20 && command <= 0x23 && packet->wordCount >= 4) {
		primitive->type = NAMCOS_GL_RASTER_FLAT_POLYGON;
		primitive->vertexCount = 3;
		NamcosGlRasterDecodeColor(words[0], &primitive->vertex[0]);
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterDecodePosition(words[i + 1], &packet->state,
				&primitive->vertex[i]);
			if (i != 0) NamcosGlRasterCopyColor(&primitive->vertex[i],
				&primitive->vertex[0]);
		}
		return true;
	}

	if (command >= 0x28 && command <= 0x2b && packet->wordCount >= 5) {
		primitive->type = NAMCOS_GL_RASTER_FLAT_POLYGON;
		primitive->vertexCount = 4;
		NamcosGlRasterDecodeColor(words[0], &primitive->vertex[0]);
		for (UINT32 i = 0; i < 4; i++) {
			NamcosGlRasterDecodePosition(words[i + 1], &packet->state,
				&primitive->vertex[i]);
			if (i != 0) NamcosGlRasterCopyColor(&primitive->vertex[i],
				&primitive->vertex[0]);
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
		if (!gouraud) NamcosGlRasterDecodeColor(words[0], &primitive->vertex[0]);
		for (UINT32 i = 0; i < points; i++) {
			const UINT32 colorIndex = gouraud ? i * 3 : 0;
			const UINT32 positionIndex = gouraud ? i * 3 + 1 : i * 2 + 1;
			const UINT32 textureIndex = positionIndex + 1;
			if (gouraud) {
				NamcosGlRasterDecodeColor(words[colorIndex], &primitive->vertex[i]);
			} else if (i != 0) {
				NamcosGlRasterCopyColor(&primitive->vertex[i],
					&primitive->vertex[0]);
			}
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
	destination->x = (INT16)source->x;
	destination->y = (INT16)source->y;
	destination->red = source->red;
	destination->green = source->green;
	destination->blue = source->blue;
	destination->u = source->u;
	destination->v = source->v;
	destination->padding = 0;
}

static inline void NamcosGlRasterCopyColorDrawVertex(
	NamcosGlRasterDrawVertex *destination,
	const NamcosGlRasterVertex *source)
{
	destination->x = (INT16)source->x;
	destination->y = (INT16)source->y;
	destination->red = source->red;
	destination->green = source->green;
	destination->blue = source->blue;
	destination->u = 0;
	destination->v = 0;
	destination->padding = 0;
}

static inline void NamcosGlRasterCopyBuiltVertex(
	NamcosGlRasterDrawVertex *destination,
	const NamcosGlRasterDrawVertex *source)
{
	memcpy(destination, source,
		offsetof(NamcosGlRasterDrawVertex, textureState0));
}

static inline bool NamcosGlRasterEdgeIsOversized(
	const NamcosGlRasterVertex *first, const NamcosGlRasterVertex *second)
{
	INT32 dx = first->x - second->x;
	INT32 dy = first->y - second->y;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	// The PSX GPU rejects polygons whose edge reaches 1024 pixels
	// horizontally or 512 pixels vertically.
	return dx >= 1024 || dy >= 512;
}

static inline bool NamcosGlRasterTriangleIsOversized(
	const NamcosGlRasterVertex *a, const NamcosGlRasterVertex *b,
	const NamcosGlRasterVertex *c)
{
	return NamcosGlRasterEdgeIsOversized(a, b) ||
		NamcosGlRasterEdgeIsOversized(b, c) ||
		NamcosGlRasterEdgeIsOversized(c, a);
}

static inline UINT32 NamcosGlRasterBuildColorTriangles(
	const NamcosGlRasterPrimitive *primitive,
	NamcosGlRasterDrawVertex *vertices, UINT32 capacity)
{
	if (primitive == NULL || vertices == NULL || capacity < 6 ||
		(primitive->type != NAMCOS_GL_RASTER_FLAT_POLYGON &&
		 primitive->type != NAMCOS_GL_RASTER_GOURAUD_POLYGON &&
		 primitive->type != NAMCOS_GL_RASTER_FLAT_RECTANGLE &&
		 primitive->type != NAMCOS_GL_RASTER_FILL &&
		 primitive->type != NAMCOS_GL_RASTER_VRAM_COPY)) {
		return 0;
	}

	if (primitive->type == NAMCOS_GL_RASTER_FLAT_RECTANGLE ||
		primitive->type == NAMCOS_GL_RASTER_FILL ||
		primitive->type == NAMCOS_GL_RASTER_VRAM_COPY) {
		NamcosGlRasterCopyColorDrawVertex(&vertices[0], &primitive->vertex[0]);
		NamcosGlRasterCopyBuiltVertex(&vertices[1], &vertices[0]);
		vertices[1].x += (INT16)primitive->width;
		NamcosGlRasterCopyBuiltVertex(&vertices[2], &vertices[0]);
		vertices[2].y += (INT16)primitive->height;
		NamcosGlRasterCopyBuiltVertex(&vertices[3], &vertices[1]);
		NamcosGlRasterCopyBuiltVertex(&vertices[4], &vertices[1]);
		vertices[4].y += (INT16)primitive->height;
		NamcosGlRasterCopyBuiltVertex(&vertices[5], &vertices[2]);
		return 6;
	}

	if (primitive->vertexCount == 3) {
		if (NamcosGlRasterTriangleIsOversized(&primitive->vertex[0],
			&primitive->vertex[1], &primitive->vertex[2])) return 0;
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterCopyColorDrawVertex(&vertices[i], &primitive->vertex[i]);
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
		if (cull0) {
			order = secondTriangle;
			count = 3;
		} else if (cull1) {
			order = firstTriangle;
			count = 3;
		} else {
			NamcosGlRasterCopyColorDrawVertex(&vertices[0],
				&primitive->vertex[0]);
			NamcosGlRasterCopyColorDrawVertex(&vertices[1],
				&primitive->vertex[1]);
			NamcosGlRasterCopyColorDrawVertex(&vertices[2],
				&primitive->vertex[3]);
			NamcosGlRasterCopyColorDrawVertex(&vertices[5],
				&primitive->vertex[2]);
			NamcosGlRasterCopyBuiltVertex(&vertices[3], &vertices[0]);
			NamcosGlRasterCopyBuiltVertex(&vertices[4], &vertices[2]);
			return 6;
		}
		for (UINT32 i = 0; i < count; i++) {
			NamcosGlRasterCopyColorDrawVertex(&vertices[i],
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
		NamcosGlRasterCopyDrawVertex(&vertices[0], &primitive->vertex[0]);
		NamcosGlRasterCopyBuiltVertex(&vertices[1], &vertices[0]);
		vertices[1].x += (INT16)primitive->width;
		vertices[1].u = (UINT8)(vertices[1].u + primitive->width);
		NamcosGlRasterCopyBuiltVertex(&vertices[2], &vertices[0]);
		vertices[2].y += (INT16)primitive->height;
		vertices[2].v = (UINT8)(vertices[2].v + primitive->height);
		NamcosGlRasterCopyBuiltVertex(&vertices[3], &vertices[1]);
		NamcosGlRasterCopyBuiltVertex(&vertices[4], &vertices[1]);
		vertices[4].y += (INT16)primitive->height;
		vertices[4].v = (UINT8)(vertices[4].v + primitive->height);
		NamcosGlRasterCopyBuiltVertex(&vertices[5], &vertices[2]);
		return 6;
	}
	if (primitive->vertexCount != 3 && primitive->vertexCount != 4) return 0;

	if (primitive->vertexCount == 3) {
		if (NamcosGlRasterTriangleIsOversized(&primitive->vertex[0],
			&primitive->vertex[1], &primitive->vertex[2])) return 0;
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterCopyDrawVertex(&vertices[i], &primitive->vertex[i]);
		}
		return 3;
	}
	const bool cull0 = NamcosGlRasterTriangleIsOversized(
		&primitive->vertex[0], &primitive->vertex[1], &primitive->vertex[2]);
	const bool cull1 = NamcosGlRasterTriangleIsOversized(
		&primitive->vertex[1], &primitive->vertex[2], &primitive->vertex[3]);
	if (cull0 && cull1) return 0;
	if (cull0 || cull1) {
		static const UINT8 firstTriangle[3] = { 0, 1, 2 };
		static const UINT8 secondTriangle[3] = { 1, 2, 3 };
		const UINT8 *order = cull0 ? secondTriangle : firstTriangle;
		for (UINT32 i = 0; i < 3; i++) {
			NamcosGlRasterCopyDrawVertex(&vertices[i],
				&primitive->vertex[order[i]]);
		}
		return 3;
	}

	NamcosGlRasterCopyDrawVertex(&vertices[0], &primitive->vertex[0]);
	NamcosGlRasterCopyDrawVertex(&vertices[1], &primitive->vertex[1]);
	NamcosGlRasterCopyDrawVertex(&vertices[2], &primitive->vertex[2]);
	NamcosGlRasterCopyDrawVertex(&vertices[4], &primitive->vertex[3]);
	NamcosGlRasterCopyBuiltVertex(&vertices[3], &vertices[1]);
	NamcosGlRasterCopyBuiltVertex(&vertices[5], &vertices[2]);
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
		primitive->type == NAMCOS_GL_RASTER_TEXTURED_RECTANGLE ||
		primitive->type == NAMCOS_GL_RASTER_VRAM_COPY) {
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

	if (primitive->type != NAMCOS_GL_RASTER_FILL &&
		primitive->type != NAMCOS_GL_RASTER_VRAM_COPY) {
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

static inline bool NamcosGlRasterDirtyIntersectsWrapped(
	const NamcosGlRasterDirtyBounds *dirty, INT32 x, INT32 y,
	INT32 width, INT32 height)
{
	if (dirty == NULL || !dirty->valid || width <= 0 || height <= 0)
		return false;
	x &= 0x3ff;
	y &= 0x3ff;
	if (width > 1024) width = 1024;
	if (height > 1024) height = 1024;
	const INT32 firstWidth = width < 1024 - x ? width : 1024 - x;
	const INT32 firstHeight = height < 1024 - y ? height : 1024 - y;
	if (dirty->Intersects(x, y, x + firstWidth - 1,
		y + firstHeight - 1)) return true;
	if (width > firstWidth && dirty->Intersects(0, y,
		width - firstWidth - 1, y + firstHeight - 1)) return true;
	if (height > firstHeight && dirty->Intersects(x, 0,
		x + firstWidth - 1, height - firstHeight - 1)) return true;
	return width > firstWidth && height > firstHeight &&
		dirty->Intersects(0, 0, width - firstWidth - 1,
			height - firstHeight - 1);
}

static inline INT32 NamcosGlRasterBuildWrappedReadRects(
	const NamcosGlRasterDirtyBounds *dirty, INT32 x, INT32 y,
	INT32 width, INT32 height, NamcosGlRasterRect *output, INT32 capacity,
	INT64 readbackCallCost = 16384)
{
	if (dirty == NULL || !dirty->valid || output == NULL || capacity < 4 ||
		width <= 0 || height <= 0) return 0;
	x &= 0x3ff;
	y &= 0x3ff;
	if (width > 1024) width = 1024;
	if (height > 1024) height = 1024;
	const INT32 firstWidth = width < 1024 - x ? width : 1024 - x;
	const INT32 firstHeight = height < 1024 - y ? height : 1024 - y;
	const INT32 widths[2] = { firstWidth, width - firstWidth };
	const INT32 heights[2] = { firstHeight, height - firstHeight };
	const INT32 xs[2] = { x, 0 };
	const INT32 ys[2] = { y, 0 };
	NamcosGlRasterRect fallback[4];
	INT32 fallbackCount = 0;
	INT64 fallbackArea = 0;
	for (INT32 yPart = 0; yPart < 2; yPart++) {
		for (INT32 xPart = 0; xPart < 2; xPart++) {
			if (widths[xPart] <= 0 || heights[yPart] <= 0) continue;
			NamcosGlRasterRect rect;
			rect.x1 = xs[xPart];
			rect.y1 = ys[yPart];
			rect.x2 = rect.x1 + widths[xPart] - 1;
			rect.y2 = rect.y1 + heights[yPart] - 1;
			if (!dirty->Intersects(rect.x1, rect.y1, rect.x2, rect.y2)) continue;
			fallback[fallbackCount++] = rect;
			fallbackArea += (INT64)widths[xPart] * heights[yPart];
		}
	}
	if (fallbackCount == 0) return 0;
	if (fallbackCount == 1 && dirty->rectCount <= 1) {
		// Most texture dependencies are one non-wrapped request against one
		// dirty draw. Its exact intersection cannot cost more than the fallback.
		const NamcosGlRasterRect source = dirty->sparse ? dirty->rects[0] :
			NamcosGlRasterRect{ dirty->x1, dirty->y1, dirty->x2, dirty->y2 };
		output[0].x1 = fallback[0].x1 > source.x1 ? fallback[0].x1 : source.x1;
		output[0].y1 = fallback[0].y1 > source.y1 ? fallback[0].y1 : source.y1;
		output[0].x2 = fallback[0].x2 < source.x2 ? fallback[0].x2 : source.x2;
		output[0].y2 = fallback[0].y2 < source.y2 ? fallback[0].y2 : source.y2;
		return 1;
	}

	// Clip the exact dirty union to the requested wrapped pieces before the
	// readback cost model merges rectangles.  Merging globally first can make
	// a small C0 transfer inherit a large bounding area outside its request.
	NamcosGlRasterDirtyBounds requestedDirty;
	const INT32 dirtyCount = dirty->sparse ? dirty->rectCount : 1;
	for (INT32 request = 0; request < fallbackCount; request++) {
		for (INT32 source = 0; source < dirtyCount; source++) {
			NamcosGlRasterRect dirtyRect;
			if (dirty->sparse) {
				dirtyRect = dirty->rects[source];
			} else {
				dirtyRect.x1 = dirty->x1;
				dirtyRect.y1 = dirty->y1;
				dirtyRect.x2 = dirty->x2;
				dirtyRect.y2 = dirty->y2;
			}
			NamcosGlRasterRect rect;
			rect.x1 = fallback[request].x1 > dirtyRect.x1 ?
				fallback[request].x1 : dirtyRect.x1;
			rect.y1 = fallback[request].y1 > dirtyRect.y1 ?
				fallback[request].y1 : dirtyRect.y1;
			rect.x2 = fallback[request].x2 < dirtyRect.x2 ?
				fallback[request].x2 : dirtyRect.x2;
			rect.y2 = fallback[request].y2 < dirtyRect.y2 ?
				fallback[request].y2 : dirtyRect.y2;
			if (rect.x2 < rect.x1 || rect.y2 < rect.y1) continue;
			requestedDirty.Include(rect.x1, rect.y1, rect.x2, rect.y2);
		}
	}
	const INT32 sparseCount = requestedDirty.GetReadbackRects(output,
		capacity, readbackCallCost);
	INT64 sparseArea = 0;
	for (INT32 i = 0; i < sparseCount; i++) {
		sparseArea += (INT64)(output[i].x2 - output[i].x1 + 1) *
			(output[i].y2 - output[i].y1 + 1);
	}
	if (sparseCount > 0 && sparseArea + (INT64)sparseCount * readbackCallCost <
		fallbackArea + (INT64)fallbackCount * readbackCallCost) return sparseCount;
	if (fallbackCount > capacity) return 0;
	memcpy(output, fallback, (size_t)fallbackCount * sizeof(fallback[0]));
	return fallbackCount;
}

static inline INT32 NamcosGlRasterBuildSelectiveCopyRects(
	const NamcosGlRasterDirtyBounds *dirty, INT32 x, INT32 y,
	INT32 width, INT32 height, NamcosGlRasterRect *output, INT32 capacity,
	bool *copyAll, INT64 copyCallCost = 65536)
{
	if (copyAll != NULL) *copyAll = false;
	if (dirty == NULL || !dirty->valid || output == NULL || capacity < 4)
		return 0;
	const INT32 selectedCount = NamcosGlRasterBuildWrappedReadRects(dirty,
		x, y, width, height, output, capacity, copyCallCost);
	if (selectedCount <= 0) return 0;

	NamcosGlRasterRect full[NAMCOS_GL_RASTER_DIRTY_RECTS];
	const INT32 fullCount = dirty->GetCopyRects(full,
		NAMCOS_GL_RASTER_DIRTY_RECTS, copyCallCost);
	INT64 selectedCost = (INT64)selectedCount * copyCallCost;
	for (INT32 i = 0; i < selectedCount; i++) {
		selectedCost += (INT64)(output[i].x2 - output[i].x1 + 1) *
			(output[i].y2 - output[i].y1 + 1);
	}
	INT64 fullCost = (INT64)fullCount * copyCallCost;
	for (INT32 i = 0; i < fullCount; i++) {
		fullCost += (INT64)(full[i].x2 - full[i].x1 + 1) *
			(full[i].y2 - full[i].y1 + 1);
	}
	if (selectedCost < fullCost) return selectedCount;
	if (fullCount > capacity) return 0;
	memcpy(output, full, (size_t)fullCount * sizeof(full[0]));
	if (copyAll != NULL) *copyAll = true;
	return fullCount;
}

static inline bool NamcosGlRasterTextureStateReadsDirty(
	const UINT32 *state, const NamcosGlRasterDirtyBounds *dirty)
{
	if (state == NULL || dirty == NULL || !dirty->valid)
		return false;
	const INT32 mode = (INT32)state[2];
	const bool interleaved = state[3] != 0;
	const INT32 shift = mode == 0 ? 2 : (mode == 1 ? 1 : 0);
	const INT32 windowWidth = (INT32)(state[4] & 0xff) + 1;
	const INT32 windowHeight = (INT32)(state[5] & 0xff) + 1;
	const bool swizzled = interleaved && mode < 2;
	const INT32 textureWords = swizzled ? (mode == 0 ? 64 : 128) :
		((windowWidth + (1 << shift) - 1) >> shift);
	const INT32 textureRows = swizzled ? 256 : windowHeight;
	if (NamcosGlRasterDirtyIntersectsWrapped(dirty, (INT32)state[0],
		(INT32)state[1], textureWords, textureRows)) return true;
	if (mode < 2) {
		const INT32 clutWords = mode == 0 ? 16 : 256;
		if (NamcosGlRasterDirtyIntersectsWrapped(dirty, (INT32)state[6],
			(INT32)state[7], clutWords, 1)) return true;
	}
	return false;
}

static inline void NamcosGlRasterMaskTextureRange(INT32 *minimum,
	INT32 *maximum, UINT32 windowMask)
{
	const INT32 mask = (INT32)(windowMask & 0xff);
	const INT32 removed = (~mask) & 0xff;
	const INT32 lowestRemoved = removed & -removed;
	if (removed == 0 || ((*minimum & -lowestRemoved) ==
		(*maximum & -lowestRemoved))) {
		*minimum &= mask;
		*maximum &= mask;
	} else {
		*minimum = 0;
		*maximum = mask;
	}
}

struct NamcosGlRasterReadRegion
{
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static inline INT32 NamcosGlRasterGetTexturePrimitiveReadRegions(
	const UINT32 *state, const NamcosGlRasterPrimitive *primitive,
	NamcosGlRasterReadRegion *regions, INT32 capacity)
{
	if (state == NULL || primitive == NULL || regions == NULL || capacity < 2)
		return 0;
	const INT32 mode = (INT32)state[2];
	if (mode < 0 || mode > 2) return 0;
	if (state[3] != 0 && mode < 2) {
		// The interleaved PSX layouts swizzle a complete 256x256 texel page
		// into 64x256 words at 4bpp or 128x256 words at 8bpp.
		regions[0].x = (INT32)state[0];
		regions[0].y = (INT32)state[1];
		regions[0].width = mode == 0 ? 64 : 128;
		regions[0].height = 256;
		regions[1].x = (INT32)state[6];
		regions[1].y = (INT32)state[7];
		regions[1].width = mode == 0 ? 16 : 256;
		regions[1].height = 1;
		return 2;
	}

	INT32 minU;
	INT32 maxU;
	INT32 minV;
	INT32 maxV;
	if (primitive->type == NAMCOS_GL_RASTER_TEXTURED_RECTANGLE) {
		if (primitive->vertexCount == 0 || primitive->width <= 0 ||
			primitive->height <= 0 || primitive->width > 256 ||
			primitive->height > 256) {
			return 0;
		}
		minU = primitive->vertex[0].u;
		minV = primitive->vertex[0].v;
		maxU = minU + primitive->width - 1;
		maxV = minV + primitive->height - 1;
		if (maxU > 255 || maxV > 255) {
			return 0;
		}
	} else if (primitive->type == NAMCOS_GL_RASTER_TEXTURED_POLYGON &&
		(primitive->vertexCount == 3 || primitive->vertexCount == 4)) {
		minU = maxU = primitive->vertex[0].u;
		minV = maxV = primitive->vertex[0].v;
		for (UINT32 i = 1; i < primitive->vertexCount; i++) {
			const INT32 u = primitive->vertex[i].u;
			const INT32 v = primitive->vertex[i].v;
			if (u < minU) minU = u;
			if (u > maxU) maxU = u;
			if (v < minV) minV = v;
			if (v > maxV) maxV = v;
		}
	} else {
		return 0;
	}
	NamcosGlRasterMaskTextureRange(&minU, &maxU, state[4]);
	NamcosGlRasterMaskTextureRange(&minV, &maxV, state[5]);

	const INT32 shift = mode == 0 ? 2 : (mode == 1 ? 1 : 0);
	const INT32 firstWord = minU >> shift;
	const INT32 lastWord = maxU >> shift;
	regions[0].x = (INT32)state[0] + firstWord;
	regions[0].y = (INT32)state[1] + minV;
	regions[0].width = lastWord - firstWord + 1;
	regions[0].height = maxV - minV + 1;
	if (mode < 2) {
		regions[1].x = (INT32)state[6];
		regions[1].y = (INT32)state[7];
		regions[1].width = mode == 0 ? 16 : 256;
		regions[1].height = 1;
		return 2;
	}
	return 1;
}

static inline bool NamcosGlRasterTexturePrimitiveReadsDirty(
	const UINT32 *state, const NamcosGlRasterPrimitive *primitive,
	const NamcosGlRasterDirtyBounds *dirty)
{
	if (state == NULL || primitive == NULL || dirty == NULL ||
		!dirty->valid) return false;
	NamcosGlRasterReadRegion regions[2];
	const INT32 count = NamcosGlRasterGetTexturePrimitiveReadRegions(state,
		primitive, regions, 2);
	if (count == 0) return NamcosGlRasterTextureStateReadsDirty(state, dirty);
	for (INT32 i = 0; i < count; i++) {
		if (NamcosGlRasterDirtyIntersectsWrapped(dirty, regions[i].x,
			regions[i].y, regions[i].width, regions[i].height)) return true;
	}
	return false;
}

static inline INT32 NamcosGlRasterBuildTextureSelectiveCopyRects(
	const UINT32 *state, const NamcosGlRasterPrimitive *primitive,
	const NamcosGlRasterDirtyBounds *dirty, NamcosGlRasterRect *output,
	INT32 capacity, bool *copyAll, bool *dependencyKnown,
	INT64 copyCallCost = 65536)
{
	if (copyAll != NULL) *copyAll = false;
	if (dependencyKnown != NULL) *dependencyKnown = false;
	if (dirty == NULL || !dirty->valid || output == NULL ||
		capacity < NAMCOS_GL_RASTER_DIRTY_RECTS) return 0;
	NamcosGlRasterReadRegion regions[2];
	const INT32 regionCount = NamcosGlRasterGetTexturePrimitiveReadRegions(
		state, primitive, regions, 2);
	if (regionCount == 0) return 0;
	if (dependencyKnown != NULL) *dependencyKnown = true;

	NamcosGlRasterDirtyBounds selectedDirty;
	NamcosGlRasterRect clipped[NAMCOS_GL_RASTER_DIRTY_RECTS];
	for (INT32 region = 0; region < regionCount; region++) {
		const INT32 count = NamcosGlRasterBuildWrappedReadRects(dirty,
			regions[region].x, regions[region].y, regions[region].width,
			regions[region].height, clipped, NAMCOS_GL_RASTER_DIRTY_RECTS,
			copyCallCost);
		for (INT32 i = 0; i < count; i++) {
			selectedDirty.Include(clipped[i].x1, clipped[i].y1,
				clipped[i].x2, clipped[i].y2);
		}
	}
	if (!selectedDirty.valid) return 0;
	const INT32 selectedCount = selectedDirty.GetCopyRects(output, capacity,
		copyCallCost);
	NamcosGlRasterRect full[NAMCOS_GL_RASTER_DIRTY_RECTS];
	const INT32 fullCount = dirty->GetCopyRects(full,
		NAMCOS_GL_RASTER_DIRTY_RECTS, copyCallCost);
	INT64 selectedCost = (INT64)selectedCount * copyCallCost;
	for (INT32 i = 0; i < selectedCount; i++) {
		selectedCost += (INT64)(output[i].x2 - output[i].x1 + 1) *
			(output[i].y2 - output[i].y1 + 1);
	}
	INT64 fullCost = (INT64)fullCount * copyCallCost;
	for (INT32 i = 0; i < fullCount; i++) {
		fullCost += (INT64)(full[i].x2 - full[i].x1 + 1) *
			(full[i].y2 - full[i].y1 + 1);
	}
	if (selectedCost < fullCost) return selectedCount;
	if (fullCount > capacity) return 0;
	memcpy(output, full, (size_t)fullCount * sizeof(full[0]));
	if (copyAll != NULL) *copyAll = true;
	return fullCount;
}

static inline bool NamcosGlRasterIsVramCommand(UINT8 command)
{
	return command == 0x02 ||
		(command >= 0x20 && command <= 0x7f) || command == 0x80 ||
		command == 0xa0 || command == 0xc0;
}

static inline bool NamcosGlRasterCanSubmitCommand(UINT8 command)
{
	// Bits 0 and 1 select raw texture modulation and semi-transparency. The
	// hardware path handles both through the synchronized VRAM sample texture,
	// so all four encodings share one primitive decoder. VRAM transfers and
	// unsupported line shapes remain on the ordered software path.
	if (command == 0x02) return true;
	if (command == 0x80) return true;
	switch (command & 0xfc) {
		case 0x20:
		case 0x24:
		case 0x28:
		case 0x2c:
		case 0x30:
		case 0x34:
		case 0x38:
		case 0x3c:
		case 0x40:
		case 0x60:
		case 0x64:
		case 0x68:
		case 0x6c:
		case 0x70:
		case 0x74:
		case 0x78:
		case 0x7c:
			return true;
	}
	return false;
}

#endif
