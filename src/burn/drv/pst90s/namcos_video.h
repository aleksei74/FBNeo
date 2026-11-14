#pragma once

struct NamcosFrameConvertContext {
	static const INT32 MAX_MAP_WIDTH = 2048;
	static const INT32 MAX_MAP_HEIGHT = 1024;

	const UINT16 *vram;
	UINT16 *output;
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
	INT32 mapped;
	const UINT16 *sourceXMap;
	const UINT16 *sourceYMap;
};

struct NamcosFrameMapCache {
	INT32 xOutputWidth;
	INT32 xSourceWidth;
	INT32 xOutputShift;
	INT32 yOutputHeight;
	INT32 ySourceHeight;
	INT32 yDisplayY;
	INT32 yCropTop;
	INT32 yCropHeight;
	UINT16 sourceXMap[NamcosFrameConvertContext::MAX_MAP_WIDTH];
	UINT16 sourceYMap[NamcosFrameConvertContext::MAX_MAP_HEIGHT];
};

static NamcosFrameMapCache NamcosFrameMaps = {};

static void NamcosFramePrepareMaps(NamcosFrameConvertContext *context)
{
	context->mapped = context->outputWidth > 0 && context->outputHeight > 0 &&
		context->outputWidth <= NamcosFrameConvertContext::MAX_MAP_WIDTH &&
		context->outputHeight <= NamcosFrameConvertContext::MAX_MAP_HEIGHT;
	context->sourceXMap = NULL;
	context->sourceYMap = NULL;

	if (!context->mapped) return;

	if (NamcosFrameMaps.xOutputWidth != context->outputWidth ||
		NamcosFrameMaps.xSourceWidth != context->sourceWidth ||
		NamcosFrameMaps.xOutputShift != context->outputShiftX) {
		for (INT32 x = 0; x < context->outputWidth; x++) {
			const INT32 shiftedX = x - context->outputShiftX;
			NamcosFrameMaps.sourceXMap[x] = shiftedX < 0 ? 0xffff :
				(UINT16)(((INT64)shiftedX * context->sourceWidth) / context->outputWidth);
		}
		NamcosFrameMaps.xOutputWidth = context->outputWidth;
		NamcosFrameMaps.xSourceWidth = context->sourceWidth;
		NamcosFrameMaps.xOutputShift = context->outputShiftX;
	}

	if (NamcosFrameMaps.yOutputHeight != context->outputHeight ||
		NamcosFrameMaps.ySourceHeight != context->sourceHeight ||
		NamcosFrameMaps.yDisplayY != context->displayY ||
		NamcosFrameMaps.yCropTop != context->cropTop ||
		NamcosFrameMaps.yCropHeight != context->cropHeight) {
		for (INT32 y = 0; y < context->outputHeight; y++) {
			INT32 sourceY = context->cropTop +
				(INT32)(((INT64)y * context->cropHeight) / context->outputHeight);
			NamcosFrameMaps.sourceYMap[y] = (UINT16)((context->displayY +
				((INT64)sourceY * context->sourceHeight) / context->outputHeight) & 0x3ff);
		}
		NamcosFrameMaps.yOutputHeight = context->outputHeight;
		NamcosFrameMaps.ySourceHeight = context->sourceHeight;
		NamcosFrameMaps.yDisplayY = context->displayY;
		NamcosFrameMaps.yCropTop = context->cropTop;
		NamcosFrameMaps.yCropHeight = context->cropHeight;
	}

	context->sourceXMap = NamcosFrameMaps.sourceXMap;
	context->sourceYMap = NamcosFrameMaps.sourceYMap;
}

static void NamcosFrameConvertRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosFrameConvertContext *context = (NamcosFrameConvertContext*)opaque;

	for (INT32 y = begin; y < end; y++) {
		UINT32 sy;
		if (context->mapped) {
			sy = context->sourceYMap[y];
		} else {
			INT32 sourceY = context->cropTop + (INT32)(((INT64)y * context->cropHeight) / context->outputHeight);
			sy = (context->displayY + ((INT64)sourceY * context->sourceHeight) / context->outputHeight) & 0x3ff;
		}
		UINT16 *destination = context->output + (y * context->outputWidth);
		if (context->rgb24) {
			for (INT32 x = 0; x < context->outputWidth; x++) {
				const INT32 shiftedX = x - context->outputShiftX;
				const UINT32 sourcePixel = context->mapped ? context->sourceXMap[x] :
					(shiftedX < 0 ? 0xffff :
					(UINT32)(((INT64)shiftedX * context->sourceWidth) / context->outputWidth));
				if (sourcePixel == 0xffff) {
					destination[x] = 0;
					continue;
				}
				const UINT32 sourceByte = sourcePixel * 3;
				const UINT32 sx = context->displayX + (sourceByte >> 1);
				const UINT16 word0 = context->vram[(sy << 10) | (sx & 0x3ff)];
				const UINT16 word1 = context->vram[(sy << 10) | ((sx + 1) & 0x3ff)];
				UINT8 r;
				UINT8 g;
				UINT8 b;

				if (sourceByte & 1) {
					r = word0 >> 8;
					g = word1 & 0xff;
					b = word1 >> 8;
				} else {
					r = word0 & 0xff;
					g = word0 >> 8;
					b = word1 & 0xff;
				}

				destination[x] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
			}
		} else {
			if (context->outputShiftX == 0 &&
				context->sourceWidth == context->outputWidth && context->displayX >= 0 &&
				context->displayX + context->outputWidth <= 1024) {
				const UINT16 *source = context->vram + (sy << 10) + context->displayX;
				for (INT32 x = 0; x < context->outputWidth; x++) {
					destination[x] = source[x] & 0x7fff;
				}
				continue;
			}

			for (INT32 x = 0; x < context->outputWidth; x++) {
				const INT32 shiftedX = x - context->outputShiftX;
				const UINT32 sourceX = context->mapped ? context->sourceXMap[x] :
					(shiftedX < 0 ? 0xffff :
					(UINT32)(((INT64)shiftedX * context->sourceWidth) / context->outputWidth));
				if (sourceX == 0xffff) {
					destination[x] = 0;
					continue;
				}
				const UINT32 sx = (context->displayX + sourceX) & 0x3ff;
				destination[x] = context->vram[(sy << 10) | sx] & 0x7fff;
			}
		}
	}
}

struct NamcosOutputTransferContext {
	const UINT16 *source;
	UINT8 *destination;
	const UINT32 *palette;
	INT32 width;
	INT32 pitch;
	INT32 bytesPerPixel;
};

static void NamcosOutputTransferRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosOutputTransferContext *context = (NamcosOutputTransferContext *)opaque;

	for (INT32 y = begin; y < end; y++) {
		const UINT16 *source = context->source + y * context->width;
		UINT8 *destination = context->destination + y * context->pitch;

		switch (context->bytesPerPixel) {
			case 2:
				for (INT32 x = 0; x < context->width; x++) {
					((UINT16 *)destination)[x] = (UINT16)context->palette[source[x]];
				}
				break;

			case 3:
				for (INT32 x = 0; x < context->width; x++) {
					const UINT32 color = context->palette[source[x]];
					destination[x * 3 + 0] = color & 0xff;
					destination[x * 3 + 1] = (color >> 8) & 0xff;
					destination[x * 3 + 2] = color >> 16;
				}
				break;

			case 4:
				for (INT32 x = 0; x < context->width; x++) {
					((UINT32 *)destination)[x] = context->palette[source[x]];
				}
				break;
		}
	}
}
