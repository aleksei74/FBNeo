// Acclaim RAX sound daughterboard used by Batman Forever.
#include "adsp2100_intf.h"
#include "adsp2100/adsp2100.h"

namespace StvRax
{
enum { Clock = 16670000, RomSize = 0x1000000, QueueSize = 2048 };
enum { DmaInternal = 1, DmaExternal, DmaControl, DmaCount, PortFlags,
	SportAutobuf = 19, SportClock = 21, SystemControl = 31 };

static UINT8* Rom;
static struct {
	UINT32 program[0x4000];
	UINT16 external[5][0x2000];
	UINT16 internal[0x1fe0];
	UINT16 regs[32];
	UINT16 hostIn, hostOut, flags, romBank, ramBank;
	INT32 index, stride, length, base, samplePeriod;
	INT32 dmaEndAddress, read, queued, phase;
	INT64 origin, dmaDue, sportDue;
	INT16 pcm[QueueSize][2];
} Board;

static INT64 Now()
{
	return Board.origin + Adsp2100TotalCycles();
}

static UINT16& Data(UINT32 address)
{
	if (address >= 0x2000) return Board.internal[address - 0x2000];
	const INT32 bank = Adsp2100GetState()->dmovlay == 1 ? 1 + Board.ramBank : 0;
	return Board.external[bank][address];
}

static UINT16 ReadData(UINT32 address)
{
	if (address < 0x3fe0) return Data(address);
	const UINT32 reg = address - 0x3fe0;
	return reg == PortFlags ? Board.flags : Board.regs[reg];
}

static void WriteData(UINT32 address, UINT16 value);

static void StartDma()
{
	const UINT16 control = Board.regs[DmaControl];
	const INT32 type = control & 3;
	const INT32 bytes = type == 0 ? 3 : type == 1 ? 2 : 1;
	const INT32 count = Board.regs[DmaCount];
	UINT32 source = ((control >> 8) << 14) | Board.regs[DmaExternal];
	UINT32 destination = Board.regs[DmaInternal];

	// This board wires byte memory to ROM. It cannot store DSP writes.
	for (INT32 i = 0; i < count && !(control & 4); i++, destination++, source += bytes) {
		const UINT32 offset = (UINT32(Board.romBank) << 22) + source;
		UINT32 word = 0;
		for (INT32 b = 0; b < bytes; b++) {
			word <<= 8;
			if (offset + b < RomSize) word |= Rom[offset + b];
		}
		const UINT32 target = destination & 0x3fff;
		if (type == 0) Board.program[target] = word;
		else if (target < 0x3fe0) Data(target) = word << (type == 2 ? 8 : 0);
	}
	if (control & 4) {
		destination += count;
		source += count * bytes;
	}
	Board.regs[DmaInternal] = destination & 0x3fff;
	Board.dmaEndAddress = source;
	Board.dmaDue = Now() + count;
	Adsp2100RunEnd();
}

static void WriteData(UINT32 address, UINT16 value)
{
	if (address < 0x3fe0) {
		Data(address) = value;
		return;
	}
	const UINT32 reg = address - 0x3fe0;
	switch (reg) {
		case DmaInternal:
		case DmaExternal: value &= 0x3fff; break;
		case DmaControl: value &= 0xff0f; break;
		case DmaCount: value &= 0x3fff; break;
		case SportAutobuf: if (!(value & 2)) Board.sportDue = -1; break;
		case SystemControl: if (!(value & 0x1000)) Board.sportDue = -1; break;
	}
	Board.regs[reg] = value;
	if (reg == DmaCount && value) StartDma();
}

static UINT16 ReadIo(UINT32 address)
{
	if (address != 3) return 0xffff;
	Adsp2100SetIRQLine(ADSP2181_IRQL2, CPU_IRQSTATUS_NONE); // Active-low IRQL0 on the PCB.
	return Board.hostIn;
}

static void WriteIo(UINT32 address, UINT16 value)
{
	switch (address) {
		case 0: Board.ramBank = value & 3; break;
		case 1: Board.romBank = value & 7; break;
		case 3: Board.hostOut = value; Board.flags = 0; break;
	}
}

static INT32 TransferFrames()
{
	return Board.length / (8 * Board.stride);
}

static void SportTransmit(INT32 port, INT32)
{
	if (port != 0) return;
	Board.sportDue = -1;
	const UINT16 control = Board.regs[SportAutobuf];
	if (!(Board.regs[SystemControl] & 0x1000) || !(control & 2)) return;
	auto* dsp = Adsp2100GetState();
	Board.index = (control >> 9) & 7;
	Board.stride = dsp->m[((control >> 7) & 3) | (Board.index & 4)];
	Board.length = dsp->l[Board.index];
	if (Board.stride <= 0 || !TransferFrames()) return;

	Board.base = (dsp->i[Board.index] - Board.stride) & 0x3fff;
	dsp->i[Board.index] = Board.base;
	Board.samplePeriod = 16 * (Board.regs[SportClock] + 1);
	Board.sportDue = Now() + INT64(Board.samplePeriod) * TransferFrames();
	Adsp2100RunEnd();
}

static void TransferAudio()
{
	auto* dsp = Adsp2100GetState();
	INT32 index = dsp->i[Board.index];
	const INT32 frames = TransferFrames();
	for (INT32 i = 0; i < frames; i++) {
		const INT16 left = ReadData(index & 0x3fff);
		index += Board.stride;
		const INT16 right = ReadData(index & 0x3fff);
		index += Board.stride;
		if (Board.queued == QueueSize) {
			Board.read = (Board.read + 1) % QueueSize;
			Board.queued--;
		}
		const INT32 write = (Board.read + Board.queued++) % QueueSize;
		Board.pcm[write][0] = left;
		Board.pcm[write][1] = right;
	}
	dsp->i[Board.index] = index >= Board.base + Board.length ? Board.base : index & 0x3fff;
	Board.sportDue += INT64(Board.samplePeriod) * frames;
}

static INT32 Init()
{
	Rom = (UINT8*)BurnMalloc(RomSize);
	if (!Rom) return 1;
	static const INT32 locations[] = { 0, 0x400000, 0x600000, 0x800000, 0xa00000 };
	for (INT32 i = 0; i < 5; i++) {
		if (BurnLoadRom(Rom + locations[i], 9 + i, 1)) {
			BurnFree(Rom);
			return 1;
		}
	}
	Adsp2181Init();
	auto* dsp = Adsp2100GetState();
	dsp->io_read = ReadIo;
	dsp->io_write = WriteIo;
	Adsp2100MapMemory((UINT8*)Board.program, 0, 0x3fff, MAP_RAM);
	Adsp2100MapDataHandler(0, 0, 0x3fff, MAP_RAM);
	Adsp2100SetReadDataWordHandler(0, ReadData);
	Adsp2100SetWriteDataWordHandler(0, WriteData);
	Adsp2100SetTxCallback(SportTransmit);
	return 0;
}

static void Reset()
{
	if (!Rom) return;
	memset(&Board, 0, sizeof(Board));
	Board.flags = 1;
	Board.dmaDue = Board.sportDue = -1;
	for (INT32 i = 0; i < 32; i++)
		Board.program[i] = (Rom[3*i] << 16) | (Rom[3*i+1] << 8) | Rom[3*i+2];
	Adsp2100Reset();
	Adsp2100NewFrame();
}

static void Exit()
{
	if (!Rom) return;
	Adsp2100Exit();
	BurnFree(Rom);
}

static void NewFrame()
{
	Board.origin = Now();
	Adsp2100NewFrame();
}

static void RunTo(INT32 cycles)
{
	const INT64 target = Board.origin + cycles;
	for (;;) {
		const INT64 now = Now();
		if (Board.dmaDue >= 0 && Board.dmaDue <= now) {
			Board.dmaDue = -1;
			Board.regs[DmaCount] = 0;
			Board.regs[DmaExternal] = Board.dmaEndAddress & 0x3fff;
			Board.regs[DmaControl] = (Board.regs[DmaControl] & 0xff) | ((Board.dmaEndAddress >> 6) & 0xff00);
			if (Board.regs[DmaControl] & 8) Adsp2100Reset();
			else Adsp2181BdmaComplete(Adsp2100GetState());
		}
		if (Board.sportDue >= 0 && Board.sportDue <= now) TransferAudio();
		if (now >= target) break;
		INT64 next = target;
		if (Board.dmaDue >= 0 && Board.dmaDue < next) next = Board.dmaDue;
		if (Board.sportDue >= 0 && Board.sportDue < next) next = Board.sportDue;
		Adsp2100Run(INT32(next - now));
	}
}

static void Command(UINT16 value, INT32 cycles)
{
	RunTo(cycles);
	Board.hostIn = value;
	Adsp2100SetIRQLine(ADSP2181_IRQL2, CPU_IRQSTATUS_ACK);
}

static void Mix()
{
	if (!Board.samplePeriod || nBurnSoundRate <= 0) return;
	const INT32 denominator = Board.samplePeriod * nBurnSoundRate;
	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		if (Board.queued == 0) {
			Board.phase = 0;
			continue;
		}
		const INT32 next = Board.queued > 1 ? (Board.read + 1) % QueueSize : Board.read;
		if (pBurnSoundOut) {
			for (INT32 ch = 0; ch < 2; ch++) {
				const INT32 first = Board.pcm[Board.read][ch];
				const INT32 diff = Board.pcm[next][ch] - first;
				const INT32 sample = first + INT32(INT64(diff) * Board.phase / denominator);
				pBurnSoundOut[2*i+ch] = BURN_SND_CLIP(pBurnSoundOut[2*i+ch] + sample);
			}
		}
		const INT64 phase = INT64(Board.phase) + Clock;
		const INT32 advance = std::min(Board.queued, INT32(phase / denominator));
		Board.phase = INT32(phase % denominator);
		Board.queued -= advance;
		Board.read = (Board.read + advance) % QueueSize;
	}
}

static void Scan(INT32 nAction)
{
	SCAN_VAR(Board);
	Adsp2100Scan(nAction);
	auto* dsp = Adsp2100GetState();
}
}
