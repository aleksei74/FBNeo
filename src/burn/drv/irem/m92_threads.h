#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>
#endif

typedef void (*M92ThreadCallback)(void *context, INT32 begin, INT32 end);

static UINT32 M92DetectCores()
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

class M92ThreadPool
{
public:
	M92ThreadPool()
		: m_worker_count(0), m_generation(0), m_pending(0), m_stop(false),
		  m_count(0), m_parts(1), m_callback(NULL), m_context(NULL)
	{
	}

	~M92ThreadPool()
	{
		Shutdown();
	}

	void Configure()
	{
		Shutdown();

		const UINT32 cores = M92DetectCores();
		m_worker_count = (cores >= 4) ? (INT32)(cores - 1) : 0;
		if (m_worker_count > 7) m_worker_count = 7;

		m_generation = 0;
		m_stop = false;

		for (INT32 i = 0; i < m_worker_count; i++) {
			m_workers[i] = std::thread(&M92ThreadPool::Worker, this, i + 1);
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

	void ParallelFor(INT32 count, INT32 minimum, M92ThreadCallback callback, void *context)
	{
		const INT32 cores = m_worker_count + 1;
		INT32 parts = (minimum > 0) ? ((count + minimum - 1) / minimum) : cores;

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

private:
	void Worker(INT32 part)
	{
		UINT32 generation = 0;

		for (;;) {
			M92ThreadCallback callback;
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
	M92ThreadCallback m_callback;
	void *m_context;
};
