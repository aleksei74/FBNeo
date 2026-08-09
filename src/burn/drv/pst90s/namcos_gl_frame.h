#ifndef FBNEO_NAMCOS_GL_FRAME_H
#define FBNEO_NAMCOS_GL_FRAME_H

#include "namcos_gl_raster.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "namcos_poly_threads.h"

static const UINT32 NAMCOS_GL_RASTER_STREAM_VERTICES = 6 * 1024;

struct NamcosGlPackVramContext
{
	const UINT16 *source;
	UINT8 *destination;
	#if defined(LSB_FIRST)
	const UINT32 *table;
	#endif
};

static void NamcosGlPackVramRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlPackVramContext *context = (NamcosGlPackVramContext *)opaque;
	for (INT32 y = begin; y < end; y++) {
		UINT8 *row = context->destination + (size_t)(1023 - y) * 1024 * 4;
		const UINT16 *source = context->source + (size_t)y * 1024;
		for (INT32 x = 0; x < 1024; x++) {
			#if defined(LSB_FIRST)
			((UINT32 *)row)[x] = context->table[source[x]];
			#else
			NamcosGlRasterPackVramPixel(source[x], row + x * 4);
			#endif
		}
	}
}

static void NamcosGlPackVramParallel(const UINT16 *source, UINT8 *destination,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlPackVramContext context;
	context.source = source;
	context.destination = destination;
	#if defined(LSB_FIRST)
	context.table = NamcosGlRasterGetPackTable();
	#endif
	if (threadPool != NULL) {
		threadPool->ParallelForWork(1024, 1024LL * 1024, 131072,
			NamcosGlPackVramRows, &context);
	} else {
		NamcosGlPackVramRows(&context, 0, 1024);
	}
}

struct NamcosGlUnpackVramContext
{
	const UINT8 *source;
	UINT16 *destination;
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static void NamcosGlUnpackVramRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlUnpackVramContext *context = (NamcosGlUnpackVramContext *)opaque;
	for (INT32 yy = begin; yy < end; yy++) {
		const UINT8 *row = context->source +
			(size_t)(context->height - 1 - yy) * context->width * 4;
		UINT16 *output = context->destination +
			(size_t)(context->y + yy) * 1024 + context->x;
		for (INT32 xx = 0; xx < context->width; xx++) {
			output[xx] = NamcosGlRasterUnpackVramPixel(row + xx * 4);
		}
	}
}

static void NamcosGlUnpackVramRectParallel(const UINT8 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlUnpackVramContext context;
	context.source = source;
	context.destination = destination;
	context.x = x;
	context.y = y;
	context.width = width;
	context.height = height;
	const INT64 work = (INT64)width * height;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(height, work, 32768,
			NamcosGlUnpackVramRows, &context);
	} else {
		NamcosGlUnpackVramRows(&context, 0, height);
	}
}

#if defined(FBNEO_NAMCOS_OPENGL_ES)

struct NamcosGlReadConvertContext
{
	const UINT8 *source;
	UINT8 *destination;
	UINT16 *indexedDestination;
	const UINT16 *readTable;
	const UINT32 *directReadTable;
	const UINT32 *palette;
	INT32 width;
	INT32 sourcePitch;
	INT32 destinationPitch;
	INT32 destinationBytes;
	INT32 redOffset;
	INT32 blueOffset;
};

static void NamcosGlConvertNative16Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlReadConvertContext *context = (NamcosGlReadConvertContext *)opaque;

	for (INT32 y = begin; y < end; y++) {
		const UINT16 *source = (const UINT16 *)(context->source +
			(size_t)y * context->sourcePitch);
		if (context->destinationBytes == 4) {
			UINT32 *destination = (UINT32 *)(context->destination +
				y * context->destinationPitch);
			for (INT32 x = 0; x < context->width; x++) {
				destination[x] = context->directReadTable[source[x]];
			}
		} else if (context->destinationBytes == 2) {
			UINT16 *destination = (UINT16 *)(context->destination +
				y * context->destinationPitch);
			for (INT32 x = 0; x < context->width; x++) {
				destination[x] = (UINT16)context->directReadTable[source[x]];
			}
		} else if (context->destinationBytes == 3) {
			UINT8 *destination = context->destination + y * context->destinationPitch;
			for (INT32 x = 0; x < context->width; x++, destination += 3) {
				const UINT32 color = context->directReadTable[source[x]];
				destination[0] = (UINT8)color;
				destination[1] = (UINT8)(color >> 8);
				destination[2] = (UINT8)(color >> 16);
			}
		} else {
			UINT16 *destination = context->indexedDestination + y * context->width;
			for (INT32 x = 0; x < context->width; x++) {
				destination[x] = context->readTable[source[x]];
			}
		}
	}
}

static void NamcosGlConvertRgbaRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlReadConvertContext *context = (NamcosGlReadConvertContext *)opaque;

	for (INT32 y = begin; y < end; y++) {
		const UINT8 *source = context->source + (size_t)y * context->sourcePitch;
		if (context->destinationBytes != 0) {
			UINT8 *destination = context->destination + y * context->destinationPitch;
			for (INT32 x = 0; x < context->width; x++, source += 4) {
				const UINT16 color = (UINT16)((source[context->redOffset] >> 3) |
					((source[1] & 0xf8) << 2) |
					((source[context->blueOffset] & 0xf8) << 7));
				const UINT32 outputColor = context->palette[color];
				if (context->destinationBytes == 4) {
					*(UINT32 *)destination = outputColor;
				} else if (context->destinationBytes == 2) {
					*(UINT16 *)destination = (UINT16)outputColor;
				} else {
					destination[0] = (UINT8)outputColor;
					destination[1] = (UINT8)(outputColor >> 8);
					destination[2] = (UINT8)(outputColor >> 16);
				}
				destination += context->destinationBytes;
			}
		} else {
			UINT16 *destination = context->indexedDestination + y * context->width;
			for (INT32 x = 0; x < context->width; x++, source += 4) {
				destination[x] = (UINT16)((source[context->redOffset] >> 3) |
					((source[1] & 0xf8) << 2) |
					((source[context->blueOffset] & 0xf8) << 7));
			}
		}
	}
}

static void NamcosGlConvertReadRows(const NamcosFrameConvertContext *frame,
	NamcosPolyThreadCallback callback, NamcosGlReadConvertContext *context)
{
	const INT64 work = (INT64)frame->outputWidth * frame->outputHeight;
	if (frame->threadPool != NULL) {
		frame->threadPool->ParallelForWork(frame->outputHeight, work, 32768,
			callback, context);
	} else {
		callback(context, 0, frame->outputHeight);
	}
}

#endif

static void NamcosPackRgb24Row(const NamcosFrameConvertContext *frame,
	UINT8 *output, INT32 y)
{
	const UINT8 *vramBytes = (const UINT8 *)frame->vram;
	const INT32 rowBytes = frame->sourceWidth * 3;
	const INT32 sourceOffset = (frame->displayX & 0x3ff) * 2;
	const UINT8 *sourceRow = vramBytes +
		(((frame->displayY + y) & 0x3ff) * 2048);
	if (sourceOffset + rowBytes <= 2048) {
		memcpy(output + (size_t)y * rowBytes, sourceRow + sourceOffset, rowBytes);
		return;
	}
	UINT8 *destination = output + (size_t)y * rowBytes;
	INT32 offset = sourceOffset;
	INT32 remaining = rowBytes;
	while (remaining > 0) {
		const INT32 count = remaining < 2048 - offset ? remaining : 2048 - offset;
		memcpy(destination, sourceRow + offset, count);
		destination += count;
		remaining -= count;
		offset = 0;
	}
}

struct NamcosPackRgb24Context
{
	const NamcosFrameConvertContext *frame;
	UINT8 *output;
	const UINT8 *selectedRows;
};

static void NamcosPackRgb24Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosPackRgb24Context *context = (NamcosPackRgb24Context *)opaque;
	for (INT32 y = begin; y < end; y++) {
		if (context->selectedRows != NULL && !context->selectedRows[y]) continue;
		NamcosPackRgb24Row(context->frame, context->output, y);
	}
}

static void NamcosPackRgb24Selected(const NamcosFrameConvertContext *frame,
	UINT8 *output, const UINT8 *selectedRows, INT32 selectedCount)
{
	if (selectedCount <= 0) return;
	NamcosPackRgb24Context context;
	context.frame = frame;
	context.output = output;
	context.selectedRows = selectedRows;
	const INT64 work = (INT64)frame->sourceWidth * selectedCount * 3;
	if (frame->threadPool != NULL) {
		frame->threadPool->ParallelForWork(frame->sourceHeight, work, 196608,
			NamcosPackRgb24Rows, &context);
	} else {
		NamcosPackRgb24Rows(&context, 0, frame->sourceHeight);
	}
}

static void NamcosPackRgb24(const NamcosFrameConvertContext *frame, UINT8 *output)
{
	NamcosPackRgb24Context context;
	context.frame = frame;
	context.output = output;
	context.selectedRows = NULL;
	const INT64 work = (INT64)frame->sourceWidth * frame->sourceHeight * 3;
	if (frame->threadPool != NULL) {
		frame->threadPool->ParallelForWork(frame->sourceHeight, work, 196608,
			NamcosPackRgb24Rows, &context);
	} else {
		NamcosPackRgb24Rows(&context, 0, frame->sourceHeight);
	}
}

struct NamcosCopyPackedRowsContext
{
	const UINT8 *source;
	UINT8 *destination;
	const UINT8 *selectedRows;
	size_t rowBytes;
};

static void NamcosCopyPackedRowsWorker(void *opaque, INT32 begin, INT32 end)
{
	NamcosCopyPackedRowsContext *context =
		(NamcosCopyPackedRowsContext *)opaque;
	for (INT32 y = begin; y < end; y++) {
		if (context->selectedRows != NULL && !context->selectedRows[y]) continue;
		memcpy(context->destination + (size_t)y * context->rowBytes,
			context->source + (size_t)y * context->rowBytes,
			context->rowBytes);
	}
}

static void NamcosCopyPackedRows(const UINT8 *source, UINT8 *destination,
	size_t rowBytes, INT32 rows, const UINT8 *selectedRows,
	INT32 selectedCount, NamcosPolyThreadPool *threadPool)
{
	if (selectedCount <= 0) return;

	NamcosCopyPackedRowsContext context;
	context.source = source;
	context.destination = destination;
	context.selectedRows = selectedRows;
	context.rowBytes = rowBytes;
	const INT64 work = (INT64)rowBytes * selectedCount;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(rows, work, 196608,
			NamcosCopyPackedRowsWorker, &context);
	} else {
		NamcosCopyPackedRowsWorker(&context, 0, rows);
	}
}

#if defined(_WIN32) || defined(FBNEO_NAMCOS_OPENGL_ES2)

struct NamcosCopy16RowsContext
{
	const UINT16 *source;
	UINT16 *destination;
	const UINT8 *selectedRows;
	INT32 width;
};

static void NamcosCopy16RowsWorker(void *opaque, INT32 begin, INT32 end)
{
	NamcosCopy16RowsContext *context = (NamcosCopy16RowsContext *)opaque;
	for (INT32 y = begin; y < end; y++) {
		if (context->selectedRows != NULL && !context->selectedRows[y]) continue;
		memcpy(context->destination + (size_t)y * context->width,
			context->source + (size_t)y * 1024,
			(size_t)context->width * sizeof(UINT16));
	}
}

static void NamcosCopy16RowsSelected(const UINT16 *source, UINT16 *destination,
	INT32 width, INT32 rows, const UINT8 *selectedRows, INT32 selectedCount,
	NamcosPolyThreadPool *threadPool)
{
	if (selectedCount <= 0) return;

	NamcosCopy16RowsContext context;
	context.source = source;
	context.destination = destination;
	context.selectedRows = selectedRows;
	context.width = width;
	const INT64 work = (INT64)width * selectedCount;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(rows, work, 65536,
			NamcosCopy16RowsWorker, &context);
	} else {
		NamcosCopy16RowsWorker(&context, 0, rows);
	}
}

static void NamcosCopy16Rows(const UINT16 *source, UINT16 *destination,
	INT32 width, INT32 rows, NamcosPolyThreadPool *threadPool)
{
	NamcosCopy16RowsSelected(source, destination, width, rows, NULL, rows,
		threadPool);
}

#endif

struct NamcosFrameUploadKey
{
	UINT64 generation;
	INT32 displayX;
	INT32 displayY;
	INT32 sourceWidth;
	INT32 sourceHeight;
	INT32 outputHeight;
	INT32 cropTop;
	INT32 cropHeight;
	INT32 rgb24;
	INT32 vertical;
};

struct NamcosFrameOutputKey
{
	UINT64 generation;
	const void *destination;
	const UINT32 *palette;
	INT32 pitch;
	INT32 bytes;
	INT32 outputWidth;
	INT32 outputHeight;
	INT32 sourceWidth;
	INT32 sourceHeight;
	INT32 displayX;
	INT32 displayY;
	INT32 cropTop;
	INT32 cropHeight;
	INT32 outputShiftX;
	INT32 rgb24;
	INT32 vertical;
	INT32 verticalReconstruct2x;
};

static void NamcosFrameGetObservedRows(const NamcosFrameConvertContext *frame,
	INT32 *firstRow, INT32 *rowCount)
{
	INT32 first = 0;
	INT32 last = frame->sourceHeight;

	if (!frame->vertical && frame->outputHeight > 0 && frame->cropHeight > 0) {
		first = (INT32)(((INT64)frame->cropTop * frame->sourceHeight) /
			frame->outputHeight);
		last = (INT32)((((INT64)(frame->cropTop + frame->cropHeight) *
			frame->sourceHeight) + frame->outputHeight - 1) /
			frame->outputHeight);

		// Keep one guard row on each side for fractional texture coordinates.
		if (first > 0) first--;
		if (last < frame->sourceHeight) last++;
		if (first < 0) first = 0;
		if (last > frame->sourceHeight) last = frame->sourceHeight;
	}

	*firstRow = first;
	*rowCount = last > first ? last - first : 0;
}

static bool NamcosFrameOutputMatches(const NamcosFrameOutputKey *key,
	const NamcosFrameConvertContext *frame, const void *destination,
	INT32 pitch, INT32 bytes, const UINT32 *palette,
	const UINT64 *rowGeneration)
{
	if (!frame->allowOutputReuse || destination == NULL ||
		key->destination != destination || key->palette != palette ||
		key->pitch != pitch || key->bytes != bytes ||
		key->outputWidth != frame->outputWidth ||
		key->outputHeight != frame->outputHeight ||
		key->sourceWidth != frame->sourceWidth ||
		key->sourceHeight != frame->sourceHeight ||
		key->displayX != frame->displayX || key->displayY != frame->displayY ||
		key->cropTop != frame->cropTop || key->cropHeight != frame->cropHeight ||
		key->outputShiftX != frame->outputShiftX || key->rgb24 != frame->rgb24 ||
		key->vertical != frame->vertical ||
		key->verticalReconstruct2x != frame->verticalReconstruct2x) {
		return false;
	}
	if (key->generation == frame->vramGeneration) return true;
	if (frame->vramRowGeneration == NULL || frame->sourceHeight > 1024) return false;
	INT32 firstRow;
	INT32 rowCount;
	NamcosFrameGetObservedRows(frame, &firstRow, &rowCount);
	for (INT32 y = firstRow; y < firstRow + rowCount; y++) {
		if (rowGeneration[y] !=
			frame->vramRowGeneration[(frame->displayY + y) & 0x3ff]) return false;
	}
	return true;
}

static void NamcosFrameRememberOutput(NamcosFrameOutputKey *key,
	const NamcosFrameConvertContext *frame, const void *destination,
	INT32 pitch, INT32 bytes, const UINT32 *palette, UINT64 *rowGeneration)
{
	key->generation = frame->vramGeneration;
	key->destination = destination;
	key->palette = palette;
	key->pitch = pitch;
	key->bytes = bytes;
	key->outputWidth = frame->outputWidth;
	key->outputHeight = frame->outputHeight;
	key->sourceWidth = frame->sourceWidth;
	key->sourceHeight = frame->sourceHeight;
	key->displayX = frame->displayX;
	key->displayY = frame->displayY;
	key->cropTop = frame->cropTop;
	key->cropHeight = frame->cropHeight;
	key->outputShiftX = frame->outputShiftX;
	key->rgb24 = frame->rgb24;
	key->vertical = frame->vertical;
	key->verticalReconstruct2x = frame->verticalReconstruct2x;
	if (frame->vramRowGeneration != NULL) {
		INT32 firstRow;
		INT32 rowCount;
		NamcosFrameGetObservedRows(frame, &firstRow, &rowCount);
		for (INT32 y = firstRow; y < firstRow + rowCount && y < 1024; y++) {
			rowGeneration[y] =
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff];
		}
	}
}

static bool NamcosFrameUploadMatches(const NamcosFrameUploadKey *key,
	const NamcosFrameConvertContext *frame)
{
	return key->generation == frame->vramGeneration &&
		key->displayX == frame->displayX && key->displayY == frame->displayY &&
		key->sourceWidth == frame->sourceWidth &&
		key->sourceHeight == frame->sourceHeight &&
		key->outputHeight == frame->outputHeight &&
		key->cropTop == frame->cropTop && key->cropHeight == frame->cropHeight &&
		key->rgb24 == frame->rgb24 && key->vertical == frame->vertical;
}

static void NamcosFrameRememberUpload(NamcosFrameUploadKey *key,
	const NamcosFrameConvertContext *frame)
{
	key->generation = frame->vramGeneration;
	key->displayX = frame->displayX;
	key->displayY = frame->displayY;
	key->sourceWidth = frame->sourceWidth;
	key->sourceHeight = frame->sourceHeight;
	key->outputHeight = frame->outputHeight;
	key->cropTop = frame->cropTop;
	key->cropHeight = frame->cropHeight;
	key->rgb24 = frame->rgb24;
	key->vertical = frame->vertical;
}

static bool NamcosFrameGetMergedDirtySpan(const UINT8 *dirtyRows, INT32 rows,
	INT32 changedRows, INT32 runCount, INT32 *start, INT32 *count)
{
	if (dirtyRows == NULL || start == NULL || count == NULL ||
		changedRows <= 1 || runCount <= 1) return false;

	INT32 first = 0;
	while (first < rows && !dirtyRows[first]) first++;
	INT32 last = rows - 1;
	while (last >= first && !dirtyRows[last]) last--;
	const INT32 span = last - first + 1;
	if (span <= 0 || span > changedRows * 2) return false;

	*start = first;
	*count = span;
	return true;
}

static bool NamcosStringContainsNoCase(const char *text, const char *needle)
{
	if (text == NULL || needle == NULL || *needle == 0) return false;

	for (; *text != 0; text++) {
		const char *left = text;
		const char *right = needle;
		while (*left != 0 && *right != 0) {
			char a = *left++;
			char b = *right++;
			if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
			if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
			if (a != b) break;
		}
		if (*right == 0) return true;
	}

	return false;
}

static bool NamcosOpenGLRendererIsHardware(const char *renderer)
{
	static const char *softwareRenderers[] = {
		"gdi generic",
		"microsoft basic render driver",
		"software rasterizer",
		"swiftshader",
		"llvmpipe",
		"softpipe",
		"swrast",
		"lavapipe"
	};

	if (renderer == NULL || *renderer == 0) return false;
	for (UINT32 i = 0; i < sizeof(softwareRenderers) / sizeof(softwareRenderers[0]); i++) {
		if (NamcosStringContainsNoCase(renderer, softwareRenderers[i])) return false;
	}

	return true;
}

static bool NamcosPaletteIsRgb565(const UINT32 *palette)
{
	return palette != NULL &&
		(UINT16)palette[0x001f] == 0xf800 &&
		(UINT16)palette[0x03e0] == 0x07e0 &&
		(UINT16)palette[0x7c00] == 0x001f;
}

static bool NamcosFrameOutputIsUniform(const UINT8 *output, INT32 width,
	INT32 height, INT32 pitch, INT32 bytesPerPixel, bool ignoreAlpha)
{
	if (output == NULL || width <= 0 || height <= 0 || bytesPerPixel <= 0) {
		return false;
	}

	INT32 blackPixels = 0;
	INT32 whitePixels = 0;
	for (INT32 y = 0; y < height; y++) {
		const UINT8 *row = output + (size_t)y * pitch;
		for (INT32 x = 0; x < width; x++) {
			const UINT8 *pixel = row + (size_t)x * bytesPerPixel;
			if (bytesPerPixel == 2) {
				UINT16 first;
				memcpy(&first, pixel, sizeof(first));
				const INT32 red = ignoreAlpha ? (first & 0x1f) : ((first >> 11) & 0x1f);
				const INT32 green = ignoreAlpha ? ((first >> 5) & 0x1f) : ((first >> 5) & 0x3f);
				const INT32 blue = ignoreAlpha ? ((first >> 10) & 0x1f) : (first & 0x1f);
				if (red <= 1 && green <= 1 && blue <= 1) blackPixels++;
				if (red >= 30 && green >= (ignoreAlpha ? 30 : 62) && blue >= 30) whitePixels++;
			} else {
				const INT32 red = pixel[2];
				const INT32 green = pixel[1];
				const INT32 blue = pixel[0];
				if (red <= 8 && green <= 8 && blue <= 8) blackPixels++;
				if (red >= 247 && green >= 247 && blue >= 247) whitePixels++;
			}
		}
	}
	const INT32 pixels = width * height;
	return blackPixels >= pixels - (pixels / 50) ||
		whitePixels >= pixels - (pixels / 50);
}

#if defined(_WIN32)

#include <windows.h>
#include <GL/gl.h>

#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80e1
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_BGR
#define GL_BGR 0x80e0
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0cf2
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_COMPILE_STATUS 0x8b81
#define GL_LINK_STATUS 0x8b82
#define GL_INFO_LOG_LENGTH 0x8b84
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88e4
#define GL_DYNAMIC_DRAW 0x88e8
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8d40
#define GL_COLOR_ATTACHMENT0 0x8ce0
#define GL_FRAMEBUFFER_COMPLETE 0x8cd5
#endif
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

typedef HGLRC (WINAPI *NamcosWglCreateContextAttribsProc)(HDC dc,
	HGLRC shareContext, const INT32 *attributes);

typedef GLuint (APIENTRY *NamcosCreateShaderProc)(GLenum type);
typedef void (APIENTRY *NamcosShaderSourceProc)(GLuint shader, GLsizei count,
	const char **string, const GLint *length);
typedef void (APIENTRY *NamcosCompileShaderProc)(GLuint shader);
typedef void (APIENTRY *NamcosGetShaderivProc)(GLuint shader, GLenum pname,
	GLint *params);
typedef void (APIENTRY *NamcosGetShaderInfoLogProc)(GLuint shader,
	GLsizei bufferSize, GLsizei *length, char *infoLog);
typedef void (APIENTRY *NamcosDeleteShaderProc)(GLuint shader);
typedef GLuint (APIENTRY *NamcosCreateProgramProc)();
typedef void (APIENTRY *NamcosAttachShaderProc)(GLuint program, GLuint shader);
typedef void (APIENTRY *NamcosLinkProgramProc)(GLuint program);
typedef void (APIENTRY *NamcosGetProgramivProc)(GLuint program, GLenum pname,
	GLint *params);
typedef void (APIENTRY *NamcosGetProgramInfoLogProc)(GLuint program,
	GLsizei bufferSize, GLsizei *length, char *infoLog);
typedef void (APIENTRY *NamcosDeleteProgramProc)(GLuint program);
typedef void (APIENTRY *NamcosUseProgramProc)(GLuint program);
typedef GLint (APIENTRY *NamcosGetAttribLocationProc)(GLuint program,
	const char *name);
typedef GLint (APIENTRY *NamcosGetUniformLocationProc)(GLuint program,
	const char *name);
typedef void (APIENTRY *NamcosUniform1iProc)(GLint location, GLint value);
typedef void (APIENTRY *NamcosUniform4fProc)(GLint location, GLfloat value0,
	GLfloat value1, GLfloat value2, GLfloat value3);
typedef void (APIENTRY *NamcosEnableVertexAttribArrayProc)(GLuint index);
typedef void (APIENTRY *NamcosVertexAttribPointerProc)(GLuint index, GLint size,
	GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (APIENTRY *NamcosGenBuffersProc)(GLsizei count, GLuint *buffers);
typedef void (APIENTRY *NamcosBindBufferProc)(GLenum target, GLuint buffer);
typedef void (APIENTRY *NamcosBufferDataProc)(GLenum target, ptrdiff_t size,
	const void *data, GLenum usage);
typedef void (APIENTRY *NamcosBufferSubDataProc)(GLenum target, ptrdiff_t offset,
	ptrdiff_t size, const void *data);
typedef void (APIENTRY *NamcosDeleteBuffersProc)(GLsizei count,
	const GLuint *buffers);
typedef void (APIENTRY *NamcosGenFramebuffersProc)(GLsizei count,
	GLuint *framebuffers);
typedef void (APIENTRY *NamcosBindFramebufferProc)(GLenum target,
	GLuint framebuffer);
typedef void (APIENTRY *NamcosFramebufferTexture2DProc)(GLenum target,
	GLenum attachment, GLenum textureTarget, GLuint texture, GLint level);
typedef GLenum (APIENTRY *NamcosCheckFramebufferStatusProc)(GLenum target);
typedef void (APIENTRY *NamcosDeleteFramebuffersProc)(GLsizei count,
	const GLuint *framebuffers);

struct NamcosGlPaletteConvertContext
{
	const UINT16 *source;
	UINT8 *destination;
	const UINT32 *palette;
	INT32 width;
	INT32 destinationPitch;
};

static void NamcosGlConvertPaletteRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlPaletteConvertContext *context =
		(NamcosGlPaletteConvertContext *)opaque;
	for (INT32 y = begin; y < end; y++) {
		const UINT16 *source = context->source + (size_t)y * context->width;
		UINT16 *destination = (UINT16 *)(context->destination +
			y * context->destinationPitch);
		for (INT32 x = 0; x < context->width; x++) {
			destination[x] = (UINT16)context->palette[source[x] & 0x7fff];
		}
	}
}

class NamcosOpenGLFrameConverter
{
public:
	NamcosOpenGLFrameConverter()
		: window(NULL),
		  dc(NULL),
		  context(NULL),
		  texture(0),
		  program(0),
		  vertexBuffer(0),
		  rasterTexture(0),
		  rasterSampleTexture(0),
		  rasterFramebuffer(0),
		  rasterProgram(0),
		  rasterVertexBuffer(0),
		  rasterVertexOffset(0),
		  rasterValidationCounter(0),
		  rasterPositionAttribute(-1),
		  rasterColorAttribute(-1),
		  rasterTextureAttribute(-1),
		  rasterSampleUniform(-1),
		  rasterTextureState0Uniform(-1),
		  rasterTextureState1Uniform(-1),
		  rasterFlagsUniform(-1),
		  rasterMaskUniform(-1),
		  rasterMaskUniformValid(false),
		  lastRasterDrawStp(false),
		  lastRasterCheckStp(false),
		  positionAttribute(-1),
		  positionRectUniform(-1),
		  textureRectUniform(-1),
		  verticalUniform(-1),
		  verticalReconstructUniform(-1),
		  drawUniformsValid(false),
		  textureUniformValid(false),
		  lastOutputLeft(0.0f),
		  lastOutputRight(0.0f),
		  lastTextureU0(0.0f),
		  lastTextureV0(0.0f),
		  lastTextureU1(0.0f),
		  lastTextureV1(0.0f),
		  lastVertical(false),
		  shaderMode(false),
		  createShader(NULL),
		  shaderSource(NULL),
		  compileShader(NULL),
		  getShaderiv(NULL),
		  deleteShader(NULL),
		  createProgram(NULL),
		  attachShader(NULL),
		  linkProgram(NULL),
		  getProgramiv(NULL),
		  deleteProgram(NULL),
		  useProgram(NULL),
		  getAttribLocation(NULL),
		  getUniformLocation(NULL),
		  uniform1i(NULL),
		  uniform4f(NULL),
		  enableVertexAttribArray(NULL),
		  vertexAttribPointer(NULL),
		  genBuffers(NULL),
		  bindBuffer(NULL),
		  bufferData(NULL),
		  bufferSubData(NULL),
		  deleteBuffers(NULL),
		  genFramebuffers(NULL),
		  bindFramebuffer(NULL),
		  framebufferTexture2D(NULL),
		  checkFramebufferStatus(NULL),
		  deleteFramebuffers(NULL),
		  rasterTransferPixels(NULL),
		  rgbUploadPixels(NULL),
		  rgbUploadCache(NULL),
		  rgbUploadCacheBytes(0),
		  rgbCacheWidth(0),
		  rgbCacheHeight(0),
		  rgbCacheDisplayX(0),
		  rgbCacheDisplayY(0),
		  rgbCacheValid(false),
		  rgbDenseFrames(0),
		  surfaceWidth(0),
		  surfaceHeight(0),
		  viewportWidth(0),
		  viewportHeight(0),
		  uploadSlotIndex(0),
		  uploadModeValid(false),
		  lastUploadRgb24(false),
		  uploadFrameValid(false),
		  outputFrameValid(false),
		  fullRasterizerCapable(false),
		  rasterizerFailureReason(0),
		  rasterVramSynchronized(false),
		  rasterThreadPool(NULL),
		  rasterStateActive(false),
		  initialized(false),
		  available(false),
		  failed(false)
	{
		for (INT32 i = 0; i < 4; i++) {
			uploadSlots[i].pixels = NULL;
			uploadSlots[i].pixelCapacity = 0;
			uploadSlots[i].destinationX = 0;
			uploadSlots[i].destinationY = 0;
			uploadSlots[i].width = 0;
			uploadSlots[i].rows = 0;
			uploadSlots[i].denseFrames = 0;
			uploadSlots[i].valid = false;
		}
	}

	~NamcosOpenGLFrameConverter()
	{
		Shutdown();
	}

	bool Probe(INT32 width, INT32 height)
	{
		return width > 0 && height > 0 && EnsureInitialized(width, height);
	}

	bool SupportsFullRasterizer() const
	{
		return available && fullRasterizerCapable;
	}

	bool SupportsRasterizerApi() const
	{
		return available && VersionAtLeast((const char *)glGetString(GL_VERSION),
			3, 3);
	}

	INT32 RasterizerFailureReason() const
	{
		return rasterizerFailureReason;
	}

	bool SupportsOpenGL2() const
	{
		return available && VersionAtLeast((const char *)glGetString(GL_VERSION),
			2, 0);
	}

	bool SupportsShaderMode() const
	{
		return shaderMode;
	}

	bool RasterizePacket(const NamcosGlRasterPacket *packet)
	{
		NamcosGlRasterPrimitive primitive;
		if (packet == NULL || !NamcosGlRasterDecodePacket(packet, &primitive)) {
			if (packet != NULL) SynchronizeVram(packet->vram,
				packet->vramGeneration != NULL ? *packet->vramGeneration : 0,
				packet->vramRowGeneration);
			return false;
		}
		if (!rasterVramSynchronized &&
			!UploadRasterVram(packet->vram, packet->vramRowGeneration,
				packet->threadPool)) return false;
		rasterThreadPool = packet->threadPool;
		if (SubmitRasterPrimitive(packet, &primitive)) return true;
		SynchronizeVram(packet->vram,
			packet->vramGeneration != NULL ? *packet->vramGeneration : 0,
			packet->vramRowGeneration);
		return false;
	}

	bool SynchronizeVram(UINT16 *vram, UINT64 generation, UINT64 *rowGeneration)
	{
		if (!rasterVramSynchronized) return true;
		const bool dirty = rasterDirty.valid;
		const INT32 firstDirtyRow = dirty ? rasterDirty.y1 : 0;
		const INT32 lastDirtyRow = dirty ? rasterDirty.y2 : -1;
		if (!ReadbackRasterVram(vram)) return false;
		if (rowGeneration != NULL && dirty) {
			for (INT32 row = firstDirtyRow; row <= lastDirtyRow; row++) {
				rowGeneration[row] = generation;
			}
			rasterUploadTracker.RememberRange(rowGeneration, firstDirtyRow,
				lastDirtyRow - firstDirtyRow + 1);
		}
		rasterVramSynchronized = false;
		InvalidateUploadCaches();
		return true;
	}

	bool Convert(const NamcosFrameConvertContext *frame)
	{
		return ConvertInternal(frame, NULL, 0, 0, NULL);
	}

	bool ConvertDirect(const NamcosFrameConvertContext *frame, UINT8 *destination,
		INT32 pitch, const UINT32 *)
	{
		if (frame == NULL || destination == NULL ||
			pitch < frame->outputWidth * 4 || (pitch & 3) != 0) {
			return false;
		}
		return ConvertInternal(frame, destination, pitch, 4, NULL);
	}

	bool ConvertDirect16(const NamcosFrameConvertContext *frame, UINT8 *destination,
		INT32 pitch, const UINT32 *palette)
	{
		if (frame == NULL || destination == NULL || palette == NULL ||
			pitch < frame->outputWidth * 2 || (pitch & 1) != 0) {
			return false;
		}
		return ConvertInternal(frame, destination, pitch, 2, palette);
	}

	bool ConvertDirect24(const NamcosFrameConvertContext *frame, UINT8 *destination,
		INT32 pitch, const UINT32 *)
	{
		if (frame == NULL || destination == NULL ||
			pitch < frame->outputWidth * 3 || (pitch % 3) != 0) {
			return false;
		}
		return ConvertInternal(frame, destination, pitch, 3, NULL);
	}

	bool ConvertInternal(const NamcosFrameConvertContext *frame,
		UINT8 *directDestination, INT32 directPitch, INT32 directBytes,
		const UINT32 *palette)
	{
		if (frame == NULL || frame->vram == NULL || frame->output == NULL ||
			(frame->vertical && (frame->rgb24 || frame->outputShiftX != 0)) ||
			frame->outputShiftX < 0 ||
			frame->outputShiftX >= frame->outputWidth ||
			frame->outputWidth <= 0 || frame->outputHeight <= 0 ||
			frame->sourceWidth <= 0 || frame->sourceWidth > 1024 ||
			frame->sourceHeight <= 0 || frame->sourceHeight > 1024) {
			return false;
		}

		if (!EnsureInitialized(frame->outputWidth, frame->outputHeight)) {
			return false;
		}

		if ((wglGetCurrentContext() != context || wglGetCurrentDC() != dc) &&
			!wglMakeCurrent(dc, context)) {
			Disable();
			return false;
		}
		EndRasterState();

		if (viewportWidth != frame->outputWidth || viewportHeight != frame->outputHeight) {
			glViewport(0, 0, frame->outputWidth, frame->outputHeight);
			viewportWidth = frame->outputWidth;
			viewportHeight = frame->outputHeight;
		}

		const bool rgb24 = frame->rgb24 != 0;
		if (!uploadModeValid || lastUploadRgb24 != rgb24) {
			InvalidateUploadCaches();
			uploadModeValid = true;
			lastUploadRgb24 = rgb24;
		}
		void *outputDestination = directDestination != NULL ?
			directDestination : (void *)frame->output;
		const INT32 outputPitch = directDestination != NULL ?
			directPitch : frame->outputWidth * 2;
		const INT32 outputBytes = directDestination != NULL ? directBytes : 2;
		const UINT32 *outputPalette = directDestination != NULL ? palette : NULL;
		if (outputFrameValid && NamcosFrameOutputMatches(&outputFrameKey, frame,
			outputDestination, outputPitch, outputBytes, outputPalette,
			outputRowGeneration)) {
			return true;
		}

		const bool rasterSource = rasterVramSynchronized && !rgb24;
		if (!rasterSource && (!uploadFrameValid ||
			!NamcosFrameUploadMatches(&uploadFrameKey, frame))) {
			if (rgb24) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
				UploadRgb24(frame);
			} else {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 1024);
				uploadSlotIndex = 0;
				INT32 uploadFirstRow;
				INT32 uploadRows;
				NamcosFrameGetObservedRows(frame, &uploadFirstRow, &uploadRows);
				const INT32 uploadX = frame->displayX & 0x3ff;
				const INT32 uploadY = (frame->displayY + uploadFirstRow) & 0x3ff;
				const INT32 firstColumns = frame->sourceWidth < 1024 - uploadX ?
					frame->sourceWidth : 1024 - uploadX;
				const INT32 firstRows = uploadRows < 1024 - uploadY ?
					uploadRows : 1024 - uploadY;
				UploadRect(frame->vram + uploadY * 1024 + uploadX,
					uploadX, uploadY, firstColumns, firstRows, frame->vramRowGeneration,
					frame->threadPool);
				if (firstColumns < frame->sourceWidth) {
					UploadRect(frame->vram + uploadY * 1024, 0, uploadY,
						frame->sourceWidth - firstColumns, firstRows,
						frame->vramRowGeneration, frame->threadPool);
				}
				if (firstRows < uploadRows) {
					UploadRect(frame->vram + uploadX, uploadX, 0, firstColumns,
						uploadRows - firstRows, frame->vramRowGeneration,
						frame->threadPool);
					if (firstColumns < frame->sourceWidth) {
						UploadRect(frame->vram, 0, 0,
							frame->sourceWidth - firstColumns,
							uploadRows - firstRows, frame->vramRowGeneration,
							frame->threadPool);
					}
				}
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			}
			NamcosFrameRememberUpload(&uploadFrameKey, frame);
			uploadFrameValid = true;
		}

		if (frame->outputShiftX != 0) {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

		const GLfloat textureX = frame->rgb24 ? 0.0f : (GLfloat)frame->displayX;
		const GLfloat textureY = frame->rgb24 ? 0.0f : (GLfloat)frame->displayY;
		const GLfloat u0 = (textureX + 0.5f - (frame->vertical ?
			0.5f * frame->sourceWidth / frame->outputHeight : 0.0f)) / 1024.0f;
		const GLfloat sourceTop = (GLfloat)frame->cropTop * frame->sourceHeight /
			frame->outputHeight;
		const GLfloat sourceRows = (GLfloat)frame->cropHeight * frame->sourceHeight /
			frame->outputHeight;
		const GLfloat v0 = frame->vertical ?
			(textureY + frame->sourceHeight - 1 - frame->cropTop + 0.5f +
			0.5f * frame->cropHeight / frame->outputWidth) / 1024.0f :
			(textureY + sourceTop + 0.5f) / 1024.0f;
		const GLfloat u1 = u0 + ((GLfloat)frame->sourceWidth / 1024.0f);
		const GLfloat v1 = v0 + (frame->vertical ?
			-(GLfloat)frame->cropHeight / 1024.0f : sourceRows / 1024.0f);
		const GLfloat sourceV0 = rasterSource ? 1.0f - v0 : v0;
		const GLfloat sourceV1 = rasterSource ? 1.0f - v1 : v1;

		const GLfloat outputLeft = -1.0f +
			(2.0f * frame->outputShiftX / frame->outputWidth);
		const GLfloat outputRight = outputLeft + 2.0f;
		if (shaderMode) {
			if (!drawUniformsValid || lastOutputLeft != outputLeft ||
				lastOutputRight != outputRight) {
				uniform4f(positionRectUniform, outputLeft, -1.0f, outputRight, 1.0f);
				lastOutputLeft = outputLeft;
				lastOutputRight = outputRight;
			}
			if (!drawUniformsValid || lastVertical != frame->vertical) {
				uniform1i(verticalUniform, frame->vertical ? 1 : 0);
				lastVertical = frame->vertical != 0;
			}
			uniform1i(verticalReconstructUniform,
				frame->verticalReconstruct2x ? 1 : 0);
			if (!textureUniformValid || lastTextureU0 != u0 ||
				lastTextureV0 != sourceV0 || lastTextureU1 != u1 ||
				lastTextureV1 != sourceV1) {
				uniform4f(textureRectUniform, u0, sourceV0, u1, sourceV1);
				lastTextureU0 = u0;
				lastTextureV0 = sourceV0;
				lastTextureU1 = u1;
				lastTextureV1 = sourceV1;
				textureUniformValid = true;
			}
			drawUniformsValid = true;
		} else {
			const GLfloat outputSpan = outputRight - outputLeft;
			const GLfloat textureUSpan = u1 - u0;
			const GLfloat textureVSpan = sourceV1 - sourceV0;
			const GLfloat vertices[] = {
				outputLeft, -1.0f,
				outputLeft + outputSpan * 2.0f, -1.0f,
				outputLeft, 3.0f
			};
			const GLfloat textureCoordinates[] = {
				u0, sourceV0,
				frame->vertical ? u0 : u0 + textureUSpan * 2.0f,
				frame->vertical ? sourceV0 + textureVSpan * 2.0f : sourceV0,
				frame->vertical ? u0 + textureUSpan * 2.0f : u0,
				frame->vertical ? sourceV0 : sourceV0 + textureVSpan * 2.0f
			};
			glVertexPointer(2, GL_FLOAT, 0, vertices);
			glTexCoordPointer(2, GL_FLOAT, 0, textureCoordinates);
		}
		glBindTexture(GL_TEXTURE_2D, rasterSource ? rasterTexture : texture);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindTexture(GL_TEXTURE_2D, texture);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		if (directBytes == 2) {
			if (NamcosPaletteIsRgb565(palette)) {
				glPixelStorei(GL_PACK_ALIGNMENT, 2);
				glPixelStorei(GL_PACK_ROW_LENGTH, directPitch / 2);
				glReadPixels(0, 0, frame->outputWidth, frame->outputHeight, GL_RGB,
					GL_UNSIGNED_SHORT_5_6_5, directDestination);
				glPixelStorei(GL_PACK_ROW_LENGTH, 0);
				if (glGetError() != GL_NO_ERROR) {
					Disable();
					return false;
				}
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration);
				outputFrameValid = true;
				return true;
			}
			UINT16 *readDestination = directPitch == frame->outputWidth * 2 ?
				(UINT16 *)directDestination : frame->output;
			glPixelStorei(GL_PACK_ALIGNMENT, 2);
			glPixelStorei(GL_PACK_ROW_LENGTH, 0);
			glReadPixels(0, 0, frame->outputWidth, frame->outputHeight, GL_RGBA,
				GL_UNSIGNED_SHORT_1_5_5_5_REV, readDestination);
			if (glGetError() != GL_NO_ERROR) {
				Disable();
				return false;
			}
			NamcosGlPaletteConvertContext convertContext;
			convertContext.source = readDestination;
			convertContext.destination = directDestination;
			convertContext.palette = palette;
			convertContext.width = frame->outputWidth;
			convertContext.destinationPitch = directPitch;
			const INT64 convertWork = (INT64)frame->outputWidth * frame->outputHeight;
			if (frame->threadPool != NULL) {
				frame->threadPool->ParallelForWork(frame->outputHeight, convertWork,
					32768, NamcosGlConvertPaletteRows, &convertContext);
			} else {
				NamcosGlConvertPaletteRows(&convertContext, 0, frame->outputHeight);
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration);
			outputFrameValid = true;
			return true;
		}

		if (directDestination != NULL) {
			glPixelStorei(GL_PACK_ALIGNMENT, directBytes == 4 ? 4 : 1);
			glPixelStorei(GL_PACK_ROW_LENGTH, directPitch / directBytes);
			glReadPixels(0, 0, frame->outputWidth, frame->outputHeight,
				directBytes == 4 ? GL_BGRA : GL_BGR, GL_UNSIGNED_BYTE,
				directDestination);
			glPixelStorei(GL_PACK_ROW_LENGTH, 0);
			if (glGetError() != GL_NO_ERROR) {
				Disable();
				return false;
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration);
			outputFrameValid = true;
			return true;
		}

		glPixelStorei(GL_PACK_ALIGNMENT, 2);
		glReadPixels(0, 0, frame->outputWidth, frame->outputHeight, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV, frame->output);

		if (glGetError() != GL_NO_ERROR) {
			Disable();
			return false;
		}
		NamcosFrameRememberOutput(&outputFrameKey, frame, outputDestination,
			outputPitch, outputBytes, outputPalette, outputRowGeneration);
		outputFrameValid = true;

		return true;
	}

	void InvalidatePalette()
	{
		outputFrameValid = false;
	}

	void InvalidateVram()
	{
		InvalidateUploadCaches();
		rasterVramSynchronized = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
	}

	void Shutdown()
	{
		if (context != NULL && dc != NULL) {
			wglMakeCurrent(dc, context);
			EndRasterState();
			DestroyRasterizerResources();
			if (vertexBuffer != 0 && deleteBuffers != NULL) {
				deleteBuffers(1, &vertexBuffer);
				vertexBuffer = 0;
			}
			if (program != 0 && deleteProgram != NULL) {
				deleteProgram(program);
				program = 0;
			}
			if (texture != 0) {
				glDeleteTextures(1, &texture);
				texture = 0;
			}
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(context);
			context = NULL;
		}

		if (window != NULL && dc != NULL) {
			ReleaseDC(window, dc);
			dc = NULL;
		}

		if (window != NULL) {
			DestroyWindow(window);
			window = NULL;
		}
		free(rgbUploadPixels);
		free(rgbUploadCache);
		free(rasterTransferPixels);
		rgbUploadPixels = NULL;
		rgbUploadCache = NULL;
		rasterTransferPixels = NULL;
		rgbUploadCacheBytes = 0;
		rgbCacheValid = false;
		rgbDenseFrames = 0;
		for (INT32 i = 0; i < 4; i++) {
			free(uploadSlots[i].pixels);
			uploadSlots[i].pixels = NULL;
			uploadSlots[i].pixelCapacity = 0;
			uploadSlots[i].valid = false;
		}

		initialized = false;
		available = false;
		surfaceWidth = 0;
		surfaceHeight = 0;
		viewportWidth = 0;
		viewportHeight = 0;
		shaderMode = false;
		drawUniformsValid = false;
		textureUniformValid = false;
		uploadModeValid = false;
		outputFrameValid = false;
	}

private:
	struct UploadSlot
	{
		UINT16 *pixels;
		size_t pixelCapacity;
		UINT64 rowGeneration[1024];
		INT32 destinationX;
		INT32 destinationY;
		INT32 width;
		INT32 rows;
		UINT8 denseFrames;
		bool valid;
	};

	void InvalidateUploadCaches()
	{
		for (INT32 i = 0; i < 4; i++) {
			uploadSlots[i].denseFrames = 0;
			uploadSlots[i].valid = false;
		}
		rgbCacheValid = false;
		rgbDenseFrames = 0;
		uploadFrameValid = false;
		outputFrameValid = false;
	}

	static bool ValidProc(const PROC proc)
	{
		return proc != NULL && proc != (PROC)1 && proc != (PROC)2 &&
			proc != (PROC)3 && proc != (PROC)-1;
	}

	bool CreateShaderProgram()
	{
		createShader = (NamcosCreateShaderProc)wglGetProcAddress("glCreateShader");
		shaderSource = (NamcosShaderSourceProc)wglGetProcAddress("glShaderSource");
		compileShader = (NamcosCompileShaderProc)wglGetProcAddress("glCompileShader");
		getShaderiv = (NamcosGetShaderivProc)wglGetProcAddress("glGetShaderiv");
		getShaderInfoLog = (NamcosGetShaderInfoLogProc)
			wglGetProcAddress("glGetShaderInfoLog");
		deleteShader = (NamcosDeleteShaderProc)wglGetProcAddress("glDeleteShader");
		createProgram = (NamcosCreateProgramProc)wglGetProcAddress("glCreateProgram");
		attachShader = (NamcosAttachShaderProc)wglGetProcAddress("glAttachShader");
		linkProgram = (NamcosLinkProgramProc)wglGetProcAddress("glLinkProgram");
		getProgramiv = (NamcosGetProgramivProc)wglGetProcAddress("glGetProgramiv");
		getProgramInfoLog = (NamcosGetProgramInfoLogProc)
			wglGetProcAddress("glGetProgramInfoLog");
		deleteProgram = (NamcosDeleteProgramProc)wglGetProcAddress("glDeleteProgram");
		useProgram = (NamcosUseProgramProc)wglGetProcAddress("glUseProgram");
		getAttribLocation = (NamcosGetAttribLocationProc)
			wglGetProcAddress("glGetAttribLocation");
		getUniformLocation = (NamcosGetUniformLocationProc)
			wglGetProcAddress("glGetUniformLocation");
		uniform1i = (NamcosUniform1iProc)wglGetProcAddress("glUniform1i");
		uniform4f = (NamcosUniform4fProc)wglGetProcAddress("glUniform4f");
		enableVertexAttribArray = (NamcosEnableVertexAttribArrayProc)
			wglGetProcAddress("glEnableVertexAttribArray");
		vertexAttribPointer = (NamcosVertexAttribPointerProc)
			wglGetProcAddress("glVertexAttribPointer");
		genBuffers = (NamcosGenBuffersProc)wglGetProcAddress("glGenBuffers");
		bindBuffer = (NamcosBindBufferProc)wglGetProcAddress("glBindBuffer");
		bufferData = (NamcosBufferDataProc)wglGetProcAddress("glBufferData");
		bufferSubData = (NamcosBufferSubDataProc)
			wglGetProcAddress("glBufferSubData");
		deleteBuffers = (NamcosDeleteBuffersProc)wglGetProcAddress("glDeleteBuffers");
		if (!ValidProc((PROC)createShader) || !ValidProc((PROC)shaderSource) ||
			!ValidProc((PROC)compileShader) || !ValidProc((PROC)getShaderiv) ||
			!ValidProc((PROC)deleteShader) || !ValidProc((PROC)createProgram) ||
			!ValidProc((PROC)attachShader) || !ValidProc((PROC)linkProgram) ||
			!ValidProc((PROC)getProgramiv) || !ValidProc((PROC)deleteProgram) ||
			!ValidProc((PROC)useProgram) || !ValidProc((PROC)getAttribLocation) ||
			!ValidProc((PROC)getUniformLocation) || !ValidProc((PROC)uniform1i) ||
			!ValidProc((PROC)uniform4f) ||
			!ValidProc((PROC)enableVertexAttribArray) ||
			!ValidProc((PROC)vertexAttribPointer) || !ValidProc((PROC)genBuffers) ||
			!ValidProc((PROC)bindBuffer) || !ValidProc((PROC)bufferData) ||
			!ValidProc((PROC)bufferSubData) ||
			!ValidProc((PROC)deleteBuffers)) {
			return false;
		}

		static const char vertexText[] =
			"#version 110\n"
			"attribute vec2 aCorner;\n"
			"uniform vec4 uPositionRect;\n"
			"uniform vec4 uTextureRect;\n"
			"uniform int uVertical;\n"
			"varying vec2 vTexCoord;\n"
			"void main() {\n"
			" vec2 position = mix(uPositionRect.xy, uPositionRect.zw, aCorner);\n"
			" gl_Position = vec4(position, 0.0, 1.0);\n"
			" if (uVertical != 0)\n"
			"  vTexCoord = vec2(mix(uTextureRect.x, uTextureRect.z, aCorner.y),"
			" mix(uTextureRect.y, uTextureRect.w, aCorner.x));\n"
			" else vTexCoord = mix(uTextureRect.xy, uTextureRect.zw, aCorner);\n"
			"}\n";
		static const char fragmentText[] =
			"#version 110\n"
			"uniform sampler2D uTexture;\n"
			"uniform vec4 uTextureRect;\n"
			"uniform int uVerticalReconstruct2x;\n"
			"varying vec2 vTexCoord;\n"
			"void main() {\n"
			" if (uVerticalReconstruct2x == 0) {\n"
			"  gl_FragColor = texture2D(uTexture, vTexCoord);\n"
			" } else {\n"
			"  float y = vTexCoord.y * 1024.0 - 0.5;\n"
			"  float f = fract(y);\n"
			"  float base = floor(y) + 0.5;\n"
			"  vec2 p = vec2(vTexCoord.x, base / 1024.0);\n"
			"  vec4 c0 = texture2D(uTexture, p + vec2(0.0, -1.0 / 1024.0));\n"
			"  vec4 c1 = texture2D(uTexture, p);\n"
			"  vec4 c2 = texture2D(uTexture, p + vec2(0.0, 1.0 / 1024.0));\n"
			"  vec4 c3 = texture2D(uTexture, p + vec2(0.0, 2.0 / 1024.0));\n"
			"  vec4 a = -0.5*c0 + 1.5*c1 - 1.5*c2 + 0.5*c3;\n"
			"  vec4 b = c0 - 2.5*c1 + 2.0*c2 - 0.5*c3;\n"
			"  vec4 c = -0.5*c0 + 0.5*c2;\n"
			"  gl_FragColor = clamp(((a*f+b)*f+c)*f+c1, 0.0, 1.0);\n"
			" }\n"
			"}\n";
		const GLuint vertex = createShader(GL_VERTEX_SHADER);
		const GLuint fragment = createShader(GL_FRAGMENT_SHADER);
		if (vertex == 0 || fragment == 0) {
			if (vertex != 0) deleteShader(vertex);
			if (fragment != 0) deleteShader(fragment);
			return false;
		}
		const char *source = vertexText;
		shaderSource(vertex, 1, &source, NULL);
		compileShader(vertex);
		GLint status = GL_FALSE;
		getShaderiv(vertex, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			deleteShader(vertex);
			deleteShader(fragment);
			return false;
		}
		source = fragmentText;
		shaderSource(fragment, 1, &source, NULL);
		compileShader(fragment);
		getShaderiv(fragment, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			deleteShader(vertex);
			deleteShader(fragment);
			return false;
		}
		program = createProgram();
		if (program != 0) {
			attachShader(program, vertex);
			attachShader(program, fragment);
			linkProgram(program);
		}
		deleteShader(vertex);
		deleteShader(fragment);
		if (program == 0) return false;
		getProgramiv(program, GL_LINK_STATUS, &status);
		if (status != GL_TRUE) return false;
		positionAttribute = getAttribLocation(program, "aCorner");
		positionRectUniform = getUniformLocation(program, "uPositionRect");
		textureRectUniform = getUniformLocation(program, "uTextureRect");
		verticalUniform = getUniformLocation(program, "uVertical");
		verticalReconstructUniform = getUniformLocation(program,
			"uVerticalReconstruct2x");
		const GLint textureUniform = getUniformLocation(program, "uTexture");
		if (positionAttribute < 0 || positionRectUniform < 0 ||
			textureRectUniform < 0 || verticalUniform < 0 ||
			verticalReconstructUniform < 0 ||
			textureUniform < 0) {
			return false;
		}
		static const GLfloat corners[] = {
			0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f
		};
		genBuffers(1, &vertexBuffer);
		if (vertexBuffer == 0) return false;
		bindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		bufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
		useProgram(program);
		uniform1i(textureUniform, 0);
		enableVertexAttribArray(positionAttribute);
		vertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		return glGetError() == GL_NO_ERROR;
	}

	GLuint CompileRasterShader(GLenum type, const char *source)
	{
		const GLuint shader = createShader(type);
		if (shader == 0) return 0;
		shaderSource(shader, 1, &source, NULL);
		compileShader(shader);
		GLint status = GL_FALSE;
		getShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			deleteShader(shader);
			return 0;
		}
		return shader;
	}

	void DestroyRasterizerResources()
	{
		if (rasterVertexBuffer != 0 && deleteBuffers != NULL) {
			deleteBuffers(1, &rasterVertexBuffer);
			rasterVertexBuffer = 0;
		}
		rasterVertexOffset = 0;
		rasterValidationCounter = 0;
		if (rasterProgram != 0 && deleteProgram != NULL) {
			deleteProgram(rasterProgram);
			rasterProgram = 0;
		}
		if (rasterFramebuffer != 0 && deleteFramebuffers != NULL) {
			deleteFramebuffers(1, &rasterFramebuffer);
			rasterFramebuffer = 0;
		}
		if (rasterTexture != 0) {
			glDeleteTextures(1, &rasterTexture);
			rasterTexture = 0;
		}
		if (rasterSampleTexture != 0) {
			glDeleteTextures(1, &rasterSampleTexture);
			rasterSampleTexture = 0;
		}
		rasterPositionAttribute = -1;
		rasterColorAttribute = -1;
		rasterTextureAttribute = -1;
		rasterSampleUniform = -1;
		rasterTextureState0Uniform = -1;
		rasterTextureState1Uniform = -1;
		rasterFlagsUniform = -1;
		rasterMaskUniform = -1;
		rasterMaskUniformValid = false;
		rasterVramSynchronized = false;
		rasterStateActive = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
	}

	bool CreateRasterizerResources()
	{
		rasterizerFailureReason = 0;
		genFramebuffers = (NamcosGenFramebuffersProc)
			wglGetProcAddress("glGenFramebuffers");
		bindFramebuffer = (NamcosBindFramebufferProc)
			wglGetProcAddress("glBindFramebuffer");
		framebufferTexture2D = (NamcosFramebufferTexture2DProc)
			wglGetProcAddress("glFramebufferTexture2D");
		checkFramebufferStatus = (NamcosCheckFramebufferStatusProc)
			wglGetProcAddress("glCheckFramebufferStatus");
		deleteFramebuffers = (NamcosDeleteFramebuffersProc)
			wglGetProcAddress("glDeleteFramebuffers");
		if (!ValidProc((PROC)genFramebuffers)) {
			genFramebuffers = (NamcosGenFramebuffersProc)
				wglGetProcAddress("glGenFramebuffersEXT");
		}
		if (!ValidProc((PROC)bindFramebuffer)) {
			bindFramebuffer = (NamcosBindFramebufferProc)
				wglGetProcAddress("glBindFramebufferEXT");
		}
		if (!ValidProc((PROC)framebufferTexture2D)) {
			framebufferTexture2D = (NamcosFramebufferTexture2DProc)
				wglGetProcAddress("glFramebufferTexture2DEXT");
		}
		if (!ValidProc((PROC)checkFramebufferStatus)) {
			checkFramebufferStatus = (NamcosCheckFramebufferStatusProc)
				wglGetProcAddress("glCheckFramebufferStatusEXT");
		}
		if (!ValidProc((PROC)deleteFramebuffers)) {
			deleteFramebuffers = (NamcosDeleteFramebuffersProc)
				wglGetProcAddress("glDeleteFramebuffersEXT");
		}
		if (!ValidProc((PROC)genFramebuffers) ||
			!ValidProc((PROC)bindFramebuffer) ||
			!ValidProc((PROC)framebufferTexture2D) ||
			!ValidProc((PROC)checkFramebufferStatus) ||
			!ValidProc((PROC)deleteFramebuffers)) {
			rasterizerFailureReason = 1;
			return false;
		}

		static const char vertexText[] =
			"#version 130\n"
			"in vec2 aPosition;\n"
			"in vec3 aColor;\n"
			"in vec2 aTexture;\n"
			"out vec3 vColor;\n"
			"out vec2 vTexture;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0,"
			" 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor;\n"
			" vTexture = aTexture;\n"
			"}\n";
		static const char fragmentText[] =
			"#version 130\n"
			"in vec3 vColor;\n"
			"in vec2 vTexture;\n"
			"uniform sampler2D uVram;\n"
			"uniform vec4 uTextureState0;\n"
			"uniform vec4 uTextureState1;\n"
			"uniform vec4 uFlags;\n"
			"uniform vec4 uMask;\n"
			"out vec4 outputColor;\n"
			"int wordAt(int x, int y) {\n"
			" vec4 c = texelFetch(uVram, ivec2(x & 1023, 1023 - (y & 1023)), 0);\n"
			" ivec3 q = ivec3(floor(c.rgb * 31.0 + 0.5));\n"
			" return q.r | (q.g << 5) | (q.b << 10) | (c.a >= 0.5 ? 32768 : 0);\n"
			"}\n"
			"vec4 unpackWord(int word) {\n"
			" return vec4(float(word & 31), float((word >> 5) & 31),\n"
			"  float((word >> 10) & 31), (word & 32768) != 0 ? 1.0 : 0.0);\n"
			"}\n"
			"int textureWord() {\n"
			" int u = int(floor(vTexture.x)) & int(uTextureState1.x);\n"
			" int v = int(floor(vTexture.y)) & int(uTextureState1.y);\n"
			" int tx = int(uTextureState0.x); int ty = int(uTextureState0.y);\n"
			" int mode = int(uTextureState0.z); bool interleaved = uTextureState0.w != 0.0;\n"
			" int x = tx; int y = (ty + (v & 255)) & 1023; int data;\n"
			" if (mode == 0) {\n"
			"  if (interleaved) { x += ((u >> 2) & ~60) + ((v << 2) & 60); y = ty + (v & ~15) + ((u >> 4) & 15); }\n"
			"  else x += (u & 255) >> 2;\n"
			"  data = (wordAt(x, y) >> ((u & 3) << 2)) & 15;\n"
			"  return wordAt(int(uTextureState1.z) + data, int(uTextureState1.w));\n"
			" }\n"
			" if (mode == 1) {\n"
			"  if (interleaved) { x += ((u >> 1) & ~120) + ((u << 2) & 64) + ((v << 3) & 56); y = ty + (v & ~7) + ((u >> 5) & 7); }\n"
			"  else x += (u & 255) >> 1;\n"
			"  data = wordAt(x, y); data = (u & 1) != 0 ? (data >> 8) : (data & 255);\n"
			"  return wordAt(int(uTextureState1.z) + data, int(uTextureState1.w));\n"
			" }\n"
			" if (mode == 2) return wordAt(tx + (u & 255), y);\n"
			" return 0;\n"
			"}\n"
			"void main() {\n"
			" bool textured = uFlags.x != 0.0; int texel = textured ? textureWord() : 0;\n"
			" if (textured && texel == 0) discard;\n"
			" vec4 color = textured ? unpackWord(texel) : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && uFlags.y == 0.0) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = uFlags.z != 0.0 && (!textured || (texel & 32768) != 0);\n"
			" int backgroundWord = 0;\n"
			" if (uMask.y != 0.0 || blend) backgroundWord = wordAt(int(gl_FragCoord.x), 1023 - int(gl_FragCoord.y));\n"
			" if (uMask.y != 0.0 && (backgroundWord & 32768) != 0) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = unpackWord(backgroundWord).rgb; int abr = int(uFlags.w);\n"
			"  if (abr == 0) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr == 1) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr == 2) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  if (textured) color.a = 1.0; else color.a = 0.0;\n"
			" }\n"
			" if (uMask.x != 0.0) color.a = 1.0;\n"
			" outputColor = vec4(color.rgb / 31.0, color.a);\n"
			"}\n";
		static const char legacyVertexText[] =
			"#version 120\n"
			"attribute vec2 aPosition;\n"
			"attribute vec3 aColor;\n"
			"attribute vec2 aTexture;\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0, 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor;\n"
			" vTexture = aTexture;\n"
			"}\n";
		static const char legacyFragmentText[] =
			"#version 120\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"uniform sampler2D uVram;\n"
			"uniform vec4 uTextureState0;\n"
			"uniform vec4 uTextureState1;\n"
			"uniform vec4 uFlags;\n"
			"uniform vec4 uMask;\n"
			"float and8(float a, float b) {\n"
			" float result = 0.0; float bitValue = 1.0;\n"
			" for (int i = 0; i < 8; i++) {\n"
			"  if (mod(floor(a / bitValue), 2.0) >= 1.0 && mod(floor(b / bitValue), 2.0) >= 1.0) result += bitValue;\n"
			"  bitValue *= 2.0;\n"
			" }\n"
			" return result;\n"
			"}\n"
			"float wordAt(float x, float y) {\n"
			" float sx = mod(floor(x), 1024.0); float sy = mod(floor(y), 1024.0);\n"
			" vec4 c = texture2D(uVram, vec2((sx + 0.5) / 1024.0, (1023.0 - sy + 0.5) / 1024.0));\n"
			" vec3 q = floor(c.rgb * 31.0 + 0.5);\n"
			" return q.r + q.g * 32.0 + q.b * 1024.0 + (c.a >= 0.5 ? 32768.0 : 0.0);\n"
			"}\n"
			"vec4 unpackWord(float word) {\n"
			" return vec4(mod(word, 32.0), mod(floor(word / 32.0), 32.0),\n"
			"  mod(floor(word / 1024.0), 32.0), word >= 32768.0 ? 1.0 : 0.0);\n"
			"}\n"
			"float textureWord() {\n"
			" float u = and8(floor(vTexture.x), uTextureState1.x);\n"
			" float v = and8(floor(vTexture.y), uTextureState1.y);\n"
			" float tx = uTextureState0.x; float ty = uTextureState0.y;\n"
			" float mode = uTextureState0.z; bool interleaved = uTextureState0.w != 0.0;\n"
			" float x = tx; float y = mod(ty + mod(v, 256.0), 1024.0); float data;\n"
			" if (mode < 0.5) {\n"
			"  if (interleaved) { x += and8(floor(u / 4.0), 195.0) + and8(v * 4.0, 60.0); y = ty + (v - mod(v, 16.0)) + mod(floor(u / 16.0), 16.0); }\n"
			"  else x += floor(mod(u, 256.0) / 4.0);\n"
			"  data = mod(floor(wordAt(x, y) / pow(16.0, mod(u, 4.0))), 16.0);\n"
			"  return wordAt(uTextureState1.z + data, uTextureState1.w);\n"
			" }\n"
			" if (mode < 1.5) {\n"
			"  if (interleaved) { x += and8(floor(u / 2.0), 135.0) + and8(u * 4.0, 64.0) + and8(v * 8.0, 56.0); y = ty + (v - mod(v, 8.0)) + mod(floor(u / 32.0), 8.0); }\n"
			"  else x += floor(mod(u, 256.0) / 2.0);\n"
			"  data = wordAt(x, y); data = mod(u, 2.0) >= 1.0 ? floor(data / 256.0) : mod(data, 256.0);\n"
			"  return wordAt(uTextureState1.z + data, uTextureState1.w);\n"
			" }\n"
			" if (mode < 2.5) return wordAt(tx + mod(u, 256.0), y);\n"
			" return 0.0;\n"
			"}\n"
			"void main() {\n"
			" bool textured = uFlags.x != 0.0; float texel = textured ? textureWord() : 0.0;\n"
			" if (textured && texel < 0.5) discard;\n"
			" vec4 color = textured ? unpackWord(texel) : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && uFlags.y == 0.0) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = uFlags.z != 0.0 && (!textured || texel >= 32768.0);\n"
			" float backgroundWord = 0.0;\n"
			" if (uMask.y != 0.0 || blend) backgroundWord = wordAt(floor(gl_FragCoord.x), 1023.0 - floor(gl_FragCoord.y));\n"
			" if (uMask.y != 0.0 && backgroundWord >= 32768.0) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = unpackWord(backgroundWord).rgb; float abr = uFlags.w;\n"
			"  if (abr < 0.5) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr < 1.5) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr < 2.5) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  color.a = textured ? 1.0 : 0.0;\n"
			" }\n"
			" if (uMask.x != 0.0) color.a = 1.0;\n"
			" gl_FragColor = vec4(color.rgb / 31.0, color.a);\n"
			"}\n";

		const bool modernShader = VersionAtLeast(
			(const char *)glGetString(GL_VERSION), 3, 0);
		const GLuint vertex = CompileRasterShader(GL_VERTEX_SHADER,
			modernShader ? vertexText : legacyVertexText);
		const GLuint fragment = CompileRasterShader(GL_FRAGMENT_SHADER,
			modernShader ? fragmentText : legacyFragmentText);
		if (vertex == 0 || fragment == 0) {
			rasterizerFailureReason = 2;
			if (vertex != 0) deleteShader(vertex);
			if (fragment != 0) deleteShader(fragment);
			return false;
		}
		rasterProgram = createProgram();
		if (rasterProgram != 0) {
			attachShader(rasterProgram, vertex);
			attachShader(rasterProgram, fragment);
			linkProgram(rasterProgram);
		}
		deleteShader(vertex);
		deleteShader(fragment);
		GLint linked = GL_FALSE;
		if (rasterProgram != 0) {
			getProgramiv(rasterProgram, GL_LINK_STATUS, &linked);
		}
		if (linked != GL_TRUE) {
			rasterizerFailureReason = 3;
			DestroyRasterizerResources();
			return false;
		}
		rasterPositionAttribute = getAttribLocation(rasterProgram, "aPosition");
		rasterColorAttribute = getAttribLocation(rasterProgram, "aColor");
		rasterTextureAttribute = getAttribLocation(rasterProgram, "aTexture");
		rasterSampleUniform = getUniformLocation(rasterProgram, "uVram");
		rasterTextureState0Uniform = getUniformLocation(rasterProgram,
			"uTextureState0");
		rasterTextureState1Uniform = getUniformLocation(rasterProgram,
			"uTextureState1");
		rasterFlagsUniform = getUniformLocation(rasterProgram, "uFlags");
		rasterMaskUniform = getUniformLocation(rasterProgram, "uMask");
		if (rasterPositionAttribute < 0 || rasterColorAttribute < 0 ||
			rasterTextureAttribute < 0 || rasterSampleUniform < 0 ||
			rasterTextureState0Uniform < 0 || rasterTextureState1Uniform < 0 ||
			rasterFlagsUniform < 0 || rasterMaskUniform < 0) {
			rasterizerFailureReason = 4;
			DestroyRasterizerResources();
			return false;
		}
		useProgram(rasterProgram);
		uniform1i(rasterSampleUniform, 0);
		uniform4f(rasterTextureState0Uniform, 0.0f, 0.0f, 0.0f, 0.0f);
		uniform4f(rasterTextureState1Uniform, 0.0f, 0.0f, 0.0f, 0.0f);
		uniform4f(rasterFlagsUniform, 0.0f, 0.0f, 0.0f, 0.0f);
		useProgram(program);

		glGenTextures(1, &rasterTexture);
		glBindTexture(GL_TEXTURE_2D, rasterTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1024, 1024, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, NULL);
		glGenTextures(1, &rasterSampleTexture);
		glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1024, 1024, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, NULL);
		genFramebuffers(1, &rasterFramebuffer);
		bindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, rasterTexture, 0);
		const bool framebufferReady = rasterTexture != 0 &&
			rasterSampleTexture != 0 &&
			rasterFramebuffer != 0 &&
			checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
			glGetError() == GL_NO_ERROR;
		bindFramebuffer(GL_FRAMEBUFFER, 0);
		if (!framebufferReady) {
			rasterizerFailureReason = 5;
			DestroyRasterizerResources();
			return false;
		}

		genBuffers(1, &rasterVertexBuffer);
		if (rasterVertexBuffer == 0) {
			rasterizerFailureReason = 6;
			DestroyRasterizerResources();
			return false;
		}
		bindBuffer(GL_ARRAY_BUFFER, rasterVertexBuffer);
		bufferData(GL_ARRAY_BUFFER,
			sizeof(NamcosGlRasterDrawVertex) *
				NAMCOS_GL_RASTER_STREAM_VERTICES, NULL,
			GL_DYNAMIC_DRAW);
		rasterVertexOffset = 0;
		const bool ready = glGetError() == GL_NO_ERROR;
		bindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		useProgram(program);
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterizerFailureReason = ready ? 0 : 7;
		return ready;
	}

	bool BeginRasterState()
	{
		if (rasterStateActive) return true;
		bindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		glViewport(0, 0, 1024, 1024);
		glEnable(GL_SCISSOR_TEST);
		useProgram(rasterProgram);
		bindBuffer(GL_ARRAY_BUFFER, rasterVertexBuffer);
		enableVertexAttribArray(rasterPositionAttribute);
		enableVertexAttribArray(rasterColorAttribute);
		enableVertexAttribArray(rasterTextureAttribute);
		rasterStateActive = glGetError() == GL_NO_ERROR;
		if (!rasterStateActive) EndRasterState(true);
		return rasterStateActive;
	}

	void EndRasterState(bool force = false)
	{
		if (!rasterStateActive && !force) return;
		glDisable(GL_SCISSOR_TEST);
		bindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, viewportWidth, viewportHeight);
		bindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		useProgram(program);
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterStateActive = false;
	}

	bool SubmitRasterPrimitive(const NamcosGlRasterPacket *packet,
		const NamcosGlRasterPrimitive *primitive)
	{
		if (packet == NULL || primitive == NULL) return false;
		NamcosGlRasterDrawVertex vertices[6];
		const bool fastClear = primitive->type == NAMCOS_GL_RASTER_FILL ||
			primitive->type == NAMCOS_GL_RASTER_FLAT_RECTANGLE;
		const bool textured = primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_POLYGON || primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_RECTANGLE;
		const UINT32 count = fastClear ? 0 : textured ? NamcosGlRasterBuildTexturedTriangles(
			primitive, vertices, 6) : NamcosGlRasterBuildColorTriangles(
			primitive, vertices, 6);
		if (!fastClear && count == 0) return false;
		if (!rasterStateActive && wglGetCurrentContext() != context &&
			!wglMakeCurrent(dc, context))
			return false;

		INT32 x1, y1, x2, y2;
		if (!NamcosGlRasterGetDrawBounds(packet, primitive,
			&x1, &y1, &x2, &y2)) return false;

		if (!BeginRasterState()) return false;
		glScissor(x1, 1024 - y2 - 1, x2 - x1 + 1, y2 - y1 + 1);
		if (fastClear) {
			const GLfloat red = (GLfloat)((primitive->vertex[0].red * 31 + 127) /
				255) / 31.0f;
			const GLfloat green = (GLfloat)((primitive->vertex[0].green * 31 + 127) /
				255) / 31.0f;
			const GLfloat blue = (GLfloat)((primitive->vertex[0].blue * 31 + 127) /
				255) / 31.0f;
			const GLfloat alpha = primitive->type != NAMCOS_GL_RASTER_FILL &&
				packet->state.drawStp ? 1.0f : 0.0f;
			glClearColor(red, green, blue, alpha);
			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			if ((++rasterValidationCounter & 0xff) == 0 &&
				glGetError() != GL_NO_ERROR) {
				EndRasterState();
				return false;
			}
			rasterDirty.Include(x1, y1, x2, y2);
			const UINT64 generation = packet->vramGeneration != NULL ?
				*packet->vramGeneration : 0;
			if (packet->vramRowGeneration != NULL) {
				for (INT32 y = y1; y <= y2; y++) {
					packet->vramRowGeneration[y & 0x3ff] = generation;
				}
				rasterUploadTracker.RememberRange(packet->vramRowGeneration,
					y1, y2 - y1 + 1);
			}
			outputFrameValid = false;
			return true;
		}
		if (textured || primitive->semiTransparent) {
			glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
			UINT32 textureState[8] = { 0 };
			if (textured) NamcosGlRasterGetTextureState(packet, primitive,
				textureState);
			uniform4f(rasterTextureState0Uniform, (GLfloat)textureState[0],
				(GLfloat)textureState[1], (GLfloat)textureState[2],
				(GLfloat)textureState[3]);
			uniform4f(rasterTextureState1Uniform, (GLfloat)textureState[4],
				(GLfloat)textureState[5], (GLfloat)textureState[6],
				(GLfloat)textureState[7]);
			const UINT32 abr = (primitive->tpage >>
				(packet->state.gpuType == 2 ? 5 : 7)) & 3;
			uniform4f(rasterFlagsUniform, textured ? 1.0f : 0.0f,
				primitive->rawTexture ? 1.0f : 0.0f,
				primitive->semiTransparent ? 1.0f : 0.0f, (GLfloat)abr);
		}
		const bool fill = primitive->type == NAMCOS_GL_RASTER_FILL;
		const bool drawStp = !fill && packet->state.drawStp != 0;
		const bool checkStp = !fill && packet->state.checkStp != 0;
		if (!rasterMaskUniformValid || lastRasterDrawStp != drawStp ||
			lastRasterCheckStp != checkStp) {
			uniform4f(rasterMaskUniform, drawStp ? 1.0f : 0.0f,
				checkStp ? 1.0f : 0.0f, 0.0f, 0.0f);
			lastRasterDrawStp = drawStp;
			lastRasterCheckStp = checkStp;
			rasterMaskUniformValid = true;
		}
		if (rasterVertexOffset + count > NAMCOS_GL_RASTER_STREAM_VERTICES) {
			bufferData(GL_ARRAY_BUFFER,
				sizeof(NamcosGlRasterDrawVertex) *
					NAMCOS_GL_RASTER_STREAM_VERTICES, NULL,
				GL_DYNAMIC_DRAW);
			rasterVertexOffset = 0;
		}
		const ptrdiff_t vertexOffset = sizeof(NamcosGlRasterDrawVertex) *
			rasterVertexOffset;
		bufferSubData(GL_ARRAY_BUFFER, vertexOffset,
			sizeof(NamcosGlRasterDrawVertex) * count, vertices);
		vertexAttribPointer(rasterPositionAttribute, 2, GL_FLOAT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex), (const void *)vertexOffset);
		vertexAttribPointer(rasterColorAttribute, 3, GL_FLOAT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)(vertexOffset + sizeof(float) * 2));
		vertexAttribPointer(rasterTextureAttribute, 2, GL_FLOAT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)(vertexOffset + sizeof(float) * 5));
		glDrawArrays(GL_TRIANGLES, 0, count);
		rasterVertexOffset += count;
		if ((++rasterValidationCounter & 0xff) == 0 &&
			glGetError() != GL_NO_ERROR) {
			EndRasterState();
			return false;
		}
		rasterDirty.Include(x1, y1, x2, y2);
		const UINT64 generation = packet->vramGeneration != NULL ?
			*packet->vramGeneration : 0;
		if (packet->vramRowGeneration != NULL) {
			for (INT32 y = y1; y <= y2; y++) {
				packet->vramRowGeneration[y & 0x3ff] = generation;
			}
			rasterUploadTracker.RememberRange(packet->vramRowGeneration,
				y1, y2 - y1 + 1);
		}
		outputFrameValid = false;
		return true;
	}

	bool UploadRasterVram(const UINT16 *vram, const UINT64 *rowGeneration,
		NamcosPolyThreadPool *threadPool)
	{
		if (vram == NULL) return false;
		if (wglGetCurrentContext() != context && !wglMakeCurrent(dc, context))
			return false;
		if (rasterTransferPixels == NULL) {
			rasterTransferPixels = (UINT8 *)malloc(1024 * 1024 * 4);
			if (rasterTransferPixels == NULL) return false;
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		if (!rasterUploadTracker.valid || rowGeneration == NULL) {
			NamcosGlPackVramParallel(vram, rasterTransferPixels, threadPool);
			glBindTexture(GL_TEXTURE_2D, rasterTexture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA,
				GL_UNSIGNED_BYTE, rasterTransferPixels);
		} else {
			NamcosGlRasterUploadSpan spans[8];
			INT32 first;
			INT32 rows;
			const INT32 spanCount = NamcosGlRasterBuildUploadSpans(
				&rasterUploadTracker, rowGeneration, spans, 8, &first, &rows);
			if (spanCount != 0) glBindTexture(GL_TEXTURE_2D, rasterTexture);
			if (spanCount < 0) {
				NamcosGlRasterPackVramRange(vram, rasterTransferPixels, first, rows);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1024 - first - rows,
					1024, rows, GL_RGBA, GL_UNSIGNED_BYTE, rasterTransferPixels);
			} else {
				for (INT32 span = 0; span < spanCount; span++) {
					first = spans[span].firstRow;
					rows = spans[span].rowCount;
					NamcosGlRasterPackVramRange(vram, rasterTransferPixels,
						first, rows);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0,
						1024 - first - rows, 1024, rows, GL_RGBA,
						GL_UNSIGNED_BYTE, rasterTransferPixels);
				}
			}
		}
		const bool uploaded = glGetError() == GL_NO_ERROR;
		if (uploaded) rasterUploadTracker.RememberAll(rowGeneration);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterVramSynchronized = uploaded;
		if (uploaded) rasterDirty.Reset();
		return uploaded;
	}

	bool ReadbackRasterVram(UINT16 *vram)
	{
		if (vram == NULL || rasterTransferPixels == NULL) return false;
		if (wglGetCurrentContext() != context && !wglMakeCurrent(dc, context))
			return false;
		if (!rasterDirty.valid) return true;
		NamcosGlRasterRect rects[8];
		const INT32 rectCount = rasterDirty.GetReadbackRects(rects, 8);
		bool read = rectCount > 0;
		bindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		for (INT32 i = 0; i < rectCount; i++) {
			const INT32 width = rects[i].x2 - rects[i].x1 + 1;
			const INT32 height = rects[i].y2 - rects[i].y1 + 1;
			glReadPixels(rects[i].x1, 1024 - rects[i].y2 - 1,
				width, height, GL_RGBA, GL_UNSIGNED_BYTE, rasterTransferPixels);
			if (glGetError() != GL_NO_ERROR) {
				read = false;
				break;
			}
			NamcosGlUnpackVramRectParallel(rasterTransferPixels, vram,
				rects[i].x1, rects[i].y1, width, height, rasterThreadPool);
		}
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		EndRasterState(true);
		if (read) rasterDirty.Reset();
		return read;
	}

	void UploadRgb24(const NamcosFrameConvertContext *frame)
	{
		const INT32 width = frame->sourceWidth;
		const INT32 rows = frame->sourceHeight;
		const size_t rowBytes = (size_t)width * 3;
		const size_t totalBytes = rowBytes * rows;
		INT32 observedFirst;
		INT32 observedRows;
		NamcosFrameGetObservedRows(frame, &observedFirst, &observedRows);
		if (rgbUploadCacheBytes < totalBytes) {
			UINT8 *newCache = (UINT8 *)realloc(rgbUploadCache, totalBytes);
			if (newCache == NULL) {
				rgbCacheValid = false;
				rgbDenseFrames = 0;
				NamcosPackRgb24(frame, rgbUploadPixels);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, rows,
					GL_RGB, GL_UNSIGNED_BYTE, rgbUploadPixels);
				return;
			}
			rgbUploadCache = newCache;
			rgbUploadCacheBytes = totalBytes;
		}

		if (!rgbCacheValid || rgbCacheWidth != width || rgbCacheHeight != rows ||
			rgbCacheDisplayX != frame->displayX || rgbCacheDisplayY != frame->displayY) {
			NamcosPackRgb24(frame, rgbUploadPixels);
			NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
				rows, NULL, rows, frame->threadPool);
			rgbCacheWidth = width;
			rgbCacheHeight = rows;
			rgbCacheDisplayX = frame->displayX;
			rgbCacheDisplayY = frame->displayY;
			for (INT32 y = 0; y < rows; y++) {
				rgbRowGeneration[y] = frame->vramRowGeneration != NULL ?
					frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			}
			rgbCacheValid = true;
			rgbDenseFrames = 0;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, rows,
				GL_RGB, GL_UNSIGNED_BYTE, rgbUploadPixels);
			return;
		}
		if (rgbDenseFrames != 0) {
			UINT8 denseRows[1024];
			memset(denseRows, 0, rows);
			INT32 denseChangedRows = 0;
			if (frame->vramRowGeneration != NULL) {
				for (INT32 y = observedFirst; y < observedFirst + observedRows; y++) {
					const UINT64 generation =
						frame->vramRowGeneration[(frame->displayY + y) & 0x3ff];
					denseRows[y] = rgbRowGeneration[y] != generation ? 1 : 0;
					if (denseRows[y]) denseChangedRows++;
				}
				if (denseChangedRows == 0) {
					rgbDenseFrames = 0;
					return;
				}
				if (denseChangedRows * 3 < observedRows) rgbDenseFrames = 0;
			} else {
				memset(denseRows + observedFirst, 1, observedRows);
				denseChangedRows = observedRows;
			}
			if (rgbDenseFrames != 0) {
				NamcosPackRgb24Selected(frame, rgbUploadPixels, denseRows,
					denseChangedRows);
				NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
					rows, denseRows, denseChangedRows, frame->threadPool);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width,
					observedRows, GL_RGB, GL_UNSIGNED_BYTE,
					rgbUploadCache + (size_t)observedFirst * rowBytes);
				for (INT32 y = observedFirst; y < observedFirst + observedRows; y++) {
					rgbRowGeneration[y] = frame->vramRowGeneration != NULL ?
						frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
				}
				--rgbDenseFrames;
				return;
			}
		}

		UINT8 dirtyRows[1024];
		INT32 changedRows = 0;
		INT32 candidateRows = 0;
		INT32 runCount = 0;
		bool inRun = false;
		for (INT32 y = 0; y < rows; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool observed = y >= observedFirst &&
				y < observedFirst + observedRows;
			const bool candidate = observed && (frame->vramRowGeneration == NULL ||
				rgbRowGeneration[y] != generation);
			dirtyRows[y] = candidate ? 1 : 0;
			if (candidate) candidateRows++;
		}
		NamcosPackRgb24Selected(frame, rgbUploadPixels, dirtyRows, candidateRows);
		for (INT32 y = 0; y < rows; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool candidate = dirtyRows[y] != 0;
			const bool changed = candidate &&
				memcmp(rgbUploadCache + (size_t)y * rowBytes,
					rgbUploadPixels + (size_t)y * rowBytes, rowBytes) != 0;
			if (candidate) rgbRowGeneration[y] = generation;
			dirtyRows[y] = changed ? 1 : 0;
			if (changed) {
				changedRows++;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= observedRows) {
			rgbDenseFrames = 7;
			NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
				rows, dirtyRows, changedRows, frame->threadPool);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width, observedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)observedFirst * rowBytes);
			return;
		}
		if (runCount > 8) {
			NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
				rows, dirtyRows, changedRows, frame->threadPool);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width, observedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)observedFirst * rowBytes);
			return;
		}

		NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
			rows, dirtyRows, changedRows, frame->threadPool);
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, mergedStart, width, mergedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)mergedStart * rowBytes);
			return;
		}
		for (INT32 y = 0; y < rows;) {
			while (y < rows && !dirtyRows[y]) y++;
			const INT32 start = y;
			while (y < rows && dirtyRows[y]) y++;
			if (start < y) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, start, width, y - start,
					GL_RGB, GL_UNSIGNED_BYTE,
					rgbUploadPixels + (size_t)start * rowBytes);
			}
		}
	}

	void UploadRect(const UINT16 *source, INT32 destinationX, INT32 destinationY,
		INT32 width, INT32 rows, const UINT64 *rowGeneration,
		NamcosPolyThreadPool *threadPool)
	{
		UploadSlot *slot = uploadSlotIndex < 4 ?
			&uploadSlots[uploadSlotIndex++] : NULL;
		const size_t pixelCount = (size_t)width * rows;
		const bool sameGeometry = slot != NULL && slot->valid &&
			slot->destinationX == destinationX &&
			slot->destinationY == destinationY &&
			slot->width == width && slot->rows == rows;

		if (slot != NULL && slot->pixelCapacity < pixelCount) {
			UINT16 *newPixels = (UINT16 *)realloc(slot->pixels,
				pixelCount * sizeof(UINT16));
			if (newPixels == NULL) {
				slot->valid = false;
				slot = NULL;
			} else {
				slot->pixels = newPixels;
				slot->pixelCapacity = pixelCount;
			}
		}
		if (slot == NULL || !sameGeometry) {
			if (slot != NULL) {
				NamcosCopy16Rows(source, slot->pixels, width, rows, threadPool);
				for (INT32 y = 0; y < rows; y++) {
					slot->rowGeneration[y] = rowGeneration != NULL ?
						rowGeneration[(destinationY + y) & 0x3ff] : 0;
				}
				slot->destinationX = destinationX;
				slot->destinationY = destinationY;
				slot->width = width;
				slot->rows = rows;
				slot->denseFrames = 0;
				slot->valid = true;
			}
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
				width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
			return;
		}
		if (slot->denseFrames != 0) {
			UINT8 denseRows[1024];
			INT32 denseChangedRows = rows;
			if (rowGeneration != NULL) {
				denseChangedRows = 0;
				for (INT32 y = 0; y < rows; y++) {
					const UINT64 generation =
						rowGeneration[(destinationY + y) & 0x3ff];
					denseRows[y] = slot->rowGeneration[y] != generation ? 1 : 0;
					if (denseRows[y]) denseChangedRows++;
				}
				if (denseChangedRows == 0) {
					slot->denseFrames = 0;
					return;
				}
				if (denseChangedRows * 3 < rows) slot->denseFrames = 0;
			}
			if (slot->denseFrames != 0) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
					width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
				if (rowGeneration != NULL) {
					NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
						denseRows, denseChangedRows, threadPool);
					for (INT32 y = 0; y < rows; y++) {
						slot->rowGeneration[y] =
							rowGeneration[(destinationY + y) & 0x3ff];
					}
				}
				if (--slot->denseFrames == 0 && rowGeneration == NULL) {
					NamcosCopy16Rows(source, slot->pixels, width, rows, threadPool);
				}
				return;
			}
		}

		UINT8 dirtyRows[1024];
		INT32 changedRows = 0;
		INT32 runCount = 0;
		bool inRun = false;
		for (INT32 y = 0; y < rows; y++) {
			const UINT64 generation = rowGeneration != NULL ?
				rowGeneration[(destinationY + y) & 0x3ff] : 0;
			const bool changed = (rowGeneration == NULL ||
				slot->rowGeneration[y] != generation) &&
				memcmp(slot->pixels + (size_t)y * width,
					source + (size_t)y * 1024,
					(size_t)width * sizeof(UINT16)) != 0;
			slot->rowGeneration[y] = generation;
			dirtyRows[y] = changed ? 1 : 0;
			if (changed) {
				changedRows++;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= rows) {
			slot->denseFrames = 7;
			NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
				dirtyRows, changedRows, threadPool);
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
				width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
			return;
		}
		if (runCount > 8) {
			NamcosCopy16Rows(source, slot->pixels, width, rows, threadPool);
			for (INT32 y = 0; y < rows; y++) {
				slot->rowGeneration[y] = rowGeneration != NULL ?
					rowGeneration[(destinationY + y) & 0x3ff] : 0;
			}
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
				width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
			return;
		}

		NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
			dirtyRows, changedRows, threadPool);
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX,
				destinationY + mergedStart, width, mergedRows, GL_RGBA,
				GL_UNSIGNED_SHORT_1_5_5_5_REV, source + mergedStart * 1024);
			return;
		}
		for (INT32 y = 0; y < rows;) {
			while (y < rows && !dirtyRows[y]) y++;
			const INT32 start = y;
			while (y < rows && dirtyRows[y]) y++;
			if (start < y) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX,
					destinationY + start, width, y - start, GL_RGBA,
					GL_UNSIGNED_SHORT_1_5_5_5_REV, source + start * 1024);
			}
		}
	}

	static bool VersionAtLeast(const char *version, INT32 requiredMajor, INT32 requiredMinor)
	{
		if (version == NULL) return false;

		while (*version != 0 && (*version < '0' || *version > '9')) version++;
		if (*version == 0) return false;

		INT32 major = 0;
		INT32 minor = 0;
		while (*version >= '0' && *version <= '9') {
			major = (major * 10) + (*version++ - '0');
		}
		if (*version == '.') version++;
		while (*version >= '0' && *version <= '9') {
			minor = (minor * 10) + (*version++ - '0');
		}

		return major > requiredMajor || (major == requiredMajor && minor >= requiredMinor);
	}

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
	{
		return DefWindowProc(hwnd, message, wparam, lparam);
	}

	bool EnsureInitialized(INT32 width, INT32 height)
	{
		if (initialized) {
			if (available && (surfaceWidth != width || surfaceHeight != height)) {
				if (!SetWindowPos(window, NULL, 0, 0, width, height,
					SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)) {
					Disable();
					return false;
				}
				surfaceWidth = width;
				surfaceHeight = height;
				viewportWidth = 0;
				viewportHeight = 0;
			}
			return available;
		}

		initialized = true;
		if (failed) {
			return false;
		}

		WNDCLASSA windowClass;
		memset(&windowClass, 0, sizeof(windowClass));
		windowClass.style = CS_OWNDC;
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = GetModuleHandle(NULL);
		windowClass.lpszClassName = "FBNeoNamcosOpenGLFrame";
		RegisterClassA(&windowClass);

		window = CreateWindowA(windowClass.lpszClassName, "", WS_POPUP,
			0, 0, width, height, NULL, NULL, windowClass.hInstance, NULL);
		if (window == NULL) {
			Disable();
			return false;
		}

		dc = GetDC(window);
		if (dc == NULL) {
			Disable();
			return false;
		}

		PIXELFORMATDESCRIPTOR format;
		memset(&format, 0, sizeof(format));
		format.nSize = sizeof(format);
		format.nVersion = 1;
		format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
		format.iPixelType = PFD_TYPE_RGBA;
		format.cColorBits = 32;
		format.cAlphaBits = 8;
		format.iLayerType = PFD_MAIN_PLANE;

		const INT32 pixelFormat = ChoosePixelFormat(dc, &format);
		if (pixelFormat == 0 || !SetPixelFormat(dc, pixelFormat, &format)) {
			Disable();
			return false;
		}

		context = wglCreateContext(dc);
		if (context == NULL || !wglMakeCurrent(dc, context)) {
			Disable();
			return false;
		}

		NamcosWglCreateContextAttribsProc createContextAttribs =
			(NamcosWglCreateContextAttribsProc)
			wglGetProcAddress("wglCreateContextAttribsARB");
		if (ValidProc((PROC)createContextAttribs)) {
			const INT32 contextAttributes[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
				WGL_CONTEXT_MINOR_VERSION_ARB, 3,
				WGL_CONTEXT_PROFILE_MASK_ARB,
				WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
				0
			};
			HGLRC modernContext = createContextAttribs(dc, NULL,
				contextAttributes);
			if (modernContext != NULL && wglMakeCurrent(dc, modernContext)) {
				wglDeleteContext(context);
				context = modernContext;
			} else if (modernContext != NULL) {
				wglDeleteContext(modernContext);
				wglMakeCurrent(dc, context);
			}
		}

		GLint maximumTextureSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
		const char *version = (const char *)glGetString(GL_VERSION);
		const char *renderer = (const char *)glGetString(GL_RENDERER);

		if (maximumTextureSize < 1024 || !VersionAtLeast(version, 1, 3) ||
			!NamcosOpenGLRendererIsHardware(renderer)) {
			Disable();
			return false;
		}
		if (VersionAtLeast(version, 2, 0)) {
			shaderMode = CreateShaderProgram();
			if (!shaderMode) {
				while (glGetError() != GL_NO_ERROR) { }
				if (vertexBuffer != 0 && deleteBuffers != NULL) {
					deleteBuffers(1, &vertexBuffer);
					vertexBuffer = 0;
				}
				if (program != 0 && deleteProgram != NULL) deleteProgram(program);
				program = 0;
			}
		}
		fullRasterizerCapable = false;

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		rgbUploadPixels = (UINT8 *)malloc(1024 * 1024 * 3);

		if (texture == 0 || rgbUploadPixels == NULL || glGetError() != GL_NO_ERROR) {
			Disable();
			return false;
		}
		rasterizerFailureReason = shaderMode ?
			(VersionAtLeast(version, 2, 0) ? 102 : 103) : 101;
		if (shaderMode && VersionAtLeast(version, 2, 0)) {
			fullRasterizerCapable = CreateRasterizerResources();
		}
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_DITHER);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		glEnable(GL_TEXTURE_2D);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT);

		surfaceWidth = width;
		surfaceHeight = height;
		available = true;
		return true;
	}

	void Disable()
	{
		failed = true;
		Shutdown();
	}

	HWND window;
	HDC dc;
	HGLRC context;
	GLuint texture;
	GLuint program;
	GLuint vertexBuffer;
	GLuint rasterTexture;
	GLuint rasterSampleTexture;
	GLuint rasterFramebuffer;
	GLuint rasterProgram;
	GLuint rasterVertexBuffer;
	UINT32 rasterVertexOffset;
	UINT32 rasterValidationCounter;
	GLint rasterPositionAttribute;
	GLint rasterColorAttribute;
	GLint rasterTextureAttribute;
	GLint rasterSampleUniform;
	GLint rasterTextureState0Uniform;
	GLint rasterTextureState1Uniform;
	GLint rasterFlagsUniform;
	GLint rasterMaskUniform;
	bool rasterMaskUniformValid;
	bool lastRasterDrawStp;
	bool lastRasterCheckStp;
	GLint positionAttribute;
	GLint positionRectUniform;
	GLint textureRectUniform;
	GLint verticalUniform;
	GLint verticalReconstructUniform;
	bool drawUniformsValid;
	bool textureUniformValid;
	GLfloat lastOutputLeft;
	GLfloat lastOutputRight;
	GLfloat lastTextureU0;
	GLfloat lastTextureV0;
	GLfloat lastTextureU1;
	GLfloat lastTextureV1;
	bool lastVertical;
	bool shaderMode;
	NamcosCreateShaderProc createShader;
	NamcosShaderSourceProc shaderSource;
	NamcosCompileShaderProc compileShader;
	NamcosGetShaderivProc getShaderiv;
	NamcosGetShaderInfoLogProc getShaderInfoLog;
	NamcosDeleteShaderProc deleteShader;
	NamcosCreateProgramProc createProgram;
	NamcosAttachShaderProc attachShader;
	NamcosLinkProgramProc linkProgram;
	NamcosGetProgramivProc getProgramiv;
	NamcosGetProgramInfoLogProc getProgramInfoLog;
	NamcosDeleteProgramProc deleteProgram;
	NamcosUseProgramProc useProgram;
	NamcosGetAttribLocationProc getAttribLocation;
	NamcosGetUniformLocationProc getUniformLocation;
	NamcosUniform1iProc uniform1i;
	NamcosUniform4fProc uniform4f;
	NamcosEnableVertexAttribArrayProc enableVertexAttribArray;
	NamcosVertexAttribPointerProc vertexAttribPointer;
	NamcosGenBuffersProc genBuffers;
	NamcosBindBufferProc bindBuffer;
	NamcosBufferDataProc bufferData;
	NamcosBufferSubDataProc bufferSubData;
	NamcosDeleteBuffersProc deleteBuffers;
	NamcosGenFramebuffersProc genFramebuffers;
	NamcosBindFramebufferProc bindFramebuffer;
	NamcosFramebufferTexture2DProc framebufferTexture2D;
	NamcosCheckFramebufferStatusProc checkFramebufferStatus;
	NamcosDeleteFramebuffersProc deleteFramebuffers;
	UINT8 *rasterTransferPixels;
	UINT8 *rgbUploadPixels;
	UINT8 *rgbUploadCache;
	size_t rgbUploadCacheBytes;
	INT32 rgbCacheWidth;
	INT32 rgbCacheHeight;
	INT32 rgbCacheDisplayX;
	INT32 rgbCacheDisplayY;
	UINT64 rgbRowGeneration[1024];
	bool rgbCacheValid;
	UINT8 rgbDenseFrames;
	INT32 surfaceWidth;
	INT32 surfaceHeight;
	INT32 viewportWidth;
	INT32 viewportHeight;
	UploadSlot uploadSlots[4];
	INT32 uploadSlotIndex;
	bool uploadModeValid;
	bool lastUploadRgb24;
	NamcosFrameUploadKey uploadFrameKey;
	bool uploadFrameValid;
	NamcosFrameOutputKey outputFrameKey;
	UINT64 outputRowGeneration[1024];
	bool outputFrameValid;
	bool fullRasterizerCapable;
	INT32 rasterizerFailureReason;
	bool rasterVramSynchronized;
	NamcosGlRasterUploadTracker rasterUploadTracker;
	NamcosPolyThreadPool *rasterThreadPool;
	NamcosGlRasterDirtyBounds rasterDirty;
	bool rasterStateActive;
	bool initialized;
	bool available;
	bool failed;
};

#elif defined(FBNEO_NAMCOS_OPENGL_ES)

#include <EGL/egl.h>
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
#include <GLES2/gl2.h>
#else
#include <GLES/gl.h>
#endif

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#ifndef GL_IMPLEMENTATION_COLOR_READ_TYPE_OES
#define GL_IMPLEMENTATION_COLOR_READ_TYPE_OES 0x8b9a
#endif
#ifndef GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES 0x8b9b
#endif
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80e1
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_FRAMEBUFFER_OES
#define GL_FRAMEBUFFER_OES 0x8d40
#endif
#ifndef GL_COLOR_EXT
#define GL_COLOR_EXT 0x1800
#endif
#ifndef GL_UNPACK_ROW_LENGTH_EXT
#define GL_UNPACK_ROW_LENGTH_EXT 0x0cf2
#endif

typedef void (*NamcosDiscardFramebufferProc)(GLenum target, GLsizei count,
	const GLenum *attachments);

class NamcosOpenGLFrameConverter
{
public:
	NamcosOpenGLFrameConverter()
		: display(EGL_NO_DISPLAY),
		  surface(EGL_NO_SURFACE),
		  context(EGL_NO_CONTEXT),
		  texture(0),
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		  rgbTexture(0),
		  vertexBuffer(0),
		  rasterTexture(0),
		  rasterSampleTexture(0),
		  rasterFramebuffer(0),
		  rasterProgram(0),
		  rasterVertexBuffer(0),
		  rasterVertexOffset(0),
		  rasterValidationCounter(0),
		  rasterPositionAttribute(-1),
		  rasterColorAttribute(-1),
		  rasterTextureAttribute(-1),
		  rasterSampleUniform(-1),
		  rasterTextureState0Uniform(-1),
		  rasterTextureState1Uniform(-1),
		  rasterFlagsUniform(-1),
		  rasterMaskUniform(-1),
		  rasterMaskUniformValid(false),
		  lastRasterDrawStp(false),
		  lastRasterCheckStp(false),
		  activeShaderIndex(0),
		  lastOutputLeft(0.0f),
		  lastOutputRight(0.0f),
		  lastVertical(false),
#endif
		  uploadPixels(NULL),
		  rasterTransferPixels(NULL),
		  rgbUploadCache(NULL),
		  rgbUploadCacheBytes(0),
		  rgbCacheWidth(0),
		  rgbCacheHeight(0),
		  rgbCacheDisplayX(0),
		  rgbCacheDisplayY(0),
		  rgbCacheValid(false),
		  rgbDenseFrames(0),
		  readPixels(NULL),
		  readPixelBytes(0),
		  surfaceWidth(0),
		  surfaceHeight(0),
		  viewportWidth(0),
		  viewportHeight(0),
		  surfacePixelBytes(0),
		  nativeReadFormat(GL_RGBA),
		  nativeReadType(GL_UNSIGNED_BYTE),
		  readRgb565Supported(false),
		  readRgb888Supported(false),
		  readBgraSupported(false),
		  unpackSubimage(false),
		  discardFramebuffer(NULL),
		  discardFramebufferVerified(false),
		  directPalette(NULL),
		  uploadSlotIndex(0),
		  uploadModeValid(false),
		  lastUploadRgb24(false),
		  uploadFrameValid(false),
		  outputFrameValid(false),
		  fullRasterizerCapable(false),
		  rasterVramSynchronized(false),
		  rasterThreadPool(NULL),
		  rasterStateActive(false),
		  initialized(false),
		  available(false),
		  failed(false)
	{
		#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		for (INT32 i = 0; i < 2; i++) {
			programs[i] = 0;
			positionRectUniforms[i] = -1;
			textureRectUniforms[i] = -1;
			verticalUniforms[i] = -1;
			verticalReconstructUniforms[i] = -1;
			swapRedBlueUniforms[i] = -1;
			drawUniformsValid[i] = false;
			textureUniformsValid[i] = false;
			lastSwapRedBlue[i] = false;
			lastTextureU0[i] = 0.0f;
			lastTextureV0[i] = 0.0f;
			lastTextureU1[i] = 0.0f;
			lastTextureV1[i] = 0.0f;
		}
		#endif
		#if !defined(FBNEO_NAMCOS_OPENGL_ES2)
		for (INT32 i = 0; i < 0x10000; i++) {
			uploadTable[i] = (UINT16)(((i & 0x001f) << 11) |
				((i & 0x03e0) << 1) | ((i & 0x7c00) >> 9) | 1);
		}
		#endif
		for (INT32 i = 0; i < 4; i++) {
			uploadSlots[i].pixels = NULL;
			uploadSlots[i].pixelCapacity = 0;
			uploadSlots[i].destinationX = 0;
			uploadSlots[i].destinationY = 0;
			uploadSlots[i].width = 0;
			uploadSlots[i].rows = 0;
			uploadSlots[i].denseFrames = 0;
			uploadSlots[i].valid = false;
		}
	}

	~NamcosOpenGLFrameConverter()
	{
		Shutdown();
	}

	bool Probe(INT32 width, INT32 height)
	{
		return width > 0 && height > 0 && EnsureInitialized(width, height, 4);
	}

	bool SupportsFullRasterizer() const
	{
		return available && fullRasterizerCapable;
	}

	bool SupportsRasterizerApi() const
	{
		return false;
	}

	INT32 RasterizerFailureReason() const
	{
		return -1;
	}

	bool SupportsOpenGL2() const { return false; }
	bool SupportsShaderMode() const { return false; }

	bool RasterizePacket(const NamcosGlRasterPacket *packet)
	{
		NamcosGlRasterPrimitive primitive;
		if (packet == NULL || !NamcosGlRasterDecodePacket(packet, &primitive)) {
			if (packet != NULL) SynchronizeVram(packet->vram,
				packet->vramGeneration != NULL ? *packet->vramGeneration : 0,
				packet->vramRowGeneration);
			return false;
		}
		if (!rasterVramSynchronized &&
			!UploadEs3RasterVram(packet->vram, packet->vramRowGeneration,
				packet->threadPool)) return false;
		rasterThreadPool = packet->threadPool;
		if (SubmitEs3RasterPrimitive(packet, &primitive)) return true;
		SynchronizeVram(packet->vram,
			packet->vramGeneration != NULL ? *packet->vramGeneration : 0,
			packet->vramRowGeneration);
		return false;
	}

	bool SynchronizeVram(UINT16 *vram, UINT64 generation, UINT64 *rowGeneration)
	{
		if (!rasterVramSynchronized) return true;
		const bool dirty = rasterDirty.valid;
		const INT32 firstDirtyRow = dirty ? rasterDirty.y1 : 0;
		const INT32 lastDirtyRow = dirty ? rasterDirty.y2 : -1;
		if (!ReadbackEs3RasterVram(vram)) return false;
		if (rowGeneration != NULL && dirty) {
			for (INT32 row = firstDirtyRow; row <= lastDirtyRow; row++) {
				rowGeneration[row] = generation;
			}
			rasterUploadTracker.RememberRange(rowGeneration, firstDirtyRow,
				lastDirtyRow - firstDirtyRow + 1);
		}
		rasterVramSynchronized = false;
		InvalidateUploadCaches();
		return true;
	}

	bool Convert(const NamcosFrameConvertContext *frame)
	{
		return ConvertInternal(frame, NULL, 0, NULL, 0);
	}

	bool ConvertDirect(const NamcosFrameConvertContext *frame, UINT8 *destination,
		INT32 pitch, const UINT32 *palette)
	{
		if (frame == NULL || destination == NULL || palette == NULL ||
			pitch < frame->outputWidth * 4) {
			return false;
		}
		return ConvertInternal(frame, destination, pitch, palette, 4);
	}

	bool ConvertDirect16(const NamcosFrameConvertContext *frame, UINT8 *destination,
		INT32 pitch, const UINT32 *palette)
	{
		if (frame == NULL || destination == NULL || palette == NULL ||
			pitch < frame->outputWidth * 2) {
			return false;
		}
		return ConvertInternal(frame, destination, pitch, palette, 2);
	}

	bool ConvertDirect24(const NamcosFrameConvertContext *frame, UINT8 *destination,
		INT32 pitch, const UINT32 *palette)
	{
		if (frame == NULL || destination == NULL || palette == NULL ||
			pitch < frame->outputWidth * 3) {
			return false;
		}
		return ConvertInternal(frame, destination, pitch, palette, 3);
	}

	bool ConvertInternal(const NamcosFrameConvertContext *frame,
		UINT8 *directDestination, INT32 directPitch, const UINT32 *palette,
		INT32 directBytes)
	{
		if (frame == NULL || frame->vram == NULL || frame->output == NULL ||
			(frame->vertical && (frame->rgb24 || frame->outputShiftX != 0)) ||
			frame->outputShiftX < 0 ||
			frame->outputShiftX >= frame->outputWidth ||
			frame->outputWidth <= 0 || frame->outputHeight <= 0 ||
			frame->sourceWidth <= 0 || frame->sourceWidth > 1024 ||
			frame->sourceHeight <= 0 || frame->sourceHeight > 1024) {
			return false;
		}

		if (!EnsureInitialized(frame->outputWidth, frame->outputHeight,
			directBytes)) {
			return false;
		}

		if ((eglGetCurrentContext() != context ||
			eglGetCurrentSurface(EGL_DRAW) != surface ||
			eglGetCurrentSurface(EGL_READ) != surface) &&
			!eglMakeCurrent(display, surface, surface, context)) {
			Disable();
			return false;
		}
		#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		EndEsRasterState();
		#endif
		if (viewportWidth != frame->outputWidth ||
			viewportHeight != frame->outputHeight) {
			glViewport(0, 0, frame->outputWidth, frame->outputHeight);
			viewportWidth = frame->outputWidth;
			viewportHeight = frame->outputHeight;
		}

		const bool rgb24 = frame->rgb24 != 0;
		if (!uploadModeValid || lastUploadRgb24 != rgb24) {
		#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			uploadFrameValid = false;
			outputFrameValid = false;
		#else
			InvalidateUploadCaches();
		#endif
			uploadModeValid = true;
			lastUploadRgb24 = rgb24;
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			glBindTexture(GL_TEXTURE_2D, rgb24 ? rgbTexture : texture);
			activeShaderIndex = rgb24 ? 1 : 0;
			glUseProgram(programs[activeShaderIndex]);
			if (glGetError() != GL_NO_ERROR) {
				Disable();
				return false;
			}
#endif
		}
		void *outputDestination = directDestination != NULL ?
			directDestination : (void *)frame->output;
		const INT32 outputPitch = directDestination != NULL ?
			directPitch : frame->outputWidth * 2;
		const INT32 outputBytes = directDestination != NULL ? directBytes : 2;
		const UINT32 *outputPalette = directDestination != NULL ? palette : NULL;
		if (outputFrameValid && NamcosFrameOutputMatches(&outputFrameKey, frame,
			outputDestination, outputPitch, outputBytes, outputPalette,
			outputRowGeneration)) {
			return true;
		}

		const bool rasterSource = rasterVramSynchronized && !rgb24;
		if (!rasterSource && (!uploadFrameValid ||
			!NamcosFrameUploadMatches(&uploadFrameKey, frame))) {
			if (rgb24) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				UploadRgb24(frame);
			} else {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
				uploadSlotIndex = 0;
				INT32 uploadFirstRow;
				INT32 uploadRows;
				NamcosFrameGetObservedRows(frame, &uploadFirstRow, &uploadRows);
				const INT32 uploadX = frame->displayX & 0x3ff;
				const INT32 uploadY = (frame->displayY + uploadFirstRow) & 0x3ff;
				const INT32 firstColumns = frame->sourceWidth < 1024 - uploadX ?
					frame->sourceWidth : 1024 - uploadX;
				const INT32 firstRows = uploadRows < 1024 - uploadY ?
					uploadRows : 1024 - uploadY;
				UploadRect(frame->vram + uploadY * 1024 + uploadX, uploadX, uploadY,
					firstColumns, firstRows, frame->vramRowGeneration, frame->threadPool);
				if (firstColumns < frame->sourceWidth) {
					UploadRect(frame->vram + uploadY * 1024, 0, uploadY,
						frame->sourceWidth - firstColumns, firstRows,
						frame->vramRowGeneration, frame->threadPool);
				}
				if (firstRows < uploadRows) {
					UploadRect(frame->vram + uploadX, uploadX, 0, firstColumns,
						uploadRows - firstRows, frame->vramRowGeneration,
						frame->threadPool);
					if (firstColumns < frame->sourceWidth) {
						UploadRect(frame->vram, 0, 0, frame->sourceWidth - firstColumns,
							uploadRows - firstRows, frame->vramRowGeneration,
							frame->threadPool);
					}
				}
			}
			NamcosFrameRememberUpload(&uploadFrameKey, frame);
			uploadFrameValid = true;
		}

		if (frame->outputShiftX != 0) glClear(GL_COLOR_BUFFER_BIT);

		const GLfloat textureX = frame->rgb24 ? 0.0f : (GLfloat)frame->displayX;
		const GLfloat textureY = frame->rgb24 ? 0.0f : (GLfloat)frame->displayY;
		const GLfloat u0 = (textureX + 0.5f - (frame->vertical ?
			0.5f * frame->sourceWidth / frame->outputHeight : 0.0f)) / 1024.0f;
		const GLfloat sourceTop = (GLfloat)frame->cropTop * frame->sourceHeight /
			frame->outputHeight;
		const GLfloat sourceRows = (GLfloat)frame->cropHeight * frame->sourceHeight /
			frame->outputHeight;
		const GLfloat v0 = frame->vertical ?
			(textureY + frame->sourceHeight - 1 - frame->cropTop + 0.5f +
			0.5f * frame->cropHeight / frame->outputWidth) / 1024.0f :
			(textureY + sourceTop + 0.5f) / 1024.0f;
		const GLfloat u1 = u0 + ((GLfloat)frame->sourceWidth / 1024.0f);
		const GLfloat v1 = v0 + (frame->vertical ?
			-(GLfloat)frame->cropHeight / 1024.0f : sourceRows / 1024.0f);
		const GLfloat sourceV0 = rasterSource ? 1.0f - v0 : v0;
		const GLfloat sourceV1 = rasterSource ? 1.0f - v1 : v1;
		const GLfloat outputLeft = -1.0f +
			(2.0f * frame->outputShiftX / frame->outputWidth);
		const GLfloat outputRight = outputLeft + 2.0f;
		const bool directReadBgra = readBgraSupported && directBytes == 4 &&
			directPitch == frame->outputWidth * 4;
		const bool directReadRgb =
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			readRgb888Supported && directBytes == 3 &&
			directPitch == frame->outputWidth * 3;
#else
			false;
#endif
		const bool directReadRgba = !directReadBgra && nativeReadFormat == GL_RGBA &&
			nativeReadType == GL_UNSIGNED_BYTE && directBytes == 4 &&
			directPitch == frame->outputWidth * 4;
		if (discardFramebuffer != NULL && frame->outputShiftX == 0) {
			const GLenum attachment = GL_COLOR_EXT;
			discardFramebuffer(GL_FRAMEBUFFER_OES, 1, &attachment);
			if (!discardFramebufferVerified) {
				if (glGetError() != GL_NO_ERROR) {
					discardFramebuffer = NULL;
				} else {
					discardFramebufferVerified = true;
				}
			}
		}

		#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		const INT32 sourceShaderIndex = rasterSource ? 1 : activeShaderIndex;
		if (sourceShaderIndex != activeShaderIndex) glUseProgram(programs[sourceShaderIndex]);
		if (lastOutputLeft != outputLeft || lastOutputRight != outputRight ||
			lastVertical != (frame->vertical != 0)) {
			drawUniformsValid[0] = false;
			drawUniformsValid[1] = false;
			lastOutputLeft = outputLeft;
			lastOutputRight = outputRight;
			lastVertical = frame->vertical != 0;
		}
		if (!drawUniformsValid[sourceShaderIndex]) {
			glUniform4f(positionRectUniforms[sourceShaderIndex], outputLeft, -1.0f,
				outputRight, 1.0f);
			glUniform1i(verticalUniforms[sourceShaderIndex], frame->vertical ? 1 : 0);
		}
		glUniform1i(verticalReconstructUniforms[sourceShaderIndex],
			frame->verticalReconstruct2x ? 1 : 0);
		if (!drawUniformsValid[sourceShaderIndex] ||
			lastSwapRedBlue[sourceShaderIndex] !=
				(directReadRgba || directReadRgb)) {
			glUniform1i(swapRedBlueUniforms[sourceShaderIndex],
				(directReadRgba || directReadRgb) ? 1 : 0);
			lastSwapRedBlue[sourceShaderIndex] = directReadRgba || directReadRgb;
		}
		if (!textureUniformsValid[sourceShaderIndex] ||
			lastTextureU0[sourceShaderIndex] != u0 ||
			lastTextureV0[sourceShaderIndex] != sourceV0 ||
			lastTextureU1[sourceShaderIndex] != u1 ||
			lastTextureV1[sourceShaderIndex] != sourceV1) {
			glUniform4f(textureRectUniforms[sourceShaderIndex], u0, sourceV0, u1, sourceV1);
			lastTextureU0[sourceShaderIndex] = u0;
			lastTextureV0[sourceShaderIndex] = sourceV0;
			lastTextureU1[sourceShaderIndex] = u1;
			lastTextureV1[sourceShaderIndex] = sourceV1;
			textureUniformsValid[sourceShaderIndex] = true;
		}
		drawUniformsValid[sourceShaderIndex] = true;
		#else
		const GLfloat outputSpan = outputRight - outputLeft;
		const GLfloat textureUSpan = u1 - u0;
		const GLfloat textureVSpan = sourceV1 - sourceV0;
		const GLfloat vertices[] = {
			outputLeft, -1.0f,
			outputLeft + outputSpan * 2.0f, -1.0f,
			outputLeft, 3.0f
		};
		const GLfloat textureCoordinates[] = {
			u0, sourceV0,
			frame->vertical ? u0 : u0 + textureUSpan * 2.0f,
			frame->vertical ? sourceV0 + textureVSpan * 2.0f : sourceV0,
			frame->vertical ? u0 + textureUSpan * 2.0f : u0,
			frame->vertical ? sourceV0 : sourceV0 + textureVSpan * 2.0f
		};
		glVertexPointer(2, GL_FLOAT, 0, vertices);
		glTexCoordPointer(2, GL_FLOAT, 0, textureCoordinates);
		#endif
		glBindTexture(GL_TEXTURE_2D, rasterSource ? rasterTexture :
			(rgb24 ? rgbTexture : texture));
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindTexture(GL_TEXTURE_2D, rgb24 ? rgbTexture : texture);
		#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		if (sourceShaderIndex != activeShaderIndex) glUseProgram(programs[activeShaderIndex]);
		#endif

		if (readRgb565Supported && directBytes == 2 &&
			directPitch == frame->outputWidth * 2 &&
			NamcosPaletteIsRgb565(palette)) {
			glPixelStorei(GL_PACK_ALIGNMENT, 2);
			glReadPixels(0, 0, frame->outputWidth, frame->outputHeight,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, directDestination);
			if (glGetError() == GL_NO_ERROR) {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration);
				outputFrameValid = true;
				return true;
			}
			readRgb565Supported = false;
		}

		if (directReadRgb) {
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadPixels(0, 0, frame->outputWidth, frame->outputHeight,
				GL_RGB, GL_UNSIGNED_BYTE, directDestination);
			if (glGetError() == GL_NO_ERROR) {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration);
				outputFrameValid = true;
				return true;
			}
			readRgb888Supported = false;
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			glUniform1i(swapRedBlueUniforms[activeShaderIndex], 0);
			lastSwapRedBlue[activeShaderIndex] = false;
			glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
		}

		if (directReadBgra) {
			glPixelStorei(GL_PACK_ALIGNMENT, 4);
			glReadPixels(0, 0, frame->outputWidth, frame->outputHeight,
				GL_BGRA_EXT, GL_UNSIGNED_BYTE, directDestination);
			if (glGetError() != GL_NO_ERROR) {
				readBgraSupported = false;
			} else {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration);
				outputFrameValid = true;
				return true;
			}
		}

		if (directReadRgba) {
			glPixelStorei(GL_PACK_ALIGNMENT, 4);
			glReadPixels(0, 0, frame->outputWidth, frame->outputHeight,
				GL_RGBA, GL_UNSIGNED_BYTE, directDestination);
			if (glGetError() != GL_NO_ERROR) {
				Disable();
				return false;
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration);
			outputFrameValid = true;
			return true;
		}

		const bool nativeReadBgra = nativeReadFormat == GL_BGRA_EXT &&
			nativeReadType == GL_UNSIGNED_BYTE;
		if (nativeReadBgra && directBytes == 4 &&
			directPitch == frame->outputWidth * 4) {
			glPixelStorei(GL_PACK_ALIGNMENT, 4);
			glReadPixels(0, 0, frame->outputWidth, frame->outputHeight,
				GL_BGRA_EXT, GL_UNSIGNED_BYTE, directDestination);
			if (glGetError() != GL_NO_ERROR) {
				Disable();
				return false;
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration);
			outputFrameValid = true;
			return true;
		}

		const bool nativeRead16 = nativeReadType == GL_UNSIGNED_SHORT_5_5_5_1 ||
			nativeReadType == GL_UNSIGNED_SHORT_5_6_5;
		const size_t bytes = (size_t)frame->outputWidth * frame->outputHeight *
			(nativeRead16 ? 2 : 4);
		UINT8 *readDestination = readPixels;
		if (nativeRead16 && directDestination == NULL) {
			readDestination = (UINT8 *)frame->output;
		} else if (nativeRead16 && directBytes == 2 &&
			directPitch == frame->outputWidth * 2) {
			readDestination = directDestination;
		}
		if (readDestination == readPixels && !PrepareReadPixels(bytes)) {
			Disable();
			return false;
		}

		glPixelStorei(GL_PACK_ALIGNMENT, nativeRead16 ? 2 : 1);
		glReadPixels(0, 0, frame->outputWidth, frame->outputHeight,
			nativeReadFormat, nativeReadType, readDestination);
		if (glGetError() != GL_NO_ERROR) {
			Disable();
			return false;
		}

		if (nativeRead16) {
			if (nativeReadType == GL_UNSIGNED_SHORT_5_6_5 && directBytes == 2 &&
				directPitch == frame->outputWidth * 2 &&
				NamcosPaletteIsRgb565(palette)) {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration);
				outputFrameValid = true;
				return true;
			}
			if (directDestination != NULL && directPalette != palette) {
				PrepareDirectReadTable(palette);
			}
			NamcosGlReadConvertContext convertContext;
			convertContext.source = readDestination;
			convertContext.destination = directDestination;
			convertContext.indexedDestination = frame->output;
			convertContext.readTable = readTable;
			convertContext.directReadTable = directReadTable;
			convertContext.palette = palette;
			convertContext.width = frame->outputWidth;
			convertContext.sourcePitch = frame->outputWidth * 2;
			convertContext.destinationPitch = directPitch;
			convertContext.destinationBytes = directBytes;
			convertContext.redOffset = 0;
			convertContext.blueOffset = 2;
			NamcosGlConvertReadRows(frame, NamcosGlConvertNative16Rows,
				&convertContext);
			NamcosFrameRememberOutput(&outputFrameKey, frame, outputDestination,
				outputPitch, outputBytes, outputPalette, outputRowGeneration);
			outputFrameValid = true;
			return true;
		}
		if (nativeReadBgra && directBytes == 4) {
			const size_t rowBytes = (size_t)frame->outputWidth * 4;
			for (INT32 y = 0; y < frame->outputHeight; y++) {
				memcpy(directDestination + y * directPitch,
					readPixels + y * rowBytes, rowBytes);
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration);
			outputFrameValid = true;
			return true;
		}

		const INT32 redOffset = nativeReadBgra ? 2 : 0;
		const INT32 blueOffset = nativeReadBgra ? 0 : 2;
		NamcosGlReadConvertContext convertContext;
		convertContext.source = readPixels;
		convertContext.destination = directDestination;
		convertContext.indexedDestination = frame->output;
		convertContext.readTable = readTable;
		convertContext.directReadTable = directReadTable;
		convertContext.palette = palette;
		convertContext.width = frame->outputWidth;
		convertContext.sourcePitch = frame->outputWidth * 4;
		convertContext.destinationPitch = directPitch;
		convertContext.destinationBytes = directBytes;
		convertContext.redOffset = redOffset;
		convertContext.blueOffset = blueOffset;
		NamcosGlConvertReadRows(frame, NamcosGlConvertRgbaRows, &convertContext);

		NamcosFrameRememberOutput(&outputFrameKey, frame, outputDestination,
			outputPitch, outputBytes, outputPalette, outputRowGeneration);
		outputFrameValid = true;
		return true;
	}

	void InvalidatePalette()
	{
		directPalette = NULL;
		outputFrameValid = false;
	}

	void InvalidateVram()
	{
		InvalidateUploadCaches();
		rasterVramSynchronized = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
	}

	void Shutdown()
	{
		if (display != EGL_NO_DISPLAY) {
			if (context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE) {
				eglMakeCurrent(display, surface, surface, context);
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
				EndEsRasterState();
				DestroyEs3RasterizerResources();
				if (vertexBuffer != 0) glDeleteBuffers(1, &vertexBuffer);
				vertexBuffer = 0;
				if (rgbTexture != 0) glDeleteTextures(1, &rgbTexture);
				rgbTexture = 0;
				for (INT32 i = 0; i < 2; i++) {
					if (programs[i] != 0) glDeleteProgram(programs[i]);
					programs[i] = 0;
					drawUniformsValid[i] = false;
					textureUniformsValid[i] = false;
				}
#endif
				if (texture != 0) glDeleteTextures(1, &texture);
				texture = 0;
			}
			eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
			if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
			eglTerminate(display);
		}

		free(uploadPixels);
		free(rasterTransferPixels);
		free(rgbUploadCache);
		free(readPixels);
		for (INT32 i = 0; i < 4; i++) {
			free(uploadSlots[i].pixels);
			uploadSlots[i].pixels = NULL;
			uploadSlots[i].pixelCapacity = 0;
			uploadSlots[i].valid = false;
		}
		uploadPixels = NULL;
		rasterTransferPixels = NULL;
		rgbUploadCache = NULL;
		rgbUploadCacheBytes = 0;
		rgbCacheValid = false;
		rgbDenseFrames = 0;
		readPixels = NULL;
		readPixelBytes = 0;
		display = EGL_NO_DISPLAY;
		surface = EGL_NO_SURFACE;
		context = EGL_NO_CONTEXT;
		surfaceWidth = 0;
		surfaceHeight = 0;
		viewportWidth = 0;
		viewportHeight = 0;
		surfacePixelBytes = 0;
		nativeReadFormat = GL_RGBA;
		nativeReadType = GL_UNSIGNED_BYTE;
		readRgb565Supported = false;
		readRgb888Supported = false;
		readBgraSupported = false;
		unpackSubimage = false;
		discardFramebuffer = NULL;
		discardFramebufferVerified = false;
		directPalette = NULL;
		uploadModeValid = false;
		outputFrameValid = false;
		initialized = false;
		available = false;
	}

private:
	struct UploadSlot
	{
		UINT16 *pixels;
		size_t pixelCapacity;
		UINT64 rowGeneration[1024];
		INT32 destinationX;
		INT32 destinationY;
		INT32 width;
		INT32 rows;
		UINT8 denseFrames;
		bool valid;
	};

	void InvalidateUploadCaches()
	{
		for (INT32 i = 0; i < 4; i++) {
			uploadSlots[i].denseFrames = 0;
			uploadSlots[i].valid = false;
		}
		rgbCacheValid = false;
		rgbDenseFrames = 0;
		uploadFrameValid = false;
		outputFrameValid = false;
	}

#if defined(FBNEO_NAMCOS_OPENGL_ES2)
	GLuint CompileShader(GLenum type, const GLchar *source)
	{
		const GLuint shader = glCreateShader(type);
		if (shader == 0) return 0;
		glShaderSource(shader, 1, &source, NULL);
		glCompileShader(shader);
		GLint compiled = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled != GL_TRUE) {
			glDeleteShader(shader);
			return 0;
		}
		return shader;
	}

	bool CreateProgramVariant(INT32 index, GLuint vertexShader,
		const GLchar *fragmentSource)
	{
		const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER,
			fragmentSource);
		if (fragmentShader == 0) return false;
		programs[index] = glCreateProgram();
		if (programs[index] != 0) {
			glAttachShader(programs[index], vertexShader);
			glAttachShader(programs[index], fragmentShader);
			glBindAttribLocation(programs[index], 0, "aCorner");
			glLinkProgram(programs[index]);
		}
		glDeleteShader(fragmentShader);
		if (programs[index] == 0) return false;
		GLint linked = GL_FALSE;
		glGetProgramiv(programs[index], GL_LINK_STATUS, &linked);
		if (linked != GL_TRUE) return false;

		const GLint positionAttribute = glGetAttribLocation(programs[index],
			"aCorner");
		positionRectUniforms[index] = glGetUniformLocation(programs[index],
			"uPositionRect");
		textureRectUniforms[index] = glGetUniformLocation(programs[index],
			"uTextureRect");
		verticalUniforms[index] = glGetUniformLocation(programs[index],
			"uVertical");
		verticalReconstructUniforms[index] = glGetUniformLocation(programs[index],
			"uVerticalReconstruct2x");
		swapRedBlueUniforms[index] = glGetUniformLocation(programs[index],
			"uSwapRedBlue");
		const GLint textureUniform = glGetUniformLocation(programs[index],
			"uTexture");
		if (positionAttribute != 0 || positionRectUniforms[index] < 0 ||
			textureRectUniforms[index] < 0 || verticalUniforms[index] < 0 ||
			verticalReconstructUniforms[index] < 0 ||
			swapRedBlueUniforms[index] < 0 || textureUniform < 0) {
			return false;
		}
		glUseProgram(programs[index]);
		glUniform1i(textureUniform, 0);
		return glGetError() == GL_NO_ERROR;
	}

	bool CreateProgram()
	{
		static const GLchar vertexSource[] =
			"attribute vec2 aCorner;\n"
			"uniform vec4 uPositionRect;\n"
			"uniform vec4 uTextureRect;\n"
			"uniform int uVertical;\n"
			"varying vec2 vTexCoord;\n"
			"void main() {\n"
			"  vec2 position = mix(uPositionRect.xy, uPositionRect.zw, aCorner);\n"
			"  gl_Position = vec4(position, 0.0, 1.0);\n"
			"  if (uVertical != 0)\n"
			"    vTexCoord = vec2(mix(uTextureRect.x, uTextureRect.z, aCorner.y),"
			" mix(uTextureRect.y, uTextureRect.w, aCorner.x));\n"
			"  else vTexCoord = mix(uTextureRect.xy, uTextureRect.zw, aCorner);\n"
			"}\n";
		static const GLchar packedFragmentSource[] =
			"precision mediump float;\n"
			"uniform sampler2D uTexture;\n"
			"uniform vec4 uTextureRect;\n"
			"uniform int uVerticalReconstruct2x;\n"
			"uniform int uSwapRedBlue;\n"
			"varying vec2 vTexCoord;\n"
			"vec4 decodeColor(vec2 coord) {\n"
			"  vec4 sampleColor = texture2D(uTexture, coord);\n"
			"  float lowByte = floor(sampleColor.r * 255.0 + 0.5);\n"
			"  float highByte = floor(sampleColor.a * 255.0 + 0.5);\n"
			"  float red = mod(lowByte, 32.0);\n"
			"  float green = floor(lowByte / 32.0) + mod(highByte, 4.0) * 8.0;\n"
			"  float blue = mod(floor(highByte / 4.0), 32.0);\n"
			"  return vec4(red, green, blue, 31.0) / 31.0;\n"
			"}\n"
			"void main() {\n"
			"  vec4 outputColor;\n"
			"  if (uVerticalReconstruct2x == 0) outputColor = decodeColor(vTexCoord);\n"
			"  else {\n"
			"    float y = vTexCoord.y * 1024.0 - 0.5;\n"
			"    float f = smoothstep(0.0, 1.0, fract(y));\n"
			"    vec2 p = vec2(vTexCoord.x, (floor(y) + 0.5) / 1024.0);\n"
			"    outputColor = mix(decodeColor(p), decodeColor(p + vec2(0.0, 1.0 / 1024.0)), f);\n"
			"  }\n"
			"  gl_FragColor = uSwapRedBlue != 0 ? outputColor.bgra : outputColor;\n"
			"}\n";
		static const GLchar rgbFragmentSource[] =
			"precision mediump float;\n"
			"uniform sampler2D uTexture;\n"
			"uniform vec4 uTextureRect;\n"
			"uniform int uVerticalReconstruct2x;\n"
			"uniform int uSwapRedBlue;\n"
			"varying vec2 vTexCoord;\n"
			"void main() {\n"
			"  vec4 outputColor;\n"
			"  if (uVerticalReconstruct2x == 0) outputColor = texture2D(uTexture, vTexCoord);\n"
			"  else {\n"
			"    float y = vTexCoord.y * 1024.0 - 0.5;\n"
			"    float f = smoothstep(0.0, 1.0, fract(y));\n"
			"    vec2 p = vec2(vTexCoord.x, (floor(y) + 0.5) / 1024.0);\n"
			"    outputColor = mix(texture2D(uTexture, p), texture2D(uTexture, p + vec2(0.0, 1.0 / 1024.0)), f);\n"
			"  }\n"
			"  gl_FragColor = uSwapRedBlue != 0 ? outputColor.bgra : outputColor;\n"
			"}\n";

		const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
		if (vertexShader == 0) return false;
		const bool created = CreateProgramVariant(0, vertexShader,
			packedFragmentSource) && CreateProgramVariant(1, vertexShader,
			rgbFragmentSource);
		glDeleteShader(vertexShader);
		if (!created) return false;
		static const GLfloat corners[] = {
			0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f
		};
		glGenBuffers(1, &vertexBuffer);
		if (vertexBuffer == 0) return false;
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
		glUseProgram(programs[0]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		return glGetError() == GL_NO_ERROR;
	}

	void DestroyEs3RasterizerResources()
	{
		if (rasterVertexBuffer != 0) glDeleteBuffers(1, &rasterVertexBuffer);
		if (rasterProgram != 0) glDeleteProgram(rasterProgram);
		if (rasterFramebuffer != 0) glDeleteFramebuffers(1, &rasterFramebuffer);
		if (rasterTexture != 0) glDeleteTextures(1, &rasterTexture);
		if (rasterSampleTexture != 0) glDeleteTextures(1,
			&rasterSampleTexture);
		rasterVertexBuffer = 0;
		rasterVertexOffset = 0;
		rasterValidationCounter = 0;
		rasterProgram = 0;
		rasterFramebuffer = 0;
		rasterTexture = 0;
		rasterSampleTexture = 0;
		rasterPositionAttribute = -1;
		rasterColorAttribute = -1;
		rasterTextureAttribute = -1;
		rasterSampleUniform = -1;
		rasterTextureState0Uniform = -1;
		rasterTextureState1Uniform = -1;
		rasterFlagsUniform = -1;
		rasterMaskUniform = -1;
		rasterMaskUniformValid = false;
		rasterVramSynchronized = false;
		rasterStateActive = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
	}

	bool CreateEs3RasterizerResources(bool useEs3)
	{
		static const GLchar vertexSource[] =
			"#version 300 es\n"
			"in vec2 aPosition;\n"
			"in vec3 aColor;\n"
			"in vec2 aTexture;\n"
			"out vec3 vColor;\n"
			"out vec2 vTexture;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0,"
			" 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor;\n"
			" vTexture = aTexture;\n"
			"}\n";
		static const GLchar fragmentSource[] =
			"#version 300 es\n"
			"precision highp float;\n"
			"precision highp int;\n"
			"in vec3 vColor;\n"
			"in vec2 vTexture;\n"
			"uniform sampler2D uVram;\n"
			"uniform vec4 uTextureState0;\n"
			"uniform vec4 uTextureState1;\n"
			"uniform vec4 uFlags;\n"
			"uniform vec4 uMask;\n"
			"out vec4 outputColor;\n"
			"int wordAt(int x, int y) {\n"
			" vec4 c = texelFetch(uVram, ivec2(x & 1023, 1023 - (y & 1023)), 0);\n"
			" ivec3 q = ivec3(floor(c.rgb * 31.0 + 0.5));\n"
			" return q.r | (q.g << 5) | (q.b << 10) | (c.a >= 0.5 ? 32768 : 0);\n"
			"}\n"
			"vec4 unpackWord(int word) {\n"
			" return vec4(float(word & 31), float((word >> 5) & 31),\n"
			"  float((word >> 10) & 31), (word & 32768) != 0 ? 1.0 : 0.0);\n"
			"}\n"
			"int textureWord() {\n"
			" int u = int(floor(vTexture.x)) & int(uTextureState1.x);\n"
			" int v = int(floor(vTexture.y)) & int(uTextureState1.y);\n"
			" int tx = int(uTextureState0.x); int ty = int(uTextureState0.y);\n"
			" int mode = int(uTextureState0.z); bool interleaved = uTextureState0.w != 0.0;\n"
			" int x = tx; int y = (ty + (v & 255)) & 1023; int data;\n"
			" if (mode == 0) {\n"
			"  if (interleaved) { x += ((u >> 2) & ~60) + ((v << 2) & 60); y = ty + (v & ~15) + ((u >> 4) & 15); }\n"
			"  else x += (u & 255) >> 2;\n"
			"  data = (wordAt(x, y) >> ((u & 3) << 2)) & 15;\n"
			"  return wordAt(int(uTextureState1.z) + data, int(uTextureState1.w));\n"
			" }\n"
			" if (mode == 1) {\n"
			"  if (interleaved) { x += ((u >> 1) & ~120) + ((u << 2) & 64) + ((v << 3) & 56); y = ty + (v & ~7) + ((u >> 5) & 7); }\n"
			"  else x += (u & 255) >> 1;\n"
			"  data = wordAt(x, y); data = (u & 1) != 0 ? (data >> 8) : (data & 255);\n"
			"  return wordAt(int(uTextureState1.z) + data, int(uTextureState1.w));\n"
			" }\n"
			" if (mode == 2) return wordAt(tx + (u & 255), y);\n"
			" return 0;\n"
			"}\n"
			"void main() {\n"
			" bool textured = uFlags.x != 0.0; int texel = textured ? textureWord() : 0;\n"
			" if (textured && texel == 0) discard;\n"
			" vec4 color = textured ? unpackWord(texel) : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && uFlags.y == 0.0) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = uFlags.z != 0.0 && (!textured || (texel & 32768) != 0);\n"
			" int backgroundWord = 0;\n"
			" if (uMask.y != 0.0 || blend) backgroundWord = wordAt(int(gl_FragCoord.x), 1023 - int(gl_FragCoord.y));\n"
			" if (uMask.y != 0.0 && (backgroundWord & 32768) != 0) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = unpackWord(backgroundWord).rgb; int abr = int(uFlags.w);\n"
			"  if (abr == 0) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr == 1) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr == 2) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  if (textured) color.a = 1.0; else color.a = 0.0;\n"
			" }\n"
			" if (uMask.x != 0.0) color.a = 1.0;\n"
			" outputColor = vec4(color.rgb / 31.0, color.a);\n"
			"}\n";
		static const GLchar es2VertexSource[] =
			"attribute vec2 aPosition;\n"
			"attribute vec3 aColor;\n"
			"attribute vec2 aTexture;\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0, 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor; vTexture = aTexture;\n"
			"}\n";
		static const GLchar es2FragmentSource[] =
			"precision highp float;\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"uniform sampler2D uVram;\n"
			"uniform vec4 uTextureState0;\n"
			"uniform vec4 uTextureState1;\n"
			"uniform vec4 uFlags;\n"
			"uniform vec4 uMask;\n"
			"float and8(float a, float b) {\n"
			" float result = 0.0; float bitValue = 1.0;\n"
			" for (int i = 0; i < 8; i++) {\n"
			"  if (mod(floor(a / bitValue), 2.0) >= 1.0 && mod(floor(b / bitValue), 2.0) >= 1.0) result += bitValue;\n"
			"  bitValue *= 2.0;\n"
			" } return result;\n"
			"}\n"
			"float wordAt(float x, float y) {\n"
			" float sx = mod(floor(x), 1024.0); float sy = mod(floor(y), 1024.0);\n"
			" vec4 c = texture2D(uVram, vec2((sx + 0.5) / 1024.0, (1023.0 - sy + 0.5) / 1024.0));\n"
			" vec3 q = floor(c.rgb * 31.0 + 0.5);\n"
			" return q.r + q.g * 32.0 + q.b * 1024.0 + (c.a >= 0.5 ? 32768.0 : 0.0);\n"
			"}\n"
			"vec4 unpackWord(float word) {\n"
			" return vec4(mod(word, 32.0), mod(floor(word / 32.0), 32.0), mod(floor(word / 1024.0), 32.0), word >= 32768.0 ? 1.0 : 0.0);\n"
			"}\n"
			"float textureWord() {\n"
			" float u = and8(floor(vTexture.x), uTextureState1.x);\n"
			" float v = and8(floor(vTexture.y), uTextureState1.y);\n"
			" float tx = uTextureState0.x; float ty = uTextureState0.y;\n"
			" float mode = uTextureState0.z; bool interleaved = uTextureState0.w != 0.0;\n"
			" float x = tx; float y = mod(ty + mod(v, 256.0), 1024.0); float data;\n"
			" if (mode < 0.5) {\n"
			"  if (interleaved) { x += and8(floor(u / 4.0), 195.0) + and8(v * 4.0, 60.0); y = ty + (v - mod(v, 16.0)) + mod(floor(u / 16.0), 16.0); }\n"
			"  else x += floor(mod(u, 256.0) / 4.0);\n"
			"  data = mod(floor(wordAt(x, y) / pow(16.0, mod(u, 4.0))), 16.0);\n"
			"  return wordAt(uTextureState1.z + data, uTextureState1.w);\n"
			" }\n"
			" if (mode < 1.5) {\n"
			"  if (interleaved) { x += and8(floor(u / 2.0), 135.0) + and8(u * 4.0, 64.0) + and8(v * 8.0, 56.0); y = ty + (v - mod(v, 8.0)) + mod(floor(u / 32.0), 8.0); }\n"
			"  else x += floor(mod(u, 256.0) / 2.0);\n"
			"  data = wordAt(x, y); data = mod(u, 2.0) >= 1.0 ? floor(data / 256.0) : mod(data, 256.0);\n"
			"  return wordAt(uTextureState1.z + data, uTextureState1.w);\n"
			" }\n"
			" if (mode < 2.5) return wordAt(tx + mod(u, 256.0), y);\n"
			" return 0.0;\n"
			"}\n"
			"void main() {\n"
			" bool textured = uFlags.x != 0.0; float texel = textured ? textureWord() : 0.0;\n"
			" if (textured && texel < 0.5) discard;\n"
			" vec4 color = textured ? unpackWord(texel) : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && uFlags.y == 0.0) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = uFlags.z != 0.0 && (!textured || texel >= 32768.0);\n"
			" float backgroundWord = 0.0;\n"
			" if (uMask.y != 0.0 || blend) backgroundWord = wordAt(floor(gl_FragCoord.x), 1023.0 - floor(gl_FragCoord.y));\n"
			" if (uMask.y != 0.0 && backgroundWord >= 32768.0) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = unpackWord(backgroundWord).rgb; float abr = uFlags.w;\n"
			"  if (abr < 0.5) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr < 1.5) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr < 2.5) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  color.a = textured ? 1.0 : 0.0;\n"
			" }\n"
			" if (uMask.x != 0.0) color.a = 1.0;\n"
			" gl_FragColor = vec4(color.rgb / 31.0, color.a);\n"
			"}\n";
		const GLuint vertex = CompileShader(GL_VERTEX_SHADER,
			useEs3 ? vertexSource : es2VertexSource);
		const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER,
			useEs3 ? fragmentSource : es2FragmentSource);
		if (vertex == 0 || fragment == 0) {
			if (vertex != 0) glDeleteShader(vertex);
			if (fragment != 0) glDeleteShader(fragment);
			return false;
		}
		rasterProgram = glCreateProgram();
		if (rasterProgram != 0) {
			glAttachShader(rasterProgram, vertex);
			glAttachShader(rasterProgram, fragment);
			glLinkProgram(rasterProgram);
		}
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		GLint linked = GL_FALSE;
		if (rasterProgram != 0) glGetProgramiv(rasterProgram, GL_LINK_STATUS,
			&linked);
		if (linked != GL_TRUE) {
			DestroyEs3RasterizerResources();
			return false;
		}
		rasterPositionAttribute = glGetAttribLocation(rasterProgram,
			"aPosition");
		rasterColorAttribute = glGetAttribLocation(rasterProgram, "aColor");
		rasterTextureAttribute = glGetAttribLocation(rasterProgram, "aTexture");
		rasterSampleUniform = glGetUniformLocation(rasterProgram, "uVram");
		rasterTextureState0Uniform = glGetUniformLocation(rasterProgram,
			"uTextureState0");
		rasterTextureState1Uniform = glGetUniformLocation(rasterProgram,
			"uTextureState1");
		rasterFlagsUniform = glGetUniformLocation(rasterProgram, "uFlags");
		rasterMaskUniform = glGetUniformLocation(rasterProgram, "uMask");
		if (rasterPositionAttribute < 0 || rasterColorAttribute < 0 ||
			rasterTextureAttribute < 0 || rasterSampleUniform < 0 ||
			rasterTextureState0Uniform < 0 || rasterTextureState1Uniform < 0 ||
			rasterFlagsUniform < 0 || rasterMaskUniform < 0) {
			DestroyEs3RasterizerResources();
			return false;
		}
		glUseProgram(rasterProgram);
		glUniform1i(rasterSampleUniform, 0);
		glUniform4f(rasterTextureState0Uniform, 0.0f, 0.0f, 0.0f, 0.0f);
		glUniform4f(rasterTextureState1Uniform, 0.0f, 0.0f, 0.0f, 0.0f);
		glUniform4f(rasterFlagsUniform, 0.0f, 0.0f, 0.0f, 0.0f);
		glUseProgram(programs[activeShaderIndex]);

		glGenTextures(1, &rasterTexture);
		glBindTexture(GL_TEXTURE_2D, rasterTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, useEs3 ? GL_RGBA8 : GL_RGBA,
			1024, 1024, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, NULL);
		glGenTextures(1, &rasterSampleTexture);
		glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, useEs3 ? GL_RGBA8 : GL_RGBA,
			1024, 1024, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, NULL);
		glGenFramebuffers(1, &rasterFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, rasterTexture, 0);
		const bool framebufferReady = rasterTexture != 0 &&
			rasterSampleTexture != 0 &&
			rasterFramebuffer != 0 &&
			glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
			glGetError() == GL_NO_ERROR;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (!framebufferReady) {
			DestroyEs3RasterizerResources();
			return false;
		}

		glGenBuffers(1, &rasterVertexBuffer);
		if (rasterVertexBuffer == 0) {
			DestroyEs3RasterizerResources();
			return false;
		}
		glBindBuffer(GL_ARRAY_BUFFER, rasterVertexBuffer);
		glBufferData(GL_ARRAY_BUFFER,
			sizeof(NamcosGlRasterDrawVertex) *
				NAMCOS_GL_RASTER_STREAM_VERTICES, NULL,
			GL_DYNAMIC_DRAW);
		rasterVertexOffset = 0;
		const bool ready = glGetError() == GL_NO_ERROR;
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glUseProgram(programs[activeShaderIndex]);
		glBindTexture(GL_TEXTURE_2D, texture);
		return ready;
	}

	bool BeginEsRasterState()
	{
		if (rasterStateActive) return true;
		glBindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		glViewport(0, 0, 1024, 1024);
		glEnable(GL_SCISSOR_TEST);
		glUseProgram(rasterProgram);
		glBindBuffer(GL_ARRAY_BUFFER, rasterVertexBuffer);
		glEnableVertexAttribArray(rasterPositionAttribute);
		glEnableVertexAttribArray(rasterColorAttribute);
		glEnableVertexAttribArray(rasterTextureAttribute);
		rasterStateActive = glGetError() == GL_NO_ERROR;
		if (!rasterStateActive) EndEsRasterState(true);
		return rasterStateActive;
	}

	void EndEsRasterState(bool force = false)
	{
		if (!rasterStateActive && !force) return;
		glDisable(GL_SCISSOR_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, viewportWidth, viewportHeight);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glUseProgram(programs[activeShaderIndex]);
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterStateActive = false;
	}

	bool SubmitEs3RasterPrimitive(const NamcosGlRasterPacket *packet,
		const NamcosGlRasterPrimitive *primitive)
	{
		if (packet == NULL || primitive == NULL) return false;
		NamcosGlRasterDrawVertex vertices[6];
		const bool fastClear = primitive->type == NAMCOS_GL_RASTER_FILL ||
			primitive->type == NAMCOS_GL_RASTER_FLAT_RECTANGLE;
		const bool textured = primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_POLYGON || primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_RECTANGLE;
		const UINT32 count = fastClear ? 0 : textured ? NamcosGlRasterBuildTexturedTriangles(
			primitive, vertices, 6) : NamcosGlRasterBuildColorTriangles(
			primitive, vertices, 6);
		if (!fastClear && count == 0) return false;
		if (!rasterStateActive && eglGetCurrentContext() != context &&
			eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)
			return false;

		INT32 x1, y1, x2, y2;
		if (!NamcosGlRasterGetDrawBounds(packet, primitive,
			&x1, &y1, &x2, &y2)) return false;

		if (!BeginEsRasterState()) return false;
		glScissor(x1, 1024 - y2 - 1, x2 - x1 + 1, y2 - y1 + 1);
		if (fastClear) {
			const GLfloat red = (GLfloat)((primitive->vertex[0].red * 31 + 127) /
				255) / 31.0f;
			const GLfloat green = (GLfloat)((primitive->vertex[0].green * 31 + 127) /
				255) / 31.0f;
			const GLfloat blue = (GLfloat)((primitive->vertex[0].blue * 31 + 127) /
				255) / 31.0f;
			const GLfloat alpha = primitive->type != NAMCOS_GL_RASTER_FILL &&
				packet->state.drawStp ? 1.0f : 0.0f;
			glClearColor(red, green, blue, alpha);
			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			if ((++rasterValidationCounter & 0xff) == 0 &&
				glGetError() != GL_NO_ERROR) {
				EndEsRasterState();
				return false;
			}
			rasterDirty.Include(x1, y1, x2, y2);
			const UINT64 generation = packet->vramGeneration != NULL ?
				*packet->vramGeneration : 0;
			if (packet->vramRowGeneration != NULL) {
				for (INT32 y = y1; y <= y2; y++) {
					packet->vramRowGeneration[y & 0x3ff] = generation;
				}
				rasterUploadTracker.RememberRange(packet->vramRowGeneration,
					y1, y2 - y1 + 1);
			}
			outputFrameValid = false;
			return true;
		}
		if (textured || primitive->semiTransparent) {
			glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
			UINT32 textureState[8] = { 0 };
			if (textured) NamcosGlRasterGetTextureState(packet, primitive,
				textureState);
			glUniform4f(rasterTextureState0Uniform, (GLfloat)textureState[0],
				(GLfloat)textureState[1], (GLfloat)textureState[2],
				(GLfloat)textureState[3]);
			glUniform4f(rasterTextureState1Uniform, (GLfloat)textureState[4],
				(GLfloat)textureState[5], (GLfloat)textureState[6],
				(GLfloat)textureState[7]);
			const UINT32 abr = (primitive->tpage >>
				(packet->state.gpuType == 2 ? 5 : 7)) & 3;
			glUniform4f(rasterFlagsUniform, textured ? 1.0f : 0.0f,
				primitive->rawTexture ? 1.0f : 0.0f,
				primitive->semiTransparent ? 1.0f : 0.0f, (GLfloat)abr);
		}
		const bool fill = primitive->type == NAMCOS_GL_RASTER_FILL;
		const bool drawStp = !fill && packet->state.drawStp != 0;
		const bool checkStp = !fill && packet->state.checkStp != 0;
		if (!rasterMaskUniformValid || lastRasterDrawStp != drawStp ||
			lastRasterCheckStp != checkStp) {
			glUniform4f(rasterMaskUniform, drawStp ? 1.0f : 0.0f,
				checkStp ? 1.0f : 0.0f, 0.0f, 0.0f);
			lastRasterDrawStp = drawStp;
			lastRasterCheckStp = checkStp;
			rasterMaskUniformValid = true;
		}
		if (rasterVertexOffset + count > NAMCOS_GL_RASTER_STREAM_VERTICES) {
			glBufferData(GL_ARRAY_BUFFER,
				sizeof(NamcosGlRasterDrawVertex) *
					NAMCOS_GL_RASTER_STREAM_VERTICES, NULL,
				GL_DYNAMIC_DRAW);
			rasterVertexOffset = 0;
		}
		const ptrdiff_t vertexOffset = sizeof(NamcosGlRasterDrawVertex) *
			rasterVertexOffset;
		glBufferSubData(GL_ARRAY_BUFFER, vertexOffset,
			sizeof(NamcosGlRasterDrawVertex) * count, vertices);
		glVertexAttribPointer(rasterPositionAttribute, 2, GL_FLOAT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex), (const void *)vertexOffset);
		glVertexAttribPointer(rasterColorAttribute, 3, GL_FLOAT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)(vertexOffset + sizeof(float) * 2));
		glVertexAttribPointer(rasterTextureAttribute, 2, GL_FLOAT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)(vertexOffset + sizeof(float) * 5));
		glDrawArrays(GL_TRIANGLES, 0, count);
		rasterVertexOffset += count;
		if ((++rasterValidationCounter & 0xff) == 0 &&
			glGetError() != GL_NO_ERROR) {
			EndEsRasterState();
			return false;
		}
		rasterDirty.Include(x1, y1, x2, y2);
		const UINT64 generation = packet->vramGeneration != NULL ?
			*packet->vramGeneration : 0;
		if (packet->vramRowGeneration != NULL) {
			for (INT32 y = y1; y <= y2; y++) {
				packet->vramRowGeneration[y & 0x3ff] = generation;
			}
			rasterUploadTracker.RememberRange(packet->vramRowGeneration,
				y1, y2 - y1 + 1);
		}
		outputFrameValid = false;
		return true;
	}

	bool UploadEs3RasterVram(const UINT16 *vram,
		const UINT64 *rowGeneration, NamcosPolyThreadPool *threadPool)
	{
		if (vram == NULL) return false;
		if (eglGetCurrentContext() != context &&
			eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)
			return false;
		if (rasterTransferPixels == NULL) {
			rasterTransferPixels = (UINT8 *)malloc(1024 * 1024 * 4);
			if (rasterTransferPixels == NULL) return false;
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		if (!rasterUploadTracker.valid || rowGeneration == NULL) {
			NamcosGlPackVramParallel(vram, rasterTransferPixels, threadPool);
			glBindTexture(GL_TEXTURE_2D, rasterTexture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA,
				GL_UNSIGNED_BYTE, rasterTransferPixels);
		} else {
			NamcosGlRasterUploadSpan spans[8];
			INT32 first;
			INT32 rows;
			const INT32 spanCount = NamcosGlRasterBuildUploadSpans(
				&rasterUploadTracker, rowGeneration, spans, 8, &first, &rows);
			if (spanCount != 0) glBindTexture(GL_TEXTURE_2D, rasterTexture);
			if (spanCount < 0) {
				NamcosGlRasterPackVramRange(vram, rasterTransferPixels, first, rows);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1024 - first - rows,
					1024, rows, GL_RGBA, GL_UNSIGNED_BYTE, rasterTransferPixels);
			} else {
				for (INT32 span = 0; span < spanCount; span++) {
					first = spans[span].firstRow;
					rows = spans[span].rowCount;
					NamcosGlRasterPackVramRange(vram, rasterTransferPixels,
						first, rows);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0,
						1024 - first - rows, 1024, rows, GL_RGBA,
						GL_UNSIGNED_BYTE, rasterTransferPixels);
				}
			}
		}
		const bool uploaded = glGetError() == GL_NO_ERROR;
		if (uploaded) rasterUploadTracker.RememberAll(rowGeneration);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterVramSynchronized = uploaded;
		if (uploaded) rasterDirty.Reset();
		return uploaded;
	}

	bool ReadbackEs3RasterVram(UINT16 *vram)
	{
		if (vram == NULL || rasterTransferPixels == NULL) return false;
		if (eglGetCurrentContext() != context &&
			eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)
			return false;
		if (!rasterDirty.valid) return true;
		NamcosGlRasterRect rects[8];
		const INT32 rectCount = rasterDirty.GetReadbackRects(rects, 8);
		bool read = rectCount > 0;
		glBindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		for (INT32 i = 0; i < rectCount; i++) {
			const INT32 width = rects[i].x2 - rects[i].x1 + 1;
			const INT32 height = rects[i].y2 - rects[i].y1 + 1;
			glReadPixels(rects[i].x1, 1024 - rects[i].y2 - 1,
				width, height, GL_RGBA, GL_UNSIGNED_BYTE, rasterTransferPixels);
			if (glGetError() != GL_NO_ERROR) {
				read = false;
				break;
			}
			NamcosGlUnpackVramRectParallel(rasterTransferPixels, vram,
				rects[i].x1, rects[i].y1, width, height, rasterThreadPool);
		}
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		EndEsRasterState(true);
		if (read) rasterDirty.Reset();
		return read;
	}
	#endif

	void UploadRgb24(const NamcosFrameConvertContext *frame)
	{
		const INT32 width = frame->sourceWidth;
		const INT32 rows = frame->sourceHeight;
		UINT8 *rgbUploadPixels = (UINT8 *)uploadPixels;
		const size_t rowBytes = (size_t)width * 3;
		const size_t totalBytes = rowBytes * rows;
		INT32 observedFirst;
		INT32 observedRows;
		NamcosFrameGetObservedRows(frame, &observedFirst, &observedRows);
		if (rgbUploadCacheBytes < totalBytes) {
			UINT8 *newCache = (UINT8 *)realloc(rgbUploadCache, totalBytes);
			if (newCache == NULL) {
				rgbCacheValid = false;
				rgbDenseFrames = 0;
				NamcosPackRgb24(frame, rgbUploadPixels);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, rows,
					GL_RGB, GL_UNSIGNED_BYTE, rgbUploadPixels);
				return;
			}
			rgbUploadCache = newCache;
			rgbUploadCacheBytes = totalBytes;
		}

		if (!rgbCacheValid || rgbCacheWidth != width || rgbCacheHeight != rows ||
			rgbCacheDisplayX != frame->displayX || rgbCacheDisplayY != frame->displayY) {
			NamcosPackRgb24(frame, rgbUploadPixels);
			NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
				rows, NULL, rows, frame->threadPool);
			rgbCacheWidth = width;
			rgbCacheHeight = rows;
			rgbCacheDisplayX = frame->displayX;
			rgbCacheDisplayY = frame->displayY;
			for (INT32 y = 0; y < rows; y++) {
				rgbRowGeneration[y] = frame->vramRowGeneration != NULL ?
					frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			}
			rgbCacheValid = true;
			rgbDenseFrames = 0;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, rows,
				GL_RGB, GL_UNSIGNED_BYTE, rgbUploadPixels);
			return;
		}
		if (rgbDenseFrames != 0) {
			UINT8 denseRows[1024];
			memset(denseRows, 0, rows);
			INT32 denseChangedRows = 0;
			if (frame->vramRowGeneration != NULL) {
				for (INT32 y = observedFirst; y < observedFirst + observedRows; y++) {
					const UINT64 generation =
						frame->vramRowGeneration[(frame->displayY + y) & 0x3ff];
					denseRows[y] = rgbRowGeneration[y] != generation ? 1 : 0;
					if (denseRows[y]) denseChangedRows++;
				}
				if (denseChangedRows == 0) {
					rgbDenseFrames = 0;
					return;
				}
				if (denseChangedRows * 3 < observedRows) rgbDenseFrames = 0;
			} else {
				memset(denseRows + observedFirst, 1, observedRows);
				denseChangedRows = observedRows;
			}
			if (rgbDenseFrames != 0) {
				NamcosPackRgb24Selected(frame, rgbUploadPixels, denseRows,
					denseChangedRows);
				NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
					rows, denseRows, denseChangedRows, frame->threadPool);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width,
					observedRows, GL_RGB, GL_UNSIGNED_BYTE,
					rgbUploadCache + (size_t)observedFirst * rowBytes);
				for (INT32 y = observedFirst; y < observedFirst + observedRows; y++) {
					rgbRowGeneration[y] = frame->vramRowGeneration != NULL ?
						frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
				}
				--rgbDenseFrames;
				return;
			}
		}

		UINT8 dirtyRows[1024];
		INT32 changedRows = 0;
		INT32 candidateRows = 0;
		INT32 runCount = 0;
		bool inRun = false;
		for (INT32 y = 0; y < rows; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool observed = y >= observedFirst &&
				y < observedFirst + observedRows;
			const bool candidate = observed && (frame->vramRowGeneration == NULL ||
				rgbRowGeneration[y] != generation);
			dirtyRows[y] = candidate ? 1 : 0;
			if (candidate) candidateRows++;
		}
		NamcosPackRgb24Selected(frame, rgbUploadPixels, dirtyRows, candidateRows);
		for (INT32 y = 0; y < rows; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool candidate = dirtyRows[y] != 0;
			const bool changed = candidate &&
				memcmp(rgbUploadCache + (size_t)y * rowBytes,
					rgbUploadPixels + (size_t)y * rowBytes, rowBytes) != 0;
			if (candidate) rgbRowGeneration[y] = generation;
			dirtyRows[y] = changed ? 1 : 0;
			if (changed) {
				changedRows++;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= observedRows) {
			rgbDenseFrames = 7;
			NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
				rows, dirtyRows, changedRows, frame->threadPool);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width, observedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)observedFirst * rowBytes);
			return;
		}
		if (runCount > 8) {
			NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
				rows, dirtyRows, changedRows, frame->threadPool);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width, observedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)observedFirst * rowBytes);
			return;
		}

		NamcosCopyPackedRows(rgbUploadPixels, rgbUploadCache, rowBytes,
			rows, dirtyRows, changedRows, frame->threadPool);
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, mergedStart, width, mergedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)mergedStart * rowBytes);
			return;
		}
		for (INT32 y = 0; y < rows;) {
			while (y < rows && !dirtyRows[y]) y++;
			const INT32 start = y;
			while (y < rows && dirtyRows[y]) y++;
			if (start < y) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, start, width, y - start,
					GL_RGB, GL_UNSIGNED_BYTE,
					rgbUploadPixels + (size_t)start * rowBytes);
			}
		}
	}

	static bool HasExtension(const char *extensions, const char *extension)
	{
		if (extensions == NULL || extension == NULL || *extension == 0 ||
			strchr(extension, ' ') != NULL) {
			return false;
		}

		const size_t length = strlen(extension);
		const char *position = extensions;
		while ((position = strstr(position, extension)) != NULL) {
			const bool startsToken = position == extensions || position[-1] == ' ';
			const bool endsToken = position[length] == 0 || position[length] == ' ';
			if (startsToken && endsToken) return true;
			position += length;
		}
		return false;
	}

	void PrepareReadTable(GLenum type)
	{
		if (type == GL_UNSIGNED_SHORT_5_5_5_1) {
			for (INT32 i = 0; i < 0x10000; i++) {
				readTable[i] = (UINT16)(((i >> 11) & 0x001f) |
					((i >> 1) & 0x03e0) | ((i << 9) & 0x7c00));
			}
		} else {
			for (INT32 i = 0; i < 0x10000; i++) {
				readTable[i] = (UINT16)(((i >> 11) & 0x001f) |
					((i >> 1) & 0x03e0) | ((i << 10) & 0x7c00));
			}
		}
	}

	void PrepareDirectReadTable(const UINT32 *palette)
	{
		for (INT32 i = 0; i < 0x10000; i++) {
			directReadTable[i] = palette[readTable[i]];
		}
		directPalette = palette;
	}

	bool EnsureInitialized(INT32 width, INT32 height, INT32 requestedBytes)
	{
		const INT32 preferredBytes = requestedBytes == 2 ? 2 :
			(requestedBytes >= 3 ? 4 : 0);
		if (initialized && width <= surfaceWidth && height <= surfaceHeight &&
			(preferredBytes == 0 || preferredBytes == surfacePixelBytes)) {
			return available;
		}
		if (failed) return false;
		if (initialized) Shutdown();

		initialized = true;
		surfaceWidth = width;
		surfaceHeight = height;
		display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL) ||
			!eglBindAPI(EGL_OPENGL_ES_API)) {
			Disable();
			return false;
		}

		const EGLint redSize = preferredBytes == 2 ? 5 :
			(preferredBytes == 4 ? 8 : 5);
		const EGLint greenSize = preferredBytes == 2 ? 6 :
			(preferredBytes == 4 ? 8 : 5);
		const EGLint blueSize = preferredBytes == 2 ? 5 :
			(preferredBytes == 4 ? 8 : 5);
		const EGLint alphaSize = preferredBytes == 2 ? 0 :
			(preferredBytes == 4 ? 8 : 1);
		const EGLint configAttributes[] = {
			EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT_KHR,
#else
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
#endif
			EGL_RED_SIZE, redSize,
			EGL_GREEN_SIZE, greenSize,
			EGL_BLUE_SIZE, blueSize,
			EGL_ALPHA_SIZE, alphaSize,
			EGL_NONE
		};
		EGLConfig config = NULL;
		EGLint configCount = 0;
		if (!eglChooseConfig(display, configAttributes, &config, 1, &configCount) ||
			configCount < 1) {
			const EGLint fallbackAttributes[] = {
				EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
#endif
				EGL_RED_SIZE, 5,
				EGL_GREEN_SIZE, 5,
				EGL_BLUE_SIZE, 5,
				EGL_ALPHA_SIZE, 1,
				EGL_NONE
			};
			if (!eglChooseConfig(display, fallbackAttributes, &config, 1,
				&configCount) || configCount < 1) {
				Disable();
				return false;
			}
		}
		// Track the requested output class, not the implementation-selected
		// channel sizes, so a compatible fallback config is not recreated every frame.
		surfacePixelBytes = preferredBytes;

		const EGLint surfaceAttributes[] = {
			EGL_WIDTH, width,
			EGL_HEIGHT, height,
			EGL_NONE
		};
		surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		const EGLint contextAttributes3[] = {
			EGL_CONTEXT_CLIENT_VERSION, 3,
			EGL_NONE
		};
		context = eglCreateContext(display, config, EGL_NO_CONTEXT,
			contextAttributes3);
		if (context == EGL_NO_CONTEXT) {
			const EGLint contextAttributes2[] = {
				EGL_CONTEXT_CLIENT_VERSION, 2,
				EGL_NONE
			};
			context = eglCreateContext(display, config, EGL_NO_CONTEXT,
				contextAttributes2);
		}
#else
		context = eglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
#endif
		if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
			!eglMakeCurrent(display, surface, surface, context)) {
			Disable();
			return false;
		}

		GLint maximumTextureSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
		const char *renderer = (const char *)glGetString(GL_RENDERER);
		const char *version = (const char *)glGetString(GL_VERSION);
		if (maximumTextureSize < 1024 ||
			!NamcosOpenGLRendererIsHardware(renderer)) {
			Disable();
			return false;
		}
		fullRasterizerCapable = false;

#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		if (!CreateProgram()) {
			Disable();
			return false;
		}
#endif

		const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
		readBgraSupported = HasExtension(extensions, "GL_EXT_read_format_bgra");
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		unpackSubimage = HasExtension(extensions, "GL_EXT_unpack_subimage");
#endif
		if (HasExtension(extensions, "GL_EXT_discard_framebuffer")) {
			discardFramebuffer = (NamcosDiscardFramebufferProc)
				eglGetProcAddress("glDiscardFramebufferEXT");
		}
		const bool canQueryReadFormat =
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			true;
#else
			HasExtension(extensions, "GL_OES_read_format") ||
			HasExtension(extensions, "GL_EXT_read_format_bgra");
#endif
		if (canQueryReadFormat) {
			GLint readFormat = 0;
			GLint readType = 0;
			glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES, &readFormat);
			glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE_OES, &readType);
			const bool read5551 = readFormat == GL_RGBA &&
				readType == GL_UNSIGNED_SHORT_5_5_5_1;
			const bool read565 = readFormat == GL_RGB &&
				readType == GL_UNSIGNED_SHORT_5_6_5;
			const bool readBgra = readFormat == GL_BGRA_EXT &&
				readType == GL_UNSIGNED_BYTE;
			if (glGetError() == GL_NO_ERROR && (read5551 || read565 || readBgra)) {
				nativeReadFormat = (GLenum)readFormat;
				nativeReadType = (GLenum)readType;
				if (readBgra) readBgraSupported = true;
				if (read5551 || read565) PrepareReadTable(nativeReadType);
			}
		}
		for (INT32 errorIndex = 0; errorIndex < 8; errorIndex++) {
			if (glGetError() == GL_NO_ERROR) break;
		}
		UINT16 read565Probe = 0;
		glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
			&read565Probe);
		readRgb565Supported = glGetError() == GL_NO_ERROR;
		for (INT32 errorIndex = 0; errorIndex < 8; errorIndex++) {
			if (glGetError() == GL_NO_ERROR) break;
		}
		UINT8 readRgbProbe[3] = { 0, 0, 0 };
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, readRgbProbe);
		readRgb888Supported = glGetError() == GL_NO_ERROR;

		// The same staging allocation holds packed RGB888 MDEC rows when needed.
		uploadPixels = (UINT16 *)malloc(1024 * 1024 * 3);
		if (uploadPixels == NULL) {
			Disable();
			return false;
		}

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1024, 1024, 0,
			GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, NULL);
		glGenTextures(1, &rgbTexture);
		glBindTexture(GL_TEXTURE_2D, rgbTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB,
			GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, texture);
#else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA,
			GL_UNSIGNED_SHORT_5_5_5_1, NULL);
#endif
		if (texture == 0
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			|| rgbTexture == 0
#endif
			|| glGetError() != GL_NO_ERROR) {
			Disable();
			return false;
		}
		#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		const bool useEs3Rasterizer = version != NULL &&
			(NamcosStringContainsNoCase(version, "OpenGL ES 3.") ||
			 NamcosStringContainsNoCase(version, "OpenGL ES 4."));
		fullRasterizerCapable = CreateEs3RasterizerResources(useEs3Rasterizer);
		#endif

		glViewport(0, 0, width, height);
		viewportWidth = width;
		viewportHeight = height;
		#if !defined(FBNEO_NAMCOS_OPENGL_ES2)
		glDisable(GL_ALPHA_TEST);
		#endif
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_DITHER);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		#if !defined(FBNEO_NAMCOS_OPENGL_ES2)
		glEnable(GL_TEXTURE_2D);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		#endif
		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		#if !defined(FBNEO_NAMCOS_OPENGL_ES2)
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		#endif
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		available = true;
		return true;
	}

	void UploadRows(const UINT16 *source, INT32 destinationX, INT32 destinationY,
		INT32 width, INT32 rows, NamcosPolyThreadPool *threadPool)
	{
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		if (width == 1024 || unpackSubimage) {
			if (width != 1024) glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 1024);
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY, width, rows,
				GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, source);
			if (width != 1024) glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
			return;
		}
		NamcosCopy16Rows(source, uploadPixels, width, rows, threadPool);
		glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY, width, rows,
			GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, uploadPixels);
#else
		for (INT32 y = 0; y < rows; y++) {
			const UINT16 *sourceRow = source + y * 1024;
			UINT16 *destinationRow = uploadPixels + y * width;
			INT32 x = 0;
			for (; x + 3 < width; x += 4) {
				destinationRow[x + 0] = uploadTable[sourceRow[x + 0]];
				destinationRow[x + 1] = uploadTable[sourceRow[x + 1]];
				destinationRow[x + 2] = uploadTable[sourceRow[x + 2]];
				destinationRow[x + 3] = uploadTable[sourceRow[x + 3]];
			}
			for (; x < width; x++) {
				destinationRow[x] = uploadTable[sourceRow[x]];
			}
		}
		glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY, width, rows,
			GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, uploadPixels);
#endif
	}

#if defined(FBNEO_NAMCOS_OPENGL_ES2)
	void UploadTightRows(const UINT16 *source, INT32 destinationX,
		INT32 destinationY, INT32 width, INT32 rows)
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY, width, rows,
			GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, source);
	}
#endif

	void UploadRect(const UINT16 *source, INT32 destinationX, INT32 destinationY,
		INT32 width, INT32 rows, const UINT64 *rowGeneration,
		NamcosPolyThreadPool *threadPool)
	{
		UploadSlot *slot = uploadSlotIndex < 4 ?
			&uploadSlots[uploadSlotIndex++] : NULL;
		const size_t pixelCount = (size_t)width * rows;
		const bool sameGeometry = slot != NULL && slot->valid &&
			slot->destinationX == destinationX &&
			slot->destinationY == destinationY &&
			slot->width == width && slot->rows == rows;

		if (slot != NULL && slot->pixelCapacity < pixelCount) {
			UINT16 *newPixels = (UINT16 *)realloc(slot->pixels,
				pixelCount * sizeof(UINT16));
			if (newPixels == NULL) {
				slot->valid = false;
				slot = NULL;
			} else {
				slot->pixels = newPixels;
				slot->pixelCapacity = pixelCount;
			}
		}
		if (slot == NULL || !sameGeometry) {
			if (slot != NULL) {
				#if defined(FBNEO_NAMCOS_OPENGL_ES2)
				NamcosCopy16Rows(source, slot->pixels, width, rows, threadPool);
				#endif
				for (INT32 y = 0; y < rows; y++) {
					#if !defined(FBNEO_NAMCOS_OPENGL_ES2)
					memcpy(slot->pixels + (size_t)y * width,
						source + (size_t)y * 1024,
						(size_t)width * sizeof(UINT16));
					#endif
					slot->rowGeneration[y] = rowGeneration != NULL ?
						rowGeneration[(destinationY + y) & 0x3ff] : 0;
				}
				slot->destinationX = destinationX;
				slot->destinationY = destinationY;
				slot->width = width;
				slot->rows = rows;
					slot->denseFrames = 0;
					slot->valid = true;
				}
			#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			if (slot != NULL) {
				UploadTightRows(slot->pixels, destinationX, destinationY, width, rows);
				return;
			}
			#endif
			UploadRows(source, destinationX, destinationY, width, rows, threadPool);
			return;
		}
		if (slot->denseFrames != 0) {
			UINT8 denseRows[1024];
			INT32 denseChangedRows = rows;
			if (rowGeneration != NULL) {
				denseChangedRows = 0;
				for (INT32 y = 0; y < rows; y++) {
					const UINT64 generation =
						rowGeneration[(destinationY + y) & 0x3ff];
					denseRows[y] = slot->rowGeneration[y] != generation ? 1 : 0;
					if (denseRows[y]) denseChangedRows++;
				}
				if (denseChangedRows == 0) {
					slot->denseFrames = 0;
					return;
				}
				if (denseChangedRows * 3 < rows) slot->denseFrames = 0;
			}
			if (slot->denseFrames != 0) {
				UploadRows(source, destinationX, destinationY, width, rows, threadPool);
				if (rowGeneration != NULL) {
					#if defined(FBNEO_NAMCOS_OPENGL_ES2)
					NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
						denseRows, denseChangedRows, threadPool);
					#else
					for (INT32 y = 0; y < rows; y++) {
						if (!denseRows[y]) continue;
						memcpy(slot->pixels + (size_t)y * width,
							source + (size_t)y * 1024,
							(size_t)width * sizeof(UINT16));
					}
					#endif
					for (INT32 y = 0; y < rows; y++) {
						slot->rowGeneration[y] =
							rowGeneration[(destinationY + y) & 0x3ff];
					}
				}
				if (--slot->denseFrames == 0 && rowGeneration == NULL) {
					#if defined(FBNEO_NAMCOS_OPENGL_ES2)
					NamcosCopy16Rows(source, slot->pixels, width, rows, threadPool);
					#else
					for (INT32 y = 0; y < rows; y++) {
						memcpy(slot->pixels + (size_t)y * width,
							source + (size_t)y * 1024,
							(size_t)width * sizeof(UINT16));
					}
					#endif
				}
				return;
			}
		}

		UINT8 dirtyRows[1024];
		INT32 changedRows = 0;
		INT32 runCount = 0;
		bool inRun = false;
		for (INT32 y = 0; y < rows; y++) {
			const UINT64 generation = rowGeneration != NULL ?
				rowGeneration[(destinationY + y) & 0x3ff] : 0;
			const bool changed = (rowGeneration == NULL ||
				slot->rowGeneration[y] != generation) &&
				memcmp(slot->pixels + (size_t)y * width,
					source + (size_t)y * 1024,
					(size_t)width * sizeof(UINT16)) != 0;
			slot->rowGeneration[y] = generation;
			dirtyRows[y] = changed ? 1 : 0;
			if (changed) {
				changedRows++;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= rows) {
			slot->denseFrames = 7;
			#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
				dirtyRows, changedRows, threadPool);
			#else
			for (INT32 y = 0; y < rows; y++) {
				if (!dirtyRows[y]) continue;
				memcpy(slot->pixels + (size_t)y * width,
					source + (size_t)y * 1024,
					(size_t)width * sizeof(UINT16));
			}
			#endif
			UploadRows(source, destinationX, destinationY, width, rows, threadPool);
			return;
		}
		if (runCount > 8) {
			#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			NamcosCopy16Rows(source, slot->pixels, width, rows, threadPool);
			#endif
			for (INT32 y = 0; y < rows; y++) {
				#if !defined(FBNEO_NAMCOS_OPENGL_ES2)
				memcpy(slot->pixels + (size_t)y * width,
					source + (size_t)y * 1024,
					(size_t)width * sizeof(UINT16));
				#endif
				slot->rowGeneration[y] = rowGeneration != NULL ?
					rowGeneration[(destinationY + y) & 0x3ff] : 0;
			}
			#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			UploadTightRows(slot->pixels, destinationX, destinationY, width, rows);
			#else
			UploadRows(source, destinationX, destinationY, width, rows, threadPool);
			#endif
			return;
		}

		#if defined(FBNEO_NAMCOS_OPENGL_ES2)
		NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
			dirtyRows, changedRows, threadPool);
		#else
		for (INT32 y = 0; y < rows; y++) {
			if (!dirtyRows[y]) continue;
			memcpy(slot->pixels + (size_t)y * width,
				source + (size_t)y * 1024,
				(size_t)width * sizeof(UINT16));
		}
		#endif
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			#if defined(FBNEO_NAMCOS_OPENGL_ES2)
			UploadTightRows(slot->pixels + (size_t)mergedStart * width, destinationX,
				destinationY + mergedStart, width, mergedRows);
			#else
			UploadRows(source + mergedStart * 1024, destinationX,
				destinationY + mergedStart, width, mergedRows, threadPool);
			#endif
			return;
		}
		for (INT32 y = 0; y < rows;) {
			while (y < rows && !dirtyRows[y]) y++;
			const INT32 start = y;
			while (y < rows && dirtyRows[y]) y++;
			if (start < y) {
				#if defined(FBNEO_NAMCOS_OPENGL_ES2)
				UploadTightRows(slot->pixels + (size_t)start * width, destinationX,
					destinationY + start, width, y - start);
				#else
				UploadRows(source + start * 1024, destinationX,
					destinationY + start, width, y - start, threadPool);
				#endif
			}
		}
	}

	bool PrepareReadPixels(size_t bytes)
	{
		if (readPixelBytes >= bytes) return true;
		UINT8 *newPixels = (UINT8 *)realloc(readPixels, bytes);
		if (newPixels == NULL) return false;
		readPixels = newPixels;
		readPixelBytes = bytes;
		return true;
	}

	void Disable()
	{
		failed = true;
		Shutdown();
	}

	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	GLuint texture;
#if defined(FBNEO_NAMCOS_OPENGL_ES2)
	GLuint rgbTexture;
	GLuint programs[2];
	GLuint vertexBuffer;
	GLuint rasterTexture;
	GLuint rasterSampleTexture;
	GLuint rasterFramebuffer;
	GLuint rasterProgram;
	GLuint rasterVertexBuffer;
	UINT32 rasterVertexOffset;
	UINT32 rasterValidationCounter;
	GLint rasterPositionAttribute;
	GLint rasterColorAttribute;
	GLint rasterTextureAttribute;
	GLint rasterSampleUniform;
	GLint rasterTextureState0Uniform;
	GLint rasterTextureState1Uniform;
	GLint rasterFlagsUniform;
	GLint rasterMaskUniform;
	bool rasterMaskUniformValid;
	bool lastRasterDrawStp;
	bool lastRasterCheckStp;
	GLint positionRectUniforms[2];
	GLint textureRectUniforms[2];
	GLint verticalUniforms[2];
	GLint verticalReconstructUniforms[2];
	GLint swapRedBlueUniforms[2];
	bool drawUniformsValid[2];
	bool textureUniformsValid[2];
	bool lastSwapRedBlue[2];
	INT32 activeShaderIndex;
	GLfloat lastOutputLeft;
	GLfloat lastOutputRight;
	GLfloat lastTextureU0[2];
	GLfloat lastTextureV0[2];
	GLfloat lastTextureU1[2];
	GLfloat lastTextureV1[2];
	bool lastVertical;
#endif
	UINT16 *uploadPixels;
	UINT8 *rasterTransferPixels;
	UINT8 *rgbUploadCache;
	size_t rgbUploadCacheBytes;
	INT32 rgbCacheWidth;
	INT32 rgbCacheHeight;
	INT32 rgbCacheDisplayX;
	INT32 rgbCacheDisplayY;
	UINT64 rgbRowGeneration[1024];
	bool rgbCacheValid;
	UINT8 rgbDenseFrames;
	UINT8 *readPixels;
	size_t readPixelBytes;
	INT32 surfaceWidth;
	INT32 surfaceHeight;
	INT32 viewportWidth;
	INT32 viewportHeight;
	INT32 surfacePixelBytes;
	GLenum nativeReadFormat;
	GLenum nativeReadType;
	bool readRgb565Supported;
	bool readRgb888Supported;
	bool readBgraSupported;
	bool unpackSubimage;
	NamcosDiscardFramebufferProc discardFramebuffer;
	bool discardFramebufferVerified;
	const UINT32 *directPalette;
	UploadSlot uploadSlots[4];
	INT32 uploadSlotIndex;
	bool uploadModeValid;
	bool lastUploadRgb24;
	NamcosFrameUploadKey uploadFrameKey;
	bool uploadFrameValid;
	NamcosFrameOutputKey outputFrameKey;
	UINT64 outputRowGeneration[1024];
	bool outputFrameValid;
	bool fullRasterizerCapable;
	bool rasterVramSynchronized;
	NamcosGlRasterUploadTracker rasterUploadTracker;
	NamcosPolyThreadPool *rasterThreadPool;
	NamcosGlRasterDirtyBounds rasterDirty;
	bool rasterStateActive;
	bool initialized;
	bool available;
	bool failed;
	UINT16 uploadTable[0x10000];
	UINT16 readTable[0x10000];
	UINT32 directReadTable[0x10000];
};

#else

class NamcosOpenGLFrameConverter
{
public:
	bool Probe(INT32, INT32)
	{
		return false;
	}

	bool SupportsFullRasterizer() const
	{
		return false;
	}

	bool SupportsRasterizerApi() const
	{
		return false;
	}

	INT32 RasterizerFailureReason() const
	{
		return -1;
	}

	bool SupportsOpenGL2() const { return false; }
	bool SupportsShaderMode() const { return false; }

	bool RasterizePacket(const NamcosGlRasterPacket *)
	{
		return false;
	}

	bool SynchronizeVram(UINT16 *, UINT64, UINT64 *)
	{
		return true;
	}

	bool Convert(const NamcosFrameConvertContext *)
	{
		return false;
	}

	bool ConvertDirect(const NamcosFrameConvertContext *, UINT8 *, INT32,
		const UINT32 *)
	{
		return false;
	}

	bool ConvertDirect16(const NamcosFrameConvertContext *, UINT8 *, INT32,
		const UINT32 *)
	{
		return false;
	}

	bool ConvertDirect24(const NamcosFrameConvertContext *, UINT8 *, INT32,
		const UINT32 *)
	{
		return false;
	}

	void InvalidatePalette()
	{
	}

	void InvalidateVram()
	{
	}

	void Shutdown()
	{
	}
};

#endif

#endif
