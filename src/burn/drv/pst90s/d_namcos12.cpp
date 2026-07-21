// FB Neo Namco System 12 driver module
// Based on MAME namcos12.cpp
// Generated with Codex AI (by DsNo)

#include "tiles_generic.h"
#include "psx_intf.h"
#include "c352.h"
#include "burn_gun.h"
#include "h83002/h83002.h"

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvTestSwitch;
static UINT8 DrvTestSwitchLast;
static INT16 DrvAnalog[4];

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBankROM;
static UINT8 *DrvC76ROM;
static UINT8 *DrvC352ROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSharedRAM;
static UINT8 *DrvEEPROM;
static UINT8 *DrvScratchRAM;
static UINT8 *DrvIoRAM;
static UINT16 *DrvGpuVram;
static UINT32 *DrvPalette;

static UINT8 DrvBank[8];
static UINT32 DrvBankOffset;
static UINT32 DrvDmaOffset;
static UINT16 DrvSystem12Bank;
static UINT16 DrvH8Frame;
static UINT8 DrvUseH83002;
static UINT8 DrvH8Port8;
static UINT8 DrvH8PortA;
static UINT8 DrvH8PortB;
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
static UINT8 DrvDmaResume;
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
static INT32 DrvMainRomLinear;
static INT32 DrvTekken3Inputs;
static INT32 DrvLbgrande;
static INT32 DrvToukon3;
static INT32 DrvSoulclbr;
static INT32 DrvEhrgeiz;
static INT32 DrvFgtlayer;
static INT32 DrvPtblank2;
static INT32 DrvMrdrillr;
static INT32 DrvTektagt;
static INT32 DrvTektagtRomType;
static UINT32 DrvBankRomSize = 0x2000000;
static UINT32 DrvTektagtDmaOffset;
static UINT32 DrvTektagtProtValue[2];
static INT32 DrvTektagtProtCount;
static INT32 DrvTektagtDmaPending;
static INT32 DrvBankRom64;
static INT32 DrvBankRomCompact64;
static UINT32 DrvBankRomPairStride;
static INT32 DrvUseBootDecompressHook;
static INT32 DrvGpuVpos;
static UINT16 DrvSioStatus[2];
static UINT16 DrvSioMode[2];
static UINT16 DrvSioControl[2];
static UINT16 DrvSioBaud[2];
static UINT8 DrvSioRxData[2];
static UINT8 DrvSioTxData[2];
static UINT16 DrvKeycusP1;
static UINT16 DrvKeycusP2;
static UINT16 DrvKeycusP3;
static UINT32 DrvKeycusRand;
static INT32 DrvKeycusType;
static INT32 DrvLightgunGame;
static INT32 DrvEEPROMBusy;
static UINT8 DrvEEPROMBusyData;
static UINT32 DrvEEPROMBusyUntil;
#define NAMCOS11_MDEC_DCTSIZE2 64
#define NAMCOS11_MDEC_COS_BITS 21

static UINT32 DrvMdecDecoded;
static UINT32 DrvMdecOffset;
static UINT16 DrvMdecOutput[24 * 16];
static INT32 DrvMdecQuantY[NAMCOS11_MDEC_DCTSIZE2];
static INT32 DrvMdecQuantUV[NAMCOS11_MDEC_DCTSIZE2];
static INT32 DrvMdecCos[NAMCOS11_MDEC_DCTSIZE2];
static INT32 DrvMdecCosPrecalc[NAMCOS11_MDEC_DCTSIZE2 * NAMCOS11_MDEC_DCTSIZE2];
static INT32 DrvMdecUnpacked[NAMCOS11_MDEC_DCTSIZE2 * 6 * 2];
static UINT32 DrvMdecCommand0;
static UINT32 DrvMdecAddress0;
static UINT32 DrvMdecSize0;
static UINT32 DrvMdecCommand1;
static UINT32 DrvMdecStatus1;

#define NAMCOS11_BIU_RAM 0x00000008
#define NAMCOS11_BIU_DS  0x00000080

static const INT32 NAMCOS11_PSX_CLOCK = 100000000;
// The PSX CPU core executes one cycle for every four input clocks, as in MAME.
static const INT32 NAMCOS11_PSX_CPU_CLOCK = NAMCOS11_PSX_CLOCK / 4;
static const INT32 NAMCOS11_C352_CLOCK = 25401600;

static const UINT8 Namcos11MdecZigzag[NAMCOS11_MDEC_DCTSIZE2] = {
	 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

static inline INT32 Namcos11MdecSign10(UINT16 value)
{
	return (value & 0x0200) ? (INT32)(value | 0xfffffc00) : (INT32)(value & 0x03ff);
}

static inline UINT16 Namcos11MdecReadWord(UINT32 address)
{
	return *((UINT16*)(DrvMainRAM + (address & 0x3fffff)));
}

static void Namcos11MdecCosPrecalc()
{
	INT32 *dest = DrvMdecCosPrecalc;
	for (INT32 y = 0; y < 8; y++) {
		for (INT32 x = 0; x < 8; x++) {
			for (INT32 v = 0; v < 8; v++) {
				for (INT32 u = 0; u < 8; u++) {
					*dest++ = (DrvMdecCos[u * 8 + x] * DrvMdecCos[v * 8 + y]) >> (30 - NAMCOS11_MDEC_COS_BITS);
				}
			}
		}
	}
}

static void Namcos11MdecIdct(INT32 *source, INT32 *dest)
{
	INT32 *precalc = DrvMdecCosPrecalc;
	for (INT32 yx = 0; yx < NAMCOS11_MDEC_DCTSIZE2; yx++) {
		INT32 sum[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		INT32 *data = source;
		for (INT32 vu = 0; vu < 8; vu++) {
			for (INT32 i = 0; i < 8; i++) sum[i] += data[i] * precalc[i];
			data += 8;
			precalc += 8;
		}
		*dest++ = (sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7]) >> (NAMCOS11_MDEC_COS_BITS + 2);
	}
}

static UINT32 Namcos11MdecUnpack(UINT32 address)
{
	INT32 *block = DrvMdecUnpacked;
	INT32 *quant = DrvMdecQuantUV;
	for (INT32 blockNo = 0; blockNo < 6; blockNo++) {
		INT32 unpacked[NAMCOS11_MDEC_DCTSIZE2];
		memset(unpacked, 0, sizeof(unpacked));
		if (blockNo == 2) quant = DrvMdecQuantY;
		UINT16 packed = Namcos11MdecReadWord(address);
		address += 2;
		if (packed == 0xfe00) break;
		INT32 qscale = packed >> 10;
		unpacked[0] = Namcos11MdecSign10(packed) * quant[0];
		INT32 z = 0;
		for (;;) {
			packed = Namcos11MdecReadWord(address);
			address += 2;
			if (packed == 0xfe00) break;
			z += (packed >> 10) + 1;
			if (z > 63) break;
			unpacked[Namcos11MdecZigzag[z]] = (Namcos11MdecSign10(packed) * quant[z] * qscale) / 8;
		}
		Namcos11MdecIdct(unpacked, block);
		block += NAMCOS11_MDEC_DCTSIZE2;
	}
	return address;
}

static inline UINT8 Namcos11MdecClamp8(INT32 value)
{
	value += 128;
	if (value < 0) return 0;
	if (value > 255) return 255;
	return value;
}

static inline UINT16 Namcos11MdecRgb15(INT32 r, INT32 g, INT32 b, UINT16 stp)
{
	return stp | (Namcos11MdecClamp8(r) >> 3) | ((Namcos11MdecClamp8(g) >> 3) << 5) | ((Namcos11MdecClamp8(b) >> 3) << 10);
}

static void Namcos11MdecMakeRgb15(UINT32 address, INT32 r, INT32 g, INT32 b, INT32 *y, UINT16 stp)
{
	DrvMdecOutput[(address + 0) >> 1] = Namcos11MdecRgb15(y[0] + r, y[0] + g, y[0] + b, stp);
	DrvMdecOutput[(address + 2) >> 1] = Namcos11MdecRgb15(y[1] + r, y[1] + g, y[1] + b, stp);
}

static void Namcos11MdecYuvToRgb15()
{
	INT32 *cr = DrvMdecUnpacked;
	INT32 *cb = DrvMdecUnpacked + NAMCOS11_MDEC_DCTSIZE2;
	INT32 *y = DrvMdecUnpacked + NAMCOS11_MDEC_DCTSIZE2 * 2;
	UINT16 stp = (DrvMdecCommand0 & (1U << 25)) ? 0x8000 : 0;
	UINT32 address = 0;
	for (INT32 z = 0; z < 2; z++) {
		for (INT32 yy = 0; yy < 4; yy++) {
			for (INT32 x = 0; x < 4; x++) {
				INT32 r = (1435 * *cr) >> 10;
				INT32 g = ((-731 * *cr) >> 10) + ((-351 * *cb) >> 10);
				INT32 b = (1814 * *cb) >> 10;
				Namcos11MdecMakeRgb15(address + 0, r, g, b, y, stp);
				Namcos11MdecMakeRgb15(address + 32, r, g, b, y + 8, stp);
				r = (1435 * cr[4]) >> 10;
				g = ((-731 * cr[4]) >> 10) + ((-351 * cb[4]) >> 10);
				b = (1814 * cb[4]) >> 10;
				Namcos11MdecMakeRgb15(address + 16, r, g, b, y + NAMCOS11_MDEC_DCTSIZE2, stp);
				Namcos11MdecMakeRgb15(address + 48, r, g, b, y + NAMCOS11_MDEC_DCTSIZE2 + 8, stp);
				cr++; cb++; y += 2; address += 4;
			}
			cr += 4; cb += 4; y += 8; address += 48;
		}
		y += NAMCOS11_MDEC_DCTSIZE2;
	}
	DrvMdecDecoded = 128;
}

static void Namcos11MdecMakeRgb24(UINT32 address, INT32 r, INT32 g, INT32 b, INT32 *y)
{
	UINT8 *output = (UINT8*)DrvMdecOutput;
	output[address + 0] = Namcos11MdecClamp8(y[0] + r);
	output[address + 1] = Namcos11MdecClamp8(y[0] + g);
	output[address + 2] = Namcos11MdecClamp8(y[0] + b);
	output[address + 3] = Namcos11MdecClamp8(y[1] + r);
	output[address + 4] = Namcos11MdecClamp8(y[1] + g);
	output[address + 5] = Namcos11MdecClamp8(y[1] + b);
}

static void Namcos11MdecYuvToRgb24()
{
	INT32 *cr = DrvMdecUnpacked;
	INT32 *cb = DrvMdecUnpacked + NAMCOS11_MDEC_DCTSIZE2;
	INT32 *y = DrvMdecUnpacked + NAMCOS11_MDEC_DCTSIZE2 * 2;
	UINT32 address = 0;
	for (INT32 z = 0; z < 2; z++) {
		for (INT32 yy = 0; yy < 4; yy++) {
			for (INT32 x = 0; x < 4; x++) {
				INT32 r = (1435 * *cr) >> 10;
				INT32 g = ((-731 * *cr) >> 10) + ((-351 * *cb) >> 10);
				INT32 b = (1814 * *cb) >> 10;
				Namcos11MdecMakeRgb24(address + 0, r, g, b, y);
				Namcos11MdecMakeRgb24(address + 48, r, g, b, y + 8);
				r = (1435 * cr[4]) >> 10;
				g = ((-731 * cr[4]) >> 10) + ((-351 * cb[4]) >> 10);
				b = (1814 * cb[4]) >> 10;
				Namcos11MdecMakeRgb24(address + 24, r, g, b, y + NAMCOS11_MDEC_DCTSIZE2);
				Namcos11MdecMakeRgb24(address + 72, r, g, b, y + NAMCOS11_MDEC_DCTSIZE2 + 8);
				cr++; cb++; y += 2; address += 6;
			}
			cr += 4; cb += 4; y += 8; address += 72;
		}
		y += NAMCOS11_MDEC_DCTSIZE2;
	}
	DrvMdecDecoded = 192;
}

static void Namcos11MdecReset()
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

static void Namcos11MdecDmaWrite(UINT32 address, INT32 size)
{
	address &= 0x3fffff;
	switch (DrvMdecCommand0 >> 28)
	{
		case 3:
			DrvMdecAddress0 = address;
			DrvMdecSize0 = size * 4;
			DrvMdecStatus1 |= 1U << 29;
			break;

		case 4:
			for (INT32 index = 0; size > 0; index += 4, address += 4, size--) {
				UINT32 data = *((UINT32*)(DrvMainRAM + (address & 0x3fffff)));
				for (INT32 byte = 0; byte < 4; byte++) {
					if (index + byte < 64) DrvMdecQuantY[index + byte] = (data >> (byte * 8)) & 0xff;
					else if (index + byte < 128) DrvMdecQuantUV[index + byte - 64] = (data >> (byte * 8)) & 0xff;
				}
			}
			break;

		case 6:
			for (INT32 index = 0; size > 0 && index < 64; index += 2, address += 4, size--) {
				UINT32 data = *((UINT32*)(DrvMainRAM + (address & 0x3fffff)));
				DrvMdecCos[index + 0] = (INT16)(data & 0xffff);
				DrvMdecCos[index + 1] = (INT16)(data >> 16);
			}
			Namcos11MdecCosPrecalc();
			break;
	}
}

static void Namcos11MdecDmaRead(UINT32 address, INT32 size)
{
	address &= 0x3fffff;
	if ((DrvMdecCommand0 & (1U << 29)) && DrvMdecSize0) {
		while (size > 0) {
			if (DrvMdecDecoded == 0) {
				if ((INT32)DrvMdecSize0 <= 0) {
					DrvMdecSize0 = 0;
					break;
				}
				UINT32 next = Namcos11MdecUnpack(DrvMdecAddress0);
				DrvMdecSize0 -= next - DrvMdecAddress0;
				DrvMdecAddress0 = next;
				if (DrvMdecCommand0 & (1U << 27)) Namcos11MdecYuvToRgb15();
				else Namcos11MdecYuvToRgb24();
				DrvMdecOffset = 0;
				while (DrvMdecSize0 && Namcos11MdecReadWord(DrvMdecAddress0) == 0xfe00) {
					DrvMdecAddress0 += 2;
					DrvMdecSize0 -= 2;
				}
			}

			UINT32 count = DrvMdecDecoded;
			if (count > (UINT32)size) count = size;
			memcpy(DrvMainRAM + address, (UINT8*)DrvMdecOutput + DrvMdecOffset, count * 4);
			DrvMdecDecoded -= count;
			DrvMdecOffset += count * 4;
			address = (address + count * 4) & 0x3fffff;
			size -= count;
		}
	}
	if ((INT32)DrvMdecSize0 <= 0) DrvMdecStatus1 &= ~(1U << 29);
}

static void Namcos11GpuUpdateVisibleArea()
{
	if (DrvGpuStatus & (1 << 20)) {
		DrvGpuScreenHeight = (DrvGpuStatus & (1 << 19)) ? 512 : 256;
	} else {
		DrvGpuScreenHeight = (DrvGpuStatus & (1 << 19)) ? 480 : 240;
	}

		switch ((DrvGpuStatus >> 17) & 3)
	{
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

#define PSX_RC_STOP					0x0001
#define PSX_RC_RESET				0x0004
#define PSX_RC_COUNTTARGET			0x0008
#define PSX_RC_IRQTARGET			0x0010
#define PSX_RC_IRQOVERFLOW			0x0020
#define PSX_RC_REPEAT				0x0040
#define PSX_RC_CLC					0x0100
#define PSX_RC_DIV					0x0200
#define PSX_RC_REACHEDTARGET		0x0800
#define PSX_RC_REACHEDFFFF			0x1000

#define PSX_SIO_STATUS_TX_RDY		0x0001
#define PSX_SIO_STATUS_RX_RDY		0x0002
#define PSX_SIO_STATUS_TX_EMPTY		0x0004
#define PSX_SIO_STATUS_OVERRUN		0x0010
#define PSX_SIO_STATUS_DSR			0x0080
#define PSX_SIO_STATUS_IRQ			0x0200

#define PSX_SIO_CONTROL_TX_ENA		0x0001
#define PSX_SIO_CONTROL_IACK		0x0010
#define PSX_SIO_CONTROL_RESET		0x0040
#define PSX_SIO_CONTROL_TX_IENA		0x0400
#define PSX_SIO_CONTROL_RX_IENA		0x0800
#define PSX_SIO_CONTROL_DSR_IENA	0x1000
#define PSX_SIO_CONTROL_DTR			0x2000

static UINT8 Namcos11MakePlayerInput(UINT8 *joy, UINT8 start)
{
	UINT8 data = 0xff;

	data ^= (joy[3] & 1) << 0; // right
	data ^= (joy[2] & 1) << 1; // left
	data ^= (joy[1] & 1) << 2; // down
	data ^= (joy[0] & 1) << 3; // up
	data ^= (joy[4] & 1) << 4; // button 1
	data ^= (joy[5] & 1) << 5; // button 2
	data ^= (joy[6] & 1) << 6; // button 3
	data ^= (start & 1) << 7;

	return data;
}

static UINT8 *Namcos11Scratchpad()
{
	UINT8 *scratch = PsxGetDCache();

	return scratch ? scratch : DrvScratchRAM;
}

static void Namcos11InstructionHook(UINT32 pc)
{
	if (!DrvUseBootDecompressHook) return;
	if (pc != 0x1fc201c8) return;

	UINT8 *source = DrvMainROM + 0x028000;
	UINT8 *sourceEnd = DrvMainROM + 0x080000;
	UINT8 *destination = DrvMainRAM + 0x010000;
	UINT8 *destinationEnd = DrvMainRAM + 0x400000;

	while (source < sourceEnd && destination < destinationEnd) {
		UINT32 flags = *source++;

		if (flags == 0) break;

		while (flags >= 2) {
			if (flags & 1) {
				if (source >= sourceEnd) break;
				*destination++ = *source++;
			} else {
				if (source + 1 >= sourceEnd) break;

				UINT32 code = (source[0] << 8) | source[1];
				source += 2;

				UINT32 offset = code & 0x07ff;
				UINT32 length = (code >> 11) & 0x1f;
				if (offset == 0) offset = 0x0800;
				if (length == 0) length = 0x20;

				if (destination - DrvMainRAM < (INT32)offset || destination + length > destinationEnd) {
					source = sourceEnd;
					break;
				}

				UINT8 *copy = destination - offset;
				while (length--) *destination++ = *copy++;
			}

			flags >>= 1;
		}
	}

	if (destination == DrvMainRAM + 0x13fb00 && source == DrvMainROM + 0x07ffd0) {
		memset(destination, 0, 0x14);
		PsxSetReg(2, 0x0012fb00);
		PsxSetInstructionHook(NULL);
	}
}

static INT32 MemIndex()
{
	UINT8 *Next = AllMem;

	DrvMainROM		= Next; Next += 0x0400000;
	DrvBankROM		= Next; Next += 0x4000000;
	DrvC76ROM		= Next; Next += 0x0080000;
	DrvC352ROM		= Next; Next += 0x1000000;
	DrvMainRAM		= Next; Next += 0x0400000;
	DrvSharedRAM	= Next; Next += 0x0010000;
	DrvEEPROM		= Next; Next += 0x0001000;
	DrvScratchRAM	= Next; Next += 0x0001000;
	DrvIoRAM		= Next; Next += 0x0000040;
	DrvGpuVram		= (UINT16*)Next; Next += 1024 * 1024 * sizeof(UINT16);
	DrvPalette		= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32);

	MemEnd			= Next;

	return 0;
}

static void Namcos11ResetIo()
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
	DrvDmaResume = 0;
	Namcos11MdecReset();
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
	DrvTektagtDmaOffset = 0;
	memset(DrvTektagtProtValue, 0, sizeof(DrvTektagtProtValue));
	DrvTektagtProtCount = 0;
	DrvTektagtDmaPending = 0;
	Namcos11GpuUpdateVisibleArea();
	for (INT32 i = 0; i < 2; i++) {
		DrvSioStatus[i] = PSX_SIO_STATUS_TX_EMPTY | PSX_SIO_STATUS_TX_RDY;
		DrvSioMode[i] = 0;
		DrvSioControl[i] = 0;
		DrvSioBaud[i] = 0;
		DrvSioRxData[i] = 0xff;
		DrvSioTxData[i] = 0xff;
	}
	DrvKeycusP1 = 0;
	DrvKeycusP2 = 0;
	DrvKeycusP3 = 0;
	DrvKeycusRand = 0x12345678;
	DrvEEPROMBusy = 0;
	DrvEEPROMBusyData = 0xff;
	DrvEEPROMBusyUntil = 0;
}

static void Namcos12ApplyBootState()
{
	if (DrvFgtlayer || DrvPtblank2 || DrvMrdrillr || DrvTektagt) {
		memcpy(DrvMainRAM + 0x10000, DrvMainROM + 0x20280, 12);
	}
}

static void Namcos11UpdateIrq()
{
	PsxSetIRQLine(0, (DrvIrqStatus & DrvIrqMask) ? 1 : 0);
}

static void Namcos11SioUpdateIrq(INT32 sio)
{
	UINT32 bit = sio ? 0x0100 : 0x0080;

	if (DrvSioStatus[sio] & PSX_SIO_STATUS_IRQ) {
		DrvIrqStatus |= bit;
	}

	Namcos11UpdateIrq();
}

static UINT32 Namcos11PsxTotalCycles()
{
	INT32 remaining = PsxGetRemainingCycles();

	if (remaining < 0) remaining = 0;
	if ((UINT32)remaining > DrvPsxRunSegment) remaining = DrvPsxRunSegment;

	return DrvPsxRunBaseCycles + (DrvPsxRunSegment - remaining);
}

static UINT32 Namcos11GpuFrameCycles()
{
	if (DrvGpuStatus & (1 << 20)) return NAMCOS11_PSX_CPU_CLOCK / 50;
	if (DrvGpuStatus & (1 << 19)) return 417083;

	return 417871;
}

static INT32 Namcos11GpuCurrentVpos()
{
	UINT32 frameCycles = Namcos11GpuFrameCycles();
	UINT32 elapsed = Namcos11PsxTotalCycles() % frameCycles;

	return (INT32)(((UINT64)elapsed * DrvGpuScreenHeight) / frameCycles);
}

static void Namcos11EEPROMUpdateBusy()
{
	if (DrvEEPROMBusy && Namcos11PsxTotalCycles() >= DrvEEPROMBusyUntil) {
		DrvEEPROMBusy = 0;
	}
}

static UINT8 Namcos11EEPROMRead(UINT32 address)
{
	Namcos11EEPROMUpdateBusy();

	UINT32 offset = ((address - 0x1f140000) >> 1) & 0x7ff;
	UINT8 result;

	result = DrvEEPROMBusy ? (DrvEEPROMBusyData ^ 0x80) : DrvEEPROM[offset];

	return result;
}

static void Namcos11EEPROMWrite(UINT32 address, UINT8 data)
{
	Namcos11EEPROMUpdateBusy();

	UINT32 offset = ((address - 0x1f140000) >> 1) & 0x7ff;

	if (DrvEEPROMBusy) {
		return;
	}

	if (DrvEEPROM[offset] != data) {
		DrvEEPROM[offset] = data;
		DrvEEPROMBusy = 1;
		DrvEEPROMBusyData = data;
		DrvEEPROMBusyUntil = Namcos11PsxTotalCycles() + (NAMCOS11_PSX_CPU_CLOCK / 5000);
	}
}

static UINT32 Namcos11SioRead(UINT32 address)
{
	INT32 sio = (address >= 0x1f801050) ? 1 : 0;
	UINT32 reg = ((address - (sio ? 0x1f801050 : 0x1f801040)) >> 2) & 3;
	UINT32 data = 0;

	switch (reg)
	{
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

static void Namcos11SioWrite(UINT32 address, UINT32 data, UINT32 mem_mask)
{
	INT32 sio = (address >= 0x1f801050) ? 1 : 0;
	UINT32 reg = ((address - (sio ? 0x1f801050 : 0x1f801040)) >> 2) & 3;

	switch (reg)
	{
		case 0:
			if (mem_mask & 0x000000ff) {
				DrvSioTxData[sio] = data & 0xff;
				if (sio == 0) {
					if (DrvSioStatus[sio] & PSX_SIO_STATUS_RX_RDY) {
						DrvSioStatus[sio] |= PSX_SIO_STATUS_OVERRUN;
					}
					DrvSioRxData[sio] = 0xff;
					DrvSioStatus[sio] |= PSX_SIO_STATUS_RX_RDY;
				}
				DrvSioStatus[sio] |= PSX_SIO_STATUS_TX_EMPTY | PSX_SIO_STATUS_TX_RDY;
				if (DrvSioControl[sio] & (PSX_SIO_CONTROL_TX_IENA | PSX_SIO_CONTROL_RX_IENA)) {
					DrvSioStatus[sio] |= PSX_SIO_STATUS_IRQ;
					Namcos11SioUpdateIrq(sio);
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
					Namcos11SioUpdateIrq(sio);
				}
				if (DrvSioControl[sio] & PSX_SIO_CONTROL_IACK) {
					DrvSioStatus[sio] &= ~PSX_SIO_STATUS_IRQ;
					DrvSioControl[sio] &= ~PSX_SIO_CONTROL_IACK;
					Namcos11SioUpdateIrq(sio);
				}
				if ((DrvSioControl[sio] & PSX_SIO_CONTROL_DSR_IENA) && (DrvSioStatus[sio] & PSX_SIO_STATUS_DSR)) {
					DrvSioStatus[sio] |= PSX_SIO_STATUS_IRQ;
					Namcos11SioUpdateIrq(sio);
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

static INT32 Namcos11RootDivider(INT32 counter)
{
	if (counter == 0 && (DrvRootMode[counter] & PSX_RC_CLC)) return 5;
	if (counter == 1 && (DrvRootMode[counter] & PSX_RC_CLC)) return 2150;
	if (counter == 2 && (DrvRootMode[counter] & PSX_RC_DIV)) return 8;

	return 1;
}

static UINT32 Namcos11RootCycles()
{
	return Namcos11PsxTotalCycles() * 2;
}

static UINT32 Namcos11RootTimerTicks(UINT32 ticks)
{
	return (UINT32)(((UINT64)ticks * NAMCOS11_PSX_CPU_CLOCK * 2 + 33868799) / 33868800);
}

static UINT16 Namcos11RootCurrent(INT32 counter)
{
	if (DrvRootMode[counter] & PSX_RC_STOP) {
		return DrvRootCount[counter];
	}

	UINT32 current = (Namcos11RootCycles() - DrvRootStart[counter]) / Namcos11RootDivider(counter);
	current += DrvRootCount[counter];

	if (current > 0xffff) {
		DrvRootCount[counter] = current & 0xffff;
		DrvRootStart[counter] = Namcos11RootCycles();
		DrvRootMode[counter] |= PSX_RC_REACHEDFFFF;
	}

	return current & 0xffff;
}

static INT32 Namcos11RootTarget(INT32 counter)
{
	if (DrvRootMode[counter] & (PSX_RC_COUNTTARGET | PSX_RC_IRQTARGET)) {
		return DrvRootTarget[counter];
	}

	return 0x10000;
}

static void Namcos11RootAdjust(INT32 counter)
{
	if (DrvRootMode[counter] & PSX_RC_STOP) {
		DrvRootActive[counter] = 0;
		return;
	}

	INT32 duration = Namcos11RootTarget(counter) - Namcos11RootCurrent(counter);
	if (duration < 1) {
		duration += 0x10000;
		if (DrvRootMode[counter] & PSX_RC_COUNTTARGET) {
			DrvRootMode[counter] |= PSX_RC_REACHEDTARGET;
		} else {
			DrvRootMode[counter] |= PSX_RC_REACHEDFFFF;
		}
	}

	UINT32 ticks = duration * Namcos11RootDivider(counter);
	DrvRootExpire[counter] = Namcos11RootCycles() + Namcos11RootTimerTicks(ticks);
	DrvRootActive[counter] = 1;
}

static void Namcos11RootFinish(INT32 counter)
{
	DrvRootCount[counter] = 0;
	DrvRootStart[counter] = Namcos11RootCycles();
	DrvRootActive[counter] = 0;

	if (DrvRootMode[counter] & PSX_RC_REPEAT) {
		Namcos11RootAdjust(counter);
	}

	if (DrvRootMode[counter] & (PSX_RC_IRQTARGET | PSX_RC_IRQOVERFLOW)) {
		DrvIrqStatus |= 1 << (counter + 4);
		Namcos11UpdateIrq();
	}
}

static UINT32 Namcos11RootRead(UINT32 address)
{
	INT32 counter = (address - 0x1f801100) >> 4;
	INT32 reg = (address >> 2) & 3;

	if (counter < 0 || counter >= 3) return 0;

	switch (reg)
	{
		case 0:
			return Namcos11RootCurrent(counter);

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

static void Namcos11RootWrite(UINT32 address, UINT32 data)
{
	INT32 counter = (address - 0x1f801100) >> 4;
	INT32 reg = (address >> 2) & 3;

	if (counter < 0 || counter >= 3) return;

	switch (reg)
	{
		case 0:
			DrvRootCount[counter] = data;
			DrvRootStart[counter] = Namcos11RootCycles();
			break;

		case 1:
			DrvRootCount[counter] = Namcos11RootCurrent(counter);
			DrvRootStart[counter] = Namcos11RootCycles();
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

	Namcos11RootAdjust(counter);
}

static void Namcos11RootAdvance()
{
	for (INT32 i = 0; i < 3; i++) {
		if (DrvRootActive[i] && (INT32)(Namcos11RootCycles() - DrvRootExpire[i]) >= 0) {
			Namcos11RootFinish(i);
		}
	}
}

static void Namcos12MapTektagtProtection()
{
	if (!DrvTektagt) return;

	PsxMapHandler(0, 0x1fb00000, 0x1fb00fff, MAP_READ);
	PsxMapHandler(0, 0x1fb80000, 0x1fb80fff, MAP_READ);
}

static void Namcos11MapBanks()
{
	UINT32 bank = DrvSystem12Bank;
	UINT32 offset = bank * 0x200000;

	if (offset >= DrvBankRomSize) {
		offset = 0;
	}

	PsxMapMemory(DrvBankROM + offset, 0x1fa00000, 0x1fbfffff, MAP_READ);
	Namcos12MapTektagtProtection();
}

static void Namcos11SetBank(INT32 bank, UINT16 data)
{
	if (bank < 0 || bank >= 8) return;

	if (DrvBankRom64) {
		DrvBank[bank] = ((((data & 0x00c0) >> 3) + (data & 0x0007)) ^ DrvBankOffset) & 0x1f;
	} else {
		DrvBank[bank] = ((data & 0x00c0) >> 4) + (data & 0x0003);
	}

	Namcos11MapBanks();
}

static void Namcos11MapRamConfig()
{
	UINT32 window = 0x0400000;

	PsxMapHandler(0, 0x00000000, 0x1effffff, MAP_READ | MAP_WRITE);

	UINT32 size = 0x0400000;
	if (size > window) size = window;

	for (UINT32 start = 0; start < window; start += size) {
		PsxMapMemory(DrvMainRAM, start, start + size - 1, MAP_RAM);
	}
}

static void Namcos11MapRomConfig()
{
	UINT32 window = 1 << ((DrvRomConfig >> 16) & 0x1f);
	UINT32 size = 0x0400000;

	if (window > 0x0400000) {
		window = 0x0400000;
	}

	if (size > window) {
		size = window;
	}

	PsxMapHandler(0, 0x1fc00000, 0x1fffffff, MAP_READ | MAP_WRITE);
	PsxMapHandler(0, 0x9fc00000, 0x9fffffff, MAP_READ | MAP_WRITE);
	PsxMapHandler(0, 0xbfc00000, 0xbfffffff, MAP_READ | MAP_WRITE);

	for (UINT32 start = 0; start < window; start += size) {
		PsxMapMemory(DrvMainROM, 0x1fc00000 + start, 0x1fc00000 + start + size - 1, MAP_READ);
		PsxMapMemory(DrvMainROM, 0x9fc00000 + start, 0x9fc00000 + start + size - 1, MAP_READ);
		PsxMapMemory(DrvMainROM, 0xbfc00000 + start, 0xbfc00000 + start + size - 1, MAP_READ);
	}
}

static UINT32 Namcos11ReadIo(UINT32 address)
{
	switch (address & ~3)
	{
		case 0x1f801000: return DrvExpBase;
		case 0x1f801004: return DrvControlRAM0;
		case 0x1f801008: return DrvExpConfig;
		case 0x1f80100c: return DrvMemControl;
		case 0x1f801010: return DrvRomConfig;
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
			return Namcos11SioRead(address & ~3);
		case 0x1f801060: return DrvRamConfig;
		case 0x1f801064:
		case 0x1f801068:
		case 0x1f80106c:
			return DrvControlRAM1[((address & ~3) - 0x1f801064) >> 2];
		case 0x1f801070: return DrvIrqStatus;
		case 0x1f801074: return DrvIrqMask;
		case 0x1f801810:
			if (DrvGpuReadCount) {
				UINT32 data = 0;
				for (INT32 i = 0; i < 2 && DrvGpuReadCount; i++) {
					UINT32 pos = (DrvGpuReadW * DrvGpuReadH) - DrvGpuReadCount;
					UINT32 x = (DrvGpuReadX + (pos % DrvGpuReadW)) & 0x3ff;
					UINT32 y = (DrvGpuReadY + (pos / DrvGpuReadW)) & 0x3ff;
					data |= (UINT32)DrvGpuVram[(y << 10) | x] << (i * 16);
					DrvGpuReadCount--;
				}
				if (DrvGpuReadCount == 0) DrvGpuStatus &= ~(1 << 27);
				return data;
			}
			return DrvGpuInfo;
		case 0x1f801814:
		{
			DrvGpuVpos = Namcos11GpuCurrentVpos();
			UINT32 data = DrvGpuStatus | ((((DrvGpuStatus & (1 << 22)) && (DrvGpuStatus & (1 << 13))) || (!(DrvGpuStatus & (1 << 22)) && (DrvGpuVpos & 1))) ? (1u << 31) : 0);
			return data;
		}
		case 0x1f801820:
			return 0;
		case 0x1f801824:
			return DrvMdecStatus1;
		case 0xfffe0130: return PsxGetBIU();
	}

	if (address >= 0x1f801080 && address <= 0x1f8010ff) {
		UINT32 offset = (address - 0x1f801080) >> 2;
		UINT32 channel = offset >> 2;

		if (channel < 7) {
			switch (offset & 3)
			{
				case 0: return DrvDmaBase[channel];
				case 1: return DrvDmaBlock[channel];
				case 2:
				case 3: return DrvDmaControl[channel];
			}
		} else {
			switch (offset & 3)
			{
				case 0: return DrvDmaDpcr;
				case 1: return DrvDmaDicr;
			}
		}
	}

	if (address >= 0x1f801100 && address <= 0x1f80112f) {
		return Namcos11RootRead(address);
	}

	return 0;
}

static void Namcos11DmaInterruptUpdate()
{
	INT32 n_int = (DrvDmaDicr >> 24) & 0x7f;
	INT32 n_mask = (DrvDmaDicr >> 16) & 0xff;

	if ((n_mask & 0x80) && (n_int & n_mask)) {
		DrvDmaDicr |= 0x80000000;
		DrvIrqStatus |= 0x0008;
	}

	DrvDmaDicr &= 0x00ffffff | (DrvDmaDicr << 8);
	Namcos11UpdateIrq();
}

static INT32 Namcos11GpuPacketLen(UINT8 command)
{
	if (command == 0x00 || command == 0x01) return 1;
	if (command == 0x02) return 3;
	if (command >= 0x20 && command <= 0x23) return 4;
	if (command >= 0x24 && command <= 0x27) return 7;
	if (command >= 0x28 && command <= 0x2b) return 5;
	if (command >= 0x2c && command <= 0x2f) return 9;
	if (command >= 0x30 && command <= 0x33) return 6;
	if (command >= 0x34 && command <= 0x37) return 9;
	if (command >= 0x38 && command <= 0x3b) return 8;
	if (command >= 0x3c && command <= 0x3f) return 12;
	if (command >= 0x40 && command <= 0x43) return 3;
	if (command >= 0x50 && command <= 0x53) return 4;
	if (command >= 0x60 && command <= 0x63) return 3;
	if (command >= 0x64 && command <= 0x67) return 4;
	if (command >= 0x68 && command <= 0x6b) return 2;
	if (command >= 0x6c && command <= 0x6f) return 3;
	if (command >= 0x70 && command <= 0x73) return 2;
	if (command >= 0x74 && command <= 0x77) return 3;
	if (command >= 0x78 && command <= 0x7b) return 2;
	if (command >= 0x7c && command <= 0x7f) return 3;
	if (command == 0x80) return 4;
	if (command == 0xa0 || command == 0xc0) return 3;
	if (command >= 0xe1 && command <= 0xe6) return 1;

	return 1;
}

static inline void Namcos11GpuWriteVramPixel(UINT32 x, UINT32 y, UINT16 color)
{
	UINT16 *pixel = &DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
	if (DrvGpuCheckStp && (*pixel & 0x8000)) return;
	*pixel = DrvGpuDrawStp ? (color | 0x8000) : color;
}

static void Namcos11GpuWriteImage(UINT32 data)
{
	for (INT32 i = 0; i < 2 && DrvGpuImageCount > 0; i++) {
		UINT32 pixel = (i == 0) ? (data & 0xffff) : (data >> 16);
		UINT32 pos = (DrvGpuImageH * DrvGpuImageW) - DrvGpuImageCount;
		UINT32 x = (DrvGpuImageX + (pos % DrvGpuImageW)) & 0x3ff;
		UINT32 y = (DrvGpuImageY + (pos / DrvGpuImageW)) & 0x3ff;

		Namcos11GpuWriteVramPixel(x, y, pixel);
		DrvGpuImageCount--;
	}

	if (DrvGpuImageCount == 0) {
		DrvGpuPacketPos = 0;
		DrvGpuPacketLen = 0;
	}
}

static void Namcos11GpuFillRect()
{
	UINT16 color = ((DrvGpuPacket[0] & 0x0000f8) >> 3) | ((DrvGpuPacket[0] & 0x00f800) >> 6) | ((DrvGpuPacket[0] & 0xf80000) >> 9);
	UINT32 x = DrvGpuPacket[1] & 0xffff;
	UINT32 y = DrvGpuPacket[1] >> 16;
	UINT32 w = DrvGpuPacket[2] & 0xffff;
	UINT32 h = DrvGpuPacket[2] >> 16;

	for (UINT32 yy = 0; yy < h; yy++) {
		for (UINT32 xx = 0; xx < w; xx++) {
			DrvGpuVram[((((y + yy) & 0x3ff) << 10) | ((x + xx) & 0x3ff))] = color;
		}
	}
}

static void Namcos11GpuCopyVram()
{
	UINT32 sx = DrvGpuPacket[1] & 0xffff;
	UINT32 sy = DrvGpuPacket[1] >> 16;
	UINT32 dx = DrvGpuPacket[2] & 0xffff;
	UINT32 dy = DrvGpuPacket[2] >> 16;
	UINT32 w = DrvGpuPacket[3] & 0xffff;
	UINT32 h = DrvGpuPacket[3] >> 16;

	for (UINT32 yy = 0; yy < h; yy++) {
		for (UINT32 xx = 0; xx < w; xx++) {
			UINT16 color = DrvGpuVram[(((sy + yy) & 0x3ff) << 10) | ((sx + xx) & 0x3ff)];
			Namcos11GpuWriteVramPixel(dx + xx, dy + yy, color);
		}
	}
}

static inline UINT16 Namcos11GpuShadeComponent(UINT32 component)
{
	component = (16 * component) / 128;
	return (component > 31) ? 31 : component;
}

static inline UINT16 Namcos11GpuColor(UINT32 data)
{
	return Namcos11GpuShadeComponent(data & 0xff) |
		(Namcos11GpuShadeComponent((data >> 8) & 0xff) << 5) |
		(Namcos11GpuShadeComponent((data >> 16) & 0xff) << 10);
}

static inline INT32 Namcos11GpuS11Coord(UINT32 data)
{
	data &= 0x7ff;
	return (data & 0x400) ? (INT32)data - 0x800 : (INT32)data;
}

static inline INT32 Namcos11GpuX(UINT32 data)
{
	return Namcos11GpuS11Coord(data) + DrvGpuDrawOffsetX;
}

static inline INT32 Namcos11GpuY(UINT32 data)
{
	return Namcos11GpuS11Coord(data >> 16) + DrvGpuDrawOffsetY;
}

static inline INT32 Namcos11GpuPolyX(UINT32 data)
{
	return Namcos11GpuS11Coord(data) + DrvGpuDrawOffsetX;
}

static inline INT32 Namcos11GpuPolyY(UINT32 data)
{
	return Namcos11GpuS11Coord(data >> 16) + DrvGpuDrawOffsetY;
}

static inline void Namcos11GpuPutPixel(INT32 x, INT32 y, UINT16 color)
{
	INT32 x1 = DrvGpuDrawX1;
	INT32 y1 = DrvGpuDrawY1;
	INT32 x2 = DrvGpuDrawX2;
	INT32 y2 = DrvGpuDrawY2;

	if (x < x1 || x > x2 || y < y1 || y > y2) return;

	Namcos11GpuWriteVramPixel(x, y, color);
}

static inline void Namcos11GpuPutSolidPixel(INT32 x, INT32 y, UINT16 color, INT32 semi);

static inline void Namcos11GpuGetClip(INT32 &x1, INT32 &y1, INT32 &x2, INT32 &y2)
{
	x1 = DrvGpuDrawX1;
	y1 = DrvGpuDrawY1;
	x2 = DrvGpuDrawX2;
	y2 = DrvGpuDrawY2;
}

static void Namcos11GpuDrawFlatRect(INT32 width, INT32 height)
{
	UINT16 color = Namcos11GpuColor(DrvGpuPacket[0]);
	INT32 x = Namcos11GpuX(DrvGpuPacket[1]);
	INT32 y = Namcos11GpuY(DrvGpuPacket[1]);

	if (width <= 0 || height <= 0) return;

	for (INT32 yy = 0; yy < height; yy++) {
		for (INT32 xx = 0; xx < width; xx++) {
			Namcos11GpuPutSolidPixel(x + xx, y + yy, color, DrvGpuPacket[0] & 0x02000000);
		}
	}
}

static void Namcos11GpuDrawFlatRectVariable()
{
	Namcos11GpuDrawFlatRect(DrvGpuPacket[2] & 0xffff, DrvGpuPacket[2] >> 16);
}

static void Namcos11GpuDrawDot()
{
	Namcos11GpuDrawFlatRect(1, 1);
}

static void Namcos11GpuDrawLine()
{
	INT32 x0 = Namcos11GpuS11Coord(DrvGpuPacket[1]);
	INT32 y0 = Namcos11GpuS11Coord(DrvGpuPacket[1] >> 16);
	INT32 x1 = Namcos11GpuS11Coord(DrvGpuPacket[2]);
	INT32 y1 = Namcos11GpuS11Coord(DrvGpuPacket[2] >> 16);
	INT32 dx = x1 - x0; if (dx < 0) dx = -dx;
	INT32 dy = y1 - y0; if (dy < 0) dy = -dy;
	INT32 length = (dx > dy) ? dx : dy;
	UINT16 color = Namcos11GpuColor(DrvGpuPacket[0]);
	INT32 semi = DrvGpuPacket[0] & 0x02000000;

	if (length == 0) length = 1;
	INT64 x = (INT64)x0 << 16;
	INT64 y = (INT64)y0 << 16;
	INT64 stepx = (((INT64)x1 << 16) - x) / length;
	INT64 stepy = (((INT64)y1 << 16) - y) / length;

	while (length > 0) {
		Namcos11GpuPutSolidPixel((INT32)(x >> 16) + DrvGpuDrawOffsetX,
			(INT32)(y >> 16) + DrvGpuDrawOffsetY, color, semi);
		x += stepx;
		y += stepy;
		length--;
	}
}

static void Namcos11GpuDrawGouraudLine()
{
	INT32 x0 = Namcos11GpuS11Coord(DrvGpuPacket[1]);
	INT32 y0 = Namcos11GpuS11Coord(DrvGpuPacket[1] >> 16);
	INT32 x1 = Namcos11GpuS11Coord(DrvGpuPacket[3]);
	INT32 y1 = Namcos11GpuS11Coord(DrvGpuPacket[3] >> 16);
	UINT32 c0 = DrvGpuPacket[0] & 0x00ffffff;
	UINT32 c1 = DrvGpuPacket[2] & 0x00ffffff;
	INT32 dx = x1 - x0; if (dx < 0) dx = -dx;
	INT32 dy = y1 - y0; if (dy < 0) dy = -dy;
	INT32 length = (dx > dy) ? dx : dy;
	INT32 semi = DrvGpuPacket[0] & 0x02000000;

	if (length == 0) length = 1;
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
		if (rr < 0) rr = 0; else if (rr > 0xff) rr = 0xff;
		if (gg < 0) gg = 0; else if (gg > 0xff) gg = 0xff;
		if (bb < 0) bb = 0; else if (bb > 0xff) bb = 0xff;
		Namcos11GpuPutSolidPixel((INT32)(x >> 16) + DrvGpuDrawOffsetX,
			(INT32)(y >> 16) + DrvGpuDrawOffsetY,
			Namcos11GpuShadeComponent(rr) |
			(Namcos11GpuShadeComponent(gg) << 5) |
			(Namcos11GpuShadeComponent(bb) << 10), semi);
		x += stepx;
		y += stepy;
		r += stepr;
		g += stepg;
		b += stepb;
		length--;
	}
}

static inline INT32 Namcos11GpuCullTriangle(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 x2, INT32 y2)
{
	INT32 dx01 = x0 - x1; if (dx01 < 0) dx01 = -dx01;
	INT32 dy01 = y0 - y1; if (dy01 < 0) dy01 = -dy01;
	INT32 dx12 = x1 - x2; if (dx12 < 0) dx12 = -dx12;
	INT32 dy12 = y1 - y2; if (dy12 < 0) dy12 = -dy12;
	INT32 dx20 = x2 - x0; if (dx20 < 0) dx20 = -dx20;
	INT32 dy20 = y2 - y0; if (dy20 < 0) dy20 = -dy20;

	return dx01 > 1023 || dy01 > 1023 || dx12 > 1023 || dy12 > 1023 || dx20 > 1023 || dy20 > 1023;
}

static inline UINT16 Namcos11GpuBlendColor(UINT16 tex, UINT32 shade, INT32 raw)
{
	if (raw) return tex;

	INT32 tr = tex & 0x1f;
	INT32 tg = (tex >> 5) & 0x1f;
	INT32 tb = (tex >> 10) & 0x1f;
	INT32 sr = shade & 0xff;
	INT32 sg = (shade >> 8) & 0xff;
	INT32 sb = (shade >> 16) & 0xff;

	tr = (tr * sr) / 128;
	tg = (tg * sg) / 128;
	tb = (tb * sb) / 128;
	if (tr > 0x1f) tr = 0x1f;
	if (tg > 0x1f) tg = 0x1f;
	if (tb > 0x1f) tb = 0x1f;

	return tr | (tg << 5) | (tb << 10) | (tex & 0x8000);
}

static inline UINT16 Namcos11GpuBlendSemiTransparent(UINT16 foreground, UINT16 background, UINT32 tpage)
{
	INT32 fr = foreground & 0x1f;
	INT32 fg = (foreground >> 5) & 0x1f;
	INT32 fb = (foreground >> 10) & 0x1f;
	INT32 br = background & 0x1f;
	INT32 bg = (background >> 5) & 0x1f;
	INT32 bb = (background >> 10) & 0x1f;
	INT32 abr = (tpage >> ((DrvGpuType == 2) ? 5 : 7)) & 3;

	switch (abr)
	{
		case 0: fr = (fr >> 1) + (br >> 1); fg = (fg >> 1) + (bg >> 1); fb = (fb >> 1) + (bb >> 1); break;
		case 1: fr += br; fg += bg; fb += bb; break;
		case 2: fr = br - fr; fg = bg - fg; fb = bb - fb; break;
		case 3: fr = (fr >> 2) + br; fg = (fg >> 2) + bg; fb = (fb >> 2) + bb; break;
	}

	if (fr < 0) fr = 0; if (fr > 0x1f) fr = 0x1f;
	if (fg < 0) fg = 0; if (fg > 0x1f) fg = 0x1f;
	if (fb < 0) fb = 0; if (fb > 0x1f) fb = 0x1f;

	return fr | (fg << 5) | (fb << 10);
}

static inline void Namcos11GpuPutSolidPixel(INT32 x, INT32 y, UINT16 color, INT32 semi)
{
	if (semi) {
		UINT32 tpage = DrvGpuTPage;
		UINT16 background = DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
		color = Namcos11GpuBlendSemiTransparent(color, background, tpage);
	}

	Namcos11GpuPutPixel(x, y, color);
}

static inline void Namcos11GpuPutTexturedPixel(INT32 x, INT32 y, UINT16 texel, UINT16 color, UINT32 tpage, INT32 semi)
{
	if (semi && (texel & 0x8000)) {
		UINT16 background = DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
		color = Namcos11GpuBlendSemiTransparent(color, background, tpage) | 0x8000;
	}

	Namcos11GpuPutPixel(x, y, color);
}

static inline UINT16 Namcos11GpuFetchTexture(INT32 u, INT32 v, UINT32 clut, UINT32 tpage)
{
	UINT32 texx = (tpage & 0x0f) << 6;
	UINT32 texy;
	UINT32 mode;
	UINT32 interleaved = 0;

	if (DrvGpuType == 2) {
		texy = ((tpage & 0x10) << 4) | ((tpage & 0x800) >> 2);
		mode = (tpage >> 7) & 3;
	} else {
		texy = (tpage & 0x60) << 3;
		mode = (tpage >> 9) & 3;
		interleaved = tpage & 0x2000;
	}

	if (mode == 3) return 0;

	UINT32 x = 0;
	u &= DrvGpuTextureWindowW;
	v &= DrvGpuTextureWindowH;
	texx += DrvGpuTextureWindowX >> ((mode == 0) ? 2 : ((mode == 1) ? 1 : 0));
	texy += DrvGpuTextureWindowY;
	UINT32 y = (texy + (v & 0xff)) & 0x3ff;
	UINT16 data = 0;

	if (mode == 0) {
		if (interleaved) {
			x = texx + (((u >> 2) & ~0x3c) + ((v << 2) & 0x3c));
			y = texy + ((v & ~0x0f) + ((u >> 4) & 0x0f));
		} else {
			x = texx + ((u & 0xff) >> 2);
		}
		data = DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
		data = (data >> ((u & 3) << 2)) & 0x0f;
		x = ((clut & 0x3f) << 4) + data;
		y = (clut >> 6) & 0x3ff;
		return DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
	}

	if (mode == 1) {
		if (interleaved) {
			x = texx + (((u >> 1) & ~0x78) + ((u << 2) & 0x40) + ((v << 3) & 0x38));
			y = texy + ((v & ~0x07) + ((u >> 5) & 0x07));
		} else {
			x = texx + ((u & 0xff) >> 1);
		}
		data = DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
		data = (u & 1) ? (data >> 8) : (data & 0xff);
		x = ((clut & 0x3f) << 4) + data;
		y = (clut >> 6) & 0x3ff;
		return DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
	}

	x = texx + (u & 0xff);
	data = DrvGpuVram[((y & 0x3ff) << 10) | (x & 0x3ff)];
	if (data == 0) return 0;
	return data;
}

static void Namcos11GpuDrawFlatPoly(const INT32 *px, const INT32 *py, INT32 points, UINT16 color, INT32 semi)
{
	if (DrvToukon3 && points == 4) {
		INT32 tx0[3] = { px[0], px[1], px[2] };
		INT32 ty0[3] = { py[0], py[1], py[2] };
		INT32 tx1[3] = { px[2], px[1], px[3] };
		INT32 ty1[3] = { py[2], py[1], py[3] };

		Namcos11GpuDrawFlatPoly(tx0, ty0, 3, color, semi);
		Namcos11GpuDrawFlatPoly(tx1, ty1, 3, color, semi);
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
		INT32 cull0 = Namcos11GpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2]);
		INT32 cull1 = Namcos11GpuCullTriangle(px[1], py[1], px[2], py[2], px[3], py[3]);
		if (cull0) {
			if (cull1) return;
			next = next4b;
			prev = prev4b;
			left = 1;
		} else if (cull1) {
			points = 3;
		} else {
			next = next4;
			prev = prev4;
		}
	} else if (Namcos11GpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2])) {
		return;
	}

	for (INT32 i = left + 1; i < points; i++) {
		if (py[i] < py[left] || (py[i] == py[left] && px[i] < px[left])) left = i;
	}
	INT32 right = left;
	INT32 y = py[right];
	UINT32 cx1 = 0, cx2 = 0;
	INT32 dx1 = 0, dx2 = 0;
	INT32 clipx1, clipy1, clipx2, clipy2;
	Namcos11GpuGetClip(clipx1, clipy1, clipx2, clipy2);

	for (;;) {
		if (y == py[left]) {
			while (y == py[prev[left]]) {
				left = prev[left];
				if (left == right) break;
			}
			cx1 = (UINT32)(UINT16)px[left] << 16;
			left = prev[left];
			INT32 distance = py[left] - y;
			if (distance < 1) return;
			dx1 = (INT32)(((UINT32)(UINT16)px[left] << 16) - cx1);
			dx1 /= distance;
		}

		if (y == py[right]) {
			while (y == py[next[right]]) {
				right = next[right];
				if (right == left) break;
			}
			cx2 = (UINT32)(UINT16)px[right] << 16;
			right = next[right];
			INT32 distance = py[right] - y;
			if (distance < 1) return;
			dx2 = (INT32)(((UINT32)(UINT16)px[right] << 16) - cx2);
			dx2 /= distance;
		}

		INT32 x1 = (INT16)(cx1 >> 16);
		INT32 x2 = (INT16)(cx2 >> 16);
		if (x1 > x2) {
			INT32 swap = x1;
			x1 = x2;
			x2 = swap;
		}
		INT32 distance = x2 - x1;
		INT32 drawx = x1 + DrvGpuDrawOffsetX;
		INT32 drawy = y + DrvGpuDrawOffsetY;
		if (distance > 0 && drawy >= clipy1 && drawy <= clipy2) {
			if (drawx < clipx1) {
				distance -= clipx1 - drawx;
				drawx = clipx1;
			}
			if (drawx + distance > clipx2 + 1) distance = clipx2 - drawx + 1;
			for (INT32 x = drawx; distance > 0; x++, distance--) {
				Namcos11GpuPutSolidPixel(x, drawy, color, semi);
			}
		}

		cx1 += dx1;
		cx2 += dx2;
		y++;
	}
}

static void Namcos11GpuDrawTriangleFlat(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT16 color, INT32 semi)
{
	INT32 px[3] = { x0, x1, x2 };
	INT32 py[3] = { y0, y1, y2 };

	Namcos11GpuDrawFlatPoly(px, py, 3, color, semi);
}

static void Namcos11GpuDrawGouraudPolyMame(const INT32 *px, const INT32 *py, const UINT32 *pc, INT32 points, INT32 semi)
{
	if (DrvToukon3 && points == 4) {
		INT32 tx0[3] = { px[0], px[1], px[2] };
		INT32 ty0[3] = { py[0], py[1], py[2] };
		UINT32 tc0[3] = { pc[0], pc[1], pc[2] };
		INT32 tx1[3] = { px[2], px[1], px[3] };
		INT32 ty1[3] = { py[2], py[1], py[3] };
		UINT32 tc1[3] = { pc[2], pc[1], pc[3] };

		Namcos11GpuDrawGouraudPolyMame(tx0, ty0, tc0, 3, semi);
		Namcos11GpuDrawGouraudPolyMame(tx1, ty1, tc1, 3, semi);
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
		INT32 cull0 = Namcos11GpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2]);
		INT32 cull1 = Namcos11GpuCullTriangle(px[1], py[1], px[2], py[2], px[3], py[3]);
		if (cull0) {
			if (cull1) return;
			next = next4b;
			prev = prev4b;
			left = 1;
		} else if (cull1) {
			points = 3;
		} else {
			next = next4;
			prev = prev4;
		}
	} else if (Namcos11GpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2])) {
		return;
	}

	for (INT32 i = left + 1; i < points; i++) {
		if (py[i] < py[left] || (py[i] == py[left] && px[i] < px[left])) left = i;
	}
	INT32 right = left;
	INT32 y = py[right];
	UINT32 cx1 = 0, cr1 = 0, cg1 = 0, cb1 = 0;
	UINT32 cx2 = 0, cr2 = 0, cg2 = 0, cb2 = 0;
	INT32 dx1 = 0, dr1 = 0, dg1 = 0, db1 = 0;
	INT32 dx2 = 0, dr2 = 0, dg2 = 0, db2 = 0;
	INT32 clipx1, clipy1, clipx2, clipy2;
	Namcos11GpuGetClip(clipx1, clipy1, clipx2, clipy2);

	for (;;) {
		if (y == py[left]) {
			while (y == py[prev[left]]) {
				left = prev[left];
				if (left == right) break;
			}
			cx1 = (UINT32)(UINT16)px[left] << 16;
			cr1 = (pc[left] & 0xff) << 16;
			cg1 = ((pc[left] >> 8) & 0xff) << 16;
			cb1 = ((pc[left] >> 16) & 0xff) << 16;
			left = prev[left];
			INT32 distance = py[left] - y;
			if (distance < 1) return;
			dx1 = (INT32)(((UINT32)(UINT16)px[left] << 16) - cx1);
			dr1 = (INT32)(((pc[left] & 0xff) << 16) - cr1);
			dg1 = (INT32)((((pc[left] >> 8) & 0xff) << 16) - cg1);
			db1 = (INT32)((((pc[left] >> 16) & 0xff) << 16) - cb1);
			dx1 /= distance; dr1 /= distance; dg1 /= distance; db1 /= distance;
		}

		if (y == py[right]) {
			while (y == py[next[right]]) {
				right = next[right];
				if (right == left) break;
			}
			cx2 = (UINT32)(UINT16)px[right] << 16;
			cr2 = (pc[right] & 0xff) << 16;
			cg2 = ((pc[right] >> 8) & 0xff) << 16;
			cb2 = ((pc[right] >> 16) & 0xff) << 16;
			right = next[right];
			INT32 distance = py[right] - y;
			if (distance < 1) return;
			dx2 = (INT32)(((UINT32)(UINT16)px[right] << 16) - cx2);
			dr2 = (INT32)(((pc[right] & 0xff) << 16) - cr2);
			dg2 = (INT32)((((pc[right] >> 8) & 0xff) << 16) - cg2);
			db2 = (INT32)((((pc[right] >> 16) & 0xff) << 16) - cb2);
			dx2 /= distance; dr2 /= distance; dg2 /= distance; db2 /= distance;
		}

		INT32 x1 = (INT16)(cx1 >> 16);
		INT32 x2 = (INT16)(cx2 >> 16);
		UINT32 r = cr1;
		UINT32 g = cg1;
		UINT32 b = cb1;
		if (x1 > x2) {
			INT32 tx = x1; x1 = x2; x2 = tx;
			r = cr2;
			g = cg2;
			b = cb2;
		}
		INT32 distance = x2 - x1;
		INT32 drawx = x1 + DrvGpuDrawOffsetX;
		INT32 drawy = y + DrvGpuDrawOffsetY;
		if (distance > 0 && drawy >= clipy1 && drawy <= clipy2) {
			INT32 side1 = x1 == (INT16)(cx1 >> 16);
			INT32 dr = (INT32)(side1 ? (cr2 - cr1) : (cr1 - cr2)) / distance;
			INT32 dg = (INT32)(side1 ? (cg2 - cg1) : (cg1 - cg2)) / distance;
			INT32 db = (INT32)(side1 ? (cb2 - cb1) : (cb1 - cb2)) / distance;
			if (drawx < clipx1) {
				INT32 clipped = clipx1 - drawx;
				r += dr * clipped;
				g += dg * clipped;
				b += db * clipped;
				distance -= clipped;
				drawx = clipx1;
			}
			if (drawx + distance > clipx2 + 1) distance = clipx2 - drawx + 1;
			for (INT32 x = drawx; distance > 0; x++, distance--, r += dr, g += dg, b += db) {
				INT32 rr = (r >> 16) & 0xff;
				INT32 gg = (g >> 16) & 0xff;
				INT32 bb = (b >> 16) & 0xff;
				Namcos11GpuPutSolidPixel(x, drawy, Namcos11GpuShadeComponent(rr) |
					(Namcos11GpuShadeComponent(gg) << 5) |
					(Namcos11GpuShadeComponent(bb) << 10), semi);
			}
		}

		cx1 += dx1; cr1 += dr1; cg1 += dg1; cb1 += db1;
		cx2 += dx2; cr2 += dr2; cg2 += dg2; cb2 += db2;
		y++;
	}
}

static void Namcos11GpuDrawGouraudPoly(INT32 quad)
{
	UINT32 c0 = DrvGpuPacket[0] & 0x00ffffff;
	INT32 semi = DrvGpuPacket[0] & 0x02000000;
	INT32 x0 = Namcos11GpuS11Coord(DrvGpuPacket[1]);
	INT32 y0 = Namcos11GpuS11Coord(DrvGpuPacket[1] >> 16);
	UINT32 c1 = DrvGpuPacket[2] & 0x00ffffff;
	INT32 x1 = Namcos11GpuS11Coord(DrvGpuPacket[3]);
	INT32 y1 = Namcos11GpuS11Coord(DrvGpuPacket[3] >> 16);
	UINT32 c2 = DrvGpuPacket[4] & 0x00ffffff;
	INT32 x2 = Namcos11GpuS11Coord(DrvGpuPacket[5]);
	INT32 y2 = Namcos11GpuS11Coord(DrvGpuPacket[5] >> 16);

	INT32 px[4] = { x0, x1, x2, 0 };
	INT32 py[4] = { y0, y1, y2, 0 };
	UINT32 pc[4] = { c0, c1, c2, 0 };
	if (quad) {
		pc[3] = DrvGpuPacket[6] & 0x00ffffff;
		px[3] = Namcos11GpuS11Coord(DrvGpuPacket[7]);
		py[3] = Namcos11GpuS11Coord(DrvGpuPacket[7] >> 16);
	}

	Namcos11GpuDrawGouraudPolyMame(px, py, pc, quad ? 4 : 3, semi);
}

static void Namcos11GpuDrawFlatQuad()
{
	UINT16 color = Namcos11GpuColor(DrvGpuPacket[0]);
	INT32 x0 = Namcos11GpuS11Coord(DrvGpuPacket[1]);
	INT32 y0 = Namcos11GpuS11Coord(DrvGpuPacket[1] >> 16);
	INT32 x1 = Namcos11GpuS11Coord(DrvGpuPacket[2]);
	INT32 y1 = Namcos11GpuS11Coord(DrvGpuPacket[2] >> 16);
	INT32 x2 = Namcos11GpuS11Coord(DrvGpuPacket[3]);
	INT32 y2 = Namcos11GpuS11Coord(DrvGpuPacket[3] >> 16);
	INT32 x3 = Namcos11GpuS11Coord(DrvGpuPacket[4]);
	INT32 y3 = Namcos11GpuS11Coord(DrvGpuPacket[4] >> 16);

	INT32 px[4] = { x0, x1, x2, x3 };
	INT32 py[4] = { y0, y1, y2, y3 };

	Namcos11GpuDrawFlatPoly(px, py, 4, color, DrvGpuPacket[0] & 0x02000000);
}

static void Namcos11GpuDrawTexturedPolyMame(const INT32 *px, const INT32 *py, const INT32 *pu, const INT32 *pv,
	const UINT32 *pc, INT32 points, UINT32 clut, UINT32 tpage, INT32 raw, INT32 semi, INT32 gouraud)
{
	if (DrvToukon3 && points == 4) {
		INT32 tx0[3] = { px[0], px[1], px[2] };
		INT32 ty0[3] = { py[0], py[1], py[2] };
		INT32 tu0[3] = { pu[0], pu[1], pu[2] };
		INT32 tv0[3] = { pv[0], pv[1], pv[2] };
		UINT32 tc0[3] = { pc[0], pc[1], pc[2] };
		INT32 tx1[3] = { px[2], px[1], px[3] };
		INT32 ty1[3] = { py[2], py[1], py[3] };
		INT32 tu1[3] = { pu[2], pu[1], pu[3] };
		INT32 tv1[3] = { pv[2], pv[1], pv[3] };
		UINT32 tc1[3] = { pc[2], pc[1], pc[3] };

		Namcos11GpuDrawTexturedPolyMame(tx0, ty0, tu0, tv0, tc0, 3, clut, tpage, raw, semi, gouraud);
		Namcos11GpuDrawTexturedPolyMame(tx1, ty1, tu1, tv1, tc1, 3, clut, tpage, raw, semi, gouraud);
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
		INT32 cull0 = Namcos11GpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2]);
		INT32 cull1 = Namcos11GpuCullTriangle(px[1], py[1], px[2], py[2], px[3], py[3]);
		if (cull0) {
			if (cull1) return;
			next = next4b;
			prev = prev4b;
			left = 1;
		} else if (cull1) {
			points = 3;
		} else {
			next = next4;
			prev = prev4;
		}
	} else if (Namcos11GpuCullTriangle(px[0], py[0], px[1], py[1], px[2], py[2])) {
		return;
	}

	for (INT32 i = left + 1; i < points; i++) {
		if (py[i] < py[left] || (py[i] == py[left] && px[i] < px[left])) left = i;
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
	Namcos11GpuGetClip(clipx1, clipy1, clipx2, clipy2);

	for (;;) {
		if (y == py[left]) {
			while (y == py[prev[left]]) {
				left = prev[left];
				if (left == right) break;
			}
			cx1 = (UINT32)(UINT16)px[left] << 16;
			cr1 = (raw ? 0x80 : (pc[left] & 0xff)) << 16;
			cg1 = (raw ? 0x80 : ((pc[left] >> 8) & 0xff)) << 16;
			cb1 = (raw ? 0x80 : ((pc[left] >> 16) & 0xff)) << 16;
			cu1 = (UINT32)(UINT16)pu[left] << 16;
			cv1 = (UINT32)(UINT16)pv[left] << 16;
			left = prev[left];
			INT32 distance = py[left] - y;
			if (distance < 1) return;
			dx1 = (INT32)(((UINT32)(UINT16)px[left] << 16) - cx1);
			dr1 = raw ? 0 : (INT32)(((pc[left] & 0xff) << 16) - cr1);
			dg1 = raw ? 0 : (INT32)((((pc[left] >> 8) & 0xff) << 16) - cg1);
			db1 = raw ? 0 : (INT32)((((pc[left] >> 16) & 0xff) << 16) - cb1);
			du1 = (INT32)(((UINT32)(UINT16)pu[left] << 16) - cu1);
			dv1 = (INT32)(((UINT32)(UINT16)pv[left] << 16) - cv1);
			dx1 /= distance; dr1 /= distance; dg1 /= distance; db1 /= distance;
			du1 /= distance; dv1 /= distance;
		}

		if (y == py[right]) {
			while (y == py[next[right]]) {
				right = next[right];
				if (right == left) break;
			}
			cx2 = (UINT32)(UINT16)px[right] << 16;
			cr2 = (raw ? 0x80 : (pc[right] & 0xff)) << 16;
			cg2 = (raw ? 0x80 : ((pc[right] >> 8) & 0xff)) << 16;
			cb2 = (raw ? 0x80 : ((pc[right] >> 16) & 0xff)) << 16;
			cu2 = (UINT32)(UINT16)pu[right] << 16;
			cv2 = (UINT32)(UINT16)pv[right] << 16;
			right = next[right];
			INT32 distance = py[right] - y;
			if (distance < 1) return;
			dx2 = (INT32)(((UINT32)(UINT16)px[right] << 16) - cx2);
			dr2 = raw ? 0 : (INT32)(((pc[right] & 0xff) << 16) - cr2);
			dg2 = raw ? 0 : (INT32)((((pc[right] >> 8) & 0xff) << 16) - cg2);
			db2 = raw ? 0 : (INT32)((((pc[right] >> 16) & 0xff) << 16) - cb2);
			du2 = (INT32)(((UINT32)(UINT16)pu[right] << 16) - cu2);
			dv2 = (INT32)(((UINT32)(UINT16)pv[right] << 16) - cv2);
			dx2 /= distance; dr2 /= distance; dg2 /= distance; db2 /= distance;
			du2 /= distance; dv2 /= distance;
		}

		INT32 x1 = (INT16)(cx1 >> 16);
		INT32 x2 = (INT16)(cx2 >> 16);
		UINT32 r = cr1;
		UINT32 g = cg1;
		UINT32 b = cb1;
		UINT32 u = cu1;
		UINT32 v = cv1;
		if (x1 > x2) {
			INT32 tx = x1; x1 = x2; x2 = tx;
			r = cr2;
			g = cg2;
			b = cb2;
			u = cu2;
			v = cv2;
		}
		INT32 distance = x2 - x1;
		INT32 drawy = y + DrvGpuDrawOffsetY;
		if (distance > 0 && drawy >= clipy1 && drawy <= clipy2) {
			INT32 side1 = x1 == (INT16)(cx1 >> 16);
			INT32 dr = (INT32)(side1 ? (cr2 - cr1) : (cr1 - cr2)) / distance;
			INT32 dg = (INT32)(side1 ? (cg2 - cg1) : (cg1 - cg2)) / distance;
			INT32 db = (INT32)(side1 ? (cb2 - cb1) : (cb1 - cb2)) / distance;
			INT32 du = (INT32)(side1 ? (cu2 - cu1) : (cu1 - cu2)) / distance;
			INT32 dv = (INT32)(side1 ? (cv2 - cv1) : (cv1 - cv2)) / distance;
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
			if (drawx + distance > clipx2 + 1) distance = clipx2 - drawx + 1;
			for (INT32 x = drawx; distance > 0; x++, distance--, r += dr, g += dg, b += db, u += du, v += dv) {
				UINT16 texel = Namcos11GpuFetchTexture((UINT16)(u >> 16), (UINT16)(v >> 16), clut, tpage);
				if (texel) {
					UINT32 shade = gouraud ? ((UINT32)(r >> 16) & 0xff) | (((UINT32)(g >> 16) & 0xff) << 8) | (((UINT32)(b >> 16) & 0xff) << 16) : pc[0];
					Namcos11GpuPutTexturedPixel(x, drawy, texel, Namcos11GpuBlendColor(texel, shade, raw), tpage, semi);
				}
			}
		}

		cx1 += dx1; cr1 += dr1; cg1 += dg1; cb1 += db1; cu1 += du1; cv1 += dv1;
		cx2 += dx2; cr2 += dr2; cg2 += dg2; cb2 += db2; cu2 += du2; cv2 += dv2;
		y++;
	}
}

static void Namcos11GpuDrawTexturedPoly(INT32 gouraud, INT32 quad)
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
		x0 = Namcos11GpuS11Coord(DrvGpuPacket[1]);
		y0 = Namcos11GpuS11Coord(DrvGpuPacket[1] >> 16);
		u0 = DrvGpuPacket[2] & 0xff;
		v0 = (DrvGpuPacket[2] >> 8) & 0xff;
		clut = DrvGpuPacket[2] >> 16;
		c1 = DrvGpuPacket[3] & 0x00ffffff;
		x1 = Namcos11GpuS11Coord(DrvGpuPacket[4]);
		y1 = Namcos11GpuS11Coord(DrvGpuPacket[4] >> 16);
		u1 = DrvGpuPacket[5] & 0xff;
		v1 = (DrvGpuPacket[5] >> 8) & 0xff;
		tpage = DrvGpuPacket[5] >> 16;
		c2 = DrvGpuPacket[6] & 0x00ffffff;
		x2 = Namcos11GpuS11Coord(DrvGpuPacket[7]);
		y2 = Namcos11GpuS11Coord(DrvGpuPacket[7] >> 16);
		u2 = DrvGpuPacket[8] & 0xff;
		v2 = (DrvGpuPacket[8] >> 8) & 0xff;
		if (quad) {
			c3 = DrvGpuPacket[9] & 0x00ffffff;
			x3 = Namcos11GpuS11Coord(DrvGpuPacket[10]);
			y3 = Namcos11GpuS11Coord(DrvGpuPacket[10] >> 16);
			u3 = DrvGpuPacket[11] & 0xff;
			v3 = (DrvGpuPacket[11] >> 8) & 0xff;
		}
	} else {
		x0 = Namcos11GpuS11Coord(DrvGpuPacket[1]);
		y0 = Namcos11GpuS11Coord(DrvGpuPacket[1] >> 16);
		u0 = DrvGpuPacket[2] & 0xff;
		v0 = (DrvGpuPacket[2] >> 8) & 0xff;
		clut = DrvGpuPacket[2] >> 16;
		x1 = Namcos11GpuS11Coord(DrvGpuPacket[3]);
		y1 = Namcos11GpuS11Coord(DrvGpuPacket[3] >> 16);
		u1 = DrvGpuPacket[4] & 0xff;
		v1 = (DrvGpuPacket[4] >> 8) & 0xff;
		tpage = DrvGpuPacket[4] >> 16;
		x2 = Namcos11GpuS11Coord(DrvGpuPacket[5]);
		y2 = Namcos11GpuS11Coord(DrvGpuPacket[5] >> 16);
		u2 = DrvGpuPacket[6] & 0xff;
		v2 = (DrvGpuPacket[6] >> 8) & 0xff;
		if (quad) {
			x3 = Namcos11GpuS11Coord(DrvGpuPacket[7]);
			y3 = Namcos11GpuS11Coord(DrvGpuPacket[7] >> 16);
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
	Namcos11GpuDrawTexturedPolyMame(px, py, pu, pv, pc, quad ? 4 : 3, clut, tpage, raw, semi, gouraud);
}

static void Namcos11GpuDrawTexturedRect(INT32 width, INT32 height)
{
	UINT32 color = DrvGpuPacket[0] & 0x00ffffff;
	INT32 raw = DrvGpuPacket[0] & 0x01000000;
	INT32 semi = DrvGpuPacket[0] & 0x02000000;
	INT32 x0 = Namcos11GpuPolyX(DrvGpuPacket[1]);
	INT32 y0 = Namcos11GpuPolyY(DrvGpuPacket[1]);
	INT32 u0 = DrvGpuPacket[2] & 0xff;
	INT32 v0 = (DrvGpuPacket[2] >> 8) & 0xff;
	UINT32 clut = DrvGpuPacket[2] >> 16;
	UINT32 tpage = DrvGpuTPage;

	if (width == 0 || height == 0) {
		width = DrvGpuPacket[3] & 0xffff;
		height = DrvGpuPacket[3] >> 16;
	}

	INT32 du = 1;
	INT32 dv = 1;
	if (DrvGpuType == 2) {
		if (tpage & 0x1000) {
			u0 += width - 1;
			du = -1;
		}
		if (tpage & 0x2000) {
			v0 += height - 1;
			dv = -1;
		}
	}

	for (INT32 y = 0; y < height; y++) {
		for (INT32 x = 0; x < width; x++) {
			UINT16 texel = Namcos11GpuFetchTexture(u0 + x * du, v0 + y * dv, clut, tpage);
			if (texel) {
				Namcos11GpuPutTexturedPixel(x0 + x, y0 + y, texel, Namcos11GpuBlendColor(texel, color, raw), tpage, semi);
			}
		}
	}
}

static void Namcos11GpuExecutePacket()
{
	UINT8 command = DrvGpuPacket[0] >> 24;
	switch (command)
	{
		case 0x02:
			Namcos11GpuFillRect();
			break;

		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			Namcos11GpuDrawTriangleFlat(Namcos11GpuS11Coord(DrvGpuPacket[1]), Namcos11GpuS11Coord(DrvGpuPacket[1] >> 16),
				Namcos11GpuS11Coord(DrvGpuPacket[2]), Namcos11GpuS11Coord(DrvGpuPacket[2] >> 16),
				Namcos11GpuS11Coord(DrvGpuPacket[3]), Namcos11GpuS11Coord(DrvGpuPacket[3] >> 16), Namcos11GpuColor(DrvGpuPacket[0]),
				DrvGpuPacket[0] & 0x02000000);
			break;

		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
			Namcos11GpuDrawTexturedPoly(0, 0);
			break;

		case 0x28:
		case 0x29:
		case 0x2a:
		case 0x2b:
			Namcos11GpuDrawFlatQuad();
			break;

		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
			Namcos11GpuDrawGouraudPoly(0);
			break;

		case 0x2c:
		case 0x2d:
		case 0x2e:
		case 0x2f:
			Namcos11GpuDrawTexturedPoly(0, 1);
			break;

		case 0x38:
		case 0x39:
		case 0x3a:
		case 0x3b:
			Namcos11GpuDrawGouraudPoly(1);
			break;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
			Namcos11GpuDrawLine();
			break;

		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
			Namcos11GpuDrawGouraudLine();
			break;

		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
			Namcos11GpuDrawTexturedPoly(1, 0);
			break;

		case 0x3c:
		case 0x3d:
		case 0x3e:
		case 0x3f:
			Namcos11GpuDrawTexturedPoly(1, 1);
			break;

		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
			Namcos11GpuDrawFlatRectVariable();
			break;

		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
			Namcos11GpuDrawTexturedRect(0, 0);
			break;

		case 0x68:
		case 0x69:
		case 0x6a:
		case 0x6b:
			Namcos11GpuDrawDot();
			break;

		case 0x6c:
		case 0x6d:
		case 0x6e:
		case 0x6f:
			Namcos11GpuDrawTexturedRect(1, 1);
			break;

		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
			Namcos11GpuDrawFlatRect(8, 8);
			break;

		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
			Namcos11GpuDrawTexturedRect(8, 8);
			break;

		case 0x78:
		case 0x79:
		case 0x7a:
		case 0x7b:
			Namcos11GpuDrawFlatRect(16, 16);
			break;

		case 0x7c:
		case 0x7d:
		case 0x7e:
		case 0x7f:
			Namcos11GpuDrawTexturedRect(16, 16);
			break;

		case 0x80:
		{
			Namcos11GpuCopyVram();
			break;
		}

		case 0xa0:
		{
			DrvGpuImageX = DrvGpuPacket[1] & 0xffff;
			DrvGpuImageY = DrvGpuPacket[1] >> 16;
			DrvGpuImageW = DrvGpuPacket[2] & 0xffff;
			DrvGpuImageH = DrvGpuPacket[2] >> 16;
			DrvGpuImageCount = DrvGpuImageW * DrvGpuImageH;
			return;
		}

		case 0xc0:
			DrvGpuReadX = DrvGpuPacket[1] & 0xffff;
			DrvGpuReadY = DrvGpuPacket[1] >> 16;
			DrvGpuReadW = DrvGpuPacket[2] & 0xffff;
			DrvGpuReadH = DrvGpuPacket[2] >> 16;
			DrvGpuReadCount = DrvGpuReadW * DrvGpuReadH;
			if (DrvGpuReadCount) DrvGpuStatus |= 1 << 27;
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
			DrvGpuDrawOffsetY = ((DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 11 : 12)) & 0x400) ? (INT32)((DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 11 : 12)) & 2047) - 2048 : (INT32)((DrvGpuPacket[0] >> ((DrvGpuType == 2) ? 11 : 12)) & 2047);
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

static void Namcos11GpuWrite(UINT32 data)
{
	if (DrvGpuImageCount) {
		Namcos11GpuWriteImage(data);
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

			Namcos11GpuDrawLine();
			if ((DrvGpuPacket[3] & 0xf000f000) != 0x50005000) {
				DrvGpuPacket[1] = DrvGpuPacket[2];
				DrvGpuPacket[2] = DrvGpuPacket[3];
				DrvGpuPacketPos = 3;
				return;
			}
		} else {
			if (DrvGpuPacketPos < 5 &&
				(DrvGpuPacketPos != 4 || (DrvGpuPacket[4] & 0xf000f000) != 0x50005000)) {
				DrvGpuPacketPos++;
				return;
			}

			Namcos11GpuDrawGouraudLine();
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
		if (command == 0x48 || command == 0x4a || command == 0x4c || command == 0x4e ||
			command == 0x58 || command == 0x5a || command == 0x5c || command == 0x5e) {
			DrvGpuPolyline = command;
			DrvGpuPacket[0] = data;
			DrvGpuPacketPos = 1;
			return;
		}
		DrvGpuPacketLen = Namcos11GpuPacketLen(command);
	}

	DrvGpuPacket[DrvGpuPacketPos++] = data;

	if (DrvGpuPacketPos >= DrvGpuPacketLen) {
		Namcos11GpuExecutePacket();
	}
}

static void Namcos11GpuControl(UINT32 data)
{
	switch (data >> 24)
	{
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
			Namcos11GpuUpdateVisibleArea();
			break;

		case 0x01:
			DrvGpuPacketPos = 0;
			DrvGpuPacketLen = 0;
			DrvGpuPolyline = 0;
			DrvGpuImageCount = 0;
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
			Namcos11GpuUpdateVisibleArea();
			break;

		case 0x10:
			switch (data & 0xff)
			{
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

static void Namcos11GpuDmaWrite(UINT32 address, INT32 size)
{
	UINT32 *ram = (UINT32*)DrvMainRAM;
	UINT32 mask = 0x3fffff;
	for (INT32 i = 0; i < size; i++) {
		UINT32 data = ram[((address & mask) >> 2) & 0x0fffff];
		Namcos11GpuWrite(data);
		address += 4;
	}

}

static void Namcos11DmaFinish(INT32 channel)
{
	DrvDmaControl[channel] &= ~((1 << 24) | (1 << 28));
	DrvDmaDicr |= 1 << (24 + channel);
	Namcos11DmaInterruptUpdate();
}

static void Namcos11DmaScheduleFinish(INT32 channel, UINT32 cycles)
{
	DrvDmaFinishCycle[channel] = Namcos11PsxTotalCycles() + cycles;
	DrvDmaPending |= 1 << channel;
	DrvDmaResume &= ~(1 << channel);
}

static void Namcos11DmaScheduleResume(INT32 channel, UINT32 cycles)
{
	DrvDmaFinishCycle[channel] = Namcos11PsxTotalCycles() + cycles;
	DrvDmaPending |= 1 << channel;
	DrvDmaResume |= 1 << channel;
}

static UINT32 Namcos11DmaTicksToCpuCycles(UINT32 ticks)
{
	// The PSX DMA timer runs at 33.8688 MHz in MAME.
	return (UINT32)(((UINT64)ticks * NAMCOS11_PSX_CPU_CLOCK + 33868799) / 33868800);
}

static void Namcos11DmaStart(INT32 channel);

static void Namcos11DmaAdvance()
{
	UINT32 now = Namcos11PsxTotalCycles();
	for (INT32 channel = 0; channel < 7; channel++) {
		if ((DrvDmaPending & (1 << channel)) && (INT32)(now - DrvDmaFinishCycle[channel]) >= 0) {
			DrvDmaPending &= ~(1 << channel);
			if (DrvDmaResume & (1 << channel)) {
				DrvDmaResume &= ~(1 << channel);
				Namcos11DmaStart(channel);
			} else {
				Namcos11DmaFinish(channel);
			}
		}
	}
}

static void Namcos11DmaStart(INT32 channel)
{
	UINT32 control = DrvDmaControl[channel];

	if ((control & (1 << 24)) == 0) return;
	if ((DrvDmaDpcr & (1 << (3 + (channel * 4)))) == 0) return;

	if ((channel == 0 && control == 0x01000201) || (channel == 1 && control == 0x01000200)) {
		UINT32 blocks = DrvDmaBlock[channel] >> 16;
		if (blocks == 0) blocks = 0x10000;
		INT32 size = (DrvDmaBlock[channel] & 0xffff) * blocks;
		if (channel == 0) Namcos11MdecDmaWrite(DrvDmaBase[channel], size);
		else Namcos11MdecDmaRead(DrvDmaBase[channel], size);
		if (channel == 1) Namcos11DmaScheduleFinish(channel, Namcos11DmaTicksToCpuCycles(26000));
		else Namcos11DmaFinish(channel);
		return;
	}

	if (channel == 2 && control == 0x01000401) {
		UINT32 *ram = (UINT32*)DrvMainRAM;
		UINT32 address = DrvDmaBase[channel] & 0x00ffffff;
		UINT32 mask = 0x3fffff;
		UINT32 total = 0;

		if (address != 0x00ffffff) {
			for (;;) {
				if (address == 0x00ffffff) {
					DrvDmaBase[channel] = address;
					Namcos11DmaScheduleFinish(channel, Namcos11DmaTicksToCpuCycles(500));
					return;
				}

				if (total > 65535) {
					DrvDmaBase[channel] = address;
					Namcos11DmaScheduleResume(channel, Namcos11DmaTicksToCpuCycles(16000));
					return;
				}

				address &= mask;
				UINT32 next = ram[address >> 2];
				INT32 size = next >> 24;
				Namcos11GpuDmaWrite(address + 4, size);

				UINT32 next_address = next & 0x00ffffff;
				if (next_address != 0x00ffffff) {
					UINT32 linked = next_address & mask;
					if (address == ram[linked >> 2] || address == linked) break;
				}

				address = next_address;
				total += size + 1;
			}
		}

		Namcos11DmaFinish(channel);
		return;
	}

	if (channel == 2 && control == 0x01000201) {
		INT32 size = DrvDmaBlock[channel];
		if (control & 0x200) {
			UINT32 blocks = DrvDmaBlock[channel] >> 16;
			if (blocks == 0) blocks = 0x10000;
			size = (DrvDmaBlock[channel] & 0xffff) * blocks;
		}
		Namcos11GpuDmaWrite(DrvDmaBase[channel], size);
		Namcos11DmaFinish(channel);
		return;
	}

	if (channel == 2 && control == 0x01000200) {
		UINT32 *ram = (UINT32*)DrvMainRAM;
		UINT32 address = DrvDmaBase[channel] & 0x3fffff;
		UINT32 blocks = DrvDmaBlock[channel] >> 16;
		if (blocks == 0) blocks = 0x10000;
		INT32 size = (DrvDmaBlock[channel] & 0xffff) * blocks;
		for (INT32 i = 0; i < size; i++) {
			ram[(address & 0x3fffff) >> 2] = Namcos11ReadIo(0x1f801810);
			address += 4;
		}
		Namcos11DmaFinish(channel);
		return;
	}

	if (channel == 5 && (control & 0x01000000)) {
		UINT32 address = DrvDmaBase[channel] & 0x3fffff;
		UINT32 size = DrvDmaBlock[channel] & 0xffff;
		UINT32 source = (DrvTektagt && DrvTektagtDmaPending) ? DrvTektagtDmaOffset : DrvDmaOffset;
		UINT8 *region;
		UINT32 regionSize;

		DrvTektagtDmaPending = 0;

		if (control & 0x00000200) {
			UINT32 blocks = DrvDmaBlock[channel] >> 16;
			if (blocks == 0) blocks = 0x10000;
			if (size == 0) size = 0x10000;
			size *= blocks;
		} else if (size == 0) {
			size = 0x10000;
		}

		if ((source & 0x80000000) || DrvExpBase == 0x1f300000) {
			region = DrvMainROM;
			regionSize = 0x0400000;
			source &= 0x00ffffff;
		} else {
			region = DrvBankROM;
			regionSize = DrvBankRomSize;
			source &= 0x7fffffff;
		}

		for (UINT32 i = 0; i < size; i++) {
			UINT32 sourceOffset = source + (i * 4);
			UINT32 destination = address + (i * 4);

			if (sourceOffset + 3 >= regionSize || destination + 3 >= 0x0400000) break;

			DrvMainRAM[destination + 0] = region[sourceOffset + 0];
			DrvMainRAM[destination + 1] = region[sourceOffset + 1];
			DrvMainRAM[destination + 2] = region[sourceOffset + 2];
			DrvMainRAM[destination + 3] = region[sourceOffset + 3];
		}

		Namcos11DmaFinish(channel);
		return;
	}

	if (channel == 6 && control == 0x11000002) {
		UINT32 *ram = (UINT32*)DrvMainRAM;
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

		Namcos11DmaScheduleFinish(channel, Namcos11DmaTicksToCpuCycles(2150));
	}
}

static void Namcos11WriteDma(UINT32 address, UINT32 data)
{
	UINT32 offset = (address - 0x1f801080) >> 2;
	UINT32 channel = offset >> 2;

	if (channel < 7) {
		switch (offset & 3)
		{
			case 0:
				DrvDmaBase[channel] = data;
				break;

			case 1:
				DrvDmaBlock[channel] = data;
				break;

			case 2:
			case 3:
				DrvDmaControl[channel] = data;
				Namcos11DmaStart(channel);
				break;
		}
	} else {
		switch (offset & 3)
		{
			case 0:
				DrvDmaDpcr = data;
				break;

			case 1:
				DrvDmaDicr = (DrvDmaDicr & 0x80000000) | (DrvDmaDicr & ~data & 0x7f000000) | (data & 0x00ff803f);
				if ((DrvDmaDicr & 0x80000000) && ((DrvDmaDicr & 0x7f000000) == 0)) {
					DrvDmaDicr &= ~0x80000000;
				}
				break;
		}
	}
}

static void Namcos11WriteIo(UINT32 address, UINT32 data)
{
	switch (address & ~3)
	{
		case 0x1f801040:
		case 0x1f801044:
		case 0x1f801048:
		case 0x1f80104c:
		case 0x1f801050:
		case 0x1f801054:
		case 0x1f801058:
		case 0x1f80105c:
			Namcos11SioWrite(address & ~3, data, ~0);
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
				Namcos11MapRomConfig();
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
				Namcos11MapRamConfig();
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
			Namcos11UpdateIrq();
			return;

		case 0x1f801074:
			DrvIrqMask = data;
			Namcos11UpdateIrq();
			return;

		case 0x1f801810:
			Namcos11GpuWrite(data);
			return;

		case 0x1f801814:
			Namcos11GpuControl(data);
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
		Namcos11WriteDma(address, data);
		return;
	}

	if (address >= 0x1f801100 && address <= 0x1f80112f) {
		Namcos11RootWrite(address, data);
		return;
	}
}

static void Namcos11WriteIoMasked(UINT32 address, UINT32 data, UINT32 mem_mask)
{
	address &= ~3;

	switch (address)
	{
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
			UINT32 old = Namcos11ReadIo(address);
			Namcos11WriteIo(address, (old & ~mem_mask) | (data & mem_mask));
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
			Namcos11SioWrite(address, data, mem_mask);
			return;

		case 0x1f801070:
			DrvIrqStatus = (DrvIrqStatus & ~mem_mask) | (DrvIrqStatus & DrvIrqMask & data & mem_mask);
			Namcos11UpdateIrq();
			return;

		case 0x1f801074:
			DrvIrqMask = (DrvIrqMask & ~mem_mask) | (data & mem_mask);
			Namcos11UpdateIrq();
			return;
	}

	if (address >= 0x1f801080 && address <= 0x1f8010ff) {
		UINT32 old = Namcos11ReadIo(address);
		Namcos11WriteIo(address, (old & ~mem_mask) | (data & mem_mask));
		return;
	}

	UINT32 old = Namcos11ReadIo(address);
	Namcos11WriteIo(address, (old & ~mem_mask) | (data & mem_mask));
}

static UINT16 Namcos11KeycusRead(UINT32 address)
{
	UINT32 offset = (address - 0x1fa20000) >> 1;
	UINT16 result;

	if (DrvKeycusType == 409) {
		if (offset == 7) {
			INT32 a2 = (DrvKeycusP1 - 0x01) & 0x1f;
			INT32 a3 = (0x20 - DrvKeycusP1) & 0x1f;
			INT32 r = (((DrvKeycusP2 & 0x1f) * a2) + ((DrvKeycusP3 & 0x1f) * a3)) / 0x1f;
			INT32 g = ((((DrvKeycusP2 >> 5) & 0x1f) * a2) + (((DrvKeycusP3 >> 5) & 0x1f) * a3)) / 0x1f;
			INT32 b = ((((DrvKeycusP2 >> 10) & 0x1f) * a2) + (((DrvKeycusP3 >> 10) & 0x1f) * a3)) / 0x1f;

			return r | (g << 5) | (b << 10);
		}
	} else if (DrvKeycusType == 410) {
		if (DrvKeycusP2 == 0) {
			UINT16 value = (DrvKeycusP1 == 0xfffe) ? 410 : DrvKeycusP1;

			switch (offset)
			{
				case 1: return (value / 1) % 10;
				case 2: return ((value / 100) % 10) | (((value / 1000) % 10) << 8);
				case 3: return ((value / 10000) % 10) | (((value / 10) % 10) << 8);
			}
		}
	} else if (DrvKeycusType == 411) {
		if (DrvKeycusP2 == 0x0000 && ((((DrvKeycusP1 == 0x0000 || DrvKeycusP1 == 0x0100) && DrvKeycusP3 == 0xff7f)) || DrvKeycusP1 == 0x7256)) {
			UINT16 value = (DrvKeycusP1 == 0x7256) ? DrvKeycusP3 : 411;

			switch (offset)
			{
				case 0: return ((value / 1) % 10) | (((value / 10) % 10) << 8);
				case 2: return ((value / 100) % 10) | (((value / 1000) % 10) << 8);
				case 8: return (value / 10000) % 10;
			}
		}
	} else if (DrvKeycusType == 430) {
		if (DrvKeycusP2 == 0x0000 && ((DrvKeycusP1 == 0xbfff && DrvKeycusP3 == 0x0000) || DrvKeycusP3 == 0xe296)) {
			UINT16 value = (DrvKeycusP3 == 0xe296) ? DrvKeycusP1 : 430;

			switch (offset)
			{
				case 1: return (value / 10000) % 10;
				case 4: return ((value / 100) % 10) | (((value / 1000) % 10) << 8);
				case 5: return ((value / 1) % 10) | (((value / 10) % 10) << 8);
			}
		}
	} else if (DrvKeycusType == 442) {
		if (offset == 0 && DrvKeycusP1 == 0x0020 && (DrvKeycusP2 == 0x0000 || DrvKeycusP2 == 0x0021)) return 0x0000;
		if (offset == 1 && DrvKeycusP1 == 0x0020 && (DrvKeycusP2 == 0x0020 || DrvKeycusP2 == 0x3af2)) return 0x0000;
		if (offset == 1 && DrvKeycusP1 == 0x0020 && DrvKeycusP2 == 0x0021) return 0xc442;
	} else if (DrvKeycusType == 443) {
		if (offset == 0 && DrvKeycusP1 == 0x0020 && (DrvKeycusP2 == 0x0000 || DrvKeycusP2 == 0xffff || DrvKeycusP2 == 0xffe0)) return 0x0020;
		if (offset == 1 && DrvKeycusP1 == 0x0020 && DrvKeycusP2 == 0xffdf) return 0x0000;
		if (offset == 1 && DrvKeycusP1 == 0x0020 && (DrvKeycusP2 == 0xffff || DrvKeycusP2 == 0xffe0)) return 0xc443;
	} else if (DrvKeycusType == 432) {
		if (DrvKeycusP1 == 0x0000 && ((((DrvKeycusP3 == 0x0000 || DrvKeycusP3 == 0x00dc) && DrvKeycusP2 == 0xefff)) || DrvKeycusP3 == 0x2f15)) {
			UINT16 value = (DrvKeycusP3 == 0x2f15) ? DrvKeycusP2 : 432;

			switch (offset)
			{
				case 2: return ((value / 1) % 10) | (((value / 10) % 10) << 8);
				case 4: return ((value / 100) % 10) | (((value / 1000) % 10) << 8);
				case 6: return ((value / 10000) % 10) | (((value / 100000) % 10) << 8);
			}
		}
	} else if (DrvKeycusType == 431) {
		if (DrvKeycusP2 == 0x0000 && ((((DrvKeycusP1 == 0x0000 || DrvKeycusP1 == 0xab50) && DrvKeycusP3 == 0x7fff)) || DrvKeycusP1 == 0x9e61)) {
			UINT16 value = (DrvKeycusP1 == 0x9e61) ? DrvKeycusP3 : 431;

			switch (offset)
			{
				case 0: return ((value / 1) % 10) | (((value / 10) % 10) << 8);
				case 4: return ((value / 100) % 10) | (((value / 1000) % 10) << 8);
				case 8: return (value / 10000) % 10;
			}
		}
	} else if (offset == 0 && DrvKeycusP1 == 0x1234 && DrvKeycusP2 == 0x5678 && DrvKeycusP3 == 0x000f) {
		return 0x3256;
	}

	DrvKeycusRand = (DrvKeycusRand * 1103515245) + 12345;
	result = DrvKeycusRand >> 16;

	return result;
}

static UINT16 Namcos11KeycusLatch(UINT32 offset)
{
	if (DrvKeycusType == 409) {
		switch (offset)
		{
			case 1: return DrvKeycusP1;
			case 3: return DrvKeycusP2;
			case 7: return DrvKeycusP3;
		}
	} else if (DrvKeycusType == 410) {
		switch (offset)
		{
			case 0: return DrvKeycusP1;
			case 2: return DrvKeycusP2;
		}
	} else if (DrvKeycusType == 411) {
		switch (offset)
		{
			case 2: return DrvKeycusP1;
			case 8: return DrvKeycusP2;
			case 10: return DrvKeycusP3;
		}
	} else if (DrvKeycusType == 430) {
		switch (offset)
		{
			case 0: return DrvKeycusP1;
			case 1: return DrvKeycusP2;
			case 4: return DrvKeycusP3;
		}
	} else if (DrvKeycusType == 442) {
		switch (offset)
		{
			case 0: return DrvKeycusP1;
			case 1: return DrvKeycusP2;
		}
	} else if (DrvKeycusType == 443) {
		switch (offset)
		{
			case 0: return DrvKeycusP1;
			case 1: return DrvKeycusP2;
		}
	} else if (DrvKeycusType == 432) {
		switch (offset)
		{
			case 0: return DrvKeycusP1;
			case 2: return DrvKeycusP2;
			case 6: return DrvKeycusP3;
		}
	} else if (DrvKeycusType == 431) {
		switch (offset)
		{
			case 0: return DrvKeycusP1;
			case 4: return DrvKeycusP2;
			case 12: return DrvKeycusP3;
		}
	} else {
		switch (offset)
		{
			case 1: return DrvKeycusP1;
			case 2: return DrvKeycusP2;
			case 3: return DrvKeycusP3;
		}
	}

	return 0;
}

static void Namcos11KeycusWrite(UINT32 address, UINT16 data)
{
	UINT32 offset = (address - 0x1fa20000) >> 1;

	if (DrvKeycusType == 409) {
		switch (offset)
		{
			case 1: DrvKeycusP1 = data; return;
			case 3: DrvKeycusP2 = data; return;
			case 7: DrvKeycusP3 = data; return;
		}
	} else if (DrvKeycusType == 410) {
		switch (offset)
		{
			case 0: DrvKeycusP1 = data; return;
			case 2: DrvKeycusP2 = data; return;
		}
	} else if (DrvKeycusType == 411) {
		switch (offset)
		{
			case 2: DrvKeycusP1 = data; return;
			case 8: DrvKeycusP2 = data; return;
			case 10: DrvKeycusP3 = data; return;
		}
	} else if (DrvKeycusType == 430) {
		switch (offset)
		{
			case 0: DrvKeycusP1 = data; return;
			case 1: DrvKeycusP2 = data; return;
			case 4: DrvKeycusP3 = data; return;
		}
	} else if (DrvKeycusType == 442) {
		switch (offset)
		{
			case 0: DrvKeycusP1 = data; return;
			case 1: DrvKeycusP2 = data; return;
		}
	} else if (DrvKeycusType == 443) {
		switch (offset)
		{
			case 0: DrvKeycusP1 = data; return;
			case 1: DrvKeycusP2 = data; return;
		}
	} else if (DrvKeycusType == 432) {
		switch (offset)
		{
			case 0: DrvKeycusP1 = data; return;
			case 2: DrvKeycusP2 = data; return;
			case 6: DrvKeycusP3 = data; return;
		}
	} else if (DrvKeycusType == 431) {
		switch (offset)
		{
			case 0: DrvKeycusP1 = data; return;
			case 4: DrvKeycusP2 = data; return;
			case 12: DrvKeycusP3 = data; return;
		}
	} else {
		switch (offset)
		{
			case 1: DrvKeycusP1 = data; return;
			case 2: DrvKeycusP2 = data; return;
			case 3: DrvKeycusP3 = data; return;
		}
	}
}

static UINT8 Namcos12TektagtBankReadByte(UINT32 address)
{
	UINT32 offset = ((UINT32)DrvSystem12Bank * 0x200000) + (address - 0x1fa00000);

	return (offset < DrvBankRomSize) ? DrvBankROM[offset] : 0xff;
}

static UINT16 Namcos12TektagtProtectionRead(UINT32 address)
{
	if (address >= 0x1f700000 && address <= 0x1f700003) {
		DrvTektagtDmaPending = 1;
		return 0;
	}

	if (address >= 0x1fb00000 && address <= 0x1fb00003) {
		return ((address - 0x1fb00000) >> 1) ? 0x0000 : 0x8000;
	}

	if (address >= 0x1fb80000 && address <= 0x1fb80003) {
		if (DrvTektagtProtCount == 2) {
			if (DrvTektagtProtValue[0] == 0x806d2c24 && DrvTektagtProtValue[1] == 0xd5545715) {
				return ((address - 0x1fb80000) >> 1) ? 0x0000 : 0x36e2;
			}
			if (DrvTektagtProtValue[0] == 0x804c2c84 && DrvTektagtProtValue[1] == 0xd5545615) {
				return ((address - 0x1fb80000) >> 1) ? 0x0000 : 0x2651;
			}
			if (DrvTektagtProtValue[0] == 0x2aaba8e6 && DrvTektagtProtValue[1] == 0x00820040) {
				return ((address - 0x1fb80000) >> 1) ? 0x4186 : 0x0000;
			}
			if (DrvTektagtProtValue[0] == 0x2aaba592 && DrvTektagtProtValue[1] == 0x01780544) {
				return ((address - 0x1fb80000) >> 1) ? 0x3c7d : 0x0000;
			}
			if (((DrvTektagtProtValue[1] >> 16) & 0xff) == 0xa9) {
				return 0x552e;
			}
		}

		return 0;
	}

	UINT32 offset = address - 0x1fa00000;
	return Namcos12TektagtBankReadByte(address) |
		(Namcos12TektagtBankReadByte(0x1fa00000 + ((offset + 1) & 0x1fffff)) << 8);
}

static INT32 Namcos12TektagtProtectionWrite(UINT32 address, UINT16 data)
{
	if (address >= 0x1fb00000 && address <= 0x1fb00003) {
		INT32 offset = (address - 0x1fb00000) >> 1;

		if (offset == 0) {
			DrvTektagtDmaOffset = data;
			if (DrvTektagtProtCount != 2) DrvTektagtProtValue[DrvTektagtProtCount] = data;
		} else {
			DrvTektagtDmaOffset |= (UINT32)data << 16;
			if (DrvTektagtProtCount != 2) {
				DrvTektagtProtValue[DrvTektagtProtCount++] |= (UINT32)data << 16;
			}
		}

		return 1;
	}

	if (address >= 0x1fb80000 && address <= 0x1fb80003) {
		if (((address - 0x1fb80000) >> 1) == 0) DrvTektagtProtCount = 0;
		return 1;
	}

	return 0;
}

static UINT16 Namcos12GunRead(UINT32 address)
{
	address &= ~0x00800000;
	UINT32 offset = (address - 0x1f780000) >> 1;
	UINT16 gun[4] = {
		(UINT16)(0x00dc + (BurnGunReturnX(0) * (0x0387 - 0x00dc)) / 0xff),
		(UINT16)(0x0028 + (BurnGunReturnY(0) * (0x011b - 0x0028)) / 0xff),
		(UINT16)(0x00dc + (BurnGunReturnX(1) * (0x0387 - 0x00dc)) / 0xff),
		(UINT16)(0x0028 + (BurnGunReturnY(1) * (0x011b - 0x0028)) / 0xff)
	};
	UINT16 data = gun[offset >> 1];

	if ((data & 3) == 3) data++;

	return data;
}

static UINT8 Namcos11ReadByte(UINT32 address)
{
	UINT32 raw = address;
	address &= 0x1fffffff;

	if (DrvTektagt && ((address >= 0x1f700000 && address <= 0x1f700003) ||
		(address >= 0x1fb00000 && address <= 0x1fb00003) ||
		(address >= 0x1fb80000 && address <= 0x1fb80003))) {
		return Namcos12TektagtProtectionRead(address & ~1) >> ((address & 1) << 3);
	}

	if (DrvTektagt && ((address >= 0x1fb00000 && address <= 0x1fb00fff) ||
		(address >= 0x1fb80000 && address <= 0x1fb80fff))) {
		return Namcos12TektagtBankReadByte(address);
	}

	UINT32 gunAddress = address & ~0x00800000;
	if (DrvLightgunGame && gunAddress >= 0x1f780000 && gunAddress <= 0x1f78000f) {
		return Namcos12GunRead(address & ~1) >> ((address & 1) << 3);
	}

	if (address >= 0x1f800000 && address <= 0x1f8003ff) {
		if ((DrvBiu & (NAMCOS11_BIU_RAM | NAMCOS11_BIU_DS)) != (NAMCOS11_BIU_RAM | NAMCOS11_BIU_DS)) {
			PsxBusError();
			return 0xff;
		}
		return Namcos11Scratchpad()[address & 0x3ff];
	}

	if (address >= 0x1f080000 && address <= 0x1f083fff) {
		return DrvSharedRAM[address - 0x1f080000];
	}

	if ((address >= 0x1f801000 && address <= 0x1f801fff) || (raw >= 0xfffe0000 && raw <= 0xfffeffff)) {
		UINT32 io = (address >= 0x1f801000 && address <= 0x1f801fff) ? (address & ~3) : (raw & ~3);
		return Namcos11ReadIo(io) >> ((raw & 3) << 3);
	}

	if (address >= 0x1f802020 && address <= 0x1f802033) {
		return DrvIoRAM[address - 0x1f802020];
	}

	if (address >= 0x1fa20000 && address <= 0x1fa2001f) {
		UINT8 data = Namcos11KeycusRead(address) >> ((address & 1) << 3);
		return data;
	}

	if (address >= 0x1f140000 && address <= 0x1f140fff) {
		if (address & 1) return 0xff;
		return Namcos11EEPROMRead(address);
	}

	return 0xff;
}

static UINT16 Namcos11ReadHalf(UINT32 address)
{
	UINT32 raw = address;
	address &= 0x1fffffff;

	if (DrvTektagt && ((address >= 0x1f700000 && address <= 0x1f700003) ||
		(address >= 0x1fb00000 && address <= 0x1fb00003) ||
		(address >= 0x1fb80000 && address <= 0x1fb80003))) {
		return Namcos12TektagtProtectionRead(address);
	}

	if (DrvTektagt && ((address >= 0x1fb00000 && address <= 0x1fb00fff) ||
		(address >= 0x1fb80000 && address <= 0x1fb80fff))) {
		return Namcos12TektagtProtectionRead(address);
	}

	UINT32 gunAddress = address & ~0x00800000;
	if (DrvLightgunGame && gunAddress >= 0x1f780000 && gunAddress <= 0x1f78000f) {
		return Namcos12GunRead(address);
	}

	if (address >= 0x1f080000 && address <= 0x1f083fff) {
		UINT32 offset = address - 0x1f080000;
		return DrvSharedRAM[offset] | (DrvSharedRAM[offset + 1] << 8);
	}

	if (address >= 0x1f800000 && address <= 0x1f8003ff) {
		if ((DrvBiu & (NAMCOS11_BIU_RAM | NAMCOS11_BIU_DS)) != (NAMCOS11_BIU_RAM | NAMCOS11_BIU_DS)) {
			PsxBusError();
			return 0xffff;
		}
		UINT32 offset = address & 0x3ff;
		UINT8 *scratch = Namcos11Scratchpad();
		return scratch[offset] | (scratch[(offset + 1) & 0x3ff] << 8);
	}

	if (address >= 0x1f801000 && address <= 0x1f801fff) {
		return Namcos11ReadIo(address & ~3) >> ((address & 2) << 3);
	}

	if (raw >= 0xfffe0000 && raw <= 0xfffeffff) {
		return Namcos11ReadIo(raw & ~3) >> ((raw & 2) << 3);
	}

	if (address >= 0x1f802020 && address <= 0x1f802033) {
		UINT32 offset = address - 0x1f802020;
		return DrvIoRAM[offset] | (DrvIoRAM[(offset + 1) & 0x3f] << 8);
	}

	if (address >= 0x1fa20000 && address <= 0x1fa2001f) {
		UINT16 data = Namcos11KeycusRead(address);
		return data;
	}

	if (address >= 0x1f140000 && address <= 0x1f140fff) {
		return 0xff00 | Namcos11EEPROMRead(address);
	}

	return 0xffff;
}

static UINT32 Namcos11ReadWord(UINT32 address)
{
	UINT32 raw = address;
	UINT32 physical = address & 0x1fffffff;

	if ((physical >= 0x1f801000 && physical <= 0x1f801fff) || (raw >= 0xfffe0000 && raw <= 0xfffeffff)) {
		UINT32 io = (physical >= 0x1f801000 && physical <= 0x1f801fff) ? (physical & ~3) : (raw & ~3);
		return Namcos11ReadIo(io);
	}

	UINT32 low = Namcos11ReadHalf(address);
	UINT32 high = Namcos11ReadHalf(address + 2);

	return low | (high << 16);
}

static void Namcos11WriteByte(UINT32 address, UINT8 data)
{
	UINT32 raw = address;
	address &= 0x1fffffff;

	if (DrvTektagt && address >= 0x1fb00000 && address <= 0x1fb00003) {
		UINT32 aligned = address & ~1;
		UINT16 old = (aligned & 2) ? (DrvTektagtDmaOffset >> 16) : DrvTektagtDmaOffset;
		UINT32 shift = (address & 1) << 3;
		Namcos12TektagtProtectionWrite(aligned, (old & ~(0xff << shift)) | (data << shift));
		return;
	}

	if (DrvTektagt && address >= 0x1fb80000 && address <= 0x1fb80003) {
		Namcos12TektagtProtectionWrite(address & ~1, data);
		return;
	}

	UINT32 gunAddress = address & ~0x00800000;
	if (DrvLightgunGame && gunAddress >= 0x1f788000 && gunAddress <= 0x1f788003) {
		return;
	}

	if (address >= 0x1f080000 && address <= 0x1f083fff) {
		DrvSharedRAM[address - 0x1f080000] = data;
		return;
	}

	if (address >= 0x1f800000 && address <= 0x1f8003ff) {
		if ((DrvBiu & NAMCOS11_BIU_RAM) == 0) {
			PsxBusError();
			return;
		}
		if ((DrvBiu & NAMCOS11_BIU_DS) == 0) {
			return;
		}
		Namcos11Scratchpad()[address & 0x3ff] = data;
		return;
	}

	if (address >= 0x1f802020 && address <= 0x1f802033) {
		DrvIoRAM[address - 0x1f802020] = data;
		return;
	}

	if (address >= 0x1f802040 && address <= 0x1f802043) {
		return;
	}

	if (address >= 0x1f140000 && address <= 0x1f140fff) {
		if ((address & 1) == 0) {
			Namcos11EEPROMWrite(address, data);
		}
		return;
	}

	if (address >= 0x1fa20000 && address <= 0x1fa2001f) {
		UINT16 old = Namcos11KeycusLatch((address - 0x1fa20000) >> 1);
		UINT32 shift = (address & 1) << 3;
		Namcos11KeycusWrite(address, (old & ~(0xff << shift)) | (data << shift));
		return;
	}

	if (address >= 0x1fa10020 && address <= 0x1fa1002f) {
		INT32 bank = (address - 0x1fa10020) >> 1;
		if ((address & 1) == 0 && bank >= 0 && bank < 8) {
			Namcos11SetBank(bank, data);
		}
		return;
	}

	if (address >= 0x1f000000 && address <= 0x1f000001) {
		DrvSystem12Bank = data;
		Namcos11MapBanks();
		return;
	}

	if (address >= 0x1f700000 && address <= 0x1f70ffff) {
		UINT32 lane = address & 3;
		if (lane < 2) {
			UINT32 shift = lane << 3;
			UINT16 old = DrvDmaOffset >> 16;
			UINT16 value = (old & ~(0xff << shift)) | (data << shift);
			UINT32 offset = (address - 0x1f700000) & ~1;
			DrvDmaOffset = offset | ((UINT32)value << 16);
		}
		return;
	}

	if (address >= 0x1fb00000 && address <= 0x1fb00003) {
		return;
	}

	if (address >= 0x1fbf6000 && address <= 0x1fbf6003) {
		return;
	}

	if ((address >= 0x1f801000 && address <= 0x1f801fff) || (raw >= 0xfffe0000 && raw <= 0xfffeffff)) {
		UINT32 io = (address >= 0x1f801000 && address <= 0x1f801fff) ? (address & ~3) : (raw & ~3);
		UINT32 shift = (raw & 3) << 3;
		Namcos11WriteIoMasked(io, data << shift, 0xff << shift);
		return;
	}

}

static void Namcos11WriteHalf(UINT32 address, UINT16 data)
{
	UINT32 raw = address;
	address &= 0x1fffffff;

	if (DrvTektagt && Namcos12TektagtProtectionWrite(address, data)) {
		return;
	}

	UINT32 gunAddress = address & ~0x00800000;
	if (DrvLightgunGame && gunAddress >= 0x1f788000 && gunAddress <= 0x1f788003) {
		return;
	}

	if (address >= 0x1f080000 && address <= 0x1f083fff) {
		UINT32 offset = address - 0x1f080000;
		DrvSharedRAM[offset] = data & 0xff;
		DrvSharedRAM[offset + 1] = data >> 8;
		return;
	}

	if (address >= 0x1f800000 && address <= 0x1f8003ff) {
		if ((DrvBiu & NAMCOS11_BIU_RAM) == 0) {
			PsxBusError();
			return;
		}
		if ((DrvBiu & NAMCOS11_BIU_DS) == 0) {
			return;
		}
		UINT32 offset = address & 0x3ff;
		UINT8 *scratch = Namcos11Scratchpad();
		scratch[offset] = data & 0xff;
		scratch[(offset + 1) & 0x3ff] = data >> 8;
		return;
	}

	if (address >= 0x1f802020 && address <= 0x1f802033) {
		UINT32 offset = address - 0x1f802020;
		DrvIoRAM[offset] = data & 0xff;
		DrvIoRAM[(offset + 1) & 0x3f] = data >> 8;
		return;
	}

	if (address >= 0x1f802040 && address <= 0x1f802043) {
		return;
	}

	if (address >= 0x1f801000 && address <= 0x1f801fff) {
		UINT32 io = address & ~3;
		UINT32 shift = (address & 2) << 3;
		Namcos11WriteIoMasked(io, data << shift, 0xffff << shift);
		return;
	}

	if (raw >= 0xfffe0000 && raw <= 0xfffeffff) {
		UINT32 io = raw & ~3;
		UINT32 shift = (raw & 2) << 3;
		Namcos11WriteIoMasked(io, data << shift, 0xffff << shift);
		return;
	}

	if (address >= 0x1f140000 && address <= 0x1f140fff) {
		Namcos11EEPROMWrite(address, data & 0xff);
		return;
	}

	if (address >= 0x1fa20000 && address <= 0x1fa2001f) {
		Namcos11KeycusWrite(address, data);
		return;
	}

	if (address >= 0x1fa10020 && address <= 0x1fa1002f) {
		INT32 bank = (address - 0x1fa10020) >> 1;
		if (bank >= 0 && bank < 8) {
			Namcos11SetBank(bank, data);
		}
		return;
	}

	if (address >= 0x1f000000 && address <= 0x1f000001) {
		DrvSystem12Bank = data;
		Namcos11MapBanks();
		return;
	}

	if (address >= 0x1f700000 && address <= 0x1f70ffff) {
		if ((address & 1) == 0) {
			UINT32 offset = address - 0x1f700000;
			DrvDmaOffset = offset | ((UINT32)data << 16);
		}
		return;
	}

	if (address >= 0x1fb00000 && address <= 0x1fb00003) {
		return;
	}

	if (address >= 0x1fbf6000 && address <= 0x1fbf6003) {
		return;
	}

}

static void Namcos11WriteWord(UINT32 address, UINT32 data)
{
	UINT32 raw = address;
	address &= 0x1fffffff;

	if ((address >= 0x1f801000 && address <= 0x1f801fff) || (raw >= 0xfffe0000 && raw <= 0xfffeffff)) {
		UINT32 io = (address >= 0x1f801000 && address <= 0x1f801fff) ? (address & ~3) : (raw & ~3);
		Namcos11WriteIo(io, data);
		return;
	}

	Namcos11WriteHalf(address, data & 0xffff);
	Namcos11WriteHalf(address + 2, data >> 16);
}

static INT32 DrvMapMemory()
{
	if (PsxInit()) return 1;

	PsxSetReadByteHandler(0, Namcos11ReadByte);
	PsxSetReadHalfHandler(0, Namcos11ReadHalf);
	PsxSetReadWordHandler(0, Namcos11ReadWord);
	PsxSetWriteByteHandler(0, Namcos11WriteByte);
	PsxSetWriteHalfHandler(0, Namcos11WriteHalf);
	PsxSetWriteWordHandler(0, Namcos11WriteWord);

	memset(DrvBank, 0, sizeof(DrvBank));
	DrvBankOffset = 0;
	DrvDmaOffset = 0;
	DrvSystem12Bank = 0;
	Namcos11ResetIo();
	Namcos11MapRamConfig();
	Namcos11MapRomConfig();

	PsxMapHandler(0, 0x1fb00000, 0x1fbfffff, MAP_WRITE);
	PsxMapHandler(0, 0x1f800000, 0x1f80ffff, MAP_READ | MAP_WRITE);
	PsxMapHandler(0, 0xfffe0000, 0xfffeffff, MAP_READ | MAP_WRITE);
	Namcos11MapBanks();

	PsxReset();
	Namcos12ApplyBootState();

	return 0;
}

static struct BurnInputInfo Namcos11InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 4,	"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Namcos11)

static struct BurnInputInfo LbgrandeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 6"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 6"	},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 4,	"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Lbgrande)

static struct BurnInputInfo MrdrillrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 4,	"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mrdrillr)

static struct BurnInputInfo TektagtInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 5"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 5"	},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 4,	"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tektagt)

static struct BurnInputInfo Myangel3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 4,	"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Myangel3)

static struct BurnInputInfo Ptblank2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Gun X",		BIT_ANALOG_REL,	(UINT8*)(DrvAnalog + 0), "mouse x-axis"},
	{"P1 Gun Y",		BIT_ANALOG_REL,	(UINT8*)(DrvAnalog + 1), "mouse y-axis"},
	{"P1 Trigger",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Gun X",		BIT_ANALOG_REL,	(UINT8*)(DrvAnalog + 2), "p2 x-axis"},
	{"P2 Gun Y",		BIT_ANALOG_REL,	(UINT8*)(DrvAnalog + 3), "p2 y-axis"},
	{"P2 Trigger",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Service",			BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 4,	"diag"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ptblank2)

static struct BurnDIPInfo Namcos11DIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL},
	{0x17, 0xff, 0xff, 0xff, NULL},
};

STDDIPINFO(Namcos11)

static struct BurnDIPInfo LbgrandeDIPList[]=
{
	{0x1a, 0xff, 0xff, 0xff, NULL},
	{0x1b, 0xff, 0xff, 0xff, NULL},

	{0,    0xfe, 0,    2,    "DIP SW2:2"		},
	{0x1b, 0x01, 0x40, 0x40, "Off"			},
	{0x1b, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Lbgrande)

static struct BurnDIPInfo Toukon3DIPList[]=
{
	{0x1a, 0xff, 0xff, 0xff, NULL},
	{0x1b, 0xff, 0xff, 0xff, NULL},

	{0,    0xfe, 0,    2,    "DIP SW2:2"		},
	{0x1b, 0x01, 0x40, 0x40, "Off"			},
	{0x1b, 0x01, 0x40, 0x00, "On"			},

	{0,    0xfe, 0,    2,    "Resolution Type"	},
	{0x1b, 0x01, 0x80, 0x80, "240p"			},
	{0x1b, 0x01, 0x80, 0x00, "480p"			},
};

STDDIPINFO(Toukon3)

static struct BurnDIPInfo MrdrillrDIPList[]=
{
	{0x10, 0xff, 0xff, 0xff, NULL},
	{0x11, 0xff, 0xff, 0xff, NULL},
};

STDDIPINFO(Mrdrillr)

static struct BurnDIPInfo TektagtDIPList[]=
{
	{0x18, 0xff, 0xff, 0xff, NULL},
	{0x19, 0xff, 0xff, 0xff, NULL},
};

STDDIPINFO(Tektagt)

static struct BurnDIPInfo Myangel3DIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL},
	{0x0f, 0xff, 0xff, 0xff, NULL},
};

STDDIPINFO(Myangel3)

static struct BurnDIPInfo Ptblank2DIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL},
	{0x0e, 0xff, 0xff, 0xff, NULL},
};

STDDIPINFO(Ptblank2)

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo PocketrcInputList[] = {
	{ "P1 Coin",       BIT_DIGITAL,    DrvJoy3 + 0,  "p1 coin"   },
	A("Steering",      BIT_ANALOG_REL, &DrvAnalog[0], "p1 x-axis"),
	A("Accelerator",   BIT_ANALOG_REL, &DrvAnalog[1], "p1 fire 1"),
	{ "Accelerator Button", BIT_DIGITAL, DrvJoy1 + 4, "p1 fire 1" },
	{ "Service",       BIT_DIGITAL,    DrvJoy3 + 5,  "service"   },
	{ "Service Mode",  BIT_DIGITAL,    DrvJoy3 + 4,  "diag"      },
	{ "Reset",         BIT_DIGITAL,    &DrvReset,     "reset"     },
	{ "Dip A",         BIT_DIPSWITCH,  DrvDips + 0,  "dip"       },
	{ "Dip B",         BIT_DIPSWITCH,  DrvDips + 1,  "dip"       },
};
#undef A

STDINPUTINFO(Pocketrc)

static struct BurnDIPInfo PocketrcDIPList[]=
{
	{0x07, 0xff, 0xff, 0xff, NULL},
	{0x08, 0xff, 0xff, 0xff, NULL},
};

STDDIPINFO(Pocketrc)

static INT32 DrvLoadRoms()
{
	memset(DrvMainROM, 0xff, 0x0400000);
	memset(DrvBankROM, 0xff, 0x4000000);
	memset(DrvC76ROM,  0xff, 0x0080000);
	memset(DrvC352ROM, 0xff, 0x1000000);
	memset(DrvEEPROM,  0xff, 0x0000800);

	if (BurnLoadRom(DrvMainROM + 0x0000000, 0, 2)) return 1;
	if (BurnLoadRom(DrvMainROM + 0x0000001, 1, 2)) return 1;

	if (DrvPtblank2) {
		*((UINT32 *)(DrvMainROM + 0x331c4)) = 0;
	}

	INT32 subRom;
	INT32 soundRom;

	if (DrvSoulclbr) {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 1)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0800000, 3, 1)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1000000, 4, 1)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800000, 5, 1)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1c00000, 6, 1)) return 1;
		subRom = 7;
		soundRom = 8;
	} else if (DrvToukon3) {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0000001, 3, 2)) return 1;
		subRom = 4;
		soundRom = 5;
	} else if (DrvLbgrande) {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0000001, 3, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800000, 4, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800001, 5, 2)) return 1;
		subRom = 6;
		soundRom = 7;
	} else if (DrvEhrgeiz) {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0000001, 3, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1000000, 4, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1000001, 5, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1400000, 6, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1400001, 7, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800000, 8, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800001, 9, 2)) return 1;
		subRom = 10;
		soundRom = 11;
	} else if (DrvFgtlayer) {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 1)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0800000, 3, 1)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1000000, 4, 1)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800000, 5, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800001, 6, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1c00000, 7, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1c00001, 8, 2)) return 1;
		subRom = 9;
		soundRom = 10;
	} else if (DrvPtblank2) {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0000001, 3, 2)) return 1;
		subRom = 4;
		soundRom = 5;
	} else if (DrvMrdrillr) {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0000001, 3, 2)) return 1;
		subRom = 4;
		soundRom = 5;
	} else if (DrvTektagtRomType == 1) {
		for (INT32 pair = 0; pair < 3; pair++) {
			UINT32 offset = pair * 0x1000000;
			if (BurnLoadRomExt(DrvBankROM + offset + 0, 2 + (pair * 2), 4, LD_GROUP(2))) return 1;
			if (BurnLoadRomExt(DrvBankROM + offset + 2, 3 + (pair * 2), 4, LD_GROUP(2))) return 1;
		}
		for (INT32 lane = 0; lane < 4; lane++) {
			if (BurnLoadRom(DrvBankROM + 0x3000000 + lane, 8 + lane, 4)) return 1;
		}
		subRom = 12;
		soundRom = 13;
	} else if (DrvTektagtRomType == 2) {
		for (INT32 pair = 0; pair < 7; pair++) {
			UINT32 offset = pair * 0x0800000;
			if (BurnLoadRomExt(DrvBankROM + offset + 0, 2 + (pair * 2), 4, LD_GROUP(2))) return 1;
			if (BurnLoadRomExt(DrvBankROM + offset + 2, 3 + (pair * 2), 4, LD_GROUP(2))) return 1;
		}
		subRom = 16;
		soundRom = 17;
	} else {
		if (BurnLoadRom(DrvBankROM + 0x0000000, 2, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0000001, 3, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0800000, 4, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x0800001, 5, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1000000, 6, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1000001, 7, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800000, 8, 2)) return 1;
		if (BurnLoadRom(DrvBankROM + 0x1800001, 9, 2)) return 1;
		subRom = 10;
		soundRom = 11;
	}

	if (BurnLoadRom(DrvC76ROM, subRom, 1)) return 1;
	if (DrvGpuDefaultType == 2) {
		for (INT32 i = 0; i < 0x80000; i += 2) {
			UINT8 data = DrvC76ROM[i];
			DrvC76ROM[i] = DrvC76ROM[i + 1];
			DrvC76ROM[i + 1] = data;
		}
	}

	if (DrvSoulclbr) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom, 1)) return 1;
	} else if (DrvToukon3) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom + 0, 1)) return 1;
		if (BurnLoadRom(DrvC352ROM + 0x0400000, soundRom + 1, 1)) return 1;
	} else if (DrvLbgrande) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom, 1)) return 1;
	} else if (DrvEhrgeiz) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom, 1)) return 1;
	} else if (DrvFgtlayer) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom + 0, 1)) return 1;
		if (BurnLoadRom(DrvC352ROM + 0x0800000, soundRom + 1, 1)) return 1;
	} else if (DrvPtblank2) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom, 1)) return 1;
		memcpy(DrvC352ROM + 0x0800000, DrvC352ROM, 0x400000);
	} else if (DrvMrdrillr) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom, 1)) return 1;
	} else if (DrvTektagtRomType == 1) {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom + 0, 1)) return 1;
		if (BurnLoadRom(DrvC352ROM + 0x0800000, soundRom + 1, 1)) return 1;
	} else if (DrvTektagtRomType == 2) {
		for (INT32 i = 0; i < 4; i++) {
			if (BurnLoadRom(DrvC352ROM + (i * 0x400000), soundRom + i, 1)) return 1;
		}
	} else {
		if (BurnLoadRom(DrvC352ROM + 0x0000000, soundRom + 0, 1)) return 1;
		if (BurnLoadRom(DrvC352ROM + 0x0400000, soundRom + 1, 1)) return 1;
	}

	return 0;
}

static void Namcos12H8InitShared()
{
	static const UINT8 board_id[] = {
		0xff, 0x01, 0x30, 0x51, 0x4e, 0x31, 0x38, 0x30, 0x32, 0x31
	};
	static const UINT8 boot_state[] = {
		0x20, 0x00, 0x20, 0x22, 0x3b, 0x00, 0x00, 0xff,
		0xff, 0x00, 0x00, 0x00, 0x02, 0x00, 0x38
	};
	static const UINT8 rtc_state[] = {
		0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x97, 0x19,
		0x28, 0x05, 0x19, 0x03, 0x07, 0x05
	};
	static const UINT8 io_state[] = {
		0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01
	};
	static const UINT8 version[] = {
		0x79, 0x53, 0x74, 0x73, 0x6d, 0x65, 0x32, 0x31,
		0x56, 0x20, 0x72, 0x65, 0x2e, 0x32, 0x30, 0x30
	};

	memset(DrvSharedRAM, 0, 0x4000);

	DrvSharedRAM[0x0101] = 0x80;
	DrvSharedRAM[0x0141] = 0x01;
	memcpy(DrvSharedRAM + 0x03fe, board_id, sizeof(board_id));
	memcpy(DrvSharedRAM + 0x0583, boot_state, sizeof(boot_state));
	DrvSharedRAM[0x3001] = 0x02;
	memcpy(DrvSharedRAM + 0x300a, rtc_state, sizeof(rtc_state));
	memcpy(DrvSharedRAM + 0x3030, "\x26\x20\x18\x07\x01\x06\x51\x13", 8);
	DrvSharedRAM[0x3036] = DrvTestSwitch ? 0x04 : 0x00;
	DrvSharedRAM[0x3037] = 0x08;
	memcpy(DrvSharedRAM + 0x3050, io_state, sizeof(io_state));

	DrvSharedRAM[0x30f2] = 0x01;
	DrvSharedRAM[0x30f8] = 0x0c;
	DrvSharedRAM[0x30fa] = 0x30;
	DrvSharedRAM[0x30fc] = 0x07;
	DrvSharedRAM[0x30fe] = 0x03;
	DrvSharedRAM[0x3102] = 0x01;
	memset(DrvSharedRAM + 0x3104, 0xff, 0x3c);
	memset(DrvSharedRAM + 0x3280, 0xff, 0x40);
	memset(DrvSharedRAM + 0x33c0, 0xff, 0x40);
	memset(DrvSharedRAM + 0x3440, 0xff, 0x10);
	memset(DrvSharedRAM + 0x3460, 0xff, 0x10);
	memset(DrvSharedRAM + 0x3480, 0xff, 0x08);
	memset(DrvSharedRAM + 0x3810, 0xff, 0x08);

	for (INT32 offset = 0x38c0; offset <= 0x38fe; offset += 2) {
		DrvSharedRAM[offset] = 0x01;
	}

	memcpy(DrvSharedRAM + 0x3b00, version, sizeof(version));
	DrvH8Frame = 0;
}

static void Namcos12H8RunFrame()
{
	DrvH8Frame++;
	DrvSharedRAM[0x3036] = DrvTestSwitch ? 0x04 : 0x00;
	DrvSharedRAM[0x3037] = 0x08;
	DrvSharedRAM[0x300e] = DrvH8Frame & 0xff;
	DrvSharedRAM[0x300f] = DrvH8Frame >> 8;
	if (DrvH8Frame >= 120) {
		DrvSharedRAM[0x0101] = 0;
	}
	if (DrvH8Frame >= 240) {
		DrvSharedRAM[0x0580] = 0x5a;
		DrvSharedRAM[0x3002] = 0x01;
		DrvSharedRAM[0x3003] = 0x76;
		DrvSharedRAM[0x300a] = 0x00;
		DrvSharedRAM[0x3050] = 0xc0;
		DrvSharedRAM[0x3052] = 0x30;
		DrvSharedRAM[0x3054] = 0x30;
		DrvSharedRAM[0x3056] = 0x30;
		DrvSharedRAM[0x306d] = 0xc0;
		DrvSharedRAM[0x306f] = 0xc0;
		DrvSharedRAM[0x3071] = 0xc0;
		DrvSharedRAM[0x3073] = 0x0c;
		DrvSharedRAM[0x3075] = 0x0c;
		DrvSharedRAM[0x3077] = 0x0c;
		DrvSharedRAM[0x3280] = 0x00;
		DrvSharedRAM[0x3281] = 0x00;
		DrvSharedRAM[0x3282] = 0x01;
		DrvSharedRAM[0x3283] = 0x00;
		DrvSharedRAM[0x38b0] = 0x02;
		DrvSharedRAM[0x3a00] = 0x63;
		DrvSharedRAM[0x3a01] = 0x31;
	}

	DrvSharedRAM[0x3006] = 0;
	DrvSharedRAM[0x3007] = 0;
	DrvSharedRAM[0x306a] = 0;
	DrvSharedRAM[0x306b] = 0;
	DrvSharedRAM[0x3078] = 0;
	DrvSharedRAM[0x3079] = 0;
	DrvSharedRAM[0x30f0] = 0;
	DrvSharedRAM[0x30f1] = 0;
	DrvSharedRAM[0x30f2] = 1;
	DrvSharedRAM[0x30f3] = 0;

	DrvSharedRAM[0x3140] = 0;
	DrvSharedRAM[0x3141] = 0;
	DrvSharedRAM[0x3142] = 0;
	DrvSharedRAM[0x3143] = 0;
	DrvSharedRAM[0x3180] = 0;
	DrvSharedRAM[0x3181] = 0;
	DrvSharedRAM[0x3182] = 0;
	DrvSharedRAM[0x3183] = 0;
	DrvSharedRAM[0x31c0] = 0;
	DrvSharedRAM[0x31c1] = 0;
	DrvSharedRAM[0x31c2] = 0;
	DrvSharedRAM[0x31c3] = 0;
}

static UINT8 __fastcall Namcos12H8Read(UINT32 address)
{
	if (address <= 0x07ffff) {
		return DrvC76ROM[address];
	}
	if (address >= 0x080000 && address <= 0x08ffff) {
		return DrvSharedRAM[(address - 0x080000) ^ 1];
	}
	if (address >= 0x280000 && address <= 0x287fff) {
		UINT16 data = c352_read((address & 0x7fff) >> 1);
		return (address & 1) ? data : (data >> 8);
	}
	if (address >= 0x300000 && address <= 0x300003) {
		switch (address & 3) {
			case 0:
				return Namcos11MakePlayerInput(DrvJoy2, DrvJoy3[3]) | (DrvTekken3Inputs ? 0x40 : 0);

			case 1:
				return Namcos11MakePlayerInput(DrvJoy1, DrvJoy3[2]) | (DrvTekken3Inputs ? 0x40 : 0);

			case 2: {
				UINT8 data = 0xff;
				data ^= (DrvJoy3[5] & 1) << 7;
				data ^= (DrvTestSwitch & 1) << 6;
				data ^= (DrvJoy3[0] & 1) << 5;
				data ^= (DrvJoy3[1] & 1) << 4;
				return data;
			}

			case 3: {
				UINT8 data = 0xff;
				if (DrvTekken3Inputs) {
					data ^= (DrvJoy2[6] & 1) << 5;
					data ^= (DrvJoy2[7] & 1) << 6;
					data ^= (DrvJoy1[6] & 1) << 1;
					data ^= (DrvJoy1[7] & 1) << 2;
				} else if (DrvTektagt) {
					data ^= (DrvJoy2[7] & 1) << 5;
					data ^= (DrvJoy2[8] & 1) << 6;
					data ^= (DrvJoy1[7] & 1) << 1;
					data ^= (DrvJoy1[8] & 1) << 2;
				} else if (DrvFgtlayer || DrvLbgrande || DrvToukon3) {
					data ^= (DrvJoy2[7] & 1) << 5;
					data ^= (DrvJoy2[8] & 1) << 6;
					data ^= (DrvJoy2[9] & 1) << 7;
					data ^= (DrvJoy1[7] & 1) << 1;
					data ^= (DrvJoy1[8] & 1) << 2;
					data ^= (DrvJoy1[9] & 1) << 3;
				} else {
					data ^= (DrvJoy2[7] & 1) << 5;
					data ^= (DrvJoy1[7] & 1) << 1;
				}
				return data;
			}
		}
	}
	switch (address) {
		case 0xffffcb: return 0xff;
		case 0xffffce: return DrvDips[0];
		case 0xffffcf: return 0xef;
		case 0xffffd3: return 0xff;
		case 0xffffd6: return DrvH8PortB | 0x7f;
	}
	return 0xff;
}

static void __fastcall Namcos12H8Write(UINT32 address, UINT8 data)
{
	if (address >= 0x080000 && address <= 0x08ffff) {
		DrvSharedRAM[(address - 0x080000) ^ 1] = data;
		return;
	}
	if (address >= 0x280000 && address <= 0x287fff) {
		UINT32 offset = (address & 0x7fff) >> 1;
		UINT16 value = c352_read(offset);
		if (address & 1) {
			value = (value & 0xff00) | data;
		} else {
			value = (value & 0x00ff) | (data << 8);
		}
		c352_write(offset, value);
		return;
	}
	switch (address) {
		case 0xffffcf:
			DrvH8Port8 = data;
			break;

		case 0xffffd3:
			DrvH8PortA = data;
			break;

		case 0xffffd6:
			DrvH8PortB = (DrvH8PortB & 0x80) | (data & 0x7f);
			break;
	}
}

static INT32 DrvInit()
{
	DrvTestSwitch = 0;
	DrvTestSwitchLast = 0;
	DrvH8Port8 = 0;
	DrvH8PortA = 0;
	DrvH8PortB = 0x50;
	DrvBankRom64 = (DrvKeycusType == 443);
	DrvBankRomPairStride = DrvBankRom64 ? (DrvBankRomCompact64 ? 0x0800000 : 0x1000000) : 0x0400000;
	DrvBankRomSize = DrvTektagt ? 0x3800000 : (DrvToukon3 ? 0x0800000 : ((DrvLbgrande || DrvEhrgeiz) ? 0x1c00000 : (DrvPtblank2 ? 0x1000000 : (DrvMrdrillr ? 0x0800000 : 0x2000000))));

	BurnAllocMemIndex();
	GenericTilesInit();

	if (DrvLoadRoms()) {
		GenericTilesExit();
		BurnFreeMemIndex();
		return 1;
	}

	DrvUseH83002 = (DrvGpuDefaultType == 2);
	if (DrvUseH83002) {
		memset(DrvSharedRAM, 0, 0x10000);
		DrvH8Frame = 0;
	} else {
		Namcos12H8InitShared();
	}

	if (DrvMapMemory()) {
		PsxExit();
		GenericTilesExit();
		BurnFreeMemIndex();
		return 1;
	}

	PsxSetInstructionHook(DrvUseBootDecompressHook ? Namcos11InstructionHook : NULL);
	PsxSetExternalInterruptGteCompletion(1);

	c352_init(NAMCOS11_C352_CLOCK, 288, DrvC352ROM, 0x1000000, 0);
	c352_reset();
	if (DrvUseH83002) {
		H83002Init(Namcos12H8Read, Namcos12H8Write);
		H83002Reset();
	}
	if (DrvLightgunGame) {
		BurnGunInit(2, true);
	}

	return 0;
}

static INT32 Tekken3Init()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	return DrvInit();
}

static INT32 LbgrandeInit()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 0;
	DrvLbgrande = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	return DrvInit();
}

static INT32 Toukon3SetResolution()
{
	INT32 height = (DrvDips[1] & 0x80) ? 240 : 480;

	if (height == nScreenHeight) return 0;

	BurnTransferSetDimensions(640, height);
	GenericTilesSetClipRaw(0, 640, 0, height);
	BurnDrvSetVisibleSize(640, height);
	BurnDrvSetAspect(4, 3);
	ReinitialiseVideo();
	BurnTransferRealloc();

	return 1;
}

static INT32 Toukon3Init()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 0;
	DrvToukon3 = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	INT32 result = DrvInit();
	if (result == 0) {
		Toukon3SetResolution();
	}

	return result;
}

static INT32 SoulclbrInit()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 1;
	DrvSoulclbr = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	return DrvInit();
}

static INT32 EhrgeizInit()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 0;
	DrvEhrgeiz = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	return DrvInit();
}

static INT32 FgtlayerInit()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 0;
	DrvFgtlayer = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	return DrvInit();
}

static INT32 Ptblank2Init()
{
	BurnSetRefreshRate(60.0);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 0;
	DrvPtblank2 = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 1;

	return DrvInit();
}

static INT32 MrdrillrInit()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 0;
	DrvMrdrillr = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	return DrvInit();
}

static INT32 TektagtInit()
{
	BurnSetRefreshRate(59.8260978565);

	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvTekken3Inputs = 0;
	DrvTektagt = 1;
	if (DrvTektagtRomType == 0) DrvTektagtRomType = 1;
	DrvKeycusType = 0;
	DrvUseBootDecompressHook = 0;
	DrvLightgunGame = 0;

	return DrvInit();
}

static INT32 TektagtC1aInit()
{
	DrvTektagtRomType = 2;

	return TektagtInit();
}

static INT32 DrvExit()
{
	if (DrvLightgunGame) BurnGunExit();
	DrvLightgunGame = 0;
	DrvBankRomCompact64 = 0;
	DrvTekken3Inputs = 0;
	DrvLbgrande = 0;
	DrvToukon3 = 0;
	DrvSoulclbr = 0;
	DrvEhrgeiz = 0;
	DrvFgtlayer = 0;
	DrvPtblank2 = 0;
	DrvMrdrillr = 0;
	DrvTektagt = 0;
	DrvTektagtRomType = 0;
	DrvBankRomSize = 0x2000000;
	PsxSetInstructionHook(NULL);
	c352_exit();
	if (DrvUseH83002) H83002Exit();
	PsxExit();
	GenericTilesExit();
	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw();

static INT32 DrvFrame()
{
	if (DrvToukon3 && Toukon3SetResolution()) {
		return 0;
	}

	if (DrvLightgunGame) {
		BurnGunMakeInputs(0, DrvAnalog[0], DrvAnalog[1]);
		BurnGunMakeInputs(1, DrvAnalog[2], DrvAnalog[3]);
	}

	if (DrvJoy3[4] && !DrvTestSwitchLast) {
		DrvTestSwitch ^= 1;
	}
	DrvTestSwitchLast = DrvJoy3[4];

	if (DrvReset) {
		PsxReset();
		Namcos12ApplyBootState();
		PsxSetInstructionHook(DrvUseBootDecompressHook ? Namcos11InstructionHook : NULL);
		Namcos11ResetIo();
		Namcos11MapRamConfig();
		Namcos11MapRomConfig();
		Namcos11MapBanks();
		c352_reset();
		if (DrvUseH83002) {
			memset(DrvSharedRAM, 0, 0x10000);
			DrvH8Frame = 0;
			H83002Reset();
		} else {
			Namcos12H8InitShared();
		}
	}

	DrvPsxFrameStartCycles = DrvPsxTotalCycles;
	if (!DrvUseH83002) Namcos12H8RunFrame();

	const INT32 nInterleave = 960;
	const INT32 nPsxCycles = Namcos11GpuFrameCycles();
	const INT32 nH8Cycles = (INT32)(((INT64)16934400 * nPsxCycles) / NAMCOS11_PSX_CPU_CLOCK);
	const INT32 nVblankSlices = (INT32)(((INT64)nInterleave * 2500 * nBurnFPS + 99999999) / 100000000);
	const INT32 nVblankSlice = nInterleave - nVblankSlices;
	INT32 nPsxDone = 0;
	INT32 nH8Done = 0;

	for (INT32 i = 0; i < nInterleave; i++) {
		DrvGpuVpos = (i * DrvGpuScreenHeight) / nInterleave;

		INT32 nPsxSegment = ((i + 1) * nPsxCycles / nInterleave) - nPsxDone;

		if (nPsxSegment > 0) {
			DrvPsxRunBaseCycles = DrvPsxTotalCycles;
			DrvPsxRunSegment = nPsxSegment;
			INT32 nRan = PsxRun(nPsxSegment);
			DrvPsxTotalCycles += nRan;
			nPsxDone += nRan;

			DrvPsxRunBaseCycles = DrvPsxTotalCycles;
			DrvPsxRunSegment = 0;
			Namcos11DmaAdvance();
			Namcos11RootAdvance();
		}

		if (i == nVblankSlice) {
			if (DrvGpuStatus & (1 << 22)) {
				DrvGpuStatus ^= 1 << 13;
			} else {
				DrvGpuStatus |= 1 << 13;
			}

			if (DrvUseH83002) H83002SetIRQLine(1, 1);
			DrvH8PortB |= 0x80;
			DrvIrqStatus |= 0x0001;
			Namcos11UpdateIrq();
		}

		if (DrvUseH83002) {
			INT32 nH8Segment = ((i + 1) * nH8Cycles / nInterleave) - nH8Done;
			if (nH8Segment > 0) {
				nH8Done += H83002Run(nH8Segment);
			}
		}
	}
	if (DrvUseH83002) H83002SetIRQLine(1, 0);
	DrvH8PortB &= 0x7f;
	if (!DrvUseH83002) DrvSharedRAM[0x3036] = DrvTestSwitch ? 0x04 : 0x00;

	if (pBurnSoundOut) {
		c352_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static inline UINT8 pal5bit(UINT16 data)
{
	data &= 0x1f;

	return (data << 3) | (data >> 2);
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x8000; i++) {
		DrvPalette[i] = BurnHighCol(pal5bit(i >> 0), pal5bit(i >> 5), pal5bit(i >> 10), 0);
	}
}

static void Namcos11RemoveTopBorder()
{
	if (DrvKeycusType != 406) return;

	const INT32 border = 10;

	for (INT32 y = 0; y < nScreenHeight; y++) {
		INT32 sourceY = border + (y * (nScreenHeight - border)) / nScreenHeight;
		memcpy(pTransDraw + (y * nScreenWidth), pTransDraw + (sourceY * nScreenWidth), nScreenWidth * sizeof(UINT16));
	}
}

static void Namcos12FixedDisplayRange(INT32 sourceTop, INT32 sourceBottom)
{
	INT32 sourceHeight = nScreenHeight - sourceTop - sourceBottom;
	if (sourceHeight <= 0 || sourceHeight >= nScreenHeight) return;

	INT32 splitLine = 0;

	for (; splitLine < nScreenHeight; splitLine++) {
		INT32 sourceY = sourceTop + (splitLine * sourceHeight) / nScreenHeight;
		if (sourceY < splitLine) break;
		memcpy(pTransDraw + (splitLine * nScreenWidth), pTransDraw + (sourceY * nScreenWidth), nScreenWidth * sizeof(UINT16));
	}

	for (INT32 y = nScreenHeight - 1; y >= splitLine; y--) {
		INT32 sourceY = sourceTop + (y * sourceHeight) / nScreenHeight;
		memcpy(pTransDraw + (y * nScreenWidth), pTransDraw + (sourceY * nScreenWidth), nScreenWidth * sizeof(UINT16));
	}
}

static void Namcos12RemoveMdecBorders()
{
	if (DrvToukon3 && !(DrvGpuStatus & (1 << 21)) && DrvGpuScreenHeight == 240) {
		INT32 border = (nScreenHeight == 480) ? 20 : 10;
		Namcos12FixedDisplayRange(border, border);
		return;
	}

	if (DrvPtblank2) {
		Namcos12FixedDisplayRange(12, 4);
		return;
	}

	if (DrvSoulclbr) {
		Namcos12FixedDisplayRange(0, 20);
		return;
	}

	if (DrvEhrgeiz) {
		Namcos12FixedDisplayRange(6, 5);
		return;
	}

	if (DrvTektagt || DrvTekken3Inputs) {
		Namcos12FixedDisplayRange(10, 2);
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	Namcos11GpuUpdateVisibleArea();

	BurnTransferClear();

	if (DrvGpuStatus & (1 << 23)) {
		BurnTransferCopy(DrvPalette);
		return 0;
	}

	if (DrvGpuStatus & (1 << 21)) {
		INT32 sourceHeight = DrvGpuScreenHeight;
		INT32 height = DrvGpuScreenHeight;
		INT32 scaleToOutput = DrvToukon3 && nScreenHeight == 480;
		if (scaleToOutput) {
			height = nScreenHeight;
		} else if (height > nScreenHeight) {
			height = nScreenHeight;
		}
		for (INT32 y = 0; y < height; y++) {
			UINT32 sy = (DrvGpuDisplayY + (scaleToOutput ? ((y * sourceHeight) / height) : y)) & 0x3ff;
			for (INT32 x = 0; x < nScreenWidth; x++) {
				UINT32 sourcePixel = (x * DrvGpuScreenWidth) / nScreenWidth;
				UINT32 sourceByte = sourcePixel * 3;
				UINT32 sx = DrvGpuDisplayX + (sourceByte >> 1);
				UINT16 word0 = DrvGpuVram[(sy << 10) | (sx & 0x3ff)];
				UINT16 word1 = DrvGpuVram[(sy << 10) | ((sx + 1) & 0x3ff)];
				UINT8 r, g, b;
				if (sourceByte & 1) {
					r = word0 >> 8;
					g = word1 & 0xff;
					b = word1 >> 8;
				} else {
					r = word0 & 0xff;
					g = word0 >> 8;
					b = word1 & 0xff;
				}
				pTransDraw[(y * nScreenWidth) + x] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
			}
		}
	} else {
		INT32 height = DrvTekken3Inputs ? ((DrvGpuStatus & (1 << 22)) ? 480 : 240) : DrvGpuScreenHeight;
		INT32 sourceHeight = height;

		if (DrvToukon3 && nScreenHeight == 480) {
			height = nScreenHeight;
		} else if (height > nScreenHeight) {
			height = nScreenHeight;
		}
		for (INT32 y = 0; y < height; y++) {
			for (INT32 x = 0; x < nScreenWidth; x++) {
				UINT32 sx = (DrvGpuDisplayX + ((x * DrvGpuScreenWidth) / nScreenWidth)) & 0x3ff;
				UINT32 sy = (DrvGpuDisplayY + ((y * sourceHeight) / height)) & 0x3ff;
				UINT16 pixel = DrvGpuVram[(sy << 10) | sx] & 0x7fff;
				pTransDraw[(y * nScreenWidth) + x] = pixel;
			}
		}
	}

	Namcos12RemoveMdecBorders();
	Namcos11RemoveTopBorder();

	BurnTransferCopy(DrvPalette);
	if (DrvLightgunGame) BurnGunDrawTargets();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_MEMORY_RAM) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));

		ba.Data = DrvMainRAM;
		ba.nLen = 0x0400000;
		ba.szName = "Main RAM";
		BurnAcb(&ba);

		ba.Data = DrvSharedRAM;
		ba.nLen = 0x0010000;
		ba.szName = "C76 Shared RAM";
		BurnAcb(&ba);

		ba.Data = DrvScratchRAM;
		ba.nLen = 0x0001000;
		ba.szName = "Scratch RAM";
		BurnAcb(&ba);

		ba.Data = DrvIoRAM;
		ba.nLen = 0x0000040;
		ba.szName = "I/O RAM";
		BurnAcb(&ba);

		ba.Data = DrvGpuVram;
		ba.nLen = 1024 * 1024 * sizeof(UINT16);
		ba.szName = "GPU VRAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data = DrvEEPROM;
		ba.nLen = 0x0000800;
		ba.nAddress = 0;
		ba.szName = "EEPROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		PsxScan(nAction);
		c352_scan(nAction, pnMin);
		SCAN_VAR(DrvBank);
		SCAN_VAR(DrvBankOffset);
		SCAN_VAR(DrvDmaOffset);
		SCAN_VAR(DrvSystem12Bank);
		SCAN_VAR(DrvTektagtDmaOffset);
		SCAN_VAR(DrvTektagtProtValue);
		SCAN_VAR(DrvTektagtProtCount);
		SCAN_VAR(DrvTektagtDmaPending);
		SCAN_VAR(DrvH8Frame);
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
		SCAN_VAR(DrvDmaResume);
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
		SCAN_VAR(DrvGpuVpos);
		SCAN_VAR(DrvKeycusP1);
		SCAN_VAR(DrvKeycusP2);
		SCAN_VAR(DrvKeycusP3);
		SCAN_VAR(DrvKeycusRand);
		SCAN_VAR(DrvEEPROMBusy);
		SCAN_VAR(DrvEEPROMBusyData);
		SCAN_VAR(DrvEEPROMBusyUntil);
		SCAN_VAR(DrvTestSwitch);
		SCAN_VAR(DrvTestSwitchLast);
		if (DrvUseH83002) H83002Scan(nAction);
		if (DrvLightgunGame) BurnGunScan();
	}

	return 0;
}

#define TEKKEN3_DATA_ROMS \
	{ "tet1rom0l.6",  0x400000, 0x2886bb32, 2 | BRF_PRG | BRF_ESS }, \
	{ "tet1rom0u.9",  0x400000, 0xc5705b92, 2 | BRF_PRG | BRF_ESS }, \
	{ "tet1rom1l.7",  0x400000, 0x0397d283, 2 | BRF_PRG | BRF_ESS }, \
	{ "tet1rom1u.10", 0x400000, 0x502ba5cd, 2 | BRF_PRG | BRF_ESS }, \
	{ "tet1rom2l.8",  0x400000, 0xe03b1c24, 2 | BRF_PRG | BRF_ESS }, \
	{ "tet1rom2u.11", 0x400000, 0x75eb2ab3, 2 | BRF_PRG | BRF_ESS }

#define TEKKEN3_FLASH_ROMS \
	{ "tet1fl3l.12", 0x200000, 0x45513073, 2 | BRF_PRG | BRF_ESS }, \
	{ "tet1fl3u.13", 0x200000, 0x1917d993, 2 | BRF_PRG | BRF_ESS }

#define TEKKEN3_FLASH_A_ROMS \
	{ "tet1verafl3l.12", 0x200000, 0x64fa1f83, 2 | BRF_PRG | BRF_ESS }, \
	{ "tet1verafl3u.13", 0x200000, 0x32a2516b, 2 | BRF_PRG | BRF_ESS }

#define TEKKEN3_SOUND_E_ROMS \
	{ "tet1vere.11s", 0x080000, 0xc92b98d1, 3 | BRF_PRG | BRF_ESS }, \
	{ "tet1wave0.5",  0x400000, 0x77ba7975, 4 | BRF_SND }, \
	{ "tet1wave1.4",  0x400000, 0xffeba79f, 4 | BRF_SND }

#define TEKKEN3_SOUND_A_ROMS \
	{ "tet1vera.11s", 0x080000, 0xa74dfe7f, 3 | BRF_PRG | BRF_ESS }, \
	{ "tet1wave0.5",  0x400000, 0x77ba7975, 4 | BRF_SND }, \
	{ "tet1wave1.4",  0x400000, 0xffeba79f, 4 | BRF_SND }

#define TEKKEN3_SOUND_B_ROMS \
	{ "tet1verb.11s", 0x080000, 0xb13d88a9, 3 | BRF_PRG | BRF_ESS }, \
	{ "tet1wave0.5",  0x400000, 0x77ba7975, 4 | BRF_SND }, \
	{ "tet1wave1.4",  0x400000, 0xffeba79f, 4 | BRF_SND }

static struct BurnRomInfo tekken3RomDesc[] = {
	{ "tet2vere1.2e", 0x200000, 0x7ded5461, 1 | BRF_PRG | BRF_ESS },
	{ "tet2vere1.2j", 0x200000, 0x25c96e1e, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_ROMS,
	TEKKEN3_SOUND_E_ROMS
};

STD_ROM_PICK(tekken3)
STD_ROM_FN(tekken3)

static struct BurnRomInfo tekken3aRomDesc[] = {
	{ "tet2vera.2e", 0x200000, 0x7270f157, 1 | BRF_PRG | BRF_ESS },
	{ "tet2vera.2j", 0x200000, 0x94ceb446, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_A_ROMS,
	TEKKEN3_SOUND_A_ROMS
};

STD_ROM_PICK(tekken3a)
STD_ROM_FN(tekken3a)

static struct BurnRomInfo tekken3bRomDesc[] = {
	{ "tet2verb.2e", 0x200000, 0xa6cbc434, 1 | BRF_PRG | BRF_ESS },
	{ "tet2verb.2j", 0x200000, 0xc8f95ec5, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_ROMS,
	TEKKEN3_SOUND_B_ROMS
};

STD_ROM_PICK(tekken3b)
STD_ROM_FN(tekken3b)

static struct BurnRomInfo tekken3cRomDesc[] = {
	{ "tet2verc.2e", 0x200000, 0x4483d76e, 1 | BRF_PRG | BRF_ESS },
	{ "tet2verc.2j", 0x200000, 0x3b4fee42, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_ROMS,
	TEKKEN3_SOUND_B_ROMS
};

STD_ROM_PICK(tekken3c)
STD_ROM_FN(tekken3c)

static struct BurnRomInfo tekken3dRomDesc[] = {
	{ "tet2verd.2e", 0x200000, 0xff269bcd, 1 | BRF_PRG | BRF_ESS },
	{ "tet2verd.2j", 0x200000, 0x46f9205c, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_ROMS,
	TEKKEN3_SOUND_B_ROMS
};

STD_ROM_PICK(tekken3d)
STD_ROM_FN(tekken3d)

static struct BurnRomInfo tekken3uaRomDesc[] = {
	{ "tet3vera.2e", 0x200000, 0x2fb6fac2, 1 | BRF_PRG | BRF_ESS },
	{ "tet3vera.2j", 0x200000, 0x968af792, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_A_ROMS,
	TEKKEN3_SOUND_A_ROMS
};

STD_ROM_PICK(tekken3ua)
STD_ROM_FN(tekken3ua)

static struct BurnRomInfo tekken3udRomDesc[] = {
	{ "tet3verd.2e", 0x200000, 0x9056a8d1, 1 | BRF_PRG | BRF_ESS },
	{ "tet3verd.2j", 0x200000, 0x60ae06f4, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_ROMS,
	TEKKEN3_SOUND_B_ROMS
};

STD_ROM_PICK(tekken3ud)
STD_ROM_FN(tekken3ud)

static struct BurnRomInfo tekken3jaRomDesc[] = {
	{ "tet1vera.2e", 0x200000, 0x98fe53b4, 1 | BRF_PRG | BRF_ESS },
	{ "tet1vera.2j", 0x200000, 0x4dc6bb4a, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_A_ROMS,
	TEKKEN3_SOUND_A_ROMS
};

STD_ROM_PICK(tekken3ja)
STD_ROM_FN(tekken3ja)

static struct BurnRomInfo tekken3jdRomDesc[] = {
	{ "tet1verd.2e", 0x200000, 0x8a2e5ed5, 1 | BRF_PRG | BRF_ESS },
	{ "tet1verd.2j", 0x200000, 0xfd2128f7, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_ROMS,
	TEKKEN3_SOUND_B_ROMS
};

STD_ROM_PICK(tekken3jd)
STD_ROM_FN(tekken3jd)

static struct BurnRomInfo tekken3je1RomDesc[] = {
	{ "tet1vere1.2e", 0x200000, 0x8b01113b, 1 | BRF_PRG | BRF_ESS },
	{ "tet1vere1.2j", 0x200000, 0xdf4c96fb, 1 | BRF_PRG | BRF_ESS },
	TEKKEN3_DATA_ROMS,
	TEKKEN3_FLASH_ROMS,
	TEKKEN3_SOUND_E_ROMS
};

STD_ROM_PICK(tekken3je1)
STD_ROM_FN(tekken3je1)

struct BurnDriver BurnDrvTekken3 = {
	"tekken3", NULL, NULL, NULL, "1997",
	"Tekken 3 (World, TET2/VER.E1)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3RomInfo, tekken3RomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3a = {
	"tekken3a", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (World, TET2/VER.A)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3aRomInfo, tekken3aRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3b = {
	"tekken3b", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (World, TET2/VER.B)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3bRomInfo, tekken3bRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3c = {
	"tekken3c", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (World, TET2/VER.C)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3cRomInfo, tekken3cRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3d = {
	"tekken3d", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (World, TET2/VER.D)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3dRomInfo, tekken3dRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3ua = {
	"tekken3ua", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (US, TET3/VER.A)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3uaRomInfo, tekken3uaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3ud = {
	"tekken3ud", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (US, TET3/VER.D)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3udRomInfo, tekken3udRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3ja = {
	"tekken3ja", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (Japan, TET1/VER.A)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3jaRomInfo, tekken3jaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3jd = {
	"tekken3jd", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (Japan, TET1/VER.D)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3jdRomInfo, tekken3jdRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTekken3je1 = {
	"tekken3je1", "tekken3", NULL, NULL, "1997",
	"Tekken 3 (Japan, TET1/VER.E1)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken3je1RomInfo, tekken3je1RomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

static struct BurnRomInfo lbgrandeRomDesc[] = {
	{ "lg2vera.2l",  0x200000, 0x5ed6b152, 1 | BRF_PRG | BRF_ESS },
	{ "lg2vera.2p",  0x200000, 0x97c57149, 1 | BRF_PRG | BRF_ESS },

	{ "lg1rom0l.6",  0x400000, 0xc5df7f27, 2 | BRF_PRG | BRF_ESS },
	{ "lg1rom0u.9",  0x400000, 0x74607817, 2 | BRF_PRG | BRF_ESS },
	{ "lg1fl3l.12",  0x200000, 0xc9947d3e, 2 | BRF_PRG | BRF_ESS },
	{ "lg1fl3u.13",  0x200000, 0xf3d69f45, 2 | BRF_PRG | BRF_ESS },

	{ "lg1vera.11s", 0x080000, 0xde717a09, 3 | BRF_PRG | BRF_ESS },
	{ "lg1wave0.5",  0x400000, 0x4647fada, 4 | BRF_SND },
};

STD_ROM_PICK(lbgrande)
STD_ROM_FN(lbgrande)

struct BurnDriver BurnDrvLbgrande = {
	"lbgrande", NULL, NULL, NULL, "1997",
	"Libero Grande (World, LG2/VER.A)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, lbgrandeRomInfo, lbgrandeRomName, NULL, NULL, NULL, NULL, LbgrandeInputInfo, LbgrandeDIPInfo,
	LbgrandeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

static struct BurnRomInfo toukon3RomDesc[] = {
	{ "tr1vera.2e",  0x200000, 0x126ebb73, 1 | BRF_PRG | BRF_ESS },
	{ "tr1vera.2j",  0x200000, 0x2edb3ad2, 1 | BRF_PRG | BRF_ESS },

	{ "tr1rom0l.6",  0x400000, 0x42946d26, 2 | BRF_PRG | BRF_ESS },
	{ "tr1rom0u.9",  0x400000, 0xe3cd0be0, 2 | BRF_PRG | BRF_ESS },

	{ "tr1vera.11s", 0x080000, 0xf9ecbd19, 3 | BRF_PRG | BRF_ESS },
	{ "tr1wave0.5",  0x400000, 0x07d10e55, 4 | BRF_SND },
	{ "tr1wave1.4",  0x400000, 0x34539cdd, 4 | BRF_SND },
};

STD_ROM_PICK(toukon3)
STD_ROM_FN(toukon3)

struct BurnDriver BurnDrvToukon3 = {
	"toukon3", NULL, NULL, NULL, "1997",
	"Shin Nihon Pro Wrestling Toukon Retsuden 3 Arcade Edition (Japan, TR1/VER.A)\0", NULL, "Namco / Tomy", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, toukon3RomInfo, toukon3RomName, NULL, NULL, NULL, NULL, LbgrandeInputInfo, Toukon3DIPInfo,
	Toukon3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

#define SOULCLBR_DATA_ROMS \
	{ "soc1rom0.7", 0x800000, 0xcdc47b55, 2 | BRF_PRG | BRF_ESS }, \
	{ "soc1rom1.8", 0x800000, 0x30d2dd5a, 2 | BRF_PRG | BRF_ESS }, \
	{ "soc1rom2.9", 0x800000, 0xdbb93955, 2 | BRF_PRG | BRF_ESS }, \
	{ "soc1fl3.6",  0x400000, 0x24d94c38, 2 | BRF_PRG | BRF_ESS }, \
	{ "soc1fl4.5",  0x400000, 0x6212090e, 2 | BRF_PRG | BRF_ESS }

#define SOULCLBR_SOUND_ROMS \
	{ "soc1vera.11s", 0x080000, 0x52aa206a, 3 | BRF_PRG | BRF_ESS }, \
	{ "soc1wave0.2",  0x800000, 0xc100618d, 4 | BRF_SND }

static struct BurnRomInfo soulclbrRomDesc[] = {
	{ "soc12vera.2l", 0x200000, 0xceadcc9a, 1 | BRF_PRG | BRF_ESS },
	{ "soc12vera.2p", 0x200000, 0x65a74cf0, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbr)
STD_ROM_FN(soulclbr)

static struct BurnRomInfo soulclbrabRomDesc[] = {
	{ "soc14verb.2l", 0x200000, 0x6af5c5f6, 1 | BRF_PRG | BRF_ESS },
	{ "soc14verb.2p", 0x200000, 0x23e7a4c4, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbrab)
STD_ROM_FN(soulclbrab)

static struct BurnRomInfo soulclbracRomDesc[] = {
	{ "soc14verc.2e", 0x200000, 0xc40e9614, 1 | BRF_PRG | BRF_ESS },
	{ "soc14verc.2j", 0x200000, 0x80c41446, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbrac)
STD_ROM_FN(soulclbrac)

static struct BurnRomInfo soulclbrubRomDesc[] = {
	{ "soc13verb.2e", 0x200000, 0xad7cfb1e, 1 | BRF_PRG | BRF_ESS },
	{ "soc13verb.2j", 0x200000, 0x7449c045, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbrub)
STD_ROM_FN(soulclbrub)

static struct BurnRomInfo soulclbrucRomDesc[] = {
	{ "soc13verc.2l", 0x200000, 0x4ba962fb, 1 | BRF_PRG | BRF_ESS },
	{ "soc13verc.2p", 0x200000, 0x140c40de, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbruc)
STD_ROM_FN(soulclbruc)

static struct BurnRomInfo soulclbrja2RomDesc[] = {
	{ "soc1vera.2l", 0x200000, 0x37e0a203, 1 | BRF_PRG | BRF_ESS },
	{ "soc1vera.2p", 0x200000, 0x7cd87a35, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbrja2)
STD_ROM_FN(soulclbrja2)

static struct BurnRomInfo soulclbrjbRomDesc[] = {
	{ "soc11verb.2e", 0x200000, 0x9660d996, 1 | BRF_PRG | BRF_ESS },
	{ "soc11verb.2j", 0x200000, 0x49939880, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbrjb)
STD_ROM_FN(soulclbrjb)

static struct BurnRomInfo soulclbrjcRomDesc[] = {
	{ "soc11verc.2l", 0x200000, 0xf5e3679c, 1 | BRF_PRG | BRF_ESS },
	{ "soc11verc.2p", 0x200000, 0x7537719c, 1 | BRF_PRG | BRF_ESS },
	SOULCLBR_DATA_ROMS,
	SOULCLBR_SOUND_ROMS
};

STD_ROM_PICK(soulclbrjc)
STD_ROM_FN(soulclbrjc)

struct BurnDriver BurnDrvSoulclbr = {
	"soulclbr", NULL, NULL, NULL, "1998",
	"Soul Calibur (World, SOC12/VER.A2)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbrRomInfo, soulclbrRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvSoulclbrab = {
	"soulclbrab", "soulclbr", NULL, NULL, "1998",
	"Soul Calibur (Asia, SOC14/VER.B)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbrabRomInfo, soulclbrabRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvSoulclbrac = {
	"soulclbrac", "soulclbr", NULL, NULL, "1998",
	"Soul Calibur (Asia, SOC14/VER.C)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbracRomInfo, soulclbracRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvSoulclbrub = {
	"soulclbrub", "soulclbr", NULL, NULL, "1998",
	"Soul Calibur (US, SOC13/VER.B)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbrubRomInfo, soulclbrubRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvSoulclbruc = {
	"soulclbruc", "soulclbr", NULL, NULL, "1998",
	"Soul Calibur (US, SOC13/VER.C)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbrucRomInfo, soulclbrucRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvSoulclbrja2 = {
	"soulclbrja2", "soulclbr", NULL, NULL, "1998",
	"Soul Calibur (Japan, SOC11/VER.A2)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbrja2RomInfo, soulclbrja2RomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvSoulclbrjb = {
	"soulclbrjb", "soulclbr", NULL, NULL, "1998",
	"Soul Calibur (Japan, SOC11/VER.B)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbrjbRomInfo, soulclbrjbRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvSoulclbrjc = {
	"soulclbrjc", "soulclbr", NULL, NULL, "1998",
	"Soul Calibur (Japan, SOC11/VER.C)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, soulclbrjcRomInfo, soulclbrjcRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SoulclbrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

#define EHRGEIZ_DATA_ROMS \
	{ "eg1rom0l.12", 0x800000, 0xfe11a48e, 2 | BRF_PRG | BRF_ESS }, \
	{ "eg1rom0u.11", 0x800000, 0x7cb90c25, 2 | BRF_PRG | BRF_ESS }, \
	{ "eg1fl1l.9",   0x200000, 0xdd4cac2b, 2 | BRF_PRG | BRF_ESS }, \
	{ "eg1fl1u.10",  0x200000, 0xe3348407, 2 | BRF_PRG | BRF_ESS }, \
	{ "eg1fl2l.7",   0x200000, 0x34493262, 2 | BRF_PRG | BRF_ESS }, \
	{ "eg1fl2u.8",   0x200000, 0x4fb84f1d, 2 | BRF_PRG | BRF_ESS }, \
	{ "eg1fl3l.5",   0x200000, 0xf9441f87, 2 | BRF_PRG | BRF_ESS }, \
	{ "eg1fl3u.6",   0x200000, 0xcea397de, 2 | BRF_PRG | BRF_ESS }

#define EHRGEIZ_SOUND_ROMS \
	{ "eg1vera.11s", 0x080000, 0x9e44645e, 3 | BRF_PRG | BRF_ESS }, \
	{ "eg1wave0.2",  0x800000, 0x961fe69f, 4 | BRF_SND }

static struct BurnRomInfo ehrgeizRomDesc[] = {
	{ "eg2vera.2e", 0x200000, 0x9174ec90, 1 | BRF_PRG | BRF_ESS },
	{ "eg2vera.2j", 0x200000, 0xa8388248, 1 | BRF_PRG | BRF_ESS },
	EHRGEIZ_DATA_ROMS,
	EHRGEIZ_SOUND_ROMS
};

STD_ROM_PICK(ehrgeiz)
STD_ROM_FN(ehrgeiz)

static struct BurnRomInfo ehrgeizuaRomDesc[] = {
	{ "eg3vera.2l", 0x200000, 0x64c00ff0, 1 | BRF_PRG | BRF_ESS },
	{ "eg3vera.2p", 0x200000, 0xe722c030, 1 | BRF_PRG | BRF_ESS },
	EHRGEIZ_DATA_ROMS,
	EHRGEIZ_SOUND_ROMS
};

STD_ROM_PICK(ehrgeizua)
STD_ROM_FN(ehrgeizua)

static struct BurnRomInfo ehrgeizjaRomDesc[] = {
	{ "eg1vera.2l", 0x200000, 0x302d62cf, 1 | BRF_PRG | BRF_ESS },
	{ "eg1vera.2p", 0x200000, 0x1d7fb3a1, 1 | BRF_PRG | BRF_ESS },
	EHRGEIZ_DATA_ROMS,
	EHRGEIZ_SOUND_ROMS
};

STD_ROM_PICK(ehrgeizja)
STD_ROM_FN(ehrgeizja)

struct BurnDriver BurnDrvEhrgeiz = {
	"ehrgeiz", NULL, NULL, NULL, "1998",
	"Ehrgeiz (World, EG2/VER.A)\0", NULL, "Square / Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ehrgeizRomInfo, ehrgeizRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	EhrgeizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvEhrgeizua = {
	"ehrgeizua", "ehrgeiz", NULL, NULL, "1998",
	"Ehrgeiz (US, EG3/VER.A)\0", NULL, "Square / Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ehrgeizuaRomInfo, ehrgeizuaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	EhrgeizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvEhrgeizja = {
	"ehrgeizja", "ehrgeiz", NULL, NULL, "1998",
	"Ehrgeiz (Japan, EG1/VER.A)\0", NULL, "Square / Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, ehrgeizjaRomInfo, ehrgeizjaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	EhrgeizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

#define FGTLAYER_DATA_ROMS \
	{ "ftl1rom0.9",  0x800000, 0xe33ce365, 2 | BRF_PRG | BRF_ESS }, \
	{ "ftl1rom1.10", 0x800000, 0xa1ec7d08, 2 | BRF_PRG | BRF_ESS }, \
	{ "ftl1rom2.11", 0x800000, 0x204be858, 2 | BRF_PRG | BRF_ESS }, \
	{ "ftl1fl3l.7",  0x200000, 0xe4fb01e1, 2 | BRF_PRG | BRF_ESS }, \
	{ "ftl1fl3h.8",  0x200000, 0x67a5c56f, 2 | BRF_PRG | BRF_ESS }, \
	{ "ftl1fl4l.5",  0x200000, 0x8039242c, 2 | BRF_PRG | BRF_ESS }, \
	{ "ftl1fl4h.6",  0x200000, 0x5ad59726, 2 | BRF_PRG | BRF_ESS }

#define FGTLAYER_SOUND_ROMS \
	{ "ftl1vera.11s", 0x080000, 0xe3f957cd, 3 | BRF_PRG | BRF_ESS }, \
	{ "ftl1wave0.2",  0x800000, 0xee009a2b, 4 | BRF_SND }, \
	{ "ftl1wave1.1",  0x800000, 0xa54a89cd, 4 | BRF_SND }

static struct BurnRomInfo fgtlayerRomDesc[] = {
	{ "ftl3vera.2e", 0x200000, 0x17c23471, 1 | BRF_PRG | BRF_ESS },
	{ "ftl3vera.2j", 0x200000, 0x1626465d, 1 | BRF_PRG | BRF_ESS },
	FGTLAYER_DATA_ROMS,
	FGTLAYER_SOUND_ROMS
};

STD_ROM_PICK(fgtlayer)
STD_ROM_FN(fgtlayer)

static struct BurnRomInfo fgtlayerjaRomDesc[] = {
	{ "ftl0vera.2e", 0x200000, 0xf4156e79, 1 | BRF_PRG | BRF_ESS },
	{ "ftl0vera.2j", 0x200000, 0xc65b57c0, 1 | BRF_PRG | BRF_ESS },
	FGTLAYER_DATA_ROMS,
	FGTLAYER_SOUND_ROMS
};

STD_ROM_PICK(fgtlayerja)
STD_ROM_FN(fgtlayerja)

struct BurnDriver BurnDrvFgtlayer = {
	"fgtlayer", NULL, NULL, NULL, "1998",
	"Fighting Layer (Asia, FTL3/VER.A)\0", NULL, "Arika / Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, fgtlayerRomInfo, fgtlayerRomName, NULL, NULL, NULL, NULL, LbgrandeInputInfo, LbgrandeDIPInfo,
	FgtlayerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvFgtlayerja = {
	"fgtlayerja", "fgtlayer", NULL, NULL, "1998",
	"Fighting Layer (Japan, FTL0/VER.A)\0", NULL, "Arika / Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, fgtlayerjaRomInfo, fgtlayerjaRomName, NULL, NULL, NULL, NULL, LbgrandeInputInfo, LbgrandeDIPInfo,
	FgtlayerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

#define PTBLANK2_DATA_ROMS \
	{ "gnb1prg0l.ic12", 0x800000, 0x78746037, 2 | BRF_PRG | BRF_ESS }, \
	{ "gnb1prg0u.ic11", 0x800000, 0x697d3279, 2 | BRF_PRG | BRF_ESS }

#define PTBLANK2_SOUND_ROMS \
	{ "gnb_vera.11s",   0x080000, 0xd45a53eb, 3 | BRF_PRG | BRF_ESS }, \
	{ "gnb1waveb.ic2",  0x400000, 0x4e19d9d6, 4 | BRF_SND }

static struct BurnRomInfo ptblank2RomDesc[] = {
	{ "gnb5vera.2l", 0x200000, 0x4d0ef3b7, 1 | BRF_PRG | BRF_ESS },
	{ "gnb5vera.2p", 0x200000, 0x5d1d19ff, 1 | BRF_PRG | BRF_ESS },
	PTBLANK2_DATA_ROMS,
	PTBLANK2_SOUND_ROMS
};

STD_ROM_PICK(ptblank2)
STD_ROM_FN(ptblank2)

static struct BurnRomInfo gunbarlRomDesc[] = {
	{ "gnb4vera.2l", 0x200000, 0x88c05cde, 1 | BRF_PRG | BRF_ESS },
	{ "gnb4vera.2p", 0x200000, 0x7d57437a, 1 | BRF_PRG | BRF_ESS },
	PTBLANK2_DATA_ROMS,
	PTBLANK2_SOUND_ROMS
};

STD_ROM_PICK(gunbarl)
STD_ROM_FN(gunbarl)

struct BurnDriver BurnDrvPtblank2 = {
	"ptblank2", NULL, NULL, NULL, "1999",
	"Point Blank 2 (World, GNB5/VER.A)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ptblank2RomInfo, ptblank2RomName, NULL, NULL, NULL, NULL, Ptblank2InputInfo, Ptblank2DIPInfo,
	Ptblank2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

struct BurnDriver BurnDrvGunbarl = {
	"gunbarl", "ptblank2", NULL, NULL, "1999",
	"Gunbarl (Japan, GNB4/VER.A)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, gunbarlRomInfo, gunbarlRomName, NULL, NULL, NULL, NULL, Ptblank2InputInfo, Ptblank2DIPInfo,
	Ptblank2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

#define TEKTAGT_DATA_ROMS \
	{ "teg1_rom0e.ic9",  0x800000, 0xc962a373, 2 | BRF_PRG | BRF_ESS }, \
	{ "teg1_rom0o.ic13", 0x800000, 0xbadb7dcf, 2 | BRF_PRG | BRF_ESS }, \
	{ "teg1_rom1e.ic10", 0x800000, 0xb3d56124, 2 | BRF_PRG | BRF_ESS }, \
	{ "teg1_rom1o.ic14", 0x800000, 0x2434ceb6, 2 | BRF_PRG | BRF_ESS }, \
	{ "teg1_rom2e.ic11", 0x800000, 0x6e5c3428, 2 | BRF_PRG | BRF_ESS }, \
	{ "teg1_rom2o.ic15", 0x800000, 0x21ce9dfa, 2 | BRF_PRG | BRF_ESS }, \
	{ "flel.ic4",        0x200000, 0x88b3823c, 2 | BRF_PRG | BRF_ESS }, \
	{ "fleu.ic5",        0x200000, 0x36df0867, 2 | BRF_PRG | BRF_ESS }, \
	{ "flol.ic6",        0x200000, 0x03a76765, 2 | BRF_PRG | BRF_ESS }, \
	{ "flou.ic7",        0x200000, 0x6d6947d1, 2 | BRF_PRG | BRF_ESS }

#define TEKTAGT_SOUND_ROMS \
	{ "teg1.11s",       0x080000, 0x67d0c469, 3 | BRF_PRG | BRF_ESS }, \
	{ "teg1_wave0.ic1", 0x800000, 0x4bd99104, 4 | BRF_SND }, \
	{ "teg1_wave1.ic12",0x800000, 0xdbc74fff, 4 | BRF_SND }

static struct BurnRomInfo tektagtRomDesc[] = {
	{ "teg2verc1.2e", 0x200000, 0xc6da0717, 1 | BRF_PRG | BRF_ESS },
	{ "teg2verc1.2j", 0x200000, 0x25a1d2ff, 1 | BRF_PRG | BRF_ESS },
	TEKTAGT_DATA_ROMS,
	TEKTAGT_SOUND_ROMS
};

STD_ROM_PICK(tektagt)
STD_ROM_FN(tektagt)

static struct BurnRomInfo tektagtc1aRomDesc[] = {
	{ "teg2ver_c1.2e",   0x200000, 0xc0800960, 1 | BRF_PRG | BRF_ESS },
	{ "teg2ver_c1.2j",   0x200000, 0xc0476713, 1 | BRF_PRG | BRF_ESS },
	{ "teg2roml0",       0x400000, 0xcf984e85, 2 | BRF_PRG | BRF_ESS },
	{ "teg2roml00.ic13", 0x400000, 0x927723a5, 2 | BRF_PRG | BRF_ESS },
	{ "teg2romh0",       0x400000, 0xea088657, 2 | BRF_PRG | BRF_ESS },
	{ "teg2romh00",      0x400000, 0xa85aa306, 2 | BRF_PRG | BRF_ESS },
	{ "teg2roml1",       0x400000, 0x8552f0ef, 2 | BRF_PRG | BRF_ESS },
	{ "teg2roml10.ic14", 0x400000, 0x13eb424b, 2 | BRF_PRG | BRF_ESS },
	{ "teg2romh1",       0x400000, 0x86eb5abe, 2 | BRF_PRG | BRF_ESS },
	{ "teg2romh10",      0x400000, 0x25131a87, 2 | BRF_PRG | BRF_ESS },
	{ "teg2roml2",       0x400000, 0xabdfc6ca, 2 | BRF_PRG | BRF_ESS },
	{ "teg2roml20.ic15", 0x400000, 0xed5ec7f7, 2 | BRF_PRG | BRF_ESS },
	{ "teg2romh2",       0x400000, 0x8c03b301, 2 | BRF_PRG | BRF_ESS },
	{ "teg2romh20",      0x400000, 0xc873c362, 2 | BRF_PRG | BRF_ESS },
	{ "teg2rf4",         0x400000, 0x612f6a37, 2 | BRF_PRG | BRF_ESS },
	{ "teg2rf6",         0x400000, 0x0c9292ce, 2 | BRF_PRG | BRF_ESS },
	{ "teg1.11s",        0x080000, 0x67d0c469, 3 | BRF_PRG | BRF_ESS },
	{ "teg2wavel0.ic12", 0x400000, 0x1865336f, 4 | BRF_SND },
	{ "teg2wavel1",      0x400000, 0xf5ab70e6, 4 | BRF_SND },
	{ "teg2waveh0",      0x400000, 0xbc0325bf, 4 | BRF_SND },
	{ "teg2waveh1",      0x400000, 0x6c654921, 4 | BRF_SND }
};

STD_ROM_PICK(tektagtc1a)
STD_ROM_FN(tektagtc1a)

static struct BurnRomInfo tektagtubRomDesc[] = {
	{ "teg3verb.2l", 0x200000, 0x97df2855, 1 | BRF_PRG | BRF_ESS },
	{ "teg3verb.2p", 0x200000, 0x1dbe7591, 1 | BRF_PRG | BRF_ESS },
	TEKTAGT_DATA_ROMS,
	TEKTAGT_SOUND_ROMS
};

STD_ROM_PICK(tektagtub)
STD_ROM_FN(tektagtub)

static struct BurnRomInfo tektagtuc1RomDesc[] = {
	{ "teg3verc1.2l", 0x200000, 0x1efb7b85, 1 | BRF_PRG | BRF_ESS },
	{ "teg3verc1.2p", 0x200000, 0x7caef9b2, 1 | BRF_PRG | BRF_ESS },
	TEKTAGT_DATA_ROMS,
	TEKTAGT_SOUND_ROMS
};

STD_ROM_PICK(tektagtuc1)
STD_ROM_FN(tektagtuc1)

static struct BurnRomInfo tektagtja3RomDesc[] = {
	{ "teg1vera.2e", 0x200000, 0x17c4bf36, 1 | BRF_PRG | BRF_ESS },
	{ "teg1vera.2j", 0x200000, 0x97cd9524, 1 | BRF_PRG | BRF_ESS },
	TEKTAGT_DATA_ROMS,
	TEKTAGT_SOUND_ROMS
};

STD_ROM_PICK(tektagtja3)
STD_ROM_FN(tektagtja3)

static struct BurnRomInfo tektagtjbRomDesc[] = {
	{ "teg1verb.2e", 0x200000, 0xca6c305f, 1 | BRF_PRG | BRF_ESS },
	{ "teg1verb.2j", 0x200000, 0x5413e2ed, 1 | BRF_PRG | BRF_ESS },
	TEKTAGT_DATA_ROMS,
	TEKTAGT_SOUND_ROMS
};

STD_ROM_PICK(tektagtjb)
STD_ROM_FN(tektagtjb)

static struct BurnRomInfo tektagtjc1RomDesc[] = {
	{ "teg1verc1.2e", 0x200000, 0xadbdfc2e, 1 | BRF_PRG | BRF_ESS },
	{ "teg1verc1.2j", 0x200000, 0x2fa33418, 1 | BRF_PRG | BRF_ESS },
	TEKTAGT_DATA_ROMS,
	TEKTAGT_SOUND_ROMS
};

STD_ROM_PICK(tektagtjc1)
STD_ROM_FN(tektagtjc1)

struct BurnDriver BurnDrvTektagt = {
	"tektagt", NULL, NULL, NULL, "1999",
	"Tekken Tag Tournament (World, TEG2/VER.C1, set 1)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tektagtRomInfo, tektagtRomName, NULL, NULL, NULL, NULL, TektagtInputInfo, TektagtDIPInfo,
	TektagtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTektagtc1a = {
	"tektagtc1a", "tektagt", NULL, NULL, "1999",
	"Tekken Tag Tournament (World, TEG2/VER.C1, set 2)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tektagtc1aRomInfo, tektagtc1aRomName, NULL, NULL, NULL, NULL, TektagtInputInfo, TektagtDIPInfo,
	TektagtC1aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTektagtub = {
	"tektagtub", "tektagt", NULL, NULL, "1999",
	"Tekken Tag Tournament (US, TEG3/VER.B)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tektagtubRomInfo, tektagtubRomName, NULL, NULL, NULL, NULL, TektagtInputInfo, TektagtDIPInfo,
	TektagtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTektagtuc1 = {
	"tektagtuc1", "tektagt", NULL, NULL, "1999",
	"Tekken Tag Tournament (US, TEG3/VER.C1)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tektagtuc1RomInfo, tektagtuc1RomName, NULL, NULL, NULL, NULL, TektagtInputInfo, TektagtDIPInfo,
	TektagtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTektagtja3 = {
	"tektagtja3", "tektagt", NULL, NULL, "1999",
	"Tekken Tag Tournament (Japan, TEG1/VER.A3)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tektagtja3RomInfo, tektagtja3RomName, NULL, NULL, NULL, NULL, TektagtInputInfo, TektagtDIPInfo,
	TektagtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTektagtjb = {
	"tektagtjb", "tektagt", NULL, NULL, "1999",
	"Tekken Tag Tournament (Japan, TEG1/VER.B)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tektagtjbRomInfo, tektagtjbRomName, NULL, NULL, NULL, NULL, TektagtInputInfo, TektagtDIPInfo,
	TektagtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvTektagtjc1 = {
	"tektagtjc1", "tektagt", NULL, NULL, "1999",
	"Tekken Tag Tournament (Japan, TEG1/VER.C1)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tektagtjc1RomInfo, tektagtjc1RomName, NULL, NULL, NULL, NULL, TektagtInputInfo, TektagtDIPInfo,
	TektagtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

#define MRDRILLR_DATA_ROMS \
	{ "dri1rom0l.6", 0x400000, 0x021bb2fa, 2 | BRF_PRG | BRF_ESS }, \
	{ "dri1rom0u.9", 0x400000, 0x5aae85ea, 2 | BRF_PRG | BRF_ESS }

#define MRDRILLR_SOUND_ROMS \
	{ "dri1vera.11s", 0x080000, 0x33ea9c0e, 3 | BRF_PRG | BRF_ESS }, \
	{ "dri1wave0.5",  0x800000, 0x32928df1, 4 | BRF_SND }

static struct BurnRomInfo mrdrillrRomDesc[] = {
	{ "dri3vera2.2l", 0x200000, 0x36b9eeab, 1 | BRF_PRG | BRF_ESS },
	{ "dri3vera2.2p", 0x200000, 0x811c00d5, 1 | BRF_PRG | BRF_ESS },
	MRDRILLR_DATA_ROMS,
	MRDRILLR_SOUND_ROMS
};

STD_ROM_PICK(mrdrillr)
STD_ROM_FN(mrdrillr)

static struct BurnRomInfo mrdrillrja2RomDesc[] = {
	{ "dri1vera2.2l", 0x200000, 0x751ca21d, 1 | BRF_PRG | BRF_ESS },
	{ "dri1vera2.2p", 0x200000, 0x2a2b0704, 1 | BRF_PRG | BRF_ESS },
	MRDRILLR_DATA_ROMS,
	MRDRILLR_SOUND_ROMS
};

STD_ROM_PICK(mrdrillrja2)
STD_ROM_FN(mrdrillrja2)

struct BurnDriver BurnDrvMrdrillr = {
	"mrdrillr", NULL, NULL, NULL, "1999",
	"Mr. Driller (US, DRI3/VER.A2)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mrdrillrRomInfo, mrdrillrRomName, NULL, NULL, NULL, NULL, MrdrillrInputInfo, MrdrillrDIPInfo,
	MrdrillrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

struct BurnDriver BurnDrvMrdrillrja2 = {
	"mrdrillrja2", "mrdrillr", NULL, NULL, "1999",
	"Mr. Driller (Japan, DRI1/VER.A2)\0", NULL, "Namco", "Namco System 12",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mrdrillrja2RomInfo, mrdrillrja2RomName, NULL, NULL, NULL, NULL, MrdrillrInputInfo, MrdrillrDIPInfo,
	MrdrillrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

#undef MRDRILLR_SOUND_ROMS
#undef MRDRILLR_DATA_ROMS
#undef TEKTAGT_SOUND_ROMS
#undef TEKTAGT_DATA_ROMS
#undef PTBLANK2_SOUND_ROMS
#undef PTBLANK2_DATA_ROMS
#undef FGTLAYER_SOUND_ROMS
#undef FGTLAYER_DATA_ROMS
#undef EHRGEIZ_SOUND_ROMS
#undef EHRGEIZ_DATA_ROMS
#undef SOULCLBR_SOUND_ROMS
#undef SOULCLBR_DATA_ROMS
#undef TEKKEN3_SOUND_B_ROMS
#undef TEKKEN3_SOUND_A_ROMS
#undef TEKKEN3_SOUND_E_ROMS
#undef TEKKEN3_FLASH_A_ROMS
#undef TEKKEN3_FLASH_ROMS
#undef TEKKEN3_DATA_ROMS

