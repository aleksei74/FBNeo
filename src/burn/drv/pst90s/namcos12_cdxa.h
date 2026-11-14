// license:BSD-3-Clause
// Copyright (c) windyfairy (MAME device implementations); FB Neo contributors.
// Namco System 12 CDXA PCB. Reference: MAME 0.289 namcos12_cdxa.cpp,
// sh7014*.cpp (BSD-3-Clause, copyright windyfairy), ATA/T10 MMC devices.
// The SH7014 executes the program downloaded by the PSX; it is not replaced
// with game-specific command responses. All devices run on the emulation thread.
#include "sh2_intf.h"
#include "burn_hdd.h"
#include "../../../intf/cd/cd_chd.h"
#include <stdarg.h>
#include <math.h>

static ChdImage *CdxaDisc;
static UINT8 *CdxaRAM;
static FILE *CdxaLog;
static void CdxaLogf(const char *fmt, ...)
{
	if (!CdxaLog)
		return;
	va_list args;
	va_start(args, fmt);
#ifdef __LIBRETRO__
	filestream_vprintf(CdxaLog, fmt, args);
#else
	vfprintf(CdxaLog, fmt, args);
#endif
	va_end(args);
	fflush(CdxaLog);
}
struct CdxaState {
	UINT8 running, sram, ideSh, idePs, irqBusy;
	UINT8 ata[8], packet[12], sense, asc;
	UINT8 data[65536];
	UINT32 pos, length, chunkEnd, limit, lba, sectors, sectorSize;
	UINT8 packetPos, packetMode, streaming, rawFlags, readRaw;
	UINT8 regs[0x8000], pending[256];
	INT32 irqVector, irqLevel, sciCount;
	UINT64 serialFrac, dacFrac;
	UINT32 frameCycles, audioTime[2048];
	INT16 dacSample[2], lastSample[2];
	INT32 gain[2];
	UINT32 audioWord, audioBytes, audioFrames;
	UINT32 clockRegs[4], clockHz, clockCmd;
	INT32 clockTimeout, cycleExtra;
	UINT8 clockData, clockPrev, clockPin, clockState, clockUnlock, clockBit, clockSelect;
	UINT8 clockPrescale[4];
	INT16 audio[4096];
	UINT32 frames, reads;
	UINT32 timerFrac[3];
	UINT8 volControl, volPhase;
	UINT16 volLatch[4];
	INT32 attenuation;
};
static CdxaState Cdxa;
static UINT16 CdxaReg16(UINT32 a)
{
	a &= 0x7fff;
	return (Cdxa.regs[a] << 8) | Cdxa.regs[a + 1];
}
static UINT32 CdxaReg32(UINT32 a)
{
	return (UINT32(CdxaReg16(a)) << 16) | CdxaReg16(a + 2);
}
static void CdxaRegSet32(UINT32 a, UINT32 v)
{
	a &= 0x7fff;
	for (INT32 i = 3; i >= 0; i--) {
		Cdxa.regs[a + i] = v;
		v >>= 8;
	}
}
static UINT32 CdxaBE32(const UINT8 *p)
{
	return (UINT32(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
static void CdxaPut32(UINT8 *p, UINT32 v)
{
	for (INT32 i = 3; i >= 0; i--) {
		p[i] = v;
		v >>= 8;
	}
}
static void CdxaComplete()
{
	Cdxa.ata[7] = 0x40;
	Cdxa.ata[2] = 3;
	Cdxa.ata[4] = Cdxa.ata[5] = 0;
	Cdxa.length = Cdxa.pos = 0;
	Cdxa.packetMode = 0;
	Cdxa.streaming = 0;
}
static void CdxaError(UINT8 sense, UINT8 asc)
{
	CdxaComplete();
	Cdxa.sense = sense;
	Cdxa.asc = asc;
	Cdxa.ata[1] = (sense << 4) | 4;
	Cdxa.ata[7] |= 1;
}
static void CdxaDataPhase()
{
	UINT32 n = Cdxa.length - Cdxa.pos;
	if (n > Cdxa.limit)
		n = Cdxa.limit;
	Cdxa.chunkEnd = Cdxa.pos + n;
	Cdxa.ata[4] = n;
	Cdxa.ata[5] = n >> 8;
	Cdxa.ata[2] = 2;
	Cdxa.ata[7] = 0x48;
}
static bool CdxaReadNext()
{
	if (!Cdxa.sectors) {
		CdxaComplete();
		return false;
	}
	UINT8 raw[2352];
	INT32 result;
	if (!Cdxa.readRaw)
		result = ChdReadSector(CdxaDisc, Cdxa.lba, CHD_TRACK_MODE1, Cdxa.data);
	else {
		result = ChdReadSector(CdxaDisc, Cdxa.lba, CHD_TRACK_MODE2_RAW, raw);
		if (!result) {
			UINT32 n = 0;
			UINT8 flags = Cdxa.rawFlags;
			if (flags & 0x80) {
				memcpy(Cdxa.data + n, raw, 12);
				n += 12;
			}
			UINT8 header = (flags >> 5) & 3;
			if (header == 1 || header == 3) {
				memcpy(Cdxa.data + n, raw + 12, 4);
				n += 4;
			}
			if (header == 2 || header == 3) {
				memcpy(Cdxa.data + n, raw + 16, 8);
				n += 8;
			}
			if (flags & 0x10) {
				UINT32 bytes = (raw[15] == 2 && (raw[18] & 0x20)) ? 2328 : 2048;
				UINT32 off = raw[15] == 2 ? 24 : 16;
				memcpy(Cdxa.data + n, raw + off, bytes);
				n += bytes;
			}
			if ((flags & 8) && !(raw[15] == 2 && (raw[18] & 0x20))) {
				UINT32 off = raw[15] == 2 ? ((raw[18] & 0x20) ? 2348 : 2072) : 2064;
				memcpy(Cdxa.data + n, raw + off, 2352 - off);
				n += 2352 - off;
			}
			Cdxa.sectorSize = n;
		}
	}
	if (result) {
		CdxaLogf("CD read error LBA=%u\n", Cdxa.lba);
		CdxaError(3, 0x11);
		return false;
	}
	Cdxa.lba++;
	Cdxa.sectors--;
	Cdxa.reads++;
	Cdxa.pos = 0;
	Cdxa.length = Cdxa.sectorSize;
	CdxaDataPhase();
	return true;
}
static void CdxaReply(UINT32 n)
{
	Cdxa.pos = 0;
	Cdxa.length = n;
	if (n)
		CdxaDataPhase();
	else
		CdxaComplete();
}
static void CdxaPacket()
{
	const UINT8 *p = Cdxa.packet;
	Cdxa.packetMode = 0;
	Cdxa.streaming = 0;
	memset(Cdxa.data, 0, sizeof(Cdxa.data));
	CdxaLogf("CDB %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
	switch (p[0]) {
		case 0x00:
		case 0x1b:
		case 0x1e:
		case 0xbb:
			CdxaComplete();
			break;
		case 0x2b:
			// MAME atapicdr: Truck explicitly waits for Drive Seek Complete.
			CdxaComplete();
			Cdxa.ata[7] |= 0x10;
			break;
		case 0x03:
			Cdxa.data[0] = 0x70;
			Cdxa.data[2] = Cdxa.sense;
			Cdxa.data[7] = 10;
			Cdxa.data[12] = Cdxa.asc;
			CdxaReply(p[4] < 18 ? p[4] : 18);
			Cdxa.sense = Cdxa.asc = 0;
			break;
		case 0x12:
			Cdxa.data[0] = 5;
			Cdxa.data[1] = 0x80;
			Cdxa.data[2] = 0;
			Cdxa.data[3] = 0x21;
			Cdxa.data[4] = 31;
			memcpy(Cdxa.data + 8, "TOSHIBA CD-ROM XM-6402B 1015", 28);
			CdxaReply(p[4] < 36 ? p[4] : 36);
			break;
		case 0x25:
			CdxaPut32(Cdxa.data, ChdGetTotalFrames(CdxaDisc) - 1);
			CdxaPut32(Cdxa.data + 4, 2048);
			CdxaReply(8);
			break;
		case 0x28:
		case 0xa8:
		case 0xbe:
			Cdxa.lba = CdxaBE32(p + 2);
			Cdxa.sectors = p[0] == 0xa8 ? CdxaBE32(p + 6) : (p[0] == 0xbe ? ((p[6] << 16) | (p[7] << 8) | p[8]) : ((p[7] << 8) | p[8]));
			Cdxa.readRaw = p[0] == 0xbe;
			Cdxa.sectorSize = Cdxa.readRaw ? 2352 : 2048;
			Cdxa.rawFlags = p[9];
			Cdxa.streaming = 1;
			CdxaReadNext();
			break;
		case 0x43: {
			INT32 tracks = ChdGetNumTracks(CdxaDisc);
			UINT32 n = 4;
			Cdxa.data[2] = 1;
			Cdxa.data[3] = tracks;
			for (INT32 t = 0; t <= tracks; t++) {
				INT32 tn = t == tracks ? 0xaa : t + 1;
				if (p[6] && tn < p[6])
					continue;
				const ChdTrack *tr = t < tracks ? ChdGetTrack(CdxaDisc, t) : NULL;
				UINT32 lba = tr ? tr->nIndex1LBA : ChdGetTotalFrames(CdxaDisc);
				Cdxa.data[n + 1] = tr ? ((tr->nControl << 4) | (tr->nControl >> 4)) : 0x14;
				Cdxa.data[n + 2] = tn;
				if (p[1] & 2) {
					lba += 150;
					Cdxa.data[n + 5] = lba / (75 * 60);
					Cdxa.data[n + 6] = (lba / 75) % 60;
					Cdxa.data[n + 7] = lba % 75;
				} else
					CdxaPut32(Cdxa.data + n + 4, lba);
				n += 8;
			}
			Cdxa.data[0] = (n - 2) >> 8;
			Cdxa.data[1] = n - 2;
			UINT32 alloc = (p[7] << 8) | p[8];
			CdxaReply(n < alloc ? n : alloc);
			break;
		}
		case 0x5a: {
			Cdxa.data[1] = 0x1a;
			Cdxa.data[8] = 0x2a;
			Cdxa.data[9] = 0x12;
			Cdxa.data[10] = 3;
			Cdxa.data[12] = 0x71;
			UINT32 n = (p[7] << 8) | p[8];
			CdxaReply(n < 28 ? n : 28);
			break;
		}
		default:
			CdxaLogf("Unsupported CDB %02x\n", p[0]);
			CdxaError(5, 0x20);
			break;
	}
}
static UINT16 CdxaAtaRead(INT32 reg)
{
	if (reg)
		return Cdxa.ata[reg & 7];
	if (!(Cdxa.ata[7] & 8) || Cdxa.packetMode)
		return 0xffff;
	UINT16 v = Cdxa.data[Cdxa.pos++];
	if (Cdxa.pos < Cdxa.length)
		v |= Cdxa.data[Cdxa.pos++] << 8;
	if (Cdxa.pos >= Cdxa.length) {
		if (Cdxa.streaming)
			CdxaReadNext();
		else
			CdxaComplete();
	} else if (Cdxa.pos >= Cdxa.chunkEnd)
		CdxaDataPhase();
	return v;
}
static void CdxaAtaWrite(INT32 reg, UINT16 v)
{
	if (reg == 0) {
		if (Cdxa.packetMode && Cdxa.packetPos < 12) {
			Cdxa.packet[Cdxa.packetPos++] = v;
			Cdxa.packet[Cdxa.packetPos++] = v >> 8;
			if (Cdxa.packetPos == 12)
				CdxaPacket();
		}
		return;
	}
	if (reg != 7) {
		Cdxa.ata[reg & 7] = v;
		return;
	}
	CdxaLogf("ATA %02x pc=%08x\n", v, Sh2GetPC(0));
	Cdxa.ata[1] = 0;
	Cdxa.limit = (Cdxa.ata[5] << 8) | Cdxa.ata[4];
	if (!Cdxa.limit)
		Cdxa.limit = 0xfffe;
	if (v == 0xa0) {
		Cdxa.packetMode = 1;
		Cdxa.packetPos = 0;
		Cdxa.ata[7] = 0x48;
		Cdxa.ata[2] = 1;
	} else if (v == 0xa1) {
		memset(Cdxa.data, 0, 512);
		Cdxa.data[0] = 0x80;
		Cdxa.data[1] = 0x85;
		Cdxa.data[98] = 0;
		Cdxa.data[99] = 2;
		const char *model = "TOSHIBA CD-ROM XM-6402B                   ";
		for (INT32 i = 0; i < 40; i++)
			Cdxa.data[54 + (i ^ 1)] = model[i];
		Cdxa.streaming = 0;
		CdxaReply(512);
	} else if (v == 0xe5) {
		CdxaComplete();
		Cdxa.ata[2] = 0xff;
	} else if (v == 0x08) {
		CdxaComplete();
		Cdxa.ata[1] = 1;
		Cdxa.ata[2] = 1;
		Cdxa.ata[3] = 1;
		Cdxa.ata[4] = 0x14;
		Cdxa.ata[5] = 0xeb;
	} else if (v == 0xef || v == 0x90)
		CdxaComplete();
	else
		CdxaError(5, 0x20);
}
static UINT8 __fastcall CdxaShRead8(UINT32 a);
static UINT16 __fastcall CdxaShRead16(UINT32 a);
static UINT32 __fastcall CdxaShRead32(UINT32 a);
static void __fastcall CdxaShWrite8(UINT32 a, UINT8 v);
static void __fastcall CdxaShWrite16(UINT32 a, UINT16 v);
static void __fastcall CdxaShWrite32(UINT32 a, UINT32 v);
static INT32 CdxaPriority(INT32 vector)
{
	if (vector == 72 || vector == 76)
		return (CdxaReg16(0x834c) >> (vector == 72 ? 12 : 8)) & 15;
	if (vector >= 88 && vector <= 92)
		return (CdxaReg16(0x834e) >> (vector == 92 ? 8 : 12)) & 15;
	if (vector >= 96 && vector <= 101)
		return (CdxaReg16(0x834e) >> (vector >= 100 ? 0 : 4)) & 15;
	if (vector >= 104 && vector <= 109)
		return (CdxaReg16(0x8350) >> (vector >= 108 ? 8 : 12)) & 15;
	if (vector >= 128 && vector <= 135)
		return (CdxaReg16(0x8352) >> (vector < 132 ? 4 : 0)) & 15;
	return 0;
}
static void CdxaUpdateIrq()
{
	INT32 vec = -1, level = 0;
	for (INT32 v = 0; v < 256; v++)
		if (Cdxa.pending[v] && CdxaPriority(v) > level) {
			vec = v;
			level = CdxaPriority(v);
		}
	if (Cdxa.irqLevel > 0)
		Sh2SetIRQLine(Cdxa.irqLevel, CPU_IRQSTATUS_NONE);
	Cdxa.irqVector = vec;
	Cdxa.irqLevel = level;
	if (level)
		Sh2SetIRQLine(level, CPU_IRQSTATUS_ACK);
}
static INT32 CdxaIrqAck(INT32)
{
	INT32 vector = Cdxa.irqVector;
	if (vector >= 0)
		Cdxa.pending[vector] = 0;
	if (Cdxa.irqLevel > 0)
		Sh2SetIRQLine(Cdxa.irqLevel, CPU_IRQSTATUS_NONE);
	Cdxa.irqLevel = 0;
	return vector >= 0 ? vector : 64;
}
static void CdxaDmaTransfer(INT32 ch)
{
	UINT32 b = 0x86c0 + ch * 16, cr = CdxaReg32(b + 12);
	if ((cr & 3) != 1 || (CdxaReg16(0x86b0) & 7) != 1)
		return;
	UINT32 size = 1 << ((cr >> 3) & 3), sm = (cr >> 12) & 3, dm = (cr >> 14) & 3;
	if (size > 4 || sm == 3 || dm == 3)
		return;
	UINT32 src = CdxaReg32(b), dst = CdxaReg32(b + 4), count = CdxaReg32(b + 8) & 0xffff;
	if (!count)
		count = 0x10000;
	if (sm == 2)
		src -= size;
	if (dm == 2)
		dst -= size;
	if (size == 1)
		CdxaShWrite8(dst, CdxaShRead8(src));
	if (size == 2)
		CdxaShWrite16(dst, CdxaShRead16(src));
	if (size == 4)
		CdxaShWrite32(dst, CdxaShRead32(src));
	if (dst == 0xffff81a3)
		Cdxa.regs[0x1a4] &= ~0x80; // DMA write clears TDRE.
	if (sm == 1)
		src += size;
	if (dm == 1)
		dst += size;
	CdxaRegSet32(b, src);
	CdxaRegSet32(b + 4, dst);
	CdxaRegSet32(b + 8, --count);
	if (!count) {
		CdxaRegSet32(b + 12, cr | 2);
		if (cr & 4)
			Cdxa.pending[72 + ch * 4] = 1;
	}
}
static UINT8 __fastcall CdxaShRead8(UINT32 a)
{
	if (a < 0x40000)
		return CdxaRAM[a ^ 3];
	if (a >= 0x400000 && a < 0x410000)
		return CdxaRAM[0x40000 + ((a & 0xffff) ^ 3)];
	if (a == 0xffff8620)
		return Cdxa.regs[0x620] | 0x20;
	if (a >= 0xffff8000)
		return Cdxa.regs[a & 0x7fff];
	return CdxaShRead16(a & ~1) >> ((~a & 1) * 8);
}
static UINT16 __fastcall CdxaShRead16(UINT32 a)
{
	if (a < 0x40000 || (a >= 0x400000 && a < 0x410000) || a >= 0xffff8000)
		return (CdxaShRead8(a) << 8) | CdxaShRead8(a + 1);
	if (a >= 0xc00000 && a < 0xc20000)
		return CdxaAtaRead((a - 0xc00000) >> 14);
	if (a >= 0x41c000 && a <= 0x41c003)
		return Cdxa.irqBusy ? 0 : 0x800;
	return 0;
}
static UINT32 __fastcall CdxaShRead32(UINT32 a)
{
	return (UINT32(CdxaShRead16(a)) << 16) | CdxaShRead16(a + 2);
}
static void __fastcall CdxaShWrite8(UINT32 a, UINT8 v)
{
	if (a < 0x40000) {
		CdxaRAM[a ^ 3] = v;
		return;
	}
	if (a >= 0x400000 && a < 0x410000) {
		CdxaRAM[0x40000 + ((a & 0xffff) ^ 3)] = v;
		return;
	}
	if (a >= 0xffff8000) {
		UINT32 off = a & 0x7fff;
		if (off == 0x1a4 || off == 0x1b4) {
			Cdxa.regs[off] = (Cdxa.regs[off] & v & 0xf8) | (Cdxa.regs[off] & 6) | (v & 1);
		} else
			Cdxa.regs[off] = v;
		return;
	}
	if (a >= 0x41601c && a <= 0x41601f) {
		Cdxa.irqBusy = 1;
		DrvIrqStatus |= 0x400;
		Namcos11UpdateIrq();
	}
}
static void __fastcall CdxaShWrite16(UINT32 a, UINT16 v)
{
	if (a >= 0xc00000 && a < 0xc20000) {
		CdxaAtaWrite((a - 0xc00000) >> 14, v);
		return;
	}
	CdxaShWrite8(a, v >> 8);
	CdxaShWrite8(a + 1, v);
}
static void __fastcall CdxaShWrite32(UINT32 a, UINT32 v)
{
	CdxaShWrite16(a, v >> 16);
	CdxaShWrite16(a + 2, v);
}
static void CdxaSerialTick()
{
	UINT8 scr = Cdxa.regs[0x1a2];
	if (!(scr & 0x20))
		return;
	if (!(Cdxa.regs[0x1a4] & 0x80)) {
		// MAME SCI and LC78836 shift LSB first: the first byte is sample low.
		if (!(Cdxa.audioBytes & 1))
			Cdxa.audioWord = Cdxa.regs[0x1a3];
		else
			Cdxa.audioWord |= UINT32(Cdxa.regs[0x1a3]) << 8;
		Cdxa.audioBytes++;
		if ((Cdxa.audioBytes & 1) == 0) {
			// LC78836: LRCK starts low (right), then high (left).
			Cdxa.dacSample[(Cdxa.audioBytes & 2) ? 1 : 0] = (INT16)Cdxa.audioWord;
		}
		Cdxa.regs[0x1a4] |= 0x84;
	}
	if ((scr & 0x80) && (Cdxa.regs[0x1a4] & 0x80)) {
		bool dma = false;
		for (INT32 ch = 0; ch < 2; ch++) {
			UINT32 cr = CdxaReg32(0x86cc + ch * 16);
			if ((cr & 3) == 1 && ((cr >> 8) & 15) == 12 && (CdxaReg16(0x86b0) & 7) == 1) {
				CdxaDmaTransfer(ch);
				dma = true;
			}
		}
		if (!dma)
			Cdxa.pending[130] = 1;
	}
	if ((scr & 4) && (Cdxa.regs[0x1a4] & 4))
		Cdxa.pending[131] = 1;
}
static void CdxaTimers(INT32 cycles)
{
	for (INT32 ch = 0; ch < 3; ch++) {
		if (!(Cdxa.regs[0x240] & (1 << ch)))
			continue;
		UINT32 b = 0x260 + ch * 0x20, control = Cdxa.regs[b], prescale = control & 7;
		INT32 divisor = prescale < 4 ? (1 << (prescale * 2)) : ((ch == 1 && prescale == 6) ? 256 : ((ch == 2 && prescale == 7) ? 1024 : 0));
		if (!divisor)
			continue;
		Cdxa.timerFrac[ch] += cycles;
		while (Cdxa.timerFrac[ch] >= UINT32(divisor)) {
			Cdxa.timerFrac[ch] -= divisor;
			UINT16 count = CdxaReg16(b + 6) + 1;
			if (!count) {
				Cdxa.regs[b + 5] |= 0x10;
				if (Cdxa.regs[b + 4] & 0x10)
					Cdxa.pending[92 + ch * 8] = 1;
			}
			for (INT32 i = 0; i < (ch ? 2 : 4); i++)
				if (count == CdxaReg16(b + 8 + i * 2)) {
					Cdxa.regs[b + 5] |= 1 << i;
					if (Cdxa.regs[b + 4] & (1 << i))
						Cdxa.pending[88 + ch * 8 + i] = 1;
					if (((control >> 5) & 7) == UINT32(i + 1))
						count = 0;
				}
			Cdxa.regs[b + 6] = count >> 8;
			Cdxa.regs[b + 7] = count;
		}
	}
}
// ICD2061A Manchester programming, using MAME's CDXA reference crystal.
static void CdxaClockUpdate()
{
	UINT32 r = Cdxa.clockRegs[Cdxa.clockSelect];
	Cdxa.clockHz =
	    UINT64(14742890) * Cdxa.clockPrescale[Cdxa.clockSelect] * (((r >> 10) & 127) + 3) / ((r & 127) + 2) / (1 << ((r >> 7) & 7));
	CdxaLogf("Clock %u Hz\n", Cdxa.clockHz);
}
static void CdxaClockWrite(INT32 pin)
{
	Cdxa.clockTimeout = 73728; // ICD2061A's 5 ms programming watchdog.
	if (pin < 2) {
		Cdxa.clockData = pin;
		return;
	}
	UINT8 value = pin - 2;
	if (!Cdxa.clockPin && value) {
		if (Cdxa.clockState == 0 && Cdxa.clockData)
			Cdxa.clockUnlock++;
		else if (Cdxa.clockState == 0 && !Cdxa.clockData && Cdxa.clockUnlock >= 5) {
			Cdxa.clockState = 1;
			Cdxa.clockUnlock = 0;
		} else if (Cdxa.clockState == 1 && !Cdxa.clockData) {
			Cdxa.clockState = 2;
			Cdxa.clockBit = 0;
			Cdxa.clockCmd = 0;
		} else if (Cdxa.clockState == 2 && Cdxa.clockPrev != Cdxa.clockData && Cdxa.clockBit < 24)
			Cdxa.clockCmd |= UINT32(Cdxa.clockData) << Cdxa.clockBit++;
		else if (Cdxa.clockState == 2 && Cdxa.clockData && Cdxa.clockBit == 24) {
			UINT32 index = Cdxa.clockCmd >> 21;
			if (index < 4)
				Cdxa.clockRegs[index] = Cdxa.clockCmd;
			else if (index == 6)
				for (INT32 i = 0; i < 3; i++)
					Cdxa.clockPrescale[i] = 2 << ((Cdxa.clockCmd >> (12 + i)) & 1);
			Cdxa.clockState = Cdxa.clockUnlock = 0;
		} else
			Cdxa.clockState = Cdxa.clockUnlock = 0;
	} else
		Cdxa.clockPrev = Cdxa.clockData;
	Cdxa.clockPin = value;
}
static void CdxaGainUpdate()
{
	for (INT32 c = 0; c < 2; c++) {
		UINT16 latch = Cdxa.volLatch[c];
		Cdxa.gain[c] =
		    !(latch & 0x40) ? 0 : INT32(32768.0 * pow(10.0, -0.025 * ((latch & 0x100) ? 64 : ((latch & 0x80) ? 0 : ((~latch) & 63)))));
	}
}
static void CdxaDacTick()
{
	if (Cdxa.audioFrames < 2048) {
		Cdxa.audioTime[Cdxa.audioFrames] = Cdxa.frameCycles;
		for (INT32 c = 0; c < 2; c++)
			Cdxa.audio[Cdxa.audioFrames * 2 + c] = INT64(Cdxa.dacSample[c]) * Cdxa.gain[c] * Cdxa.attenuation / (32768 * 1024);
		Cdxa.audioFrames++;
	}
	Cdxa.dacSample[0] = Cdxa.dacSample[1] = 0;
	if (CdxaReg16(0x8390) & 0x40) {
		if (Cdxa.attenuation)
			Cdxa.attenuation--;
	} else if (Cdxa.attenuation < 1024)
		Cdxa.attenuation++;
}
static void CdxaRun(INT32 cycles)
{
	if (!Cdxa.running) {
		Cdxa.frameCycles += cycles;
		return;
	}
	INT32 done = Cdxa.cycleExtra;
	while (done < cycles) {
		INT32 n = cycles - done;
		if (n > 32)
			n = 32;
		CdxaUpdateIrq();
		n = Sh2Run(n);
		done += n;
		Cdxa.frameCycles += n;
		CdxaTimers(n);
		for (INT32 ch = 0; ch < 2; ch++) {
			UINT32 cr = CdxaReg32(0x86cc + ch * 16);
			// MAME SH7014 schedules external requests like auto requests until
			// DREQ/DACK are modelled; CDXA uses channel 1 for the ATA data port.
			UINT32 request = (cr >> 8) & 15;
			if (request == 0 || request == 2 || request == 3 || request == 4)
				for (INT32 i = 0; i < n / 2; i++)
					CdxaDmaTransfer(ch);
		}
		if (Cdxa.clockTimeout > 0) {
			Cdxa.clockTimeout -= n;
			if (Cdxa.clockTimeout <= 0) {
				Cdxa.clockSelect = Cdxa.clockData ? (Cdxa.clockPin ? 2 : 3) : Cdxa.clockPin;
				if (Cdxa.clockSelect == 3)
					Cdxa.clockHz = 0; // FEATCLK is not connected.
				else
					CdxaClockUpdate();
				Cdxa.clockState = Cdxa.clockUnlock = 0;
			}
		}
		Cdxa.serialFrac += UINT64(n) * Cdxa.clockHz;
		const UINT64 serialPeriod = UINT64(14745600) * 224;
		while (Cdxa.serialFrac >= serialPeriod) {
			Cdxa.serialFrac -= serialPeriod;
			CdxaSerialTick();
		}
		// MAME's LC78836 configuration: input clock / 2, CKSL=00 (/384).
		Cdxa.dacFrac += UINT64(n) * Cdxa.clockHz;
		const UINT64 dacPeriod = UINT64(14745600) * 768;
		while (Cdxa.dacFrac >= dacPeriod) {
			Cdxa.dacFrac -= dacPeriod;
			CdxaDacTick();
		}
	}
	Cdxa.cycleExtra = done - cycles;
}
static void CdxaMix(INT32 cycles)
{
	// Preserve timing of silence and partial frames instead of stretching the
	// received serial words to fill an entire output frame.
	UINT32 next = 0;
	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		UINT32 time = UINT64(i + 1) * cycles / nBurnSoundLen;
		while (next < Cdxa.audioFrames && Cdxa.audioTime[next] <= time) {
			Cdxa.lastSample[0] = Cdxa.audio[next * 2];
			Cdxa.lastSample[1] = Cdxa.audio[next * 2 + 1];
			next++;
		}
		if (pBurnSoundOut)
			for (INT32 c = 0; c < 2; c++)
				pBurnSoundOut[i * 2 + c] = BURN_SND_CLIP(pBurnSoundOut[i * 2 + c] + Cdxa.lastSample[c] * 3 / 10);
	}
	if (Cdxa.audioFrames) {
		Cdxa.lastSample[0] = Cdxa.audio[(Cdxa.audioFrames - 1) * 2];
		Cdxa.lastSample[1] = Cdxa.audio[(Cdxa.audioFrames - 1) * 2 + 1];
	}
	Cdxa.audioFrames = 0;
	Cdxa.frameCycles = 0;
}
static UINT16 CdxaPsRead(UINT32 a)
{
	if (a >= 0x1f7c0000 && a < 0x1f7d0000) {
		UINT32 off = (a & 0xfffe) ^ 2;
		UINT8 *p = CdxaRAM + (Cdxa.sram ? 0x40000 : 0);
		return p[off] | (p[off + 1] << 8);
	}
	if (a == 0x1f7d800a)
		return 0x20;
	if (a >= 0x1f7e0000 && a < 0x1f7e0010)
		return CdxaAtaRead((a & 15) >> 1);
	return 0;
}
static void CdxaPsWrite(UINT32 a, UINT16 v, UINT16 mask = 0xffff)
{
	if (a >= 0x1f7c0000 && a < 0x1f7d0000) {
		UINT32 off = (a & 0xfffe) ^ 2;
		for (INT32 b = 0; b < 2; b++)
			if (mask & (0xff << (b * 8))) {
				if (!Cdxa.sram)
					CdxaRAM[off + b] = v >> (b * 8);
				CdxaRAM[0x40000 + off + b] = v >> (b * 8);
			}
		return;
	}
	if (a >= 0x1f7e0000 && a < 0x1f7e0010) {
		CdxaAtaWrite((a & 15) >> 1, v);
		return;
	}
	if (a == 0x1f7d6002 && v == 1) {
		Sh2Reset();
		Cdxa.running = 1;
		CdxaLogf("SH reset PC=%08x\n", Sh2GetPC(0));
	}
	if (a >= 0x1f7d6004 && a <= 0x1f7d600a)
		CdxaClockWrite((a - 0x1f7d6004) >> 1);
	if (a == 0x1f7d6010)
		Cdxa.ideSh = v != 0;
	if (a == 0x1f7d6012)
		Cdxa.idePs = v != 0;
	if (a == 0x1f7d601a)
		Cdxa.sram = v != 0;
	if (a >= 0x1f7f8000 && a <= 0x1f7f80ff) {
		Cdxa.volPhase ^= 1;
		UINT8 d = (a - 0x1f7f8000) >> 1;
		if (Cdxa.volPhase)
			Cdxa.volControl = d & 0x1f;
		else {
			Cdxa.volLatch[Cdxa.volControl & 3] = ((Cdxa.volControl << 4) & 0x1c0) | (d & 0x3f);
			CdxaGainUpdate();
		}
	}
	if (a == 0x1f7d601e) {
		Cdxa.irqBusy = 0;
	}
	if (a >= 0x1f7d6000 && a < 0x1f7d6020)
		CdxaLogf("PS board %08x=%04x\n", a, v);
}
static void CdxaReset()
{
	memset(&Cdxa, 0, sizeof(Cdxa));
	Cdxa.irqVector = -1;
	Cdxa.clockData = Cdxa.clockPrev = Cdxa.clockPin = 1;
	for (INT32 i = 0; i < 4; i++)
		Cdxa.clockPrescale[i] = 2;
	Cdxa.clockHz = 44228670;
	Cdxa.regs[0x1a1] = Cdxa.regs[0x1b1] = 0xff;
	Cdxa.regs[0x1a3] = Cdxa.regs[0x1b3] = 0xff;
	Cdxa.regs[0x1a4] = Cdxa.regs[0x1b4] = 0x84;
	for (INT32 c = 0; c < 4; c++)
		Cdxa.volLatch[c] = 0x7f;
	Cdxa.attenuation = 1024;
	CdxaGainUpdate();
	Cdxa.ata[7] = 0x40;
	Cdxa.ata[1] = 1;
	Cdxa.ata[2] = Cdxa.ata[3] = 1;
	Cdxa.ata[4] = 0x14;
	Cdxa.ata[5] = 0xeb;
	Sh2Reset(0, 0);
}
static void CdxaExit()
{
	if (CdxaRAM) {
		Sh2Exit();
		BurnFree(CdxaRAM);
	}
	if (CdxaDisc) {
		ChdClose(CdxaDisc);
		CdxaDisc = NULL;
	}
	if (CdxaLog) {
		fclose(CdxaLog);
		CdxaLog = NULL;
	}
}
static INT32 CdxaInit()
{
	TCHAR path[MAX_PATH];
	if (BurnHDDGetPath(0, _T(".chd"), path))
		return 1;
	CdxaDisc = ChdOpenFile(path);
	if (!CdxaDisc)
		return 1;
	if (ChdGetContainerType(CdxaDisc) != CHD_CONTAINER_CD) {
		CdxaExit();
		return 1;
	}
	CdxaRAM = (UINT8 *)BurnMalloc(0x50000);
	if (!CdxaRAM) {
		CdxaExit();
		return 1;
	}
	memset(CdxaRAM, 0, 0x50000);
	if (Sh2Init(1)) {
		BurnFree(CdxaRAM);
		CdxaExit();
		return 1;
	}
	Sh2Open(0);
	Sh2SetAddressMask(0xffffffff);
	Sh2MapHandler(0, 0, 0x07ffffff, MAP_RAM);
	Sh2MapHandler(0, 0x08000000, 0xffffffff, MAP_RAM);
	Sh2SetReadByteHandler(0, CdxaShRead8);
	Sh2SetReadWordHandler(0, CdxaShRead16);
	Sh2SetReadLongHandler(0, CdxaShRead32);
	Sh2SetWriteByteHandler(0, CdxaShWrite8);
	Sh2SetWriteWordHandler(0, CdxaShWrite16);
	Sh2SetWriteLongHandler(0, CdxaShWrite32);
	Sh2MapMemory(CdxaRAM, 0, 0x3ffff, MAP_RAM);
	Sh2MapMemory(CdxaRAM + 0x40000, 0x400000, 0x40ffff, MAP_RAM);
	Sh2SetIRQCallback(CdxaIrqAck);
	CdxaReset();
	const char *trace = getenv("S12_CDXA_TRACE");
	if (trace && trace[0])
		CdxaLog = fopen(trace, "w");
	CdxaLogf("CDXA disc tracks=%d frames=%d\n", ChdGetNumTracks(CdxaDisc), ChdGetTotalFrames(CdxaDisc));
	return 0;
}
static void CdxaScan(INT32 action)
{
	if (action & ACB_MEMORY_RAM) {
		BurnArea ba = {};
		ba.Data = CdxaRAM;
		ba.nLen = 0x50000;
		ba.szName = "CDXA RAM";
		BurnAcb(&ba);
	}
	if (action & ACB_DRIVER_DATA) {
		SCAN_VAR(Cdxa);
		Sh2Scan(action);
	}
}
