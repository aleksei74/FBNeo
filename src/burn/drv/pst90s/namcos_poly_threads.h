#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>
#endif

typedef void (*NamcosPolyThreadCallback)(void *context, INT32 begin, INT32 end);

class NamcosPolyThreadPool;

static INT64 NamcosPolyEstimateWork(const INT32 *px, const INT32 *py, INT32 points)
{
	INT64 area2 = ((INT64)px[1] - px[0]) * ((INT64)py[2] - py[0]) -
		((INT64)py[1] - py[0]) * ((INT64)px[2] - px[0]);
	if (area2 < 0) area2 = -area2;

	if (points == 4) {
		INT64 second = ((INT64)px[2] - px[1]) * ((INT64)py[3] - py[1]) -
			((INT64)py[2] - py[1]) * ((INT64)px[3] - px[1]);
		if (second < 0) second = -second;
		area2 += second;
	}

	return (area2 + 1) >> 1;
}

static UINT32 NamcosPolyAdvance(UINT32 value, INT32 step, INT32 rows)
{
	return value + (UINT32)((INT64)step * rows);
}

struct NamcosFrameConvertContext {
	static const INT32 MAX_MAP_WIDTH = 2048;
	static const INT32 MAX_MAP_HEIGHT = 1024;

	const UINT16 *vram;
	UINT64 vramGeneration;
	const UINT64 *vramRowGeneration;
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
	INT32 verticalReconstruct2x;
	INT32 rgb24;
	INT32 vertical;
	INT32 allowOutputReuse;
	NamcosPolyThreadPool *threadPool;
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
	NamcosOutputTransferContext *context = (NamcosOutputTransferContext*)opaque;

	for (INT32 y = begin; y < end; y++) {
		const UINT16 *source = context->source + y * context->width;
		UINT8 *destination = context->destination + y * context->pitch;

		switch (context->bytesPerPixel) {
			case 2:
				for (INT32 x = 0; x < context->width; x++) {
					((UINT16*)destination)[x] = (UINT16)context->palette[source[x]];
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
					((UINT32*)destination)[x] = context->palette[source[x]];
				}
				break;
		}
	}
}

static UINT32 NamcosPolyDetectCores()
{
	UINT32 cores = 0;

#if defined(_WIN32)
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	cores = (UINT32)info.dwNumberOfProcessors;
#elif defined(__linux__) || defined(__ANDROID__)
	const long online = sysconf(_SC_NPROCESSORS_ONLN);
	if (online > 0) cores = (UINT32)online;
#endif

	if (cores == 0) cores = std::thread::hardware_concurrency();
	if (cores == 0) cores = 1;

	return cores;
}

class NamcosPolyThreadPool
{
public:
	NamcosPolyThreadPool()
		: m_worker_count(0), m_generation(0), m_pending(0), m_stop(false),
		  m_count(0), m_parts(1), m_callback(NULL), m_context(NULL)
	{
	}

	~NamcosPolyThreadPool()
	{
		Shutdown();
	}

	void Configure(INT32 forceSingle = 0)
	{
		Shutdown();

		const UINT32 cores = NamcosPolyDetectCores();
		if (forceSingle) {
			m_worker_count = 0;
		} else if (cores >= 12) {
			m_worker_count = 7;
		} else if (cores >= 8) {
			m_worker_count = 5;
		} else {
			m_worker_count = (cores >= 4) ? 3 : 0;
		}
		m_generation = 0;
		m_stop = false;

		for (INT32 i = 0; i < m_worker_count; i++) {
			m_workers[i] = std::thread(&NamcosPolyThreadPool::Worker, this, i + 1);
		}
	}

	void Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_stop = true;
			m_generation++;
		}
		m_work.notify_all();

		for (INT32 i = 0; i < m_worker_count; i++) {
			if (m_workers[i].joinable()) m_workers[i].join();
		}

		m_worker_count = 0;
		m_pending = 0;
		m_parts = 1;
		m_callback = NULL;
		m_context = NULL;
	}

	INT32 Cores() const
	{
		return m_worker_count + 1;
	}

	void ParallelFor(INT32 count, INT32 minimum, NamcosPolyThreadCallback callback, void *context)
	{
		const INT32 cores = Cores();
		INT32 parts = (minimum > 0) ? (count / minimum) : cores;
		Dispatch(count, parts, callback, context);
	}

	void ParallelForWork(INT32 count, INT64 work, INT32 minimumWork, NamcosPolyThreadCallback callback, void *context)
	{
		const INT32 cores = Cores();
		const INT64 requested = (minimumWork > 0) ? (work / minimumWork) : cores;
		INT32 parts = (requested > cores) ? cores : (INT32)requested;
		Dispatch(count, parts, callback, context);
	}

private:
	void Dispatch(INT32 count, INT32 parts, NamcosPolyThreadCallback callback, void *context)
	{
		const INT32 cores = Cores();
		if (parts > cores) parts = cores;
		if (parts > count) parts = count;
		if (parts < 2) {
			callback(context, 0, count);
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_count = count;
			m_parts = parts;
			m_callback = callback;
			m_context = context;
			m_pending = parts - 1;
			m_generation++;
		}
		m_work.notify_all();

		callback(context, 0, count / parts);

		std::unique_lock<std::mutex> lock(m_mutex);
		m_done.wait(lock, [this]() { return m_pending == 0; });
	}

	void Worker(INT32 part)
	{
		UINT32 generation = 0;

		for (;;) {
			NamcosPolyThreadCallback callback;
			void *context;
			INT32 begin;
			INT32 end;

			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_work.wait(lock, [this, generation]() { return m_stop || m_generation != generation; });
				if (m_stop) return;

				generation = m_generation;
				if (part >= m_parts) continue;
				begin = (m_count * part) / m_parts;
				end = (m_count * (part + 1)) / m_parts;
				callback = m_callback;
				context = m_context;
			}

			callback(context, begin, end);

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_pending--;
				if (m_pending == 0) m_done.notify_one();
			}
		}
	}

	std::thread m_workers[7];
	std::mutex m_mutex;
	std::condition_variable m_work;
	std::condition_variable m_done;
	INT32 m_worker_count;
	UINT32 m_generation;
	INT32 m_pending;
	bool m_stop;
	INT32 m_count;
	INT32 m_parts;
	NamcosPolyThreadCallback m_callback;
	void *m_context;
};
