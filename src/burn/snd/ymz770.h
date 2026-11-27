void ymz770_init(UINT8 *rom, INT32 rom_length);
void ymz774_init(UINT8 *rom, INT32 rom_length); // ymz774-specific!
void ymz770_exit();
void ymz770_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);
void ymz770_set_volume(double dVolume);

void ymz770_write(INT32 offset, UINT8 data);
UINT8 ymz774_read(INT32 offset); // ymz774-specific!

void ymz770_scan(INT32 nAction, INT32 *pnMin);
void ymz770_update(INT16 *output, INT32 samples_len);
void ymz770_reset();

// Host-only diagnostics. Counters are never part of the saved device state.
struct Ymz770PerfStats {
	UINT64 writeNs,streamNs,decodeNs,decodePeakNs;
	UINT64 writes,streamSamples,decodes,emptyDecodes,decodedSamples,keyOns;
	UINT64 syncBits,syncMaxBits,peakSyncBits;
	UINT32 peakPhrase,peakChannel;
};
void ymz770_perf_enable(bool enabled);
void ymz770_perf_reset();
void ymz770_perf_get(Ymz770PerfStats *stats);
