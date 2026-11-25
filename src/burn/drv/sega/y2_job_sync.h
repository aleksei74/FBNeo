// SPDX-License-Identifier: BSD-3-Clause
#ifndef FBNEO_Y2_JOB_SYNC_H
#define FBNEO_Y2_JOB_SYNC_H
#include <stdint.h>

namespace Y2JobSync {
enum Action { None, WaitRunning, DrainAll };
// Levels: 0 unused, 1 read by worker, 2 written by worker. A CPU
// read conflicts with a worker write; a CPU write conflicts with both.
inline bool Conflicts(uint8_t level, bool write) { return write ? level != 0 : level == 2; }
inline Action Plan(const uint8_t *pending, const uint8_t *running, uint32_t first, uint32_t last, bool write)
{
	bool wait = false;
	for (uint32_t page = first; page <= last; page++) {
		if (Conflicts(pending[page], write)) return DrainAll;
		wait |= Conflicts(running[page], write);
	}
	return wait ? WaitRunning : None;
}
}
#endif
