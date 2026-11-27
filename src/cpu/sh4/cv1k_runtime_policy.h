#ifndef FBNEO_CV1K_RUNTIME_POLICY_H
#define FBNEO_CV1K_RUNTIME_POLICY_H

#include <string.h>

// Native Linux ARM64 libretro builds use the CV1000 performance preset.
// Other platforms retain their previous opt-in policy.
#if defined(__aarch64__) && !defined(__AARCH64EB__) && defined(__linux__) && !defined(__ANDROID__) && defined(__LIBRETRO__)
static constexpr bool cv1kPerformanceDefault = true;
#else
static constexpr bool cv1kPerformanceDefault = false;
#endif

static inline bool Cv1kPerformanceOption(const char *value, bool fallback=cv1kPerformanceDefault)
{
	return value ? !strcmp(value,"1") : fallback;
}

#endif
