#ifndef FBNEO_NAMCOS_GL_FRAME_H
#define FBNEO_NAMCOS_GL_FRAME_H

#include "namcos_gl_raster.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "namcos_poly_threads.h"

#if !defined(_WIN32) && (defined(__ARM_NEON) || defined(__aarch64__))
#include <arm_neon.h>
#define NAMCOS_GL_ARM_NEON 1
#endif

#if !defined(_WIN32) && defined(__SSE2__)
#include <emmintrin.h>
#define NAMCOS_GL_X86_SSE2 1
#endif

static const UINT32 NAMCOS_GL_RASTER_BATCH_VERTICES = 6 * 4096;
static const UINT32 NAMCOS_GL_RASTER_STREAM_VERTICES = 6 * 32768;
static const INT32 NAMCOS_GL_RASTER_FAST_CLEAR_PIXELS = 64 * 64;

static void NamcosGlCopyWrappedVramToLinear(const UINT16 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height)
{
	for (INT32 yy = 0; yy < height; yy++) {
		const UINT16 *sourceRow = source +
			((size_t)((y + yy) & 0x3ff) << 10);
		UINT16 *destinationRow = destination + (size_t)yy * width;
		INT32 sourceX = x & 0x3ff;
		INT32 remaining = width;

		while (remaining > 0) {
			const INT32 run = (remaining < 1024 - sourceX) ?
				remaining : 1024 - sourceX;
			memcpy(destinationRow, sourceRow + sourceX,
				(size_t)run * sizeof(UINT16));
			destinationRow += run;
			remaining -= run;
			sourceX = 0;
		}
	}
}

static void NamcosGlCopyLinearToWrappedVram(const UINT16 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height)
{
	for (INT32 yy = 0; yy < height; yy++) {
		const UINT16 *sourceRow = source + (size_t)yy * width;
		UINT16 *destinationRow = destination +
			((size_t)((y + yy) & 0x3ff) << 10);
		INT32 destinationX = x & 0x3ff;
		INT32 remaining = width;

		while (remaining > 0) {
			const INT32 run = (remaining < 1024 - destinationX) ?
				remaining : 1024 - destinationX;
			memcpy(destinationRow + destinationX, sourceRow,
				(size_t)run * sizeof(UINT16));
			sourceRow += run;
			remaining -= run;
			destinationX = 0;
		}
	}
}

#if !defined(_WIN32)

static void NamcosGlUnpackVramRow(const UINT8 *source,
	UINT16 *destination, INT32 width)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	const uint16x8_t round = vdupq_n_u16(1014);
	const uint16x8_t alphaMask = vdupq_n_u16(0x8000);
	for (; x + 8 <= width; x += 8) {
		const uint8x8x4_t rgba = vld4_u8(source + (size_t)x * 4);
		const uint16x8_t red = vshrq_n_u16(vmlaq_n_u16(round,
			vmovl_u8(rgba.val[0]), 249), 11);
		const uint16x8_t green = vshrq_n_u16(vmlaq_n_u16(round,
			vmovl_u8(rgba.val[1]), 249), 11);
		const uint16x8_t blue = vshrq_n_u16(vmlaq_n_u16(round,
			vmovl_u8(rgba.val[2]), 249), 11);
		const uint16x8_t alpha = vandq_u16(vmovl_u8(vcge_u8(
			rgba.val[3], vdup_n_u8(0x80))), alphaMask);
		uint16x8_t output = vorrq_u16(red, vshlq_n_u16(green, 5));
		output = vorrq_u16(output, vshlq_n_u16(blue, 10));
		output = vorrq_u16(output, alpha);
		vst1q_u16(destination + x, output);
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i byteMask = _mm_set1_epi32(0xff);
	const __m128i scale = _mm_set1_epi16(249);
	const __m128i round = _mm_set1_epi16(1014);
	const __m128i alphaThreshold = _mm_set1_epi16(127);
	const __m128i alphaMask = _mm_set1_epi32(0x8000);
	const __m128i packBias = _mm_set1_epi32(0x8000);
	const __m128i packXor = _mm_set1_epi16((short)0x8000);
	const __m128i zero = _mm_setzero_si128();
	for (; x + 4 <= width; x += 4) {
		const __m128i rgba = _mm_loadu_si128(
			(const __m128i *)(source + (size_t)x * 4));
		const __m128i redBytes = _mm_and_si128(rgba, byteMask);
		const __m128i greenBytes = _mm_and_si128(
			_mm_srli_epi32(rgba, 8), byteMask);
		const __m128i blueBytes = _mm_and_si128(
			_mm_srli_epi32(rgba, 16), byteMask);
		const __m128i alphaBytes = _mm_and_si128(
			_mm_srli_epi32(rgba, 24), byteMask);
		const __m128i red = _mm_srli_epi16(_mm_add_epi16(
			_mm_mullo_epi16(redBytes, scale), round), 11);
		const __m128i green = _mm_slli_epi32(_mm_srli_epi16(_mm_add_epi16(
			_mm_mullo_epi16(greenBytes, scale), round), 11), 5);
		const __m128i blue = _mm_slli_epi32(_mm_srli_epi16(_mm_add_epi16(
			_mm_mullo_epi16(blueBytes, scale), round), 11), 10);
		const __m128i alpha = _mm_and_si128(
			_mm_cmpgt_epi16(alphaBytes, alphaThreshold), alphaMask);
		const __m128i color = _mm_or_si128(
			_mm_or_si128(red, green), _mm_or_si128(blue, alpha));
		const __m128i packed = _mm_xor_si128(_mm_packs_epi32(
			_mm_sub_epi32(color, packBias), zero), packXor);
		_mm_storel_epi64((__m128i *)(destination + x), packed);
	}
#endif
	for (; x < width; x++) {
		destination[x] = NamcosGlRasterUnpackVramPixel(source + x * 4);
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
		NamcosGlUnpackVramRow(row, output, context->width);
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

static inline UINT16 NamcosGlVramTo5551(UINT16 pixel)
{
	return (UINT16)(((pixel & 0x001f) << 11) |
		((pixel & 0x03e0) << 1) | ((pixel & 0x7c00) >> 9) |
		((pixel & 0x8000) >> 15));
}

static inline UINT16 NamcosGl5551ToVram(UINT16 pixel)
{
	return (UINT16)(((pixel >> 11) & 0x001f) |
		((pixel >> 1) & 0x03e0) | ((pixel << 9) & 0x7c00) |
		((pixel & 0x0001) << 15));
}

static void NamcosGlVramTo5551Row(const UINT16 *source,
	UINT16 *destination, INT32 width)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	const uint16x8_t redMask = vdupq_n_u16(0x001f);
	const uint16x8_t greenMask = vdupq_n_u16(0x03e0);
	const uint16x8_t blueMask = vdupq_n_u16(0x7c00);
	const uint16x8_t alphaMask = vdupq_n_u16(0x8000);
	for (; x + 8 <= width; x += 8) {
		const uint16x8_t pixel = vld1q_u16(source + x);
		uint16x8_t output = vshlq_n_u16(vandq_u16(pixel, redMask), 11);
		output = vorrq_u16(output,
			vshlq_n_u16(vandq_u16(pixel, greenMask), 1));
		output = vorrq_u16(output,
			vshrq_n_u16(vandq_u16(pixel, blueMask), 9));
		output = vorrq_u16(output,
			vshrq_n_u16(vandq_u16(pixel, alphaMask), 15));
		vst1q_u16(destination + x, output);
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i redMask = _mm_set1_epi16(0x001f);
	const __m128i greenMask = _mm_set1_epi16(0x03e0);
	const __m128i blueMask = _mm_set1_epi16(0x7c00);
	const __m128i alphaMask = _mm_set1_epi16((short)0x8000);
	for (; x + 8 <= width; x += 8) {
		const __m128i pixel = _mm_loadu_si128((const __m128i *)(source + x));
		__m128i output = _mm_slli_epi16(_mm_and_si128(pixel, redMask), 11);
		output = _mm_or_si128(output,
			_mm_slli_epi16(_mm_and_si128(pixel, greenMask), 1));
		output = _mm_or_si128(output,
			_mm_srli_epi16(_mm_and_si128(pixel, blueMask), 9));
		output = _mm_or_si128(output,
			_mm_srli_epi16(_mm_and_si128(pixel, alphaMask), 15));
		_mm_storeu_si128((__m128i *)(destination + x), output);
	}
#endif
	for (; x < width; x++) destination[x] = NamcosGlVramTo5551(source[x]);
}

static void NamcosGl5551ToVramRow(const UINT16 *source,
	UINT16 *destination, INT32 width)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	const uint16x8_t redMask = vdupq_n_u16(0xf800);
	const uint16x8_t greenMask = vdupq_n_u16(0x07c0);
	const uint16x8_t blueMask = vdupq_n_u16(0x003e);
	const uint16x8_t alphaMask = vdupq_n_u16(0x0001);
	for (; x + 8 <= width; x += 8) {
		const uint16x8_t pixel = vld1q_u16(source + x);
		uint16x8_t output = vshrq_n_u16(vandq_u16(pixel, redMask), 11);
		output = vorrq_u16(output,
			vshrq_n_u16(vandq_u16(pixel, greenMask), 1));
		output = vorrq_u16(output,
			vshlq_n_u16(vandq_u16(pixel, blueMask), 9));
		output = vorrq_u16(output,
			vshlq_n_u16(vandq_u16(pixel, alphaMask), 15));
		vst1q_u16(destination + x, output);
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i redMask = _mm_set1_epi16((short)0xf800);
	const __m128i greenMask = _mm_set1_epi16(0x07c0);
	const __m128i blueMask = _mm_set1_epi16(0x003e);
	const __m128i alphaMask = _mm_set1_epi16(0x0001);
	for (; x + 8 <= width; x += 8) {
		const __m128i pixel = _mm_loadu_si128((const __m128i *)(source + x));
		__m128i output = _mm_srli_epi16(_mm_and_si128(pixel, redMask), 11);
		output = _mm_or_si128(output,
			_mm_srli_epi16(_mm_and_si128(pixel, greenMask), 1));
		output = _mm_or_si128(output,
			_mm_slli_epi16(_mm_and_si128(pixel, blueMask), 9));
		output = _mm_or_si128(output,
			_mm_slli_epi16(_mm_and_si128(pixel, alphaMask), 15));
		_mm_storeu_si128((__m128i *)(destination + x), output);
	}
#endif
	for (; x < width; x++) destination[x] = NamcosGl5551ToVram(source[x]);
}

struct NamcosGlPackVram5551Context
{
	const UINT16 *source;
	UINT16 *destination;
	INT32 first;
	INT32 rows;
};

static void NamcosGlPackVram5551Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlPackVram5551Context *context =
		(NamcosGlPackVram5551Context *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const UINT16 *source = context->source +
			(size_t)(context->first + context->rows - 1 - row) * 1024;
		UINT16 *destination = context->destination + (size_t)row * 1024;
		NamcosGlVramTo5551Row(source, destination, 1024);
	}
}

static void NamcosGlPackVram5551RangeParallel(const UINT16 *source,
	UINT16 *destination, INT32 first, INT32 rows,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlPackVram5551Context context;
	context.source = source;
	context.destination = destination;
	context.first = first;
	context.rows = rows;
	const INT64 work = (INT64)rows * 1024;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(rows, work, 131072,
			NamcosGlPackVram5551Rows, &context);
	} else {
		NamcosGlPackVram5551Rows(&context, 0, rows);
	}
}

struct NamcosGlPackVram5551RectContext
{
	const UINT16 *source;
	UINT16 *destination;
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static void NamcosGlPackVram5551RectRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlPackVram5551RectContext *context =
		(NamcosGlPackVram5551RectContext *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const UINT16 *input = context->source +
			(size_t)(context->y + context->height - 1 - row) * 1024 +
			context->x;
		UINT16 *output = context->destination +
			(size_t)row * context->width;
		NamcosGlVramTo5551Row(input, output, context->width);
	}
}

static void NamcosGlPackVram5551RectParallel(const UINT16 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlPackVram5551RectContext context;
	context.source = source;
	context.destination = destination;
	context.x = x;
	context.y = y;
	context.width = width;
	context.height = height;
	const INT64 work = (INT64)width * height;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(height, work, 32768,
			NamcosGlPackVram5551RectRows, &context);
	} else {
		NamcosGlPackVram5551RectRows(&context, 0, height);
	}
}

struct NamcosGlReadVram5551Context
{
	const UINT16 *source;
	UINT16 *destination;
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static void NamcosGlReadVram5551Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlReadVram5551Context *context =
		(NamcosGlReadVram5551Context *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const UINT16 *source = context->source +
			(size_t)(context->height - 1 - row) * context->width;
		UINT16 *destination = context->destination +
			(size_t)(context->y + row) * 1024 + context->x;
		NamcosGl5551ToVramRow(source, destination, context->width);
	}
}

static void NamcosGlReadVram5551RectParallel(const UINT16 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlReadVram5551Context context;
	context.source = source;
	context.destination = destination;
	context.x = x;
	context.y = y;
	context.width = width;
	context.height = height;
	const INT64 work = (INT64)width * height;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(height, work, 32768,
			NamcosGlReadVram5551Rows, &context);
	} else {
		NamcosGlReadVram5551Rows(&context, 0, height);
	}
}

#endif

#if defined(_WIN32)

struct NamcosGlWinUnpackVramContext
{
	const UINT8 *source;
	UINT16 *destination;
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static void NamcosGlWinUnpackVramRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlWinUnpackVramContext *context =
		(NamcosGlWinUnpackVramContext *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const UINT8 *source = context->source +
			(size_t)(context->height - 1 - row) * context->width * 4;
		UINT16 *destination = context->destination +
			(size_t)(context->y + row) * 1024 + context->x;
		for (INT32 x = 0; x < context->width; x++) {
			destination[x] = NamcosGlRasterUnpackVramPixel(source + x * 4);
		}
	}
}

static void NamcosGlUnpackVramRectParallel(const UINT8 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlWinUnpackVramContext context;
	context.source = source;
	context.destination = destination;
	context.x = x;
	context.y = y;
	context.width = width;
	context.height = height;
	const INT64 work = (INT64)width * height;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(height, work, 32768,
			NamcosGlWinUnpackVramRows, &context);
	} else {
		NamcosGlWinUnpackVramRows(&context, 0, height);
	}
}

struct NamcosGlCopyVram16Context
{
	const UINT16 *source;
	UINT16 *destination;
	INT32 first;
	INT32 rows;
};

static void NamcosGlCopyVram16Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlCopyVram16Context *context =
		(NamcosGlCopyVram16Context *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const UINT16 *source = context->source +
			(size_t)(context->first + context->rows - 1 - row) * 1024;
		UINT16 *output = context->destination + (size_t)row * 1024;
		memcpy(output, source, 1024 * sizeof(UINT16));
	}
}

static void NamcosGlCopyVram16RangeParallel(const UINT16 *source,
	UINT16 *destination, INT32 first, INT32 rows,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlCopyVram16Context context;
	context.source = source;
	context.destination = destination;
	context.first = first;
	context.rows = rows;
	const INT64 work = (INT64)rows * 1024;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(rows, work, 131072,
			NamcosGlCopyVram16Rows, &context);
	} else {
		NamcosGlCopyVram16Rows(&context, 0, rows);
	}
}

struct NamcosGlCopyVram16RectContext
{
	const UINT16 *source;
	UINT16 *destination;
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static void NamcosGlCopyVram16RectRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlCopyVram16RectContext *context =
		(NamcosGlCopyVram16RectContext *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const UINT16 *source = context->source +
			(size_t)(context->y + context->height - 1 - row) * 1024 +
			context->x;
		UINT16 *output = context->destination + (size_t)row * context->width;
		memcpy(output, source, (size_t)context->width * sizeof(UINT16));
	}
}

static void NamcosGlCopyVram16RectParallel(const UINT16 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlCopyVram16RectContext context;
	context.source = source;
	context.destination = destination;
	context.x = x;
	context.y = y;
	context.width = width;
	context.height = height;
	const INT64 work = (INT64)width * height;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(height, work, 131072,
			NamcosGlCopyVram16RectRows, &context);
	} else {
		NamcosGlCopyVram16RectRows(&context, 0, height);
	}
}

struct NamcosGlReadVram16Context
{
	const UINT16 *source;
	UINT16 *destination;
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static void NamcosGlReadVram16Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlReadVram16Context *context =
		(NamcosGlReadVram16Context *)opaque;
	for (INT32 row = begin; row < end; row++) {
		UINT16 *destination = context->destination +
			(size_t)(context->y + row) * 1024 + context->x;
		const UINT16 *source = context->source +
			(size_t)(context->height - 1 - row) * context->width;
		memcpy(destination, source, (size_t)context->width * sizeof(UINT16));
	}
}

static void NamcosGlReadVram16RectParallel(const UINT16 *source,
	UINT16 *destination, INT32 x, INT32 y, INT32 width, INT32 height,
	NamcosPolyThreadPool *threadPool)
{
	NamcosGlReadVram16Context context;
	context.source = source;
	context.destination = destination;
	context.x = x;
	context.y = y;
	context.width = width;
	context.height = height;
	const INT64 work = (INT64)width * height;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(height, work, 32768,
			NamcosGlReadVram16Rows, &context);
	} else {
		NamcosGlReadVram16Rows(&context, 0, height);
	}
}

#endif

#if defined(FBNEO_NAMCOS_OPENGL_ES2)

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
	INT32 native5551;
	INT32 directRgb565;
	INT32 directXrgb8888;
};

static void NamcosGlConvertNative16XrgbRow(const UINT16 *source,
	UINT32 *destination, INT32 width, INT32 native5551)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	const uint16x8_t mask5 = vdupq_n_u16(0x001f);
	for (; x + 8 <= width; x += 8) {
		const uint16x8_t pixel = vld1q_u16(source + x);
		const uint16x8_t red5 = vandq_u16(vshrq_n_u16(pixel, 11), mask5);
		const uint16x8_t green5 = vandq_u16(vshrq_n_u16(pixel, 6), mask5);
		const uint16x8_t blue5 = vandq_u16(native5551 ?
			vshrq_n_u16(pixel, 1) : pixel, mask5);
		const uint16x8_t red8 = vorrq_u16(vshlq_n_u16(red5, 3),
			vshrq_n_u16(red5, 2));
		const uint16x8_t green8 = vorrq_u16(vshlq_n_u16(green5, 3),
			vshrq_n_u16(green5, 2));
		const uint16x8_t blue8 = vorrq_u16(vshlq_n_u16(blue5, 3),
			vshrq_n_u16(blue5, 2));
		for (INT32 half = 0; half < 2; half++) {
			const uint32x4_t red = vmovl_u16(half == 0 ?
				vget_low_u16(red8) : vget_high_u16(red8));
			const uint32x4_t green = vmovl_u16(half == 0 ?
				vget_low_u16(green8) : vget_high_u16(green8));
			const uint32x4_t blue = vmovl_u16(half == 0 ?
				vget_low_u16(blue8) : vget_high_u16(blue8));
			vst1q_u32(destination + x + half * 4,
				vorrq_u32(vorrq_u32(vshlq_n_u32(red, 16),
				vshlq_n_u32(green, 8)), blue));
		}
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i mask5 = _mm_set1_epi16(0x001f);
	const __m128i zero = _mm_setzero_si128();
	for (; x + 8 <= width; x += 8) {
		const __m128i pixel = _mm_loadu_si128((const __m128i *)(source + x));
		const __m128i red5 = _mm_and_si128(_mm_srli_epi16(pixel, 11), mask5);
		const __m128i green5 = _mm_and_si128(_mm_srli_epi16(pixel, 6), mask5);
		const __m128i blue5 = _mm_and_si128(native5551 ?
			_mm_srli_epi16(pixel, 1) : pixel, mask5);
		__m128i red8 = _mm_or_si128(_mm_slli_epi16(red5, 3),
			_mm_srli_epi16(red5, 2));
		__m128i green8 = _mm_or_si128(_mm_slli_epi16(green5, 3),
			_mm_srli_epi16(green5, 2));
		__m128i blue8 = _mm_or_si128(_mm_slli_epi16(blue5, 3),
			_mm_srli_epi16(blue5, 2));
		for (INT32 half = 0; half < 2; half++) {
			const __m128i red = _mm_unpacklo_epi16(red8, zero);
			const __m128i green = _mm_unpacklo_epi16(green8, zero);
			const __m128i blue = _mm_unpacklo_epi16(blue8, zero);
			const __m128i color = _mm_or_si128(_mm_or_si128(
				_mm_slli_epi32(red, 16), _mm_slli_epi32(green, 8)), blue);
			_mm_storeu_si128((__m128i *)(destination + x + half * 4), color);
			red8 = _mm_srli_si128(red8, 8);
			green8 = _mm_srli_si128(green8, 8);
			blue8 = _mm_srli_si128(blue8, 8);
		}
	}
#endif
	for (; x < width; x++) {
		const UINT16 pixel = source[x];
		const UINT32 red5 = (pixel >> 11) & 0x1f;
		const UINT32 green5 = (pixel >> 6) & 0x1f;
		const UINT32 blue5 = (pixel >> (native5551 ? 1 : 0)) & 0x1f;
		const UINT32 red8 = (red5 << 3) | (red5 >> 2);
		const UINT32 green8 = (green5 << 3) | (green5 >> 2);
		const UINT32 blue8 = (blue5 << 3) | (blue5 >> 2);
		destination[x] = (red8 << 16) | (green8 << 8) | blue8;
	}
}

static void NamcosGlConvertNative16Rgb565Row(const UINT16 *source,
	UINT16 *destination, INT32 width, INT32 native5551)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	const uint16x8_t mask5 = vdupq_n_u16(0x001f);
	for (; x + 8 <= width; x += 8) {
		const uint16x8_t pixel = vld1q_u16(source + x);
		const uint16x8_t red = vandq_u16(pixel, vdupq_n_u16(0xf800));
		const uint16x8_t green5 = vandq_u16(vshrq_n_u16(pixel, 6), mask5);
		const uint16x8_t green6 = vorrq_u16(vshlq_n_u16(green5, 1),
			vshrq_n_u16(green5, 4));
		const uint16x8_t blue = vandq_u16(native5551 ?
			vshrq_n_u16(pixel, 1) : pixel, mask5);
		vst1q_u16(destination + x, vorrq_u16(vorrq_u16(red,
			vshlq_n_u16(green6, 5)), blue));
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i mask5 = _mm_set1_epi16(0x001f);
	const __m128i redMask = _mm_set1_epi16((short)0xf800);
	for (; x + 8 <= width; x += 8) {
		const __m128i pixel = _mm_loadu_si128((const __m128i *)(source + x));
		const __m128i red = _mm_and_si128(pixel, redMask);
		const __m128i green5 = _mm_and_si128(_mm_srli_epi16(pixel, 6), mask5);
		const __m128i green6 = _mm_or_si128(_mm_slli_epi16(green5, 1),
			_mm_srli_epi16(green5, 4));
		const __m128i blue = _mm_and_si128(native5551 ?
			_mm_srli_epi16(pixel, 1) : pixel, mask5);
		_mm_storeu_si128((__m128i *)(destination + x),
			_mm_or_si128(_mm_or_si128(red, _mm_slli_epi16(green6, 5)), blue));
	}
#endif
	for (; x < width; x++) {
		const UINT16 pixel = source[x];
		const UINT16 green5 = (pixel >> 6) & 0x001f;
		const UINT16 green6 = (UINT16)((green5 << 1) | (green5 >> 4));
		destination[x] = (UINT16)((pixel & 0xf800) | (green6 << 5) |
			((pixel >> (native5551 ? 1 : 0)) & 0x001f));
	}
}

static void NamcosGlConvertRgbaRgb565Row(const UINT8 *source,
	UINT16 *destination, INT32 width, INT32 redOffset, INT32 blueOffset)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	for (; x + 8 <= width; x += 8) {
		const uint8x8x4_t rgba = vld4_u8(source + (size_t)x * 4);
		const uint8x8_t red8 = redOffset == 0 ? rgba.val[0] : rgba.val[2];
		const uint8x8_t blue8 = blueOffset == 0 ? rgba.val[0] : rgba.val[2];
		const uint16x8_t red = vshlq_n_u16(
			vmovl_u8(vshr_n_u8(red8, 3)), 11);
		const uint16x8_t green = vshlq_n_u16(
			vmovl_u8(vshr_n_u8(rgba.val[1], 2)), 5);
		const uint16x8_t blue = vmovl_u8(vshr_n_u8(blue8, 3));
		vst1q_u16(destination + x,
			vorrq_u16(vorrq_u16(red, green), blue));
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i redMask = _mm_set1_epi32(0x000000f8);
	const __m128i greenMask = _mm_set1_epi32(0x0000fc00);
	const __m128i bias32 = _mm_set1_epi32(0x00008000);
	const __m128i bias16 = _mm_set1_epi16((short)0x8000);
	const __m128i zero = _mm_setzero_si128();
	for (; x + 4 <= width; x += 4) {
		const __m128i rgba = _mm_loadu_si128(
			(const __m128i *)(source + (size_t)x * 4));
		const __m128i redBytes = redOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i blueBytes = blueOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i red = _mm_slli_epi32(
			_mm_and_si128(redBytes, redMask), 8);
		const __m128i green = _mm_srli_epi32(
			_mm_and_si128(rgba, greenMask), 5);
		const __m128i blue = _mm_srli_epi32(
			_mm_and_si128(blueBytes, redMask), 3);
		const __m128i color = _mm_or_si128(_mm_or_si128(red, green), blue);
		const __m128i packed = _mm_xor_si128(
			_mm_packs_epi32(_mm_sub_epi32(color, bias32), zero), bias16);
		_mm_storel_epi64((__m128i *)(destination + x), packed);
	}
#endif
	for (; x < width; x++) {
		const UINT8 *pixel = source + (size_t)x * 4;
		destination[x] = (UINT16)(((pixel[redOffset] & 0xf8) << 8) |
			((pixel[1] & 0xfc) << 3) | (pixel[blueOffset] >> 3));
	}
}

static void NamcosGlConvertRgbaXrgbRow(const UINT8 *source,
	UINT32 *destination, INT32 width, INT32 redOffset, INT32 blueOffset)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	for (; x + 8 <= width; x += 8) {
		const uint8x8x4_t rgba = vld4_u8(source + (size_t)x * 4);
		const uint8x8_t red5 = vshr_n_u8(
			redOffset == 0 ? rgba.val[0] : rgba.val[2], 3);
		const uint8x8_t green5 = vshr_n_u8(rgba.val[1], 3);
		const uint8x8_t blue5 = vshr_n_u8(
			blueOffset == 0 ? rgba.val[0] : rgba.val[2], 3);
		const uint16x8_t red8 = vmovl_u8(vorr_u8(vshl_n_u8(red5, 3),
			vshr_n_u8(red5, 2)));
		const uint16x8_t green8 = vmovl_u8(vorr_u8(vshl_n_u8(green5, 3),
			vshr_n_u8(green5, 2)));
		const uint16x8_t blue8 = vmovl_u8(vorr_u8(vshl_n_u8(blue5, 3),
			vshr_n_u8(blue5, 2)));
		for (INT32 half = 0; half < 2; half++) {
			const uint32x4_t red = vmovl_u16(half == 0 ?
				vget_low_u16(red8) : vget_high_u16(red8));
			const uint32x4_t green = vmovl_u16(half == 0 ?
				vget_low_u16(green8) : vget_high_u16(green8));
			const uint32x4_t blue = vmovl_u16(half == 0 ?
				vget_low_u16(blue8) : vget_high_u16(blue8));
			vst1q_u32(destination + x + half * 4,
				vorrq_u32(vorrq_u32(vshlq_n_u32(red, 16),
				vshlq_n_u32(green, 8)), blue));
		}
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i channelMask = _mm_set1_epi32(0x000000f8);
	for (; x + 4 <= width; x += 4) {
		const __m128i rgba = _mm_loadu_si128(
			(const __m128i *)(source + (size_t)x * 4));
		const __m128i redBytes = redOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i blueBytes = blueOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i red5 = _mm_and_si128(redBytes, channelMask);
		const __m128i green5 = _mm_and_si128(
			_mm_srli_epi32(rgba, 8), channelMask);
		const __m128i blue5 = _mm_and_si128(blueBytes, channelMask);
		const __m128i red = _mm_slli_epi32(
			_mm_or_si128(red5, _mm_srli_epi32(red5, 5)), 16);
		const __m128i green = _mm_slli_epi32(
			_mm_or_si128(green5, _mm_srli_epi32(green5, 5)), 8);
		const __m128i blue = _mm_or_si128(blue5, _mm_srli_epi32(blue5, 5));
		_mm_storeu_si128((__m128i *)(destination + x),
			_mm_or_si128(_mm_or_si128(red, green), blue));
	}
#endif
	for (; x < width; x++) {
		const UINT8 *pixel = source + (size_t)x * 4;
		const UINT32 red5 = pixel[redOffset] >> 3;
		const UINT32 green5 = pixel[1] >> 3;
		const UINT32 blue5 = pixel[blueOffset] >> 3;
		const UINT32 red8 = (red5 << 3) | (red5 >> 2);
		const UINT32 green8 = (green5 << 3) | (green5 >> 2);
		const UINT32 blue8 = (blue5 << 3) | (blue5 >> 2);
		destination[x] = (red8 << 16) | (green8 << 8) | blue8;
	}
}

#if defined(NAMCOS_GL_X86_SSE2)
static inline void NamcosGlStoreBgr24x4(__m128i color, UINT8 *destination)
{
	const UINT32 c0 = (UINT32)_mm_cvtsi128_si32(color);
	const UINT32 c1 = (UINT32)_mm_cvtsi128_si32(_mm_srli_si128(color, 4));
	const UINT32 c2 = (UINT32)_mm_cvtsi128_si32(_mm_srli_si128(color, 8));
	const UINT32 c3 = (UINT32)_mm_cvtsi128_si32(_mm_srli_si128(color, 12));
	const UINT32 packed[3] = {
		c0 | (c1 << 24),
		(c1 >> 8) | (c2 << 16),
		(c2 >> 16) | (c3 << 8)
	};
	memcpy(destination, packed, sizeof(packed));
}
#endif

static void NamcosGlConvertNative16Rgb888Row(const UINT16 *source,
	UINT8 *destination, INT32 width, INT32 native5551)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	const uint16x8_t mask5 = vdupq_n_u16(0x001f);
	for (; x + 8 <= width; x += 8) {
		const uint16x8_t pixel = vld1q_u16(source + x);
		const uint16x8_t red5 = vandq_u16(vshrq_n_u16(pixel, 11), mask5);
		const uint16x8_t green5 = vandq_u16(vshrq_n_u16(pixel, 6), mask5);
		const uint16x8_t blue5 = vandq_u16(native5551 ?
			vshrq_n_u16(pixel, 1) : pixel, mask5);
		uint8x8x3_t bgr;
		bgr.val[0] = vmovn_u16(vorrq_u16(vshlq_n_u16(blue5, 3),
			vshrq_n_u16(blue5, 2)));
		bgr.val[1] = vmovn_u16(vorrq_u16(vshlq_n_u16(green5, 3),
			vshrq_n_u16(green5, 2)));
		bgr.val[2] = vmovn_u16(vorrq_u16(vshlq_n_u16(red5, 3),
			vshrq_n_u16(red5, 2)));
		vst3_u8(destination + (size_t)x * 3, bgr);
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i mask5 = _mm_set1_epi16(0x001f);
	const __m128i zero = _mm_setzero_si128();
	for (; x + 4 <= width; x += 4) {
		const __m128i pixel = _mm_loadl_epi64((const __m128i *)(source + x));
		const __m128i red5 = _mm_and_si128(_mm_srli_epi16(pixel, 11), mask5);
		const __m128i green5 = _mm_and_si128(_mm_srli_epi16(pixel, 6), mask5);
		const __m128i blue5 = _mm_and_si128(native5551 ?
			_mm_srli_epi16(pixel, 1) : pixel, mask5);
		const __m128i red8 = _mm_or_si128(_mm_slli_epi16(red5, 3),
			_mm_srli_epi16(red5, 2));
		const __m128i green8 = _mm_or_si128(_mm_slli_epi16(green5, 3),
			_mm_srli_epi16(green5, 2));
		const __m128i blue8 = _mm_or_si128(_mm_slli_epi16(blue5, 3),
			_mm_srli_epi16(blue5, 2));
		const __m128i color = _mm_or_si128(_mm_or_si128(
			_mm_slli_epi32(_mm_unpacklo_epi16(red8, zero), 16),
			_mm_slli_epi32(_mm_unpacklo_epi16(green8, zero), 8)),
			_mm_unpacklo_epi16(blue8, zero));
		NamcosGlStoreBgr24x4(color, destination + (size_t)x * 3);
	}
#endif
	for (; x < width; x++) {
		const UINT16 pixel = source[x];
		const UINT32 red5 = (pixel >> 11) & 0x1f;
		const UINT32 green5 = (pixel >> 6) & 0x1f;
		const UINT32 blue5 = (pixel >> (native5551 ? 1 : 0)) & 0x1f;
		destination[(size_t)x * 3 + 0] = (UINT8)((blue5 << 3) | (blue5 >> 2));
		destination[(size_t)x * 3 + 1] = (UINT8)((green5 << 3) | (green5 >> 2));
		destination[(size_t)x * 3 + 2] = (UINT8)((red5 << 3) | (red5 >> 2));
	}
}

static void NamcosGlConvertRgbaRgb888Row(const UINT8 *source,
	UINT8 *destination, INT32 width, INT32 redOffset, INT32 blueOffset)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	for (; x + 8 <= width; x += 8) {
		const uint8x8x4_t rgba = vld4_u8(source + (size_t)x * 4);
		const uint8x8_t red5 = vshr_n_u8(
			redOffset == 0 ? rgba.val[0] : rgba.val[2], 3);
		const uint8x8_t green5 = vshr_n_u8(rgba.val[1], 3);
		const uint8x8_t blue5 = vshr_n_u8(
			blueOffset == 0 ? rgba.val[0] : rgba.val[2], 3);
		uint8x8x3_t bgr;
		bgr.val[0] = vorr_u8(vshl_n_u8(blue5, 3), vshr_n_u8(blue5, 2));
		bgr.val[1] = vorr_u8(vshl_n_u8(green5, 3), vshr_n_u8(green5, 2));
		bgr.val[2] = vorr_u8(vshl_n_u8(red5, 3), vshr_n_u8(red5, 2));
		vst3_u8(destination + (size_t)x * 3, bgr);
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i channelMask = _mm_set1_epi32(0x000000f8);
	for (; x + 4 <= width; x += 4) {
		const __m128i rgba = _mm_loadu_si128(
			(const __m128i *)(source + (size_t)x * 4));
		const __m128i redBytes = redOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i blueBytes = blueOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i red5 = _mm_and_si128(redBytes, channelMask);
		const __m128i green5 = _mm_and_si128(
			_mm_srli_epi32(rgba, 8), channelMask);
		const __m128i blue5 = _mm_and_si128(blueBytes, channelMask);
		const __m128i color = _mm_or_si128(_mm_or_si128(
			_mm_slli_epi32(_mm_or_si128(red5, _mm_srli_epi32(red5, 5)), 16),
			_mm_slli_epi32(_mm_or_si128(green5, _mm_srli_epi32(green5, 5)), 8)),
			_mm_or_si128(blue5, _mm_srli_epi32(blue5, 5)));
		NamcosGlStoreBgr24x4(color, destination + (size_t)x * 3);
	}
#endif
	for (; x < width; x++) {
		const UINT8 *pixel = source + (size_t)x * 4;
		const UINT8 red5 = pixel[redOffset] >> 3;
		const UINT8 green5 = pixel[1] >> 3;
		const UINT8 blue5 = pixel[blueOffset] >> 3;
		destination[(size_t)x * 3 + 0] = (UINT8)((blue5 << 3) | (blue5 >> 2));
		destination[(size_t)x * 3 + 1] = (UINT8)((green5 << 3) | (green5 >> 2));
		destination[(size_t)x * 3 + 2] = (UINT8)((red5 << 3) | (red5 >> 2));
	}
}

static void NamcosGlConvertNative16IndexedRow(const UINT16 *source,
	UINT16 *destination, INT32 width, INT32 native5551)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	const uint16x8_t redMask = vdupq_n_u16(0xf800);
	const uint16x8_t greenMask = vdupq_n_u16(0x07c0);
	const uint16x8_t blueMask = vdupq_n_u16(native5551 ? 0x003e : 0x001f);
	for (; x + 8 <= width; x += 8) {
		const uint16x8_t pixel = vld1q_u16(source + x);
		uint16x8_t output = vshrq_n_u16(vandq_u16(pixel, redMask), 11);
		output = vorrq_u16(output,
			vshrq_n_u16(vandq_u16(pixel, greenMask), 1));
		output = vorrq_u16(output, native5551 ?
			vshlq_n_u16(vandq_u16(pixel, blueMask), 9) :
			vshlq_n_u16(vandq_u16(pixel, blueMask), 10));
		vst1q_u16(destination + x, output);
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i redMask = _mm_set1_epi16((short)0xf800);
	const __m128i greenMask = _mm_set1_epi16(0x07c0);
	const __m128i blueMask = _mm_set1_epi16(native5551 ? 0x003e : 0x001f);
	for (; x + 8 <= width; x += 8) {
		const __m128i pixel = _mm_loadu_si128((const __m128i *)(source + x));
		__m128i output = _mm_srli_epi16(_mm_and_si128(pixel, redMask), 11);
		output = _mm_or_si128(output,
			_mm_srli_epi16(_mm_and_si128(pixel, greenMask), 1));
		output = _mm_or_si128(output, native5551 ?
			_mm_slli_epi16(_mm_and_si128(pixel, blueMask), 9) :
			_mm_slli_epi16(_mm_and_si128(pixel, blueMask), 10));
		_mm_storeu_si128((__m128i *)(destination + x), output);
	}
#endif
	if (native5551) {
		for (; x < width; x++) {
			const UINT16 pixel = source[x];
			destination[x] = (UINT16)(((pixel >> 11) & 0x001f) |
				((pixel >> 1) & 0x03e0) | ((pixel << 9) & 0x7c00));
		}
	} else {
		for (; x < width; x++) {
			const UINT16 pixel = source[x];
			destination[x] = (UINT16)(((pixel >> 11) & 0x001f) |
				((pixel >> 1) & 0x03e0) | ((pixel << 10) & 0x7c00));
		}
	}
}

static void NamcosGlConvertRgbaIndexedRow(const UINT8 *source,
	UINT16 *destination, INT32 width, INT32 redOffset, INT32 blueOffset)
{
	INT32 x = 0;
#if defined(NAMCOS_GL_ARM_NEON)
	for (; x + 8 <= width; x += 8) {
		const uint8x8x4_t rgba = vld4_u8(source + (size_t)x * 4);
		const uint8x8_t red8 = redOffset == 0 ? rgba.val[0] : rgba.val[2];
		const uint8x8_t blue8 = blueOffset == 0 ? rgba.val[0] : rgba.val[2];
		const uint16x8_t red = vmovl_u8(vshr_n_u8(red8, 3));
		const uint16x8_t green = vshlq_n_u16(
			vmovl_u8(vshr_n_u8(rgba.val[1], 3)), 5);
		const uint16x8_t blue = vshlq_n_u16(
			vmovl_u8(vshr_n_u8(blue8, 3)), 10);
		vst1q_u16(destination + x,
			vorrq_u16(vorrq_u16(red, green), blue));
	}
#elif defined(NAMCOS_GL_X86_SSE2)
	const __m128i channelMask = _mm_set1_epi32(0xf8);
	const __m128i zero = _mm_setzero_si128();
	for (; x + 4 <= width; x += 4) {
		const __m128i rgba = _mm_loadu_si128(
			(const __m128i *)(source + (size_t)x * 4));
		const __m128i redBytes = redOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i blueBytes = blueOffset == 0 ? rgba :
			_mm_srli_epi32(rgba, 16);
		const __m128i red = _mm_srli_epi32(
			_mm_and_si128(redBytes, channelMask), 3);
		const __m128i green = _mm_slli_epi32(
			_mm_and_si128(_mm_srli_epi32(rgba, 8), channelMask), 2);
		const __m128i blue = _mm_slli_epi32(
			_mm_and_si128(blueBytes, channelMask), 7);
		const __m128i color = _mm_or_si128(_mm_or_si128(red, green), blue);
		_mm_storel_epi64((__m128i *)(destination + x),
			_mm_packs_epi32(color, zero));
	}
#endif
	for (; x < width; x++) {
		const UINT8 *pixel = source + (size_t)x * 4;
		destination[x] = (UINT16)((pixel[redOffset] >> 3) |
			((pixel[1] & 0xf8) << 2) |
			((pixel[blueOffset] & 0xf8) << 7));
	}
}

static void NamcosGlConvertNative16Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlReadConvertContext *context = (NamcosGlReadConvertContext *)opaque;

	for (INT32 y = begin; y < end; y++) {
		const UINT16 *source = (const UINT16 *)(context->source +
			(size_t)y * context->sourcePitch);
		if (context->destinationBytes == 4) {
			UINT32 *destination = (UINT32 *)(context->destination +
				y * context->destinationPitch);
			if (context->directXrgb8888) {
				NamcosGlConvertNative16XrgbRow(source, destination, context->width,
					context->native5551);
			} else {
				for (INT32 x = 0; x < context->width; x++) {
					destination[x] = context->directReadTable[source[x]];
				}
			}
		} else if (context->destinationBytes == 2) {
			UINT16 *destination = (UINT16 *)(context->destination +
				y * context->destinationPitch);
			if (context->directRgb565) {
				NamcosGlConvertNative16Rgb565Row(source, destination, context->width,
					context->native5551);
			} else {
				for (INT32 x = 0; x < context->width; x++) {
					destination[x] = (UINT16)context->directReadTable[source[x]];
				}
			}
		} else if (context->destinationBytes == 3) {
			UINT8 *destination = context->destination + y * context->destinationPitch;
			if (context->directXrgb8888) {
				NamcosGlConvertNative16Rgb888Row(source, destination, context->width,
					context->native5551);
			} else {
				for (INT32 x = 0; x < context->width; x++, destination += 3) {
					const UINT32 color = context->directReadTable[source[x]];
					destination[0] = (UINT8)color;
					destination[1] = (UINT8)(color >> 8);
					destination[2] = (UINT8)(color >> 16);
				}
			}
		} else {
			UINT16 *destination = context->indexedDestination + y * context->width;
			NamcosGlConvertNative16IndexedRow(source, destination, context->width,
				context->native5551);
		}
	}
}

static void NamcosGlConvertRgbaRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosGlReadConvertContext *context = (NamcosGlReadConvertContext *)opaque;

	for (INT32 y = begin; y < end; y++) {
		const UINT8 *source = context->source + (size_t)y * context->sourcePitch;
		if (context->destinationBytes == 4) {
			UINT32 *destination = (UINT32 *)(context->destination +
				y * context->destinationPitch);
			if (context->directXrgb8888) {
				NamcosGlConvertRgbaXrgbRow(source, destination, context->width,
					context->redOffset, context->blueOffset);
			} else {
				for (INT32 x = 0; x < context->width; x++, source += 4) {
					const UINT16 color = (UINT16)((source[context->redOffset] >> 3) |
						((source[1] & 0xf8) << 2) |
						((source[context->blueOffset] & 0xf8) << 7));
					destination[x] = context->palette[color];
				}
			}
		} else if (context->destinationBytes == 2) {
			UINT16 *destination = (UINT16 *)(context->destination +
				y * context->destinationPitch);
			if (context->directRgb565) {
				NamcosGlConvertRgbaRgb565Row(source, destination, context->width,
					context->redOffset, context->blueOffset);
			} else {
				for (INT32 x = 0; x < context->width; x++, source += 4) {
					const UINT16 color = (UINT16)((source[context->redOffset] >> 3) |
						((source[1] & 0xf8) << 2) |
						((source[context->blueOffset] & 0xf8) << 7));
					destination[x] = (UINT16)context->palette[color];
				}
			}
		} else if (context->destinationBytes == 3) {
			UINT8 *destination = context->destination + y * context->destinationPitch;
			if (context->directXrgb8888) {
				NamcosGlConvertRgbaRgb888Row(source, destination, context->width,
					context->redOffset, context->blueOffset);
			} else {
				for (INT32 x = 0; x < context->width; x++, source += 4, destination += 3) {
					const UINT16 color = (UINT16)((source[context->redOffset] >> 3) |
						((source[1] & 0xf8) << 2) |
						((source[context->blueOffset] & 0xf8) << 7));
					const UINT32 outputColor = context->palette[color];
					destination[0] = (UINT8)outputColor;
					destination[1] = (UINT8)(outputColor >> 8);
					destination[2] = (UINT8)(outputColor >> 16);
				}
			}
		} else {
			UINT16 *destination = context->indexedDestination + y * context->width;
			NamcosGlConvertRgbaIndexedRow(source, destination, context->width,
				context->redOffset, context->blueOffset);
		}
	}
}

static void NamcosGlConvertReadRows(const NamcosFrameConvertContext *frame,
	NamcosPolyThreadCallback callback, NamcosGlReadConvertContext *context,
	INT32 rows)
{
	const INT64 work = (INT64)frame->outputWidth * rows;
	if (frame->threadPool != NULL) {
		frame->threadPool->ParallelForWork(rows, work, 32768,
			callback, context);
	} else {
		callback(context, 0, rows);
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
	INT32 firstRow;
};

static void NamcosPackRgb24Rows(void *opaque, INT32 begin, INT32 end)
{
	NamcosPackRgb24Context *context = (NamcosPackRgb24Context *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const INT32 y = context->firstRow + row;
		if (context->selectedRows != NULL && !context->selectedRows[y]) continue;
		NamcosPackRgb24Row(context->frame, context->output, y);
	}
}

static void NamcosPackRgb24Selected(const NamcosFrameConvertContext *frame,
	UINT8 *output, const UINT8 *selectedRows, INT32 selectedCount,
	INT32 firstRow, INT32 rowCount)
{
	if (selectedCount <= 0 || rowCount <= 0) return;
	NamcosPackRgb24Context context;
	context.frame = frame;
	context.output = output;
	context.selectedRows = selectedRows;
	context.firstRow = firstRow;
	const INT64 work = (INT64)frame->sourceWidth * selectedCount * 3;
	if (frame->threadPool != NULL) {
		frame->threadPool->ParallelForWork(rowCount, work, 196608,
			NamcosPackRgb24Rows, &context);
	} else {
		NamcosPackRgb24Rows(&context, 0, rowCount);
	}
}

static void NamcosPackRgb24(const NamcosFrameConvertContext *frame, UINT8 *output)
{
	NamcosPackRgb24Context context;
	context.frame = frame;
	context.output = output;
	context.selectedRows = NULL;
	context.firstRow = 0;
	const INT64 work = (INT64)frame->sourceWidth * frame->sourceHeight * 3;
	if (frame->threadPool != NULL) {
		frame->threadPool->ParallelForWork(frame->sourceHeight, work, 196608,
			NamcosPackRgb24Rows, &context);
	} else {
		NamcosPackRgb24Rows(&context, 0, frame->sourceHeight);
	}
}

#if defined(FBNEO_NAMCOS_OPENGL_ES2)

struct NamcosCopyPitchedRowsContext
{
	const UINT8 *source;
	UINT8 *destination;
	size_t rowBytes;
	INT32 sourcePitch;
	INT32 destinationPitch;
};

static void NamcosCopyPitchedRowsWorker(void *opaque, INT32 begin, INT32 end)
{
	NamcosCopyPitchedRowsContext *context =
		(NamcosCopyPitchedRowsContext *)opaque;
	for (INT32 row = begin; row < end; row++) {
		memcpy(context->destination + (size_t)row * context->destinationPitch,
			context->source + (size_t)row * context->sourcePitch,
			context->rowBytes);
	}
}

static void NamcosCopyPitchedRows(const UINT8 *source, UINT8 *destination,
	size_t rowBytes, INT32 sourcePitch, INT32 destinationPitch, INT32 rows,
	NamcosPolyThreadPool *threadPool)
{
	if (source == NULL || destination == NULL || rowBytes == 0 || rows <= 0)
		return;
	NamcosCopyPitchedRowsContext context;
	context.source = source;
	context.destination = destination;
	context.rowBytes = rowBytes;
	context.sourcePitch = sourcePitch;
	context.destinationPitch = destinationPitch;
	const INT64 work = (INT64)rowBytes * rows;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(rows, work, 196608,
			NamcosCopyPitchedRowsWorker, &context);
	} else {
		NamcosCopyPitchedRowsWorker(&context, 0, rows);
	}
}

#endif

#if defined(_WIN32) || defined(FBNEO_NAMCOS_OPENGL_ES2)

struct NamcosCopy16RowsContext
{
	const UINT16 *source;
	UINT16 *destination;
	const UINT8 *selectedRows;
	INT32 width;
	INT32 firstRow;
};

static void NamcosCopy16RowsWorker(void *opaque, INT32 begin, INT32 end)
{
	NamcosCopy16RowsContext *context = (NamcosCopy16RowsContext *)opaque;
	for (INT32 row = begin; row < end; row++) {
		const INT32 y = context->firstRow + row;
		if (context->selectedRows != NULL && !context->selectedRows[y]) continue;
		memcpy(context->destination + (size_t)y * context->width,
			context->source + (size_t)y * 1024,
			(size_t)context->width * sizeof(UINT16));
	}
}

static void NamcosCopy16RowsSelected(const UINT16 *source, UINT16 *destination,
	INT32 width, INT32 rows, const UINT8 *selectedRows, INT32 selectedCount,
	NamcosPolyThreadPool *threadPool, INT32 firstRow = 0, INT32 rowCount = -1)
{
	if (selectedCount <= 0) return;
	if (rowCount < 0) rowCount = rows;
	if (rowCount <= 0) return;

	NamcosCopy16RowsContext context;
	context.source = source;
	context.destination = destination;
	context.selectedRows = selectedRows;
	context.width = width;
	context.firstRow = firstRow;
	const INT64 work = (INT64)width * selectedCount;
	if (threadPool != NULL) {
		threadPool->ParallelForWork(rowCount, work, 65536,
			NamcosCopy16RowsWorker, &context);
	} else {
		NamcosCopy16RowsWorker(&context, 0, rowCount);
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

struct NamcosFrameObservedRows
{
	INT32 firstRow;
	INT32 rowCount;
	bool valid;
};

static void NamcosFrameGetObservedRowsCached(
	const NamcosFrameConvertContext *frame, NamcosFrameObservedRows *observed,
	INT32 *firstRow, INT32 *rowCount)
{
	if (!observed->valid) {
		NamcosFrameGetObservedRows(frame, &observed->firstRow,
			&observed->rowCount);
		observed->valid = true;
	}
	*firstRow = observed->firstRow;
	*rowCount = observed->rowCount;
}

static bool NamcosFrameRowsMatch(const UINT64 *remembered,
	const UINT64 *current, INT32 displayY, INT32 firstRow, INT32 rowCount)
{
	if (rowCount <= 0) return true;

	const INT32 sourceY = (displayY + firstRow) & 0x3ff;
	const INT32 firstCount = rowCount < 1024 - sourceY ?
		rowCount : 1024 - sourceY;
	if (memcmp(remembered + firstRow, current + sourceY,
		(size_t)firstCount * sizeof(UINT64)) != 0) return false;

	const INT32 secondCount = rowCount - firstCount;
	return secondCount <= 0 || memcmp(remembered + firstRow + firstCount,
		current, (size_t)secondCount * sizeof(UINT64)) == 0;
}

static void NamcosFrameRememberRows(UINT64 *remembered, const UINT64 *current,
	INT32 displayY, INT32 firstRow, INT32 rowCount)
{
	if (rowCount <= 0) return;

	const INT32 sourceY = (displayY + firstRow) & 0x3ff;
	const INT32 firstCount = rowCount < 1024 - sourceY ?
		rowCount : 1024 - sourceY;
	memcpy(remembered + firstRow, current + sourceY,
		(size_t)firstCount * sizeof(UINT64));

	const INT32 secondCount = rowCount - firstCount;
	if (secondCount > 0) {
		memcpy(remembered + firstRow + firstCount, current,
			(size_t)secondCount * sizeof(UINT64));
	}
}

static void NamcosFrameRememberUploadRows(UINT64 *remembered,
	const UINT64 *current, INT32 displayY, INT32 rowCount)
{
	if (current != NULL) {
		NamcosFrameRememberRows(remembered, current, displayY, 0, rowCount);
	} else {
		memset(remembered, 0, (size_t)rowCount * sizeof(UINT64));
	}
}

static bool NamcosFrameOutputLayoutMatches(const NamcosFrameOutputKey *key,
	const NamcosFrameConvertContext *frame, const void *destination,
	INT32 pitch, INT32 bytes, const UINT32 *palette)
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
	return true;
}

static bool NamcosFrameOutputMatches(const NamcosFrameOutputKey *key,
	const NamcosFrameConvertContext *frame, const void *destination,
	INT32 pitch, INT32 bytes, const UINT32 *palette,
	const UINT64 *rowGeneration, NamcosFrameObservedRows *observed)
{
	if (!NamcosFrameOutputLayoutMatches(key, frame, destination, pitch,
		bytes, palette)) return false;
	if (key->generation == frame->vramGeneration) return true;
	if (frame->vramRowGeneration == NULL || frame->sourceHeight > 1024) return false;
	INT32 firstRow;
	INT32 rowCount;
	NamcosFrameGetObservedRowsCached(frame, observed, &firstRow, &rowCount);
	return NamcosFrameRowsMatch(rowGeneration, frame->vramRowGeneration,
		frame->displayY, firstRow, rowCount);
}

static bool NamcosFrameGetPartialReadback(const NamcosFrameOutputKey *key,
	const NamcosFrameConvertContext *frame, const void *destination,
	INT32 pitch, INT32 bytes, const UINT32 *palette,
	const UINT64 *rowGeneration, INT32 *readY, INT32 *readHeight,
	UINT8 *dirtyOutputRows = NULL)
{
	if (readY == NULL || readHeight == NULL ||
		!NamcosFrameOutputLayoutMatches(key, frame, destination, pitch,
			bytes, palette) || frame->vramRowGeneration == NULL ||
		frame->vertical || frame->verticalReconstruct2x ||
		frame->sourceHeight > 1024 || frame->outputHeight > 1024) {
		return false;
	}
	const bool scale1x = frame->sourceHeight == frame->outputHeight;
	const bool scale2x = frame->outputHeight == frame->sourceHeight * 2;
	const bool scaleHalf = frame->sourceHeight == frame->outputHeight * 2;
	const bool cropped1x = scale1x && frame->cropTop >= 0 &&
		frame->cropHeight > 0 &&
		frame->cropTop + frame->cropHeight <= frame->sourceHeight;
	if ((!scale1x && !scale2x && !scaleHalf) ||
		(!cropped1x && (frame->cropTop != 0 ||
			frame->cropHeight != frame->outputHeight))) return false;

	if (dirtyOutputRows != NULL) {
		memset(dirtyOutputRows, 0, (size_t)frame->outputHeight);
	}
	INT32 first = frame->outputHeight;
	INT32 last = -1;
	const INT32 sourceFirst = cropped1x ? frame->cropTop : 0;
	const INT32 sourceLast = cropped1x ?
		frame->cropTop + frame->cropHeight : frame->sourceHeight;
	for (INT32 y = sourceFirst; y < sourceLast; y++) {
		if (rowGeneration[y] !=
			frame->vramRowGeneration[(frame->displayY + y) & 0x3ff]) {
			INT32 mappedFirst;
			INT32 mappedLast;
			if (cropped1x && (frame->cropTop != 0 ||
				frame->cropHeight != frame->outputHeight)) {
				const INT32 relative = y - frame->cropTop;
				mappedFirst = (INT32)(((INT64)relative * frame->outputHeight) /
					frame->cropHeight);
				mappedLast = (INT32)((((INT64)(relative + 1) *
					frame->outputHeight) + frame->cropHeight - 1) /
					frame->cropHeight) - 1;
			} else if (scale2x) {
				mappedFirst = y * 2;
				mappedLast = mappedFirst + 1;
			} else if (scaleHalf) {
				mappedFirst = y / 2;
				mappedLast = mappedFirst;
			} else {
				mappedFirst = y;
				mappedLast = y;
			}
			if (mappedFirst > 0) mappedFirst--;
			if (mappedLast + 1 < frame->outputHeight) mappedLast++;
			if (mappedFirst < first) first = mappedFirst;
			if (mappedLast > last) last = mappedLast;
			if (dirtyOutputRows != NULL) {
				memset(dirtyOutputRows + mappedFirst, 1,
					(size_t)(mappedLast - mappedFirst + 1));
			}
		}
	}
	if (last < first) return false;
	INT32 dirtyRows = last - first + 1;
	if (dirtyOutputRows != NULL) {
		dirtyRows = 0;
		for (INT32 y = first; y <= last; y++) {
			if (dirtyOutputRows[y]) dirtyRows++;
		}
	}
	if (dirtyRows >= (frame->outputHeight * 3) / 4) return false;

	*readY = first;
	*readHeight = last - first + 1;
	return true;
}

static void NamcosFrameRememberOutput(NamcosFrameOutputKey *key,
	const NamcosFrameConvertContext *frame, const void *destination,
	INT32 pitch, INT32 bytes, const UINT32 *palette, UINT64 *rowGeneration,
	NamcosFrameObservedRows *observed)
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
		NamcosFrameGetObservedRowsCached(frame, observed, &firstRow, &rowCount);
		NamcosFrameRememberRows(rowGeneration, frame->vramRowGeneration,
			frame->displayY, firstRow, rowCount);
	}
}

static bool NamcosFrameUploadMatches(const NamcosFrameUploadKey *key,
	const NamcosFrameConvertContext *frame, const UINT64 *rowGeneration,
	NamcosFrameObservedRows *observed)
{
	if (key->displayX != frame->displayX || key->displayY != frame->displayY ||
		key->sourceHeight != frame->sourceHeight) {
		return false;
	}
	if (key->sourceWidth != frame->sourceWidth ||
		key->outputHeight != frame->outputHeight ||
		key->cropTop != frame->cropTop || key->cropHeight != frame->cropHeight ||
		key->rgb24 != frame->rgb24 || key->vertical != frame->vertical) {
		return false;
	}
	if (key->generation == frame->vramGeneration) return true;
	if (frame->vramRowGeneration == NULL) return false;
	INT32 firstRow;
	INT32 rowCount;
	NamcosFrameGetObservedRowsCached(frame, observed, &firstRow, &rowCount);
	return NamcosFrameRowsMatch(rowGeneration, frame->vramRowGeneration,
		frame->displayY, firstRow, rowCount);
}

static void NamcosFrameRememberUpload(NamcosFrameUploadKey *key,
	const NamcosFrameConvertContext *frame, UINT64 *rowGeneration,
	NamcosFrameObservedRows *observed)
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
	if (frame->vramRowGeneration != NULL) {
		INT32 firstRow;
		INT32 rowCount;
		NamcosFrameGetObservedRowsCached(frame, observed, &firstRow, &rowCount);
		NamcosFrameRememberRows(rowGeneration, frame->vramRowGeneration,
			frame->displayY, firstRow, rowCount);
	}
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

struct NamcosFrameRowSpan
{
	INT32 start;
	INT32 count;
};

static INT32 NamcosFrameBuildUploadSpans(const UINT8 *dirtyRows, INT32 rows,
	size_t rowBytes, NamcosFrameRowSpan *spans, INT32 capacity,
	size_t extraCallBytes = 8192)
{
	if (dirtyRows == NULL || rows <= 0 || rowBytes == 0 || spans == NULL ||
		capacity <= 0) return 0;

	INT32 first = 0;
	while (first < rows && !dirtyRows[first]) first++;
	if (first == rows) return 0;
	INT32 last = rows;
	while (last > first && !dirtyRows[last - 1]) last--;

	static const INT32 maxTrackedSpans = 8;
	const INT32 maxSpans = capacity < maxTrackedSpans ?
		capacity : maxTrackedSpans;
	INT32 selectedGapFirst[maxTrackedSpans - 1];
	INT32 selectedGapRows[maxTrackedSpans - 1];
	INT32 selectedCount = 0;
	INT32 y = first;
	while (y < last) {
		while (y < last && dirtyRows[y]) y++;
		const INT32 gapFirst = y;
		while (y < last && !dirtyRows[y]) y++;
		if (y == last) break;
		const size_t gapBytes = (size_t)(y - gapFirst) * rowBytes;
		if (gapBytes <= extraCallBytes || maxSpans <= 1) continue;

		INT32 insert = selectedCount;
		if (selectedCount == maxSpans - 1) {
			if (y - gapFirst <= selectedGapRows[selectedCount - 1]) continue;
			insert--;
		} else {
			selectedCount++;
		}
		while (insert > 0 && y - gapFirst > selectedGapRows[insert - 1]) {
			selectedGapRows[insert] = selectedGapRows[insert - 1];
			selectedGapFirst[insert] = selectedGapFirst[insert - 1];
			insert--;
		}
		selectedGapRows[insert] = y - gapFirst;
		selectedGapFirst[insert] = gapFirst;
	}

	// Selected gaps are ranked by size. Sort the small retained set by row so
	// upload/readback spans can be emitted without scanning every row again.
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
		spans[spanCount].start = spanFirst;
		spans[spanCount].count = selectedGapFirst[gap] - spanFirst;
		spanCount++;
		spanFirst = selectedGapFirst[gap] + selectedGapRows[gap];
	}
	spans[spanCount].start = spanFirst;
	spans[spanCount].count = last - spanFirst;
	spanCount++;

	size_t uploadBytes = (size_t)(last - first) * rowBytes;
	for (INT32 gap = 0; gap < selectedCount; gap++) {
		uploadBytes -= (size_t)selectedGapRows[gap] * rowBytes;
	}
	const size_t boundingBytes = (size_t)(last - first) * rowBytes;
	const size_t extraCallCost = (size_t)(spanCount - 1) * extraCallBytes;
	if (spanCount > 1 && uploadBytes + extraCallCost >= boundingBytes) {
		spans[0].start = first;
		spans[0].count = last - first;
		return 1;
	}
	return spanCount;
}

static bool NamcosFrameReadbackSpansWorthwhile(const NamcosFrameRowSpan *spans,
	INT32 spanCount, INT32 rows, size_t rowBytes, size_t extraCallBytes)
{
	if (spans == NULL || spanCount <= 0 || rows <= 0 || rowBytes == 0)
		return false;

	size_t readBytes = 0;
	for (INT32 span = 0; span < spanCount; span++) {
		readBytes += (size_t)spans[span].count * rowBytes;
	}
	readBytes += (size_t)(spanCount - 1) * extraCallBytes;
	return readBytes < (size_t)rows * rowBytes;
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

static bool NamcosPaletteIsXrgb8888(const UINT32 *palette)
{
	return palette != NULL &&
		palette[0x001f] == 0x00ff0000 &&
		palette[0x03e0] == 0x0000ff00 &&
		palette[0x7c00] == 0x000000ff &&
		palette[0x0010] == 0x00840000 &&
		palette[0x0200] == 0x00008400 &&
		palette[0x4000] == 0x00000084;
}

static bool NamcosFrameOutputIsUniform(const UINT8 *output, INT32 width,
	INT32 height, INT32 pitch, INT32 bytesPerPixel, bool ignoreAlpha)
{
	if (output == NULL || width <= 0 || height <= 0 || bytesPerPixel <= 0) {
		return false;
	}

	// This is a recovery heuristic for failed GL readbacks, not an image
	// comparison.  A bounded, evenly distributed sample catches an all-black or
	// all-white surface without scanning up to 640x480 pixels every frame.
	const INT32 sampleWidth = width < 128 ? width : 128;
	const INT32 sampleHeight = height < 96 ? height : 96;
	const INT32 pixels = sampleWidth * sampleHeight;
	const INT32 tolerance = pixels / 50;
	INT32 nonBlackPixels = 0;
	INT32 nonWhitePixels = 0;
	INT32 sampleXs[128];
	for (INT32 sampleX = 0; sampleX < sampleWidth; sampleX++) {
		sampleXs[sampleX] = (sampleX * width) / sampleWidth;
	}
	const INT32 centerRow = sampleHeight / 2;
	for (INT32 distance = 0; distance < sampleHeight; distance++) {
		const INT32 sampleY = (distance & 1) ?
			centerRow - ((distance + 1) / 2) : centerRow + (distance / 2);
		if (sampleY >= 0 && sampleY < sampleHeight) {
			const INT32 y = (sampleY * height) / sampleHeight;
			const UINT8 *row = output + (size_t)y * pitch;
			for (INT32 sampleX = 0; sampleX < sampleWidth; sampleX++) {
				const INT32 x = sampleXs[sampleX];
				const UINT8 *pixel = row + (size_t)x * bytesPerPixel;
				bool black;
				bool white;
				if (bytesPerPixel == 2) {
					UINT16 first;
					memcpy(&first, pixel, sizeof(first));
					const INT32 red = ignoreAlpha ? (first & 0x1f) : ((first >> 11) & 0x1f);
					const INT32 green = ignoreAlpha ? ((first >> 5) & 0x1f) : ((first >> 5) & 0x3f);
					const INT32 blue = ignoreAlpha ? ((first >> 10) & 0x1f) : (first & 0x1f);
					black = red <= 1 && green <= 1 && blue <= 1;
					white = red >= 30 && green >= (ignoreAlpha ? 30 : 62) && blue >= 30;
				} else {
					const INT32 red = pixel[2];
					const INT32 green = pixel[1];
					const INT32 blue = pixel[0];
					black = red <= 8 && green <= 8 && blue <= 8;
					white = red >= 247 && green >= 247 && blue >= 247;
				}
				if (!black) nonBlackPixels++;
				if (!white) nonWhitePixels++;
				if ((sampleX & 31) == 31 && nonBlackPixels > tolerance &&
					nonWhitePixels > tolerance) return false;
			}
			if (nonBlackPixels > tolerance && nonWhitePixels > tolerance) return false;
		}
	}
	return nonBlackPixels <= tolerance || nonWhitePixels <= tolerance;
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
#ifndef GL_RGB5_A1
#define GL_RGB5_A1 0x8057
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
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88e0
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
		  rasterPendingCount(0),
		  rasterValidationCounter(0),
		  rasterSampleValidationCounter(0),
		  rasterUploadValidationCounter(0),
		  rasterReadbackValidationCounter(0),
		  readbackValidationCounter(0),
		  validatedReadFormat(0),
		  validatedReadType(0),
		  validatedReadPackAlignment(-1),
		  validatedReadPackRowLength(-1),
		  lastRasterClearColor(0),
		  rasterClearColorValid(false),
		  rasterPositionAttribute(-1),
		  rasterColorAttribute(-1),
		  rasterTextureAttribute(-1),
		  rasterStateAttribute(-1),
		  rasterTextureState0Attribute(-1),
		  rasterTextureState1Attribute(-1),
		  rasterSampleUniform(-1),
		  rasterSampleTextureBound(false),
		  rasterScissorValid(false),
		  lastRasterScissorX(0),
		  lastRasterScissorY(0),
		  lastRasterScissorWidth(0),
		  lastRasterScissorHeight(0),
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
		  lastVerticalReconstruct(false),
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
		  lastPackAlignment(-1),
		  lastPackRowLength(-1),
		  lastUnpackAlignment(-1),
		  lastUnpackRowLength(-1),
		  uploadSlotIndex(0),
		  uploadModeValid(false),
		  lastUploadRgb24(false),
		  uploadFrameValid(false),
		  outputFrameValid(false),
		  fullRasterizerCapable(false),
		  rasterizerFailureReason(0),
		  rasterVramSynchronized(false),
		  rasterSampleValid(false),
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
		return available && fullRasterizerCapable;
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
		return available;
	}

	bool HasHardwareVram() const
	{
		return rasterVramSynchronized;
	}

	bool UploadVramRect(UINT16 *vram, UINT64 generation,
		UINT64 *rowGeneration, INT32 x, INT32 y, INT32 width, INT32 height)
	{
		if (!rasterVramSynchronized || vram == NULL || width <= 0 ||
			height <= 0 || width > 1024 || height > 1024) return false;
		const INT32 startX = x & 0x3ff;
		const INT32 startY = y & 0x3ff;
		const INT32 firstWidth = width < 1024 - startX ? width : 1024 - startX;
		const INT32 firstHeight = height < 1024 - startY ? height : 1024 - startY;

		bool uploaded = false;
		if (rasterTransferPixels != NULL &&
			(rasterStateActive || wglGetCurrentContext() == context ||
				wglMakeCurrent(dc, context)) &&
			(!rasterStateActive || FlushRasterVertices())) {
			SetUnpackAlignment(2);
			SetUnpackRowLength(0);
			const INT32 widths[2] = { firstWidth, width - firstWidth };
			const INT32 heights[2] = { firstHeight, height - firstHeight };
			const INT32 xs[2] = { startX, 0 };
			const INT32 ys[2] = { startY, 0 };

			const bool validateUpload =
				(rasterUploadValidationCounter++ & 0x3f) == 0;
			if (validateUpload) glGetError();
			glBindTexture(GL_TEXTURE_2D, rasterTexture);
			uploaded = true;
			for (INT32 yPart = 0; yPart < 2; yPart++) {
				for (INT32 xPart = 0; xPart < 2; xPart++) {
					const INT32 partWidth = widths[xPart];
					const INT32 partHeight = heights[yPart];
					if (partWidth <= 0 || partHeight <= 0) continue;
					NamcosGlCopyVram16RectParallel(vram,
						(UINT16 *)rasterTransferPixels, xs[xPart], ys[yPart],
						partWidth, partHeight, rasterThreadPool);
					glTexSubImage2D(GL_TEXTURE_2D, 0, xs[xPart],
						1024 - ys[yPart] - partHeight,
						partWidth, partHeight,
						GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV,
						rasterTransferPixels);
				}
			}
			uploaded = !validateUpload || glGetError() == GL_NO_ERROR;
			if (uploaded) {
				for (INT32 yPart = 0; yPart < 2; yPart++) {
					for (INT32 xPart = 0; xPart < 2; xPart++) {
						const INT32 partWidth = widths[xPart];
						const INT32 partHeight = heights[yPart];
						if (partWidth <= 0 || partHeight <= 0) continue;
						// CPU and GPU contain the same uploaded pixels.  Remove an
						// older GPU-only overlap instead of reading it back later.
						rasterDirty.Exclude(xs[xPart], ys[yPart],
							xs[xPart] + partWidth - 1,
							ys[yPart] + partHeight - 1);
						if (rasterSampleValid) {
							rasterSampleDirty.Include(xs[xPart], ys[yPart],
								xs[xPart] + partWidth - 1,
								ys[yPart] + partHeight - 1);
						}
					}
				}
			}
			if (!rasterStateActive) glBindTexture(GL_TEXTURE_2D, texture);
			rasterSampleTextureBound = false;
			outputFrameValid = false;
		}

		if (uploaded) {
			if (rowGeneration != NULL) {
				rasterUploadTracker.SetAndRememberRange(rowGeneration, generation,
					startY, firstHeight);
				if (height > firstHeight) {
					rasterUploadTracker.SetAndRememberRange(rowGeneration, generation,
						0, height - firstHeight);
				}
			}
			return true;
		}

		UINT16 *backup = (UINT16 *)malloc((size_t)width * height * sizeof(UINT16));
		if (backup == NULL) return false;
		NamcosGlCopyWrappedVramToLinear(vram, backup, x, y, width, height);
		SynchronizeVram(vram, generation, rowGeneration);
		NamcosGlCopyLinearToWrappedVram(backup, vram, x, y, width, height);
		if (rowGeneration != NULL) {
			for (INT32 row = 0; row < height; row++) {
				rowGeneration[(y + row) & 0x3ff] = generation;
			}
		}
		free(backup);
		return false;
	}

	bool ReadVramRect(UINT16 *vram, UINT64 generation,
		UINT64 *rowGeneration, INT32 x, INT32 y, INT32 width, INT32 height)
	{
		if (!rasterVramSynchronized || vram == NULL || width <= 0 ||
			height <= 0 || width > 1024 || height > 1024) return false;
		if (!NamcosGlRasterDirtyIntersectsWrapped(&rasterDirty,
			x, y, width, height)) return true;
		const bool preserveRasterState = rasterStateActive;
		if (rasterTransferPixels == NULL ||
			(!rasterStateActive && wglGetCurrentContext() != context &&
				!wglMakeCurrent(dc, context)) || !FlushRasterVertices()) {
			return SynchronizeVram(vram, generation, rowGeneration);
		}

		NamcosGlRasterRect readRects[64];
		const INT32 readCount = NamcosGlRasterBuildWrappedReadRects(&rasterDirty,
			x, y, width, height, readRects, 64, 65536);
		if (readCount <= 0) return true;
		bool read = true;

		bindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		SetPackAlignment(2);
		SetPackRowLength(0);
		const bool validateReadback =
			(rasterReadbackValidationCounter++ & 0x3f) == 0;
		if (validateReadback) glGetError();
		for (INT32 i = 0; i < readCount; i++) {
			const INT32 partWidth = readRects[i].x2 - readRects[i].x1 + 1;
			const INT32 partHeight = readRects[i].y2 - readRects[i].y1 + 1;
			glReadPixels(readRects[i].x1,
				1024 - readRects[i].y2 - 1,
				partWidth, partHeight, GL_RGBA, GL_UNSIGNED_BYTE,
				rasterTransferPixels);
			NamcosGlUnpackVramRectParallel(rasterTransferPixels, vram,
				readRects[i].x1, readRects[i].y1, partWidth, partHeight,
				rasterThreadPool);
		}
		read = !validateReadback || glGetError() == GL_NO_ERROR;
		if (!preserveRasterState) EndRasterState(true);
		if (read) {
			for (INT32 i = 0; i < readCount; i++) {
				rasterDirty.Exclude(readRects[i].x1, readRects[i].y1,
					readRects[i].x2, readRects[i].y2);
			}
			return true;
		}
		return SynchronizeVram(vram, generation, rowGeneration);
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
		// The CPU fallback only needs the latest framebuffer contents.  The
		// sample texture is re-uploaded together with the CPU VRAM before the
		// next hardware packet, so copying dirty pixels to it here is wasted.
		if (dirty && !FlushRasterVertices()) return false;
		if (!ReadbackRasterVram(vram)) return false;
		// CPU fallback commands may immediately modify the read-back rows.  Do
		// not retain upload generations across this ownership transition or a
		// following hardware packet can reuse stale texture rows.
		(void)generation;
		(void)rowGeneration;
		rasterUploadTracker.Reset();
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
		void *outputDestination = directDestination != NULL ?
			directDestination : (void *)frame->output;
		const INT32 outputPitch = directDestination != NULL ?
			directPitch : frame->outputWidth * 2;
		const INT32 outputBytes = directDestination != NULL ? directBytes : 2;
		const UINT32 *outputPalette = directDestination != NULL ? palette : NULL;
		NamcosFrameObservedRows observedRows = { 0, 0, false };
		if (outputFrameValid && NamcosFrameOutputMatches(&outputFrameKey, frame,
			outputDestination, outputPitch, outputBytes, outputPalette,
			outputRowGeneration, &observedRows)) {
			return true;
		}
		INT32 readY = 0;
		INT32 readHeight = frame->outputHeight;
		UINT8 readDirtyRows[1024];
		bool partialReadback = outputFrameValid &&
			NamcosFrameGetPartialReadback(&outputFrameKey, frame,
				outputDestination, outputPitch, outputBytes, outputPalette,
				outputRowGeneration, &readY, &readHeight, readDirtyRows);
		NamcosFrameRowSpan readSpans[4];
		INT32 readSpanCount = partialReadback ?
			NamcosFrameBuildUploadSpans(readDirtyRows, frame->outputHeight,
				(size_t)frame->outputWidth * outputBytes, readSpans, 4, 65536) : 0;
		if (partialReadback && !NamcosFrameReadbackSpansWorthwhile(readSpans,
			readSpanCount, frame->outputHeight,
			(size_t)frame->outputWidth * outputBytes, 65536)) {
			partialReadback = false;
			readY = 0;
			readHeight = frame->outputHeight;
			readSpanCount = 0;
		}

		if (!EnsureInitialized(frame->outputWidth, frame->outputHeight)) {
			return false;
		}

		// This converter owns the context and always binds it to the same DC.
		// Checking the context is sufficient and avoids another WGL query per frame.
		if (wglGetCurrentContext() != context && !wglMakeCurrent(dc, context)) {
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
		const bool rasterSource = rasterVramSynchronized && !rgb24;
		if (!rasterSource && (!uploadFrameValid ||
			!NamcosFrameUploadMatches(&uploadFrameKey, frame,
				uploadRowGeneration, &observedRows))) {
			if (rgb24) {
				SetUnpackAlignment(1);
				SetUnpackRowLength(0);
				UploadRgb24(frame);
			} else {
				SetUnpackAlignment(2);
				SetUnpackRowLength(1024);
				uploadSlotIndex = 0;
				INT32 uploadFirstRow;
				INT32 uploadRows;
				NamcosFrameGetObservedRowsCached(frame, &observedRows,
					&uploadFirstRow, &uploadRows);
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
			}
			NamcosFrameRememberUpload(&uploadFrameKey, frame,
				uploadRowGeneration, &observedRows);
			uploadFrameValid = true;
		}

		if (frame->outputShiftX != 0) {
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT);
			rasterClearColorValid = false;
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
		if (!drawUniformsValid || lastVerticalReconstruct !=
			(frame->verticalReconstruct2x != 0)) {
			uniform1i(verticalReconstructUniform,
				frame->verticalReconstruct2x ? 1 : 0);
			lastVerticalReconstruct = frame->verticalReconstruct2x != 0;
		}
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
		if (rasterSource) glBindTexture(GL_TEXTURE_2D, rasterTexture);
		if (partialReadback && readSpanCount > 0) {
			glEnable(GL_SCISSOR_TEST);
			for (INT32 span = 0; span < readSpanCount; span++) {
				glScissor(0, readSpans[span].start, frame->outputWidth,
					readSpans[span].count);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}
			glDisable(GL_SCISSOR_TEST);
		} else {
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
		if (rasterSource) glBindTexture(GL_TEXTURE_2D, texture);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		auto readDirectRows = [&](GLenum format, GLenum type,
			UINT8 *destination, INT32 destinationPitch) {
			if (partialReadback && readSpanCount > 1) {
				for (INT32 span = 0; span < readSpanCount; span++) {
					glReadPixels(0, readSpans[span].start, frame->outputWidth,
						readSpans[span].count, format, type, destination +
						(size_t)readSpans[span].start * destinationPitch);
				}
			} else {
				glReadPixels(0, partialReadback ? readY : 0, frame->outputWidth,
					partialReadback ? readHeight : frame->outputHeight,
					format, type, destination +
					(partialReadback ? (size_t)readY * destinationPitch : 0));
			}
		};
		// Every desktop readback path checks the accumulated GL error directly
		// after glReadPixels, so a separate query here only adds a driver call.
		if (directBytes == 2) {
			if (NamcosPaletteIsRgb565(palette)) {
				SetPackAlignment(2);
				SetPackRowLength(directPitch / 2);
				readDirectRows(GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
					directDestination, directPitch);
				if (!ValidateReadback(GL_RGB, GL_UNSIGNED_SHORT_5_6_5)) {
					Disable();
					return false;
				}
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration,
					&observedRows);
				outputFrameValid = true;
				return true;
			}
			UINT16 *readDestination = directPitch == frame->outputWidth * 2 ?
				(UINT16 *)directDestination : frame->output;
			const INT32 paletteReadY = partialReadback ? readY : 0;
			const INT32 paletteReadHeight = partialReadback ? readHeight :
				frame->outputHeight;
			SetPackAlignment(2);
			SetPackRowLength(0);
			if (partialReadback && readSpanCount > 1) {
				for (INT32 span = 0; span < readSpanCount; span++) {
					glReadPixels(0, readSpans[span].start, frame->outputWidth,
						readSpans[span].count, GL_RGBA,
						GL_UNSIGNED_SHORT_1_5_5_5_REV, readDestination +
						(size_t)readSpans[span].start * frame->outputWidth);
				}
			} else {
				glReadPixels(0, paletteReadY, frame->outputWidth, paletteReadHeight,
					GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV,
					readDestination + (size_t)paletteReadY * frame->outputWidth);
			}
			if (!ValidateReadback(GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV)) {
				Disable();
				return false;
			}
			const INT32 convertSpanCount = partialReadback && readSpanCount > 1 ?
				readSpanCount : 1;
			for (INT32 span = 0; span < convertSpanCount; span++) {
				const INT32 spanY = convertSpanCount > 1 ? readSpans[span].start :
					paletteReadY;
				const INT32 spanRows = convertSpanCount > 1 ? readSpans[span].count :
					paletteReadHeight;
				NamcosGlPaletteConvertContext convertContext;
				convertContext.source = readDestination +
					(size_t)spanY * frame->outputWidth;
				convertContext.destination = directDestination +
					(size_t)spanY * directPitch;
				convertContext.palette = palette;
				convertContext.width = frame->outputWidth;
				convertContext.destinationPitch = directPitch;
				const INT64 convertWork = (INT64)frame->outputWidth * spanRows;
				if (frame->threadPool != NULL) {
					frame->threadPool->ParallelForWork(spanRows, convertWork, 32768,
						NamcosGlConvertPaletteRows, &convertContext);
				} else {
					NamcosGlConvertPaletteRows(&convertContext, 0, spanRows);
				}
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration,
				&observedRows);
			outputFrameValid = true;
			return true;
		}

		if (directDestination != NULL) {
			SetPackAlignment(directBytes == 4 ? 4 : 1);
			SetPackRowLength(directPitch / directBytes);
			readDirectRows(directBytes == 4 ? GL_BGRA : GL_BGR,
				GL_UNSIGNED_BYTE, directDestination, directPitch);
			if (!ValidateReadback(directBytes == 4 ? GL_BGRA : GL_BGR,
				GL_UNSIGNED_BYTE)) {
				Disable();
				return false;
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration,
				&observedRows);
			outputFrameValid = true;
			return true;
		}

		SetPackAlignment(2);
		SetPackRowLength(0);
		if (partialReadback && readSpanCount > 1) {
			for (INT32 span = 0; span < readSpanCount; span++) {
				glReadPixels(0, readSpans[span].start, frame->outputWidth,
					readSpans[span].count, GL_RGBA,
					GL_UNSIGNED_SHORT_1_5_5_5_REV, frame->output +
					(size_t)readSpans[span].start * frame->outputWidth);
			}
		} else {
			glReadPixels(0, partialReadback ? readY : 0, frame->outputWidth,
				partialReadback ? readHeight : frame->outputHeight, GL_RGBA,
				GL_UNSIGNED_SHORT_1_5_5_5_REV, frame->output +
				(partialReadback ? (size_t)readY * frame->outputWidth : 0));
		}

		if (!ValidateReadback(GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV)) {
			Disable();
			return false;
		}
		NamcosFrameRememberOutput(&outputFrameKey, frame, outputDestination,
			outputPitch, outputBytes, outputPalette, outputRowGeneration,
			&observedRows);
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
		rasterSampleValid = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
		rasterSampleDirty.Reset();
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
		lastPackAlignment = -1;
		lastPackRowLength = -1;
		readbackValidationCounter = 0;
		validatedReadFormat = 0;
		validatedReadType = 0;
		validatedReadPackAlignment = -1;
		validatedReadPackRowLength = -1;
		lastUnpackAlignment = -1;
		lastUnpackRowLength = -1;
		drawUniformsValid = false;
		textureUniformValid = false;
		uploadModeValid = false;
		outputFrameValid = false;
	}

private:
	void SetPackAlignment(INT32 alignment)
	{
		if (lastPackAlignment == alignment) return;
		glPixelStorei(GL_PACK_ALIGNMENT, alignment);
		lastPackAlignment = alignment;
	}

	bool ValidateReadback(GLenum format, GLenum type)
	{
		const bool stateChanged = validatedReadFormat != format ||
			validatedReadType != type ||
			validatedReadPackAlignment != lastPackAlignment ||
			validatedReadPackRowLength != lastPackRowLength;
		if (stateChanged || (++readbackValidationCounter & 0x3f) == 0) {
			if (glGetError() != GL_NO_ERROR) return false;
			validatedReadFormat = format;
			validatedReadType = type;
			validatedReadPackAlignment = lastPackAlignment;
			validatedReadPackRowLength = lastPackRowLength;
		}
		return true;
	}

	void SetPackRowLength(INT32 length)
	{
		if (lastPackRowLength == length) return;
		glPixelStorei(GL_PACK_ROW_LENGTH, length);
		lastPackRowLength = length;
	}

	void SetUnpackAlignment(INT32 alignment)
	{
		if (lastUnpackAlignment == alignment) return;
		glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
		lastUnpackAlignment = alignment;
	}

	void SetUnpackRowLength(INT32 length)
	{
		if (lastUnpackRowLength == length) return;
		glPixelStorei(GL_UNPACK_ROW_LENGTH, length);
		lastUnpackRowLength = length;
	}

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
		bool pixelCacheValid;
		bool valid;
	};

	void InvalidateUploadCaches()
	{
		for (INT32 i = 0; i < 4; i++) {
			uploadSlots[i].denseFrames = 0;
			uploadSlots[i].pixelCacheValid = false;
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
		rasterPendingCount = 0;
		rasterValidationCounter = 0;
		rasterSampleValidationCounter = 0;
		rasterUploadValidationCounter = 0;
		rasterReadbackValidationCounter = 0;
		rasterClearColorValid = false;
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
		rasterStateAttribute = -1;
		rasterTextureState0Attribute = -1;
		rasterTextureState1Attribute = -1;
		rasterSampleUniform = -1;
		rasterSampleTextureBound = false;
		rasterScissorValid = false;
		rasterVramSynchronized = false;
		rasterSampleValid = false;
		rasterStateActive = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
		rasterSampleDirty.Reset();
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
			"in float aState;\n"
			"in vec4 aTextureState0;\n"
			"in vec4 aTextureState1;\n"
			"out vec3 vColor;\n"
			"out vec2 vTexture;\n"
			"out float vState;\n"
			"out vec4 vTextureAddress;\n"
			"out vec4 vTextureControl;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0,"
			" 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor;\n"
			" vTexture = aTexture; vState = aState;\n"
			" vTextureAddress = aTextureState0; vTextureControl = aTextureState1;\n"
			"}\n";
		static const char fragmentText[] =
			"#version 130\n"
			"in vec3 vColor;\n"
			"in vec2 vTexture;\n"
			"in float vState;\n"
			"in vec4 vTextureAddress;\n"
			"in vec4 vTextureControl;\n"
			"uniform sampler2D uVram;\n"
			"#define uTextureState0 vec4(vTextureAddress.xy, vTextureControl.xy)\n"
			"#define uTextureState1 vec4(vTextureControl.zw, vTextureAddress.zw)\n"
			"out vec4 outputColor;\n"
			"int wordAt(int x, int y) {\n"
			" vec4 c = texelFetch(uVram, ivec2(x & 1023, 1023 - (y & 1023)), 0);\n"
			" ivec3 q = ivec3(c.rgb * 31.0 + 0.5);\n"
			" return q.r | (q.g << 5) | (q.b << 10) | (c.a >= 0.5 ? 32768 : 0);\n"
			"}\n"
			"vec4 unpackWord(int word) {\n"
			" return vec4(float(word & 31), float((word >> 5) & 31),\n"
			"  float((word >> 10) & 31), (word & 32768) != 0 ? 1.0 : 0.0);\n"
			"}\n"
			"int textureWord() {\n"
			" int u = int(vTexture.x) & int(uTextureState1.x);\n"
			" int v = int(vTexture.y) & int(uTextureState1.y);\n"
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
			" int state = int(vState + 0.5); bool textured = (state & 1) != 0; bool vramCopy = (state & 2) != 0;\n"
			" if (state == 0 || state == 64) { outputColor = vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5) / 31.0, state == 64 ? 1.0 : 0.0); return; }\n"
			" int texel = vramCopy ? wordAt(int(vTextureAddress.x) + int(gl_FragCoord.x) - int(vTextureAddress.z), int(vTextureAddress.y) + 1023 - int(gl_FragCoord.y) - int(vTextureAddress.w)) : (textured ? textureWord() : 0);\n"
			" if (textured && texel == 0) discard;\n"
			" vec4 color = (textured || vramCopy) ? unpackWord(texel) : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && (state & 4) == 0) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = (state & 8) != 0 && (!textured || (texel & 32768) != 0);\n"
			" int backgroundWord = 0;\n"
			" if ((state & 128) != 0 || blend) backgroundWord = wordAt(int(gl_FragCoord.x), 1023 - int(gl_FragCoord.y));\n"
			" if ((state & 128) != 0 && (backgroundWord & 32768) != 0) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = unpackWord(backgroundWord).rgb; int abr = (state >> 4) & 3;\n"
			"  if (abr == 0) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr == 1) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr == 2) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  if (textured) color.a = 1.0; else color.a = 0.0;\n"
			" }\n"
			" if ((state & 64) != 0) color.a = 1.0;\n"
			" outputColor = vec4(color.rgb / 31.0, color.a);\n"
			"}\n";
		static const char legacyVertexText[] =
			"#version 110\n"
			"attribute vec2 aPosition;\n"
			"attribute vec3 aColor;\n"
			"attribute vec2 aTexture;\n"
			"attribute float aState;\n"
			"attribute vec4 aTextureState0;\n"
			"attribute vec4 aTextureState1;\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"varying vec4 vStateBits;\n"
			"varying vec3 vStateControl;\n"
			"varying vec4 vTextureAddress;\n"
			"varying vec4 vTextureControl;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0, 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor;\n"
			" vTexture = aTexture;\n"
			" vStateBits = vec4(mod(aState, 2.0), mod(floor(aState * 0.5), 2.0),"
			" mod(floor(aState * 0.25), 2.0), mod(floor(aState * 0.125), 2.0));\n"
			" vStateControl = vec3(mod(floor(aState * 0.0625), 4.0),"
			" mod(floor(aState * 0.015625), 2.0), step(127.5, aState));\n"
			" vTextureAddress = aTextureState0; vTextureControl = aTextureState1;\n"
			"}\n";
		static const char legacyFragmentText[] =
			"#version 110\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"varying vec4 vStateBits;\n"
			"varying vec3 vStateControl;\n"
			"varying vec4 vTextureAddress;\n"
			"varying vec4 vTextureControl;\n"
			"uniform sampler2D uVram;\n"
			"#define uTextureState0 vec4(vTextureAddress.xy, vTextureControl.xy)\n"
			"#define uTextureState1 vec4(vTextureControl.zw, vTextureAddress.zw)\n"
			"float window8(float a, float bh) {\n"
			" float ah = floor(a / 8.0);\n"
			" vec4 weight = vec4(1.0, 2.0, 4.0, 8.0);\n"
			" vec4 bits = mod(floor(ah / weight), 2.0) * mod(floor(bh / weight), 2.0);\n"
			" float bit16 = mod(floor(ah / 16.0), 2.0) * mod(floor(bh / 16.0), 2.0);\n"
			" return mod(a, 8.0) + 8.0 * (dot(bits, weight) + bit16 * 16.0);\n"
			"}\n"
			"vec4 colorAt(float x, float y) {\n"
			" vec2 uv = fract(vec2(x + 0.5, 1023.5 - y) / 1024.0);\n"
			" vec4 c = texture2D(uVram, uv);\n"
			" vec3 q = floor(c.rgb * 31.0 + 0.5);\n"
			" return vec4(q, c.a >= 0.5 ? 1.0 : 0.0);\n"
			"}\n"
			"float packColor(vec4 color) {\n"
			" return color.r + color.g * 32.0 + color.b * 1024.0 + color.a * 32768.0;\n"
			"}\n"
			"float wordAt(float x, float y) {\n"
			" return packColor(colorAt(x, y));\n"
			"}\n"
			"void textureWord(out vec4 textureColor) {\n"
			" float sourceU = floor(vTexture.x); float sourceV = floor(vTexture.y);\n"
			" float u = sourceU; float v = sourceV;\n"
			" if (uTextureState1.x < 30.5) u = window8(sourceU, uTextureState1.x);\n"
			" if (uTextureState1.y < 30.5) v = window8(sourceV, uTextureState1.y);\n"
			" float tx = uTextureState0.x; float ty = uTextureState0.y;\n"
			" float mode = uTextureState0.z; bool interleaved = uTextureState0.w != 0.0;\n"
			" float x = tx; float y = mod(ty + v, 1024.0); float data;\n"
			" if (mode < 0.5) {\n"
			"  if (interleaved) { x += mod(floor(u / 4.0), 4.0) + mod(v, 16.0) * 4.0; y = ty + (v - mod(v, 16.0)) + mod(floor(u / 16.0), 16.0); }\n"
			"  else x += floor(u / 4.0);\n"
			"  float nibble = mod(u, 4.0);\n"
			"  float divisor = ((562.5 * nibble - 1575.0) * nibble + 1027.5) * nibble + 1.0;\n"
			"  data = mod(floor(wordAt(x, y) / divisor), 16.0);\n"
			"  textureColor = colorAt(uTextureState1.z + data, uTextureState1.w); return;\n"
			" }\n"
			" if (mode < 1.5) {\n"
			"  if (interleaved) { x += mod(floor(u / 2.0), 8.0) + floor(mod(u, 32.0) / 16.0) * 64.0 + mod(v, 8.0) * 8.0; y = ty + (v - mod(v, 8.0)) + mod(floor(u / 32.0), 8.0); }\n"
			"  else x += floor(u / 2.0);\n"
			"  data = wordAt(x, y); data = mix(mod(data, 256.0), floor(data / 256.0), mod(u, 2.0));\n"
			"  textureColor = colorAt(uTextureState1.z + data, uTextureState1.w); return;\n"
			" }\n"
			" if (mode < 2.5) { textureColor = colorAt(tx + u, y); return; }\n"
			" textureColor = vec4(0.0);\n"
			"}\n"
			"void main() {\n"
			" bool textured = vStateBits.x >= 0.5; bool vramCopy = vStateBits.y >= 0.5; vec4 textureColor = vec4(0.0);\n"
			" if (dot(vStateBits, vec4(1.0)) < 0.5 && vStateControl.x < 0.5 && vStateControl.z < 0.5) { gl_FragColor = vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5) / 31.0, vStateControl.y); return; }\n"
			" if (vramCopy) textureColor = colorAt(vTextureAddress.x + floor(gl_FragCoord.x) - vTextureAddress.z, vTextureAddress.y + 1023.0 - floor(gl_FragCoord.y) - vTextureAddress.w);\n"
			" if (textured) textureWord(textureColor);\n"
			" if (textured && dot(textureColor, vec4(1.0)) < 0.5) discard;\n"
			" vec4 color = (textured || vramCopy) ? textureColor : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && vStateBits.z < 0.5) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = vStateBits.w >= 0.5 && (!textured || textureColor.a >= 0.5);\n"
			" vec4 backgroundColor = vec4(0.0);\n"
			" if (vStateControl.z >= 0.5 || blend) backgroundColor = colorAt(floor(gl_FragCoord.x), 1023.0 - floor(gl_FragCoord.y));\n"
			" if (vStateControl.z >= 0.5 && backgroundColor.a >= 0.5) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = backgroundColor.rgb; float abr = vStateControl.x;\n"
			"  if (abr < 0.5) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr < 1.5) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr < 2.5) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  color.a = textured ? 1.0 : 0.0;\n"
			" }\n"
			" if (vStateControl.y >= 0.5) color.a = 1.0;\n"
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
		rasterStateAttribute = getAttribLocation(rasterProgram, "aState");
		rasterTextureState0Attribute = getAttribLocation(rasterProgram,
			"aTextureState0");
		rasterTextureState1Attribute = getAttribLocation(rasterProgram,
			"aTextureState1");
		rasterSampleUniform = getUniformLocation(rasterProgram, "uVram");
		if (rasterPositionAttribute < 0 || rasterColorAttribute < 0 ||
			rasterTextureAttribute < 0 || rasterStateAttribute < 0 ||
			rasterTextureState0Attribute < 0 ||
			rasterTextureState1Attribute < 0 || rasterSampleUniform < 0) {
			rasterizerFailureReason = 4;
			DestroyRasterizerResources();
			return false;
		}
		useProgram(rasterProgram);
		uniform1i(rasterSampleUniform, 0);
		useProgram(program);

		glGenTextures(1, &rasterTexture);
		glBindTexture(GL_TEXTURE_2D, rasterTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1024,
			1024, 0, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		glGenTextures(1, &rasterSampleTexture);
		glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1024,
			1024, 0, GL_RGBA,
			GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
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
		enableVertexAttribArray(rasterPositionAttribute);
		enableVertexAttribArray(rasterColorAttribute);
		enableVertexAttribArray(rasterTextureAttribute);
		enableVertexAttribArray(rasterStateAttribute);
		enableVertexAttribArray(rasterTextureState0Attribute);
		enableVertexAttribArray(rasterTextureState1Attribute);
		bufferData(GL_ARRAY_BUFFER,
			sizeof(NamcosGlRasterDrawVertex) *
				NAMCOS_GL_RASTER_STREAM_VERTICES, NULL,
			GL_STREAM_DRAW);
		rasterVertexOffset = 0;
		rasterPendingCount = 0;
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
		vertexAttribPointer(rasterPositionAttribute, 2, GL_SHORT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, x));
		vertexAttribPointer(rasterColorAttribute, 3, GL_UNSIGNED_BYTE, GL_TRUE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, red));
		vertexAttribPointer(rasterTextureAttribute, 2, GL_UNSIGNED_BYTE, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, u));
		vertexAttribPointer(rasterStateAttribute, 1, GL_UNSIGNED_BYTE, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, padding));
		vertexAttribPointer(rasterTextureState0Attribute, 4, GL_SHORT,
			GL_FALSE, sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, textureState0));
		vertexAttribPointer(rasterTextureState1Attribute, 4, GL_UNSIGNED_BYTE,
			GL_FALSE, sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, textureControl0));
		rasterSampleTextureBound = false;
		rasterStateActive = true;
		return true;
	}

	bool FlushRasterVertices()
	{
		if (rasterPendingCount == 0) return true;
		if (rasterVertexOffset + rasterPendingCount >
			NAMCOS_GL_RASTER_STREAM_VERTICES) {
			bufferData(GL_ARRAY_BUFFER,
				sizeof(NamcosGlRasterDrawVertex) *
					NAMCOS_GL_RASTER_STREAM_VERTICES, NULL, GL_STREAM_DRAW);
			rasterVertexOffset = 0;
		}
		const ptrdiff_t vertexOffset = sizeof(NamcosGlRasterDrawVertex) *
			rasterVertexOffset;
		bufferSubData(GL_ARRAY_BUFFER, vertexOffset,
			sizeof(NamcosGlRasterDrawVertex) * rasterPendingCount,
			rasterPendingVertices);
		glDrawArrays(GL_TRIANGLES, rasterVertexOffset, rasterPendingCount);
		rasterVertexOffset += rasterPendingCount;
		rasterPendingCount = 0;
		if ((++rasterValidationCounter & 0xff) == 0 &&
			glGetError() != GL_NO_ERROR) return false;
		return true;
	}

	void EndRasterState(bool force = false)
	{
		if (!rasterStateActive && !force) return;
		FlushRasterVertices();
		glDisable(GL_SCISSOR_TEST);
		bindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, viewportWidth, viewportHeight);
		bindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		useProgram(program);
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterSampleTextureBound = false;
		rasterStateActive = false;
	}

	bool SynchronizeRasterSample(const NamcosGlRasterRect *selectedRects = NULL,
		INT32 selectedCount = 0, bool selectedCoversAllDirty = false)
	{
		if (!FlushRasterVertices()) return false;
		if (!rasterSampleTextureBound) {
			glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
			rasterSampleTextureBound = true;
		}
		const bool completeSync = !rasterSampleValid;
		if (completeSync) {
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
				1024, 1024);
			rasterSampleDirty.Reset();
		} else if (rasterSampleDirty.valid) {
			NamcosGlRasterRect rects[NAMCOS_GL_RASTER_DIRTY_RECTS];
			const bool selective = selectedRects != NULL && selectedCount > 0;
			const INT32 count = selective ? selectedCount :
				rasterSampleDirty.GetCopyRects(rects,
					NAMCOS_GL_RASTER_DIRTY_RECTS, 65536);
			for (INT32 i = 0; i < count; i++) {
				const NamcosGlRasterRect &rect = selective ? selectedRects[i] :
					rects[i];
				const INT32 width = rect.x2 - rect.x1 + 1;
				const INT32 height = rect.y2 - rect.y1 + 1;
				const INT32 glY = 1024 - rect.y2 - 1;
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0,
					rect.x1, glY, rect.x1, glY, width, height);
			}
			if (selective && !selectedCoversAllDirty) {
				for (INT32 i = 0; i < count; i++) {
					rasterSampleDirty.Exclude(selectedRects[i].x1,
						selectedRects[i].y1, selectedRects[i].x2,
						selectedRects[i].y2);
				}
			} else {
				rasterSampleDirty.Reset();
			}
		}
		if ((++rasterSampleValidationCounter & 0xff) == 0 &&
			glGetError() != GL_NO_ERROR) return false;
		rasterSampleValid = true;
		return true;
	}

	bool SubmitRasterPrimitive(const NamcosGlRasterPacket *packet,
		const NamcosGlRasterPrimitive *primitive)
	{
		if (packet == NULL || primitive == NULL) return false;
		// Small solid rectangles batch better than flushing around glClear.
		const bool largeOpaqueRectangle = primitive->type ==
			NAMCOS_GL_RASTER_FLAT_RECTANGLE && !primitive->semiTransparent &&
			!packet->state.checkStp && (INT64)primitive->width *
			primitive->height >= NAMCOS_GL_RASTER_FAST_CLEAR_PIXELS;
		const bool fastClear = (primitive->type == NAMCOS_GL_RASTER_FILL &&
			(INT64)primitive->width * primitive->height >=
				NAMCOS_GL_RASTER_FAST_CLEAR_PIXELS) || largeOpaqueRectangle;
		const bool textured = primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_POLYGON || primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_RECTANGLE;
		const bool fill = primitive->type == NAMCOS_GL_RASTER_FILL;
		const bool vramCopy = primitive->type == NAMCOS_GL_RASTER_VRAM_COPY;
		if (!rasterStateActive && wglGetCurrentContext() != context &&
			!wglMakeCurrent(dc, context))
			return false;

		INT32 x1, y1, x2, y2;
		if (!NamcosGlRasterGetDrawBounds(packet, primitive,
			&x1, &y1, &x2, &y2)) return false;

		if (!BeginRasterState()) return false;
		INT32 scissorX1 = x1;
		INT32 scissorY1 = y1;
		INT32 scissorX2 = x2;
		INT32 scissorY2 = y2;
		if (!fastClear && primitive->type == NAMCOS_GL_RASTER_FILL) {
			scissorX1 = 0;
			scissorY1 = 0;
			scissorX2 = 1023;
			scissorY2 = 1023;
		} else if (!fastClear && !vramCopy) {
			scissorX1 = (INT32)packet->state.drawX1;
			scissorY1 = (INT32)packet->state.drawY1;
			scissorX2 = (INT32)packet->state.drawX2;
			scissorY2 = (INT32)packet->state.drawY2;
			if (scissorX1 < 0) scissorX1 = 0;
			if (scissorY1 < 0) scissorY1 = 0;
			if (scissorX2 > 1023) scissorX2 = 1023;
			if (scissorY2 > 1023) scissorY2 = 1023;
		}
		const INT32 scissorY = 1024 - scissorY2 - 1;
		const INT32 scissorWidth = scissorX2 - scissorX1 + 1;
		const INT32 scissorHeight = scissorY2 - scissorY1 + 1;
		if (!rasterScissorValid || lastRasterScissorX != scissorX1 ||
			lastRasterScissorY != scissorY ||
			lastRasterScissorWidth != scissorWidth ||
			lastRasterScissorHeight != scissorHeight) {
			if (!FlushRasterVertices()) return false;
			glScissor(scissorX1, scissorY, scissorWidth, scissorHeight);
			lastRasterScissorX = scissorX1;
			lastRasterScissorY = scissorY;
			lastRasterScissorWidth = scissorWidth;
			lastRasterScissorHeight = scissorHeight;
			rasterScissorValid = true;
		}
		if (fastClear) {
			if (!FlushRasterVertices()) return false;
			const UINT32 red = (primitive->vertex[0].red * 31 + 127) / 255;
			const UINT32 green = (primitive->vertex[0].green * 31 + 127) / 255;
			const UINT32 blue = (primitive->vertex[0].blue * 31 + 127) / 255;
			const UINT32 alpha = primitive->type != NAMCOS_GL_RASTER_FILL &&
				packet->state.drawStp ? 1 : 0;
			const UINT32 clearColor = red | (green << 5) | (blue << 10) |
				(alpha << 15);
			if (!rasterClearColorValid || lastRasterClearColor != clearColor) {
				glClearColor((GLfloat)red / 31.0f, (GLfloat)green / 31.0f,
					(GLfloat)blue / 31.0f, (GLfloat)alpha);
				lastRasterClearColor = clearColor;
				rasterClearColorValid = true;
			}
			glClear(GL_COLOR_BUFFER_BIT);
			if ((++rasterValidationCounter & 0xff) == 0 &&
				glGetError() != GL_NO_ERROR) {
				EndRasterState();
				return false;
			}
			rasterDirty.Include(x1, y1, x2, y2);
			if (rasterSampleValid) rasterSampleDirty.Include(x1, y1, x2, y2);
			outputFrameValid = false;
			return true;
		}
		UINT32 textureState[8];
		if (vramCopy) {
			textureState[0] = primitive->sourceX;
			textureState[1] = primitive->sourceY;
			textureState[2] = primitive->vertex[0].x;
			textureState[3] = primitive->vertex[0].y;
			memset(textureState + 4, 0, sizeof(UINT32) * 4);
		} else if (textured) {
			NamcosGlRasterGetTextureState(packet, primitive, textureState);
		}
		const bool checkStp = !fill && packet->state.checkStp != 0;
		if (textured || vramCopy || primitive->semiTransparent || checkStp) {
			NamcosGlRasterRect selected[NAMCOS_GL_RASTER_DIRTY_RECTS];
			const bool selectiveDestination = !textured && !vramCopy &&
				(primitive->semiTransparent || checkStp);
			const bool selectiveVramCopy = vramCopy &&
				!primitive->semiTransparent && !checkStp;
			const bool selectiveTexture = textured &&
				!primitive->semiTransparent && !checkStp;
			INT32 selectedCount = 0;
			bool selectedCoversAllDirty = false;
			bool synchronizeSample = !rasterSampleValid;
			if (!synchronizeSample && selectiveTexture) {
				bool dependencyKnown = false;
				selectedCount = NamcosGlRasterBuildTextureSelectiveCopyRects(
					textureState, primitive, &rasterSampleDirty, selected,
					NAMCOS_GL_RASTER_DIRTY_RECTS, &selectedCoversAllDirty,
					&dependencyKnown);
				synchronizeSample = dependencyKnown ? selectedCount > 0 :
					NamcosGlRasterTextureStateReadsDirty(textureState,
						&rasterSampleDirty);
			} else if (!synchronizeSample &&
				(selectiveDestination || selectiveVramCopy)) {
				const INT32 selectedX = selectiveVramCopy ?
					primitive->sourceX : x1;
				const INT32 selectedY = selectiveVramCopy ?
					primitive->sourceY : y1;
				const INT32 selectedWidth = selectiveVramCopy ?
					primitive->width : x2 - x1 + 1;
				const INT32 selectedHeight = selectiveVramCopy ?
					primitive->height : y2 - y1 + 1;
				selectedCount = NamcosGlRasterBuildSelectiveCopyRects(
					&rasterSampleDirty, selectedX, selectedY, selectedWidth,
					selectedHeight, selected, NAMCOS_GL_RASTER_DIRTY_RECTS,
					&selectedCoversAllDirty);
				synchronizeSample = selectedCount > 0;
			} else if (!synchronizeSample &&
				(primitive->semiTransparent || checkStp)) {
				synchronizeSample = rasterSampleDirty.Intersects(x1, y1, x2, y2);
			}
			if (!synchronizeSample && vramCopy) {
				synchronizeSample = rasterSampleDirty.Intersects(
					primitive->sourceX, primitive->sourceY,
					primitive->sourceX + primitive->width - 1,
					primitive->sourceY + primitive->height - 1);
			}
			if (!synchronizeSample && textured && !selectiveTexture) {
				synchronizeSample = NamcosGlRasterTexturePrimitiveReadsDirty(
					textureState, primitive, &rasterSampleDirty);
			}
			if (synchronizeSample) {
				if (!SynchronizeRasterSample(selectedCount > 0 ? selected : NULL,
					selectedCount, selectedCoversAllDirty)) return false;
			} else if (!rasterSampleTextureBound) {
				glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
				rasterSampleTextureBound = true;
			}
		}
		const bool rawTexture = textured && primitive->rawTexture;
		const UINT32 blendTpage = textured ? primitive->tpage : packet->state.tpage;
		const UINT32 abr = primitive->semiTransparent ? (blendTpage >>
			(packet->state.gpuType == 2 ? 5 : 7)) & 3 : 0;
		const bool drawStp = !fill && packet->state.drawStp != 0;
		const bool maskCheck = checkStp;
		if (rasterPendingCount + 6 > NAMCOS_GL_RASTER_BATCH_VERTICES &&
			!FlushRasterVertices()) return false;
		const UINT32 count = textured ? NamcosGlRasterBuildTexturedTriangles(
			primitive, rasterPendingVertices + rasterPendingCount, 6) :
			NamcosGlRasterBuildColorTriangles(primitive,
				rasterPendingVertices + rasterPendingCount, 6);
		if (count == 0) {
			return primitive->type == NAMCOS_GL_RASTER_FLAT_POLYGON ||
				primitive->type == NAMCOS_GL_RASTER_GOURAUD_POLYGON ||
				primitive->type == NAMCOS_GL_RASTER_TEXTURED_POLYGON;
		}
		const UINT8 vertexState = (textured ? 0x01 : 0) |
			(vramCopy ? 0x02 : 0) | (rawTexture ? 0x04 : 0) |
			(primitive->semiTransparent ? 0x08 : 0) | ((abr & 3) << 4) |
			(drawStp ? 0x40 : 0) | (maskCheck ? 0x80 : 0);
		NamcosGlRasterSetVertexStates(
			rasterPendingVertices + rasterPendingCount, count,
			vertexState, textureState, textured, vramCopy);
		rasterPendingCount += count;
		// A VRAM copy can feed a later transfer through wrapped coordinates that
		// are not represented by one dirty rectangle.  Preserve command order.
		if (vramCopy && !FlushRasterVertices()) return false;
		rasterDirty.Include(x1, y1, x2, y2);
		if (rasterSampleValid) rasterSampleDirty.Include(x1, y1, x2, y2);
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
			rasterTransferPixels = (UINT8 *)malloc((size_t)1024 * 1024 * 2);
			if (rasterTransferPixels == NULL) return false;
		}
		SetUnpackAlignment(2);
		SetUnpackRowLength(0);
		NamcosGlRasterUploadSpan spans[NAMCOS_GL_RASTER_UPLOAD_SPANS];
		INT32 spanCount = 1;
		if (!rasterUploadTracker.valid || rowGeneration == NULL) {
			spans[0].firstRow = 0;
			spans[0].rowCount = 1024;
		} else {
			INT32 first;
			INT32 rows;
			spanCount = NamcosGlRasterBuildUploadSpans(
				&rasterUploadTracker, rowGeneration, spans,
				NAMCOS_GL_RASTER_UPLOAD_SPANS, &first, &rows, 32);
			if (spanCount < 0) {
				spanCount = 1;
				spans[0].firstRow = first;
				spans[0].rowCount = rows;
			}
		}
		if (spanCount == 0) {
			rasterVramSynchronized = true;
			return true;
		}
		const bool validateUpload =
			(rasterUploadValidationCounter++ & 0x3f) == 0;
		if (validateUpload) glGetError();
		if (spanCount > 0) {
			glBindTexture(GL_TEXTURE_2D, rasterTexture);
			for (INT32 span = 0; span < spanCount; span++) {
				const INT32 first = spans[span].firstRow;
				const INT32 rows = spans[span].rowCount;
				NamcosGlCopyVram16RangeParallel(vram,
					(UINT16 *)rasterTransferPixels, first, rows, threadPool);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0,
					1024 - first - rows, 1024, rows, GL_RGBA,
					GL_UNSIGNED_SHORT_1_5_5_5_REV,
					rasterTransferPixels);
			}
			bindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
			glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
			for (INT32 span = 0; span < spanCount; span++) {
				const INT32 first = spans[span].firstRow;
				const INT32 rows = spans[span].rowCount;
				const INT32 textureY = 1024 - first - rows;
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, textureY, 0,
					textureY, 1024, rows);
			}
			rasterSampleTextureBound = false;
		}
		const bool uploaded = !validateUpload || glGetError() == GL_NO_ERROR;
		if (uploaded) {
			if (rowGeneration == NULL) {
				rasterUploadTracker.Reset();
			} else {
				for (INT32 span = 0; span < spanCount; span++) {
					rasterUploadTracker.RememberRange(rowGeneration,
						spans[span].firstRow, spans[span].rowCount);
				}
			}
		}
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterVramSynchronized = uploaded;
		if (uploaded) {
			rasterDirty.Reset();
			rasterSampleValid = true;
			rasterSampleDirty.Reset();
		}
		return uploaded;
	}

	bool ReadbackRasterVram(UINT16 *vram)
	{
		if (vram == NULL || rasterTransferPixels == NULL) return false;
		if (wglGetCurrentContext() != context && !wglMakeCurrent(dc, context))
			return false;
		if (!rasterDirty.valid) return true;
		NamcosGlRasterRect rects[NAMCOS_GL_RASTER_DIRTY_RECTS];
		const INT32 rectCount = rasterDirty.GetReadbackRects(rects,
			NAMCOS_GL_RASTER_DIRTY_RECTS, 65536);
		bool read = rectCount > 0;
		bindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		SetPackAlignment(2);
		SetPackRowLength(0);
		const bool validateReadback =
			(rasterReadbackValidationCounter++ & 0x3f) == 0;
		if (validateReadback) glGetError();
		for (INT32 i = 0; i < rectCount; i++) {
			const INT32 width = rects[i].x2 - rects[i].x1 + 1;
			const INT32 height = rects[i].y2 - rects[i].y1 + 1;
			glReadPixels(rects[i].x1, 1024 - rects[i].y2 - 1,
				width, height, GL_RGBA, GL_UNSIGNED_BYTE, rasterTransferPixels);
			NamcosGlUnpackVramRectParallel(rasterTransferPixels, vram,
				rects[i].x1, rects[i].y1, width, height, rasterThreadPool);
		}
		read = !validateReadback || glGetError() == GL_NO_ERROR;
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
			NamcosPackRgb24(frame, rgbUploadCache);
			rgbCacheWidth = width;
			rgbCacheHeight = rows;
			rgbCacheDisplayX = frame->displayX;
			rgbCacheDisplayY = frame->displayY;
			if (frame->vramRowGeneration != NULL) {
				NamcosFrameRememberRows(rgbRowGeneration,
					frame->vramRowGeneration, frame->displayY, 0, rows);
			} else {
				memset(rgbRowGeneration, 0, (size_t)rows * sizeof(UINT64));
			}
			rgbCacheValid = true;
			rgbDenseFrames = 0;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, rows,
				GL_RGB, GL_UNSIGNED_BYTE, rgbUploadCache);
			return;
		}
		if (frame->vramRowGeneration != NULL &&
			NamcosFrameRowsMatch(rgbRowGeneration, frame->vramRowGeneration,
				frame->displayY, observedFirst, observedRows)) {
			rgbDenseFrames = 0;
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
				NamcosPackRgb24Selected(frame, rgbUploadCache, denseRows,
					denseChangedRows, observedFirst, observedRows);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width,
					observedRows, GL_RGB, GL_UNSIGNED_BYTE,
					rgbUploadCache + (size_t)observedFirst * rowBytes);
				if (frame->vramRowGeneration != NULL) {
					NamcosFrameRememberRows(rgbRowGeneration,
						frame->vramRowGeneration, frame->displayY,
						observedFirst, observedRows);
				} else {
					memset(rgbRowGeneration + observedFirst, 0,
						(size_t)observedRows * sizeof(UINT64));
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
		memset(dirtyRows, 0, rows);
		const INT32 observedEnd = observedFirst + observedRows;
		for (INT32 y = observedFirst; y < observedEnd; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool candidate = frame->vramRowGeneration == NULL ||
				rgbRowGeneration[y] != generation;
			dirtyRows[y] = candidate ? 1 : 0;
			if (candidate) candidateRows++;
		}
		if (candidateRows == 0) return;
		NamcosPackRgb24Selected(frame, rgbUploadPixels, dirtyRows, candidateRows,
			observedFirst, observedRows);
		for (INT32 y = observedFirst; y < observedEnd; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool candidate = dirtyRows[y] != 0;
			const bool changed = candidate &&
				memcmp(rgbUploadCache + (size_t)y * rowBytes,
					rgbUploadPixels + (size_t)y * rowBytes, rowBytes) != 0;
			if (candidate) rgbRowGeneration[y] = generation;
			dirtyRows[y] = changed ? 1 : 0;
			if (changed) {
				memcpy(rgbUploadCache + (size_t)y * rowBytes,
					rgbUploadPixels + (size_t)y * rowBytes, rowBytes);
				changedRows++;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= observedRows) {
			rgbDenseFrames = 7;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width, observedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)observedFirst * rowBytes);
			return;
		}
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, mergedStart, width, mergedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)mergedStart * rowBytes);
			return;
		}
		NamcosFrameRowSpan spans[8];
		const INT32 spanCount = NamcosFrameBuildUploadSpans(dirtyRows, rows,
			rowBytes, spans, 8);
		for (INT32 i = 0; i < spanCount; i++) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, spans[i].start, width,
				spans[i].count, GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)spans[i].start * rowBytes);
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
			slot->width == width && slot->rows == rows &&
			(rowGeneration != NULL || slot->pixelCacheValid);

		// Row generations are authoritative for the normal PSX VRAM path.  Avoid
		// duplicating the source into a CPU cache just to compare it again.
		if (slot != NULL && rowGeneration != NULL) {
			if (!sameGeometry) {
				NamcosFrameRememberUploadRows(slot->rowGeneration, rowGeneration,
					destinationY, rows);
				slot->destinationX = destinationX;
				slot->destinationY = destinationY;
				slot->width = width;
				slot->rows = rows;
				slot->denseFrames = 0;
				slot->pixelCacheValid = false;
				slot->valid = true;
				glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
					width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
				return;
			}
			if (NamcosFrameRowsMatch(slot->rowGeneration, rowGeneration,
				destinationY, 0, rows)) return;
			UINT8 dirtyRows[1024];
			INT32 changedRows = 0;
			INT32 runCount = 0;
			bool inRun = false;
			for (INT32 y = 0; y < rows; y++) {
				const UINT64 generation =
					rowGeneration[(destinationY + y) & 0x3ff];
				const bool changed = slot->rowGeneration[y] != generation;
				slot->rowGeneration[y] = generation;
				dirtyRows[y] = changed ? 1 : 0;
				if (changed) {
					changedRows++;
					if (!inRun) runCount++;
				}
				inRun = changed;
			}
			slot->pixelCacheValid = false;
			if (changedRows == 0) return;
			if (changedRows * 3 >= rows) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
					width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
				return;
			}

			INT32 mergedStart;
			INT32 mergedRows;
			if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows,
				runCount, &mergedStart, &mergedRows)) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX,
					destinationY + mergedStart, width, mergedRows, GL_RGBA,
					GL_UNSIGNED_SHORT_1_5_5_5_REV, source + mergedStart * 1024);
				return;
			}
			NamcosFrameRowSpan spans[8];
			const INT32 spanCount = NamcosFrameBuildUploadSpans(dirtyRows, rows,
				(size_t)width * sizeof(UINT16), spans, 8);
			for (INT32 i = 0; i < spanCount; i++) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX,
					destinationY + spans[i].start, width, spans[i].count, GL_RGBA,
					GL_UNSIGNED_SHORT_1_5_5_5_REV,
					source + spans[i].start * 1024);
			}
			return;
		}

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
				NamcosFrameRememberUploadRows(slot->rowGeneration, rowGeneration,
					destinationY, rows);
				slot->destinationX = destinationX;
				slot->destinationY = destinationY;
				slot->width = width;
				slot->rows = rows;
				slot->denseFrames = 0;
				slot->pixelCacheValid = true;
				slot->valid = true;
			}
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
				width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
			return;
		}
		if (slot->denseFrames != 0) {
			UINT8 denseRows[1024];
			INT32 denseChangedRows = rows;
			INT32 denseFirstRow = 0;
			INT32 denseLastRow = rows - 1;
			if (rowGeneration != NULL) {
				denseChangedRows = 0;
				denseFirstRow = rows;
				denseLastRow = -1;
				for (INT32 y = 0; y < rows; y++) {
					const UINT64 generation =
						rowGeneration[(destinationY + y) & 0x3ff];
					denseRows[y] = slot->rowGeneration[y] != generation ? 1 : 0;
					if (denseRows[y]) {
						denseChangedRows++;
						if (denseFirstRow == rows) denseFirstRow = y;
						denseLastRow = y;
					}
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
						denseRows, denseChangedRows, threadPool, denseFirstRow,
						denseLastRow - denseFirstRow + 1);
					NamcosFrameRememberUploadRows(slot->rowGeneration,
						rowGeneration, destinationY, rows);
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
		INT32 firstChangedRow = rows;
		INT32 lastChangedRow = -1;
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
				if (firstChangedRow == rows) firstChangedRow = y;
				lastChangedRow = y;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= rows) {
			slot->denseFrames = 7;
			NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
				dirtyRows, changedRows, threadPool, firstChangedRow,
				lastChangedRow - firstChangedRow + 1);
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY,
				width, rows, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, source);
			return;
		}
		NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
			dirtyRows, changedRows, threadPool, firstChangedRow,
			lastChangedRow - firstChangedRow + 1);
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX,
				destinationY + mergedStart, width, mergedRows, GL_RGBA,
				GL_UNSIGNED_SHORT_1_5_5_5_REV, source + mergedStart * 1024);
			return;
		}
		NamcosFrameRowSpan spans[8];
		const INT32 spanCount = NamcosFrameBuildUploadSpans(dirtyRows, rows,
			(size_t)width * sizeof(UINT16), spans, 8);
		for (INT32 i = 0; i < spanCount; i++) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX,
				destinationY + spans[i].start, width, spans[i].count, GL_RGBA,
				GL_UNSIGNED_SHORT_1_5_5_5_REV,
				source + spans[i].start * 1024);
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

		if (maximumTextureSize < 1024 || !VersionAtLeast(version, 2, 0) ||
			!NamcosOpenGLRendererIsHardware(renderer)) {
			Disable();
			return false;
		}
		if (!CreateShaderProgram()) {
			Disable();
			return false;
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
		rasterizerFailureReason = 102;
		fullRasterizerCapable = CreateRasterizerResources();
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_DITHER);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
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
	NamcosGlRasterDrawVertex rasterPendingVertices[NAMCOS_GL_RASTER_BATCH_VERTICES];
	UINT32 rasterPendingCount;
	UINT32 rasterValidationCounter;
	UINT32 rasterSampleValidationCounter;
	UINT32 rasterUploadValidationCounter;
	UINT32 rasterReadbackValidationCounter;
	UINT32 readbackValidationCounter;
	GLenum validatedReadFormat;
	GLenum validatedReadType;
	INT32 validatedReadPackAlignment;
	INT32 validatedReadPackRowLength;
	UINT32 lastRasterClearColor;
	bool rasterClearColorValid;
	GLint rasterPositionAttribute;
	GLint rasterColorAttribute;
	GLint rasterTextureAttribute;
	GLint rasterStateAttribute;
	GLint rasterTextureState0Attribute;
	GLint rasterTextureState1Attribute;
	GLint rasterSampleUniform;
	bool rasterSampleTextureBound;
	bool rasterScissorValid;
	INT32 lastRasterScissorX;
	INT32 lastRasterScissorY;
	INT32 lastRasterScissorWidth;
	INT32 lastRasterScissorHeight;
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
	bool lastVerticalReconstruct;
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
	INT32 lastPackAlignment;
	INT32 lastPackRowLength;
	INT32 lastUnpackAlignment;
	INT32 lastUnpackRowLength;
	UploadSlot uploadSlots[4];
	INT32 uploadSlotIndex;
	bool uploadModeValid;
	bool lastUploadRgb24;
	NamcosFrameUploadKey uploadFrameKey;
	UINT64 uploadRowGeneration[1024];
	bool uploadFrameValid;
	NamcosFrameOutputKey outputFrameKey;
	UINT64 outputRowGeneration[1024];
	bool outputFrameValid;
	bool fullRasterizerCapable;
	INT32 rasterizerFailureReason;
	bool rasterVramSynchronized;
	bool rasterSampleValid;
	NamcosGlRasterUploadTracker rasterUploadTracker;
	NamcosPolyThreadPool *rasterThreadPool;
	NamcosGlRasterDirtyBounds rasterDirty;
	NamcosGlRasterDirtyBounds rasterSampleDirty;
	bool rasterStateActive;
	bool initialized;
	bool available;
	bool failed;
};

#elif defined(FBNEO_NAMCOS_OPENGL_ES2)

#include <EGL/egl.h>
#include <GLES2/gl2.h>

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
#ifndef GL_PACK_ROW_LENGTH
#define GL_PACK_ROW_LENGTH 0x0d02
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
		  rgbTexture(0),
		  vertexBuffer(0),
		  rasterTexture(0),
		  rasterSampleTexture(0),
		  rasterFramebuffer(0),
		  rasterProgram(0),
		  rasterVertexBuffer(0),
		  rasterVertexOffset(0),
		  rasterPendingCount(0),
		  rasterValidationCounter(0),
		  rasterSampleValidationCounter(0),
		  rasterUploadValidationCounter(0),
		  rasterReadbackValidationCounter(0),
		  readbackValidationCounter(0),
		  validatedReadFormat(0),
		  validatedReadType(0),
		  validatedReadPackAlignment(-1),
		  validatedReadPackRowLength(-1),
		  lastRasterClearColor(0),
		  rasterClearColorValid(false),
		  rasterPositionAttribute(-1),
		  rasterColorAttribute(-1),
		  rasterTextureAttribute(-1),
		  rasterStateAttribute(-1),
		  rasterTextureState0Attribute(-1),
		  rasterTextureState1Attribute(-1),
		  rasterSampleUniform(-1),
		  rasterSampleTextureBound(false),
		  rasterScissorValid(false),
		  lastRasterScissorX(0),
		  lastRasterScissorY(0),
		  lastRasterScissorWidth(0),
		  lastRasterScissorHeight(0),
		  activeShaderIndex(0),
		  lastOutputLeft(0.0f),
		  lastOutputRight(0.0f),
		  lastVertical(false),
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
		  lastPackAlignment(-1),
		  lastPackRowLength(-1),
		  lastUnpackAlignment(-1),
		  lastUnpackRowLength(-1),
		  surfacePixelBytes(0),
		  nativeReadFormat(GL_RGBA),
		  nativeReadType(GL_UNSIGNED_BYTE),
		  readRgb565Supported(false),
		  readRgb888Supported(false),
		  readBgraSupported(false),
		  packSubimage(false),
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
		  rasterSampleValid(false),
		  rasterRead5551(false),
		  rasterThreadPool(NULL),
		  rasterStateActive(false),
		  initialized(false),
		  available(false),
		  failed(false)
	{
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
			lastVerticalReconstruct[i] = false;
		}
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
		return available && fullRasterizerCapable;
	}

	INT32 RasterizerFailureReason() const
	{
		return fullRasterizerCapable ? 0 : -1;
	}

	bool SupportsOpenGL2() const
	{
		return available;
	}

	bool SupportsShaderMode() const
	{
		return SupportsOpenGL2();
	}

	bool HasHardwareVram() const
	{
		return rasterVramSynchronized;
	}

	bool UploadVramRect(UINT16 *vram, UINT64 generation,
		UINT64 *rowGeneration, INT32 x, INT32 y, INT32 width, INT32 height)
	{
		if (!rasterVramSynchronized || vram == NULL || width <= 0 ||
			height <= 0 || width > 1024 || height > 1024) return false;
		const INT32 startX = x & 0x3ff;
		const INT32 startY = y & 0x3ff;
		const INT32 firstWidth = width < 1024 - startX ? width : 1024 - startX;
		const INT32 firstHeight = height < 1024 - startY ? height : 1024 - startY;

		bool uploaded = false;
		if (rasterTransferPixels != NULL &&
			(rasterStateActive || eglGetCurrentContext() == context ||
				eglMakeCurrent(display, surface, surface, context) == EGL_TRUE) &&
			(!rasterStateActive || FlushEsRasterVertices())) {
			SetUnpackAlignment(2);
			SetUnpackRowLength(0);
			const INT32 widths[2] = { firstWidth, width - firstWidth };
			const INT32 heights[2] = { firstHeight, height - firstHeight };
			const INT32 xs[2] = { startX, 0 };
			const INT32 ys[2] = { startY, 0 };

			const bool validateUpload =
				(rasterUploadValidationCounter++ & 0x3f) == 0;
			if (validateUpload) glGetError();
			glBindTexture(GL_TEXTURE_2D, rasterTexture);
			uploaded = true;
			for (INT32 yPart = 0; yPart < 2; yPart++) {
				for (INT32 xPart = 0; xPart < 2; xPart++) {
					const INT32 partWidth = widths[xPart];
					const INT32 partHeight = heights[yPart];
					if (partWidth <= 0 || partHeight <= 0) continue;
					NamcosGlPackVram5551RectParallel(vram,
						(UINT16 *)rasterTransferPixels, xs[xPart], ys[yPart],
						partWidth, partHeight, rasterThreadPool);
					glTexSubImage2D(GL_TEXTURE_2D, 0, xs[xPart],
						1024 - ys[yPart] - partHeight,
						partWidth, partHeight,
						GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1,
						rasterTransferPixels);
				}
			}
			uploaded = !validateUpload || glGetError() == GL_NO_ERROR;
			if (uploaded) {
				for (INT32 yPart = 0; yPart < 2; yPart++) {
					for (INT32 xPart = 0; xPart < 2; xPart++) {
						const INT32 partWidth = widths[xPart];
						const INT32 partHeight = heights[yPart];
						if (partWidth <= 0 || partHeight <= 0) continue;
						// CPU and GPU contain the same uploaded pixels.  Remove an
						// older GPU-only overlap instead of reading it back later.
						rasterDirty.Exclude(xs[xPart], ys[yPart],
							xs[xPart] + partWidth - 1,
							ys[yPart] + partHeight - 1);
						if (rasterSampleValid) {
							rasterSampleDirty.Include(xs[xPart], ys[yPart],
								xs[xPart] + partWidth - 1,
								ys[yPart] + partHeight - 1);
						}
					}
				}
			}
			if (!rasterStateActive) glBindTexture(GL_TEXTURE_2D, texture);
			rasterSampleTextureBound = false;
			outputFrameValid = false;
		}

		if (uploaded) {
			if (rowGeneration != NULL) {
				rasterUploadTracker.SetAndRememberRange(rowGeneration, generation,
					startY, firstHeight);
				if (height > firstHeight) {
					rasterUploadTracker.SetAndRememberRange(rowGeneration, generation,
						0, height - firstHeight);
				}
			}
			return true;
		}

		UINT16 *backup = (UINT16 *)malloc((size_t)width * height * sizeof(UINT16));
		if (backup == NULL) return false;
		NamcosGlCopyWrappedVramToLinear(vram, backup, x, y, width, height);
		SynchronizeVram(vram, generation, rowGeneration);
		NamcosGlCopyLinearToWrappedVram(backup, vram, x, y, width, height);
		if (rowGeneration != NULL) {
			for (INT32 row = 0; row < height; row++) {
				rowGeneration[(y + row) & 0x3ff] = generation;
			}
		}
		free(backup);
		return false;
	}

	bool ReadVramRect(UINT16 *vram, UINT64 generation,
		UINT64 *rowGeneration, INT32 x, INT32 y, INT32 width, INT32 height)
	{
		if (!rasterVramSynchronized || vram == NULL || width <= 0 ||
			height <= 0 || width > 1024 || height > 1024) return false;
		if (!NamcosGlRasterDirtyIntersectsWrapped(&rasterDirty,
			x, y, width, height)) return true;
		const bool preserveRasterState = rasterStateActive;
		if (rasterTransferPixels == NULL ||
			(!rasterStateActive && eglGetCurrentContext() != context &&
				eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) ||
			!FlushEsRasterVertices()) {
			return SynchronizeVram(vram, generation, rowGeneration);
		}

		NamcosGlRasterRect readRects[64];
		const INT32 readCount = NamcosGlRasterBuildWrappedReadRects(&rasterDirty,
			x, y, width, height, readRects, 64, 65536);
		if (readCount <= 0) return true;
		bool read = true;

		glBindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		SetPackAlignment(rasterRead5551 ? 2 : 1);
		SetPackRowLength(0);
		const bool validateReadback =
			(rasterReadbackValidationCounter++ & 0x3f) == 0;
		if (validateReadback) glGetError();
		for (INT32 i = 0; i < readCount; i++) {
			const INT32 partWidth = readRects[i].x2 - readRects[i].x1 + 1;
			const INT32 partHeight = readRects[i].y2 - readRects[i].y1 + 1;
			glReadPixels(readRects[i].x1,
				1024 - readRects[i].y2 - 1,
				partWidth, partHeight, GL_RGBA,
				rasterRead5551 ?
				GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_BYTE,
				rasterTransferPixels);
			if (rasterRead5551) {
				NamcosGlReadVram5551RectParallel((UINT16 *)rasterTransferPixels,
					vram, readRects[i].x1, readRects[i].y1, partWidth, partHeight,
					rasterThreadPool);
			} else {
				NamcosGlUnpackVramRectParallel(rasterTransferPixels, vram,
					readRects[i].x1, readRects[i].y1, partWidth, partHeight,
					rasterThreadPool);
			}
		}
		read = !validateReadback || glGetError() == GL_NO_ERROR;
		if (!preserveRasterState) EndEsRasterState(true);
		if (read) {
			for (INT32 i = 0; i < readCount; i++) {
				rasterDirty.Exclude(readRects[i].x1, readRects[i].y1,
					readRects[i].x2, readRects[i].y2);
			}
			return true;
		}
		return SynchronizeVram(vram, generation, rowGeneration);
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
		// Read the completed framebuffer directly.  UploadEs3RasterVram()
		// refreshes both raster textures if hardware rasterization resumes.
		if (dirty && !FlushEsRasterVertices()) return false;
		if (!ReadbackEs3RasterVram(vram)) return false;
		// CPU fallback commands may immediately modify the read-back rows.  Do
		// not retain upload generations across this ownership transition or a
		// following hardware packet can reuse stale texture rows.
		(void)generation;
		(void)rowGeneration;
		rasterUploadTracker.Reset();
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
		void *outputDestination = directDestination != NULL ?
			directDestination : (void *)frame->output;
		const INT32 outputPitch = directDestination != NULL ?
			directPitch : frame->outputWidth * 2;
		const INT32 outputBytes = directDestination != NULL ? directBytes : 2;
		const UINT32 *outputPalette = directDestination != NULL ? palette : NULL;
		NamcosFrameObservedRows observedRows = { 0, 0, false };
		if (outputFrameValid && NamcosFrameOutputMatches(&outputFrameKey, frame,
			outputDestination, outputPitch, outputBytes, outputPalette,
			outputRowGeneration, &observedRows)) {
			return true;
		}
		INT32 readY = 0;
		INT32 readHeight = frame->outputHeight;
		UINT8 readDirtyRows[1024];
		bool partialReadback = outputFrameValid &&
			NamcosFrameGetPartialReadback(&outputFrameKey, frame,
				outputDestination, outputPitch, outputBytes, outputPalette,
				outputRowGeneration, &readY, &readHeight, readDirtyRows);
		NamcosFrameRowSpan readSpans[4];
		INT32 readSpanCount = partialReadback ?
			NamcosFrameBuildUploadSpans(readDirtyRows, frame->outputHeight,
				(size_t)frame->outputWidth * outputBytes, readSpans, 4, 65536) : 0;
		if (partialReadback && !NamcosFrameReadbackSpansWorthwhile(readSpans,
			readSpanCount, frame->outputHeight,
			(size_t)frame->outputWidth * outputBytes, 65536)) {
			partialReadback = false;
			readY = 0;
			readHeight = frame->outputHeight;
			readSpanCount = 0;
		}

		if (!EnsureInitialized(frame->outputWidth, frame->outputHeight,
			directBytes)) {
			return false;
		}

		// The private ES context is only ever bound to this pbuffer surface.
		// Avoid two eglGetCurrentSurface calls in the per-frame path.
		if (eglGetCurrentContext() != context &&
			!eglMakeCurrent(display, surface, surface, context)) {
			Disable();
			return false;
		}
		EndEsRasterState();
		if (viewportWidth != frame->outputWidth ||
			viewportHeight != frame->outputHeight) {
			glViewport(0, 0, frame->outputWidth, frame->outputHeight);
			viewportWidth = frame->outputWidth;
			viewportHeight = frame->outputHeight;
		}

		const bool rgb24 = frame->rgb24 != 0;
		if (!uploadModeValid || lastUploadRgb24 != rgb24) {
			uploadFrameValid = false;
			outputFrameValid = false;
			uploadModeValid = true;
			lastUploadRgb24 = rgb24;
			glBindTexture(GL_TEXTURE_2D, rgb24 ? rgbTexture : texture);
			activeShaderIndex = rgb24 ? 1 : 0;
			glUseProgram(programs[activeShaderIndex]);
			if (glGetError() != GL_NO_ERROR) {
				Disable();
				return false;
			}
		}
		const bool rasterSource = rasterVramSynchronized && !rgb24;
		if (!rasterSource && (!uploadFrameValid ||
			!NamcosFrameUploadMatches(&uploadFrameKey, frame,
				uploadRowGeneration, &observedRows))) {
			if (rgb24) {
				SetUnpackAlignment(1);
				SetUnpackRowLength(0);
				UploadRgb24(frame);
			} else {
				SetUnpackAlignment(2);
				uploadSlotIndex = 0;
				INT32 uploadFirstRow;
				INT32 uploadRows;
				NamcosFrameGetObservedRowsCached(frame, &observedRows,
					&uploadFirstRow, &uploadRows);
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
			NamcosFrameRememberUpload(&uploadFrameKey, frame,
				uploadRowGeneration, &observedRows);
			uploadFrameValid = true;
		}

		if (frame->outputShiftX != 0) {
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			rasterClearColorValid = false;
		}

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
		const bool directPitch4 = directBytes == 4 &&
			directPitch >= frame->outputWidth * 4 && (directPitch & 3) == 0 &&
			(directPitch == frame->outputWidth * 4 || packSubimage);
		const bool directPitch3 = directBytes == 3 &&
			directPitch >= frame->outputWidth * 3 && directPitch % 3 == 0 &&
			(directPitch == frame->outputWidth * 3 || packSubimage);
		const bool directPitch2 = directBytes == 2 &&
			directPitch >= frame->outputWidth * 2 && (directPitch & 1) == 0 &&
			(directPitch == frame->outputWidth * 2 || packSubimage);
		const bool directReadBgra = readBgraSupported && directPitch4;
		const bool directReadRgb =
			readRgb888Supported && directPitch3;
		const bool directReadRgba = !directReadBgra && nativeReadFormat == GL_RGBA &&
			nativeReadType == GL_UNSIGNED_BYTE && directPitch4;
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
		if (!drawUniformsValid[sourceShaderIndex] ||
			lastVerticalReconstruct[sourceShaderIndex] !=
			(frame->verticalReconstruct2x != 0)) {
			glUniform1i(verticalReconstructUniforms[sourceShaderIndex],
				frame->verticalReconstruct2x ? 1 : 0);
			lastVerticalReconstruct[sourceShaderIndex] =
				frame->verticalReconstruct2x != 0;
		}
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
		if (rasterSource) glBindTexture(GL_TEXTURE_2D, rasterTexture);
		if (partialReadback && readSpanCount > 0) {
			glEnable(GL_SCISSOR_TEST);
			for (INT32 span = 0; span < readSpanCount; span++) {
				glScissor(0, readSpans[span].start, frame->outputWidth,
					readSpans[span].count);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}
			glDisable(GL_SCISSOR_TEST);
		} else {
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
		if (rasterSource) {
			glBindTexture(GL_TEXTURE_2D, rgb24 ? rgbTexture : texture);
		}
		if (sourceShaderIndex != activeShaderIndex) glUseProgram(programs[activeShaderIndex]);
		auto readDirectRows = [&](GLenum format, GLenum type,
			UINT8 *destination, INT32 destinationPitch) {
			if (partialReadback && readSpanCount > 1) {
				for (INT32 span = 0; span < readSpanCount; span++) {
					glReadPixels(0, readSpans[span].start, frame->outputWidth,
						readSpans[span].count, format, type, destination +
						(size_t)readSpans[span].start * destinationPitch);
				}
			} else {
				glReadPixels(0, partialReadback ? readY : 0, frame->outputWidth,
					partialReadback ? readHeight : frame->outputHeight,
					format, type, destination +
					(partialReadback ? (size_t)readY * destinationPitch : 0));
			}
		};

		// Each readback path below consumes the accumulated GL error after
		// glReadPixels.  Avoid an additional synchronous query on every frame.
		if (readRgb565Supported && directPitch2 &&
			NamcosPaletteIsRgb565(palette)) {
			SetPackAlignment(2);
			SetPackRowLength(directPitch / 2);
			readDirectRows(GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
				directDestination, directPitch);
			const GLenum readError = ValidateReadback(GL_RGB,
				GL_UNSIGNED_SHORT_5_6_5) ? GL_NO_ERROR : GL_INVALID_OPERATION;
			if (readError == GL_NO_ERROR) {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration,
					&observedRows);
				outputFrameValid = true;
				return true;
			}
			readRgb565Supported = false;
		}

		if (directReadRgb) {
			SetPackAlignment(1);
			SetPackRowLength(directPitch / 3);
			readDirectRows(GL_RGB, GL_UNSIGNED_BYTE,
				directDestination, directPitch);
			const GLenum readError = ValidateReadback(GL_RGB,
				GL_UNSIGNED_BYTE) ? GL_NO_ERROR : GL_INVALID_OPERATION;
			if (readError == GL_NO_ERROR) {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration,
					&observedRows);
				outputFrameValid = true;
				return true;
			}
			readRgb888Supported = false;
			glUniform1i(swapRedBlueUniforms[activeShaderIndex], 0);
			lastSwapRedBlue[activeShaderIndex] = false;
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		if (directReadBgra) {
			SetPackAlignment(4);
			SetPackRowLength(directPitch / 4);
			readDirectRows(GL_BGRA_EXT, GL_UNSIGNED_BYTE,
				directDestination, directPitch);
			const GLenum readError = ValidateReadback(GL_BGRA_EXT,
				GL_UNSIGNED_BYTE) ? GL_NO_ERROR : GL_INVALID_OPERATION;
			if (readError != GL_NO_ERROR) {
				readBgraSupported = false;
			} else {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration,
					&observedRows);
				outputFrameValid = true;
				return true;
			}
		}

		if (directReadRgba) {
			SetPackAlignment(4);
			SetPackRowLength(directPitch / 4);
			readDirectRows(GL_RGBA, GL_UNSIGNED_BYTE,
				directDestination, directPitch);
			const GLenum readError = ValidateReadback(GL_RGBA,
				GL_UNSIGNED_BYTE) ? GL_NO_ERROR : GL_INVALID_OPERATION;
			if (readError != GL_NO_ERROR) {
				Disable();
				return false;
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration,
				&observedRows);
			outputFrameValid = true;
			return true;
		}

		const bool nativeReadBgra = nativeReadFormat == GL_BGRA_EXT &&
			nativeReadType == GL_UNSIGNED_BYTE;
		if (nativeReadBgra && directPitch4) {
			SetPackAlignment(4);
			SetPackRowLength(directPitch / 4);
			readDirectRows(GL_BGRA_EXT, GL_UNSIGNED_BYTE,
				directDestination, directPitch);
			const GLenum readError = ValidateReadback(GL_BGRA_EXT,
				GL_UNSIGNED_BYTE) ? GL_NO_ERROR : GL_INVALID_OPERATION;
			if (readError != GL_NO_ERROR) {
				Disable();
				return false;
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration,
				&observedRows);
			outputFrameValid = true;
			return true;
		}

		const bool nativeRead16 = nativeReadType == GL_UNSIGNED_SHORT_5_5_5_1 ||
			nativeReadType == GL_UNSIGNED_SHORT_5_6_5;
		const INT32 fallbackReadY = partialReadback ? readY : 0;
		const INT32 fallbackReadRows = partialReadback ? readHeight :
			frame->outputHeight;
		const size_t bytes = (size_t)frame->outputWidth * frame->outputHeight *
			(nativeRead16 ? 2 : 4);
		UINT8 *readDestination = NULL;
		if (nativeRead16 && directDestination == NULL) {
			readDestination = (UINT8 *)frame->output;
		} else if (nativeRead16 && directPitch2) {
			readDestination = directDestination;
		}
		if (readDestination == NULL) {
			if (!PrepareReadPixels(bytes)) {
				Disable();
				return false;
			}
			readDestination = readPixels;
		}

		const INT32 readBytesPerPixel = nativeRead16 ? 2 : 4;
		const INT32 readPitch = readDestination == directDestination &&
			directBytes > 0 ? directPitch : frame->outputWidth * readBytesPerPixel;
		SetPackAlignment(nativeRead16 ? 2 : 1);
		SetPackRowLength(readDestination == directDestination && directBytes > 0 ?
			directPitch / directBytes : 0);
		if (partialReadback && readSpanCount > 1) {
			for (INT32 span = 0; span < readSpanCount; span++) {
				glReadPixels(0, readSpans[span].start, frame->outputWidth,
					readSpans[span].count, nativeReadFormat, nativeReadType,
					readDestination + (size_t)readSpans[span].start * readPitch);
			}
		} else {
			glReadPixels(0, fallbackReadY, frame->outputWidth, fallbackReadRows,
				nativeReadFormat, nativeReadType, readDestination +
				(size_t)fallbackReadY * readPitch);
		}
		const GLenum nativeReadError = ValidateReadback(nativeReadFormat,
			nativeReadType) ? GL_NO_ERROR : GL_INVALID_OPERATION;
		if (nativeReadError != GL_NO_ERROR) {
			Disable();
			return false;
		}

		if (nativeRead16) {
			if (nativeReadType == GL_UNSIGNED_SHORT_5_6_5 && directPitch2 &&
				NamcosPaletteIsRgb565(palette)) {
				NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
					directPitch, directBytes, palette, outputRowGeneration,
					&observedRows);
				outputFrameValid = true;
				return true;
			}
			const bool directNativeRgb565 = directBytes == 2 &&
				NamcosPaletteIsRgb565(palette);
			const bool directNativeXrgb8888 = (directBytes == 3 || directBytes == 4) &&
				NamcosPaletteIsXrgb8888(palette);
			if (directDestination != NULL && !directNativeRgb565 &&
				!directNativeXrgb8888 && directPalette != palette) {
				PrepareDirectReadTable(palette);
			}
			const INT32 convertSpanCount = partialReadback && readSpanCount > 1 ?
				readSpanCount : 1;
			for (INT32 span = 0; span < convertSpanCount; span++) {
				const INT32 spanY = convertSpanCount > 1 ? readSpans[span].start :
					fallbackReadY;
				const INT32 spanRows = convertSpanCount > 1 ? readSpans[span].count :
					fallbackReadRows;
				NamcosGlReadConvertContext convertContext;
				convertContext.source = readDestination + (size_t)spanY * readPitch;
				convertContext.destination = directDestination != NULL ?
					directDestination + (size_t)spanY * directPitch : NULL;
				convertContext.indexedDestination = frame->output +
					(size_t)spanY * frame->outputWidth;
				convertContext.readTable = readTable;
				convertContext.directReadTable = directReadTable;
				convertContext.palette = palette;
				convertContext.width = frame->outputWidth;
				convertContext.sourcePitch = readPitch;
				convertContext.destinationPitch = directPitch;
				convertContext.destinationBytes = directBytes;
				convertContext.redOffset = 0;
				convertContext.blueOffset = 2;
				convertContext.native5551 =
					nativeReadType == GL_UNSIGNED_SHORT_5_5_5_1;
				convertContext.directRgb565 = directNativeRgb565;
				convertContext.directXrgb8888 = directNativeXrgb8888;
				NamcosGlConvertReadRows(frame, NamcosGlConvertNative16Rows,
					&convertContext, spanRows);
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, outputDestination,
				outputPitch, outputBytes, outputPalette, outputRowGeneration,
				&observedRows);
			outputFrameValid = true;
			return true;
		}
		if (nativeReadBgra && directBytes == 4) {
			const size_t rowBytes = (size_t)frame->outputWidth * 4;
			const INT32 copySpanCount = partialReadback && readSpanCount > 1 ?
				readSpanCount : 1;
			for (INT32 span = 0; span < copySpanCount; span++) {
				const INT32 spanY = copySpanCount > 1 ? readSpans[span].start :
					fallbackReadY;
				const INT32 spanRows = copySpanCount > 1 ? readSpans[span].count :
					fallbackReadRows;
				NamcosCopyPitchedRows(readDestination + (size_t)spanY * readPitch,
					directDestination + (size_t)spanY * directPitch, rowBytes,
					readPitch, directPitch, spanRows, frame->threadPool);
			}
			NamcosFrameRememberOutput(&outputFrameKey, frame, directDestination,
				directPitch, directBytes, palette, outputRowGeneration,
				&observedRows);
			outputFrameValid = true;
			return true;
		}

		const INT32 redOffset = nativeReadBgra ? 2 : 0;
		const INT32 blueOffset = nativeReadBgra ? 0 : 2;
		const INT32 convertSpanCount = partialReadback && readSpanCount > 1 ?
			readSpanCount : 1;
		for (INT32 span = 0; span < convertSpanCount; span++) {
			const INT32 spanY = convertSpanCount > 1 ? readSpans[span].start :
				fallbackReadY;
			const INT32 spanRows = convertSpanCount > 1 ? readSpans[span].count :
				fallbackReadRows;
			NamcosGlReadConvertContext convertContext;
			convertContext.source = readDestination + (size_t)spanY * readPitch;
			convertContext.destination = directDestination != NULL ?
				directDestination + (size_t)spanY * directPitch : NULL;
			convertContext.indexedDestination = frame->output +
				(size_t)spanY * frame->outputWidth;
			convertContext.readTable = readTable;
			convertContext.directReadTable = directReadTable;
			convertContext.palette = palette;
			convertContext.width = frame->outputWidth;
			convertContext.sourcePitch = readPitch;
			convertContext.destinationPitch = directPitch;
			convertContext.destinationBytes = directBytes;
			convertContext.redOffset = redOffset;
			convertContext.blueOffset = blueOffset;
			convertContext.native5551 = 0;
			convertContext.directRgb565 = directBytes == 2 &&
				NamcosPaletteIsRgb565(palette);
			convertContext.directXrgb8888 = (directBytes == 3 || directBytes == 4) &&
				NamcosPaletteIsXrgb8888(palette);
			NamcosGlConvertReadRows(frame, NamcosGlConvertRgbaRows,
				&convertContext, spanRows);
		}

		NamcosFrameRememberOutput(&outputFrameKey, frame, outputDestination,
			outputPitch, outputBytes, outputPalette, outputRowGeneration,
			&observedRows);
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
		rasterSampleValid = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
		rasterSampleDirty.Reset();
	}

	void Shutdown()
	{
		if (display != EGL_NO_DISPLAY) {
			if (context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE) {
				eglMakeCurrent(display, surface, surface, context);
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
		lastPackAlignment = -1;
		lastPackRowLength = -1;
		readbackValidationCounter = 0;
		validatedReadFormat = 0;
		validatedReadType = 0;
		validatedReadPackAlignment = -1;
		validatedReadPackRowLength = -1;
		lastUnpackAlignment = -1;
		lastUnpackRowLength = -1;
		surfacePixelBytes = 0;
		nativeReadFormat = GL_RGBA;
		nativeReadType = GL_UNSIGNED_BYTE;
		readRgb565Supported = false;
		readRgb888Supported = false;
		readBgraSupported = false;
		packSubimage = false;
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
	void SetPackAlignment(INT32 alignment)
	{
		if (lastPackAlignment == alignment) return;
		glPixelStorei(GL_PACK_ALIGNMENT, alignment);
		lastPackAlignment = alignment;
	}

	bool ValidateReadback(GLenum format, GLenum type)
	{
		const bool stateChanged = validatedReadFormat != format ||
			validatedReadType != type ||
			validatedReadPackAlignment != lastPackAlignment ||
			validatedReadPackRowLength != lastPackRowLength;
		if (stateChanged || (++readbackValidationCounter & 0x3f) == 0) {
			if (glGetError() != GL_NO_ERROR) return false;
			validatedReadFormat = format;
			validatedReadType = type;
			validatedReadPackAlignment = lastPackAlignment;
			validatedReadPackRowLength = lastPackRowLength;
		}
		return true;
	}

	void SetPackRowLength(INT32 length)
	{
		if (!packSubimage) {
			lastPackRowLength = 0;
			return;
		}
		if (lastPackRowLength == length) return;
		glPixelStorei(GL_PACK_ROW_LENGTH, length);
		lastPackRowLength = length;
	}

	void SetUnpackAlignment(INT32 alignment)
	{
		if (lastUnpackAlignment == alignment) return;
		glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
		lastUnpackAlignment = alignment;
	}

	void SetUnpackRowLength(INT32 length)
	{
		if (!unpackSubimage) {
			lastUnpackRowLength = 0;
			return;
		}
		if (lastUnpackRowLength == length) return;
		glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, length);
		lastUnpackRowLength = length;
	}

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
		bool pixelCacheValid;
		bool valid;
	};

	void InvalidateUploadCaches()
	{
		for (INT32 i = 0; i < 4; i++) {
			uploadSlots[i].denseFrames = 0;
			uploadSlots[i].pixelCacheValid = false;
			uploadSlots[i].valid = false;
		}
		rgbCacheValid = false;
		rgbDenseFrames = 0;
		uploadFrameValid = false;
		outputFrameValid = false;
	}

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
		rasterPendingCount = 0;
		rasterValidationCounter = 0;
		rasterSampleValidationCounter = 0;
		rasterUploadValidationCounter = 0;
		rasterReadbackValidationCounter = 0;
		rasterClearColorValid = false;
		rasterProgram = 0;
		rasterFramebuffer = 0;
		rasterTexture = 0;
		rasterSampleTexture = 0;
		rasterPositionAttribute = -1;
		rasterColorAttribute = -1;
		rasterTextureAttribute = -1;
		rasterStateAttribute = -1;
		rasterTextureState0Attribute = -1;
		rasterTextureState1Attribute = -1;
		rasterSampleUniform = -1;
		rasterSampleTextureBound = false;
		rasterScissorValid = false;
		rasterVramSynchronized = false;
		rasterSampleValid = false;
		rasterRead5551 = false;
		rasterStateActive = false;
		rasterUploadTracker.Reset();
		rasterDirty.Reset();
		rasterSampleDirty.Reset();
	}

	bool CreateEs3RasterizerResources(bool useEs3)
	{
		static const GLchar vertexSource[] =
			"#version 300 es\n"
			"in vec2 aPosition;\n"
			"in vec3 aColor;\n"
			"in vec2 aTexture;\n"
			"in float aState;\n"
			"in vec4 aTextureState0;\n"
			"in vec4 aTextureState1;\n"
			"out vec3 vColor;\n"
			"out vec2 vTexture;\n"
			"out float vState;\n"
			"out vec4 vTextureAddress;\n"
			"out vec4 vTextureControl;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0,"
			" 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor;\n"
			" vTexture = aTexture; vState = aState;\n"
			" vTextureAddress = aTextureState0; vTextureControl = aTextureState1;\n"
			"}\n";
		static const GLchar fragmentSource[] =
			"#version 300 es\n"
			"precision highp float;\n"
			"precision highp int;\n"
			"in vec3 vColor;\n"
			"in vec2 vTexture;\n"
			"in float vState;\n"
			"in vec4 vTextureAddress;\n"
			"in vec4 vTextureControl;\n"
			"uniform sampler2D uVram;\n"
			"#define uTextureState0 vec4(vTextureAddress.xy, vTextureControl.xy)\n"
			"#define uTextureState1 vec4(vTextureControl.zw, vTextureAddress.zw)\n"
			"out vec4 outputColor;\n"
			"int wordAt(int x, int y) {\n"
			" vec4 c = texelFetch(uVram, ivec2(x & 1023, 1023 - (y & 1023)), 0);\n"
			" ivec3 q = ivec3(c.rgb * 31.0 + 0.5);\n"
			" return q.r | (q.g << 5) | (q.b << 10) | (c.a >= 0.5 ? 32768 : 0);\n"
			"}\n"
			"vec4 unpackWord(int word) {\n"
			" return vec4(float(word & 31), float((word >> 5) & 31),\n"
			"  float((word >> 10) & 31), (word & 32768) != 0 ? 1.0 : 0.0);\n"
			"}\n"
			"int textureWord() {\n"
			" int u = int(vTexture.x) & ((int(uTextureState1.x) << 3) | 7);\n"
			" int v = int(vTexture.y) & ((int(uTextureState1.y) << 3) | 7);\n"
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
			" int state = int(vState + 0.5); bool textured = (state & 1) != 0; bool vramCopy = (state & 2) != 0;\n"
			" if (state == 0 || state == 64) { outputColor = vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5) / 31.0, state == 64 ? 1.0 : 0.0); return; }\n"
			" int texel = vramCopy ? wordAt(int(vTextureAddress.x) + int(gl_FragCoord.x) - int(vTextureAddress.z), int(vTextureAddress.y) + 1023 - int(gl_FragCoord.y) - int(vTextureAddress.w)) : (textured ? textureWord() : 0);\n"
			" if (textured && texel == 0) discard;\n"
			" vec4 color = (textured || vramCopy) ? unpackWord(texel) : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && (state & 4) == 0) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = (state & 8) != 0 && (!textured || (texel & 32768) != 0);\n"
			" int backgroundWord = 0;\n"
			" if ((state & 128) != 0 || blend) backgroundWord = wordAt(int(gl_FragCoord.x), 1023 - int(gl_FragCoord.y));\n"
			" if ((state & 128) != 0 && (backgroundWord & 32768) != 0) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = unpackWord(backgroundWord).rgb; int abr = (state >> 4) & 3;\n"
			"  if (abr == 0) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr == 1) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr == 2) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  if (textured) color.a = 1.0; else color.a = 0.0;\n"
			" }\n"
			" if ((state & 64) != 0) color.a = 1.0;\n"
			" outputColor = vec4(color.rgb / 31.0, color.a);\n"
			"}\n";
		static const GLchar es2VertexSource[] =
			"attribute vec2 aPosition;\n"
			"attribute vec3 aColor;\n"
			"attribute vec2 aTexture;\n"
			"attribute float aState;\n"
			"attribute vec4 aTextureState0;\n"
			"attribute vec4 aTextureState1;\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"varying vec4 vStateBits;\n"
			"varying vec3 vStateControl;\n"
			"varying vec4 vTextureAddress;\n"
			"varying vec4 vTextureControl;\n"
			"void main() {\n"
			" vec2 clip = vec2(aPosition.x / 512.0 - 1.0, 1.0 - aPosition.y / 512.0);\n"
			" gl_Position = vec4(clip, 0.0, 1.0);\n"
			" vColor = aColor; vTexture = aTexture;\n"
			" vStateBits = vec4(mod(aState, 2.0), mod(floor(aState * 0.5), 2.0),"
			" mod(floor(aState * 0.25), 2.0), mod(floor(aState * 0.125), 2.0));\n"
			" vStateControl = vec3(mod(floor(aState * 0.0625), 4.0),"
			" mod(floor(aState * 0.015625), 2.0), step(127.5, aState));\n"
			" vTextureAddress = aTextureState0; vTextureControl = aTextureState1;\n"
			"}\n";
		static const GLchar es2FragmentSource[] =
			"precision highp float;\n"
			"varying vec3 vColor;\n"
			"varying vec2 vTexture;\n"
			"varying vec4 vStateBits;\n"
			"varying vec3 vStateControl;\n"
			"varying vec4 vTextureAddress;\n"
			"varying vec4 vTextureControl;\n"
			"uniform sampler2D uVram;\n"
			"#define uTextureState0 vec4(vTextureAddress.xy, vTextureControl.xy)\n"
			"#define uTextureState1 vec4(vTextureControl.zw, vTextureAddress.zw)\n"
			"float window8(float a, float bh) {\n"
			" float ah = floor(a / 8.0);\n"
			" vec4 weight = vec4(1.0, 2.0, 4.0, 8.0);\n"
			" vec4 bits = mod(floor(ah / weight), 2.0) * mod(floor(bh / weight), 2.0);\n"
			" float bit16 = mod(floor(ah / 16.0), 2.0) * mod(floor(bh / 16.0), 2.0);\n"
			" return mod(a, 8.0) + 8.0 * (dot(bits, weight) + bit16 * 16.0);\n"
			"}\n"
			"vec4 colorAt(float x, float y) {\n"
			" vec2 uv = fract(vec2(x + 0.5, 1023.5 - y) / 1024.0);\n"
			" vec4 c = texture2D(uVram, uv);\n"
			" vec3 q = floor(c.rgb * 31.0 + 0.5);\n"
			" return vec4(q, c.a >= 0.5 ? 1.0 : 0.0);\n"
			"}\n"
			"float packColor(vec4 color) {\n"
			" return color.r + color.g * 32.0 + color.b * 1024.0 + color.a * 32768.0;\n"
			"}\n"
			"float wordAt(float x, float y) {\n"
			" return packColor(colorAt(x, y));\n"
			"}\n"
			"void textureWord(out vec4 textureColor) {\n"
			" float sourceU = floor(vTexture.x); float sourceV = floor(vTexture.y);\n"
			" float u = sourceU; float v = sourceV;\n"
			" if (uTextureState1.x < 30.5) u = window8(sourceU, uTextureState1.x);\n"
			" if (uTextureState1.y < 30.5) v = window8(sourceV, uTextureState1.y);\n"
			" float tx = uTextureState0.x; float ty = uTextureState0.y;\n"
			" float mode = uTextureState0.z; bool interleaved = uTextureState0.w != 0.0;\n"
			" float x = tx; float y = mod(ty + v, 1024.0); float data;\n"
			" if (mode < 0.5) {\n"
			"  if (interleaved) { x += mod(floor(u / 4.0), 4.0) + mod(v, 16.0) * 4.0; y = ty + (v - mod(v, 16.0)) + mod(floor(u / 16.0), 16.0); }\n"
			"  else x += floor(u / 4.0);\n"
			"  float nibble = mod(u, 4.0);\n"
			"  float divisor = ((562.5 * nibble - 1575.0) * nibble + 1027.5) * nibble + 1.0;\n"
			"  data = mod(floor(wordAt(x, y) / divisor), 16.0);\n"
			"  textureColor = colorAt(uTextureState1.z + data, uTextureState1.w); return;\n"
			" }\n"
			" if (mode < 1.5) {\n"
			"  if (interleaved) { x += mod(floor(u / 2.0), 8.0) + floor(mod(u, 32.0) / 16.0) * 64.0 + mod(v, 8.0) * 8.0; y = ty + (v - mod(v, 8.0)) + mod(floor(u / 32.0), 8.0); }\n"
			"  else x += floor(u / 2.0);\n"
			"  data = wordAt(x, y); data = mix(mod(data, 256.0), floor(data / 256.0), mod(u, 2.0));\n"
			"  textureColor = colorAt(uTextureState1.z + data, uTextureState1.w); return;\n"
			" }\n"
			" if (mode < 2.5) { textureColor = colorAt(tx + u, y); return; }\n"
			" textureColor = vec4(0.0);\n"
			"}\n"
			"void main() {\n"
			" bool textured = vStateBits.x >= 0.5; bool vramCopy = vStateBits.y >= 0.5; vec4 textureColor = vec4(0.0);\n"
			" if (dot(vStateBits, vec4(1.0)) < 0.5 && vStateControl.x < 0.5 && vStateControl.z < 0.5) { gl_FragColor = vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5) / 31.0, vStateControl.y); return; }\n"
			" if (vramCopy) textureColor = colorAt(vTextureAddress.x + floor(gl_FragCoord.x) - vTextureAddress.z, vTextureAddress.y + 1023.0 - floor(gl_FragCoord.y) - vTextureAddress.w);\n"
			" if (textured) textureWord(textureColor);\n"
			" if (textured && dot(textureColor, vec4(1.0)) < 0.5) discard;\n"
			" vec4 color = (textured || vramCopy) ? textureColor : vec4(floor(clamp(vColor, 0.0, 1.0) * 31.0 + 0.5), 0.0);\n"
			" if (textured && vStateBits.z < 0.5) color.rgb = min(vec3(31.0), floor(color.rgb * floor(vColor * 255.0 + 0.5) / 128.0));\n"
			" bool blend = vStateBits.w >= 0.5 && (!textured || textureColor.a >= 0.5);\n"
			" vec4 backgroundColor = vec4(0.0);\n"
			" if (vStateControl.z >= 0.5 || blend) backgroundColor = colorAt(floor(gl_FragCoord.x), 1023.0 - floor(gl_FragCoord.y));\n"
			" if (vStateControl.z >= 0.5 && backgroundColor.a >= 0.5) discard;\n"
			" if (blend) {\n"
			"  vec3 bg = backgroundColor.rgb; float abr = vStateControl.x;\n"
			"  if (abr < 0.5) color.rgb = floor(color.rgb * 0.5) + floor(bg * 0.5);\n"
			"  else if (abr < 1.5) color.rgb = min(vec3(31.0), color.rgb + bg);\n"
			"  else if (abr < 2.5) color.rgb = max(vec3(0.0), bg - color.rgb);\n"
			"  else color.rgb = min(vec3(31.0), floor(color.rgb * 0.25) + bg);\n"
			"  color.a = textured ? 1.0 : 0.0;\n"
			" }\n"
			" if (vStateControl.y >= 0.5) color.a = 1.0;\n"
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
		rasterStateAttribute = glGetAttribLocation(rasterProgram, "aState");
		rasterTextureState0Attribute = glGetAttribLocation(rasterProgram,
			"aTextureState0");
		rasterTextureState1Attribute = glGetAttribLocation(rasterProgram,
			"aTextureState1");
		rasterSampleUniform = glGetUniformLocation(rasterProgram, "uVram");
		if (rasterPositionAttribute < 0 || rasterColorAttribute < 0 ||
			rasterTextureAttribute < 0 || rasterStateAttribute < 0 ||
			rasterTextureState0Attribute < 0 ||
			rasterTextureState1Attribute < 0 || rasterSampleUniform < 0) {
			DestroyEs3RasterizerResources();
			return false;
		}
		glUseProgram(rasterProgram);
		glUniform1i(rasterSampleUniform, 0);
		glUseProgram(programs[activeShaderIndex]);

		glGenTextures(1, &rasterTexture);
		glBindTexture(GL_TEXTURE_2D, rasterTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024,
			1024, 0, GL_RGBA,
			GL_UNSIGNED_SHORT_5_5_5_1, NULL);
		glGenTextures(1, &rasterSampleTexture);
		glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024,
			1024, 0, GL_RGBA,
			GL_UNSIGNED_SHORT_5_5_5_1, NULL);
		glGenFramebuffers(1, &rasterFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, rasterTexture, 0);
		const bool framebufferReady = rasterTexture != 0 &&
			rasterSampleTexture != 0 &&
			rasterFramebuffer != 0 &&
			glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
			glGetError() == GL_NO_ERROR;
		if (framebufferReady) {
			GLint readFormat = 0;
			GLint readType = 0;
			glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES, &readFormat);
			glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE_OES, &readType);
			rasterRead5551 = readFormat == GL_RGBA &&
				readType == GL_UNSIGNED_SHORT_5_5_5_1 &&
				glGetError() == GL_NO_ERROR;
		}
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
		glEnableVertexAttribArray(rasterPositionAttribute);
		glEnableVertexAttribArray(rasterColorAttribute);
		glEnableVertexAttribArray(rasterTextureAttribute);
		glEnableVertexAttribArray(rasterStateAttribute);
		glEnableVertexAttribArray(rasterTextureState0Attribute);
		glEnableVertexAttribArray(rasterTextureState1Attribute);
		glBufferData(GL_ARRAY_BUFFER,
			sizeof(NamcosGlRasterDrawVertex) *
				NAMCOS_GL_RASTER_STREAM_VERTICES, NULL,
			GL_STREAM_DRAW);
		rasterVertexOffset = 0;
		rasterPendingCount = 0;
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
		glVertexAttribPointer(rasterPositionAttribute, 2, GL_SHORT, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, x));
		glVertexAttribPointer(rasterColorAttribute, 3, GL_UNSIGNED_BYTE, GL_TRUE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, red));
		glVertexAttribPointer(rasterTextureAttribute, 2, GL_UNSIGNED_BYTE, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, u));
		glVertexAttribPointer(rasterStateAttribute, 1, GL_UNSIGNED_BYTE, GL_FALSE,
			sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, padding));
		glVertexAttribPointer(rasterTextureState0Attribute, 4, GL_SHORT,
			GL_FALSE, sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, textureState0));
		glVertexAttribPointer(rasterTextureState1Attribute, 4, GL_UNSIGNED_BYTE,
			GL_FALSE, sizeof(NamcosGlRasterDrawVertex),
			(const void *)offsetof(NamcosGlRasterDrawVertex, textureControl0));
		rasterSampleTextureBound = false;
		rasterStateActive = true;
		return true;
	}

	bool FlushEsRasterVertices()
	{
		if (rasterPendingCount == 0) return true;
		if (rasterVertexOffset + rasterPendingCount >
			NAMCOS_GL_RASTER_STREAM_VERTICES) {
			glBufferData(GL_ARRAY_BUFFER,
				sizeof(NamcosGlRasterDrawVertex) *
					NAMCOS_GL_RASTER_STREAM_VERTICES, NULL, GL_STREAM_DRAW);
			rasterVertexOffset = 0;
		}
		const ptrdiff_t vertexOffset = sizeof(NamcosGlRasterDrawVertex) *
			rasterVertexOffset;
		glBufferSubData(GL_ARRAY_BUFFER, vertexOffset,
			sizeof(NamcosGlRasterDrawVertex) * rasterPendingCount,
			rasterPendingVertices);
		glDrawArrays(GL_TRIANGLES, rasterVertexOffset, rasterPendingCount);
		rasterVertexOffset += rasterPendingCount;
		rasterPendingCount = 0;
		if ((++rasterValidationCounter & 0xff) == 0 &&
			glGetError() != GL_NO_ERROR) return false;
		return true;
	}

	void EndEsRasterState(bool force = false)
	{
		if (!rasterStateActive && !force) return;
		FlushEsRasterVertices();
		glDisable(GL_SCISSOR_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, viewportWidth, viewportHeight);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glUseProgram(programs[activeShaderIndex]);
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterSampleTextureBound = false;
		rasterStateActive = false;
	}

	bool SynchronizeEsRasterSample(const NamcosGlRasterRect *selectedRects = NULL,
		INT32 selectedCount = 0, bool selectedCoversAllDirty = false)
	{
		if (!FlushEsRasterVertices()) return false;
		if (!rasterSampleTextureBound) {
			glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
			rasterSampleTextureBound = true;
		}
		const bool completeSync = !rasterSampleValid;
		if (completeSync) {
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
				1024, 1024);
			rasterSampleDirty.Reset();
		} else if (rasterSampleDirty.valid) {
			NamcosGlRasterRect rects[NAMCOS_GL_RASTER_DIRTY_RECTS];
			const bool selective = selectedRects != NULL && selectedCount > 0;
			const INT32 count = selective ? selectedCount :
				rasterSampleDirty.GetCopyRects(rects,
					NAMCOS_GL_RASTER_DIRTY_RECTS, 65536);
			for (INT32 i = 0; i < count; i++) {
				const NamcosGlRasterRect &rect = selective ? selectedRects[i] :
					rects[i];
				const INT32 width = rect.x2 - rect.x1 + 1;
				const INT32 height = rect.y2 - rect.y1 + 1;
				const INT32 glY = 1024 - rect.y2 - 1;
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0,
					rect.x1, glY, rect.x1, glY, width, height);
			}
			if (selective && !selectedCoversAllDirty) {
				for (INT32 i = 0; i < count; i++) {
					rasterSampleDirty.Exclude(selectedRects[i].x1,
						selectedRects[i].y1, selectedRects[i].x2,
						selectedRects[i].y2);
				}
			} else {
				rasterSampleDirty.Reset();
			}
		}
		if ((++rasterSampleValidationCounter & 0xff) == 0 &&
			glGetError() != GL_NO_ERROR) return false;
		rasterSampleValid = true;
		return true;
	}

	bool SubmitEs3RasterPrimitive(const NamcosGlRasterPacket *packet,
		const NamcosGlRasterPrimitive *primitive)
	{
		if (packet == NULL || primitive == NULL) return false;
		// Small solid rectangles batch better than flushing around glClear.
		const bool largeOpaqueRectangle = primitive->type ==
			NAMCOS_GL_RASTER_FLAT_RECTANGLE && !primitive->semiTransparent &&
			!packet->state.checkStp && (INT64)primitive->width *
			primitive->height >= NAMCOS_GL_RASTER_FAST_CLEAR_PIXELS;
		const bool fastClear = (primitive->type == NAMCOS_GL_RASTER_FILL &&
			(INT64)primitive->width * primitive->height >=
				NAMCOS_GL_RASTER_FAST_CLEAR_PIXELS) || largeOpaqueRectangle;
		const bool textured = primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_POLYGON || primitive->type ==
			NAMCOS_GL_RASTER_TEXTURED_RECTANGLE;
		const bool fill = primitive->type == NAMCOS_GL_RASTER_FILL;
		const bool vramCopy = primitive->type == NAMCOS_GL_RASTER_VRAM_COPY;
		if (!rasterStateActive && eglGetCurrentContext() != context &&
			eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)
			return false;

		INT32 x1, y1, x2, y2;
		if (!NamcosGlRasterGetDrawBounds(packet, primitive,
			&x1, &y1, &x2, &y2)) return false;

		if (!BeginEsRasterState()) return false;
		INT32 scissorX1 = x1;
		INT32 scissorY1 = y1;
		INT32 scissorX2 = x2;
		INT32 scissorY2 = y2;
		if (!fastClear && primitive->type == NAMCOS_GL_RASTER_FILL) {
			scissorX1 = 0;
			scissorY1 = 0;
			scissorX2 = 1023;
			scissorY2 = 1023;
		} else if (!fastClear && !vramCopy) {
			scissorX1 = (INT32)packet->state.drawX1;
			scissorY1 = (INT32)packet->state.drawY1;
			scissorX2 = (INT32)packet->state.drawX2;
			scissorY2 = (INT32)packet->state.drawY2;
			if (scissorX1 < 0) scissorX1 = 0;
			if (scissorY1 < 0) scissorY1 = 0;
			if (scissorX2 > 1023) scissorX2 = 1023;
			if (scissorY2 > 1023) scissorY2 = 1023;
		}
		const INT32 scissorY = 1024 - scissorY2 - 1;
		const INT32 scissorWidth = scissorX2 - scissorX1 + 1;
		const INT32 scissorHeight = scissorY2 - scissorY1 + 1;
		if (!rasterScissorValid || lastRasterScissorX != scissorX1 ||
			lastRasterScissorY != scissorY ||
			lastRasterScissorWidth != scissorWidth ||
			lastRasterScissorHeight != scissorHeight) {
			if (!FlushEsRasterVertices()) return false;
			glScissor(scissorX1, scissorY, scissorWidth, scissorHeight);
			lastRasterScissorX = scissorX1;
			lastRasterScissorY = scissorY;
			lastRasterScissorWidth = scissorWidth;
			lastRasterScissorHeight = scissorHeight;
			rasterScissorValid = true;
		}
		if (fastClear) {
			if (!FlushEsRasterVertices()) return false;
			const UINT32 red = (primitive->vertex[0].red * 31 + 127) / 255;
			const UINT32 green = (primitive->vertex[0].green * 31 + 127) / 255;
			const UINT32 blue = (primitive->vertex[0].blue * 31 + 127) / 255;
			const UINT32 alpha = primitive->type != NAMCOS_GL_RASTER_FILL &&
				packet->state.drawStp ? 1 : 0;
			const UINT32 clearColor = red | (green << 5) | (blue << 10) |
				(alpha << 15);
			if (!rasterClearColorValid || lastRasterClearColor != clearColor) {
				glClearColor((GLfloat)red / 31.0f, (GLfloat)green / 31.0f,
					(GLfloat)blue / 31.0f, (GLfloat)alpha);
				lastRasterClearColor = clearColor;
				rasterClearColorValid = true;
			}
			glClear(GL_COLOR_BUFFER_BIT);
			if ((++rasterValidationCounter & 0xff) == 0 &&
				glGetError() != GL_NO_ERROR) {
				EndEsRasterState();
				return false;
			}
			rasterDirty.Include(x1, y1, x2, y2);
			if (rasterSampleValid) rasterSampleDirty.Include(x1, y1, x2, y2);
			outputFrameValid = false;
			return true;
		}
		UINT32 textureState[8];
		if (vramCopy) {
			textureState[0] = primitive->sourceX;
			textureState[1] = primitive->sourceY;
			textureState[2] = primitive->vertex[0].x;
			textureState[3] = primitive->vertex[0].y;
			memset(textureState + 4, 0, sizeof(UINT32) * 4);
		} else if (textured) {
			NamcosGlRasterGetTextureState(packet, primitive, textureState);
		}
		const bool checkStp = !fill && packet->state.checkStp != 0;
		if (textured || vramCopy || primitive->semiTransparent || checkStp) {
			NamcosGlRasterRect selected[NAMCOS_GL_RASTER_DIRTY_RECTS];
			const bool selectiveDestination = !textured && !vramCopy &&
				(primitive->semiTransparent || checkStp);
			const bool selectiveVramCopy = vramCopy &&
				!primitive->semiTransparent && !checkStp;
			const bool selectiveTexture = textured &&
				!primitive->semiTransparent && !checkStp;
			INT32 selectedCount = 0;
			bool selectedCoversAllDirty = false;
			bool synchronizeSample = !rasterSampleValid;
			if (!synchronizeSample && selectiveTexture) {
				bool dependencyKnown = false;
				selectedCount = NamcosGlRasterBuildTextureSelectiveCopyRects(
					textureState, primitive, &rasterSampleDirty, selected,
					NAMCOS_GL_RASTER_DIRTY_RECTS, &selectedCoversAllDirty,
					&dependencyKnown);
				synchronizeSample = dependencyKnown ? selectedCount > 0 :
					NamcosGlRasterTextureStateReadsDirty(textureState,
						&rasterSampleDirty);
			} else if (!synchronizeSample &&
				(selectiveDestination || selectiveVramCopy)) {
				const INT32 selectedX = selectiveVramCopy ?
					primitive->sourceX : x1;
				const INT32 selectedY = selectiveVramCopy ?
					primitive->sourceY : y1;
				const INT32 selectedWidth = selectiveVramCopy ?
					primitive->width : x2 - x1 + 1;
				const INT32 selectedHeight = selectiveVramCopy ?
					primitive->height : y2 - y1 + 1;
				selectedCount = NamcosGlRasterBuildSelectiveCopyRects(
					&rasterSampleDirty, selectedX, selectedY, selectedWidth,
					selectedHeight, selected, NAMCOS_GL_RASTER_DIRTY_RECTS,
					&selectedCoversAllDirty);
				synchronizeSample = selectedCount > 0;
			} else if (!synchronizeSample &&
				(primitive->semiTransparent || checkStp)) {
				synchronizeSample = rasterSampleDirty.Intersects(x1, y1, x2, y2);
			}
			if (!synchronizeSample && vramCopy) {
				synchronizeSample = rasterSampleDirty.Intersects(
					primitive->sourceX, primitive->sourceY,
					primitive->sourceX + primitive->width - 1,
					primitive->sourceY + primitive->height - 1);
			}
			if (!synchronizeSample && textured && !selectiveTexture) {
				synchronizeSample = NamcosGlRasterTexturePrimitiveReadsDirty(
					textureState, primitive, &rasterSampleDirty);
			}
			if (synchronizeSample) {
				if (!SynchronizeEsRasterSample(selectedCount > 0 ? selected : NULL,
					selectedCount, selectedCoversAllDirty)) return false;
			} else if (!rasterSampleTextureBound) {
				glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
				rasterSampleTextureBound = true;
			}
		}
		const bool rawTexture = textured && primitive->rawTexture;
		const UINT32 blendTpage = textured ? primitive->tpage : packet->state.tpage;
		const UINT32 abr = primitive->semiTransparent ? (blendTpage >>
			(packet->state.gpuType == 2 ? 5 : 7)) & 3 : 0;
		const bool drawStp = !fill && packet->state.drawStp != 0;
		const bool maskCheck = checkStp;
		if (rasterPendingCount + 6 > NAMCOS_GL_RASTER_BATCH_VERTICES &&
			!FlushEsRasterVertices()) return false;
		const UINT32 count = textured ? NamcosGlRasterBuildTexturedTriangles(
			primitive, rasterPendingVertices + rasterPendingCount, 6) :
			NamcosGlRasterBuildColorTriangles(primitive,
				rasterPendingVertices + rasterPendingCount, 6);
		if (count == 0) {
			return primitive->type == NAMCOS_GL_RASTER_FLAT_POLYGON ||
				primitive->type == NAMCOS_GL_RASTER_GOURAUD_POLYGON ||
				primitive->type == NAMCOS_GL_RASTER_TEXTURED_POLYGON;
		}
		const UINT8 vertexState = (textured ? 0x01 : 0) |
			(vramCopy ? 0x02 : 0) | (rawTexture ? 0x04 : 0) |
			(primitive->semiTransparent ? 0x08 : 0) | ((abr & 3) << 4) |
			(drawStp ? 0x40 : 0) | (maskCheck ? 0x80 : 0);
		NamcosGlRasterSetVertexStates(
			rasterPendingVertices + rasterPendingCount, count,
			vertexState, textureState, textured, vramCopy);
		rasterPendingCount += count;
		// A VRAM copy can feed a later transfer through wrapped coordinates that
		// are not represented by one dirty rectangle.  Preserve command order.
		if (vramCopy && !FlushEsRasterVertices()) return false;
		rasterDirty.Include(x1, y1, x2, y2);
		if (rasterSampleValid) rasterSampleDirty.Include(x1, y1, x2, y2);
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
			rasterTransferPixels = (UINT8 *)malloc((size_t)1024 * 1024 * 4);
			if (rasterTransferPixels == NULL) return false;
		}
		SetUnpackAlignment(2);
		SetUnpackRowLength(0);
		NamcosGlRasterUploadSpan spans[NAMCOS_GL_RASTER_UPLOAD_SPANS];
		INT32 spanCount = 1;
		if (!rasterUploadTracker.valid || rowGeneration == NULL) {
			spans[0].firstRow = 0;
			spans[0].rowCount = 1024;
		} else {
			INT32 first;
			INT32 rows;
			spanCount = NamcosGlRasterBuildUploadSpans(
				&rasterUploadTracker, rowGeneration, spans,
				NAMCOS_GL_RASTER_UPLOAD_SPANS, &first, &rows, 32);
			if (spanCount < 0) {
				spanCount = 1;
				spans[0].firstRow = first;
				spans[0].rowCount = rows;
			}
		}
		if (spanCount == 0) {
			rasterVramSynchronized = true;
			return true;
		}
		const bool validateUpload =
			(rasterUploadValidationCounter++ & 0x3f) == 0;
		if (validateUpload) glGetError();
		if (spanCount > 0) {
			glBindTexture(GL_TEXTURE_2D, rasterTexture);
			for (INT32 span = 0; span < spanCount; span++) {
				const INT32 first = spans[span].firstRow;
				const INT32 rows = spans[span].rowCount;
				NamcosGlPackVram5551RangeParallel(vram,
					(UINT16 *)rasterTransferPixels, first, rows,
					threadPool);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0,
					1024 - first - rows, 1024, rows, GL_RGBA,
					GL_UNSIGNED_SHORT_5_5_5_1,
					rasterTransferPixels);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
			glBindTexture(GL_TEXTURE_2D, rasterSampleTexture);
			for (INT32 span = 0; span < spanCount; span++) {
				const INT32 first = spans[span].firstRow;
				const INT32 rows = spans[span].rowCount;
				const INT32 textureY = 1024 - first - rows;
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, textureY, 0,
					textureY, 1024, rows);
			}
			rasterSampleTextureBound = false;
		}
		const bool uploaded = !validateUpload || glGetError() == GL_NO_ERROR;
		if (uploaded) {
			if (rowGeneration == NULL) {
				rasterUploadTracker.Reset();
			} else {
				for (INT32 span = 0; span < spanCount; span++) {
					rasterUploadTracker.RememberRange(rowGeneration,
						spans[span].firstRow, spans[span].rowCount);
				}
			}
		}
		glBindTexture(GL_TEXTURE_2D, texture);
		rasterVramSynchronized = uploaded;
		if (uploaded) {
			rasterDirty.Reset();
			rasterSampleValid = true;
			rasterSampleDirty.Reset();
		}
		return uploaded;
	}

	bool ReadbackEs3RasterVram(UINT16 *vram)
	{
		if (vram == NULL || rasterTransferPixels == NULL) return false;
		if (eglGetCurrentContext() != context &&
			eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)
			return false;
		if (!rasterDirty.valid) return true;
		NamcosGlRasterRect rects[NAMCOS_GL_RASTER_DIRTY_RECTS];
		const INT32 rectCount = rasterDirty.GetReadbackRects(rects,
			NAMCOS_GL_RASTER_DIRTY_RECTS, 65536);
		bool read = rectCount > 0;
		glBindFramebuffer(GL_FRAMEBUFFER, rasterFramebuffer);
		SetPackAlignment(rasterRead5551 ? 2 : 1);
		SetPackRowLength(0);
		const bool validateReadback =
			(rasterReadbackValidationCounter++ & 0x3f) == 0;
		if (validateReadback) glGetError();
		for (INT32 i = 0; i < rectCount; i++) {
			const INT32 width = rects[i].x2 - rects[i].x1 + 1;
			const INT32 height = rects[i].y2 - rects[i].y1 + 1;
			glReadPixels(rects[i].x1, 1024 - rects[i].y2 - 1,
				width, height, GL_RGBA, rasterRead5551 ?
				GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_BYTE,
				rasterTransferPixels);
			if (rasterRead5551) {
				NamcosGlReadVram5551RectParallel(
					(UINT16 *)rasterTransferPixels, vram, rects[i].x1,
					rects[i].y1, width, height, rasterThreadPool);
			} else {
				NamcosGlUnpackVramRectParallel(rasterTransferPixels, vram,
					rects[i].x1, rects[i].y1, width, height, rasterThreadPool);
			}
		}
		read = !validateReadback || glGetError() == GL_NO_ERROR;
		EndEsRasterState(true);
		if (read) rasterDirty.Reset();
		return read;
	}

	void UploadRgb24(const NamcosFrameConvertContext *frame)
	{
		SetUnpackRowLength(0);
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
			NamcosPackRgb24(frame, rgbUploadCache);
			rgbCacheWidth = width;
			rgbCacheHeight = rows;
			rgbCacheDisplayX = frame->displayX;
			rgbCacheDisplayY = frame->displayY;
			if (frame->vramRowGeneration != NULL) {
				NamcosFrameRememberRows(rgbRowGeneration,
					frame->vramRowGeneration, frame->displayY, 0, rows);
			} else {
				memset(rgbRowGeneration, 0, (size_t)rows * sizeof(UINT64));
			}
			rgbCacheValid = true;
			rgbDenseFrames = 0;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, rows,
				GL_RGB, GL_UNSIGNED_BYTE, rgbUploadCache);
			return;
		}
		if (frame->vramRowGeneration != NULL &&
			NamcosFrameRowsMatch(rgbRowGeneration, frame->vramRowGeneration,
				frame->displayY, observedFirst, observedRows)) {
			rgbDenseFrames = 0;
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
				NamcosPackRgb24Selected(frame, rgbUploadCache, denseRows,
					denseChangedRows, observedFirst, observedRows);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width,
					observedRows, GL_RGB, GL_UNSIGNED_BYTE,
					rgbUploadCache + (size_t)observedFirst * rowBytes);
				if (frame->vramRowGeneration != NULL) {
					NamcosFrameRememberRows(rgbRowGeneration,
						frame->vramRowGeneration, frame->displayY,
						observedFirst, observedRows);
				} else {
					memset(rgbRowGeneration + observedFirst, 0,
						(size_t)observedRows * sizeof(UINT64));
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
		memset(dirtyRows, 0, rows);
		const INT32 observedEnd = observedFirst + observedRows;
		for (INT32 y = observedFirst; y < observedEnd; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool candidate = frame->vramRowGeneration == NULL ||
				rgbRowGeneration[y] != generation;
			dirtyRows[y] = candidate ? 1 : 0;
			if (candidate) candidateRows++;
		}
		if (candidateRows == 0) return;
		NamcosPackRgb24Selected(frame, rgbUploadPixels, dirtyRows, candidateRows,
			observedFirst, observedRows);
		for (INT32 y = observedFirst; y < observedEnd; y++) {
			const UINT64 generation = frame->vramRowGeneration != NULL ?
				frame->vramRowGeneration[(frame->displayY + y) & 0x3ff] : 0;
			const bool candidate = dirtyRows[y] != 0;
			const bool changed = candidate &&
				memcmp(rgbUploadCache + (size_t)y * rowBytes,
					rgbUploadPixels + (size_t)y * rowBytes, rowBytes) != 0;
			if (candidate) rgbRowGeneration[y] = generation;
			dirtyRows[y] = changed ? 1 : 0;
			if (changed) {
				memcpy(rgbUploadCache + (size_t)y * rowBytes,
					rgbUploadPixels + (size_t)y * rowBytes, rowBytes);
				changedRows++;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= observedRows) {
			rgbDenseFrames = 7;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, observedFirst, width, observedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)observedFirst * rowBytes);
			return;
		}
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, mergedStart, width, mergedRows,
				GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)mergedStart * rowBytes);
			return;
		}
		NamcosFrameRowSpan spans[8];
		const INT32 spanCount = NamcosFrameBuildUploadSpans(dirtyRows, rows,
			rowBytes, spans, 8);
		for (INT32 i = 0; i < spanCount; i++) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, spans[i].start, width,
				spans[i].count, GL_RGB, GL_UNSIGNED_BYTE,
				rgbUploadCache + (size_t)spans[i].start * rowBytes);
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
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT_KHR,
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
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
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

		if (!CreateProgram()) {
			Disable();
			return false;
		}

		const bool useEs3Rasterizer = version != NULL &&
			(NamcosStringContainsNoCase(version, "OpenGL ES 3.") ||
			 NamcosStringContainsNoCase(version, "OpenGL ES 4."));
		const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
		readBgraSupported = HasExtension(extensions, "GL_EXT_read_format_bgra");
		packSubimage = useEs3Rasterizer ||
			HasExtension(extensions, "GL_NV_pack_subimage");
		unpackSubimage = HasExtension(extensions, "GL_EXT_unpack_subimage");
		if (HasExtension(extensions, "GL_EXT_discard_framebuffer")) {
			discardFramebuffer = (NamcosDiscardFramebufferProc)
				eglGetProcAddress("glDiscardFramebufferEXT");
		}
		const bool canQueryReadFormat =
			true;
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
		if (texture == 0
			|| rgbTexture == 0
			|| glGetError() != GL_NO_ERROR) {
			Disable();
			return false;
		}
		fullRasterizerCapable = CreateEs3RasterizerResources(useEs3Rasterizer);

		glViewport(0, 0, width, height);
		viewportWidth = width;
		viewportHeight = height;
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_DITHER);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		SetUnpackAlignment(2);
		SetUnpackRowLength(0);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		available = true;
		return true;
	}

	void UploadRows(const UINT16 *source, INT32 destinationX, INT32 destinationY,
		INT32 width, INT32 rows, NamcosPolyThreadPool *threadPool)
	{
		// A single source row is already contiguous even without
		// GL_EXT_unpack_subimage, so it does not need a staging copy.
		if (rows == 1 || width == 1024 || unpackSubimage) {
			SetUnpackRowLength(rows == 1 || width == 1024 ? 0 : 1024);
			glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY, width, rows,
				GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, source);
			return;
		}
		NamcosCopy16Rows(source, uploadPixels, width, rows, threadPool);
		SetUnpackRowLength(0);
		glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY, width, rows,
			GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, uploadPixels);
	}

	void UploadTightRows(const UINT16 *source, INT32 destinationX,
		INT32 destinationY, INT32 width, INT32 rows)
	{
		SetUnpackRowLength(0);
		glTexSubImage2D(GL_TEXTURE_2D, 0, destinationX, destinationY, width, rows,
			GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, source);
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
			slot->width == width && slot->rows == rows &&
			(rowGeneration != NULL || slot->pixelCacheValid);
		const bool directRows = width == 1024 || unpackSubimage;

		// Row generations already identify every changed source row.  When the
		// driver accepts a source stride, upload those rows directly from VRAM
		// instead of maintaining and filling a second tightly packed CPU cache.
		if (slot != NULL && rowGeneration != NULL && directRows) {
			if (!sameGeometry) {
				NamcosFrameRememberUploadRows(slot->rowGeneration, rowGeneration,
					destinationY, rows);
				slot->destinationX = destinationX;
				slot->destinationY = destinationY;
				slot->width = width;
				slot->rows = rows;
				slot->denseFrames = 0;
				slot->pixelCacheValid = false;
				slot->valid = true;
				UploadRows(source, destinationX, destinationY, width, rows, threadPool);
				return;
			}
			if (NamcosFrameRowsMatch(slot->rowGeneration, rowGeneration,
				destinationY, 0, rows)) return;
			UINT8 dirtyRows[1024];
			INT32 changedRows = 0;
			INT32 runCount = 0;
			bool inRun = false;
			for (INT32 y = 0; y < rows; y++) {
				const UINT64 generation =
					rowGeneration[(destinationY + y) & 0x3ff];
				const bool changed = slot->rowGeneration[y] != generation;
				slot->rowGeneration[y] = generation;
				dirtyRows[y] = changed ? 1 : 0;
				if (changed) {
					changedRows++;
					if (!inRun) runCount++;
				}
				inRun = changed;
			}
			slot->pixelCacheValid = false;
			if (changedRows == 0) return;
			if (changedRows * 3 >= rows) {
				UploadRows(source, destinationX, destinationY, width, rows, threadPool);
				return;
			}

			INT32 mergedStart;
			INT32 mergedRows;
			if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows,
				runCount, &mergedStart, &mergedRows)) {
				UploadRows(source + mergedStart * 1024, destinationX,
					destinationY + mergedStart, width, mergedRows, threadPool);
				return;
			}
			NamcosFrameRowSpan spans[8];
			const INT32 spanCount = NamcosFrameBuildUploadSpans(dirtyRows, rows,
				(size_t)width * sizeof(UINT16), spans, 8);
			for (INT32 i = 0; i < spanCount; i++) {
				UploadRows(source + spans[i].start * 1024, destinationX,
					destinationY + spans[i].start, width, spans[i].count, threadPool);
			}
			return;
		}

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
				NamcosFrameRememberUploadRows(slot->rowGeneration, rowGeneration,
					destinationY, rows);
				slot->destinationX = destinationX;
				slot->destinationY = destinationY;
				slot->width = width;
				slot->rows = rows;
				slot->denseFrames = 0;
				slot->pixelCacheValid = true;
				slot->valid = true;
			}
			if (slot != NULL) {
				UploadTightRows(slot->pixels, destinationX, destinationY, width, rows);
				return;
			}
			UploadRows(source, destinationX, destinationY, width, rows, threadPool);
			return;
		}
		if (slot->denseFrames != 0) {
			UINT8 denseRows[1024];
			INT32 denseChangedRows = rows;
			INT32 denseFirstRow = 0;
			INT32 denseLastRow = rows - 1;
			if (rowGeneration != NULL) {
				denseChangedRows = 0;
				denseFirstRow = rows;
				denseLastRow = -1;
				for (INT32 y = 0; y < rows; y++) {
					const UINT64 generation =
						rowGeneration[(destinationY + y) & 0x3ff];
					denseRows[y] = slot->rowGeneration[y] != generation ? 1 : 0;
					if (denseRows[y]) {
						denseChangedRows++;
						if (denseFirstRow == rows) denseFirstRow = y;
						denseLastRow = y;
					}
				}
				if (denseChangedRows == 0) {
					slot->denseFrames = 0;
					return;
				}
				if (denseChangedRows * 3 < rows) slot->denseFrames = 0;
			}
			if (slot->denseFrames != 0) {
				if (rowGeneration != NULL) {
					NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
						denseRows, denseChangedRows, threadPool, denseFirstRow,
						denseLastRow - denseFirstRow + 1);
					NamcosFrameRememberUploadRows(slot->rowGeneration,
						rowGeneration, destinationY, rows);
					UploadTightRows(slot->pixels, destinationX, destinationY, width, rows);
				} else {
					UploadRows(source, destinationX, destinationY, width, rows, threadPool);
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
		INT32 firstChangedRow = rows;
		INT32 lastChangedRow = -1;
		bool inRun = false;
		for (INT32 y = 0; y < rows; y++) {
			const UINT64 generation = rowGeneration != NULL ?
				rowGeneration[(destinationY + y) & 0x3ff] : 0;
			const bool changed = rowGeneration != NULL ?
				slot->rowGeneration[y] != generation :
				memcmp(slot->pixels + (size_t)y * width,
					source + (size_t)y * 1024,
					(size_t)width * sizeof(UINT16)) != 0;
			slot->rowGeneration[y] = generation;
			dirtyRows[y] = changed ? 1 : 0;
			if (changed) {
				changedRows++;
				if (firstChangedRow == rows) firstChangedRow = y;
				lastChangedRow = y;
				if (!inRun) runCount++;
			}
			inRun = changed;
		}
		if (changedRows == 0) return;

		if (changedRows * 3 >= rows) {
			slot->denseFrames = 7;
			NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
				dirtyRows, changedRows, threadPool, firstChangedRow,
				lastChangedRow - firstChangedRow + 1);
			UploadTightRows(slot->pixels, destinationX, destinationY, width, rows);
			return;
		}
		NamcosCopy16RowsSelected(source, slot->pixels, width, rows,
			dirtyRows, changedRows, threadPool, firstChangedRow,
			lastChangedRow - firstChangedRow + 1);
		INT32 mergedStart;
		INT32 mergedRows;
		if (NamcosFrameGetMergedDirtySpan(dirtyRows, rows, changedRows, runCount,
			&mergedStart, &mergedRows)) {
			UploadTightRows(slot->pixels + (size_t)mergedStart * width, destinationX,
				destinationY + mergedStart, width, mergedRows);
			return;
		}
		NamcosFrameRowSpan spans[8];
		const INT32 spanCount = NamcosFrameBuildUploadSpans(dirtyRows, rows,
			(size_t)width * sizeof(UINT16), spans, 8);
		for (INT32 i = 0; i < spanCount; i++) {
			UploadTightRows(slot->pixels + (size_t)spans[i].start * width,
				destinationX, destinationY + spans[i].start, width, spans[i].count);
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
	GLuint rgbTexture;
	GLuint programs[2];
	GLuint vertexBuffer;
	GLuint rasterTexture;
	GLuint rasterSampleTexture;
	GLuint rasterFramebuffer;
	GLuint rasterProgram;
	GLuint rasterVertexBuffer;
	UINT32 rasterVertexOffset;
	NamcosGlRasterDrawVertex rasterPendingVertices[NAMCOS_GL_RASTER_BATCH_VERTICES];
	UINT32 rasterPendingCount;
	UINT32 rasterValidationCounter;
	UINT32 rasterSampleValidationCounter;
	UINT32 rasterUploadValidationCounter;
	UINT32 rasterReadbackValidationCounter;
	UINT32 readbackValidationCounter;
	GLenum validatedReadFormat;
	GLenum validatedReadType;
	INT32 validatedReadPackAlignment;
	INT32 validatedReadPackRowLength;
	UINT32 lastRasterClearColor;
	bool rasterClearColorValid;
	GLint rasterPositionAttribute;
	GLint rasterColorAttribute;
	GLint rasterTextureAttribute;
	GLint rasterStateAttribute;
	GLint rasterTextureState0Attribute;
	GLint rasterTextureState1Attribute;
	GLint rasterSampleUniform;
	bool rasterSampleTextureBound;
	bool rasterScissorValid;
	INT32 lastRasterScissorX;
	INT32 lastRasterScissorY;
	INT32 lastRasterScissorWidth;
	INT32 lastRasterScissorHeight;
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
	bool lastVerticalReconstruct[2];
	bool lastVertical;
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
	INT32 lastPackAlignment;
	INT32 lastPackRowLength;
	INT32 lastUnpackAlignment;
	INT32 lastUnpackRowLength;
	INT32 surfacePixelBytes;
	GLenum nativeReadFormat;
	GLenum nativeReadType;
	bool readRgb565Supported;
	bool readRgb888Supported;
	bool readBgraSupported;
	bool packSubimage;
	bool unpackSubimage;
	NamcosDiscardFramebufferProc discardFramebuffer;
	bool discardFramebufferVerified;
	const UINT32 *directPalette;
	UploadSlot uploadSlots[4];
	INT32 uploadSlotIndex;
	bool uploadModeValid;
	bool lastUploadRgb24;
	NamcosFrameUploadKey uploadFrameKey;
	UINT64 uploadRowGeneration[1024];
	bool uploadFrameValid;
	NamcosFrameOutputKey outputFrameKey;
	UINT64 outputRowGeneration[1024];
	bool outputFrameValid;
	bool fullRasterizerCapable;
	bool rasterVramSynchronized;
	bool rasterSampleValid;
	bool rasterRead5551;
	NamcosGlRasterUploadTracker rasterUploadTracker;
	NamcosPolyThreadPool *rasterThreadPool;
	NamcosGlRasterDirtyBounds rasterDirty;
	NamcosGlRasterDirtyBounds rasterSampleDirty;
	bool rasterStateActive;
	bool initialized;
	bool available;
	bool failed;
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
	bool HasHardwareVram() const { return false; }
	bool UploadVramRect(UINT16 *, UINT64, UINT64 *, INT32, INT32, INT32, INT32)
	{
		return false;
	}
	bool ReadVramRect(UINT16 *, UINT64, UINT64 *, INT32, INT32, INT32, INT32)
	{
		return false;
	}

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
