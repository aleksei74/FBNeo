#pragma once

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

typedef void (*NamcosPolyThreadCallback)(void *context, INT32 begin, INT32 end);

class NamcosPolyThreadPool
{
public:
	NamcosPolyThreadPool()
		: m_worker_count(0), m_generation(0), m_pending(0), m_stop(false),
		  m_count(0), m_callback(NULL), m_context(NULL)
	{
	}

	~NamcosPolyThreadPool()
	{
		Shutdown();
	}

	void Configure()
	{
		Shutdown();

		UINT32 cores = std::thread::hardware_concurrency();
		if (cores == 0) cores = 1;
		cores = std::min<UINT32>(cores, 4);
		m_worker_count = (INT32)cores - 1;
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
		if (cores <= 1 || count < minimum) {
			callback(context, 0, count);
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_count = count;
			m_callback = callback;
			m_context = context;
			m_pending = m_worker_count;
			m_generation++;
		}
		m_work.notify_all();

		callback(context, 0, count / cores);

		std::unique_lock<std::mutex> lock(m_mutex);
		m_done.wait(lock, [this]() { return m_pending == 0; });
	}

private:
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
				const INT32 cores = m_worker_count + 1;
				begin = (m_count * part) / cores;
				end = (m_count * (part + 1)) / cores;
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

	std::thread m_workers[3];
	std::mutex m_mutex;
	std::condition_variable m_work;
	std::condition_variable m_done;
	INT32 m_worker_count;
	UINT32 m_generation;
	INT32 m_pending;
	bool m_stop;
	INT32 m_count;
	NamcosPolyThreadCallback m_callback;
	void *m_context;
};
