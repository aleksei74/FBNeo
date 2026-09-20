#pragma once
#include <algorithm>

template<int Capacity, typename Object>
static inline void GxSortObjects(INT32 *indices, INT32 count, const Object *objects)
{
	// Key materialization only pays for sufficiently large object lists.
	if (count < 512) {
		std::sort(indices, indices + count, [objects](INT32 lhs, INT32 rhs) {
			const UINT32 a = (UINT32)objects[lhs].order;
			const UINT32 b = (UINT32)objects[rhs].order;
			return a != b ? a > b : lhs > rhs;
		});
		return;
	}
	UINT64 keys[Capacity];
	for (INT32 i = 0; i < count; i++) {
		const UINT32 index = (UINT32)indices[i];
		keys[i] = ((UINT64)(UINT32)objects[index].order << 32) | index;
	}
	const auto less = [](UINT64 lhs, UINT64 rhs) { return lhs > rhs; };
	UINT64 *first = std::is_sorted_until(keys, keys + count, less);
	if (keys + count - first > 8) {
		std::sort(keys, keys + count, less);
	} else {
		// Only a short tail is disordered; preserve the sorted prefix.
		for (UINT64 *current = first; current != keys + count; ++current) {
			const UINT64 value = *current;
			UINT64 *position = std::upper_bound(keys, current, value, less);
			std::move_backward(position, current, current + 1);
			*position = value;
		}
	}
	for (INT32 i = 0; i < count; i++) indices[i] = (INT32)(UINT32)keys[i];
}
