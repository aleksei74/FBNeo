// Capcom ZN-1 / ZN-2 (MAME 0.289 zn.cpp, smf and MAME contributors).
// PSX GPU, DMA, MDEC and timer implementation adapted from FB Neo d_namcos11.cpp.
// CAT702 protocol below is derived from MAME cat702.cpp (BSD-3-Clause, smf).
// No multithreading or host graphics API is required.
#include "tiles_generic.h"
#include "psx_intf.h"
#include "z80_intf.h"
#include "../capcom/cps.h"

static UINT8 ZnSerialTransfer(UINT8 data);

// PSX SPU RAM/registers and voices (TGM effects are separate from QSound music).
static UINT16 ZnSpuRegs[0x100];
static UINT8 ZnSpuRam[0x80000];
static UINT32 ZnSpuWriteAddress;
static void ZnSpuIrq();
#include "zn_spu.h"

static void ZnSpuWrite(UINT32 offset, UINT16 data)
{
	ZnSpuAudio::Sync();
	if (offset == 0x1a8) {
		ZnSpuAudio::CheckIrq(ZnSpuWriteAddress);
		ZnSpuRam[ZnSpuWriteAddress & 0x7ffff] = data;
		ZnSpuRam[(ZnSpuWriteAddress + 1) & 0x7ffff] = data >> 8;
		ZnSpuWriteAddress = (ZnSpuWriteAddress + 2) & 0x7ffff;
		return;
	}
	if (offset == 0x1ae)
		return; // SPUSTAT is read-only.
	ZnSpuRegs[offset >> 1] = data;
	if (offset == 0x1a6)
		ZnSpuWriteAddress = data << 3;
	ZnSpuAudio::Write(offset, data);
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvScratchRAM;
static UINT16 *DrvGpuVram;
// Physical VRAM wraps at 1 MiB on coh1000c, 2 MiB on coh1002c/ZN-2.
static UINT32 DrvGpuVramMask = 0xfffff;
static decltype(BurnHighCol) DrvPaletteHighCol;

static UINT32 DrvExpBase;
static UINT32 DrvControlRAM0;
static UINT32 DrvExpConfig;
static UINT32 DrvRomConfig;
static UINT32 DrvMemControl;
static UINT32 DrvDelayRAM[8];
static UINT32 DrvRamConfig;
static UINT32 DrvControlRAM1[3];
static UINT32 DrvIrqStatus;
static UINT32 DrvIrqMask;
static UINT16 DrvRootCount[3];
static UINT16 DrvRootMode[3];
static UINT16 DrvRootTarget[3];
static UINT32 DrvRootStart[3];
static UINT32 DrvRootExpire[3];
static UINT8 DrvRootActive[3];
static UINT32 DrvPsxTotalCycles;
static UINT32 DrvPsxRunBaseCycles;
static UINT32 DrvPsxRunSegment;
static UINT32 DrvPsxFrameStartCycles;
static UINT32 DrvGpuStatus;
static UINT32 DrvGpuTPage;
static UINT32 DrvGpuInfo;
static INT32 DrvGpuType;
static UINT32 DrvBiu;
static UINT32 DrvDmaBase[7];
static UINT32 DrvDmaBlock[7];
static UINT32 DrvDmaControl[7];
static UINT32 DrvDmaDpcr;
static UINT32 DrvDmaDicr;
static UINT32 DrvDmaFinishCycle[7];
static UINT8 DrvDmaPending;
static UINT32 DrvGpuPacket[16];
static UINT32 DrvGpuPacketPos;
static UINT32 DrvGpuPacketLen;
static INT32 DrvGpuPolyline;
static UINT32 DrvGpuTextureWindowX;
static UINT32 DrvGpuTextureWindowY;
static UINT32 DrvGpuTextureWindowW;
static UINT32 DrvGpuTextureWindowH;
static INT32 DrvGpuDrawStp;
static INT32 DrvGpuCheckStp;
static UINT32 DrvGpuImageX;
static UINT32 DrvGpuImageY;
static UINT32 DrvGpuImageW;
static UINT32 DrvGpuImageH;
static UINT32 DrvGpuImageCount;
static INT32 DrvGpuDiscardImage;
static UINT32 DrvGpuImageCurX;
static UINT32 DrvGpuImageCurY;
static UINT32 DrvGpuImageColumn;
static UINT32 DrvGpuReadX;
static UINT32 DrvGpuReadY;
static UINT32 DrvGpuReadW;
static UINT32 DrvGpuReadH;
static UINT32 DrvGpuReadCount;
static UINT32 DrvGpuDisplayX;
static UINT32 DrvGpuDisplayY;
static UINT32 DrvGpuDrawX1;
static UINT32 DrvGpuDrawY1;
static UINT32 DrvGpuDrawX2;
static UINT32 DrvGpuDrawY2;
static UINT32 DrvGpuHorizStart;
static UINT32 DrvGpuHorizEnd;
static UINT32 DrvGpuVertStart;
static UINT32 DrvGpuVertEnd;
static UINT32 DrvGpuScreenWidth;
static UINT32 DrvGpuScreenHeight;
static INT32 DrvGpuDrawOffsetX;
static INT32 DrvGpuDrawOffsetY;
static INT32 DrvGpuDefaultType = 1;
static INT32 DrvGpuVpos;
static UINT16 DrvSioStatus[2];
static UINT16 DrvSioMode[2];
static UINT16 DrvSioControl[2];
static UINT16 DrvSioBaud[2];
static UINT8 DrvSioRxData[2];
static UINT8 DrvSioTxData[2];

#define ZN_MDEC_DCTSIZE2 64
#define ZN_MDEC_COS_BITS 21

static UINT32 DrvMdecDecoded;
static UINT32 DrvMdecOffset;
static UINT16 DrvMdecOutput[24 * 16];
static INT32 DrvMdecQuantY[ZN_MDEC_DCTSIZE2];
static INT32 DrvMdecQuantUV[ZN_MDEC_DCTSIZE2];
static INT32 DrvMdecCos[ZN_MDEC_DCTSIZE2];
static INT32 DrvMdecCosPrecalc[ZN_MDEC_DCTSIZE2 * ZN_MDEC_DCTSIZE2];
static INT32 DrvMdecUnpacked[ZN_MDEC_DCTSIZE2 * 6 * 2];
static UINT32 DrvMdecCommand0;
static UINT32 DrvMdecAddress0;
static UINT32 DrvMdecSize0;
static UINT32 DrvMdecCommand1;
static UINT32 DrvMdecStatus1;

#define ZN_BIU_RAM 0x00000008
#define ZN_BIU_DS 0x00000080

static const INT32 ZN_PSX_CLOCK = 67737600;
// The PSX CPU core executes one cycle for every four input clocks, as in MAME.
static INT32 ZN_PSX_CPU_CLOCK;

static const UINT8 ZnMdecZigzag[ZN_MDEC_DCTSIZE2] = {
	 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

static inline INT32 ZnMdecSign10(UINT16 value)
{
	return (value & 0x0200) ? (INT32)(value | 0xfffffc00) : (INT32)(value & 0x03ff);
}

static inline UINT16 ZnMdecReadWord(UINT32 address)
{
	return *((UINT16 *)(DrvMainRAM + (address & 0x3fffff)));
}

static void ZnMdecCosPrecalc()
{
	INT32 *dest = DrvMdecCosPrecalc;
	for (INT32 y = 0; y < 8; y++) {
		for (INT32 x = 0; x < 8; x++) {
			for (INT32 v = 0; v < 8; v++) {
				for (INT32 u = 0; u < 8; u++) {
					*dest++ = (DrvMdecCos[u * 8 + x] * DrvMdecCos[v * 8 + y]) >> (30 - ZN_MDEC_COS_BITS);
				}
			}
		}
	}
}

static void ZnMdecIdct(INT32 *source, INT32 *dest)
{
	INT32 *precalc = DrvMdecCosPrecalc;
	for (INT32 yx = 0; yx < ZN_MDEC_DCTSIZE2; yx++) {
		INT32 sum[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		INT32 *data = source;
		for (INT32 vu = 0; vu < 8; vu++) {
			for (INT32 i = 0; i < 8; i++)
				sum[i] += data[i] * precalc[i];
			data += 8;
			precalc += 8;
		}
		*dest++ = (sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7]) >> (ZN_MDEC_COS_BITS + 2);
	}
}

static UINT32 ZnMdecUnpack(UINT32 address)
{
	INT32 *block = DrvMdecUnpacked;
	INT32 *quant = DrvMdecQuantUV;
	for (INT32 blockNo = 0; blockNo < 6; blockNo++) {
		INT32 unpacked[ZN_MDEC_DCTSIZE2];
		memset(unpacked, 0, sizeof(unpacked));
		if (blockNo == 2)
			quant = DrvMdecQuantY;
		UINT16 packed = ZnMdecReadWord(address);
		address += 2;
		if (packed == 0xfe00)
			break;
		INT32 qscale = packed >> 10;
		unpacked[0] = ZnMdecSign10(packed) * quant[0];
		INT32 z = 0;
		for (;;) {
			packed = ZnMdecReadWord(address);
			address += 2;
			if (packed == 0xfe00)
				break;
			z += (packed >> 10) + 1;
			if (z > 63)
				break;
			unpacked[ZnMdecZigzag[z]] = (ZnMdecSign10(packed) * quant[z] * qscale) / 8;
		}
		ZnMdecIdct(unpacked, block);
		block += ZN_MDEC_DCTSIZE2;
	}
	return address;
}

static inline UINT8 ZnMdecClamp8(INT32 value)
{
	value += 128;
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return value;
}

static inline UINT16 ZnMdecRgb15(INT32 r, INT32 g, INT32 b, UINT16 stp)
{
	return stp | (ZnMdecClamp8(r) >> 3) | ((ZnMdecClamp8(g) >> 3) << 5) | ((ZnMdecClamp8(b) >> 3) << 10);
}

static void ZnMdecMakeRgb15(UINT32 address, INT32 r, INT32 g, INT32 b, INT32 *y, UINT16 stp)
{
	DrvMdecOutput[(address + 0) >> 1] = ZnMdecRgb15(y[0] + r, y[0] + g, y[0] + b, stp);
	DrvMdecOutput[(address + 2) >> 1] = ZnMdecRgb15(y[1] + r, y[1] + g, y[1] + b, stp);
}

static void ZnMdecYuvToRgb15()
{
	INT32 *cr = DrvMdecUnpacked;
	INT32 *cb = DrvMdecUnpacked + ZN_MDEC_DCTSIZE2;
	INT32 *y = DrvMdecUnpacked + ZN_MDEC_DCTSIZE2 * 2;
	UINT16 stp = (DrvMdecCommand0 & (1U << 25)) ? 0x8000 : 0;
	UINT32 address = 0;
	for (INT32 z = 0; z < 2; z++) {
		for (INT32 yy = 0; yy < 4; yy++) {
			for (INT32 x = 0; x < 4; x++) {
				INT32 r = (1435 * *cr) >> 10;
				INT32 g = ((-731 * *cr) >> 10) + ((-351 * *cb) >> 10);
				INT32 b = (1814 * *cb) >> 10;
				ZnMdecMakeRgb15(address + 0, r, g, b, y, stp);
				ZnMdecMakeRgb15(address + 32, r, g, b, y + 8, stp);
				r = (1435 * cr[4]) >> 10;
				g = ((-731 * cr[4]) >> 10) + ((-351 * cb[4]) >> 10);
				b = (1814 * cb[4]) >> 10;
				ZnMdecMakeRgb15(address + 16, r, g, b, y + ZN_MDEC_DCTSIZE2, stp);
				ZnMdecMakeRgb15(address + 48, r, g, b, y + ZN_MDEC_DCTSIZE2 + 8, stp);
				cr++;
				cb++;
				y += 2;
				address += 4;
			}
			cr += 4;
			cb += 4;
			y += 8;
			address += 48;
		}
		y += ZN_MDEC_DCTSIZE2;
	}
	DrvMdecDecoded = 128;
}

static void ZnMdecMakeRgb24(UINT32 address, INT32 r, INT32 g, INT32 b, INT32 *y)
{
	UINT8 *output = (UINT8 *)DrvMdecOutput;
	output[address + 0] = ZnMdecClamp8(y[0] + r);
	output[address + 1] = ZnMdecClamp8(y[0] + g);
	output[address + 2] = ZnMdecClamp8(y[0] + b);
	output[address + 3] = ZnMdecClamp8(y[1] + r);
	output[address + 4] = ZnMdecClamp8(y[1] + g);
	output[address + 5] = ZnMdecClamp8(y[1] + b);
}

static void ZnMdecYuvToRgb24()
{
	INT32 *cr = DrvMdecUnpacked;
	INT32 *cb = DrvMdecUnpacked + ZN_MDEC_DCTSIZE2;
	INT32 *y = DrvMdecUnpacked + ZN_MDEC_DCTSIZE2 * 2;
	UINT32 address = 0;
	for (INT32 z = 0; z < 2; z++) {
		for (INT32 yy = 0; yy < 4; yy++) {
			for (INT32 x = 0; x < 4; x++) {
				INT32 r = (1435 * *cr) >> 10;
				INT32 g = ((-731 * *cr) >> 10) + ((-351 * *cb) >> 10);
				INT32 b = (1814 * *cb) >> 10;
				ZnMdecMakeRgb24(address + 0, r, g, b, y);
				ZnMdecMakeRgb24(address + 48, r, g, b, y + 8);
				r = (1435 * cr[4]) >> 10;
				g = ((-731 * cr[4]) >> 10) + ((-351 * cb[4]) >> 10);
				b = (1814 * cb[4]) >> 10;
				ZnMdecMakeRgb24(address + 24, r, g, b, y + ZN_MDEC_DCTSIZE2);
				ZnMdecMakeRgb24(address + 72, r, g, b, y + ZN_MDEC_DCTSIZE2 + 8);
				cr++;
				cb++;
				y += 2;
				address += 6;
			}
			cr += 4;
			cb += 4;
			y += 8;
			address += 72;
		}
		y += ZN_MDEC_DCTSIZE2;
	}
	DrvMdecDecoded = 192;
}

static void ZnMdecReset()
{
	DrvMdecDecoded = 0;
	DrvMdecOffset = 0;
	DrvMdecCommand0 = 0;
	DrvMdecAddress0 = 0;
	DrvMdecSize0 = 0;
	DrvMdecCommand1 = 0;
	DrvMdecStatus1 = 0;
	memset(DrvMdecOutput, 0, sizeof(DrvMdecOutput));
	memset(DrvMdecQuantY, 0, sizeof(DrvMdecQuantY));
	memset(DrvMdecQuantUV, 0, sizeof(DrvMdecQuantUV));
	memset(DrvMdecCos, 0, sizeof(DrvMdecCos));
	memset(DrvMdecCosPrecalc, 0, sizeof(DrvMdecCosPrecalc));
	memset(DrvMdecUnpacked, 0, sizeof(DrvMdecUnpacked));
}

static void ZnMdecDmaWrite(UINT32 address, INT32 size)
{
	address &= 0x3fffff;
	switch (DrvMdecCommand0 >> 28) {
		case 3:
			DrvMdecAddress0 = address;
			DrvMdecSize0 = size * 4;
			DrvMdecStatus1 |= 1U << 29;
			break;

		case 4:
			for (INT32 index = 0; size > 0; index += 4, address += 4, size--) {
				UINT32 data = *((UINT32 *)(DrvMainRAM + (address & 0x3fffff)));
				for (INT32 byteIndex = 0; byteIndex < 4; byteIndex++) {
					if (index + byteIndex < 64)
						DrvMdecQuantY[index + byteIndex] = (data >> (byteIndex * 8)) & 0xff;
					else if (index + byteIndex < 128)
						DrvMdecQuantUV[index + byteIndex - 64] = (data >> (byteIndex * 8)) & 0xff;
				}
			}
			break;

		case 6:
			for (INT32 index = 0; size > 0 && index < 64; index += 2, address += 4, size--) {
				UINT32 data = *((UINT32 *)(DrvMainRAM + (address & 0x3fffff)));
				DrvMdecCos[index + 0] = (INT16)(data & 0xffff);
				DrvMdecCos[index + 1] = (INT16)(data >> 16);
			}
			ZnMdecCosPrecalc();
			break;
	}
}

static void ZnMdecDmaRead(UINT32 address, INT32 size)
{
	address &= 0x3fffff;
	if ((DrvMdecCommand0 & (1U << 29)) && DrvMdecSize0) {
		while (size > 0) {
			if (DrvMdecDecoded == 0) {
				if ((INT32)DrvMdecSize0 <= 0) {
					DrvMdecSize0 = 0;
					break;
				}
				UINT32 next = ZnMdecUnpack(DrvMdecAddress0);
				DrvMdecSize0 -= next - DrvMdecAddress0;
				DrvMdecAddress0 = next;
				if (DrvMdecCommand0 & (1U << 27))
					ZnMdecYuvToRgb15();
				else
					ZnMdecYuvToRgb24();
				DrvMdecOffset = 0;
				while (DrvMdecSize0 && ZnMdecReadWord(DrvMdecAddress0) == 0xfe00) {
					DrvMdecAddress0 += 2;
					DrvMdecSize0 -= 2;
				}
			}

			UINT32 count = DrvMdecDecoded;
			if (count > (UINT32)size)
				count = size;
			memcpy(DrvMainRAM + address, (UINT8 *)DrvMdecOutput + DrvMdecOffset, count * 4);
			DrvMdecDecoded -= count;
			DrvMdecOffset += count * 4;
			address = (address + count * 4) & 0x3fffff;
			size -= count;
		}
	}
	if ((INT32)DrvMdecSize0 <= 0)
		DrvMdecStatus1 &= ~(1U << 29);
}

static void ZnGpuUpdateVisibleArea()
{
	if (DrvGpuStatus & (1 << 20)) {
		DrvGpuScreenHeight = (DrvGpuStatus & (1 << 19)) ? 512 : 256;
	} else {
		DrvGpuScreenHeight = (DrvGpuStatus & (1 << 19)) ? 480 : 240;
	}

	switch ((DrvGpuStatus >> 17) & 3) {
		case 0:
			DrvGpuScreenWidth = (DrvGpuStatus & (1 << 16)) ? 368 : 256;
			break;

		case 1:
			DrvGpuScreenWidth = (DrvGpuStatus & (1 << 16)) ? 384 : 320;
			break;

		case 2:
			DrvGpuScreenWidth = 512;
			break;

		case 3:
			DrvGpuScreenWidth = 640;
			break;
	}
}

#define PSX_RC_STOP 0x0001
#define PSX_RC_RESET 0x0004
#define PSX_RC_COUNTTARGET 0x0008
#define PSX_RC_IRQTARGET 0x0010
#define PSX_RC_IRQOVERFLOW 0x0020
#define PSX_RC_REPEAT 0x0040
#define PSX_RC_CLC 0x0100
#define PSX_RC_DIV 0x0200
#define PSX_RC_REACHEDTARGET 0x0800
#define PSX_RC_REACHEDFFFF 0x1000

#define PSX_SIO_STATUS_TX_RDY 0x0001
#define PSX_SIO_STATUS_RX_RDY 0x0002
#define PSX_SIO_STATUS_TX_EMPTY 0x0004
#define PSX_SIO_STATUS_OVERRUN 0x0010
#define PSX_SIO_STATUS_DSR 0x0080
#define PSX_SIO_STATUS_IRQ 0x0200

#define PSX_SIO_CONTROL_TX_ENA 0x0001
#define PSX_SIO_CONTROL_IACK 0x0010
#define PSX_SIO_CONTROL_RESET 0x0040
#define PSX_SIO_CONTROL_TX_IENA 0x0400
#define PSX_SIO_CONTROL_RX_IENA 0x0800
#define PSX_SIO_CONTROL_DSR_IENA 0x1000
#define PSX_SIO_CONTROL_DTR 0x2000

// MAME port layouts and SYSTEM11_MiSTer SYSTEM11.sv: only these
// layouts route P1 kicks through ADC and P2 kicks through PLAYER4.
static UINT8 *ZnScratchpad()
{
	UINT8 *scratch = PsxGetDCache();

	return scratch ? scratch : DrvScratchRAM;
}

static void ZnResetIo()
{
	memset(DrvGpuVram, 0, 1024 * 1024 * sizeof(UINT16));
	DrvExpBase = 0x00000000;
	DrvControlRAM0 = 0x00000000;
	DrvExpConfig = 0x00000000;
	DrvRomConfig = 0x00130000;
	DrvMemControl = 0x00000000;
	memset(DrvDelayRAM, 0, sizeof(DrvDelayRAM));
	DrvRamConfig = 0x00000c00;
	memset(DrvControlRAM1, 0, sizeof(DrvControlRAM1));
	DrvIrqStatus = 0;
	DrvIrqMask = 0;
	memset(DrvRootCount, 0, sizeof(DrvRootCount));
	memset(DrvRootMode, 0, sizeof(DrvRootMode));
	memset(DrvRootTarget, 0, sizeof(DrvRootTarget));
	memset(DrvRootStart, 0, sizeof(DrvRootStart));
	memset(DrvRootExpire, 0, sizeof(DrvRootExpire));
	memset(DrvRootActive, 0, sizeof(DrvRootActive));
	DrvPsxTotalCycles = 0;
	DrvPsxRunBaseCycles = 0;
	DrvPsxRunSegment = 0;
	DrvPsxFrameStartCycles = 0;
	DrvGpuStatus = 0x14802000;
	DrvGpuTPage = 0;
	DrvGpuInfo = 0;
	DrvGpuType = DrvGpuDefaultType;
	DrvBiu = 0;
	memset(DrvDmaBase, 0, sizeof(DrvDmaBase));
	memset(DrvDmaBlock, 0, sizeof(DrvDmaBlock));
	memset(DrvDmaControl, 0, sizeof(DrvDmaControl));
	DrvDmaDpcr = 0;
	DrvDmaDicr = 0;
	memset(DrvDmaFinishCycle, 0, sizeof(DrvDmaFinishCycle));
	DrvDmaPending = 0;
	ZnMdecReset();
	DrvGpuPacketPos = 0;
	DrvGpuPacketLen = 0;
	DrvGpuPolyline = 0;
	DrvGpuTextureWindowX = 0;
	DrvGpuTextureWindowY = 0;
	DrvGpuTextureWindowW = 255;
	DrvGpuTextureWindowH = 255;
	DrvGpuDrawStp = 0;
	DrvGpuCheckStp = 0;
	DrvGpuImageX = 0;
	DrvGpuImageY = 0;
	DrvGpuImageW = 0;
	DrvGpuImageH = 0;
	DrvGpuImageCount = 0;
	DrvGpuDiscardImage = 0;
	DrvGpuImageCurX = 0;
	DrvGpuImageCurY = 0;
	DrvGpuImageColumn = 0;
	DrvGpuReadX = DrvGpuReadY = 0;
	DrvGpuReadW = DrvGpuReadH = DrvGpuReadCount = 0;
	DrvGpuDisplayX = 0;
	DrvGpuDisplayY = 0;
	DrvGpuDrawX1 = 0;
	DrvGpuDrawY1 = 0;
	DrvGpuDrawX2 = 1023;
	DrvGpuDrawY2 = 1023;
	DrvGpuHorizStart = 0x260;
	DrvGpuHorizEnd = 0xc60;
	DrvGpuVertStart = 0x010;
	DrvGpuVertEnd = 0x100;
	DrvGpuDrawOffsetX = 0;
	DrvGpuDrawOffsetY = 0;
	DrvGpuVpos = 0;
	ZnGpuUpdateVisibleArea();
	for (INT32 i = 0; i < 2; i++) {
		DrvSioStatus[i] = PSX_SIO_STATUS_TX_EMPTY | PSX_SIO_STATUS_TX_RDY;
		DrvSioMode[i] = 0;
		DrvSioControl[i] = 0;
		DrvSioBaud[i] = 0;
		DrvSioRxData[i] = 0xff;
		DrvSioTxData[i] = 0xff;
	}
}

static void ZnUpdateIrq()
{
	PsxSetIRQLine(0, (DrvIrqStatus & DrvIrqMask) ? 1 : 0);
}

static void ZnSioUpdateIrq(INT32 sio)
{
	UINT32 bit = sio ? 0x0100 : 0x0080;

	if (DrvSioStatus[sio] & PSX_SIO_STATUS_IRQ) {
		DrvIrqStatus |= bit;
	} else {
		DrvIrqStatus &= ~bit;
	}

	ZnUpdateIrq();
}

static void ZnSpuIrq()
{
	// SPU IRQ9 is latched separately from the CPU interrupt controller.
	DrvIrqStatus |= 1 << 9;
	ZnUpdateIrq();
}

static UINT32 ZnPsxTotalCycles()
{
	INT32 remaining = PsxGetRemainingCycles();

	if (remaining < 0)
		remaining = 0;
	if ((UINT32)remaining > DrvPsxRunSegment)
		remaining = DrvPsxRunSegment;

	return DrvPsxRunBaseCycles + (DrvPsxRunSegment - remaining);
}

static UINT32 ZnPsxRawCycles()
{
	INT32 remaining = PsxGetRemainingCycles();

	if (remaining < 0)
		remaining = 0;
	if ((UINT32)remaining > DrvPsxRunSegment)
		remaining = DrvPsxRunSegment;

	return DrvPsxRunBaseCycles + (DrvPsxRunSegment - remaining);
}

static UINT32 ZnSioRead(UINT32 address)
{
	INT32 sio = (address >= 0x1f801050) ? 1 : 0;
	UINT32 reg = ((address - (sio ? 0x1f801050 : 0x1f801040)) >> 2) & 3;
	UINT32 data = 0;

	switch (reg) {
		case 0:
			data = DrvSioRxData[sio];
			DrvSioStatus[sio] &= ~PSX_SIO_STATUS_RX_RDY;
			DrvSioRxData[sio] = 0xff;
			break;

		case 1:
			data = DrvSioStatus[sio];
			break;

		case 2:
			data = (DrvSioControl[sio] << 16) | DrvSioMode[sio];
			break;

		case 3:
			data = DrvSioBaud[sio] << 16;
			break;
	}

	return data;
}

static void ZnSioWrite(UINT32 address, UINT32 data, UINT32 mem_mask)
{
	INT32 sio = (address >= 0x1f801050) ? 1 : 0;
	UINT32 reg = ((address - (sio ? 0x1f801050 : 0x1f801040)) >> 2) & 3;

	switch (reg) {
		case 0:
			if (mem_mask & 0x000000ff) {
				DrvSioTxData[sio] = data & 0xff;
				if (sio == 0) {
					if (DrvSioStatus[sio] & PSX_SIO_STATUS_RX_RDY) {
						DrvSioStatus[sio] |= PSX_SIO_STATUS_OVERRUN;
					}
					DrvSioRxData[sio] = ZnSerialTransfer(data & 0xff);
					DrvSioStatus[sio] |= PSX_SIO_STATUS_RX_RDY;
				}
				DrvSioStatus[sio] |= PSX_SIO_STATUS_TX_EMPTY | PSX_SIO_STATUS_TX_RDY;
				if (DrvSioControl[sio] & (PSX_SIO_CONTROL_TX_IENA | PSX_SIO_CONTROL_RX_IENA)) {
					DrvSioStatus[sio] |= PSX_SIO_STATUS_IRQ;
					ZnSioUpdateIrq(sio);
				}
			}
			break;

		case 2:
			if (mem_mask & 0x0000ffff) {
				DrvSioMode[sio] = data & 0xffff;
			}
			if (mem_mask & 0xffff0000) {
				DrvSioControl[sio] = data >> 16;
				if (DrvSioControl[sio] & PSX_SIO_CONTROL_RESET) {
					DrvSioStatus[sio] |= PSX_SIO_STATUS_TX_EMPTY | PSX_SIO_STATUS_TX_RDY;
					DrvSioStatus[sio] &= ~(PSX_SIO_STATUS_RX_RDY | PSX_SIO_STATUS_OVERRUN | PSX_SIO_STATUS_IRQ);
					DrvSioRxData[sio] = 0xff;
					DrvSioTxData[sio] = 0xff;
					ZnSioUpdateIrq(sio);
				}
				if (DrvSioControl[sio] & PSX_SIO_CONTROL_IACK) {
					DrvSioStatus[sio] &= ~PSX_SIO_STATUS_IRQ;
					DrvSioControl[sio] &= ~PSX_SIO_CONTROL_IACK;
					ZnSioUpdateIrq(sio);
				}
				if ((DrvSioControl[sio] & PSX_SIO_CONTROL_DSR_IENA) && (DrvSioStatus[sio] & PSX_SIO_STATUS_DSR)) {
					DrvSioStatus[sio] |= PSX_SIO_STATUS_IRQ;
					ZnSioUpdateIrq(sio);
				}
			}
			break;

		case 3:
			if (mem_mask & 0xffff0000) {
				DrvSioBaud[sio] = data >> 16;
			}
			break;
	}
}

static INT32 ZnRootDivider(INT32 counter)
{
	if (counter == 0 && (DrvRootMode[counter] & PSX_RC_CLC))
		return 5;
	if (counter == 1 && (DrvRootMode[counter] & PSX_RC_CLC))
		return 2150;
	if (counter == 2 && (DrvRootMode[counter] & PSX_RC_DIV))
		return 8;

	return 1;
}

static UINT32 ZnRootCycles()
{
	return ZnPsxTotalCycles() * 2;
}

static UINT16 ZnRootCurrent(INT32 counter)
{
	if (DrvRootMode[counter] & PSX_RC_STOP) {
		return DrvRootCount[counter];
	}

	UINT32 current = (ZnRootCycles() - DrvRootStart[counter]) / ZnRootDivider(counter);
	current += DrvRootCount[counter];

	if (current > 0xffff) {
		DrvRootCount[counter] = current & 0xffff;
		DrvRootStart[counter] = ZnRootCycles();
		DrvRootMode[counter] |= PSX_RC_REACHEDFFFF;
	}

	return current & 0xffff;
}

static INT32 ZnRootTarget(INT32 counter)
{
	if (DrvRootMode[counter] & (PSX_RC_COUNTTARGET | PSX_RC_IRQTARGET)) {
		return DrvRootTarget[counter];
	}

	return 0x10000;
}

static void ZnRootAdjust(INT32 counter)
{
	if (DrvRootMode[counter] & PSX_RC_STOP) {
		DrvRootActive[counter] = 0;
		return;
	}

	INT32 duration = ZnRootTarget(counter) - ZnRootCurrent(counter);
	if (duration < 1) {
		duration += 0x10000;
		if (DrvRootMode[counter] & PSX_RC_COUNTTARGET) {
			DrvRootMode[counter] |= PSX_RC_REACHEDTARGET;
		} else {
			DrvRootMode[counter] |= PSX_RC_REACHEDFFFF;
		}
	}

	DrvRootExpire[counter] = ZnRootCycles() + duration * ZnRootDivider(counter);
	DrvRootActive[counter] = 1;
}

static void ZnRootFinish(INT32 counter)
{
	DrvRootCount[counter] = 0;
	DrvRootStart[counter] = ZnRootCycles();
	DrvRootActive[counter] = 0;

	if (DrvRootMode[counter] & PSX_RC_REPEAT) {
		ZnRootAdjust(counter);
	}

	if (DrvRootMode[counter] & (PSX_RC_IRQTARGET | PSX_RC_IRQOVERFLOW)) {
		DrvIrqStatus |= 1 << (counter + 4);
		ZnUpdateIrq();
	}
}

static UINT32 ZnRootRead(UINT32 address)
{
	INT32 counter = (address - 0x1f801100) >> 4;
	INT32 reg = (address >> 2) & 3;

	if (counter < 0 || counter >= 3)
		return 0;

	switch (reg) {
		case 0:
			return ZnRootCurrent(counter);

		case 1: {
			UINT16 mode = DrvRootMode[counter];
			DrvRootMode[counter] &= ~(PSX_RC_REACHEDFFFF | PSX_RC_REACHEDTARGET);
			return mode;
		}

		case 2:
			return DrvRootTarget[counter];
	}

	return 0;
}

static void ZnRootWrite(UINT32 address, UINT32 data)
{
	INT32 counter = (address - 0x1f801100) >> 4;
	INT32 reg = (address >> 2) & 3;

	if (counter < 0 || counter >= 3)
		return;

	switch (reg) {
		case 0:
			DrvRootCount[counter] = data;
			DrvRootStart[counter] = ZnRootCycles();
			break;

		case 1:
			DrvRootCount[counter] = ZnRootCurrent(counter);
			DrvRootStart[counter] = ZnRootCycles();
			if (data & PSX_RC_RESET) {
				data &= ~(PSX_RC_RESET | PSX_RC_STOP);
				DrvRootCount[counter] = 0;
			}
			data &= ~(PSX_RC_REACHEDFFFF | PSX_RC_REACHEDTARGET);
			DrvRootMode[counter] = data | (DrvRootMode[counter] & (PSX_RC_REACHEDFFFF | PSX_RC_REACHEDTARGET));
			break;

		case 2:
			DrvRootTarget[counter] = data;
			break;
	}

	ZnRootAdjust(counter);
}

static void ZnRootAdvance()
{
	for (INT32 i = 0; i < 3; i++) {
		if (DrvRootActive[i] && (INT32)(ZnRootCycles() - DrvRootExpire[i]) >= 0) {
			ZnRootFinish(i);
		}
	}
}

static void ZnMapRamConfig()
{
	UINT32 window = 0x0400000;

	PsxMapHandler(0, 0x00000000, 0x1effffff, MAP_READ | MAP_WRITE);

	UINT32 size = 0x0400000;
	if (size > window)
		size = window;

	for (UINT32 start = 0; start < window; start += size) {
		PsxMapMemory(DrvMainRAM, start, start + size - 1, MAP_RAM);
	}
}

static void ZnMapRomConfig()
{
	PsxMapMemory(DrvMainROM, 0x1fc00000, 0x1fc7ffff, MAP_ROM);
}

static UINT32 ZnReadIo(UINT32 address)
{
	if (address >= 0x1f801c00 && address <= 0x1f801e5f) {
		ZnSpuAudio::Sync();
		UINT32 offset = (address - 0x1f801c00) & ~3U;
		return ZnSpuAudio::CurrentVolume(offset) | ((UINT32)ZnSpuAudio::CurrentVolume(offset + 2) << 16);
	}
	// Readback must follow all preceding commands, including VRAM transfers.

	switch (address & ~3) {
		case 0x1f801000:
			return DrvExpBase;
		case 0x1f801004:
			return DrvControlRAM0;
		case 0x1f801008:
			return DrvExpConfig;
		case 0x1f80100c:
			return DrvMemControl;
		case 0x1f801010:
			return DrvRomConfig;
		case 0x1f801014:
		case 0x1f801018:
		case 0x1f80101c:
		case 0x1f801020:
		case 0x1f801024:
		case 0x1f801028:
		case 0x1f80102c:
			return DrvDelayRAM[((address & ~3) - 0x1f801014) >> 2];
		case 0x1f801040:
		case 0x1f801044:
		case 0x1f801048:
		case 0x1f80104c:
		case 0x1f801050:
		case 0x1f801054:
		case 0x1f801058:
		case 0x1f80105c:
			return ZnSioRead(address & ~3);
		case 0x1f801060:
			return DrvRamConfig;
		case 0x1f801064:
		case 0x1f801068:
		case 0x1f80106c:
			return DrvControlRAM1[((address & ~3) - 0x1f801064) >> 2];
		case 0x1f801070:
			return DrvIrqStatus;
		case 0x1f801074:
			return DrvIrqMask;
		case 0x1f801810:
			if (DrvGpuReadCount) {
				UINT32 data = 0;
				for (INT32 i = 0; i < 2 && DrvGpuReadCount; i++) {
					UINT32 pos = (DrvGpuReadW * DrvGpuReadH) - DrvGpuReadCount;
					UINT32 x = (DrvGpuReadX + (pos % DrvGpuReadW)) & 0x3ff;
					UINT32 y = (DrvGpuReadY + (pos / DrvGpuReadW)) & 0x3ff;
					data |= (UINT32)DrvGpuVram[((y << 10) | x) & DrvGpuVramMask] << (i * 16);
					DrvGpuReadCount--;
				}
				if (DrvGpuReadCount == 0)
					DrvGpuStatus &= ~(1 << 27);
				return data;
			}
			return DrvGpuInfo;
		case 0x1f801814:
			return DrvGpuStatus |
			       ((((DrvGpuStatus & (1 << 22)) && (DrvGpuStatus & (1 << 13))) || (!(DrvGpuStatus & (1 << 22)) && (DrvGpuVpos & 1)))
			            ? (1u << 31)
						: 0);
		case 0x1f801820:
			return 0;
		case 0x1f801824:
			return DrvMdecStatus1;
		case 0xfffe0130:
			return PsxGetBIU();
	}

	if (address >= 0x1f801080 && address <= 0x1f8010ff) {
		UINT32 offset = (address - 0x1f801080) >> 2;
		UINT32 channel = offset >> 2;

		if (channel < 7) {
			switch (offset & 3) {
				case 0:
					return DrvDmaBase[channel];
				case 1:
					return DrvDmaBlock[channel];
				case 2:
				case 3:
					return DrvDmaControl[channel];
			}
		} else {
			switch (offset & 3) {
				case 0:
					return DrvDmaDpcr;
				case 1:
					return DrvDmaDicr;
			}
		}
	}

	if (address >= 0x1f801100 && address <= 0x1f80112f) {
		return ZnRootRead(address);
	}

	return 0;
}

static void ZnDmaInterruptUpdate()
{
	INT32 n_int = (DrvDmaDicr >> 24) & 0x7f;
	INT32 n_mask = (DrvDmaDicr >> 16) & 0xff;

	if ((n_mask & 0x80) && (n_int & n_mask)) {
		DrvDmaDicr |= 0x80000000;
		DrvIrqStatus |= 0x0008;
	} else {
		DrvIrqStatus &= ~0x0008;
	}

	DrvDmaDicr &= 0x00ffffff | (DrvDmaDicr << 8);
	ZnUpdateIrq();
}

static INT32 ZnGpuPacketLen(UINT8 command)
{
	if (command == 0x00 || command == 0x01)
		return 1;
	if (command == 0x02)
		return 3;
	if (command >= 0x20 && command <= 0x23)
		return 4;
	if (command >= 0x24 && command <= 0x27)
		return 7;
	if (command >= 0x28 && command <= 0x2b)
		return 5;
	if (command >= 0x2c && command <= 0x2f)
		return 9;
	if (command >= 0x30 && command <= 0x33)
		return 6;
	if (command >= 0x34 && command <= 0x37)
		return 9;
	if (command >= 0x38 && command <= 0x3b)
		return 8;
	if (command >= 0x3c && command <= 0x3f)
		return 12;
	if (command >= 0x40 && command <= 0x47)
		return 3;
	if (command >= 0x50 && command <= 0x57)
		return 4;
	if (command >= 0x60 && command <= 0x63)
		return 3;
	if (command >= 0x64 && command <= 0x67)
		return 4;
	if (command >= 0x68 && command <= 0x6b)
		return 2;
	if (command >= 0x6c && command <= 0x6f)
		return 3;
	if (command >= 0x70 && command <= 0x73)
		return 2;
	if (command >= 0x74 && command <= 0x77)
		return 3;
	if (command >= 0x78 && command <= 0x7b)
		return 2;
	if (command >= 0x7c && command <= 0x7f)
		return 3;
	if (command == 0x80)
		return 4;
	if (command == 0xa0 || command == 0xc0)
		return 3;
	if (command >= 0xe1 && command <= 0xe6)
		return 1;

	return 1;
}

static inline void ZnGpuWriteVramPixel(UINT32 x, UINT32 y, UINT16 color)
{
	UINT16 *pixel = &DrvGpuVram[(((y & 0x3ff) << 10) | (x & 0x3ff)) & DrvGpuVramMask];
	if (DrvGpuCheckStp && (*pixel & 0x8000))
		return;
	*pixel = DrvGpuDrawStp ? (color | 0x8000) : color;
}

static void ZnGpuWriteImage(UINT32 data)
{
	const UINT16 stp = DrvGpuDrawStp ? 0x8000 : 0;

	for (INT32 i = 0; i < 2 && DrvGpuImageCount > 0; i++) {
		const UINT16 color = (UINT16)((i == 0) ? data : (data >> 16));
		UINT16 *pixel = &DrvGpuVram[((DrvGpuImageCurY << 10) | DrvGpuImageCurX) & DrvGpuVramMask];

		if (!DrvGpuDiscardImage && (!DrvGpuCheckStp || !(*pixel & 0x8000)))
			*pixel = color | stp;
		DrvGpuImageCount--;
		DrvGpuImageCurX = (DrvGpuImageCurX + 1) & 0x3ff;
		if (++DrvGpuImageColumn == DrvGpuImageW) {
			DrvGpuImageColumn = 0;
			DrvGpuImageCurX = DrvGpuImageX & 0x3ff;
			DrvGpuImageCurY = (DrvGpuImageCurY + 1) & 0x3ff;
		}
	}

	if (DrvGpuImageCount == 0) {
		DrvGpuPacketPos = 0;
		DrvGpuPacketLen = 0;
	}
}

static void ZnGpuFillRect()
{
	UINT16 color = ((DrvGpuPacket[0] & 0x0000f8) >> 3) | ((DrvGpuPacket[0] & 0x00f800) >> 6) | ((DrvGpuPacket[0] & 0xf80000) >> 9);
	UINT32 x = DrvGpuPacket[1] & 0xffff;
	UINT32 y = DrvGpuPacket[1] >> 16;
	UINT32 w = DrvGpuPacket[2] & 0xffff;
	UINT32 h = DrvGpuPacket[2] >> 16;

	for (UINT32 yy = 0; yy < h; yy++) {
		for (UINT32 xx = 0; xx < w; xx++) {
			DrvGpuVram[(((((y + yy) & 0x3ff) << 10) | ((x + xx) & 0x3ff))) & DrvGpuVramMask] = color;
		}
	}
}

static void ZnGpuCopyVram()
{
	UINT32 sx = DrvGpuPacket[1] & 0xffff;
	UINT32 sy = DrvGpuPacket[1] >> 16;
	UINT32 dx = DrvGpuPacket[2] & 0xffff;
	UINT32 dy = DrvGpuPacket[2] >> 16;
	UINT32 w = DrvGpuPacket[3] & 0xffff;
	UINT32 h = DrvGpuPacket[3] >> 16;

	for (UINT32 yy = 0; yy < h; yy++) {
		for (UINT32 xx = 0; xx < w; xx++) {
			UINT16 color = DrvGpuVram[((((sy + yy) & 0x3ff) << 10) | ((sx + xx) & 0x3ff)) & DrvGpuVramMask];
			ZnGpuWriteVramPixel(dx + xx, dy + yy, color);
		}
	}
}

static inline UINT16 ZnGpuShadeComponent(UINT32 component)
{
	component = (16 * component) / 128;
	return (component > 31) ? 31 : component;
}

static inline UINT16 ZnGpuColor(UINT32 data)
{
	return ZnGpuShadeComponent(data & 0xff) | (ZnGpuShadeComponent((data >> 8) & 0xff) << 5) |
	       (ZnGpuShadeComponent((data >> 16) & 0xff) << 10);
}

static inline INT32 ZnGpuS11Coord(UINT32 data)
{
	data &= 0x7ff;
	return (data & 0x400) ? (INT32)data - 0x800 : (INT32)data;
}

static inline INT32 ZnGpuX(UINT32 data)
{
	return ZnGpuS11Coord(data) + DrvGpuDrawOffsetX;
}

static inline INT32 ZnGpuY(UINT32 data)
{
	return ZnGpuS11Coord(data >> 16) + DrvGpuDrawOffsetY;
}

static inline INT32 ZnGpuPolyX(UINT32 data)
{
	return ZnGpuS11Coord(data) + DrvGpuDrawOffsetX;
}

static inline INT32 ZnGpuPolyY(UINT32 data)
{
	return ZnGpuS11Coord(data >> 16) + DrvGpuDrawOffsetY;
}

static inline void ZnGpuPutPixel(INT32 x, INT32 y, UINT16 color)
{
	if (x < DrvGpuDrawX1 || x > DrvGpuDrawX2 || y < DrvGpuDrawY1 || y > DrvGpuDrawY2)
		return;

	ZnGpuWriteVramPixel(x, y, color);
}

static inline void ZnGpuPutSolidPixel(INT32 x, INT32 y, UINT16 color, INT32 semi);

static inline void ZnGpuGetClip(INT32 &x1, INT32 &y1, INT32 &x2, INT32 &y2)
{
	x1 = DrvGpuDrawX1;
	y1 = DrvGpuDrawY1;
	x2 = DrvGpuDrawX2;
	y2 = DrvGpuDrawY2;
}

struct NamcosFlatRectContext {
	INT32 x;
	INT32 y;
	INT32 width;
	UINT16 color;
	INT32 semi;
};

static void ZnGpuDrawFlatRectRows(void *opaque, INT32 begin, INT32 end)
{
	NamcosFlatRectContext *context = (NamcosFlatRectContext *)opaque;
	for (INT32 yy = begin; yy < end; yy++) {
		for (INT32 xx = 0; xx < context->width; xx++) {
			ZnGpuPutSolidPixel(context->x + xx, context->y + yy, context->color, context->semi);
		}
	}
}

static void ZnGpuDrawFlatRect(INT32 width, INT32 height)
{
	if (width <= 0 || height <= 0)
		return;

	NamcosFlatRectContext context;
	context.x = ZnGpuX(DrvGpuPacket[1]);
	context.y = ZnGpuY(DrvGpuPacket[1]);
	context.width = width;
	context.color = ZnGpuColor(DrvGpuPacket[0]);
	context.semi = DrvGpuPacket[0] & 0x02000000;
	ZnGpuDrawFlatRectRows(&context, 0, height);
}

static void ZnGpuDrawFlatRectVariable()
{
	ZnGpuDrawFlatRect(DrvGpuPacket[2] & 0xffff, DrvGpuPacket[2] >> 16);
}

static void ZnGpuDrawDot()
{
	ZnGpuDrawFlatRect(1, 1);
}

static void ZnGpuDrawLine()
{
	INT32 x0 = ZnGpuS11Coord(DrvGpuPacket[1]);
	INT32 y0 = ZnGpuS11Coord(DrvGpuPacket[1] >> 16);
	INT32 x1 = ZnGpuS11Coord(DrvGpuPacket[2]);
	INT32 y1 = ZnGpuS11Coord(DrvGpuPacket[2] >> 16);
	INT32 dirtyY1 = y0 < y1 ? y0 : y1;
	INT32 dirtyY2 = y0 > y1 ? y0 : y1;
	INT32 dx = x1 - x0;
	if (dx < 0)
		dx = -dx;
	INT32 dy = y1 - y0;
	if (dy < 0)
		dy = -dy;
	INT32 length = (dx > dy) ? dx : dy;
	UINT16 color = ZnGpuColor(DrvGpuPacket[0]);
	INT32 semi = DrvGpuPacket[0] & 0x02000000;

	if (length == 0)
		length = 1;
	INT64 x = (INT64)x0 << 16;
	INT64 y = (INT64)y0 << 16;
	INT64 stepx = (((INT64)x1 << 16) - x) / length;
	INT64 stepy = (((INT64)y1 << 16) - y) / length;

	while (length > 0) {
		ZnGpuPutSolidPixel((INT32)(x >> 16) + DrvGpuDrawOffsetX, (INT32)(y >> 16) + DrvGpuDrawOffsetY, color, semi);
		x += stepx;
		y += stepy;
		length--;
	}
}

static void ZnGpuDrawGouraudLine()
{
	INT32 x0 = ZnGpuS11Coord(DrvGpuPacket[1]);
	INT32 y0 = ZnGpuS11Coord(DrvGpuPacket[1] >> 16);
	INT32 x1 = ZnGpuS11Coord(DrvGpuPacket[3]);
	INT32 y1 = ZnGpuS11Coord(DrvGpuPacket[3] >> 16);
	INT32 dirtyY1 = y0 < y1 ? y0 : y1;
	INT32 dirtyY2 = y0 > y1 ? y0 : y1;
	UINT32 c0 = DrvGpuPacket[0] & 0x00ffffff;
	UINT32 c1 = DrvGpuPacket[2] & 0x00ffffff;
	INT32 dx = x1 - x0;
	if (dx < 0)
		dx = -dx;
	INT32 dy = y1 - y0;
	if (dy < 0)
		dy = -dy;
	INT32 length = (dx > dy) ? dx : dy;
	INT32 semi = DrvGpuPacket[0] & 0x02000000;

	if (length == 0)
		length = 1;
	INT64 x = (INT64)x0 << 16;
	INT64 y = (INT64)y0 << 16;
	INT64 r = (INT64)(c0 & 0xff) << 16;
	INT64 g = (INT64)((c0 >> 8) & 0xff) << 16;
	INT64 b = (INT64)((c0 >> 16) & 0xff) << 16;
	INT64 stepx = (((INT64)x1 << 16) - x) / length;
	INT64 stepy = (((INT64)y1 << 16) - y) / length;
	INT64 stepr = (((INT64)(c1 & 0xff) << 16) - r) / length;
	INT64 stepg = (((INT64)((c1 >> 8) & 0xff) << 16) - g) / length;
	INT64 stepb = (((INT64)((c1 >> 16) & 0xff) << 16) - b) / length;

	while (length > 0) {
		INT32 rr = (INT32)(r >> 16);
		INT32 gg = (INT32)(g >> 16);
		INT32 bb = (INT32)(b >> 16);
		if (rr < 0)
			rr = 0;
		else if (rr > 0xff)
			rr = 0xff;
		if (gg < 0)
			gg = 0;
		else if (gg > 0xff)
			gg = 0xff;
		if (bb < 0)
			bb = 0;
		else if (bb > 0xff)
			bb = 0xff;
		ZnGpuPutSolidPixel((INT32)(x >> 16) + DrvGpuDrawOffsetX, (INT32)(y >> 16) + DrvGpuDrawOffsetY,
		                   ZnGpuShadeComponent(rr) | (ZnGpuShadeComponent(gg) << 5) | (ZnGpuShadeComponent(bb) << 10), semi);
		x += stepx;
		y += stepy;
		r += stepr;
		g += stepg;
		b += stepb;
		length--;
	}
}

static inline INT64 ZnGpuEdge64(INT32 ax, INT32 ay, INT32 bx, INT32 by, INT32 cx, INT32 cy)
{
	return ((INT64)(cx - ax) * (by - ay)) - ((INT64)(cy - ay) * (bx - ax));
}

static inline INT32 ZnGpuCullTriangle(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 x2, INT32 y2)
{
	INT32 dx01 = x0 - x1;
	if (dx01 < 0)
		dx01 = -dx01;
	INT32 dy01 = y0 - y1;
	if (dy01 < 0)
		dy01 = -dy01;
	INT32 dx12 = x1 - x2;
	if (dx12 < 0)
		dx12 = -dx12;
	INT32 dy12 = y1 - y2;
	if (dy12 < 0)
		dy12 = -dy12;
	INT32 dx20 = x2 - x0;
	if (dx20 < 0)
		dx20 = -dx20;
	INT32 dy20 = y2 - y0;
	if (dy20 < 0)
		dy20 = -dy20;

	return dx01 > 1023 || dy01 > 1023 || dx12 > 1023 || dy12 > 1023 || dx20 > 1023 || dy20 > 1023;
}

static inline UINT16 ZnGpuReadRenderPixel(INT32 x, INT32 y)
{
	return DrvGpuVram[(((y & 0x3ff) << 10) | (x & 0x3ff)) & DrvGpuVramMask];
}

static inline UINT16 ZnGpuBlendColor(UINT16 tex, UINT32 shade, INT32 raw)
{
	if (raw)
		return tex;

	INT32 tr = tex & 0x1f;
	INT32 tg = (tex >> 5) & 0x1f;
	INT32 tb = (tex >> 10) & 0x1f;
	INT32 sr = shade & 0xff;
	INT32 sg = (shade >> 8) & 0xff;
	INT32 sb = (shade >> 16) & 0xff;

	tr = (tr * sr) / 128;
	tg = (tg * sg) / 128;
	tb = (tb * sb) / 128;
	if (tr > 0x1f)
		tr = 0x1f;
	if (tg > 0x1f)
		tg = 0x1f;
	if (tb > 0x1f)
		tb = 0x1f;

	return tr | (tg << 5) | (tb << 10) | (tex & 0x8000);
}

static inline UINT16 ZnGpuBlendSemiTransparent(UINT16 foreground, UINT16 background, UINT32 tpage)
{
	INT32 fr = foreground & 0x1f;
	INT32 fg = (foreground >> 5) & 0x1f;
	INT32 fb = (foreground >> 10) & 0x1f;
	INT32 br = background & 0x1f;
	INT32 bg = (background >> 5) & 0x1f;
	INT32 bb = (background >> 10) & 0x1f;
	INT32 abr = (tpage >> ((DrvGpuType == 2) ? 5 : 7)) & 3;

	switch (abr) {
		case 0:
			fr = (fr >> 1) + (br >> 1);
			fg = (fg >> 1) + (bg >> 1);
			fb = (fb >> 1) + (bb >> 1);
			break;
		case 1:
			fr += br;
			fg += bg;
			fb += bb;
			break;
		case 2:
			fr = br - fr;
			fg = bg - fg;
			fb = bb - fb;
			break;
		case 3:
			fr = (fr >> 2) + br;
			fg = (fg >> 2) + bg;
			fb = (fb >> 2) + bb;
			break;
	}

	if (fr < 0)
		fr = 0;
	if (fr > 0x1f)
		fr = 0x1f;
	if (fg < 0)
		fg = 0;
	if (fg > 0x1f)
		fg = 0x1f;
	if (fb < 0)
		fb = 0;
	if (fb > 0x1f)
		fb = 0x1f;

	return fr | (fg << 5) | (fb << 10);
}

static inline void ZnGpuPutSolidPixel(INT32 x, INT32 y, UINT16 color, INT32 semi)
{
	if (semi) {
		UINT32 tpage = DrvGpuTPage;
		UINT16 background = ZnGpuReadRenderPixel(x, y);
		color = ZnGpuBlendSemiTransparent(color, background, tpage);
	}

	ZnGpuPutPixel(x, y, color);
}

static inline void ZnGpuPutTexturedPixel(INT32 x, INT32 y, UINT16 texel, UINT16 color, UINT32 tpage, INT32 semi)
{
	if (semi && (texel & 0x8000)) {
		UINT16 background = ZnGpuReadRenderPixel(x, y);
		color = ZnGpuBlendSemiTransparent(color, background, tpage) | 0x8000;
	}

	ZnGpuPutPixel(x, y, color);
}

struct NamcosTextureState {
	UINT32 texx;
	UINT32 texy;
	UINT32 mode;
	UINT32 interleaved;
	UINT32 windowW;
	UINT32 windowH;
	UINT32 clutX;
	UINT32 clutY;
	const UINT16 *clutCache;
};

static inline void ZnGpuInitTextureState(NamcosTextureState &state, UINT32 clut, UINT32 tpage, const UINT16 *clutCache)
{
	state.texx = (tpage & 0x0f) << 6;
	state.interleaved = 0;

	if (DrvGpuType == 2) {
		state.texy = ((tpage & 0x10) << 4) | ((tpage & 0x800) >> 2);
		state.mode = (tpage >> 7) & 3;
	} else {
		state.texy = (tpage & 0x60) << 3;
		state.mode = (tpage >> 9) & 3;
		state.interleaved = tpage & 0x2000;
	}

	state.windowW = DrvGpuTextureWindowW;
	state.windowH = DrvGpuTextureWindowH;
	state.texx += (DrvGpuTextureWindowX & ~DrvGpuTextureWindowW) >> ((state.mode == 0) ? 2 : ((state.mode == 1) ? 1 : 0));
	state.texy += DrvGpuTextureWindowY & ~DrvGpuTextureWindowH;
	state.clutX = (clut & 0x3f) << 4;
	state.clutY = (clut >> 6) & 0x3ff;
	state.clutCache = clutCache;
}

static inline UINT16 ZnGpuFetchTexture(INT32 u, INT32 v, const NamcosTextureState &state)
{
	if (state.mode == 3)
		return 0;

	UINT32 x = 0;
	u &= state.windowW;
	v &= state.windowH;
	UINT32 y = (state.texy + (v & 0xff)) & 0x3ff;
	UINT16 data = 0;

	if (state.mode == 0) {
		if (state.interleaved) {
			x = state.texx + (((u >> 2) & ~0x3c) + ((v << 2) & 0x3c));
			y = state.texy + ((v & ~0x0f) + ((u >> 4) & 0x0f));
		} else {
			x = state.texx + ((u & 0xff) >> 2);
		}
		data = DrvGpuVram[(((y & 0x3ff) << 10) | (x & 0x3ff)) & DrvGpuVramMask];
		data = (data >> ((u & 3) << 2)) & 0x0f;
		if (state.clutCache)
			return state.clutCache[data];
		x = state.clutX + data;
		y = state.clutY;
		return DrvGpuVram[(((y & 0x3ff) << 10) | (x & 0x3ff)) & DrvGpuVramMask];
	}

	if (state.mode == 1) {
		if (state.interleaved) {
			x = state.texx + (((u >> 1) & ~0x78) + ((u << 2) & 0x40) + ((v << 3) & 0x38));
			y = state.texy + ((v & ~0x07) + ((u >> 5) & 0x07));
		} else {
			x = state.texx + ((u & 0xff) >> 1);
		}
		data = DrvGpuVram[(((y & 0x3ff) << 10) | (x & 0x3ff)) & DrvGpuVramMask];
		data = (u & 1) ? (data >> 8) : (data & 0xff);
		if (state.clutCache)
			return state.clutCache[data];
		x = state.clutX + data;
		y = state.clutY;
		return DrvGpuVram[(((y & 0x3ff) << 10) | (x & 0x3ff)) & DrvGpuVramMask];
	}

	x = state.texx + (u & 0xff);
	data = DrvGpuVram[(((y & 0x3ff) << 10) | (x & 0x3ff)) & DrvGpuVramMask];
	if (data == 0)
		return 0;
	return data;
}

static void ZnGpuDrawFlatPolyCore(const INT32 *px, const INT32 *py, INT32 points, UINT16 color, INT32 semi, INT32 *spans = NULL,
                                  INT32 spanY = 0)
{
	// GPU quads are two independent triangles, including concave/degenerate
	// quads generated for distant scenery. Do not walk their outer perimeter.
	if (points == 4 && spans == NULL) {
		ZnGpuDrawFlatPolyCore(px, py, 3, color, semi);
		INT32 x[3] = { px[2], px[1], px[3] };
		INT32 y[3] = { py[2], py[1], py[3] };
		ZnGpuDrawFlatPolyCore(x, y, 3, color, semi);
		return;
	}

	static const INT32 next3[3] = { 1, 2, 0 };
	static const INT32 prev3[3] = { 2, 0, 1 };
	static const INT32 next4[4] = { 1, 3, 0, 2 };
	static const INT32 prev4[4] = { 2, 0, 3, 1 };
	static const INT32 next4b[4] = { 0, 3, 1, 2 };
	static const INT32 prev4b[4] = { 0, 2, 3, 1 };
	const INT32 *next = next3;
	const INT32 *prev = prev3;
	INT32 left = 0;

	if (points == 4) {
		INT32 cull0 = ZnGpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2]);
		INT32 cull1 = ZnGpuCullTriangle(px[1], py[1], px[2], py[2], px[3], py[3]);
		if (cull0) {
			if (cull1)
				return;
			next = next4b;
			prev = prev4b;
			left = 1;
		} else if (cull1) {
			points = 3;
		} else {
			next = next4;
			prev = prev4;
		}
	} else if (ZnGpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2])) {
		return;
	}

	for (INT32 i = left + 1; i < points; i++) {
		if (py[i] < py[left] || (py[i] == py[left] && px[i] < px[left]))
			left = i;
	}
	INT32 right = left;
	INT32 y = py[right];
	UINT32 cx1 = 0, cx2 = 0;
	INT32 dx1 = 0, dx2 = 0;
	INT32 clipx1, clipy1, clipx2, clipy2;
	ZnGpuGetClip(clipx1, clipy1, clipx2, clipy2);

	for (;;) {
		if (y == py[left]) {
			while (y == py[prev[left]]) {
				left = prev[left];
				if (left == right)
					break;
			}
			cx1 = (UINT32)(UINT16)px[left] << 16;
			left = prev[left];
			INT32 distance = py[left] - y;
			if (distance < 1)
				return;
			dx1 = (INT32)(((UINT32)(UINT16)px[left] << 16) - cx1);
			dx1 /= distance;
		}

		if (y == py[right]) {
			while (y == py[next[right]]) {
				right = next[right];
				if (right == left)
					break;
			}
			cx2 = (UINT32)(UINT16)px[right] << 16;
			right = next[right];
			INT32 distance = py[right] - y;
			if (distance < 1)
				return;
			dx2 = (INT32)(((UINT32)(UINT16)px[right] << 16) - cx2);
			dx2 /= distance;
		}

		INT32 x1 = ((INT32)cx1 + 0xffff) >> 16;
		INT32 x2 = ((INT32)cx2 + 0xffff) >> 16;
		if (x1 > x2) {
			INT32 swap = x1;
			x1 = x2;
			x2 = swap;
		}
		INT32 distance = x2 - x1;
		if (distance > 0 && y >= clipy1 && y <= clipy2) {
			if (x1 < clipx1) {
				distance -= clipx1 - x1;
				x1 = clipx1;
			}
			if (x1 + distance > clipx2 + 1)
				distance = clipx2 - x1 + 1;
			if (spans != NULL && y >= spanY && y < spanY + 1024) {
				spans[(y - spanY) * 2 + 0] = x1;
				spans[(y - spanY) * 2 + 1] = x1 + distance;
			} else {
				for (INT32 x = x1; distance > 0; x++, distance--) {
					ZnGpuPutSolidPixel(x, y, color, semi);
				}
			}
		}

		cx1 += dx1;
		cx2 += dx2;
		y++;
	}
}

static void ZnGpuDrawFlatPolyPass(const INT32 *px, const INT32 *py, INT32 points, UINT16 color, INT32 semi)
{
	ZnGpuDrawFlatPolyCore(px, py, points, color, semi);
}

static void ZnGpuDrawFlatPoly(const INT32 *px, const INT32 *py, INT32 points, UINT16 color, INT32 semi)
{
	ZnGpuDrawFlatPolyPass(px, py, points, color, semi);
}

static void ZnGpuDrawTriangleFlat(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT16 color, INT32 semi)
{
	INT32 px[3] = { x0, x1, x2 };
	INT32 py[3] = { y0, y1, y2 };

	ZnGpuDrawFlatPoly(px, py, 3, color, semi);
}

static void ZnGpuDrawGouraudPolyMameCore(const INT32 *px, const INT32 *py, const UINT32 *pc, INT32 points, INT32 semi, INT32 *spans = NULL,
                                         INT32 spanY = 0)
{
	// MiSTer gpu_poly: quads use triangles (0,1,2) and (2,1,3).
	if (points == 4 && spans == NULL) {
		ZnGpuDrawGouraudPolyMameCore(px, py, pc, 3, semi);
		INT32 x[3] = { px[2], px[1], px[3] };
		INT32 y[3] = { py[2], py[1], py[3] };
		UINT32 c[3] = { pc[2], pc[1], pc[3] };
		ZnGpuDrawGouraudPolyMameCore(x, y, c, 3, semi);
		return;
	}
	INT64 area = (INT64)(px[1] - px[0]) * (py[2] - py[0]) - (INT64)(px[2] - px[0]) * (py[1] - py[0]);
	if (!area)
		return;
	INT32 colorX[3], colorY[3];
	for (INT32 ch = 0; ch < 3; ch++) {
		INT32 c0 = (pc[0] >> (ch * 8)) & 255;
		INT32 c1 = (pc[1] >> (ch * 8)) & 255;
		INT32 c2 = (pc[2] >> (ch * 8)) & 255;
		colorX[ch] = (INT32)(((INT64)(c1 - c0) * (py[2] - py[0]) - (INT64)(c2 - c0) * (py[1] - py[0])) * 65536 / area);
		colorY[ch] = (INT32)(((INT64)(px[1] - px[0]) * (c2 - c0) - (INT64)(px[2] - px[0]) * (c1 - c0)) * 65536 / area);
	}

	static const INT32 next3[3] = { 1, 2, 0 };
	static const INT32 prev3[3] = { 2, 0, 1 };
	static const INT32 next4[4] = { 1, 3, 0, 2 };
	static const INT32 prev4[4] = { 2, 0, 3, 1 };
	static const INT32 next4b[4] = { 0, 3, 1, 2 };
	static const INT32 prev4b[4] = { 0, 2, 3, 1 };
	const INT32 *next = next3;
	const INT32 *prev = prev3;
	INT32 left = 0;

	if (points == 4) {
		INT32 cull0 = ZnGpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2]);
		INT32 cull1 = ZnGpuCullTriangle(px[1], py[1], px[2], py[2], px[3], py[3]);
		if (cull0) {
			if (cull1)
				return;
			next = next4b;
			prev = prev4b;
			left = 1;
		} else if (cull1) {
			points = 3;
		} else {
			next = next4;
			prev = prev4;
		}
	} else if (ZnGpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2])) {
		return;
	}

	for (INT32 i = left + 1; i < points; i++) {
		if (py[i] < py[left] || (py[i] == py[left] && px[i] < px[left]))
			left = i;
	}
	INT32 right = left;
	INT32 y = py[right];
	UINT32 cx1 = 0;
	UINT32 cx2 = 0;
	INT32 dx1 = 0;
	INT32 dx2 = 0;
	INT32 clipx1, clipy1, clipx2, clipy2;
	ZnGpuGetClip(clipx1, clipy1, clipx2, clipy2);

	for (;;) {
		if (y == py[left]) {
			while (y == py[prev[left]]) {
				left = prev[left];
				if (left == right)
					break;
			}
			cx1 = (UINT32)(UINT16)px[left] << 16;
			left = prev[left];
			INT32 distance = py[left] - y;
			if (distance < 1)
				return;
			dx1 = (INT32)(((UINT32)(UINT16)px[left] << 16) - cx1);
			dx1 /= distance;
		}

		if (y == py[right]) {
			while (y == py[next[right]]) {
				right = next[right];
				if (right == left)
					break;
			}
			cx2 = (UINT32)(UINT16)px[right] << 16;
			right = next[right];
			INT32 distance = py[right] - y;
			if (distance < 1)
				return;
			dx2 = (INT32)(((UINT32)(UINT16)px[right] << 16) - cx2);
			dx2 /= distance;
		}

		INT32 x1 = ((INT32)cx1 + 0xffff) >> 16;
		INT32 x2 = ((INT32)cx2 + 0xffff) >> 16;
		if (x1 > x2) {
			INT32 tx = x1;
			x1 = x2;
			x2 = tx;
		}
		INT32 distance = x2 - x1;
		if (distance > 0 && y >= clipy1 && y <= clipy2) {
			// Interpolate RGB from the triangle plane, not truncated span endpoints.
			UINT32 color[3];
			for (INT32 ch = 0; ch < 3; ch++)
				color[ch] = (UINT32)((INT64)((pc[0] >> (ch * 8)) & 255) * 65536 + 0x8000 + (INT64)colorX[ch] * (x1 - px[0]) +
				                     (INT64)colorY[ch] * (y - py[0]));
			UINT32 r = color[0], g = color[1], b = color[2];
			INT32 dr = colorX[0], dg = colorX[1], db = colorX[2];
			if (x1 < clipx1) {
				r += dr * (clipx1 - x1);
				g += dg * (clipx1 - x1);
				b += db * (clipx1 - x1);
				distance -= clipx1 - x1;
				x1 = clipx1;
			}
			if (x1 + distance > clipx2 + 1)
				distance = clipx2 - x1 + 1;
			if (spans != NULL) {
				INT32 *row = spans + (y - spanY) * 8;
				row[0] = x1;
				row[1] = x1 + distance;
				row[2] = (INT32)r;
				row[3] = (INT32)g;
				row[4] = (INT32)b;
				row[5] = dr;
				row[6] = dg;
				row[7] = db;
			} else {
				for (INT32 x = x1; distance > 0; x++, distance--, r += dr, g += dg, b += db) {
					INT32 rr = (r >> 16) & 0xff;
					INT32 gg = (g >> 16) & 0xff;
					INT32 bb = (b >> 16) & 0xff;
					ZnGpuPutSolidPixel(x, y, ZnGpuShadeComponent(rr) | (ZnGpuShadeComponent(gg) << 5) | (ZnGpuShadeComponent(bb) << 10),
					                   semi);
				}
			}
		}

		cx1 += dx1;
		cx2 += dx2;
		y++;
	}
}

static void ZnGpuDrawGouraudPolyMamePass(const INT32 *px, const INT32 *py, const UINT32 *pc, INT32 points, INT32 semi)
{
	ZnGpuDrawGouraudPolyMameCore(px, py, pc, points, semi);
}

static void ZnGpuDrawGouraudPolyMame(const INT32 *px, const INT32 *py, const UINT32 *pc, INT32 points, INT32 semi)
{
	ZnGpuDrawGouraudPolyMamePass(px, py, pc, points, semi);
}

static void ZnGpuDrawGouraudPoly(INT32 quad)
{
	UINT32 c0 = DrvGpuPacket[0] & 0x00ffffff;
	INT32 semi = DrvGpuPacket[0] & 0x02000000;
	INT32 x0 = ZnGpuPolyX(DrvGpuPacket[1]);
	INT32 y0 = ZnGpuPolyY(DrvGpuPacket[1]);
	UINT32 c1 = DrvGpuPacket[2] & 0x00ffffff;
	INT32 x1 = ZnGpuPolyX(DrvGpuPacket[3]);
	INT32 y1 = ZnGpuPolyY(DrvGpuPacket[3]);
	UINT32 c2 = DrvGpuPacket[4] & 0x00ffffff;
	INT32 x2 = ZnGpuPolyX(DrvGpuPacket[5]);
	INT32 y2 = ZnGpuPolyY(DrvGpuPacket[5]);

	INT32 px[4] = { x0, x1, x2, 0 };
	INT32 py[4] = { y0, y1, y2, 0 };
	UINT32 pc[4] = { c0, c1, c2, 0 };
	if (quad) {
		pc[3] = DrvGpuPacket[6] & 0x00ffffff;
		px[3] = ZnGpuPolyX(DrvGpuPacket[7]);
		py[3] = ZnGpuPolyY(DrvGpuPacket[7]);
	}

	ZnGpuDrawGouraudPolyMame(px, py, pc, quad ? 4 : 3, semi);
}

static void ZnGpuDrawFlatQuad()
{
	UINT16 color = ZnGpuColor(DrvGpuPacket[0]);
	INT32 x0 = ZnGpuPolyX(DrvGpuPacket[1]);
	INT32 y0 = ZnGpuPolyY(DrvGpuPacket[1]);
	INT32 x1 = ZnGpuPolyX(DrvGpuPacket[2]);
	INT32 y1 = ZnGpuPolyY(DrvGpuPacket[2]);
	INT32 x2 = ZnGpuPolyX(DrvGpuPacket[3]);
	INT32 y2 = ZnGpuPolyY(DrvGpuPacket[3]);
	INT32 x3 = ZnGpuPolyX(DrvGpuPacket[4]);
	INT32 y3 = ZnGpuPolyY(DrvGpuPacket[4]);

	INT32 px[4] = { x0, x1, x2, x3 };
	INT32 py[4] = { y0, y1, y2, y3 };

	ZnGpuDrawFlatPoly(px, py, 4, color, DrvGpuPacket[0] & 0x02000000);
}

static void ZnGpuDrawTexturedPolyMameCore(const INT32 *px, const INT32 *py, const INT32 *pu, const INT32 *pv, const UINT32 *pc,
                                          INT32 points, UINT32 clut, UINT32 tpage, INT32 raw, INT32 semi, INT32 gouraud,
                                          const NamcosTextureState &texture, INT32 *spans = NULL, INT32 spanY = 0)
{
	if (points == 4 && spans == NULL) {
		ZnGpuDrawTexturedPolyMameCore(px, py, pu, pv, pc, 3, clut, tpage, raw, semi, gouraud, texture);
		const INT32 indices[3] = { 2, 1, 3 };
		INT32 x[3], y[3], u[3], v[3];
		UINT32 color[3];
		for (INT32 i = 0; i < 3; i++) {
			INT32 j = indices[i];
			x[i] = px[j];
			y[i] = py[j];
			u[i] = pu[j];
			v[i] = pv[j];
			color[i] = pc[j];
		}
		ZnGpuDrawTexturedPolyMameCore(x, y, u, v, color, 3, clut, tpage, raw, semi, gouraud, texture);
		return;
	}
	// MiSTer interpolates one affine UV plane per triangle and starts at
	// texel centre (+0.5). Re-interpolating truncated span endpoints can
	// select the preceding atlas row and leak tree/sky texels at seams.
	INT64 area = (INT64)(px[1] - px[0]) * (py[2] - py[0]) - (INT64)(px[2] - px[0]) * (py[1] - py[0]);
	if (!area)
		return;
	INT32 planeUx = (INT32)(((INT64)(pu[1] - pu[0]) * (py[2] - py[0]) - (INT64)(pu[2] - pu[0]) * (py[1] - py[0])) * 65536 / area);
	INT32 planeUy = (INT32)(((INT64)(px[1] - px[0]) * (pu[2] - pu[0]) - (INT64)(px[2] - px[0]) * (pu[1] - pu[0])) * 65536 / area);
	INT32 planeVx = (INT32)(((INT64)(pv[1] - pv[0]) * (py[2] - py[0]) - (INT64)(pv[2] - pv[0]) * (py[1] - py[0])) * 65536 / area);
	INT32 planeVy = (INT32)(((INT64)(px[1] - px[0]) * (pv[2] - pv[0]) - (INT64)(px[2] - px[0]) * (pv[1] - pv[0])) * 65536 / area);
	static const INT32 next3[3] = { 1, 2, 0 };
	static const INT32 prev3[3] = { 2, 0, 1 };
	static const INT32 next4[4] = { 1, 3, 0, 2 };
	static const INT32 prev4[4] = { 2, 0, 3, 1 };
	static const INT32 next4b[4] = { 0, 3, 1, 2 };
	static const INT32 prev4b[4] = { 0, 2, 3, 1 };
	const INT32 *next = next3;
	const INT32 *prev = prev3;
	INT32 left = 0;

	if (points == 4) {
		INT32 cull0 = ZnGpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2]);
		INT32 cull1 = ZnGpuCullTriangle(px[1], py[1], px[2], py[2], px[3], py[3]);
		if (cull0) {
			if (cull1)
				return;
			next = next4b;
			prev = prev4b;
			left = 1;
		} else if (cull1) {
			points = 3;
		} else {
			next = next4;
			prev = prev4;
		}
	} else if (ZnGpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2])) {
		return;
	}

	for (INT32 i = left + 1; i < points; i++) {
		if (py[i] < py[left] || (py[i] == py[left] && px[i] < px[left]))
			left = i;
	}
	INT32 right = left;
	INT32 y = py[right];
	UINT32 cx1 = 0, cr1 = 0, cg1 = 0, cb1 = 0;
	UINT32 cx2 = 0, cr2 = 0, cg2 = 0, cb2 = 0;
	INT32 dx1 = 0, dr1 = 0, dg1 = 0, db1 = 0;
	INT32 dx2 = 0, dr2 = 0, dg2 = 0, db2 = 0;
	UINT32 cu1 = 0, cv1 = 0, cu2 = 0, cv2 = 0;
	INT32 du1 = 0, dv1 = 0, du2 = 0, dv2 = 0;
	INT32 clipx1, clipy1, clipx2, clipy2;
	ZnGpuGetClip(clipx1, clipy1, clipx2, clipy2);

	for (;;) {
		if (y == py[left]) {
			while (y == py[prev[left]]) {
				left = prev[left];
				if (left == right)
					break;
			}
			cx1 = (UINT32)(UINT16)px[left] << 16;
			cr1 = (raw ? 0x80 : (pc[left] & 0xff)) << 16;
			cg1 = (raw ? 0x80 : ((pc[left] >> 8) & 0xff)) << 16;
			cb1 = (raw ? 0x80 : ((pc[left] >> 16) & 0xff)) << 16;
			cu1 = (UINT32)(UINT16)pu[left] << 16;
			cv1 = (UINT32)(UINT16)pv[left] << 16;
			left = prev[left];
			INT32 distance = py[left] - y;
			if (distance < 1)
				return;
			dx1 = (INT32)(((UINT32)(UINT16)px[left] << 16) - cx1);
			dr1 = raw ? 0 : (INT32)(((pc[left] & 0xff) << 16) - cr1);
			dg1 = raw ? 0 : (INT32)((((pc[left] >> 8) & 0xff) << 16) - cg1);
			db1 = raw ? 0 : (INT32)((((pc[left] >> 16) & 0xff) << 16) - cb1);
			du1 = (INT32)(((UINT32)(UINT16)pu[left] << 16) - cu1);
			dv1 = (INT32)(((UINT32)(UINT16)pv[left] << 16) - cv1);
			dx1 /= distance;
			dr1 /= distance;
			dg1 /= distance;
			db1 /= distance;
			du1 /= distance;
			dv1 /= distance;
		}

		if (y == py[right]) {
			while (y == py[next[right]]) {
				right = next[right];
				if (right == left)
					break;
			}
			cx2 = (UINT32)(UINT16)px[right] << 16;
			cr2 = (raw ? 0x80 : (pc[right] & 0xff)) << 16;
			cg2 = (raw ? 0x80 : ((pc[right] >> 8) & 0xff)) << 16;
			cb2 = (raw ? 0x80 : ((pc[right] >> 16) & 0xff)) << 16;
			cu2 = (UINT32)(UINT16)pu[right] << 16;
			cv2 = (UINT32)(UINT16)pv[right] << 16;
			right = next[right];
			INT32 distance = py[right] - y;
			if (distance < 1)
				return;
			dx2 = (INT32)(((UINT32)(UINT16)px[right] << 16) - cx2);
			dr2 = raw ? 0 : (INT32)(((pc[right] & 0xff) << 16) - cr2);
			dg2 = raw ? 0 : (INT32)((((pc[right] >> 8) & 0xff) << 16) - cg2);
			db2 = raw ? 0 : (INT32)((((pc[right] >> 16) & 0xff) << 16) - cb2);
			du2 = (INT32)(((UINT32)(UINT16)pu[right] << 16) - cu2);
			dv2 = (INT32)(((UINT32)(UINT16)pv[right] << 16) - cv2);
			dx2 /= distance;
			dr2 /= distance;
			dg2 /= distance;
			db2 /= distance;
			du2 /= distance;
			dv2 /= distance;
		}

		// Hardware starts/ends spans at the ceiling of their edge positions.
		INT32 edge1 = ((INT32)cx1 + 0xffff) >> 16;
		INT32 x1 = edge1;
		INT32 x2 = ((INT32)cx2 + 0xffff) >> 16;
		UINT32 r = cr1;
		UINT32 g = cg1;
		UINT32 b = cb1;
		UINT32 u = cu1;
		UINT32 v = cv1;
		if (x1 > x2) {
			INT32 tx = x1;
			x1 = x2;
			x2 = tx;
			r = cr2;
			g = cg2;
			b = cb2;
			u = cu2;
			v = cv2;
		}
		INT32 distance = x2 - x1;
		INT32 drawy = y + DrvGpuDrawOffsetY;
		if (distance > 0 && drawy >= clipy1 && drawy <= clipy2) {
			INT32 side1 = x1 == edge1;
			INT32 dr = (INT32)(side1 ? (cr2 - cr1) : (cr1 - cr2)) / distance;
			INT32 dg = (INT32)(side1 ? (cg2 - cg1) : (cg1 - cg2)) / distance;
			INT32 db = (INT32)(side1 ? (cb2 - cb1) : (cb1 - cb2)) / distance;
			INT32 du = planeUx, dv = planeVx;
			u = (UINT32)((INT64)pu[0] * 65536 + 0x8000 + (INT64)planeUx * (x1 - px[0]) + (INT64)planeUy * (y - py[0]));
			v = (UINT32)((INT64)pv[0] * 65536 + 0x8000 + (INT64)planeVx * (x1 - px[0]) + (INT64)planeVy * (y - py[0]));
			INT32 drawx = x1 + DrvGpuDrawOffsetX;
			if (drawx < clipx1) {
				INT32 clipped = clipx1 - drawx;
				r += dr * clipped;
				g += dg * clipped;
				b += db * clipped;
				u += du * clipped;
				v += dv * clipped;
				distance -= clipped;
				drawx = clipx1;
			}
			if (drawx + distance > clipx2 + 1)
				distance = clipx2 - drawx + 1;
			if (spans != NULL) {
				INT32 *row = spans + (drawy - spanY) * 12;
				row[0] = drawx;
				row[1] = drawx + distance;
				row[2] = (INT32)r;
				row[3] = (INT32)g;
				row[4] = (INT32)b;
				row[5] = dr;
				row[6] = dg;
				row[7] = db;
				row[8] = (INT32)u;
				row[9] = (INT32)v;
				row[10] = du;
				row[11] = dv;
			} else {
				for (INT32 x = drawx; distance > 0; x++, distance--, r += dr, g += dg, b += db, u += du, v += dv) {
					UINT16 texel = ZnGpuFetchTexture((UINT16)(u >> 16), (UINT16)(v >> 16), texture);
					if (texel) {
						UINT32 shade =
						    gouraud ? ((UINT32)(r >> 16) & 0xff) | (((UINT32)(g >> 16) & 0xff) << 8) | (((UINT32)(b >> 16) & 0xff) << 16)
							        : pc[0];
						ZnGpuPutTexturedPixel(x, drawy, texel, ZnGpuBlendColor(texel, shade, raw), tpage, semi);
					}
				}
			}
		}

		cx1 += dx1;
		cr1 += dr1;
		cg1 += dg1;
		cb1 += db1;
		cu1 += du1;
		cv1 += dv1;
		cx2 += dx2;
		cr2 += dr2;
		cg2 += dg2;
		cb2 += db2;
		cu2 += du2;
		cv2 += dv2;
		y++;
	}
}

static INT32 ZnGpuBuildClutCache(UINT16 *cache, UINT32 clut, UINT32 tpage)
{
	const UINT32 mode = (DrvGpuType == 2) ? ((tpage >> 7) & 3) : ((tpage >> 9) & 3);
	const INT32 count = (mode == 0) ? 16 : ((mode == 1) ? 256 : 0);
	const UINT32 x = (clut & 0x3f) << 4;
	const UINT32 y = (clut >> 6) & 0x3ff;

	for (INT32 i = 0; i < count; i++) {
		cache[i] = DrvGpuVram[((y << 10) | ((x + i) & 0x3ff)) & DrvGpuVramMask];
	}

	return count;
}

static void ZnGpuDrawTexturedPolyMamePass(const INT32 *px, const INT32 *py, const INT32 *pu, const INT32 *pv, const UINT32 *pc,
                                          INT32 points, UINT32 clut, UINT32 tpage, INT32 raw, INT32 semi, INT32 gouraud)
{
	UINT16 clutCache[256];
	const INT32 clutCount = ZnGpuBuildClutCache(clutCache, clut, tpage);
	NamcosTextureState texture;
	ZnGpuInitTextureState(texture, clut, tpage, clutCount ? clutCache : NULL);
	ZnGpuDrawTexturedPolyMameCore(px, py, pu, pv, pc, points, clut, tpage, raw, semi, gouraud, texture);
}

static void ZnGpuDrawTexturedPolyMame(const INT32 *px, const INT32 *py, const INT32 *pu, const INT32 *pv, const UINT32 *pc, INT32 points,
                                      UINT32 clut, UINT32 tpage, INT32 raw, INT32 semi, INT32 gouraud)
{
	ZnGpuDrawTexturedPolyMamePass(px, py, pu, pv, pc, points, clut, tpage, raw, semi, gouraud);
}

static void ZnGpuDrawTexturedPoly(INT32 gouraud, INT32 quad)
{
	UINT32 c0 = DrvGpuPacket[0] & 0x00ffffff;
	UINT32 c1 = c0;
	UINT32 c2 = c0;
	UINT32 c3 = c0;
	INT32 x0, y0, x1, y1, x2, y2, x3 = 0, y3 = 0;
	INT32 u0, v0, u1, v1, u2, v2, u3 = 0, v3 = 0;
	UINT32 clut, tpage;
	INT32 raw = DrvGpuPacket[0] & 0x01000000;
	INT32 semi = DrvGpuPacket[0] & 0x02000000;

	if (gouraud) {
		x0 = ZnGpuS11Coord(DrvGpuPacket[1]);
		y0 = ZnGpuS11Coord(DrvGpuPacket[1] >> 16);
		u0 = DrvGpuPacket[2] & 0xff;
		v0 = (DrvGpuPacket[2] >> 8) & 0xff;
		clut = DrvGpuPacket[2] >> 16;
		c1 = DrvGpuPacket[3] & 0x00ffffff;
		x1 = ZnGpuS11Coord(DrvGpuPacket[4]);
		y1 = ZnGpuS11Coord(DrvGpuPacket[4] >> 16);
		u1 = DrvGpuPacket[5] & 0xff;
		v1 = (DrvGpuPacket[5] >> 8) & 0xff;
		tpage = DrvGpuPacket[5] >> 16;
		c2 = DrvGpuPacket[6] & 0x00ffffff;
		x2 = ZnGpuS11Coord(DrvGpuPacket[7]);
		y2 = ZnGpuS11Coord(DrvGpuPacket[7] >> 16);
		u2 = DrvGpuPacket[8] & 0xff;
		v2 = (DrvGpuPacket[8] >> 8) & 0xff;
		if (quad) {
			c3 = DrvGpuPacket[9] & 0x00ffffff;
			x3 = ZnGpuS11Coord(DrvGpuPacket[10]);
			y3 = ZnGpuS11Coord(DrvGpuPacket[10] >> 16);
			u3 = DrvGpuPacket[11] & 0xff;
			v3 = (DrvGpuPacket[11] >> 8) & 0xff;
		}
	} else {
		x0 = ZnGpuS11Coord(DrvGpuPacket[1]);
		y0 = ZnGpuS11Coord(DrvGpuPacket[1] >> 16);
		u0 = DrvGpuPacket[2] & 0xff;
		v0 = (DrvGpuPacket[2] >> 8) & 0xff;
		clut = DrvGpuPacket[2] >> 16;
		x1 = ZnGpuS11Coord(DrvGpuPacket[3]);
		y1 = ZnGpuS11Coord(DrvGpuPacket[3] >> 16);
		u1 = DrvGpuPacket[4] & 0xff;
		v1 = (DrvGpuPacket[4] >> 8) & 0xff;
		tpage = DrvGpuPacket[4] >> 16;
		x2 = ZnGpuS11Coord(DrvGpuPacket[5]);
		y2 = ZnGpuS11Coord(DrvGpuPacket[5] >> 16);
		u2 = DrvGpuPacket[6] & 0xff;
		v2 = (DrvGpuPacket[6] >> 8) & 0xff;
		if (quad) {
			x3 = ZnGpuS11Coord(DrvGpuPacket[7]);
			y3 = ZnGpuS11Coord(DrvGpuPacket[7] >> 16);
			u3 = DrvGpuPacket[8] & 0xff;
			v3 = (DrvGpuPacket[8] >> 8) & 0xff;
		}
	}

	if (DrvGpuType == 2) {
		DrvGpuStatus = (DrvGpuStatus & 0xffff7800) | (tpage & 0x7ff) | ((tpage & 0x800) << 4);
	} else {
		DrvGpuStatus = (DrvGpuStatus & 0xffffe000) | (tpage & 0x1fff);
	}
	DrvGpuTPage = tpage;

	INT32 px[4] = { x0, x1, x2, x3 };
	INT32 py[4] = { y0, y1, y2, y3 };
	INT32 pu[4] = { u0, u1, u2, u3 };
	INT32 pv[4] = { v0, v1, v2, v3 };
	UINT32 pc[4] = { c0, c1, c2, c3 };
	ZnGpuDrawTexturedPolyMame(px, py, pu, pv, pc, quad ? 4 : 3, clut, tpage, raw, semi, gouraud);
}

static void ZnGpuDrawTexturedRect(INT32 width, INT32 height)
{
	struct NamcosTexturedRectContext {
		UINT32 color;
		INT32 raw;
		INT32 semi;
		INT32 x0;
		INT32 y0;
		INT32 u0;
		INT32 v0;
		INT32 width;
		INT32 du;
		INT32 dv;
		UINT32 tpage;
		UINT16 clutCache[256];
		NamcosTextureState texture;
	};
	struct NamcosTexturedRectRows {
		static void Run(void *opaque, INT32 begin, INT32 end)
		{
			NamcosTexturedRectContext *context = (NamcosTexturedRectContext *)opaque;
			for (INT32 y = begin; y < end; y++) {
				for (INT32 x = 0; x < context->width; x++) {
					UINT16 texel = ZnGpuFetchTexture(context->u0 + x * context->du, context->v0 + y * context->dv, context->texture);
					if (texel) {
						ZnGpuPutTexturedPixel(context->x0 + x, context->y0 + y, texel, ZnGpuBlendColor(texel, context->color, context->raw),
						                      context->tpage, context->semi);
					}
				}
			}
		}
	};

	NamcosTexturedRectContext context;
	context.color = DrvGpuPacket[0] & 0x00ffffff;
	context.raw = DrvGpuPacket[0] & 0x01000000;
	context.semi = DrvGpuPacket[0] & 0x02000000;
	INT32 x0 = ZnGpuPolyX(DrvGpuPacket[1]);
	INT32 y0 = ZnGpuPolyY(DrvGpuPacket[1]);
	INT32 u0 = DrvGpuPacket[2] & 0xff;
	INT32 v0 = (DrvGpuPacket[2] >> 8) & 0xff;
	UINT32 clut = DrvGpuPacket[2] >> 16;
	UINT32 tpage = DrvGpuTPage;
	const INT32 clutCount = ZnGpuBuildClutCache(context.clutCache, clut, tpage);

	if (width == 0 || height == 0) {
		width = DrvGpuPacket[3] & 0xffff;
		height = DrvGpuPacket[3] >> 16;
	}

	INT32 du = 1;
	INT32 dv = 1;
	if (DrvGpuType == 2) {
		if (tpage & 0x1000) {
			du = -1;
		}
		if (tpage & 0x2000) {
			dv = -1;
		}
	}

	context.x0 = x0;
	context.y0 = y0;
	context.u0 = u0;
	context.v0 = v0;
	context.width = width;
	context.du = du;
	context.dv = dv;
	context.tpage = tpage;
	ZnGpuInitTextureState(context.texture, clut, tpage, clutCount ? context.clutCache : NULL);
	NamcosTexturedRectRows::Run(&context, 0, height);
}

static void ZnGpuExecutePacket()
{
	UINT8 command = DrvGpuPacket[0] >> 24;

	switch (command) {
		case 0x02:
			ZnGpuFillRect();
			break;

		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			ZnGpuDrawTriangleFlat(ZnGpuPolyX(DrvGpuPacket[1]), ZnGpuPolyY(DrvGpuPacket[1]), ZnGpuPolyX(DrvGpuPacket[2]),
			                      ZnGpuPolyY(DrvGpuPacket[2]), ZnGpuPolyX(DrvGpuPacket[3]), ZnGpuPolyY(DrvGpuPacket[3]),
			                      ZnGpuColor(DrvGpuPacket[0]), DrvGpuPacket[0] & 0x02000000);
			break;

		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
			ZnGpuDrawTexturedPoly(0, 0);
			break;

		case 0x28:
		case 0x29:
		case 0x2a:
		case 0x2b:
			ZnGpuDrawFlatQuad();
			break;

		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
			ZnGpuDrawGouraudPoly(0);
			break;

		case 0x2c:
		case 0x2d:
		case 0x2e:
		case 0x2f:
			ZnGpuDrawTexturedPoly(0, 1);
			break;

		case 0x38:
		case 0x39:
		case 0x3a:
		case 0x3b:
			ZnGpuDrawGouraudPoly(1);
			break;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
			ZnGpuDrawLine();
			break;

		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
			ZnGpuDrawTexturedPoly(1, 0);
			break;

		case 0x3c:
		case 0x3d:
		case 0x3e:
		case 0x3f:
			ZnGpuDrawTexturedPoly(1, 1);
			break;

		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
			ZnGpuDrawFlatRectVariable();
			break;

		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
			ZnGpuDrawTexturedRect(0, 0);
			break;

		case 0x68:
		case 0x69:
		case 0x6a:
		case 0x6b:
			ZnGpuDrawDot();
			break;

		case 0x6c:
		case 0x6d:
		case 0x6e:
		case 0x6f:
			ZnGpuDrawTexturedRect(1, 1);
			break;

		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
			ZnGpuDrawFlatRect(8, 8);
			break;

		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
			ZnGpuDrawTexturedRect(8, 8);
			break;

		case 0x78:
		case 0x79:
		case 0x7a:
		case 0x7b:
			ZnGpuDrawFlatRect(16, 16);
			break;

		case 0x7c:
		case 0x7d:
		case 0x7e:
		case 0x7f:
			ZnGpuDrawTexturedRect(16, 16);
			break;

		case 0x80: {
			ZnGpuCopyVram();
			break;
		}

		case 0xa0: {
			DrvGpuImageX = DrvGpuPacket[1] & 0xffff;
			DrvGpuImageY = DrvGpuPacket[1] >> 16;
			DrvGpuImageW = DrvGpuPacket[2] & 0xffff;
			DrvGpuImageH = DrvGpuPacket[2] >> 16;
			DrvGpuImageCount = DrvGpuImageW * DrvGpuImageH;
			// Gallop Racer compatibility correction, not PSX hardware behavior.
			// Its file-offset table upload corrupts tree texture row 0 (also in MAME).
			// Consume the complete payload so subsequent GPU commands stay aligned.
			const char *game = BurnDrvGetTextA(DRV_NAME);
			DrvGpuDiscardImage =
			    (!strcmp(game, "glpracr") || !strcmp(game, "glpracrj")) && DrvGpuPacket[1] == 0x00000c0c && DrvGpuPacket[2] == 0x000103ff;
			DrvGpuImageCurX = DrvGpuImageX & 0x3ff;
			DrvGpuImageCurY = DrvGpuImageY & 0x3ff;
			DrvGpuImageColumn = 0;
			return;
		}

		case 0xc0:
			DrvGpuReadX = DrvGpuPacket[1] & 0xffff;
			DrvGpuReadY = DrvGpuPacket[1] >> 16;
			DrvGpuReadW = DrvGpuPacket[2] & 0xffff;
			DrvGpuReadH = DrvGpuPacket[2] >> 16;
			DrvGpuReadCount = DrvGpuReadW * DrvGpuReadH;
			if (DrvGpuReadCount)
				DrvGpuStatus |= 1 << 27;
			break;

		case 0xe1:
			DrvGpuTPage = DrvGpuPacket[0] & 0x3fff;
			if (DrvGpuType == 2) {
				DrvGpuStatus = (DrvGpuStatus & 0xffff7800) | (DrvGpuPacket[0] & 0x7ff) | ((DrvGpuPacket[0] & 0x800) << 4);
			} else {
				DrvGpuStatus = (DrvGpuStatus & 0xffffe000) | (DrvGpuPacket[0] & 0x1fff);
			}
			break;

		case 0xe2:
			DrvGpuTextureWindowY = ((DrvGpuPacket[0] >> 15) & 0x1f) << 3;
			DrvGpuTextureWindowX = ((DrvGpuPacket[0] >> 10) & 0x1f) << 3;
			DrvGpuTextureWindowH = 255 - (((DrvGpuPacket[0] >> 5) & 0x1f) << 3);
			DrvGpuTextureWindowW = 255 - ((DrvGpuPacket[0] & 0x1f) << 3);
			break;

		case 0xe3:
			DrvGpuDrawX1 = DrvGpuPacket[0] & 1023;
			DrvGpuDrawY1 = (DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 10 : 12)) & 1023;
			break;

		case 0xe4:
			DrvGpuDrawX2 = DrvGpuPacket[0] & 1023;
			DrvGpuDrawY2 = (DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 10 : 12)) & 1023;
			break;

		case 0xe5:
			DrvGpuDrawOffsetX = (DrvGpuPacket[0] & 0x400) ? (INT32)(DrvGpuPacket[0] & 2047) - 2048 : (INT32)(DrvGpuPacket[0] & 2047);
			DrvGpuDrawOffsetY = ((DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 11 : 12)) & 0x400)
			                        ? (INT32)((DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 11 : 12)) & 2047) - 2048
			                        : (INT32)((DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 11 : 12)) & 2047);
			break;

		case 0xe6:
			DrvGpuDrawStp = DrvGpuPacket[0] & 1;
			DrvGpuCheckStp = (DrvGpuPacket[0] >> 1) & 1;
			DrvGpuStatus &= ~(3 << 11);
			DrvGpuStatus |= (DrvGpuPacket[0] & 3) << 11;
			break;

		default:
			break;
	}

	DrvGpuPacketPos = 0;
	DrvGpuPacketLen = 0;
}

static void ZnGpuWrite(UINT32 data)
{
	if (DrvGpuImageCount) {
		ZnGpuWriteImage(data);
		return;
	}

	if (DrvGpuPolyline) {
		INT32 gouraud = (DrvGpuPolyline & 0x10) != 0;
		DrvGpuPacket[DrvGpuPacketPos] = data;

		if (!gouraud) {
			if (DrvGpuPacketPos < 3) {
				DrvGpuPacketPos++;
				return;
			}

			ZnGpuDrawLine();
			if ((DrvGpuPacket[3] & 0xf000f000) != 0x50005000) {
				DrvGpuPacket[1] = DrvGpuPacket[2];
				DrvGpuPacket[2] = DrvGpuPacket[3];
				DrvGpuPacketPos = 3;
				return;
			}
		} else {
			if (DrvGpuPacketPos < 5 && (DrvGpuPacketPos != 4 || (DrvGpuPacket[4] & 0xf000f000) != 0x50005000)) {
				DrvGpuPacketPos++;
				return;
			}

			ZnGpuDrawGouraudLine();
			if ((DrvGpuPacket[4] & 0xf000f000) != 0x50005000) {
				DrvGpuPacket[0] = (DrvGpuPacket[0] & 0xff000000) | (DrvGpuPacket[2] & 0x00ffffff);
				DrvGpuPacket[1] = DrvGpuPacket[3];
				DrvGpuPacket[2] = DrvGpuPacket[4];
				DrvGpuPacket[3] = DrvGpuPacket[5];
				DrvGpuPacketPos = 4;
				return;
			}
		}

		DrvGpuPolyline = 0;
		DrvGpuPacketPos = 0;
		return;
	}

	if (DrvGpuPacketPos == 0) {
		UINT8 command = data >> 24;
		if ((command >= 0x48 && command <= 0x4f) || (command >= 0x58 && command <= 0x5f)) {
			DrvGpuPolyline = command;
			DrvGpuPacket[0] = data;
			DrvGpuPacketPos = 1;
			return;
		}
		DrvGpuPacketLen = ZnGpuPacketLen(command);
	}

	DrvGpuPacket[DrvGpuPacketPos++] = data;

	if (DrvGpuPacketPos >= DrvGpuPacketLen) {
		ZnGpuExecutePacket();
	}
}

static void ZnGpuControl(UINT32 data)
{
	switch (data >> 24) {
		case 0x00:
			DrvGpuStatus = 0x14802000;
			DrvGpuTPage = 0;
			DrvGpuInfo = 0;
			DrvGpuPacketPos = 0;
			DrvGpuPacketLen = 0;
			DrvGpuPolyline = 0;
			DrvGpuTextureWindowX = 0;
			DrvGpuTextureWindowY = 0;
			DrvGpuTextureWindowW = 255;
			DrvGpuTextureWindowH = 255;
			DrvGpuDrawStp = 0;
			DrvGpuCheckStp = 0;
			DrvGpuImageCount = 0;
			DrvGpuDiscardImage = 0;
			DrvGpuReadCount = 0;
			DrvGpuDisplayX = 0;
			DrvGpuDisplayY = 0;
			DrvGpuDrawX1 = 0;
			DrvGpuDrawY1 = 0;
			DrvGpuDrawX2 = 1023;
			DrvGpuDrawY2 = 1023;
			DrvGpuDrawOffsetX = 0;
			DrvGpuDrawOffsetY = 0;
			DrvGpuHorizStart = 0x260;
			DrvGpuHorizEnd = 0xc60;
			DrvGpuVertStart = 0x010;
			DrvGpuVertEnd = 0x100;
			ZnGpuUpdateVisibleArea();
			break;

		case 0x01:
			// GP1 command-buffer reset also aborts an unfinished GP0 image upload.
			// Otherwise the next command header becomes texture pixels.
			DrvGpuImageCount = 0;
			DrvGpuDiscardImage = 0;
			DrvGpuImageColumn = 0;
			DrvGpuPacketPos = 0;
			DrvGpuPacketLen = 0;
			DrvGpuPolyline = 0;
			break;

		case 0x02:
			break;

		case 0x03:
			DrvGpuStatus &= ~(1 << 23);
			DrvGpuStatus |= (data & 1) << 23;
			break;

		case 0x04:
			DrvGpuStatus &= ~(3 << 29);
			DrvGpuStatus |= (data & 3) << 29;
			DrvGpuStatus &= ~(1 << 25);
			if ((data & 3) == 1 || (data & 3) == 2) {
				DrvGpuStatus |= 1 << 25;
			}
			break;

		case 0x05:
			DrvGpuDisplayX = data & 1023;
			DrvGpuDisplayY = (data >> ((DrvGpuType == 2) ? 10 : 12)) & 1023;
			break;

		case 0x06:
			DrvGpuHorizStart = data & 4095;
			DrvGpuHorizEnd = (data >> 12) & 4095;
			break;

		case 0x07:
			DrvGpuVertStart = data & 1023;
			DrvGpuVertEnd = (data >> 10) & 2047;
			break;

		case 0x08:
			DrvGpuStatus &= ~(127 << 16);
			DrvGpuStatus |= (data & 0x3f) << 17;
			DrvGpuStatus |= ((data & 0x40) >> 6) << 16;
			ZnGpuUpdateVisibleArea();
			break;

		case 0x10:
			switch (data & 0xff) {
				case 0x03:
					DrvGpuInfo = DrvGpuDrawX1 | (DrvGpuDrawY1 << ((DrvGpuType == 2) ? 10 : 12));
					break;

				case 0x04:
					DrvGpuInfo = DrvGpuDrawX2 | (DrvGpuDrawY2 << ((DrvGpuType == 2) ? 10 : 12));
					break;

				case 0x05:
					DrvGpuInfo = (DrvGpuDrawOffsetX & 2047) | ((DrvGpuDrawOffsetY & 2047) << ((DrvGpuType == 2) ? 11 : 12));
					break;

				case 0x07:
					DrvGpuInfo = DrvGpuType;
					break;

				case 0x08:
					DrvGpuInfo = 0;
					break;

				default:
					DrvGpuInfo = 0;
					break;
			}
			break;
	}
}

static void ZnGpuDmaWrite(UINT32 address, INT32 size)
{
	UINT32 *ram = (UINT32 *)DrvMainRAM;
	UINT32 mask = 0x3fffff;
	for (INT32 i = 0; i < size; i++) {
		UINT32 data = ram[((address & mask) >> 2) & 0x0fffff];
		ZnGpuWrite(data);
		address += 4;
	}
}

static void ZnDmaFinish(INT32 channel)
{
	// MAME's SPU write DMA commits at the delayed completion callback.
	if (channel == 4 && DrvDmaControl[channel] == 0x01000201) {
		UINT32 blocks = DrvDmaBlock[4] >> 16;
		if (!blocks)
			blocks = 0x10000;
		UINT64 transferSize = (UINT64)(DrvDmaBlock[4] & 0xffff) * blocks * 4;
		ZnSpuAudio::Sync();
		UINT32 start = ZnSpuRegs[0x1a6 / 2] << 3;
		UINT32 size = transferSize > 0x80000 - start ? 0x80000 - start : (UINT32)transferSize;
		for (UINT32 i = 0; i < size; i++) {
			if (!(i & 7))
				ZnSpuAudio::CheckIrq(start + i);
			ZnSpuRam[start + i] = DrvMainRAM[(DrvDmaBase[4] + i) & 0x3fffff];
		}
		ZnSpuWriteAddress = (start + size) & 0x7ffff;
	}
	DrvDmaControl[channel] &= ~((1 << 24) | (1 << 28));
	DrvDmaDicr |= 1 << (24 + channel);
	ZnDmaInterruptUpdate();
}

static void ZnDmaScheduleFinish(INT32 channel, UINT32 cycles)
{
	DrvDmaFinishCycle[channel] = ZnPsxTotalCycles() + cycles;
	DrvDmaPending |= 1 << channel;
}

static void ZnDmaAdvance()
{
	UINT32 now = ZnPsxTotalCycles();
	for (INT32 channel = 0; channel < 7; channel++) {
		if ((DrvDmaPending & (1 << channel)) && (INT32)(now - DrvDmaFinishCycle[channel]) >= 0) {
			DrvDmaPending &= ~(1 << channel);
			ZnDmaFinish(channel);
		}
	}
}

static void ZnDmaStart(INT32 channel)
{
	UINT32 control = DrvDmaControl[channel];

	if ((control & (1 << 24)) == 0)
		return;
	if ((DrvDmaDpcr & (1 << (3 + (channel * 4)))) == 0)
		return;

	if (channel == 4 && control == 0x01000201) {
		ZnDmaScheduleFinish(channel, 24000 / 4);
		return;
	}
	if (channel == 4 && control == 0x01000200) {
		UINT32 blocks = DrvDmaBlock[4] >> 16;
		if (!blocks)
			blocks = 0x10000;
		UINT64 transferSize = (UINT64)(DrvDmaBlock[4] & 0xffff) * blocks * 4;
		ZnSpuAudio::Sync();
		UINT32 start = ZnSpuRegs[0x1a6 / 2] << 3;
		UINT32 size = transferSize > 0x80000 - start ? 0x80000 - start : (UINT32)transferSize;
		for (UINT32 i = 0; i < size; i++) {
			if (!(i & 7))
				ZnSpuAudio::CheckIrq(start + i);
			DrvMainRAM[(DrvDmaBase[4] + i) & 0x3fffff] = ZnSpuRam[start + i];
		}
		ZnSpuWriteAddress = (start + size) & 0x7ffff;
		ZnDmaFinish(channel);
		return;
	}

	if ((channel == 0 && control == 0x01000201) || (channel == 1 && control == 0x01000200)) {
		UINT32 blocks = DrvDmaBlock[channel] >> 16;
		if (blocks == 0)
			blocks = 0x10000;
		INT32 size = (DrvDmaBlock[channel] & 0xffff) * blocks;
		if (channel == 0)
			ZnMdecDmaWrite(DrvDmaBase[channel], size);
		else
			ZnMdecDmaRead(DrvDmaBase[channel], size);
		if (channel == 1)
			ZnDmaScheduleFinish(channel, 26000 / 4);
		else
			ZnDmaFinish(channel);
		return;
	}

	if (channel == 2 && control == 0x01000401) {
		UINT32 *ram = (UINT32 *)DrvMainRAM;
		UINT32 address = DrvDmaBase[channel] & 0x00ffffff;
		UINT32 mask = 0x3fffff;
		UINT32 packets = 0;

		while (address != 0x00ffffff && packets < 0x100000) {
			address &= mask;
			UINT32 next = ram[address >> 2];
			INT32 size = next >> 24;
			ZnGpuDmaWrite(address + 4, size);

			UINT32 next_address = next & 0x00ffffff;
			if (next_address != 0x00ffffff) {
				UINT32 linked = next_address & mask;
				if (address == ram[linked >> 2] || address == linked)
					break;
			}

			address = next_address;
			packets++;
		}

		DrvDmaBase[channel] = address;
		ZnDmaScheduleFinish(channel, 500 / 4);
		return;
	}

	if (channel == 2 && control == 0x01000201) {
		INT32 size = DrvDmaBlock[channel];
		if (control & 0x200) {
			UINT32 blocks = DrvDmaBlock[channel] >> 16;
			if (blocks == 0)
				blocks = 0x10000;
			size = (DrvDmaBlock[channel] & 0xffff) * blocks;
		}
		ZnGpuDmaWrite(DrvDmaBase[channel], size);
		ZnDmaFinish(channel);
		return;
	}

	if (channel == 2 && control == 0x01000200) {
		UINT32 *ram = (UINT32 *)DrvMainRAM;
		UINT32 address = DrvDmaBase[channel] & 0x3fffff;
		UINT32 blocks = DrvDmaBlock[channel] >> 16;
		if (blocks == 0)
			blocks = 0x10000;
		INT32 size = (DrvDmaBlock[channel] & 0xffff) * blocks;
		for (INT32 i = 0; i < size; i++) {
			ram[(address & 0x3fffff) >> 2] = ZnReadIo(0x1f801810);
			address += 4;
		}
		ZnDmaFinish(channel);
		return;
	}

	if (channel == 6 && control == 0x11000002) {
		UINT32 *ram = (UINT32 *)DrvMainRAM;
		UINT32 address = DrvDmaBase[channel] & 0x3fffff;
		INT32 size = DrvDmaBlock[channel];

		if (size > 0) {
			size--;
			while (size > 0) {
				UINT32 next = (address - 4) & 0x00ffffff;
				ram[address >> 2] = next;
				address = next & 0x3fffff;
				size--;
			}
			ram[address >> 2] = 0x00ffffff;
		}

		ZnDmaFinish(channel);
	}
}

static void ZnWriteDma(UINT32 address, UINT32 data)
{
	UINT32 offset = (address - 0x1f801080) >> 2;
	UINT32 channel = offset >> 2;

	if (channel < 7) {
		switch (offset & 3) {
			case 0:
				DrvDmaBase[channel] = data;
				break;

			case 1:
				DrvDmaBlock[channel] = data;
				break;

			case 2:
			case 3:
				DrvDmaControl[channel] = data;
				ZnDmaStart(channel);
				break;
		}
	} else {
		switch (offset & 3) {
			case 0:
				DrvDmaDpcr = data;
				break;

			case 1:
				DrvDmaDicr = (DrvDmaDicr & 0x80000000) | (DrvDmaDicr & ~data & 0x7f000000) | (data & 0x00ff803f);
				if ((DrvDmaDicr & 0x80000000) && ((DrvDmaDicr & 0x7f000000) == 0)) {
					DrvDmaDicr &= ~0x80000000;
					DrvIrqStatus &= ~0x0008;
					ZnUpdateIrq();
				}
				break;
		}
	}
}

static void ZnWriteIo(UINT32 address, UINT32 data)
{
	switch (address & ~3) {
		case 0x1f801040:
		case 0x1f801044:
		case 0x1f801048:
		case 0x1f80104c:
		case 0x1f801050:
		case 0x1f801054:
		case 0x1f801058:
		case 0x1f80105c:
			ZnSioWrite(address & ~3, data, ~0);
			return;

		case 0x1f801000:
			DrvExpBase = 0x1f000000 | (data & 0x00ffffff);
			return;

		case 0x1f801004:
			DrvControlRAM0 = data;
			return;

		case 0x1f801008:
			DrvExpConfig = data & 0xaf1fffff;
			return;

		case 0x1f80100c:
			DrvMemControl = data;
			return;

		case 0x1f801010:
			if ((DrvRomConfig ^ data) & 0x001f0000) {
				DrvRomConfig = data;
				ZnMapRomConfig();
			} else {
				DrvRomConfig = data;
			}
			return;

		case 0x1f801014:
		case 0x1f801018:
		case 0x1f80101c:
		case 0x1f801020:
		case 0x1f801024:
		case 0x1f801028:
		case 0x1f80102c:
			DrvDelayRAM[((address & ~3) - 0x1f801014) >> 2] = data;
			return;

		case 0x1f801060:
			if ((DrvRamConfig ^ data) & 0x0000ff00) {
				DrvRamConfig = data;
				ZnMapRamConfig();
			} else {
				DrvRamConfig = data;
			}
			return;

		case 0x1f801064:
		case 0x1f801068:
		case 0x1f80106c:
			DrvControlRAM1[((address & ~3) - 0x1f801064) >> 2] = data;
			return;

		case 0x1f801070:
			DrvIrqStatus &= DrvIrqMask & data;
			ZnUpdateIrq();
			return;

		case 0x1f801074:
			DrvIrqMask = data;
			ZnUpdateIrq();
			return;

		case 0x1f801810:
			ZnGpuWrite(data);
			return;

		case 0x1f801814:
			ZnGpuControl(data);
			return;

		case 0x1f801820:
			DrvMdecCommand0 = data;
			return;

		case 0x1f801824:
			DrvMdecCommand1 = data;
			return;

		case 0xfffe0130:
			DrvBiu = data;
			PsxSetBIU(data);
			return;
	}

	if (address >= 0x1f801080 && address <= 0x1f8010ff) {
		ZnWriteDma(address, data);
		return;
	}

	if (address >= 0x1f801100 && address <= 0x1f80112f) {
		ZnRootWrite(address, data);
		return;
	}
}

static void ZnWriteIoMasked(UINT32 address, UINT32 data, UINT32 mem_mask)
{
	if (address >= 0x1f801c00 && address <= 0x1f801dff) {
		for (INT32 shift = 0; shift < 32; shift += 16) {
			UINT16 mask = mem_mask >> shift;
			if (!mask)
				continue;
			UINT32 offset = (address & 0x1fc) + shift / 8;
			UINT16 old = ZnSpuRegs[offset >> 1];
			ZnSpuWrite(offset, (old & ~mask) | ((data >> shift) & mask));
		}
		return;
	}
	address &= ~3;

	switch (address) {
		case 0x1f801000:
		case 0x1f801008:
		case 0x1f801010:
		case 0x1f801014:
		case 0x1f801018:
		case 0x1f80101c:
		case 0x1f801020:
		case 0x1f801024:
		case 0x1f801028:
		case 0x1f80102c:
		case 0x1f801060:
		case 0xfffe0130: {
			UINT32 old = ZnReadIo(address);
			ZnWriteIo(address, (old & ~mem_mask) | (data & mem_mask));
			return;
		}

		case 0x1f801040:
		case 0x1f801044:
		case 0x1f801048:
		case 0x1f80104c:
		case 0x1f801050:
		case 0x1f801054:
		case 0x1f801058:
		case 0x1f80105c:
			ZnSioWrite(address, data, mem_mask);
			return;

		case 0x1f801070:
			DrvIrqStatus = (DrvIrqStatus & ~mem_mask) | (DrvIrqStatus & DrvIrqMask & data & mem_mask);
			ZnUpdateIrq();
			return;

		case 0x1f801074:
			DrvIrqMask = (DrvIrqMask & ~mem_mask) | (data & mem_mask);
			ZnUpdateIrq();
			return;
	}

	if (address >= 0x1f801080 && address <= 0x1f8010ff) {
		UINT32 old = ZnReadIo(address);
		ZnWriteIo(address, (old & ~mem_mask) | (data & mem_mask));
		return;
	}

	UINT32 old = ZnReadIo(address);
	ZnWriteIo(address, (old & ~mem_mask) | (data & mem_mask));
}

static UINT8 *ZnCountry, *ZnBankRom, *ZnSoundRom, *ZnSamples, *ZnSoundRam, *ZnNvram;
static UINT32 *DrvPalette;
static UINT8 ZnKey[2][8], ZnSbox[2][8][8], ZnCatState[2];
static UINT8 ZnSelect, ZnBank, ZnSoundBank, ZnLatch, ZnCoin, ZnSpuToggle, ZnMcuByte;
static UINT16 ZnQData;
static UINT8 ZnLatchPending;
static UINT8 ZnJoy[2][10], ZnSystem[8], ZnService[8], ZnDip, DrvReset;
static UINT8 ZnInputs[8];
static INT32 ZnGeneration, ZnVramMB, ZnSilent, ZnLayout;
static INT32 ZnSoundExtra, ZnMainExtra, ZnIrqPhase;
static UINT8 DrvRecalc;
struct ZnRomLoad {
	INT32 index, region;
	UINT32 offset, size;
	INT32 swap;
};

// CAT702 transforms operate on bits, least significant bit first.
static UINT8 ZnCatTransform(UINT8 state, const UINT8 *table)
{
	UINT8 result = 0;
	for (INT32 bit = 0; bit < 8; bit++)
		if (state & (1 << bit))
			result ^= table[bit];
	return result;
}

static void ZnSecuritySelect(UINT8 data)
{
	for (INT32 chip = 0; chip < 2; chip++) {
		UINT8 mask = 4 << chip;
		if ((ZnSelect & mask) && !(data & mask))
			ZnCatState[chip] = 0xfc;
	}
	if ((data & 0x8c) == 0x8c && (ZnSelect & 0x8c) != 0x8c)
		ZnMcuByte = 0;
	ZnSelect = data;
}

static UINT8 ZnSerialTransfer(UINT8 data)
{
	static const UINT8 initial[8] = { 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x7f };
	UINT8 result = 0xff;
	for (INT32 chip = 0; chip < 2; chip++) {
		if (ZnSelect & (4 << chip))
			continue;
		ZnCatState[chip] = ZnCatTransform(ZnCatState[chip], initial);
		UINT8 value = 0;
		for (INT32 bit = 0; bit < 8; bit++) {
			value |= ZnCatState[chip] & (1 << bit);
			if (!(data & (1 << bit)))
				ZnCatState[chip] = ZnCatTransform(ZnCatState[chip], ZnSbox[chip][bit]);
		}
		result &= value;
	}
	// Digital ZN MCU response. These Capcom panels have no analog/trackball axes.
	if ((ZnSelect & 0x8c) == 0x8c) {
		INT32 count = (ZnSelect & 0x10) ? 8 : (ZnSelect & 0x20) ? 6 : 1;
		UINT8 value = ZnMcuByte == 0 ? (count << 4) | (ZnDip & 15) : (ZnSelect & 0x10) ? 0xff : 0;
		result &= value;
		if (ZnMcuByte < 255)
			ZnMcuByte++;
		DrvSioStatus[0] |= PSX_SIO_STATUS_DSR;
		if (DrvSioControl[0] & PSX_SIO_CONTROL_DSR_IENA) {
			DrvSioStatus[0] |= PSX_SIO_STATUS_IRQ;
			ZnSioUpdateIrq(0);
		}
	}
	return result;
}

static void ZnMapBank()
{
	PsxMapMemory(ZnBankRom, 0x1f000000, 0x1f3fffff, MAP_ROM);
	PsxMapMemory(ZnBankRom + 0x400000 + (ZnBank & 15) * 0x400000, 0x1f400000, 0x1f7fffff, MAP_ROM);
}

static void ZnMapSoundBank()
{
	ZetMapMemory(ZnSoundRom + 0x8000 + (ZnSoundBank & 15) * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static UINT8 __fastcall ZnSoundRead(UINT16 address)
{
	return address == 0xd007 ? QscRead() : 0xff;
}

static void __fastcall ZnSoundWrite(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xd000:
			ZnQData = (ZnQData & 0xff) | (data << 8);
			break;
		case 0xd001:
			ZnQData = (ZnQData & 0xff00) | data;
			break;
		case 0xd002:
			QscWrite(data, ZnQData);
			break;
		case 0xd003:
			ZnSoundBank = data & 15;
			ZnMapSoundBank();
			break;
	}
}

static UINT8 __fastcall ZnSoundPort(UINT16 port)
{
	if (!(port & 0xff)) {
		ZnLatchPending = 0;
		ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return ZnLatch;
	}
	return 0xff;
}

static UINT32 ZnReadWord(UINT32 address)
{
	if (address == 0xfffe0130)
		return PsxGetBIU();
	address &= 0x1fffffff;
	if (address < 0x800000)
		return *(UINT32 *)(DrvMainRAM + (address & 0x3ffffc));
	if (address >= 0x1f800000 && address < 0x1f800400)
		return *(UINT32 *)(ZnScratchpad() + (address & 0x3fc));
	if (address >= 0x1f801000 && address < 0x1f802000)
		return ZnReadIo(address);
	if (address >= 0x1faf0000 && address < 0x1faf0800)
		return *(UINT32 *)(ZnNvram + (address & 0x7fc));
	switch (address & ~3) {
		case 0x1fa00000:
			return 0xffffff00 | ZnInputs[0];
		case 0x1fa00100:
			return 0xffffff00 | ZnInputs[1];
		case 0x1fa00200:
			return 0xffffff00 | ZnInputs[2];
		case 0x1fa00300:
			return 0xffffff00 | ZnInputs[3];
		case 0x1fa10000:
			return 0xffffff00 | ZnInputs[4];
		case 0x1fa10100:
			return 0xffffff00 | ZnInputs[5];
		case 0x1fa10200:
			return 0xffffff00 | 0x61 | (ZnVramMB == 2 ? 8 : 0);
		case 0x1fa10300:
			return 0xffffff00 | ZnSelect;
		case 0x1fa20000:
			return 0xffffff00 | ZnCoin;
		case 0x1fa30000:
		case 0x1fa40000:
			return 0;
		case 0x1fa60000:
			ZnSpuToggle ^= 8;
			return ZnSpuToggle;
		case 0x1fb40010:
			return 0xffffff00 | ZnInputs[6];
		case 0x1fb40020:
			return 0xffffff00 | ZnInputs[7];
	}
	if (address >= 0x1fa51c00 && address <= 0x1fa51dff && ZnGeneration == 2)
		return 0;
	return 0xffffffff;
}
static UINT8 ZnReadByte(UINT32 address)
{
	return ZnReadWord(address & ~3) >> ((address & 3) * 8);
}
static UINT16 ZnReadHalf(UINT32 address)
{
	return ZnReadWord(address & ~3) >> ((address & 2) * 8);
}

// Render SPU voices before changing registers or sample RAM.
static void ZnSpuAudio::Sync()
{
	UINT32 elapsed = ZnPsxTotalCycles() - DrvPsxFrameStartCycles + ZnMainExtra;
	Render((INT32)((UINT64)elapsed * 735 / (ZN_PSX_CPU_CLOCK / 60)));
}

// Run the audio CPU up to this main-CPU write before changing its command
// latch. This preserves the order of consecutive commands within a scanline.
static void ZnSyncSound()
{
	UINT32 elapsed = ZnPsxTotalCycles() - DrvPsxFrameStartCycles + ZnMainExtra;
	INT32 target = (INT32)((UINT64)elapsed * 8000000 / ZN_PSX_CPU_CLOCK) - ZnSoundExtra;
	INT32 cycles = target - ZetTotalCycles();
	if (cycles > 0)
		ZetRun(cycles);
}

static void ZnWriteMasked(UINT32 address, UINT32 data, UINT32 mask)
{
	if (address == 0xfffe0130) {
		ZnWriteIoMasked(address, data, mask);
		return;
	}
	address &= 0x1fffffff;
	UINT8 *ram = NULL;
	if (address < 0x800000)
		ram = DrvMainRAM + (address & 0x3ffffc);
	if (address >= 0x1f800000 && address < 0x1f800400)
		ram = ZnScratchpad() + (address & 0x3fc);
	if (address >= 0x1faf0000 && address < 0x1faf0800)
		ram = ZnNvram + (address & 0x7fc);
	if (ram) {
		UINT32 *p = (UINT32 *)ram;
		*p = (*p & ~mask) | (data & mask);
		return;
	}
	if (address >= 0x1f801000 && address < 0x1f802000) {
		ZnWriteIoMasked(address, data, mask);
		return;
	}
	if (!(mask & 0xff))
		return;
	switch (address) {
		case 0x1fa10300:
			ZnSecuritySelect(data);
			break;
		case 0x1fa20000:
			ZnCoin = data;
			break;
		case 0x1fb00000:
			ZnBank = data & 15;
			ZnMapBank();
			break;
		case 0x1fb60000:
			if (!ZnSilent)
				ZnSyncSound();
			ZnLatch = data;
			// MAME generic_latch_8 holds NMI until the Z80 reads port 0.
			// Overwriting a pending latch must not create another NMI edge.
			if (!ZnSilent && !ZnLatchPending)
				ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			ZnLatchPending = 1;
			break;
	}
}
static void ZnWriteByte(UINT32 a, UINT8 d)
{
	ZnWriteMasked(a & ~3, (UINT32)d << ((a & 3) * 8), 0xffU << ((a & 3) * 8));
}
static void ZnWriteHalf(UINT32 a, UINT16 d)
{
	ZnWriteMasked(a & ~3, (UINT32)d << ((a & 2) * 8), 0xffffU << ((a & 2) * 8));
}
static void ZnWriteWord(UINT32 a, UINT32 d)
{
	ZnWriteMasked(a, d, ~0U);
}

static INT32 MemIndex()
{
	UINT8 *Next = AllMem;
	DrvMainROM = Next;
	Next += 0x80000;
	ZnCountry = Next;
	Next += 0x80000;
	ZnBankRom = Next;
	Next += 0x4400000;
	ZnSoundRom = Next;
	Next += 0x48000;
	ZnSamples = Next;
	Next += 0x400000;
	DrvMainRAM = Next;
	Next += 0x400000;
	DrvScratchRAM = Next;
	Next += 0x1000;
	ZnSoundRam = Next;
	Next += 0x1000;
	ZnNvram = Next;
	Next += 0x800;
	DrvGpuVram = (UINT16 *)Next;
	Next += 0x200000;
	DrvPalette = (UINT32 *)Next;
	Next += 0x8000 * 4;
	MemEnd = Next;
	return 0;
}

static INT32 DrvResetFunc()
{
	memset(DrvMainRAM, 0, 0x400000);
	memset(ZnSoundRam, 0, 0x1000);
	ZnResetIo();
	memset(ZnSpuRegs, 0, sizeof(ZnSpuRegs));
	memset(ZnSpuRam, 0, sizeof(ZnSpuRam));
	ZnSpuRegs[0x1ac / 2] = 0x480; // MAME SPU reset value.
	ZnSpuWriteAddress = 0;
	ZnSpuAudio::Reset();
	ZnBank = ZnSoundBank = ZnLatch = ZnCoin = ZnSpuToggle = ZnMcuByte = 0;
	ZnSelect = 0xff;
	ZnQData = 0;
	ZnLatchPending = 0;
	ZnSoundExtra = ZnMainExtra = ZnIrqPhase = 0;
	ZnMapRamConfig();
	ZnMapRomConfig();
	ZnMapBank();
	PsxReset();
	ZetOpen(0);
	ZetReset();
	ZnMapSoundBank();
	ZetClose();
	QscReset();
	return 0;
}

static INT32 DrvInit(const ZnRomLoad *loads, INT32 count, INT32 generation, INT32 vram, INT32 silent, INT32 layout)
{
	DrvGpuVramMask = (vram << 19) - 1;
	ZnGeneration = generation;
	ZnVramMB = vram;
	ZnSilent = silent;
	ZnLayout = layout;
	ZN_PSX_CPU_CLOCK = generation == 2 ? 25000000 : 16934400;
	DrvGpuDefaultType = 2; // CXD8561/CXD8654, standard PSX packet layout.
	DrvPaletteHighCol = NULL;
	BurnAllocMemIndex();
	memset(ZnBankRom, 0xff, 0x4400000);
	memset(ZnSoundRom, 0xff, 0x48000);
	memset(ZnNvram, 0xff, 0x800);
	bool biosLoaded = false;
	for (INT32 i = 0; i < count; i++) {
		const ZnRomLoad &r = loads[i];
		// MAME lists alternative BIOS images at the same address. Use its
		// first (default) image, rather than overwriting it with the debug BIOS.
		if (r.region == 0 && biosLoaded)
			continue;
		if (r.region == 0)
			biosLoaded = true;
		UINT8 *dest[] = { DrvMainROM, ZnCountry, ZnBankRom, ZnSoundRom, ZnSamples, ZnKey[0], ZnKey[1] };
		if (BurnLoadRom(dest[r.region] + r.offset, r.index, 1)) {
			BurnFreeMemIndex();
			return 1;
		}
		if (r.swap)
			for (UINT32 j = 0; j < r.size; j += 2) {
				UINT8 *p = dest[r.region] + r.offset + j, t = p[0];
				p[0] = p[1];
				p[1] = t;
			}
	}
	for (INT32 c = 0; c < 2; c++) {
		memcpy(ZnSbox[c][0], ZnKey[c], 8);
		for (INT32 n = 1; n < 8; n++)
			for (INT32 b = 0; b < 8; b++) {
				UINT8 v = ZnSbox[c][n - 1][(b - 1) & 7];
				ZnSbox[c][n][b] = (v << 1) | (((v >> 7) ^ (v >> 6)) & 1);
				if (b == 7)
					ZnSbox[c][n][b] ^= ZnSbox[c][n][0];
			}
	}
	GenericTilesInit();
	if (PsxInit()) {
		GenericTilesExit();
		BurnFreeMemIndex();
		return 1;
	}
	PsxSetReadByteHandler(0, ZnReadByte);
	PsxSetReadHalfHandler(0, ZnReadHalf);
	PsxSetReadWordHandler(0, ZnReadWord);
	PsxSetWriteByteHandler(0, ZnWriteByte);
	PsxSetWriteHalfHandler(0, ZnWriteHalf);
	PsxSetWriteWordHandler(0, ZnWriteWord);
	PsxMapHandler(0, 0x1f800000, 0x1fffffff, MAP_READ | MAP_WRITE);
	PsxMapHandler(0, 0xfffe0000, 0xfffeffff, MAP_READ | MAP_WRITE);
	PsxMapMemory(ZnCountry, 0x1fb80000, 0x1fbfffff, MAP_ROM);
	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(ZnSoundRom, 0, 0x7fff, MAP_ROM);
	ZetMapMemory(ZnSoundRam, 0xf000, 0xffff, MAP_RAM);
	ZetSetReadHandler(ZnSoundRead);
	ZetSetWriteHandler(ZnSoundWrite);
	ZetSetInHandler(ZnSoundPort);
	ZetClose();
	CpsQSam = (INT8 *)ZnSamples;
	nCpsQSamLen = 0x400000;
	nCpsQSamMask = 0;
	nCpsZ80Cycles = 8000000 / 60;
	QscInit(nBurnSoundRate);
	return DrvResetFunc();
}

static INT32 DrvExit()
{
	QscExit();
	ZetExit();
	PsxExit();
	GenericTilesExit();
	CpsQSam = NULL;
	nCpsQSamLen = nCpsQSamMask = 0;
	BurnFreeMemIndex();
	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc || DrvPaletteHighCol != BurnHighCol) {
		for (INT32 i = 0; i < 0x8000; i++) {
			INT32 r = i & 31, g = (i >> 5) & 31, b = (i >> 10) & 31;
			DrvPalette[i] = BurnHighCol((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 0);
		}
		DrvPaletteHighCol = BurnHighCol;
		DrvRecalc = 0;
	}
	ZnGpuUpdateVisibleArea();
	if (DrvGpuStatus & (1 << 23))
		BurnTransferClear();
	else {
		const INT32 width = (INT32)DrvGpuScreenWidth, height = (INT32)DrvGpuScreenHeight;
		// MAME psxgpu_device::update_screen: GP1 display ranges also control
		// blank borders and the first visible VRAM pixel, not just resolution.
		const INT32 top = (INT32)DrvGpuVertStart - ((DrvGpuStatus & (1 << 20)) ? 0x23 : 0x10);
		const INT32 left = ((INT32)DrvGpuHorizStart - ((DrvGpuStatus & (1 << 20)) ? 0x27e : 0x260)) * width / 2560;
		const INT32 firstY = top < 0 ? -top : 0;
		const INT32 firstX = left < 0 ? -left : 0;
		INT32 lines = (INT32)DrvGpuVertEnd - (INT32)DrvGpuVertStart - firstY;
		if (DrvGpuStatus & (1 << 22))
			lines *= 2;
		INT32 columns = ((INT32)DrvGpuHorizEnd - (INT32)DrvGpuHorizStart) * width / 2560 - firstX;
		for (INT32 y = 0; y < nScreenHeight; y++) {
			const INT32 sourceY = y * height / nScreenHeight - top;
			UINT16 *dest = pTransDraw + y * nScreenWidth;
			if (sourceY < firstY || sourceY >= firstY + lines) {
				memset(dest, 0, nScreenWidth * sizeof(UINT16));
				continue;
			}
			const UINT16 *row = DrvGpuVram + ((((sourceY + DrvGpuDisplayY) & 1023) << 10) & DrvGpuVramMask);
			for (INT32 x = 0; x < nScreenWidth; x++) {
				const INT32 sourceX = x * width / nScreenWidth - left;
				if (sourceX < firstX || sourceX >= firstX + columns) {
					dest[x] = 0;
					continue;
				}
				if (DrvGpuStatus & (1 << 21)) {
					const INT32 byte = sourceX * 3;
					const INT32 sx = DrvGpuDisplayX + (byte >> 1);
					const UINT16 lo = row[sx & 1023], hi = row[(sx + 1) & 1023];
					const UINT8 r = (byte & 1) ? lo >> 8 : lo;
					const UINT8 g = (byte & 1) ? hi : lo >> 8;
					const UINT8 b = (byte & 1) ? hi >> 8 : hi;
					dest[x] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
				} else {
					dest[x] = row[(sourceX + DrvGpuDisplayX) & 1023] & 0x7fff;
				}
			}
		}
	}
	BurnTransferCopy(DrvPalette);
	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset)
		DrvResetFunc();
	memset(ZnInputs, 0xff, sizeof(ZnInputs));
	for (INT32 p = 0; p < 2; p++) {
		for (INT32 b = 0; b < 7; b++)
			ZnInputs[p] ^= (ZnJoy[p][b] & 1) << b;
		INT32 first = ZnLayout == 4 ? 6 : 7;
		if (ZnLayout == 4)
			ZnInputs[p] |= 0x40;
		for (INT32 b = 0; b < (ZnLayout == 4 ? 2 : 3); b++) {
			ZnInputs[4 + p] ^= (ZnJoy[p][first + b] & 1) << (4 + b);
			ZnInputs[6 + p] ^= (ZnJoy[p][first + b] & 1) << b;
		}
	}
	for (INT32 b = 0; b < 8; b++) {
		ZnInputs[2] ^= (ZnService[b] & 1) << b;
		ZnInputs[3] ^= (ZnSystem[b] & 1) << b;
	}
	ZetNewFrame();
	ZetOpen(0);
	QscNewFrame();
	ZnSpuAudio::NewFrame();
	if (pBurnSoundOut)
		memset(pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
	INT32 mainDone = ZnMainExtra;
	const INT32 mainTotal = ZN_PSX_CPU_CLOCK / 60;
	DrvPsxFrameStartCycles = DrvPsxTotalCycles;
	if (DrvGpuStatus & (1 << 22))
		DrvGpuStatus ^= 1 << 13;
	else
		DrvGpuStatus |= 1 << 13;
	for (INT32 i = 0; i < 262; i++) {
		DrvGpuVpos = i;
		INT32 segment = (i + 1) * mainTotal / 262 - mainDone;
		DrvPsxRunBaseCycles = DrvPsxTotalCycles;
		DrvPsxRunSegment = segment;
		while (segment > 0) {
			// Service native SPU events before the next CPU quantum. IRQs cannot
			// be deferred to frame-end mixing, even with host audio disabled.
			INT32 nextSample = (INT32)(((INT64)(ZnSpuAudio::position + 1) * mainTotal + 734) / 735);
			INT32 quantum = nextSample - mainDone;
			if (quantum < 1)
				quantum = 1;
			if (quantum > segment)
				quantum = segment;
			DrvPsxRunBaseCycles = DrvPsxTotalCycles;
			DrvPsxRunSegment = quantum;
			INT32 ran = PsxRun(quantum);
			mainDone += ran;
			DrvPsxTotalCycles += ran;
			segment -= ran;
			DrvPsxRunBaseCycles = DrvPsxTotalCycles;
			DrvPsxRunSegment = 0;
			ZnSpuAudio::Sync();
		}
		DrvPsxRunBaseCycles = DrvPsxTotalCycles;
		DrvPsxRunSegment = 0;
		ZnDmaAdvance();
		ZnRootAdvance();
		if (!ZnSilent) {
			INT32 cycles = (i + 1) * nCpsZ80Cycles / 262 - ZnSoundExtra - ZetTotalCycles();
			if (cycles > 0)
				ZetRun(cycles);
		}
		ZnIrqPhase += 250;
		if (ZnIrqPhase >= 60 * 262) {
			ZnIrqPhase -= 60 * 262;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == 239) {
			DrvIrqStatus |= 1;
			ZnUpdateIrq();
		}
	}
	ZnMainExtra = mainDone - mainTotal;
	ZnSoundExtra = ZnSilent ? 0 : ZnSoundExtra + ZetTotalCycles() - nCpsZ80Cycles;
	if (pBurnSoundOut && !ZnSilent)
		QscUpdate(nBurnSoundLen);
	ZnSpuAudio::Mix();
	ZetClose();
	if (pBurnDraw)
		DrvDraw();
	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin)
		*pnMin = 0x029744;
	if (nAction & ACB_MEMORY_RAM) {
		BurnArea ba = {};
		ba.Data = DrvMainRAM;
		ba.nLen = 0x400000;
		ba.szName = "DrvMainRAM";
		BurnAcb(&ba);
		ba.Data = DrvScratchRAM;
		ba.nLen = 0x1000;
		ba.szName = "DrvScratchRAM";
		BurnAcb(&ba);
		ba.Data = ZnSoundRam;
		ba.nLen = 0x1000;
		ba.szName = "ZnSoundRam";
		BurnAcb(&ba);
		ba.Data = DrvGpuVram;
		ba.nLen = 0x200000;
		ba.szName = "DrvGpuVram";
		BurnAcb(&ba);
	}
	if (nAction & ACB_NVRAM) {
		BurnArea ba = {};
		ba.Data = ZnNvram;
		ba.nLen = 0x800;
		ba.szName = "AT28C16";
		BurnAcb(&ba);
	}
	if (nAction & ACB_DRIVER_DATA) {
		PsxScan(nAction);
		ZetScan(nAction);
		QscScan(nAction, 1);
		SCAN_VAR(ZnSpuRegs);
		SCAN_VAR(ZnSpuRam);
		SCAN_VAR(ZnSpuWriteAddress);
		ZnSpuAudio::Scan();
		SCAN_VAR(DrvExpBase);
		SCAN_VAR(DrvControlRAM0);
		SCAN_VAR(DrvExpConfig);
		SCAN_VAR(DrvRomConfig);
		SCAN_VAR(DrvMemControl);
		SCAN_VAR(DrvDelayRAM);
		SCAN_VAR(DrvRamConfig);
		SCAN_VAR(DrvControlRAM1);
		SCAN_VAR(DrvIrqStatus);
		SCAN_VAR(DrvIrqMask);
		SCAN_VAR(DrvRootCount);
		SCAN_VAR(DrvRootMode);
		SCAN_VAR(DrvRootTarget);
		SCAN_VAR(DrvRootStart);
		SCAN_VAR(DrvRootExpire);
		SCAN_VAR(DrvRootActive);
		SCAN_VAR(DrvPsxTotalCycles);
		SCAN_VAR(DrvPsxRunBaseCycles);
		SCAN_VAR(DrvPsxRunSegment);
		SCAN_VAR(DrvPsxFrameStartCycles);
		SCAN_VAR(DrvGpuStatus);
		SCAN_VAR(DrvGpuTPage);
		SCAN_VAR(DrvGpuInfo);
		SCAN_VAR(DrvGpuType);
		SCAN_VAR(DrvBiu);
		SCAN_VAR(DrvDmaBase);
		SCAN_VAR(DrvDmaBlock);
		SCAN_VAR(DrvDmaControl);
		SCAN_VAR(DrvDmaDpcr);
		SCAN_VAR(DrvDmaDicr);
		SCAN_VAR(DrvDmaFinishCycle);
		SCAN_VAR(DrvDmaPending);
		SCAN_VAR(DrvGpuPacket);
		SCAN_VAR(DrvGpuPacketPos);
		SCAN_VAR(DrvGpuPacketLen);
		SCAN_VAR(DrvGpuPolyline);
		SCAN_VAR(DrvGpuTextureWindowX);
		SCAN_VAR(DrvGpuTextureWindowY);
		SCAN_VAR(DrvGpuTextureWindowW);
		SCAN_VAR(DrvGpuTextureWindowH);
		SCAN_VAR(DrvGpuDrawStp);
		SCAN_VAR(DrvGpuCheckStp);
		SCAN_VAR(DrvGpuImageX);
		SCAN_VAR(DrvGpuImageY);
		SCAN_VAR(DrvGpuImageW);
		SCAN_VAR(DrvGpuImageH);
		SCAN_VAR(DrvGpuImageCount);
		SCAN_VAR(DrvGpuDiscardImage);
		SCAN_VAR(DrvGpuImageCurX);
		SCAN_VAR(DrvGpuImageCurY);
		SCAN_VAR(DrvGpuImageColumn);
		SCAN_VAR(DrvGpuReadX);
		SCAN_VAR(DrvGpuReadY);
		SCAN_VAR(DrvGpuReadW);
		SCAN_VAR(DrvGpuReadH);
		SCAN_VAR(DrvGpuReadCount);
		SCAN_VAR(DrvGpuDisplayX);
		SCAN_VAR(DrvGpuDisplayY);
		SCAN_VAR(DrvGpuDrawX1);
		SCAN_VAR(DrvGpuDrawY1);
		SCAN_VAR(DrvGpuDrawX2);
		SCAN_VAR(DrvGpuDrawY2);
		SCAN_VAR(DrvGpuHorizStart);
		SCAN_VAR(DrvGpuHorizEnd);
		SCAN_VAR(DrvGpuVertStart);
		SCAN_VAR(DrvGpuVertEnd);
		SCAN_VAR(DrvGpuScreenWidth);
		SCAN_VAR(DrvGpuScreenHeight);
		SCAN_VAR(DrvGpuDrawOffsetX);
		SCAN_VAR(DrvGpuDrawOffsetY);
		SCAN_VAR(DrvGpuDefaultType);
		SCAN_VAR(DrvGpuVpos);
		SCAN_VAR(DrvSioStatus);
		SCAN_VAR(DrvSioMode);
		SCAN_VAR(DrvSioControl);
		SCAN_VAR(DrvSioBaud);
		SCAN_VAR(DrvSioRxData);
		SCAN_VAR(DrvSioTxData);
		SCAN_VAR(DrvMdecDecoded);
		SCAN_VAR(DrvMdecOffset);
		SCAN_VAR(DrvMdecOutput);
		SCAN_VAR(DrvMdecQuantY);
		SCAN_VAR(DrvMdecQuantUV);
		SCAN_VAR(DrvMdecCos);
		SCAN_VAR(DrvMdecCosPrecalc);
		SCAN_VAR(DrvMdecUnpacked);
		SCAN_VAR(DrvMdecCommand0);
		SCAN_VAR(DrvMdecAddress0);
		SCAN_VAR(DrvMdecSize0);
		SCAN_VAR(DrvMdecCommand1);
		SCAN_VAR(DrvMdecStatus1);
		SCAN_VAR(ZnCatState);
		SCAN_VAR(ZnSelect);
		SCAN_VAR(ZnBank);
		SCAN_VAR(ZnSoundBank);
		SCAN_VAR(ZnLatch);
		SCAN_VAR(ZnLatchPending);
		SCAN_VAR(ZnCoin);
		SCAN_VAR(ZnSpuToggle);
		SCAN_VAR(ZnMcuByte);
		SCAN_VAR(ZnQData);
		SCAN_VAR(ZnMainExtra);
		SCAN_VAR(ZnSoundExtra);
		SCAN_VAR(ZnIrqPhase);
	}
	if (nAction & ACB_WRITE) {
		ZnMapRamConfig();
		ZnMapRomConfig();
		ZnMapBank();
		ZetOpen(0);
		ZnMapSoundBank();
		ZetClose();
		ZnUpdateIrq();
		DrvRecalc = 1;
	}
	return 0;
}

static struct BurnInputInfo ZnInputList[] = {
	{ "P1 Coin",      BIT_DIGITAL,   ZnSystem + 4,  "p1 coin"   },
	{ "P1 Start",     BIT_DIGITAL,   ZnSystem + 0,  "p1 start"  },
	{ "P1 Up",        BIT_DIGITAL,   ZnJoy[0] + 0,  "p1 up"     },
	{ "P1 Down",      BIT_DIGITAL,   ZnJoy[0] + 1,  "p1 down"   },
	{ "P1 Left",      BIT_DIGITAL,   ZnJoy[0] + 2,  "p1 left"   },
	{ "P1 Right",     BIT_DIGITAL,   ZnJoy[0] + 3,  "p1 right"  },
	{ "P1 Button 1",  BIT_DIGITAL,   ZnJoy[0] + 4,  "p1 fire 1" },
	{ "P1 Button 2",  BIT_DIGITAL,   ZnJoy[0] + 5,  "p1 fire 2" },
	{ "P1 Button 3",  BIT_DIGITAL,   ZnJoy[0] + 6,  "p1 fire 3" },
	{ "P1 Button 4",  BIT_DIGITAL,   ZnJoy[0] + 7,  "p1 fire 4" },
	{ "P1 Button 5",  BIT_DIGITAL,   ZnJoy[0] + 8,  "p1 fire 5" },
	{ "P1 Button 6",  BIT_DIGITAL,   ZnJoy[0] + 9,  "p1 fire 6" },
	{ "P2 Coin",      BIT_DIGITAL,   ZnSystem + 5,  "p2 coin"   },
	{ "P2 Start",     BIT_DIGITAL,   ZnSystem + 1,  "p2 start"  },
	{ "P2 Up",        BIT_DIGITAL,   ZnJoy[1] + 0,  "p2 up"     },
	{ "P2 Down",      BIT_DIGITAL,   ZnJoy[1] + 1,  "p2 down"   },
	{ "P2 Left",      BIT_DIGITAL,   ZnJoy[1] + 2,  "p2 left"   },
	{ "P2 Right",     BIT_DIGITAL,   ZnJoy[1] + 3,  "p2 right"  },
	{ "P2 Button 1",  BIT_DIGITAL,   ZnJoy[1] + 4,  "p2 fire 1" },
	{ "P2 Button 2",  BIT_DIGITAL,   ZnJoy[1] + 5,  "p2 fire 2" },
	{ "P2 Button 3",  BIT_DIGITAL,   ZnJoy[1] + 6,  "p2 fire 3" },
	{ "P2 Button 4",  BIT_DIGITAL,   ZnJoy[1] + 7,  "p2 fire 4" },
	{ "P2 Button 5",  BIT_DIGITAL,   ZnJoy[1] + 8,  "p2 fire 5" },
	{ "P2 Button 6",  BIT_DIGITAL,   ZnJoy[1] + 9,  "p2 fire 6" },
	{ "Service Mode", BIT_DIGITAL,   ZnService + 0, "diag"      },
	{ "Service",      BIT_DIGITAL,   ZnService + 1, "service"   },
	{ "Tilt",         BIT_DIGITAL,   ZnService + 2, "tilt"      },
	{ "Reset",        BIT_DIGITAL,   &DrvReset,     "reset"     },
	{ "Dip A",        BIT_DIPSWITCH, &ZnDip,        "dip"       },
};

STDINPUTINFO(Zn)

static struct BurnDIPInfo ZnDIPList[] = {
	{ 0x1c, 0xff, 0xff, 0xff, NULL                },

	{ 0,    0xfe, 0,    2,    "BIOS Service Mode" },
	{ 0x1c, 0x01, 0x02, 0x02, "Off"               },
	{ 0x1c, 0x01, 0x02, 0x00, "On"                },
};

STDDIPINFO(Zn)

static INT32 ZnBiosInit()
{
	return 1;
}

// Registration order follows MAME 0.289 zn.cpp (Capcom ZN entries only).

// Capcom ZN-1
static struct BurnRomInfo coh1000cRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },
};

STD_ROM_PICK(coh1000c)

STD_ROM_FN(coh1000c)

struct BurnDriver BurnDrvcoh1000c = {
	"coh1000c", NULL, NULL, NULL, "1995",
	"Capcom ZN-1\0", "BIOS only", "Capcom", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, coh1000cRomInfo, coh1000cRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	ZnBiosInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Battle Arena Toshinden 2 (Europe 951124)
static struct BurnRomInfo ts2RomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ts2e_04.2h",        0x080000, 0x7a3b535f, 2 | BRF_PRG | BRF_ESS            },

	{ "ts2-05m.3h",        0x400000, 0x7f4228e2, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-06m.4h",        0x200000, 0x67be6797, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-07m.5h",        0x200000, 0xdb73e2b8, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-08m.2k",        0x200000, 0x01a48431, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-09m.3k",        0x200000, 0x83f408de, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-10m.4k",        0x200000, 0xad90679a, 3 | BRF_PRG | BRF_ESS            },

	{ "ts2_02.2e",         0x020000, 0x2f45c461, 4 | BRF_PRG | BRF_ESS            },

	{ "ts2-01m.3b",        0x400000, 0xd7a505e0, 5 | BRF_SND                      },

	{ "cp02",              0x000008, 0x40b0b697, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(ts2)

STD_ROM_FN(ts2)

static const ZnRomLoad ts2Loads[] = {
	{ 0,  0, 0x0,      0x80000,  0 },
	{ 1,  0, 0x0,      0x80000,  0 },
	{ 2,  5, 0x0,      0x8,      0 },
	{ 3,  1, 0x0,      0x80000,  0 },
	{ 4,  2, 0x0,      0x400000, 0 },
	{ 5,  2, 0x400000, 0x200000, 0 },
	{ 6,  2, 0x600000, 0x200000, 0 },
	{ 7,  2, 0x800000, 0x200000, 0 },
	{ 8,  2, 0xa00000, 0x200000, 0 },
	{ 9,  2, 0xc00000, 0x200000, 0 },
	{ 10, 3, 0x0,      0x20000,  0 },
	{ 11, 4, 0x0,      0x400000, 1 },
	{ 12, 6, 0x0,      0x8,      0 },
};

static INT32 ts2Init()
{
	return DrvInit(ts2Loads, 13, 1, 1, 0, 6);
}

struct BurnDriver BurnDrvts2 = {
	"ts2", NULL, "coh1000c", NULL, "1995",
	"Battle Arena Toshinden 2 (Europe 951124)\0", NULL, "Capcom / Takara", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ts2RomInfo, ts2RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	ts2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Battle Arena Toshinden 2 (USA 951124)
static struct BurnRomInfo ts2uRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ts2u_04.2h",        0x080000, 0xddb52e7c, 2 | BRF_PRG | BRF_ESS            },

	{ "ts2-05m.3h",        0x400000, 0x7f4228e2, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-06m.4h",        0x200000, 0x67be6797, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-07m.5h",        0x200000, 0xdb73e2b8, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-08m.2k",        0x200000, 0x01a48431, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-09m.3k",        0x200000, 0x83f408de, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-10m.4k",        0x200000, 0xad90679a, 3 | BRF_PRG | BRF_ESS            },

	{ "ts2_02.2e",         0x020000, 0x2f45c461, 4 | BRF_PRG | BRF_ESS            },

	{ "ts2-01m.3b",        0x400000, 0xd7a505e0, 5 | BRF_SND                      },

	{ "cp02",              0x000008, 0x40b0b697, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(ts2u)

STD_ROM_FN(ts2u)

static const ZnRomLoad ts2uLoads[] = {
	{ 0,  0, 0x0,      0x80000,  0 },
	{ 1,  0, 0x0,      0x80000,  0 },
	{ 2,  5, 0x0,      0x8,      0 },
	{ 3,  1, 0x0,      0x80000,  0 },
	{ 4,  2, 0x0,      0x400000, 0 },
	{ 5,  2, 0x400000, 0x200000, 0 },
	{ 6,  2, 0x600000, 0x200000, 0 },
	{ 7,  2, 0x800000, 0x200000, 0 },
	{ 8,  2, 0xa00000, 0x200000, 0 },
	{ 9,  2, 0xc00000, 0x200000, 0 },
	{ 10, 3, 0x0,      0x20000,  0 },
	{ 11, 4, 0x0,      0x400000, 1 },
	{ 12, 6, 0x0,      0x8,      0 },
};

static INT32 ts2uInit()
{
	return DrvInit(ts2uLoads, 13, 1, 1, 0, 6);
}

struct BurnDriver BurnDrvts2u = {
	"ts2u", "ts2", "coh1000c", NULL, "1995",
	"Battle Arena Toshinden 2 (USA 951124)\0", NULL, "Capcom / Takara", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ts2uRomInfo, ts2uRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	ts2uInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Battle Arena Toshinden 2 (USA 951124, 32Mb mask ROMs)
static struct BurnRomInfo ts2uaRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ts2u_04.2h",        0x080000, 0xddb52e7c, 2 | BRF_PRG | BRF_ESS            },

	{ "ts2-05m.3h",        0x400000, 0x7f4228e2, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-06m",           0x400000, 0xcd7e0a27, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-08m",           0x400000, 0xb1f7f115, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-10m.4k",        0x200000, 0xad90679a, 3 | BRF_PRG | BRF_ESS            },

	{ "ts2_02.2e",         0x020000, 0x2f45c461, 4 | BRF_PRG | BRF_ESS            },

	{ "ts2-01m.3b",        0x400000, 0xd7a505e0, 5 | BRF_SND                      },

	{ "cp02",              0x000008, 0x40b0b697, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(ts2ua)

STD_ROM_FN(ts2ua)

static const ZnRomLoad ts2uaLoads[] = {
	{ 0,  0, 0x0,      0x80000,  0 },
	{ 1,  0, 0x0,      0x80000,  0 },
	{ 2,  5, 0x0,      0x8,      0 },
	{ 3,  1, 0x0,      0x80000,  0 },
	{ 4,  2, 0x0,      0x400000, 0 },
	{ 5,  2, 0x400000, 0x400000, 0 },
	{ 6,  2, 0x800000, 0x400000, 0 },
	{ 7,  2, 0xc00000, 0x200000, 0 },
	{ 8,  3, 0x0,      0x20000,  0 },
	{ 9,  4, 0x0,      0x400000, 1 },
	{ 10, 6, 0x0,      0x8,      0 },
};

static INT32 ts2uaInit()
{
	return DrvInit(ts2uaLoads, 11, 1, 1, 0, 6);
}

struct BurnDriver BurnDrvts2ua = {
	"ts2ua", "ts2", "coh1000c", NULL, "1995",
	"Battle Arena Toshinden 2 (USA 951124, 32Mb mask ROMs)\0", NULL, "Capcom / Takara", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ts2uaRomInfo, ts2uaRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	ts2uaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Battle Arena Toshinden 2 (Japan 951124)
static struct BurnRomInfo ts2jRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ts2j_04.2h",        0x080000, 0x4aba8c5e, 2 | BRF_PRG | BRF_ESS            },

	{ "ts2-05m.3h",        0x400000, 0x7f4228e2, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-06m.4h",        0x200000, 0x67be6797, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-07m.5h",        0x200000, 0xdb73e2b8, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-08m.2k",        0x200000, 0x01a48431, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-09m.3k",        0x200000, 0x83f408de, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-10m.4k",        0x200000, 0xad90679a, 3 | BRF_PRG | BRF_ESS            },

	{ "ts2_02.2e",         0x020000, 0x2f45c461, 4 | BRF_PRG | BRF_ESS            },

	{ "ts2-01m.3b",        0x400000, 0xd7a505e0, 5 | BRF_SND                      },

	{ "cp02",              0x000008, 0x40b0b697, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(ts2j)

STD_ROM_FN(ts2j)

static const ZnRomLoad ts2jLoads[] = {
	{ 0,  0, 0x0,      0x80000,  0 },
	{ 1,  0, 0x0,      0x80000,  0 },
	{ 2,  5, 0x0,      0x8,      0 },
	{ 3,  1, 0x0,      0x80000,  0 },
	{ 4,  2, 0x0,      0x400000, 0 },
	{ 5,  2, 0x400000, 0x200000, 0 },
	{ 6,  2, 0x600000, 0x200000, 0 },
	{ 7,  2, 0x800000, 0x200000, 0 },
	{ 8,  2, 0xa00000, 0x200000, 0 },
	{ 9,  2, 0xc00000, 0x200000, 0 },
	{ 10, 3, 0x0,      0x20000,  0 },
	{ 11, 4, 0x0,      0x400000, 1 },
	{ 12, 6, 0x0,      0x8,      0 },
};

static INT32 ts2jInit()
{
	return DrvInit(ts2jLoads, 13, 1, 1, 0, 6);
}

struct BurnDriver BurnDrvts2j = {
	"ts2j", "ts2", "coh1000c", NULL, "1995",
	"Battle Arena Toshinden 2 (Japan 951124)\0", NULL, "Capcom / Takara", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ts2jRomInfo, ts2jRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	ts2jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Battle Arena Toshinden 2 (Japan 951124, 32Mb mask ROMs)
static struct BurnRomInfo ts2jaRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ts2j_04.2h",        0x080000, 0x4aba8c5e, 2 | BRF_PRG | BRF_ESS            },

	{ "ts2-05m.3h",        0x400000, 0x7f4228e2, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-06m",           0x400000, 0xcd7e0a27, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-08m",           0x400000, 0xb1f7f115, 3 | BRF_PRG | BRF_ESS            },
	{ "ts2-10m.4k",        0x200000, 0xad90679a, 3 | BRF_PRG | BRF_ESS            },

	{ "ts2_02.2e",         0x020000, 0x2f45c461, 4 | BRF_PRG | BRF_ESS            },

	{ "ts2-01m.3b",        0x400000, 0xd7a505e0, 5 | BRF_SND                      },

	{ "cp02",              0x000008, 0x40b0b697, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(ts2ja)

STD_ROM_FN(ts2ja)

static const ZnRomLoad ts2jaLoads[] = {
	{ 0,  0, 0x0,      0x80000,  0 },
	{ 1,  0, 0x0,      0x80000,  0 },
	{ 2,  5, 0x0,      0x8,      0 },
	{ 3,  1, 0x0,      0x80000,  0 },
	{ 4,  2, 0x0,      0x400000, 0 },
	{ 5,  2, 0x400000, 0x400000, 0 },
	{ 6,  2, 0x800000, 0x400000, 0 },
	{ 7,  2, 0xc00000, 0x200000, 0 },
	{ 8,  3, 0x0,      0x20000,  0 },
	{ 9,  4, 0x0,      0x400000, 1 },
	{ 10, 6, 0x0,      0x8,      0 },
};

static INT32 ts2jaInit()
{
	return DrvInit(ts2jaLoads, 11, 1, 1, 0, 6);
}

struct BurnDriver BurnDrvts2ja = {
	"ts2ja", "ts2", "coh1000c", NULL, "1995",
	"Battle Arena Toshinden 2 (Japan 951124, 32Mb mask ROMs)\0", NULL, "Capcom / Takara", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ts2jaRomInfo, ts2jaRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	ts2jaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Star Gladiator Episode I: Final Crusade (USA 960627)
static struct BurnRomInfo stargladRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ps1u_04.2h",        0x080000, 0x121fb234, 2 | BRF_PRG | BRF_ESS            },

	{ "ps1-05m.3h",        0x400000, 0x8ad72c4f, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-06m.4h",        0x400000, 0x95d8ed61, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-07m.5h",        0x400000, 0xc06752db, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-08m.2k",        0x400000, 0x381f9ded, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-09m.3k",        0x400000, 0xbd894812, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-10m.4k",        0x400000, 0xff80c18a, 3 | BRF_PRG | BRF_ESS            },

	{ "ps1_02a.2e",        0x020000, 0xb854df92, 4 | BRF_PRG | BRF_ESS            },
	{ "ps1_03a.3e",        0x020000, 0xa2562fbb, 4 | BRF_PRG | BRF_ESS            },

	{ "ps1-01m.3b",        0x400000, 0x0bfb17aa, 5 | BRF_SND                      },

	{ "cp03",              0x000008, 0x27970400, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(starglad)

STD_ROM_FN(starglad)

static const ZnRomLoad stargladLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 stargladInit()
{
	return DrvInit(stargladLoads, 14, 1, 1, 0, 4);
}

struct BurnDriver BurnDrvstarglad = {
	"starglad", NULL, "coh1000c", NULL, "1996",
	"Star Gladiator Episode I: Final Crusade (USA 960627)\0", NULL, "Capcom", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, stargladRomInfo, stargladRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	stargladInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Star Gladiator Episode I: Final Crusade (Japan 960627)
static struct BurnRomInfo stargladjRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ps1j_04.2h",        0x080000, 0xf865006d, 2 | BRF_PRG | BRF_ESS            },

	{ "ps1-05m.3h",        0x400000, 0x8ad72c4f, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-06m.4h",        0x400000, 0x95d8ed61, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-07m.5h",        0x400000, 0xc06752db, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-08m.2k",        0x400000, 0x381f9ded, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-09m.3k",        0x400000, 0xbd894812, 3 | BRF_PRG | BRF_ESS            },
	{ "ps1-10m.4k",        0x400000, 0xff80c18a, 3 | BRF_PRG | BRF_ESS            },

	{ "ps1_02a.2e",        0x020000, 0xb854df92, 4 | BRF_PRG | BRF_ESS            },
	{ "ps1_03a.3e",        0x020000, 0xa2562fbb, 4 | BRF_PRG | BRF_ESS            },

	{ "ps1-01m.3b",        0x400000, 0x0bfb17aa, 5 | BRF_SND                      },

	{ "cp03",              0x000008, 0x27970400, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(stargladj)

STD_ROM_FN(stargladj)

static const ZnRomLoad stargladjLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 stargladjInit()
{
	return DrvInit(stargladjLoads, 14, 1, 1, 0, 4);
}

struct BurnDriver BurnDrvstargladj = {
	"stargladj", "starglad", "coh1000c", NULL, "1996",
	"Star Gladiator Episode I: Final Crusade (Japan 960627)\0", NULL, "Capcom", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, stargladjRomInfo, stargladjRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	stargladjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Gallop Racer (English Ver 10.17.K)
static struct BurnRomInfo glpracrRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "gra_ep-eng.2h",     0x080000, 0xf3ab9c85, 2 | BRF_PRG | BRF_ESS            },

	{ "gra-05m.3h",        0x400000, 0x78053700, 3 | BRF_PRG | BRF_ESS            },
	{ "gra-06m.4h",        0x400000, 0xd73b392b, 3 | BRF_PRG | BRF_ESS            },
	{ "gra-07m.5h",        0x400000, 0xacaefe3a, 3 | BRF_PRG | BRF_ESS            },

	{ "cp05",              0x000008, 0xae930900, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(glpracr)

STD_ROM_FN(glpracr)

static const ZnRomLoad glpracrLoads[] = {
	{ 0, 0, 0x0,      0x80000,  0 },
	{ 1, 0, 0x0,      0x80000,  0 },
	{ 2, 5, 0x0,      0x8,      0 },
	{ 3, 1, 0x0,      0x80000,  0 },
	{ 4, 2, 0x0,      0x400000, 0 },
	{ 5, 2, 0x400000, 0x400000, 0 },
	{ 6, 2, 0x800000, 0x400000, 0 },
	{ 7, 6, 0x0,      0x8,      0 },
};

static INT32 glpracrInit()
{
	return DrvInit(glpracrLoads, 8, 1, 1, 1, 6);
}

struct BurnDriver BurnDrvglpracr = {
	"glpracr", NULL, "coh1000c", NULL, "1996",
	"Gallop Racer (English Ver 10.17.K)\0", "PSX SPU sound incomplete", "Tecmo", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, glpracrRomInfo, glpracrRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	glpracrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Gallop Racer (Japanese Ver 9.01.12)
static struct BurnRomInfo glpracrjRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "gpaj_04.2h",        0x080000, 0x53bf551c, 2 | BRF_PRG | BRF_ESS            },

	{ "gra-05m.3h",        0x400000, 0x78053700, 3 | BRF_PRG | BRF_ESS            },
	{ "gra-06m.4h",        0x400000, 0xd73b392b, 3 | BRF_PRG | BRF_ESS            },
	{ "gra-07m.5h",        0x400000, 0xacaefe3a, 3 | BRF_PRG | BRF_ESS            },

	{ "cp05",              0x000008, 0xae930900, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(glpracrj)

STD_ROM_FN(glpracrj)

static const ZnRomLoad glpracrjLoads[] = {
	{ 0, 0, 0x0,      0x80000,  0 },
	{ 1, 0, 0x0,      0x80000,  0 },
	{ 2, 5, 0x0,      0x8,      0 },
	{ 3, 1, 0x0,      0x80000,  0 },
	{ 4, 2, 0x0,      0x400000, 0 },
	{ 5, 2, 0x400000, 0x400000, 0 },
	{ 6, 2, 0x800000, 0x400000, 0 },
	{ 7, 6, 0x0,      0x8,      0 },
};

static INT32 glpracrjInit()
{
	return DrvInit(glpracrjLoads, 8, 1, 1, 1, 6);
}

struct BurnDriver BurnDrvglpracrj = {
	"glpracrj", "glpracr", "coh1000c", NULL, "1996",
	"Gallop Racer (Japanese Ver 9.01.12)\0", "PSX SPU sound incomplete", "Tecmo", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, glpracrjRomInfo, glpracrjRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	glpracrjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX (Europe 961219)
static struct BurnRomInfo sfexRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfee_04a.2h",       0x080000, 0x092cfa2e, 2 | BRF_PRG | BRF_ESS            },

	{ "sfe-05m.3h",        0x400000, 0xeab781fe, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-06m.4h",        0x400000, 0x999de60c, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-07m.5h",        0x400000, 0x76117b0a, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-08m.2k",        0x400000, 0xa36bbec5, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-09m.3k",        0x400000, 0x62c424cc, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-10m.4k",        0x400000, 0x83791a8b, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex)

STD_ROM_FN(sfex)

static const ZnRomLoad sfexLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexInit()
{
	return DrvInit(sfexLoads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex = {
	"sfex", NULL, "coh1000c", NULL, "1996",
	"Street Fighter EX (Europe 961219)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexRomInfo, sfexRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX (USA 961219)
static struct BurnRomInfo sfexuRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfeu_04b.2h",       0x080000, 0xde02bd29, 2 | BRF_PRG | BRF_ESS            },

	{ "sfe-05m.3h",        0x400000, 0xeab781fe, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-06m.4h",        0x400000, 0x999de60c, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-07m.5h",        0x400000, 0x76117b0a, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-08m.2k",        0x400000, 0xa36bbec5, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-09m.3k",        0x400000, 0x62c424cc, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-10m.4k",        0x400000, 0x83791a8b, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfexu)

STD_ROM_FN(sfexu)

static const ZnRomLoad sfexuLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexuInit()
{
	return DrvInit(sfexuLoads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfexu = {
	"sfexu", "sfex", "coh1000c", NULL, "1996",
	"Street Fighter EX (USA 961219)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexuRomInfo, sfexuRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX (Asia 961219)
static struct BurnRomInfo sfexaRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfea_04a.2h",       0x080000, 0x08247bd4, 2 | BRF_PRG | BRF_ESS            },

	{ "sfe-05m.3h",        0x400000, 0xeab781fe, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-06m.4h",        0x400000, 0x999de60c, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-07m.5h",        0x400000, 0x76117b0a, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-08m.2k",        0x400000, 0xa36bbec5, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-09m.3k",        0x400000, 0x62c424cc, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-10m.4k",        0x400000, 0x83791a8b, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfexa)

STD_ROM_FN(sfexa)

static const ZnRomLoad sfexaLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexaInit()
{
	return DrvInit(sfexaLoads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfexa = {
	"sfexa", "sfex", "coh1000c", NULL, "1996",
	"Street Fighter EX (Asia 961219)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexaRomInfo, sfexaRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX (Japan 961130)
static struct BurnRomInfo sfexjRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfej_04.2h",        0x080000, 0xea100607, 2 | BRF_PRG | BRF_ESS            },

	{ "sfe-05m.3h",        0x400000, 0xeab781fe, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-06m.4h",        0x400000, 0x999de60c, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-07m.5h",        0x400000, 0x76117b0a, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-08m.2k",        0x400000, 0xa36bbec5, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-09m.3k",        0x400000, 0x62c424cc, 3 | BRF_PRG | BRF_ESS            },
	{ "sfe-10m.4k",        0x400000, 0x83791a8b, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfexj)

STD_ROM_FN(sfexj)

static const ZnRomLoad sfexjLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexjInit()
{
	return DrvInit(sfexjLoads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfexj = {
	"sfexj", "sfex", "coh1000c", NULL, "1996",
	"Street Fighter EX (Japan 961130)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexjRomInfo, sfexjRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX Plus (USA 970407)
static struct BurnRomInfo sfexpRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfpu_04a.2h",       0x080000, 0x4617adc2, 2 | BRF_PRG | BRF_ESS            },

	{ "sfp-05m.3h",        0x400000, 0xac7dcc5e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-06m.4h",        0x400000, 0x1d504758, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-07m.5h",        0x400000, 0x0f585f30, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-08m.2k",        0x400000, 0x65eabc61, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-09m.3k",        0x400000, 0x15f8b71e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-10m.4k",        0x400000, 0xc1ecf652, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfexp)

STD_ROM_FN(sfexp)

static const ZnRomLoad sfexpLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexpInit()
{
	return DrvInit(sfexpLoads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfexp = {
	"sfexp", NULL, "coh1000c", NULL, "1997",
	"Street Fighter EX Plus (USA 970407)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexpRomInfo, sfexpRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX Plus (USA 970311)
static struct BurnRomInfo sfexpu1RomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfpu_04.2h",        0x080000, 0x305e4ec0, 2 | BRF_PRG | BRF_ESS            },

	{ "sfp-05m.3h",        0x400000, 0xac7dcc5e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-06m.4h",        0x400000, 0x1d504758, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-07m.5h",        0x400000, 0x0f585f30, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-08m.2k",        0x400000, 0x65eabc61, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-09m.3k",        0x400000, 0x15f8b71e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-10m.4k",        0x400000, 0xc1ecf652, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfexpu1)

STD_ROM_FN(sfexpu1)

static const ZnRomLoad sfexpu1Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexpu1Init()
{
	return DrvInit(sfexpu1Loads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfexpu1 = {
	"sfexpu1", "sfexp", "coh1000c", NULL, "1997",
	"Street Fighter EX Plus (USA 970311)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexpu1RomInfo, sfexpu1RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexpu1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX Plus (Japan 970407)
static struct BurnRomInfo sfexpjRomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfpj_04a.2h",       0x080000, 0x6e99a0f7, 2 | BRF_PRG | BRF_ESS            },

	{ "sfp-05m.3h",        0x400000, 0xac7dcc5e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-06m.4h",        0x400000, 0x1d504758, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-07m.5h",        0x400000, 0x0f585f30, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-08m.2k",        0x400000, 0x65eabc61, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-09m.3k",        0x400000, 0x15f8b71e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-10m.4k",        0x400000, 0xc1ecf652, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfexpj)

STD_ROM_FN(sfexpj)

static const ZnRomLoad sfexpjLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexpjInit()
{
	return DrvInit(sfexpjLoads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfexpj = {
	"sfexpj", "sfexp", "coh1000c", NULL, "1997",
	"Street Fighter EX Plus (Japan 970407)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexpjRomInfo, sfexpjRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexpjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX Plus (Japan 970311)
static struct BurnRomInfo sfexpj1RomDesc[] = {
	{ "m534002c-13.ic353", 0x080000, 0x50033af6, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },
	{ "msm27c402zb.ic353", 0x080000, 0xf20f7fe5, 1 | BRF_PRG | BRF_OPT | BRF_BIOS },

	{ "cp01.ic652",        0x000008, 0xbb7ba033, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sfpj_04.2h",        0x080000, 0x18d043f5, 2 | BRF_PRG | BRF_ESS            },

	{ "sfp-05m.3h",        0x400000, 0xac7dcc5e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-06m.4h",        0x400000, 0x1d504758, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-07m.5h",        0x400000, 0x0f585f30, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-08m.2k",        0x400000, 0x65eabc61, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-09m.3k",        0x400000, 0x15f8b71e, 3 | BRF_PRG | BRF_ESS            },
	{ "sfp-10m.4k",        0x400000, 0xc1ecf652, 3 | BRF_PRG | BRF_ESS            },

	{ "sfe_02.2e",         0x020000, 0x1908475c, 4 | BRF_PRG | BRF_ESS            },
	{ "sfe_03.3e",         0x020000, 0x95c1e2e0, 4 | BRF_PRG | BRF_ESS            },

	{ "sfe-01m.3b",        0x400000, 0xf5afff0d, 5 | BRF_SND                      },

	{ "cp04",              0x000008, 0xe0dc24ae, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfexpj1)

STD_ROM_FN(sfexpj1)

static const ZnRomLoad sfexpj1Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  0, 0x0,       0x80000,  0 },
	{ 2,  5, 0x0,       0x8,      0 },
	{ 3,  1, 0x0,       0x80000,  0 },
	{ 4,  2, 0x0,       0x400000, 0 },
	{ 5,  2, 0x400000,  0x400000, 0 },
	{ 6,  2, 0x800000,  0x400000, 0 },
	{ 7,  2, 0xc00000,  0x400000, 0 },
	{ 8,  2, 0x1000000, 0x400000, 0 },
	{ 9,  2, 0x1400000, 0x400000, 0 },
	{ 10, 3, 0x0,       0x20000,  0 },
	{ 11, 3, 0x20000,   0x20000,  0 },
	{ 12, 4, 0x0,       0x400000, 1 },
	{ 13, 6, 0x0,       0x8,      0 },
};

static INT32 sfexpj1Init()
{
	return DrvInit(sfexpj1Loads, 14, 1, 2, 0, 6);
}

struct BurnDriver BurnDrvsfexpj1 = {
	"sfexpj1", "sfexp", "coh1000c", NULL, "1997",
	"Street Fighter EX Plus (Japan 970311)\0", NULL, "Capcom / Arika", "Capcom ZN-1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfexpj1RomInfo, sfexpj1RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfexpj1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Capcom ZN-2
static struct BurnRomInfo coh3002cRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },
};

STD_ROM_PICK(coh3002c)

STD_ROM_FN(coh3002c)

struct BurnDriver BurnDrvcoh3002c = {
	"coh3002c", NULL, NULL, NULL, "1997",
	"Capcom ZN-2\0", "BIOS only", "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, coh3002cRomInfo, coh3002cRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	ZnBiosInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Rival Schools: United By Fate (Europe 971117)
static struct BurnRomInfo rvschoolRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "jste_04.2h",        0x080000, 0x1567555a, 2 | BRF_PRG | BRF_ESS            },

	{ "jst-05m.3h",        0x400000, 0x723372b8, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-06m.4h",        0x400000, 0x4248988e, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-07m.5h",        0x400000, 0xc84c5a16, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-08m.2k",        0x400000, 0x791b57f3, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-09m.3k",        0x400000, 0x6df42048, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-10m.4k",        0x400000, 0xd7e22769, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-11m.5k",        0x400000, 0x0a033ac5, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-12m.6k",        0x400000, 0x43bd2ddd, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-13m.7k",        0x400000, 0x6b443235, 3 | BRF_PRG | BRF_ESS            },

	{ "jst_02.2e",         0x020000, 0x7809e2c3, 4 | BRF_PRG | BRF_ESS            },
	{ "jst_03.3e",         0x020000, 0x860ff24d, 4 | BRF_PRG | BRF_ESS            },

	{ "jst-01m.3b",        0x400000, 0x9a7c98f9, 5 | BRF_SND                      },

	{ "cp06",              0x000008, 0x99b22128, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(rvschool)

STD_ROM_FN(rvschool)

static const ZnRomLoad rvschoolLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x400000, 0 },
	{ 4,  2, 0x400000,  0x400000, 0 },
	{ 5,  2, 0x800000,  0x400000, 0 },
	{ 6,  2, 0xc00000,  0x400000, 0 },
	{ 7,  2, 0x1000000, 0x400000, 0 },
	{ 8,  2, 0x1400000, 0x400000, 0 },
	{ 9,  2, 0x1800000, 0x400000, 0 },
	{ 10, 2, 0x1c00000, 0x400000, 0 },
	{ 11, 2, 0x2000000, 0x400000, 0 },
	{ 12, 3, 0x0,       0x20000,  0 },
	{ 13, 3, 0x20000,   0x20000,  0 },
	{ 14, 4, 0x0,       0x400000, 1 },
	{ 15, 6, 0x0,       0x8,      0 },
};

static INT32 rvschoolInit()
{
	return DrvInit(rvschoolLoads, 16, 2, 2, 0, 4);
}

struct BurnDriver BurnDrvrvschool = {
	"rvschool", NULL, "coh3002c", NULL, "1997",
	"Rival Schools: United By Fate (Europe 971117)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, rvschoolRomInfo, rvschoolRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	rvschoolInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Rival Schools: United By Fate (USA 971117)
static struct BurnRomInfo rvschooluRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "jstu_04.2h",        0x080000, 0xd83724ae, 2 | BRF_PRG | BRF_ESS            },

	{ "jst-05m.3h",        0x400000, 0x723372b8, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-06m.4h",        0x400000, 0x4248988e, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-07m.5h",        0x400000, 0xc84c5a16, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-08m.2k",        0x400000, 0x791b57f3, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-09m.3k",        0x400000, 0x6df42048, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-10m.4k",        0x400000, 0xd7e22769, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-11m.5k",        0x400000, 0x0a033ac5, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-12m.6k",        0x400000, 0x43bd2ddd, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-13m.7k",        0x400000, 0x6b443235, 3 | BRF_PRG | BRF_ESS            },

	{ "jst_02.2e",         0x020000, 0x7809e2c3, 4 | BRF_PRG | BRF_ESS            },
	{ "jst_03.3e",         0x020000, 0x860ff24d, 4 | BRF_PRG | BRF_ESS            },

	{ "jst-01m.3b",        0x400000, 0x9a7c98f9, 5 | BRF_SND                      },

	{ "cp06",              0x000008, 0x99b22128, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(rvschoolu)

STD_ROM_FN(rvschoolu)

static const ZnRomLoad rvschooluLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x400000, 0 },
	{ 4,  2, 0x400000,  0x400000, 0 },
	{ 5,  2, 0x800000,  0x400000, 0 },
	{ 6,  2, 0xc00000,  0x400000, 0 },
	{ 7,  2, 0x1000000, 0x400000, 0 },
	{ 8,  2, 0x1400000, 0x400000, 0 },
	{ 9,  2, 0x1800000, 0x400000, 0 },
	{ 10, 2, 0x1c00000, 0x400000, 0 },
	{ 11, 2, 0x2000000, 0x400000, 0 },
	{ 12, 3, 0x0,       0x20000,  0 },
	{ 13, 3, 0x20000,   0x20000,  0 },
	{ 14, 4, 0x0,       0x400000, 1 },
	{ 15, 6, 0x0,       0x8,      0 },
};

static INT32 rvschooluInit()
{
	return DrvInit(rvschooluLoads, 16, 2, 2, 0, 4);
}

struct BurnDriver BurnDrvrvschoolu = {
	"rvschoolu", "rvschool", "coh3002c", NULL, "1997",
	"Rival Schools: United By Fate (USA 971117)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, rvschooluRomInfo, rvschooluRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	rvschooluInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Rival Schools: United By Fate (Asia 971117)
static struct BurnRomInfo rvschoolaRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "jsta_04.2h",        0x080000, 0x034b1011, 2 | BRF_PRG | BRF_ESS            },

	{ "jst-05m.3h",        0x400000, 0x723372b8, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-06m.4h",        0x400000, 0x4248988e, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-07m.5h",        0x400000, 0xc84c5a16, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-08m.2k",        0x400000, 0x791b57f3, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-09m.3k",        0x400000, 0x6df42048, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-10m.4k",        0x400000, 0xd7e22769, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-11m.5k",        0x400000, 0x0a033ac5, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-12m.6k",        0x400000, 0x43bd2ddd, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-13m.7k",        0x400000, 0x6b443235, 3 | BRF_PRG | BRF_ESS            },

	{ "jst_02.2e",         0x020000, 0x7809e2c3, 4 | BRF_PRG | BRF_ESS            },
	{ "jst_03.3e",         0x020000, 0x860ff24d, 4 | BRF_PRG | BRF_ESS            },

	{ "jst-01m.3b",        0x400000, 0x9a7c98f9, 5 | BRF_SND                      },

	{ "cp06",              0x000008, 0x99b22128, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(rvschoola)

STD_ROM_FN(rvschoola)

static const ZnRomLoad rvschoolaLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x400000, 0 },
	{ 4,  2, 0x400000,  0x400000, 0 },
	{ 5,  2, 0x800000,  0x400000, 0 },
	{ 6,  2, 0xc00000,  0x400000, 0 },
	{ 7,  2, 0x1000000, 0x400000, 0 },
	{ 8,  2, 0x1400000, 0x400000, 0 },
	{ 9,  2, 0x1800000, 0x400000, 0 },
	{ 10, 2, 0x1c00000, 0x400000, 0 },
	{ 11, 2, 0x2000000, 0x400000, 0 },
	{ 12, 3, 0x0,       0x20000,  0 },
	{ 13, 3, 0x20000,   0x20000,  0 },
	{ 14, 4, 0x0,       0x400000, 1 },
	{ 15, 6, 0x0,       0x8,      0 },
};

static INT32 rvschoolaInit()
{
	return DrvInit(rvschoolaLoads, 16, 2, 2, 0, 4);
}

struct BurnDriver BurnDrvrvschoola = {
	"rvschoola", "rvschool", "coh3002c", NULL, "1997",
	"Rival Schools: United By Fate (Asia 971117)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, rvschoolaRomInfo, rvschoolaRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	rvschoolaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Shiritsu Justice Gakuen: Legion of Heroes (Japan 971216)
static struct BurnRomInfo jgakuenRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "jstj_04a.2h",       0x080000, 0x91b36f38, 2 | BRF_PRG | BRF_ESS            },

	{ "jst-05m.3h",        0x400000, 0x723372b8, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-06m.4h",        0x400000, 0x4248988e, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-07m.5h",        0x400000, 0xc84c5a16, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-08m.2k",        0x400000, 0x791b57f3, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-09m.3k",        0x400000, 0x6df42048, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-10m.4k",        0x400000, 0xd7e22769, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-11m.5k",        0x400000, 0x0a033ac5, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-12m.6k",        0x400000, 0x43bd2ddd, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-13m.7k",        0x400000, 0x6b443235, 3 | BRF_PRG | BRF_ESS            },

	{ "jst_02.2e",         0x020000, 0x7809e2c3, 4 | BRF_PRG | BRF_ESS            },
	{ "jst_03.3e",         0x020000, 0x860ff24d, 4 | BRF_PRG | BRF_ESS            },

	{ "jst-01m.3b",        0x400000, 0x9a7c98f9, 5 | BRF_SND                      },

	{ "cp06",              0x000008, 0x99b22128, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(jgakuen)

STD_ROM_FN(jgakuen)

static const ZnRomLoad jgakuenLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x400000, 0 },
	{ 4,  2, 0x400000,  0x400000, 0 },
	{ 5,  2, 0x800000,  0x400000, 0 },
	{ 6,  2, 0xc00000,  0x400000, 0 },
	{ 7,  2, 0x1000000, 0x400000, 0 },
	{ 8,  2, 0x1400000, 0x400000, 0 },
	{ 9,  2, 0x1800000, 0x400000, 0 },
	{ 10, 2, 0x1c00000, 0x400000, 0 },
	{ 11, 2, 0x2000000, 0x400000, 0 },
	{ 12, 3, 0x0,       0x20000,  0 },
	{ 13, 3, 0x20000,   0x20000,  0 },
	{ 14, 4, 0x0,       0x400000, 1 },
	{ 15, 6, 0x0,       0x8,      0 },
};

static INT32 jgakuenInit()
{
	return DrvInit(jgakuenLoads, 16, 2, 2, 0, 4);
}

struct BurnDriver BurnDrvjgakuen = {
	"jgakuen", "rvschool", "coh3002c", NULL, "1997",
	"Shiritsu Justice Gakuen: Legion of Heroes (Japan 971216)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, jgakuenRomInfo, jgakuenRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	jgakuenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Shiritsu Justice Gakuen: Legion of Heroes (Japan 971117)
static struct BurnRomInfo jgakuen1RomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "jstj_04.2h",        0x080000, 0x28b8000a, 2 | BRF_PRG | BRF_ESS            },

	{ "jst-05m.3h",        0x400000, 0x723372b8, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-06m.4h",        0x400000, 0x4248988e, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-07m.5h",        0x400000, 0xc84c5a16, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-08m.2k",        0x400000, 0x791b57f3, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-09m.3k",        0x400000, 0x6df42048, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-10m.4k",        0x400000, 0xd7e22769, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-11m.5k",        0x400000, 0x0a033ac5, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-12m.6k",        0x400000, 0x43bd2ddd, 3 | BRF_PRG | BRF_ESS            },
	{ "jst-13m.7k",        0x400000, 0x6b443235, 3 | BRF_PRG | BRF_ESS            },

	{ "jst_02.2e",         0x020000, 0x7809e2c3, 4 | BRF_PRG | BRF_ESS            },
	{ "jst_03.3e",         0x020000, 0x860ff24d, 4 | BRF_PRG | BRF_ESS            },

	{ "jst-01m.3b",        0x400000, 0x9a7c98f9, 5 | BRF_SND                      },

	{ "cp06",              0x000008, 0x99b22128, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(jgakuen1)

STD_ROM_FN(jgakuen1)

static const ZnRomLoad jgakuen1Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x400000, 0 },
	{ 4,  2, 0x400000,  0x400000, 0 },
	{ 5,  2, 0x800000,  0x400000, 0 },
	{ 6,  2, 0xc00000,  0x400000, 0 },
	{ 7,  2, 0x1000000, 0x400000, 0 },
	{ 8,  2, 0x1400000, 0x400000, 0 },
	{ 9,  2, 0x1800000, 0x400000, 0 },
	{ 10, 2, 0x1c00000, 0x400000, 0 },
	{ 11, 2, 0x2000000, 0x400000, 0 },
	{ 12, 3, 0x0,       0x20000,  0 },
	{ 13, 3, 0x20000,   0x20000,  0 },
	{ 14, 4, 0x0,       0x400000, 1 },
	{ 15, 6, 0x0,       0x8,      0 },
};

static INT32 jgakuen1Init()
{
	return DrvInit(jgakuen1Loads, 16, 2, 2, 0, 4);
}

struct BurnDriver BurnDrvjgakuen1 = {
	"jgakuen1", "rvschool", "coh3002c", NULL, "1997",
	"Shiritsu Justice Gakuen: Legion of Heroes (Japan 971117)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, jgakuen1RomInfo, jgakuen1RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	jgakuen1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 (Europe 980312)
static struct BurnRomInfo sfex2RomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ex2e_04.2h",        0x080000, 0x77e1622b, 2 | BRF_PRG | BRF_ESS            },

	{ "ex2-05m.3h",        0x800000, 0x78726b17, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-06m.4h",        0x800000, 0xbe1075ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-07m.5h",        0x800000, 0x6496c6ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-08m.2k",        0x800000, 0x3194132e, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-09m.3k",        0x400000, 0x075ae585, 3 | BRF_PRG | BRF_ESS            },

	{ "ex2_02.2e",         0x020000, 0x9489875e, 4 | BRF_PRG | BRF_ESS            },

	{ "ex2-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp08",              0x000008, 0xa63d6fa6, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2)

STD_ROM_FN(sfex2)

static const ZnRomLoad sfex2Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  4, 0x0,       0x400000, 1 },
	{ 10, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2Init()
{
	return DrvInit(sfex2Loads, 11, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2 = {
	"sfex2", NULL, "coh3002c", NULL, "1998",
	"Street Fighter EX2 (Europe 980312)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2RomInfo, sfex2RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 (USA 980526)
static struct BurnRomInfo sfex2uRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ex2u_04a.2h",       0x080000, 0x8dc5317f, 2 | BRF_PRG | BRF_ESS            },

	{ "ex2-05m.3h",        0x800000, 0x78726b17, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-06m.4h",        0x800000, 0xbe1075ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-07m.5h",        0x800000, 0x6496c6ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-08m.2k",        0x800000, 0x3194132e, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-09m.3k",        0x400000, 0x075ae585, 3 | BRF_PRG | BRF_ESS            },

	{ "ex2_02.2e",         0x020000, 0x9489875e, 4 | BRF_PRG | BRF_ESS            },

	{ "ex2-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp08",              0x000008, 0xa63d6fa6, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2u)

STD_ROM_FN(sfex2u)

static const ZnRomLoad sfex2uLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  4, 0x0,       0x400000, 1 },
	{ 10, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2uInit()
{
	return DrvInit(sfex2uLoads, 11, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2u = {
	"sfex2u", "sfex2", "coh3002c", NULL, "1998",
	"Street Fighter EX2 (USA 980526)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2uRomInfo, sfex2uRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2uInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 (USA 980312)
static struct BurnRomInfo sfex2u1RomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ex2u_04.2h",        0x080000, 0xeb417c2c, 2 | BRF_PRG | BRF_ESS            },

	{ "ex2-05m.3h",        0x800000, 0x78726b17, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-06m.4h",        0x800000, 0xbe1075ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-07m.5h",        0x800000, 0x6496c6ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-08m.2k",        0x800000, 0x3194132e, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-09m.3k",        0x400000, 0x075ae585, 3 | BRF_PRG | BRF_ESS            },

	{ "ex2_02.2e",         0x020000, 0x9489875e, 4 | BRF_PRG | BRF_ESS            },

	{ "ex2-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp08",              0x000008, 0xa63d6fa6, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2u1)

STD_ROM_FN(sfex2u1)

static const ZnRomLoad sfex2u1Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  4, 0x0,       0x400000, 1 },
	{ 10, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2u1Init()
{
	return DrvInit(sfex2u1Loads, 11, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2u1 = {
	"sfex2u1", "sfex2", "coh3002c", NULL, "1998",
	"Street Fighter EX2 (USA 980312)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2u1RomInfo, sfex2u1RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2u1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 (Asia 980312)
static struct BurnRomInfo sfex2aRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ex2a_04.2h",        0x080000, 0xac9a872d, 2 | BRF_PRG | BRF_ESS            },

	{ "ex2-05m.3h",        0x800000, 0x78726b17, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-06m.4h",        0x800000, 0xbe1075ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-07m.5h",        0x800000, 0x6496c6ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-08m.2k",        0x800000, 0x3194132e, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-09m.3k",        0x400000, 0x075ae585, 3 | BRF_PRG | BRF_ESS            },

	{ "ex2_02.2e",         0x020000, 0x9489875e, 4 | BRF_PRG | BRF_ESS            },

	{ "ex2-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp08",              0x000008, 0xa63d6fa6, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2a)

STD_ROM_FN(sfex2a)

static const ZnRomLoad sfex2aLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  4, 0x0,       0x400000, 1 },
	{ 10, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2aInit()
{
	return DrvInit(sfex2aLoads, 11, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2a = {
	"sfex2a", "sfex2", "coh3002c", NULL, "1998",
	"Street Fighter EX2 (Asia 980312)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2aRomInfo, sfex2aRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 (Hispanic 980312)
static struct BurnRomInfo sfex2hRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ex2h_04.2h",        0x080000, 0x68f2ef80, 2 | BRF_PRG | BRF_ESS            },

	{ "ex2-05m.3h",        0x800000, 0x78726b17, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-06m.4h",        0x800000, 0xbe1075ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-07m.5h",        0x800000, 0x6496c6ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-08m.2k",        0x800000, 0x3194132e, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-09m.3k",        0x400000, 0x075ae585, 3 | BRF_PRG | BRF_ESS            },

	{ "ex2_02.2e",         0x020000, 0x9489875e, 4 | BRF_PRG | BRF_ESS            },

	{ "ex2-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp08",              0x000008, 0xa63d6fa6, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2h)

STD_ROM_FN(sfex2h)

static const ZnRomLoad sfex2hLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  4, 0x0,       0x400000, 1 },
	{ 10, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2hInit()
{
	return DrvInit(sfex2hLoads, 11, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2h = {
	"sfex2h", "sfex2", "coh3002c", NULL, "1998",
	"Street Fighter EX2 (Hispanic 980312)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2hRomInfo, sfex2hRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2hInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 (Japan 980312)
static struct BurnRomInfo sfex2jRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "ex2j_04.2h",        0x080000, 0x5d603586, 2 | BRF_PRG | BRF_ESS            },

	{ "ex2-05m.3h",        0x800000, 0x78726b17, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-06m.4h",        0x800000, 0xbe1075ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-07m.5h",        0x800000, 0x6496c6ed, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-08m.2k",        0x800000, 0x3194132e, 3 | BRF_PRG | BRF_ESS            },
	{ "ex2-09m.3k",        0x400000, 0x075ae585, 3 | BRF_PRG | BRF_ESS            },

	{ "ex2_02.2e",         0x020000, 0x9489875e, 4 | BRF_PRG | BRF_ESS            },

	{ "ex2-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp08",              0x000008, 0xa63d6fa6, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2j)

STD_ROM_FN(sfex2j)

static const ZnRomLoad sfex2jLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  4, 0x0,       0x400000, 1 },
	{ 10, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2jInit()
{
	return DrvInit(sfex2jLoads, 11, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2j = {
	"sfex2j", "sfex2", "coh3002c", NULL, "1998",
	"Street Fighter EX2 (Japan 980312)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2jRomInfo, sfex2jRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Plasma Sword: Nightmare of Bilstein (USA 980316)
static struct BurnRomInfo plsmaswdRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sg2u_04.2h",        0x080000, 0x154187c0, 2 | BRF_PRG | BRF_ESS            },

	{ "sg2-05m.3h",        0x800000, 0xf1759236, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-06m.4h",        0x800000, 0x33de4f72, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-07m.5h",        0x800000, 0x72f724ba, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-08m.2k",        0x800000, 0x9e169eee, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-09m.3k",        0x400000, 0x33f73d4c, 3 | BRF_PRG | BRF_ESS            },

	{ "sg2_02.2e",         0x020000, 0x415ee138, 4 | BRF_PRG | BRF_ESS            },
	{ "sg2_03.3e",         0x020000, 0x43806735, 4 | BRF_PRG | BRF_ESS            },

	{ "sg2-01m.3a",        0x400000, 0x643ea27b, 5 | BRF_SND                      },

	{ "cp07",              0x000008, 0x4c1acdc2, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(plsmaswd)

STD_ROM_FN(plsmaswd)

static const ZnRomLoad plsmaswdLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  3, 0x20000,   0x20000,  0 },
	{ 10, 4, 0x0,       0x400000, 1 },
	{ 11, 6, 0x0,       0x8,      0 },
};

static INT32 plsmaswdInit()
{
	return DrvInit(plsmaswdLoads, 12, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvplsmaswd = {
	"plsmaswd", NULL, "coh3002c", NULL, "1998",
	"Plasma Sword: Nightmare of Bilstein (USA 980316)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, plsmaswdRomInfo, plsmaswdRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	plsmaswdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Plasma Sword: Nightmare of Bilstein (Asia 980316)
static struct BurnRomInfo plsmaswdaRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sg2a_04.2h",        0x080000, 0x66e5dada, 2 | BRF_PRG | BRF_ESS            },

	{ "sg2-05m.3h",        0x800000, 0xf1759236, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-06m.4h",        0x800000, 0x33de4f72, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-07m.5h",        0x800000, 0x72f724ba, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-08m.2k",        0x800000, 0x9e169eee, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-09m.3k",        0x400000, 0x33f73d4c, 3 | BRF_PRG | BRF_ESS            },

	{ "sg2_02.2e",         0x020000, 0x415ee138, 4 | BRF_PRG | BRF_ESS            },
	{ "sg2_03.3e",         0x020000, 0x43806735, 4 | BRF_PRG | BRF_ESS            },

	{ "sg2-01m.3a",        0x400000, 0x643ea27b, 5 | BRF_SND                      },

	{ "cp07",              0x000008, 0x4c1acdc2, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(plsmaswda)

STD_ROM_FN(plsmaswda)

static const ZnRomLoad plsmaswdaLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  3, 0x20000,   0x20000,  0 },
	{ 10, 4, 0x0,       0x400000, 1 },
	{ 11, 6, 0x0,       0x8,      0 },
};

static INT32 plsmaswdaInit()
{
	return DrvInit(plsmaswdaLoads, 12, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvplsmaswda = {
	"plsmaswda", "plsmaswd", "coh3002c", NULL, "1998",
	"Plasma Sword: Nightmare of Bilstein (Asia 980316)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, plsmaswdaRomInfo, plsmaswdaRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	plsmaswdaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Star Gladiator 2: Nightmare of Bilstein (Japan 980316)
static struct BurnRomInfo stargld2RomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "sg2j_04.2h",        0x080000, 0xcf4ce6ac, 2 | BRF_PRG | BRF_ESS            },

	{ "sg2-05m.3h",        0x800000, 0xf1759236, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-06m.4h",        0x800000, 0x33de4f72, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-07m.5h",        0x800000, 0x72f724ba, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-08m.2k",        0x800000, 0x9e169eee, 3 | BRF_PRG | BRF_ESS            },
	{ "sg2-09m.3k",        0x400000, 0x33f73d4c, 3 | BRF_PRG | BRF_ESS            },

	{ "sg2_02.2e",         0x020000, 0x415ee138, 4 | BRF_PRG | BRF_ESS            },
	{ "sg2_03.3e",         0x020000, 0x43806735, 4 | BRF_PRG | BRF_ESS            },

	{ "sg2-01m.3a",        0x400000, 0x643ea27b, 5 | BRF_SND                      },

	{ "cp07",              0x000008, 0x4c1acdc2, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(stargld2)

STD_ROM_FN(stargld2)

static const ZnRomLoad stargld2Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x400000, 0 },
	{ 8,  3, 0x0,       0x20000,  0 },
	{ 9,  3, 0x20000,   0x20000,  0 },
	{ 10, 4, 0x0,       0x400000, 1 },
	{ 11, 6, 0x0,       0x8,      0 },
};

static INT32 stargld2Init()
{
	return DrvInit(stargld2Loads, 12, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvstargld2 = {
	"stargld2", "plsmaswd", "coh3002c", NULL, "1998",
	"Star Gladiator 2: Nightmare of Bilstein (Japan 980316)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, stargld2RomInfo, stargld2RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	stargld2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Tetris: The Grand Master (Japan 980710)
static struct BurnRomInfo tgmjRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "atej_04.2h",        0x080000, 0xbb4bbb96, 2 | BRF_PRG | BRF_ESS            },

	{ "ate-05m.3h",        0x400000, 0x50977f5a, 3 | BRF_PRG | BRF_ESS            },
	{ "ate-06m.4h",        0x400000, 0x05973f16, 3 | BRF_PRG | BRF_ESS            },

	{ "ate_02.2e",         0x020000, 0xf4f6e82f, 4 | BRF_PRG | BRF_ESS            },

	{ "ate-01m.3a",        0x400000, 0xa21c6521, 5 | BRF_SND                      },

	{ "cp11",              0x000008, 0x22d77c59, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(tgmj)

STD_ROM_FN(tgmj)

static const ZnRomLoad tgmjLoads[] = {
	{ 0, 0, 0x0,      0x80000,  0 },
	{ 1, 5, 0x0,      0x8,      0 },
	{ 2, 1, 0x0,      0x80000,  0 },
	{ 3, 2, 0x0,      0x400000, 0 },
	{ 4, 2, 0x400000, 0x400000, 0 },
	{ 5, 3, 0x0,      0x20000,  0 },
	{ 6, 4, 0x0,      0x400000, 1 },
	{ 7, 6, 0x0,      0x8,      0 },
};

static INT32 tgmjInit()
{
	return DrvInit(tgmjLoads, 8, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvtgmj = {
	"tgmj", NULL, "coh3002c", NULL, "1998",
	"Tetris: The Grand Master (Japan 980710)\0", NULL, "Arika / Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, tgmjRomInfo, tgmjRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	tgmjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Tech Romancer (Europe 980914)
static struct BurnRomInfo techromnRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "kioe_04.2h",        0x080000, 0xebd33b09, 2 | BRF_PRG | BRF_ESS            },

	{ "kio-05m.3h",        0x800000, 0x98e9eb24, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-06m.4h",        0x800000, 0xbe8d7d73, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-07m.5h",        0x800000, 0xffd81f18, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-08m.2k",        0x800000, 0x17302226, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-09m.3k",        0x800000, 0xa34f2119, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-10m.4k",        0x800000, 0x7400037a, 3 | BRF_PRG | BRF_ESS            },

	{ "kio_02.2e",         0x020000, 0x174309b3, 4 | BRF_PRG | BRF_ESS            },
	{ "kio_03.3e",         0x020000, 0x0b313ae5, 4 | BRF_PRG | BRF_ESS            },

	{ "kio-01m.3a",        0x400000, 0x6dc5bd07, 5 | BRF_SND                      },

	{ "cp09",              0x000008, 0x02939f83, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(techromn)

STD_ROM_FN(techromn)

static const ZnRomLoad techromnLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 techromnInit()
{
	return DrvInit(techromnLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvtechromn = {
	"techromn", NULL, "coh3002c", NULL, "1998",
	"Tech Romancer (Europe 980914)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, techromnRomInfo, techromnRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	techromnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Tech Romancer (USA 980914)
static struct BurnRomInfo techromnuRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "kiou_04.2h",        0x080000, 0x08aca34a, 2 | BRF_PRG | BRF_ESS            },

	{ "kio-05m.3h",        0x800000, 0x98e9eb24, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-06m.4h",        0x800000, 0xbe8d7d73, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-07m.5h",        0x800000, 0xffd81f18, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-08m.2k",        0x800000, 0x17302226, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-09m.3k",        0x800000, 0xa34f2119, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-10m.4k",        0x800000, 0x7400037a, 3 | BRF_PRG | BRF_ESS            },

	{ "kio_02.2e",         0x020000, 0x174309b3, 4 | BRF_PRG | BRF_ESS            },
	{ "kio_03.3e",         0x020000, 0x0b313ae5, 4 | BRF_PRG | BRF_ESS            },

	{ "kio-01m.3a",        0x400000, 0x6dc5bd07, 5 | BRF_SND                      },

	{ "cp09",              0x000008, 0x02939f83, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(techromnu)

STD_ROM_FN(techromnu)

static const ZnRomLoad techromnuLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 techromnuInit()
{
	return DrvInit(techromnuLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvtechromnu = {
	"techromnu", "techromn", "coh3002c", NULL, "1998",
	"Tech Romancer (USA 980914)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, techromnuRomInfo, techromnuRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	techromnuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Tech Romancer (Asia 980914)
static struct BurnRomInfo techromnaRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "kioa_04.2h",        0x080000, 0x8ff43381, 2 | BRF_PRG | BRF_ESS            },

	{ "kio-05m.3h",        0x800000, 0x98e9eb24, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-06m.4h",        0x800000, 0xbe8d7d73, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-07m.5h",        0x800000, 0xffd81f18, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-08m.2k",        0x800000, 0x17302226, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-09m.3k",        0x800000, 0xa34f2119, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-10m.4k",        0x800000, 0x7400037a, 3 | BRF_PRG | BRF_ESS            },

	{ "kio_02.2e",         0x020000, 0x174309b3, 4 | BRF_PRG | BRF_ESS            },
	{ "kio_03.3e",         0x020000, 0x0b313ae5, 4 | BRF_PRG | BRF_ESS            },

	{ "kio-01m.3a",        0x400000, 0x6dc5bd07, 5 | BRF_SND                      },

	{ "cp09",              0x000008, 0x02939f83, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(techromna)

STD_ROM_FN(techromna)

static const ZnRomLoad techromnaLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 techromnaInit()
{
	return DrvInit(techromnaLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvtechromna = {
	"techromna", "techromn", "coh3002c", NULL, "1998",
	"Tech Romancer (Asia 980914)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, techromnaRomInfo, techromnaRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	techromnaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Choukou Senki Kikaioh (Japan 980914)
static struct BurnRomInfo kikaiohRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "kioj_04.2h",        0x080000, 0x3a2a3bc8, 2 | BRF_PRG | BRF_ESS            },

	{ "kio-05m.3h",        0x800000, 0x98e9eb24, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-06m.4h",        0x800000, 0xbe8d7d73, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-07m.5h",        0x800000, 0xffd81f18, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-08m.2k",        0x800000, 0x17302226, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-09m.3k",        0x800000, 0xa34f2119, 3 | BRF_PRG | BRF_ESS            },
	{ "kio-10m.4k",        0x800000, 0x7400037a, 3 | BRF_PRG | BRF_ESS            },

	{ "kio_02.2e",         0x020000, 0x174309b3, 4 | BRF_PRG | BRF_ESS            },
	{ "kio_03.3e",         0x020000, 0x0b313ae5, 4 | BRF_PRG | BRF_ESS            },

	{ "kio-01m.3a",        0x400000, 0x6dc5bd07, 5 | BRF_SND                      },

	{ "cp09",              0x000008, 0x02939f83, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(kikaioh)

STD_ROM_FN(kikaioh)

static const ZnRomLoad kikaiohLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 kikaiohInit()
{
	return DrvInit(kikaiohLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvkikaioh = {
	"kikaioh", "techromn", "coh3002c", NULL, "1998",
	"Choukou Senki Kikaioh (Japan 980914)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, kikaiohRomInfo, kikaiohRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	kikaiohInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 Plus (Europe 990611)
static struct BurnRomInfo sfex2pRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "x2pe_04.2h",        0x080000, 0x2cbd44ff, 2 | BRF_PRG | BRF_ESS            },

	{ "x2p-05m.3h",        0x800000, 0x4ee3110f, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-06m.4h",        0x800000, 0x4cd53a45, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-07m.5h",        0x800000, 0x11207c2a, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-08m.2k",        0x800000, 0x3560c2cc, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-09m.3k",        0x800000, 0x344aa227, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-10m.4k",        0x800000, 0x2eef5931, 3 | BRF_PRG | BRF_ESS            },

	{ "x2p_02.2e",         0x020000, 0x3705de5e, 4 | BRF_PRG | BRF_ESS            },
	{ "x2p_03.3e",         0x020000, 0x6ae828f6, 4 | BRF_PRG | BRF_ESS            },

	{ "x2p-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp12",              0x000008, 0x7cc2ed68, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2p)

STD_ROM_FN(sfex2p)

static const ZnRomLoad sfex2pLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2pInit()
{
	return DrvInit(sfex2pLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2p = {
	"sfex2p", NULL, "coh3002c", NULL, "1999",
	"Street Fighter EX2 Plus (Europe 990611)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2pRomInfo, sfex2pRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 Plus (USA 990611)
static struct BurnRomInfo sfex2puRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "x2pu_04.2h",        0x080000, 0x2938118c, 2 | BRF_PRG | BRF_ESS            },

	{ "x2p-05m.3h",        0x800000, 0x4ee3110f, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-06m.4h",        0x800000, 0x4cd53a45, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-07m.5h",        0x800000, 0x11207c2a, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-08m.2k",        0x800000, 0x3560c2cc, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-09m.3k",        0x800000, 0x344aa227, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-10m.4k",        0x800000, 0x2eef5931, 3 | BRF_PRG | BRF_ESS            },

	{ "x2p_02.2e",         0x020000, 0x3705de5e, 4 | BRF_PRG | BRF_ESS            },
	{ "x2p_03.3e",         0x020000, 0x6ae828f6, 4 | BRF_PRG | BRF_ESS            },

	{ "x2p-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp12",              0x000008, 0x7cc2ed68, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2pu)

STD_ROM_FN(sfex2pu)

static const ZnRomLoad sfex2puLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2puInit()
{
	return DrvInit(sfex2puLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2pu = {
	"sfex2pu", "sfex2p", "coh3002c", NULL, "1999",
	"Street Fighter EX2 Plus (USA 990611)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2puRomInfo, sfex2puRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2puInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 Plus (Asia 990611)
static struct BurnRomInfo sfex2paRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "x2pa_04.2h",        0x080000, 0xc437d602, 2 | BRF_PRG | BRF_ESS            },

	{ "x2p-05m.3h",        0x800000, 0x4ee3110f, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-06m.4h",        0x800000, 0x4cd53a45, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-07m.5h",        0x800000, 0x11207c2a, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-08m.2k",        0x800000, 0x3560c2cc, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-09m.3k",        0x800000, 0x344aa227, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-10m.4k",        0x800000, 0x2eef5931, 3 | BRF_PRG | BRF_ESS            },

	{ "x2p_02.2e",         0x020000, 0x3705de5e, 4 | BRF_PRG | BRF_ESS            },
	{ "x2p_03.3e",         0x020000, 0x6ae828f6, 4 | BRF_PRG | BRF_ESS            },

	{ "x2p-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp12",              0x000008, 0x7cc2ed68, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2pa)

STD_ROM_FN(sfex2pa)

static const ZnRomLoad sfex2paLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2paInit()
{
	return DrvInit(sfex2paLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2pa = {
	"sfex2pa", "sfex2p", "coh3002c", NULL, "1999",
	"Street Fighter EX2 Plus (Asia 990611)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2paRomInfo, sfex2paRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2paInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 Plus (Hispanic 990611)
static struct BurnRomInfo sfex2phRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "x2ph_04.2h",        0x080000, 0xa4f07439, 2 | BRF_PRG | BRF_ESS            },

	{ "x2p-05m.3h",        0x800000, 0x4ee3110f, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-06m.4h",        0x800000, 0x4cd53a45, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-07m.5h",        0x800000, 0x11207c2a, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-08m.2k",        0x800000, 0x3560c2cc, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-09m.3k",        0x800000, 0x344aa227, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-10m.4k",        0x800000, 0x2eef5931, 3 | BRF_PRG | BRF_ESS            },

	{ "x2p_02.2e",         0x020000, 0x3705de5e, 4 | BRF_PRG | BRF_ESS            },
	{ "x2p_03.3e",         0x020000, 0x6ae828f6, 4 | BRF_PRG | BRF_ESS            },

	{ "x2p-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp12",              0x000008, 0x7cc2ed68, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2ph)

STD_ROM_FN(sfex2ph)

static const ZnRomLoad sfex2phLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2phInit()
{
	return DrvInit(sfex2phLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2ph = {
	"sfex2ph", "sfex2p", "coh3002c", NULL, "1999",
	"Street Fighter EX2 Plus (Hispanic 990611)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2phRomInfo, sfex2phRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2phInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Street Fighter EX2 Plus (Japan 990611)
static struct BurnRomInfo sfex2pjRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "x2pj_04.2h",        0x080000, 0xc6d0aea3, 2 | BRF_PRG | BRF_ESS            },

	{ "x2p-05m.3h",        0x800000, 0x4ee3110f, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-06m.4h",        0x800000, 0x4cd53a45, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-07m.5h",        0x800000, 0x11207c2a, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-08m.2k",        0x800000, 0x3560c2cc, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-09m.3k",        0x800000, 0x344aa227, 3 | BRF_PRG | BRF_ESS            },
	{ "x2p-10m.4k",        0x800000, 0x2eef5931, 3 | BRF_PRG | BRF_ESS            },

	{ "x2p_02.2e",         0x020000, 0x3705de5e, 4 | BRF_PRG | BRF_ESS            },
	{ "x2p_03.3e",         0x020000, 0x6ae828f6, 4 | BRF_PRG | BRF_ESS            },

	{ "x2p-01m.3a",        0x400000, 0x14a5bb0e, 5 | BRF_SND                      },

	{ "cp12",              0x000008, 0x7cc2ed68, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(sfex2pj)

STD_ROM_FN(sfex2pj)

static const ZnRomLoad sfex2pjLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x800000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 3, 0x20000,   0x20000,  0 },
	{ 11, 4, 0x0,       0x400000, 1 },
	{ 12, 6, 0x0,       0x8,      0 },
};

static INT32 sfex2pjInit()
{
	return DrvInit(sfex2pjLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvsfex2pj = {
	"sfex2pj", "sfex2p", "coh3002c", NULL, "1999",
	"Street Fighter EX2 Plus (Japan 990611)\0", NULL, "Capcom / Arika", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, sfex2pjRomInfo, sfex2pjRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	sfex2pjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Strider 2 (Europe 991213)
static struct BurnRomInfo strider2RomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "hr2e_04.2h",        0x080000, 0x32d89b1c, 2 | BRF_PRG | BRF_ESS            },

	{ "hr2-05m.3h",        0x800000, 0x18716fe8, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-06m.4h",        0x800000, 0x6f13b69c, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-07m.5h",        0x800000, 0x3925701b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-08m.2k",        0x800000, 0xd844c0dc, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-09m.3k",        0x800000, 0xcdd43e6b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-10m.4k",        0x400000, 0xd95b3f37, 3 | BRF_PRG | BRF_ESS            },

	{ "hr2_02.2e",         0x020000, 0xacd8d385, 4 | BRF_PRG | BRF_ESS            },

	{ "hr2-01m.3a",        0x200000, 0x510a16d1, 5 | BRF_SND                      },

	{ "cp13",              0x000008, 0x606edf50, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(strider2)

STD_ROM_FN(strider2)

static const ZnRomLoad strider2Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x400000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 4, 0x0,       0x200000, 1 },
	{ 10, 4, 0x200000,  0x200000, 1 }, // MAME ROM_RELOAD: upper sample-ROM mirror.
	{ 11, 6, 0x0,       0x8,      0 },
};

static INT32 strider2Init()
{
	return DrvInit(strider2Loads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvstrider2 = {
	"strider2", NULL, "coh3002c", NULL, "1999",
	"Strider 2 (Europe 991213)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, strider2RomInfo, strider2RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	strider2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Strider 2 (USA 991213)
static struct BurnRomInfo strider2uRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "hr2u_04.2h",        0x080000, 0xb28b01c6, 2 | BRF_PRG | BRF_ESS            },

	{ "hr2-05m.3h",        0x800000, 0x18716fe8, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-06m.4h",        0x800000, 0x6f13b69c, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-07m.5h",        0x800000, 0x3925701b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-08m.2k",        0x800000, 0xd844c0dc, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-09m.3k",        0x800000, 0xcdd43e6b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-10m.4k",        0x400000, 0xd95b3f37, 3 | BRF_PRG | BRF_ESS            },

	{ "hr2_02.2e",         0x020000, 0xacd8d385, 4 | BRF_PRG | BRF_ESS            },

	{ "hr2-01m.3a",        0x200000, 0x510a16d1, 5 | BRF_SND                      },

	{ "cp13",              0x000008, 0x606edf50, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(strider2u)

STD_ROM_FN(strider2u)

static const ZnRomLoad strider2uLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x400000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 4, 0x0,       0x200000, 1 },
	{ 10, 4, 0x200000,  0x200000, 1 }, // MAME ROM_RELOAD: upper sample-ROM mirror.
	{ 11, 6, 0x0,       0x8,      0 },
};

static INT32 strider2uInit()
{
	return DrvInit(strider2uLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvstrider2u = {
	"strider2u", "strider2", "coh3002c", NULL, "1999",
	"Strider 2 (USA 991213)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, strider2uRomInfo, strider2uRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	strider2uInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Strider 2 (Asia 991213)
static struct BurnRomInfo strider2aRomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "hr2a_04.2h",        0x080000, 0x56ff9394, 2 | BRF_PRG | BRF_ESS            },

	{ "hr2-05m.3h",        0x800000, 0x18716fe8, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-06m.4h",        0x800000, 0x6f13b69c, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-07m.5h",        0x800000, 0x3925701b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-08m.2k",        0x800000, 0xd844c0dc, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-09m.3k",        0x800000, 0xcdd43e6b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-10m.4k",        0x400000, 0xd95b3f37, 3 | BRF_PRG | BRF_ESS            },

	{ "hr2_02.2e",         0x020000, 0xacd8d385, 4 | BRF_PRG | BRF_ESS            },

	{ "hr2-01m.3a",        0x200000, 0x510a16d1, 5 | BRF_SND                      },

	{ "cp13",              0x000008, 0x606edf50, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(strider2a)

STD_ROM_FN(strider2a)

static const ZnRomLoad strider2aLoads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x400000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 4, 0x0,       0x200000, 1 },
	{ 10, 4, 0x200000,  0x200000, 1 }, // MAME ROM_RELOAD: upper sample-ROM mirror.
	{ 11, 6, 0x0,       0x8,      0 },
};

static INT32 strider2aInit()
{
	return DrvInit(strider2aLoads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvstrider2a = {
	"strider2a", "strider2", "coh3002c", NULL, "1999",
	"Strider 2 (Asia 991213)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, strider2aRomInfo, strider2aRomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	strider2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};

// Strider Hiryu 2 (Japan 991213)
static struct BurnRomInfo shiryu2RomDesc[] = {
	{ "m534002c-59.ic353", 0x080000, 0xe860ea8b, 1 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "cp10.ic652",        0x000008, 0x7808fcef, 6 | BRF_PRG | BRF_ESS | BRF_BIOS },

	{ "hr2j_04.2h",        0x080000, 0x0824ee5f, 2 | BRF_PRG | BRF_ESS            },

	{ "hr2-05m.3h",        0x800000, 0x18716fe8, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-06m.4h",        0x800000, 0x6f13b69c, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-07m.5h",        0x800000, 0x3925701b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-08m.2k",        0x800000, 0xd844c0dc, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-09m.3k",        0x800000, 0xcdd43e6b, 3 | BRF_PRG | BRF_ESS            },
	{ "hr2-10m.4k",        0x400000, 0xd95b3f37, 3 | BRF_PRG | BRF_ESS            },

	{ "hr2_02.2e",         0x020000, 0xacd8d385, 4 | BRF_PRG | BRF_ESS            },

	{ "hr2-01m.3a",        0x200000, 0x510a16d1, 5 | BRF_SND                      },

	{ "cp13",              0x000008, 0x606edf50, 7 | BRF_PRG | BRF_ESS            },
};

STD_ROM_PICK(shiryu2)

STD_ROM_FN(shiryu2)

static const ZnRomLoad shiryu2Loads[] = {
	{ 0,  0, 0x0,       0x80000,  0 },
	{ 1,  5, 0x0,       0x8,      0 },
	{ 2,  1, 0x0,       0x80000,  0 },
	{ 3,  2, 0x0,       0x800000, 0 },
	{ 4,  2, 0x800000,  0x800000, 0 },
	{ 5,  2, 0x1000000, 0x800000, 0 },
	{ 6,  2, 0x1800000, 0x800000, 0 },
	{ 7,  2, 0x2000000, 0x800000, 0 },
	{ 8,  2, 0x2800000, 0x400000, 0 },
	{ 9,  3, 0x0,       0x20000,  0 },
	{ 10, 4, 0x0,       0x200000, 1 },
	{ 10, 4, 0x200000,  0x200000, 1 }, // MAME ROM_RELOAD: upper sample-ROM mirror.
	{ 11, 6, 0x0,       0x8,      0 },
};

static INT32 shiryu2Init()
{
	return DrvInit(shiryu2Loads, 13, 2, 2, 0, 6);
}

struct BurnDriver BurnDrvshiryu2 = {
	"shiryu2", "strider2", "coh3002c", NULL, "1999",
	"Strider Hiryu 2 (Japan 991213)\0", NULL, "Capcom", "Capcom ZN-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, shiryu2RomInfo, shiryu2RomName, NULL, NULL, NULL, NULL, ZnInputInfo, ZnDIPInfo,
	shiryu2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	640, 480, 4, 3
};
