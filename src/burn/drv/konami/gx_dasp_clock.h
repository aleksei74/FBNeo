#pragma once

// Derived host-rate data only; the emulated fractional cycle counter stays
// in the driver and remains part of save states.
class GxDaspClock
{
public:
	GxDaspClock() : sample_rate(0), step(250U << 16) {}
	UINT32 Step(INT32 rate)
	{
		if (rate != sample_rate) {
			step = rate ? (UINT32)(((UINT64)12000000 << 16) / rate) : (250U << 16);
			sample_rate = rate;
		}
		return step;
	}
private:
	INT32 sample_rate;
	UINT32 step;
};
