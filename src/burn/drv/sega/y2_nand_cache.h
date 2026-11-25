// SPDX-License-Identifier: BSD-3-Clause
#ifndef FBNEO_Y2_NAND_CACHE_H
#define FBNEO_Y2_NAND_CACHE_H
#include <stdint.h>
#include <stddef.h>

namespace Y2Nand {
struct Line {
	uint32_t tag, rank;
	uint8_t data[4096];
};

// External storage, 1-way legacy or 4-way LRU. NAND records are immutable
// during this driver's lifetime. No speculative disk accesses or new threads.
class Cache {
	Line *lines_ = nullptr, *last_ = nullptr;
	unsigned sets_ = 0, ways_ = 0;
	void Touch(Line *set, Line *line) {
		const unsigned old = line->rank;
		for (unsigned i=0; i<ways_; i++)
			if (set[i].rank < old) set[i].rank++;
		line->rank = 0;
		last_ = line;
	}
public:
	void Init(Line *lines, unsigned count, unsigned ways) {
		lines_ = lines; ways_ = ways; sets_ = count / ways; last_ = nullptr;
		for (unsigned i=0; i<count; i++) {
			lines[i].tag = UINT32_MAX;
			lines[i].rank = i % ways;
		}
	}
	unsigned Index(unsigned chip, uint32_t record) const {
		// Preserve the old direct-mapped layout for the comparison option.
		const uint32_t mixed = ways_ == 1 ? record : record ^ (record >> 11);
		return (mixed ^ (chip * (sets_ / 2))) & (sets_-1);
	}
	template<class Fill>
	uint8_t *Get(unsigned chip, uint32_t record, Fill fill, bool &evicted) {
		record &= 0x3ffff;
		const uint32_t tag = (chip << 18) | record;
		evicted = false;
		if (last_ && last_->tag == tag) return last_->data;
		Line *set = lines_ + Index(chip,record) * ways_;
		Line *victim = set;
		for (unsigned i=0; i<ways_; i++) {
			Line *line = set+i;
			if (line->tag == tag) { Touch(set,line); return line->data; }
			if (line->rank > victim->rank) victim=line;
		}
		evicted = victim->tag != UINT32_MAX;
		fill(victim,chip,record);
		victim->tag=tag;
		Touch(set,victim);
		return victim->data;
	}
};
}
#endif
