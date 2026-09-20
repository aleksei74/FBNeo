#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <new>
#include <system_error>

class GxRangeWorkerPool
{
public:
	typedef void (*Task)(INT32 start, INT32 end);

	GxRangeWorkerPool()
		: active(false), stop(false), generation(0), completed(0), worker_count(0),
		  task(NULL)
	{
	}

	~GxRangeWorkerPool()
	{
		Shutdown();
	}

	void Configure(UINT32 cores)
	{
		Shutdown();

		if (cores < 4) return;

		worker_count = cores > MaxWorkers ? MaxWorkers : (INT32)(cores - 1);

		{
			std::lock_guard<std::mutex> lock(mutex);
			active = true;
			stop = false;
			generation = 0;
			completed = 0;
		}

		try {
			for (INT32 i = 0; i < worker_count; i++) {
				workers[i] = std::thread(&GxRangeWorkerPool::Worker, this, i);
			}
		} catch (const std::system_error&) {
			// Run() falls back to the caller after partially created workers are joined.
			Shutdown();
		} catch (const std::bad_alloc&) {
			Shutdown();
		}
	}

	void Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (!active && !workers[0].joinable()) return;
			stop = true;
			generation++;
		}
		work.notify_all();

		for (INT32 i = 0; i < MaxWorkers; i++) {
			if (workers[i].joinable()) workers[i].join();
		}

		std::lock_guard<std::mutex> lock(mutex);
		active = false;
		stop = false;
		completed = 0;
		worker_count = 0;
		task = NULL;
	}

	bool Run(Task next_task, INT32 next_items)
	{
		if (!active || next_task == NULL || next_items < worker_count + 1) return false;

		{
			std::lock_guard<std::mutex> lock(mutex);
			task = next_task;
			// Generate floor(i * next_items / parts) once, without multiplying large counts.
			const INT32 parts = worker_count + 1;
			const INT32 quotient = next_items / parts;
			const INT32 remainder = next_items % parts;
			INT32 error = 0;
			boundaries[0] = 0;
			for (INT32 i = 1; i <= parts; ++i) {
				boundaries[i] = boundaries[i - 1] + quotient;
				error += remainder;
				if (error >= parts) {
					++boundaries[i];
					error -= parts;
				}
			}
			// The mutex publishes this reset before workers observe the new generation.
			completed.store(0, std::memory_order_relaxed);
			generation++;
		}
		work.notify_all();

		next_task(0, boundaries[1]);

		// The caller may finish last; acquire still publishes every worker's writes.
		if (completed.load(std::memory_order_acquire) == worker_count) return true;

		std::unique_lock<std::mutex> lock(mutex);
		done.wait(lock, [this]() { return completed.load(std::memory_order_acquire) == worker_count; });

		return true;
	}

private:
	enum { MaxWorkers = 7 };

	void Worker(INT32 index)
	{
		UINT32 observed = 0;

		for (;;) {
			Task current_task;
			INT32 start, end;

			{
				std::unique_lock<std::mutex> lock(mutex);
				work.wait(lock, [this, observed]() {
					return stop || generation != observed;
				});
				if (stop) return;

				observed = generation;
				current_task = task;
				start = boundaries[index + 1];
				end = boundaries[index + 2];
			}

			current_task(start, end);

			// Acquire/release chains all workers' writes before Run() returns.
			if (completed.fetch_add(1, std::memory_order_acq_rel) + 1 == worker_count) {
				// Pair with the wait mutex so completion cannot lose a wakeup.
				std::lock_guard<std::mutex> lock(mutex);
				done.notify_one();
			}
		}
	}

	std::thread workers[MaxWorkers];
	std::mutex mutex;
	std::condition_variable work;
	std::condition_variable done;
	bool active;
	bool stop;
	UINT32 generation;
	std::atomic<INT32> completed;
	INT32 worker_count;
	Task task;
	INT32 boundaries[MaxWorkers + 2];
};
