// FB Neo Namco System 11 driver module
// Based on MAME namcos11.cpp
// Generated with Codex AI (by DsNo)

#include "tiles_generic.h"
#include "psx_intf.h"
#include "m377_intf.h"
#include "c352.h"
#include "burn_gun.h"
#include "namcos_poly_threads.h"

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvTestSwitch;
static UINT8 DrvTestSwitchLast;
static INT16 DrvAnalog[4];

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBankROM;
static UINT8 *DrvC76BIOS;
static UINT8 *DrvC76ROM;
static UINT8 *DrvC352ROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSharedRAM;
static UINT8 *DrvEEPROM;
static UINT8 *DrvScratchRAM;
static UINT8 *DrvIoRAM;
static UINT16 *DrvGpuVram;
static UINT32 *DrvPalette;
static NamcosPolyThreadPool DrvPolyThreads;
static thread_local INT32 DrvPolyClipY1 = -0x8000;
static thread_local INT32 DrvPolyClipY2 = 0x7fff;

static UINT8 DrvBank[8];
static UINT32 DrvBankOffset;
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
static INT32 DrvBankRomPairs = 3;
static INT32 DrvBankRomDuplicateHalves;
static INT32 DrvBankRom64;
static INT32 DrvBankRomCompact64;
static UINT32 DrvBankRomPairStride;
static INT32 DrvUseBootDecompressHook;
static UINT32 DrvC352LoadOffset;
static UINT32 DrvC352MirrorOffset;
static UINT32 DrvC352MirrorSize;
static INT32 DrvC352WordSwap;
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
static INT32 DrvNativeWidth;
static INT32 DrvFixedCropTop;
static INT32 DrvFixedCropBottom;
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

static const INT32 NAMCOS11_PSX_CLOCK = 67737600;
// The PSX CPU core executes one cycle for every four input clocks, as in MAME.
static const INT32 NAMCOS11_PSX_CPU_CLOCK = NAMCOS11_PSX_CLOCK / 4;
static const INT32 NAMCOS11_C76_OSC = 16934400;
static const INT32 NAMCOS11_C76_CLOCK = NAMCOS11_C76_OSC / 2;
static const INT32 NAMCOS11_C352_CLOCK = 25401600;

static INT32 DrvC76ExtraCycles;
static INT32 DrvC76IrqTimer;

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

static UINT8 Namcos11C76ReadAdc(UINT32 address)
{
	UINT8 data = 0xff;

	if (address & 1) {
		return 0x00;
	}

	if (DrvKeycusType == 432) {
		switch (address)
		{
			case M37710_ADC0_L:
			{
				INT16 steering = (DrvAnalog[0] * 3) / 4;
				return ProcessAnalog(steering, 1, INPUT_DEADZONE, 0x38, 0xc8);
			}

			case M37710_ADC1_L:
			{
				return DrvJoy1[4] ? 0x00 : ProcessAnalog(DrvAnalog[1], 1, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0x7f);
			}
		}
	}

	if (DrvKeycusType == 409) {
		switch (address)
		{
			case M37710_ADC2_L:
				data ^= (DrvJoy1[7] & 1) ? 0xff : 0x00; // P1 button 4
				break;
		}

		return data;
	}

	switch (address)
	{
		case M37710_ADC1_L:
			data ^= (DrvJoy1[7] & 1) ? 0xff : 0x00; // P1 button 4
			break;

		case M37710_ADC2_L:
			data ^= (DrvJoy1[6] & 1) ? 0xff : 0x00; // P1 button 3
			break;
	}

	return data;
}

static UINT8 Namcos11C76ReadPortOrAdc(UINT32 address)
{
	if (address >= M37710_ADC0_L && address <= M37710_ADC7_H) {
		return Namcos11C76ReadAdc(address);
	}

	return 0xff;
}

static void Namcos11C76WritePort(UINT32, UINT8)
{
}

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

static UINT8 Namcos11MakeQuizInput(UINT8 *joy, UINT8 start)
{
	UINT8 data = 0xff;

	data ^= (joy[7] & 1) << 0; // button 4
	data ^= (joy[6] & 1) << 1; // button 3
	data ^= (joy[5] & 1) << 2; // button 2
	data ^= (joy[4] & 1) << 3; // button 1
	data ^= (start & 1) << 7;

	return data;
}

static UINT16 Namcos11C76ReadInput(UINT32 address)
{
	UINT16 data = 0xffff;

	switch (address & 6)
	{
		case 0:
			data = 0xffff;
			if (DrvKeycusType == 409) {
				data ^= (DrvJoy2[7] & 1) << 4; // P2 button 4
			} else {
				data ^= (DrvJoy2[6] & 1) << 4; // P2 button 3
				data ^= (DrvJoy2[7] & 1) << 5; // P2 button 4
			}
			break;

		case 2: {
			UINT8 sw = DrvDips[0];
			sw ^= (DrvJoy3[5] & 1) << 7; // service
			sw ^= (DrvJoy3[0] & 1) << 5; // coin 1
			sw ^= (DrvJoy3[1] & 1) << 4; // coin 2
			sw ^= (DrvTestSwitch & 1) << 6; // test switch
			data = 0xff00 | sw;
			break;
		}

		case 4:
			data = 0xff00 | ((DrvKeycusType == 443 && !DrvLightgunGame) ? Namcos11MakeQuizInput(DrvJoy1, DrvJoy3[2]) : Namcos11MakePlayerInput(DrvJoy1, DrvJoy3[2]));
			break;

		case 6:
			data = 0xff00 | ((DrvKeycusType == 443 && !DrvLightgunGame) ? Namcos11MakeQuizInput(DrvJoy2, DrvJoy3[3]) : Namcos11MakePlayerInput(DrvJoy2, DrvJoy3[3]));
			break;
	}

	return data;
}

static UINT8 Namcos11C76ReadByte(UINT32 address)
{
	if (address >= 0x004000 && address <= 0x00bfff) {
		UINT32 offset = address - 0x004000;
		return DrvSharedRAM[offset];
	}

	if (address >= 0x001000 && address <= 0x001007) {
		return Namcos11C76ReadInput(address) >> ((address & 1) << 3);
	}

	if ((address & 0xfff000) == 0x002000) {
		return c352_read((address & 0x0fff) >> 1) >> ((address & 1) << 3);
	}

	if (address >= 0x510000 && address <= 0x51ffff) {
		return 0x80;
	}

	return 0xff;
}

static UINT16 Namcos11C76ReadWord(UINT32 address)
{
	if (address >= 0x004000 && address <= 0x00bfff) {
		UINT32 offset = address - 0x004000;
		return DrvSharedRAM[offset] | (DrvSharedRAM[(offset + 1) & 0x7fff] << 8);
	}

	if (address >= 0x001000 && address <= 0x001007) {
		return Namcos11C76ReadInput(address);
	}

	if ((address & 0xfff000) == 0x002000) {
		return c352_read((address & 0x0fff) >> 1);
	}

	if (address >= 0x510000 && address <= 0x51ffff) {
		return 0x8080;
	}

	return 0xffff;
}

static void Namcos11C76WriteByte(UINT32 address, UINT8 data)
{
	if (address >= 0x004000 && address <= 0x00bfff) {
		address -= 0x004000;
		DrvSharedRAM[address] = data;
		return;
	}

	if ((address & 0xfff000) == 0x002000) {
		UINT32 offset = (address & 0x0fff) >> 1;
		UINT16 old = c352_read(offset);
		if (address & 1) {
			old = (old & 0x00ff) | (data << 8);
		} else {
			old = (old & 0xff00) | data;
		}
		c352_write(offset, old);
		return;
	}
}

static void Namcos11C76WriteWord(UINT32 address, UINT16 data)
{
	if (address >= 0x004000 && address <= 0x00bfff) {
		address -= 0x004000;
		DrvSharedRAM[address + 0] = data & 0xff;
		DrvSharedRAM[(address + 1) & 0x7fff] = data >> 8;
		return;
	}

	if ((address & 0xfff000) == 0x002000) {
		c352_write((address & 0x0fff) >> 1, data);
		return;
	}
}

static void Namcos11C76Init()
{
	M377Init(0, M37702);
	M377Open(0);
	M377MapMemory(DrvC76BIOS,   0x00c000, 0x00ffff, MAP_ROM);
	M377MapMemory(DrvC76ROM,    0x080000, 0x0fffff, MAP_ROM);
	M377MapMemory(DrvC76ROM,    0x200000, 0x27ffff, MAP_ROM);
	M377MapMemory(DrvC76ROM,    0x280000, 0x2fffff, MAP_ROM);
	M377MapMemory(DrvSharedRAM,  0x004000, 0x00bfff, MAP_READ);
	M377SetWritePortHandler(Namcos11C76WritePort);
	M377SetReadPortHandler(Namcos11C76ReadPortOrAdc);
	M377SetWriteByteHandler(Namcos11C76WriteByte);
	M377SetWriteWordHandler(Namcos11C76WriteWord);
	M377SetReadByteHandler(Namcos11C76ReadByte);
	M377SetReadWordHandler(Namcos11C76ReadWord);
	M377Close();

	c352_init(NAMCOS11_C352_CLOCK, 288, DrvC352ROM, 0x1000000, 0);
	c352_set_sync(M377TotalCycles, NAMCOS11_C76_CLOCK);
}

static void Namcos11C76Reset()
{
	DrvC76ExtraCycles = 0;
	DrvC76IrqTimer = NAMCOS11_C76_CLOCK / 60;

	M377Open(0);
	M377Reset();
	M377Close();

	c352_reset();
}

static void Namcos11C76Exit()
{
	M377Exit();
	c352_exit();
}

static void Namcos11C76NewFrame()
{
	M377NewFrame();
}

static void Namcos11C76Run(INT32 cycles)
{
	M377Open(0);
	M377SetIdleLoop((DrvDips[2] & 1) ? 0x82 : ~0U, 0xc153, 0xff00, 0);

	while (cycles > 0) {
		INT32 segment = cycles;
		if (segment > DrvC76IrqTimer) {
			segment = DrvC76IrqTimer;
		}

		INT32 start = M377TotalCycles();
		M377Run(segment);
		INT32 ran = M377TotalCycles() - start;

		if (ran <= 0) {
			M377Idle(segment);
		} else if (ran < segment) {
			M377Idle(segment - ran);
		}

		cycles -= segment;
		DrvC76IrqTimer -= segment;

		if (DrvC76IrqTimer <= 0) {
			DrvC76IrqTimer += NAMCOS11_C76_CLOCK / 60;
			M377SetIRQLine(M37710_LINE_IRQ0, CPU_IRQSTATUS_HOLD);
			M377SetIRQLine(M37710_LINE_IRQ2, CPU_IRQSTATUS_HOLD);
		}

	}

	M377Close();
}

static UINT8 *Namcos11Scratchpad()
{
	UINT8 *scratch = PsxGetDCache();

	return scratch ? scratch : DrvScratchRAM;
}

static void Namcos11InstructionHook(UINT32 pc)
{
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
	DrvBankROM		= Next; Next += 0x2000000;
	DrvC76BIOS		= Next; Next += 0x0004000;
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

static void Namcos11UpdateIrq()
{
	PsxSetIRQLine(0, (DrvIrqStatus & DrvIrqMask) ? 1 : 0);
}

static void Namcos11SioUpdateIrq(INT32 sio)
{
	UINT32 bit = sio ? 0x0100 : 0x0080;

	if (DrvSioStatus[sio] & PSX_SIO_STATUS_IRQ) {
		DrvIrqStatus |= bit;
	} else {
		DrvIrqStatus &= ~bit;
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

static UINT32 Namcos11PsxRawCycles()
{
	INT32 remaining = PsxGetRemainingCycles();

	if (remaining < 0) remaining = 0;
	if ((UINT32)remaining > DrvPsxRunSegment) remaining = DrvPsxRunSegment;

	return DrvPsxRunBaseCycles + (DrvPsxRunSegment - remaining);
}

static INT32 Namcos11C76TotalCycles()
{
	INT32 cycles;

	M377Open(0);
	cycles = M377TotalCycles();
	M377Close();

	return cycles;
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

	UINT32 offset = ((address - 0x1fa30000) >> 1) & 0x7ff;
	UINT8 result;

	result = DrvEEPROMBusy ? (DrvEEPROMBusyData ^ 0x80) : DrvEEPROM[offset];

	return result;
}

static void Namcos11EEPROMWrite(UINT32 address, UINT8 data)
{
	Namcos11EEPROMUpdateBusy();

	UINT32 offset = ((address - 0x1fa30000) >> 1) & 0x7ff;

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

static void Namcos11SyncC76()
{
	UINT32 psxFrameCycles = Namcos11PsxRawCycles() - DrvPsxFrameStartCycles;
	INT32 cycles = (((INT64)psxFrameCycles * NAMCOS11_C76_CLOCK) / NAMCOS11_PSX_CPU_CLOCK) - Namcos11C76TotalCycles();

	if (cycles > 0) {
		Namcos11C76Run(cycles);
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

	DrvRootExpire[counter] = Namcos11RootCycles() + duration * Namcos11RootDivider(counter);
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

static void Namcos11MapBanks()
{
	for (INT32 i = 0; i < 8; i++) {
		UINT32 base = 0x1f000000 + (i << 20);
		UINT32 bank = DrvBank[i] & (DrvBankRom64 ? 0x1f : 0x0f);

		PsxMapMemory(DrvBankROM + (bank << 20), base, base + 0x0fffff, MAP_READ);
	}
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

static void Namcos11SetBankOffset(UINT32 address)
{
	if (!DrvBankRom64) return;

	DrvBankOffset = (((address - 0x1f080000) >> 1) * 16) & 0x10;
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
			return DrvGpuStatus | ((((DrvGpuStatus & (1 << 22)) && (DrvGpuStatus & (1 << 13))) || (!(DrvGpuStatus & (1 << 22)) && (DrvGpuVpos & 1))) ? (1u << 31) : 0);
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
	} else {
		DrvIrqStatus &= ~0x0008;
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
	if (command >= 0x40 && command <= 0x47) return 3;
	if (command >= 0x50 && command <= 0x57) return 4;
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

static inline INT64 Namcos11GpuEdge64(INT32 ax, INT32 ay, INT32 bx, INT32 by, INT32 cx, INT32 cy)
{
	return ((INT64)(cx - ax) * (by - ay)) - ((INT64)(cy - ay) * (bx - ax));
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

static void Namcos11GpuDrawFlatPolyCore(const INT32 *px, const INT32 *py, INT32 points, UINT16 color, INT32 semi)
{
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
		if (distance > 0 && y >= clipy1 && y <= clipy2 && y >= DrvPolyClipY1 && y <= DrvPolyClipY2) {
			if (x1 < clipx1) {
				distance -= clipx1 - x1;
				x1 = clipx1;
			}
			if (x1 + distance > clipx2 + 1) distance = clipx2 - x1 + 1;
			for (INT32 x = x1; distance > 0; x++, distance--) {
				Namcos11GpuPutSolidPixel(x, y, color, semi);
			}
		}

		cx1 += dx1;
		cx2 += dx2;
		y++;
	}
}

struct NamcosFlatPolyContext {
	INT32 px[4];
	INT32 py[4];
	INT32 points;
	INT32 y1;
	UINT16 color;
	INT32 semi;
};

static void Namcos11GpuDrawFlatPolyWorker(void *opaque, INT32 begin, INT32 end)
{
	NamcosFlatPolyContext *context = (NamcosFlatPolyContext*)opaque;
	DrvPolyClipY1 = context->y1 + begin;
	DrvPolyClipY2 = context->y1 + end - 1;
	Namcos11GpuDrawFlatPolyCore(context->px, context->py, context->points, context->color, context->semi);
	DrvPolyClipY1 = -0x8000;
	DrvPolyClipY2 = 0x7fff;
}

static void Namcos11GpuDrawFlatPoly(const INT32 *px, const INT32 *py, INT32 points, UINT16 color, INT32 semi)
{
	NamcosFlatPolyContext context;
	context.points = points;
	context.color = color;
	context.semi = semi;
	INT32 miny = py[0];
	INT32 maxy = py[0];
	for (INT32 i = 0; i < points; i++) {
		context.px[i] = px[i];
		context.py[i] = py[i];
		if (py[i] < miny) miny = py[i];
		if (py[i] > maxy) maxy = py[i];
	}
	context.y1 = miny;
	DrvPolyThreads.ParallelFor(maxy - miny + 1, 48, Namcos11GpuDrawFlatPolyWorker, &context);
}

static void Namcos11GpuDrawTriangleFlat(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT16 color, INT32 semi)
{
	INT32 px[3] = { x0, x1, x2 };
	INT32 py[3] = { y0, y1, y2 };

	Namcos11GpuDrawFlatPoly(px, py, 3, color, semi);
}

static void Namcos11GpuDrawGouraudPolyMameCore(const INT32 *px, const INT32 *py, const UINT32 *pc, INT32 points, INT32 semi)
{
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
		if (distance > 0 && y >= clipy1 && y <= clipy2 && y >= DrvPolyClipY1 && y <= DrvPolyClipY2) {
			INT32 side1 = x1 == (INT16)(cx1 >> 16);
			INT32 dr = (INT32)(side1 ? (cr2 - cr1) : (cr1 - cr2)) / distance;
			INT32 dg = (INT32)(side1 ? (cg2 - cg1) : (cg1 - cg2)) / distance;
			INT32 db = (INT32)(side1 ? (cb2 - cb1) : (cb1 - cb2)) / distance;
			if (x1 < clipx1) {
				r += dr * (clipx1 - x1);
				g += dg * (clipx1 - x1);
				b += db * (clipx1 - x1);
				distance -= clipx1 - x1;
				x1 = clipx1;
			}
			if (x1 + distance > clipx2 + 1) distance = clipx2 - x1 + 1;
			for (INT32 x = x1; distance > 0; x++, distance--, r += dr, g += dg, b += db) {
				INT32 rr = (r >> 16) & 0xff;
				INT32 gg = (g >> 16) & 0xff;
				INT32 bb = (b >> 16) & 0xff;
				Namcos11GpuPutSolidPixel(x, y, Namcos11GpuShadeComponent(rr) |
					(Namcos11GpuShadeComponent(gg) << 5) |
					(Namcos11GpuShadeComponent(bb) << 10), semi);
			}
		}

		cx1 += dx1; cr1 += dr1; cg1 += dg1; cb1 += db1;
		cx2 += dx2; cr2 += dr2; cg2 += dg2; cb2 += db2;
		y++;
	}
}

struct NamcosGouraudPolyContext {
	INT32 px[4];
	INT32 py[4];
	UINT32 pc[4];
	INT32 points;
	INT32 y1;
	INT32 semi;
};

static void Namcos11GpuDrawGouraudPolyWorker(void *opaque, INT32 begin, INT32 end)
{
	NamcosGouraudPolyContext *context = (NamcosGouraudPolyContext*)opaque;
	DrvPolyClipY1 = context->y1 + begin;
	DrvPolyClipY2 = context->y1 + end - 1;
	Namcos11GpuDrawGouraudPolyMameCore(context->px, context->py, context->pc, context->points, context->semi);
	DrvPolyClipY1 = -0x8000;
	DrvPolyClipY2 = 0x7fff;
}

static void Namcos11GpuDrawGouraudPolyMame(const INT32 *px, const INT32 *py, const UINT32 *pc, INT32 points, INT32 semi)
{
	NamcosGouraudPolyContext context;
	context.points = points;
	context.semi = semi;
	INT32 miny = py[0];
	INT32 maxy = py[0];
	for (INT32 i = 0; i < points; i++) {
		context.px[i] = px[i];
		context.py[i] = py[i];
		context.pc[i] = pc[i];
		if (py[i] < miny) miny = py[i];
		if (py[i] > maxy) maxy = py[i];
	}
	context.y1 = miny;
	DrvPolyThreads.ParallelFor(maxy - miny + 1, 48, Namcos11GpuDrawGouraudPolyWorker, &context);
}

static void Namcos11GpuDrawGouraudPoly(INT32 quad)
{
	UINT32 c0 = DrvGpuPacket[0] & 0x00ffffff;
	INT32 semi = DrvGpuPacket[0] & 0x02000000;
	INT32 x0 = Namcos11GpuPolyX(DrvGpuPacket[1]);
	INT32 y0 = Namcos11GpuPolyY(DrvGpuPacket[1]);
	UINT32 c1 = DrvGpuPacket[2] & 0x00ffffff;
	INT32 x1 = Namcos11GpuPolyX(DrvGpuPacket[3]);
	INT32 y1 = Namcos11GpuPolyY(DrvGpuPacket[3]);
	UINT32 c2 = DrvGpuPacket[4] & 0x00ffffff;
	INT32 x2 = Namcos11GpuPolyX(DrvGpuPacket[5]);
	INT32 y2 = Namcos11GpuPolyY(DrvGpuPacket[5]);

	INT32 px[4] = { x0, x1, x2, 0 };
	INT32 py[4] = { y0, y1, y2, 0 };
	UINT32 pc[4] = { c0, c1, c2, 0 };
	if (quad) {
		pc[3] = DrvGpuPacket[6] & 0x00ffffff;
		px[3] = Namcos11GpuPolyX(DrvGpuPacket[7]);
		py[3] = Namcos11GpuPolyY(DrvGpuPacket[7]);
	}

	Namcos11GpuDrawGouraudPolyMame(px, py, pc, quad ? 4 : 3, semi);
}

static void Namcos11GpuDrawFlatQuad()
{
	UINT16 color = Namcos11GpuColor(DrvGpuPacket[0]);
	INT32 x0 = Namcos11GpuPolyX(DrvGpuPacket[1]);
	INT32 y0 = Namcos11GpuPolyY(DrvGpuPacket[1]);
	INT32 x1 = Namcos11GpuPolyX(DrvGpuPacket[2]);
	INT32 y1 = Namcos11GpuPolyY(DrvGpuPacket[2]);
	INT32 x2 = Namcos11GpuPolyX(DrvGpuPacket[3]);
	INT32 y2 = Namcos11GpuPolyY(DrvGpuPacket[3]);
	INT32 x3 = Namcos11GpuPolyX(DrvGpuPacket[4]);
	INT32 y3 = Namcos11GpuPolyY(DrvGpuPacket[4]);

	INT32 px[4] = { x0, x1, x2, x3 };
	INT32 py[4] = { y0, y1, y2, y3 };

	Namcos11GpuDrawFlatPoly(px, py, 4, color, DrvGpuPacket[0] & 0x02000000);
}

static void Namcos11GpuDrawTexturedPolyMameCore(const INT32 *px, const INT32 *py, const INT32 *pu, const INT32 *pv,
	const UINT32 *pc, INT32 points, UINT32 clut, UINT32 tpage, INT32 raw, INT32 semi, INT32 gouraud)
{
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
		if (distance > 0 && drawy >= clipy1 && drawy <= clipy2 && drawy >= DrvPolyClipY1 && drawy <= DrvPolyClipY2) {
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

struct NamcosTexturedPolyContext {
	INT32 px[4];
	INT32 py[4];
	INT32 pu[4];
	INT32 pv[4];
	UINT32 pc[4];
	INT32 points;
	INT32 y1;
	UINT32 clut;
	UINT32 tpage;
	INT32 raw;
	INT32 semi;
	INT32 gouraud;
};

static void Namcos11GpuDrawTexturedPolyWorker(void *opaque, INT32 begin, INT32 end)
{
	NamcosTexturedPolyContext *context = (NamcosTexturedPolyContext*)opaque;
	DrvPolyClipY1 = context->y1 + begin;
	DrvPolyClipY2 = context->y1 + end - 1;
	Namcos11GpuDrawTexturedPolyMameCore(context->px, context->py, context->pu, context->pv, context->pc,
		context->points, context->clut, context->tpage, context->raw, context->semi, context->gouraud);
	DrvPolyClipY1 = -0x8000;
	DrvPolyClipY2 = 0x7fff;
}

static void Namcos11GpuDrawTexturedPolyMame(const INT32 *px, const INT32 *py, const INT32 *pu, const INT32 *pv,
	const UINT32 *pc, INT32 points, UINT32 clut, UINT32 tpage, INT32 raw, INT32 semi, INT32 gouraud)
{
	NamcosTexturedPolyContext context;
	context.points = points;
	context.clut = clut;
	context.tpage = tpage;
	context.raw = raw;
	context.semi = semi;
	context.gouraud = gouraud;
	INT32 miny = py[0];
	INT32 maxy = py[0];
	for (INT32 i = 0; i < points; i++) {
		context.px[i] = px[i];
		context.py[i] = py[i];
		context.pu[i] = pu[i];
		context.pv[i] = pv[i];
		context.pc[i] = pc[i];
		if (py[i] < miny) miny = py[i];
		if (py[i] > maxy) maxy = py[i];
	}
	context.y1 = miny + DrvGpuDrawOffsetY;
	DrvPolyThreads.ParallelFor(maxy - miny + 1, 48, Namcos11GpuDrawTexturedPolyWorker, &context);
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
			Namcos11GpuDrawTriangleFlat(Namcos11GpuPolyX(DrvGpuPacket[1]), Namcos11GpuPolyY(DrvGpuPacket[1]),
				Namcos11GpuPolyX(DrvGpuPacket[2]), Namcos11GpuPolyY(DrvGpuPacket[2]),
				Namcos11GpuPolyX(DrvGpuPacket[3]), Namcos11GpuPolyY(DrvGpuPacket[3]), Namcos11GpuColor(DrvGpuPacket[0]),
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
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
			Namcos11GpuDrawLine();
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
		if ((command >= 0x48 && command <= 0x4f) || (command >= 0x58 && command <= 0x5f)) {
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
}

static void Namcos11DmaAdvance()
{
	UINT32 now = Namcos11PsxTotalCycles();
	for (INT32 channel = 0; channel < 7; channel++) {
		if ((DrvDmaPending & (1 << channel)) && (INT32)(now - DrvDmaFinishCycle[channel]) >= 0) {
			DrvDmaPending &= ~(1 << channel);
			Namcos11DmaFinish(channel);
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
		if (channel == 1) Namcos11DmaScheduleFinish(channel, 26000 / 4);
		else Namcos11DmaFinish(channel);
		return;
	}

	if (channel == 2 && control == 0x01000401) {
		UINT32 *ram = (UINT32*)DrvMainRAM;
		UINT32 address = DrvDmaBase[channel] & 0x00ffffff;
		UINT32 mask = 0x3fffff;
		UINT32 packets = 0;

		while (address != 0x00ffffff && packets < 0x100000) {
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
			packets++;
		}

		DrvDmaBase[channel] = address;
		Namcos11DmaScheduleFinish(channel, 500 / 4);
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

		Namcos11DmaFinish(channel);
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
					DrvIrqStatus &= ~0x0008;
					Namcos11UpdateIrq();
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

static UINT8 Namcos11ReadByte(UINT32 address)
{
	UINT32 raw = address;
	address &= 0x1fffffff;

	if (address >= 0x1f800000 && address <= 0x1f8003ff) {
		if ((DrvBiu & (NAMCOS11_BIU_RAM | NAMCOS11_BIU_DS)) != (NAMCOS11_BIU_RAM | NAMCOS11_BIU_DS)) {
			PsxBusError();
			return 0xff;
		}
		return Namcos11Scratchpad()[address & 0x3ff];
	}

	if (address >= 0x1fa04000 && address <= 0x1fa0ffff) {
		Namcos11SyncC76();
		return DrvSharedRAM[address - 0x1fa04000];
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

	if (address >= 0x1fa30000 && address <= 0x1fa30fff) {
		if (address & 1) return 0xff;
		return Namcos11EEPROMRead(address);
	}

	return 0xff;
}

static UINT16 Namcos11ReadHalf(UINT32 address)
{
	UINT32 raw = address;
	address &= 0x1fffffff;

	if (DrvLightgunGame && address >= 0x1f780000 && address <= 0x1f78000f) {
		UINT32 offset = (address - 0x1f780000) >> 1;
		UINT16 gun[4] = {
			(UINT16)(0x00d8 + (BurnGunReturnX(0) * (0x0387 - 0x00d8)) / 0xff),
			(UINT16)(0x002c + (BurnGunReturnY(0) * (0x011b - 0x002c)) / 0xff),
			(UINT16)(0x00d8 + (BurnGunReturnX(1) * (0x0387 - 0x00d8)) / 0xff),
			(UINT16)(0x002c + (BurnGunReturnY(1) * (0x011b - 0x002c)) / 0xff)
		};

		switch (offset)
		{
			case 0: return gun[0];
			case 2: return gun[1];
			case 3: return gun[1] + 1;
			case 4: return gun[2];
			case 6: return gun[3];
			case 7: return gun[3] + 1;
		}

		return 0;
	}

	if (address >= 0x1fa04000 && address <= 0x1fa0ffff) {
		Namcos11SyncC76();
		UINT32 offset = address - 0x1fa04000;
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

	if (address >= 0x1fa30000 && address <= 0x1fa30fff) {
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

	if (address >= 0x1fa04000 && address <= 0x1fa0ffff) {
		Namcos11SyncC76();
		DrvSharedRAM[address - 0x1fa04000] = data;
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

	if (address >= 0x1fa30000 && address <= 0x1fa30fff) {
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

	if (address >= 0x1f080000 && address <= 0x1f080003) {
		Namcos11SetBankOffset(address);
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

	if (DrvLightgunGame && address >= 0x1f788000 && address <= 0x1f788003) {
		return;
	}

	if (address >= 0x1fa04000 && address <= 0x1fa0ffff) {
		Namcos11SyncC76();
		UINT32 offset = address - 0x1fa04000;
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

	if (address >= 0x1fa30000 && address <= 0x1fa30fff) {
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

	if (address >= 0x1f080000 && address <= 0x1f080003) {
		Namcos11SetBankOffset(address);
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
	Namcos11ResetIo();
	Namcos11MapRamConfig();
	Namcos11MapRomConfig();
	Namcos11MapBanks();

	PsxMapHandler(0, 0x1fa00000, 0x1fafffff, MAP_READ | MAP_WRITE);
	PsxMapHandler(0, 0x1fb00000, 0x1fbfffff, MAP_WRITE);
	PsxMapHandler(0, 0x1f800000, 0x1f80ffff, MAP_READ | MAP_WRITE);
	PsxMapHandler(0, 0xfffe0000, 0xfffeffff, MAP_READ | MAP_WRITE);

	PsxReset();

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
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Namcos11)

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
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
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
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Ptblank2)

static struct BurnDIPInfo Namcos11DIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL},
	{0x17, 0xff, 0xff, 0xff, NULL},
	{0x18, 0xff, 0xff, 0x00, NULL},

	{0   , 0xfe, 0   ,    2, "Resolution Type"},
	{0x17, 0x01, 0x80, 0x80, "240p"},
	{0x17, 0x01, 0x80, 0x00, "480p"},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"},
	{0x18, 0x01, 0x01, 0x00, "Off"},
	{0x18, 0x01, 0x01, 0x01, "On"},
};

STDDIPINFO(Namcos11)

static struct BurnDIPInfo Myangel3DIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL},
	{0x0f, 0xff, 0xff, 0xff, NULL},
	{0x10, 0xff, 0xff, 0x00, NULL},

	{0   , 0xfe, 0   ,    2, "Resolution Type"},
	{0x0f, 0x01, 0x80, 0x80, "240p"},
	{0x0f, 0x01, 0x80, 0x00, "480p"},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"},
	{0x10, 0x01, 0x01, 0x00, "Off"},
	{0x10, 0x01, 0x01, 0x01, "On"},
};

STDDIPINFO(Myangel3)

static struct BurnDIPInfo Ptblank2DIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL},
	{0x0e, 0xff, 0xff, 0xff, NULL},
	{0x0f, 0xff, 0xff, 0x00, NULL},

	{0   , 0xfe, 0   ,    2, "Resolution Type"},
	{0x0e, 0x01, 0x80, 0x80, "240p"},
	{0x0e, 0x01, 0x80, 0x00, "480p"},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"},
	{0x0f, 0x01, 0x01, 0x00, "Off"},
	{0x0f, 0x01, 0x01, 0x01, "On"},
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
	{ "Dip C",         BIT_DIPSWITCH,  DrvDips + 2,  "dip"       },
};
#undef A

STDINPUTINFO(Pocketrc)

static struct BurnDIPInfo PocketrcDIPList[]=
{
	{0x07, 0xff, 0xff, 0xff, NULL},
	{0x08, 0xff, 0xff, 0xff, NULL},
	{0x09, 0xff, 0xff, 0x00, NULL},

	{0   , 0xfe, 0   ,    2, "Resolution Type"},
	{0x08, 0x01, 0x80, 0x80, "240p"},
	{0x08, 0x01, 0x80, 0x00, "480p"},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"},
	{0x09, 0x01, 0x01, 0x00, "Off"},
	{0x09, 0x01, 0x01, 0x01, "On"},
};

STDDIPINFO(Pocketrc)

static INT32 DrvLoadRoms()
{
	memset(DrvMainROM, 0xff, 0x0400000);
	memset(DrvBankROM, 0xff, 0x2000000);
	memset(DrvC76BIOS, 0xff, 0x0004000);
	memset(DrvC76ROM,  0xff, 0x0080000);
	memset(DrvC352ROM, 0xff, 0x1000000);
	memset(DrvEEPROM,  0xff, 0x0000800);

	INT32 rom;
	if (DrvMainRomLinear == 1) {
		if (BurnLoadRom(DrvMainROM + 0x0000000, 0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0200000, 1, 1)) return 1;
		rom = 2;
	} else if (DrvMainRomLinear == 2) {
		if (BurnLoadRom(DrvMainROM + 0x0000000, 0, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0000001, 1, 2)) return 1;
		rom = 2;
	} else {
		if (BurnLoadRom(DrvMainROM + 0x0000000, 0, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0000001, 1, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0200000, 2, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0200001, 3, 2)) return 1;
		rom = 4;
	}
	if (DrvBankRomDuplicateHalves) {
		UINT8 *temp = (UINT8*)BurnMalloc(0x400000);
		if (temp == NULL) return 1;

		for (INT32 pair = 0; pair < DrvBankRomPairs; pair++) {
			UINT32 offset = pair * DrvBankRomPairStride;
			for (INT32 lane = 0; lane < 2; lane++) {
				if (BurnLoadRom(temp, rom++, 1)) {
					BurnFree(temp);
					return 1;
				}

				for (INT32 i = 0; i < 0x200000; i++) {
					DrvBankROM[offset + (i * 2) + lane] = temp[i];
				}
			}
		}

		BurnFree(temp);
	} else {
		for (INT32 pair = 0; pair < DrvBankRomPairs; pair++) {
			UINT32 offset = pair * DrvBankRomPairStride;
			if (BurnLoadRom(DrvBankROM + offset + 0, rom++, 2)) return 1;
			if (BurnLoadRom(DrvBankROM + offset + 1, rom++, 2)) return 1;
		}
	}

	if (BurnLoadRom(DrvC76BIOS, rom++, 1)) return 1;
	if (BurnLoadRom(DrvC76ROM, rom++, 1)) return 1;
	if (BurnLoadRom(DrvC352ROM + DrvC352LoadOffset, rom++, 1)) return 1;
	if (DrvC352WordSwap) {
		for (UINT32 i = 0; i < 0x400000; i += 2) {
			UINT8 data = DrvC352ROM[DrvC352LoadOffset + i + 0];
			DrvC352ROM[DrvC352LoadOffset + i + 0] = DrvC352ROM[DrvC352LoadOffset + i + 1];
			DrvC352ROM[DrvC352LoadOffset + i + 1] = data;
		}
	}
	if (DrvC352MirrorSize) {
		memcpy(DrvC352ROM + DrvC352MirrorOffset, DrvC352ROM + DrvC352LoadOffset, DrvC352MirrorSize);
	}

	return 0;
}

static INT32 DrvInit()
{
	DrvTestSwitch = 0;
	DrvTestSwitchLast = 0;
	DrvBankRom64 = (DrvKeycusType == 443);
	DrvBankRomPairStride = DrvBankRom64 ? (DrvBankRomCompact64 ? 0x0800000 : 0x1000000) : 0x0400000;

	BurnAllocMemIndex();
	GenericTilesInit();

	if (DrvLoadRoms()) {
		GenericTilesExit();
		BurnFreeMemIndex();
		return 1;
	}

	if (DrvMapMemory()) {
		PsxExit();
		GenericTilesExit();
		BurnFreeMemIndex();
		return 1;
	}

	PsxSetInstructionHook(DrvUseBootDecompressHook ? Namcos11InstructionHook : NULL);
	PsxSetExternalInterruptGteCompletion(DrvKeycusType == 409);

	Namcos11C76Init();
	Namcos11C76Reset();
	if (DrvLightgunGame) BurnGunInit(2, true);
	DrvPolyThreads.Configure();

	return 0;
}

static INT32 TekkenInit()
{
	DrvGpuDefaultType = 1;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 3;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0;
	DrvC352MirrorSize = 0;
	DrvC352WordSwap = 0;
	DrvKeycusType = 406;
	DrvUseBootDecompressHook = 1;

	return DrvInit();
}

static INT32 Tekken2OldInit()
{
	DrvGpuDefaultType = 1;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 4;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x800000;
	DrvC352MirrorOffset = 0;
	DrvC352MirrorSize = 0;
	DrvC352WordSwap = 0;
	DrvKeycusType = 406;
	DrvUseBootDecompressHook = 1;

	return DrvInit();
}

static INT32 Tekken2Init()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 4;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x800000;
	DrvC352MirrorOffset = 0;
	DrvC352MirrorSize = 0;
	DrvC352WordSwap = 0;
	DrvKeycusType = 406;
	DrvUseBootDecompressHook = 1;

	return DrvInit();
}

static INT32 DanceyesInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 4;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 431;
	DrvUseBootDecompressHook = 0;

	return DrvInit();
}

static INT32 SouledgeInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 4;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 409;
	DrvUseBootDecompressHook = 0;

	return DrvInit();
}

static INT32 DunkmniaInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 2;
	DrvBankRomDuplicateHalves = 1;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 410;
	DrvUseBootDecompressHook = 0;

	return DrvInit();
}

static INT32 Xevi3dgInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 3;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 430;
	DrvUseBootDecompressHook = 0;

	return DrvInit();
}

static INT32 PrimglexInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvBankRomPairs = 2;
	DrvBankRomDuplicateHalves = 1;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 1;
	DrvKeycusType = 411;
	DrvUseBootDecompressHook = 0;

	return DrvInit();
}

static INT32 PocketrcInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 2;
	DrvBankRomPairs = 1;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 432;
	DrvUseBootDecompressHook = 0;

	return DrvInit();
}

static INT32 StarswepInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 0;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 442;
	DrvUseBootDecompressHook = 0;
	DrvFixedCropTop = 16;
	DrvFixedCropBottom = 10;

	return DrvInit();
}

static INT32 StarswepjInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 1;
	DrvBankRomPairs = 0;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 442;
	DrvUseBootDecompressHook = 0;
	DrvFixedCropTop = 16;
	DrvFixedCropBottom = 10;

	return DrvInit();
}

static INT32 Myangel3Init()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 1;
	DrvBankRomPairs = 2;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 443;
	DrvBankRomCompact64 = 0;
	DrvLightgunGame = 0;
	DrvUseBootDecompressHook = 0;

	return DrvInit();
}

static INT32 Ptblank2Init()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 2;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 443;
	DrvBankRomCompact64 = 1;
	DrvLightgunGame = 1;
	DrvUseBootDecompressHook = 0;
	DrvFixedCropTop = 12;
	DrvFixedCropBottom = 10;

	return DrvInit();
}

static INT32 Ptblank2bInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 1;
	DrvBankRomPairs = 2;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 443;
	DrvBankRomCompact64 = 1;
	DrvLightgunGame = 1;
	DrvUseBootDecompressHook = 0;
	DrvFixedCropTop = 12;
	DrvFixedCropBottom = 10;

	return DrvInit();
}

static INT32 Ptblank2uaInit()
{
	DrvGpuDefaultType = 2;
	DrvMainRomLinear = 0;
	DrvBankRomPairs = 1;
	DrvBankRomDuplicateHalves = 0;
	DrvC352LoadOffset = 0x000000;
	DrvC352MirrorOffset = 0x800000;
	DrvC352MirrorSize = 0x400000;
	DrvC352WordSwap = 0;
	DrvKeycusType = 443;
	DrvBankRomCompact64 = 0;
	DrvLightgunGame = 1;
	DrvUseBootDecompressHook = 0;
	DrvFixedCropTop = 12;
	DrvFixedCropBottom = 10;

	return DrvInit();
}

static INT32 Namcos11SetResolution()
{
	if (DrvNativeWidth == 0) {
		INT32 nativeHeight;
		BurnDrvGetVisibleSize(&DrvNativeWidth, &nativeHeight);
	}

	INT32 height = (DrvDips[1] & 0x80) ? 240 : 480;
	INT32 width = (height == 480 && DrvNativeWidth < 512) ? DrvNativeWidth * 2 : DrvNativeWidth;

	if (width == nScreenWidth && height == nScreenHeight) return 0;

	BurnTransferSetDimensions(width, height);
	GenericTilesSetClipRaw(0, width, 0, height);
	BurnDrvSetVisibleSize(width, height);
	BurnDrvSetAspect(4, 3);
	if (DrvLightgunGame) BurnGunResolutionChanged();
	ReinitialiseVideo();
	BurnTransferRealloc();

	return 1;
}

static INT32 DrvExit()
{
	DrvPolyThreads.Shutdown();
	if (DrvLightgunGame) BurnGunExit();
	DrvLightgunGame = 0;
	DrvFixedCropTop = 0;
	DrvFixedCropBottom = 0;
	if (DrvNativeWidth) BurnDrvSetVisibleSize(DrvNativeWidth, 240);
	DrvNativeWidth = 0;
	DrvBankRomCompact64 = 0;
	PsxSetInstructionHook(NULL);
	Namcos11C76Exit();
	PsxExit();
	GenericTilesExit();
	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw();

static INT32 DrvFrame()
{
	if (Namcos11SetResolution()) {
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
		PsxSetInstructionHook(DrvUseBootDecompressHook ? Namcos11InstructionHook : NULL);
		Namcos11ResetIo();
		Namcos11MapRamConfig();
		Namcos11MapRomConfig();
		Namcos11MapBanks();
		Namcos11C76Reset();
	}

	DrvPsxFrameStartCycles = DrvPsxTotalCycles;

	if (DrvGpuStatus & (1 << 22)) {
		DrvGpuStatus ^= 1 << 13;
	} else {
		DrvGpuStatus |= 1 << 13;
	}

	Namcos11C76NewFrame();

	const INT32 nInterleave = 240;
	const INT32 nPsxCycles = NAMCOS11_PSX_CPU_CLOCK / 60;
	const INT32 nC76Cycles = NAMCOS11_C76_CLOCK / 60;
	INT32 nPsxDone = 0;
	INT32 nC76Done = DrvC76ExtraCycles;

	if (DrvC76ExtraCycles > 0) {
		M377Open(0);
		M377Idle(DrvC76ExtraCycles);
		M377Close();
	}

	for (INT32 i = 0; i < nInterleave; i++) {
		DrvGpuVpos = i;

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

		nC76Done = Namcos11C76TotalCycles();
		INT32 nC76Segment = ((i + 1) * nC76Cycles / nInterleave) - nC76Done;

		if (nC76Segment > 0) {
			Namcos11C76Run(nC76Segment);
			nC76Done += nC76Segment;
		}

		if (i == (nInterleave - 1)) {
			DrvIrqStatus |= 0x0001;
			Namcos11UpdateIrq();
		}
	}

	DrvC76ExtraCycles = Namcos11C76TotalCycles() - nC76Cycles;
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

static void Namcos11GetFixedVerticalCrop(INT32 &top, INT32 &bottom)
{
	top = DrvFixedCropTop;
	bottom = DrvFixedCropBottom;

	if (DrvKeycusType == 406) top = 10;

	top = top * nScreenHeight / 240;
	bottom = bottom * nScreenHeight / 240;
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	Namcos11GpuUpdateVisibleArea();

	BurnTransferClear();

	INT32 cropTop;
	INT32 cropBottom;
	Namcos11GetFixedVerticalCrop(cropTop, cropBottom);
	const INT32 croppedHeight = nScreenHeight - cropTop - cropBottom;

	if (DrvGpuStatus & (1 << 23)) {
		BurnTransferCopy(DrvPalette);
		return 0;
	}

	if (DrvGpuStatus & (1 << 21)) {
		for (INT32 y = 0; y < nScreenHeight; y++) {
			INT32 sourceY = cropTop + (y * croppedHeight) / nScreenHeight;
			UINT32 sy = (DrvGpuDisplayY + ((sourceY * DrvGpuScreenHeight) / nScreenHeight)) & 0x3ff;
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
		for (INT32 y = 0; y < nScreenHeight; y++) {
			INT32 sourceY = cropTop + (y * croppedHeight) / nScreenHeight;
			for (INT32 x = 0; x < nScreenWidth; x++) {
				UINT32 sx = (DrvGpuDisplayX + ((x * DrvGpuScreenWidth) / nScreenWidth)) & 0x3ff;
				UINT32 sy = (DrvGpuDisplayY + ((sourceY * DrvGpuScreenHeight) / nScreenHeight)) & 0x3ff;
				UINT16 pixel = DrvGpuVram[(sy << 10) | sx] & 0x7fff;
				pTransDraw[(y * nScreenWidth) + x] = pixel;
			}
		}
	}

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
		M377Scan(nAction);
		c352_scan(nAction, pnMin);
		SCAN_VAR(DrvBank);
		SCAN_VAR(DrvBankOffset);
		SCAN_VAR(DrvC76ExtraCycles);
		SCAN_VAR(DrvC76IrqTimer);
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
		if (DrvLightgunGame) BurnGunScan();
	}

	return 0;
}

static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, };

static struct BurnRomInfo namcoc76RomDesc[] = {
	{ "c76.bin",		0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(namcoc76)
STD_ROM_FN(namcoc76)

static INT32 Namcoc76Init()
{
	return 1;
}

struct BurnDriver BurnDrvnamcoc76 = {
	"namcoc76", NULL, NULL, NULL, "1994",
	"Namco C76 (M37702) (Bios)\0", "BIOS only", "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, namcoc76RomInfo, namcoc76RomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Namcoc76Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Tekken (World, TE2/VER.C)

static struct BurnRomInfo tekkenRomDesc[] = {
	{ "te2verc.2l",		0x100000, 0xa24c8c57, 1 | BRF_PRG  | BRF_ESS }, //  0 maincpu:rom
	{ "te2verc.2j",		0x100000, 0x3224c298, 1 | BRF_PRG  | BRF_ESS }, //  1
	{ "te1verb.2k",		0x100000, 0xb9860b29, 1 | BRF_PRG  | BRF_ESS }, //  2
	{ "te1verb.2f",		0x100000, 0x3dc01aad, 1 | BRF_PRG  | BRF_ESS }, //  3

	{ "te1rom0l.ic5",	0x200000, 0x03786e09, 2 | BRF_PRG  | BRF_ESS }, //  4 bankedroms
	{ "te1rom0u.ic6",	0x200000, 0x75d91051, 2 | BRF_PRG  | BRF_ESS }, //  5
	{ "te1rom1l.ic3",	0x200000, 0x81416f8e, 2 | BRF_PRG  | BRF_ESS }, //  6
	{ "te1rom1u.ic8",	0x200000, 0xfa7ba433, 2 | BRF_PRG  | BRF_ESS }, //  7
	{ "te1rom2l.ic4",	0x200000, 0x41d77846, 2 | BRF_PRG  | BRF_ESS }, //  8
	{ "te1rom2u.ic7",	0x200000, 0xa678987e, 2 | BRF_PRG  | BRF_ESS }, //  9

	{ "c76.bin",		0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, // 10 C76 internal BIOS

	{ "te1sprog.6d",	0x040000, 0x849587e9, 4 | BRF_PRG  | BRF_ESS }, // 11 C76 sound data

	{ "te1wave.8k",		0x200000, 0xfce6c57a, 5 | BRF_SND },           // 12 C352 samples
};

STDROMPICKEXT(tekken, tekken, namcoc76)
STD_ROM_FN(tekken)

struct BurnDriver BurnDrvTekken = {
	"tekken", NULL, "namcoc76", NULL, "1994",
	"Tekken (World, TE2/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekkenRomInfo, tekkenRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	TekkenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken (World, TE2/VER.B)

static struct BurnRomInfo tekkenbRomDesc[] = {
	{ "te2verb.2l",		0x100000, 0x246cfbdd, 1 | BRF_PRG  | BRF_ESS }, //  0 maincpu:rom
	{ "te2verb.2j",		0x100000, 0xdfa83e47, 1 | BRF_PRG  | BRF_ESS }, //  1
	{ "te1verb.2k",		0x100000, 0xb9860b29, 1 | BRF_PRG  | BRF_ESS }, //  2
	{ "te1verb.2f",		0x100000, 0x3dc01aad, 1 | BRF_PRG  | BRF_ESS }, //  3

	{ "te1rom0l.ic5",	0x200000, 0x03786e09, 2 | BRF_PRG  | BRF_ESS }, //  4 bankedroms
	{ "te1rom0u.ic6",	0x200000, 0x75d91051, 2 | BRF_PRG  | BRF_ESS }, //  5
	{ "te1rom1l.ic3",	0x200000, 0x81416f8e, 2 | BRF_PRG  | BRF_ESS }, //  6
	{ "te1rom1u.ic8",	0x200000, 0xfa7ba433, 2 | BRF_PRG  | BRF_ESS }, //  7
	{ "te1rom2l.ic4",	0x200000, 0x41d77846, 2 | BRF_PRG  | BRF_ESS }, //  8
	{ "te1rom2u.ic7",	0x200000, 0xa678987e, 2 | BRF_PRG  | BRF_ESS }, //  9

	{ "c76.bin",		0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, // 10 C76 internal BIOS

	{ "te1sprog.6d",	0x040000, 0x849587e9, 4 | BRF_PRG  | BRF_ESS }, // 11 C76 sound data

	{ "te1wave.8k",		0x200000, 0xfce6c57a, 5 | BRF_SND },           // 12 C352 samples
};

STDROMPICKEXT(tekkenb, tekkenb, namcoc76)
STD_ROM_FN(tekkenb)

struct BurnDriver BurnDrvTekkenb = {
	"tekkenb", "tekken", "namcoc76", NULL, "1994",
	"Tekken (World, TE2/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekkenbRomInfo, tekkenbRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	TekkenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken (Asia, TE4/VER.C)

static struct BurnRomInfo tekkenacRomDesc[] = {
	{ "te4verc.2l",		0x100000, 0x7ecb7892, 1 | BRF_PRG  | BRF_ESS }, //  0 maincpu:rom
	{ "te4verc.2j",		0x100000, 0xeea3365d, 1 | BRF_PRG  | BRF_ESS }, //  1
	{ "te1verb.2k",		0x100000, 0xb9860b29, 1 | BRF_PRG  | BRF_ESS }, //  2
	{ "te1verb.2f",		0x100000, 0x3dc01aad, 1 | BRF_PRG  | BRF_ESS }, //  3

	{ "te1rom0l.ic5",	0x200000, 0x03786e09, 2 | BRF_PRG  | BRF_ESS }, //  4 bankedroms
	{ "te1rom0u.ic6",	0x200000, 0x75d91051, 2 | BRF_PRG  | BRF_ESS }, //  5
	{ "te1rom1l.ic3",	0x200000, 0x81416f8e, 2 | BRF_PRG  | BRF_ESS }, //  6
	{ "te1rom1u.ic8",	0x200000, 0xfa7ba433, 2 | BRF_PRG  | BRF_ESS }, //  7
	{ "te1rom2l.ic4",	0x200000, 0x41d77846, 2 | BRF_PRG  | BRF_ESS }, //  8
	{ "te1rom2u.ic7",	0x200000, 0xa678987e, 2 | BRF_PRG  | BRF_ESS }, //  9

	{ "c76.bin",		0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, // 10 C76 internal BIOS

	{ "te1sprog.6d",	0x040000, 0x849587e9, 4 | BRF_PRG  | BRF_ESS }, // 11 C76 sound data

	{ "te1wave.8k",		0x200000, 0xfce6c57a, 5 | BRF_SND },           // 12 C352 samples
};

STDROMPICKEXT(tekkenac, tekkenac, namcoc76)
STD_ROM_FN(tekkenac)

struct BurnDriver BurnDrvTekkenac = {
	"tekkenac", "tekken", "namcoc76", NULL, "1994",
	"Tekken (Asia, TE4/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekkenacRomInfo, tekkenacRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	TekkenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken (Japan, TE1/VER.B)

static struct BurnRomInfo tekkenjbRomDesc[] = {
	{ "te1verb.2l",		0x100000, 0x4291afee, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu:rom
	{ "te1verb.2j",		0x100000, 0x5c534705, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "te1verb.2k",		0x100000, 0xb9860b29, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "te1verb.2f",		0x100000, 0x3dc01aad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "te1rom0l.ic5",	0x200000, 0x03786e09, 2 | BRF_PRG | BRF_ESS }, //  4 bankedroms
	{ "te1rom0u.ic6",	0x200000, 0x75d91051, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "te1rom1l.ic3",	0x200000, 0x81416f8e, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "te1rom1u.ic8",	0x200000, 0xfa7ba433, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "te1rom2l.ic4",	0x200000, 0x41d77846, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "te1rom2u.ic7",	0x200000, 0xa678987e, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "c76.bin",		0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, // 10 C76 internal BIOS

	{ "te1sprog.6d",	0x040000, 0x849587e9, 4 | BRF_PRG | BRF_ESS }, // 11 C76 sound data

	{ "te1wave.8k",		0x200000, 0xfce6c57a, 5 | BRF_SND },           // 12 C352 samples
};

STDROMPICKEXT(tekkenjb, tekkenjb, namcoc76)
STD_ROM_FN(tekkenjb)

struct BurnDriver BurnDrvTekkenjb = {
	"tekkenjb", "tekken", "namcoc76", NULL, "1994",
	"Tekken (Japan, TE1/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekkenjbRomInfo, tekkenjbRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	TekkenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

#define TEKKEN2_COMMON_ROMS \
	{ "tes1rom0l.ic6", 0x200000, 0xfc904ede, 2 | BRF_PRG  | BRF_ESS }, \
	{ "tes1rom0u.ic5", 0x200000, 0x57b38f5d, 2 | BRF_PRG  | BRF_ESS }, \
	{ "tes1rom1l.ic8", 0x200000, 0xaa48f04b, 2 | BRF_PRG  | BRF_ESS }, \
	{ "tes1rom1u.ic3", 0x200000, 0xb147c543, 2 | BRF_PRG  | BRF_ESS }, \
	{ "tes1rom2l.ic7", 0x200000, 0xb08da52c, 2 | BRF_PRG  | BRF_ESS }, \
	{ "tes1rom2u.ic4", 0x200000, 0x8a1561b8, 2 | BRF_PRG  | BRF_ESS }, \
	{ "tes1rom3l.ic9", 0x200000, 0xd5ac0f18, 2 | BRF_PRG  | BRF_ESS }, \
	{ "tes1rom3u.ic1", 0x200000, 0x44ed509d, 2 | BRF_PRG  | BRF_ESS }, \
	{ "c76.bin",       0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, \
	{ "tes1sprog.6d",  0x040000, 0xaf18759f, 4 | BRF_PRG  | BRF_ESS }, \
	{ "tes1wave.8k",   0x400000, 0x34a34eab, 5 | BRF_SND }

// Tekken 2 Ver.B (World, TES2/VER.D)

static struct BurnRomInfo tekken2RomDesc[] = {
	{ "tes2verd.2l", 0x100000, 0x1aef3da8, 1 | BRF_PRG | BRF_ESS },
	{ "tes2verd.2j", 0x100000, 0x03787fd0, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verd.2k", 0x100000, 0x846ace0a, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verd.2f", 0x100000, 0x7a0663b4, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2, tekken2, namcoc76)
STD_ROM_FN(tekken2)

struct BurnDriver BurnDrvTekken2 = {
	"tekken2", NULL, "namcoc76", NULL, "1996",
	"Tekken 2 Ver.B (World, TES2/VER.D)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2RomInfo, tekken2RomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken 2 Ver.B (World, TES2/VER.B)

static struct BurnRomInfo tekken2bRomDesc[] = {
	{ "tes2verb.2l", 0x100000, 0xaa9a4503, 1 | BRF_PRG | BRF_ESS },
	{ "tes2verb.2j", 0x100000, 0x63706d8c, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2k", 0x100000, 0x668ca712, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2f", 0x100000, 0xc4f66a0a, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2b, tekken2b, namcoc76)
STD_ROM_FN(tekken2b)

struct BurnDriver BurnDrvTekken2b = {
	"tekken2b", "tekken2", "namcoc76", NULL, "1995",
	"Tekken 2 Ver.B (World, TES2/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2bRomInfo, tekken2bRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2OldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken 2 Ver.B (US, TES3/VER.D)

static struct BurnRomInfo tekken2udRomDesc[] = {
	{ "tes3verd.2l", 0x100000, 0x0768f36c, 1 | BRF_PRG | BRF_ESS },
	{ "tes3verd.2j", 0x100000, 0xd29a0545, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verd.2k", 0x100000, 0x846ace0a, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verd.2f", 0x100000, 0x7a0663b4, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2ud, tekken2ud, namcoc76)
STD_ROM_FN(tekken2ud)

struct BurnDriver BurnDrvTekken2ud = {
	"tekken2ud", "tekken2", "namcoc76", NULL, "1996",
	"Tekken 2 Ver.B (US, TES3/VER.D)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2udRomInfo, tekken2udRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken 2 Ver.B (US, TES3/VER.B)

static struct BurnRomInfo tekken2ubRomDesc[] = {
	{ "tes3verb.2l", 0x100000, 0x4692075f, 1 | BRF_PRG | BRF_ESS },
	{ "tes3verb.2j", 0x100000, 0xdb3ec640, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2k", 0x100000, 0x668ca712, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2f", 0x100000, 0xc4f66a0a, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2ub, tekken2ub, namcoc76)
STD_ROM_FN(tekken2ub)

struct BurnDriver BurnDrvTekken2ub = {
	"tekken2ub", "tekken2", "namcoc76", NULL, "1995",
	"Tekken 2 Ver.B (US, TES3/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2ubRomInfo, tekken2ubRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2OldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken 2 Ver.B (Japan, TES1/VER.C)

static struct BurnRomInfo tekken2jcRomDesc[] = {
	{ "tes1verc.2l", 0x100000, 0xabcb4981, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verc.2j", 0x100000, 0xd936bf5f, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2k", 0x100000, 0x668ca712, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2f", 0x100000, 0xc4f66a0a, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2jc, tekken2jc, namcoc76)
STD_ROM_FN(tekken2jc)

struct BurnDriver BurnDrvTekken2jc = {
	"tekken2jc", "tekken2", "namcoc76", NULL, "1995",
	"Tekken 2 Ver.B (Japan, TES1/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2jcRomInfo, tekken2jcRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2OldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken 2 Ver.B (Japan, TES1/VER.B)

static struct BurnRomInfo tekken2jbRomDesc[] = {
	{ "tes1verb.2l", 0x100000, 0x9c333739, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2j", 0x100000, 0xdc8cfaea, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2k", 0x100000, 0x668ca712, 1 | BRF_PRG | BRF_ESS },
	{ "tes1verb.2f", 0x100000, 0xc4f66a0a, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2jb, tekken2jb, namcoc76)
STD_ROM_FN(tekken2jb)

struct BurnDriver BurnDrvTekken2jb = {
	"tekken2jb", "tekken2", "namcoc76", NULL, "1995",
	"Tekken 2 Ver.B (Japan, TES1/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2jbRomInfo, tekken2jbRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2OldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken 2 (World, TES2/VER.A)

static struct BurnRomInfo tekken2aRomDesc[] = {
	{ "tes2vera.2l", 0x100000, 0x8bb82bf0, 1 | BRF_PRG | BRF_ESS },
	{ "tes2vera.2j", 0x100000, 0x4e02f921, 1 | BRF_PRG | BRF_ESS },
	{ "tes1vera.2k", 0x100000, 0x78e2ce1a, 1 | BRF_PRG | BRF_ESS },
	{ "tes1vera.2f", 0x100000, 0xfbb0b146, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2a, tekken2a, namcoc76)
STD_ROM_FN(tekken2a)

struct BurnDriver BurnDrvTekken2a = {
	"tekken2a", "tekken2", "namcoc76", NULL, "1995",
	"Tekken 2 (World, TES2/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2aRomInfo, tekken2aRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2OldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

// Tekken 2 (US, TES3/VER.A)

static struct BurnRomInfo tekken2uaRomDesc[] = {
	{ "tes3vera.2l", 0x100000, 0x0276680b, 1 | BRF_PRG | BRF_ESS },
	{ "tes3vera.2j", 0x100000, 0x80e9b5ec, 1 | BRF_PRG | BRF_ESS },
	{ "tes1vera.2k", 0x100000, 0x78e2ce1a, 1 | BRF_PRG | BRF_ESS },
	{ "tes1vera.2f", 0x100000, 0xfbb0b146, 1 | BRF_PRG | BRF_ESS },
	TEKKEN2_COMMON_ROMS
};

STDROMPICKEXT(tekken2ua, tekken2ua, namcoc76)
STD_ROM_FN(tekken2ua)

struct BurnDriver BurnDrvTekken2ua = {
	"tekken2ua", "tekken2", "namcoc76", NULL, "1995",
	"Tekken 2 (US, TES3/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, tekken2uaRomInfo, tekken2uaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Tekken2OldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	512, 240, 4, 3
};

#undef TEKKEN2_COMMON_ROMS

#define SOULEDGE_COMMON_DATA_ROMS \
	{ "so1rom0u.ic5", 0x200000, 0xe364d673, 2 | BRF_PRG  | BRF_ESS }, \
	{ "so1rom0l.ic6", 0x200000, 0x9c5b0858, 2 | BRF_PRG  | BRF_ESS }, \
	{ "so1rom1u.ic3", 0x200000, 0x8f9d8c5b, 2 | BRF_PRG  | BRF_ESS }, \
	{ "so1rom1l.ic8", 0x200000, 0x4406ef16, 2 | BRF_PRG  | BRF_ESS }, \
	{ "so1rom2u.ic4", 0x200000, 0xb4baa886, 2 | BRF_PRG  | BRF_ESS }, \
	{ "so1rom2l.ic7", 0x200000, 0x37c1f66e, 2 | BRF_PRG  | BRF_ESS }, \
	{ "so1rom3u.ic1", 0x200000, 0xf11bd521, 2 | BRF_PRG  | BRF_ESS }, \
	{ "so1rom3l.ic9", 0x200000, 0x84465bcc, 2 | BRF_PRG  | BRF_ESS }, \
	{ "c76.bin",      0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }

// Soul Edge Ver. II (Asia, SO4/VER.C)

static struct BurnRomInfo souledgeRomDesc[] = {
	{ "so4verc.2l",   0x100000, 0x12b8ae0d, 1 | BRF_PRG | BRF_ESS },
	{ "so4verc.2j",   0x100000, 0x938262b0, 1 | BRF_PRG | BRF_ESS },
	{ "so1verc.2k",   0x100000, 0x1789e399, 1 | BRF_PRG | BRF_ESS },
	{ "so1verc.2f",   0x100000, 0x8cffe1c3, 1 | BRF_PRG | BRF_ESS },
	SOULEDGE_COMMON_DATA_ROMS,
	{ "so1sprogc.6d", 0x040000, 0x2bbc118c, 4 | BRF_PRG | BRF_ESS },
	{ "so1wave.8k",   0x400000, 0x0e68836b, 5 | BRF_SND }
};

STDROMPICKEXT(souledge, souledge, namcoc76)
STD_ROM_FN(souledge)

struct BurnDriver BurnDrvSouledge = {
	"souledge", NULL, "namcoc76", NULL, "1996",
	"Soul Edge Ver. II (Asia, SO4/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, souledgeRomInfo, souledgeRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SouledgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Soul Edge Ver. II (US, SO3/VER.C)

static struct BurnRomInfo souledgeucRomDesc[] = {
	{ "so3verc.2l",   0x100000, 0xc90e343b, 1 | BRF_PRG | BRF_ESS },
	{ "so3verc.2j",   0x100000, 0xb7466db5, 1 | BRF_PRG | BRF_ESS },
	{ "so1verc.2k",   0x100000, 0x1789e399, 1 | BRF_PRG | BRF_ESS },
	{ "so1verc.2f",   0x100000, 0x8cffe1c3, 1 | BRF_PRG | BRF_ESS },
	SOULEDGE_COMMON_DATA_ROMS,
	{ "so1sprogc.6d", 0x040000, 0x2bbc118c, 4 | BRF_PRG | BRF_ESS },
	{ "so1wave.8k",   0x400000, 0x0e68836b, 5 | BRF_SND }
};

STDROMPICKEXT(souledgeuc, souledgeuc, namcoc76)
STD_ROM_FN(souledgeuc)

struct BurnDriver BurnDrvSouledgeuc = {
	"souledgeuc", "souledge", "namcoc76", NULL, "1995",
	"Soul Edge Ver. II (US, SO3/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, souledgeucRomInfo, souledgeucRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SouledgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Soul Edge (World, SO2/VER.A)

static struct BurnRomInfo souledgeaRomDesc[] = {
	{ "so2vera.2l",  0x100000, 0x0e9efc5c, 1 | BRF_PRG | BRF_ESS },
	{ "so2vera.2j",  0x100000, 0xfda023f5, 1 | BRF_PRG | BRF_ESS },
	{ "so2vera.2k",  0x100000, 0x29bdc6bb, 1 | BRF_PRG | BRF_ESS },
	{ "so2vera.2f",  0x100000, 0xc035b71b, 1 | BRF_PRG | BRF_ESS },
	SOULEDGE_COMMON_DATA_ROMS,
	{ "so1sprog.6d", 0x040000, 0xf6f682b7, 4 | BRF_PRG | BRF_ESS },
	{ "so1wave.8k",  0x400000, 0x0e68836b, 5 | BRF_SND }
};

STDROMPICKEXT(souledgea, souledgea, namcoc76)
STD_ROM_FN(souledgea)

struct BurnDriver BurnDrvSouledgea = {
	"souledgea", "souledge", "namcoc76", NULL, "1995",
	"Soul Edge (World, SO2/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, souledgeaRomInfo, souledgeaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SouledgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Soul Edge (US, SO3/VER.A)

static struct BurnRomInfo souledgeuaRomDesc[] = {
	{ "so3vera.2l",  0x100000, 0x19b39096, 1 | BRF_PRG | BRF_ESS },
	{ "so3vera.2j",  0x100000, 0x09eda46f, 1 | BRF_PRG | BRF_ESS },
	{ "so1vera.2k",  0x100000, 0x29bdc6bb, 1 | BRF_PRG | BRF_ESS },
	{ "so1vera.2f",  0x100000, 0xc035b71b, 1 | BRF_PRG | BRF_ESS },
	SOULEDGE_COMMON_DATA_ROMS,
	{ "so1sprog.6d", 0x040000, 0xf6f682b7, 4 | BRF_PRG | BRF_ESS },
	{ "so1wave.8k",  0x400000, 0x0e68836b, 5 | BRF_SND }
};

STDROMPICKEXT(souledgeua, souledgeua, namcoc76)
STD_ROM_FN(souledgeua)

struct BurnDriver BurnDrvSouledgeua = {
	"souledgeua", "souledge", "namcoc76", NULL, "1995",
	"Soul Edge (US, SO3/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, souledgeuaRomInfo, souledgeuaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SouledgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Soul Edge (Japan, SO1/VER.A)

static struct BurnRomInfo souledgejaRomDesc[] = {
	{ "so1vera.2l",  0x100000, 0xbafb94c8, 1 | BRF_PRG | BRF_ESS },
	{ "so1vera.2j",  0x100000, 0xabe2d28e, 1 | BRF_PRG | BRF_ESS },
	{ "so1vera.2k",  0x100000, 0x29bdc6bb, 1 | BRF_PRG | BRF_ESS },
	{ "so1vera.2f",  0x100000, 0xc035b71b, 1 | BRF_PRG | BRF_ESS },
	SOULEDGE_COMMON_DATA_ROMS,
	{ "so1sprog.6d", 0x040000, 0xf6f682b7, 4 | BRF_PRG | BRF_ESS },
	{ "so1wave.8k",  0x400000, 0x0e68836b, 5 | BRF_SND }
};

STDROMPICKEXT(souledgeja, souledgeja, namcoc76)
STD_ROM_FN(souledgeja)

struct BurnDriver BurnDrvSouledgeja = {
	"souledgeja", "souledge", "namcoc76", NULL, "1995",
	"Soul Edge (Japan, SO1/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, souledgejaRomInfo, souledgejaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	SouledgeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

#undef SOULEDGE_COMMON_DATA_ROMS

#define DUNKMNIA_COMMON_ROMS \
	{ "dm1rom0l.ic5", 0x400000, 0x4bb5d71d, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dm1rom0u.ic6", 0x400000, 0xc16b47c5, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dm1rom1l.ic3", 0x400000, 0x20dd3294, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dm1rom1u.ic8", 0x400000, 0x01e905d3, 2 | BRF_PRG  | BRF_ESS }, \
	{ "c76.bin",      0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, \
	{ "dm1sprog.6d",  0x040000, 0xde1cbc78, 4 | BRF_PRG  | BRF_ESS }, \
	{ "dm1wave.8k",   0x400000, 0x883d7455, 5 | BRF_SND }

// Dunk Mania (World, DM2/VER.C)

static struct BurnRomInfo dunkmniaRomDesc[] = {
	{ "dm2verc.2l", 0x100000, 0xf6a6c46e, 1 | BRF_PRG | BRF_ESS },
	{ "dm2verc.2j", 0x100000, 0x1df539ce, 1 | BRF_PRG | BRF_ESS },
	{ "dm1verc.2k", 0x100000, 0xc8d72f78, 1 | BRF_PRG | BRF_ESS },
	{ "dm1verc.2f", 0x100000, 0xd379dfa9, 1 | BRF_PRG | BRF_ESS },
	DUNKMNIA_COMMON_ROMS
};

STDROMPICKEXT(dunkmnia, dunkmnia, namcoc76)
STD_ROM_FN(dunkmnia)

struct BurnDriver BurnDrvDunkmnia = {
	"dunkmnia", NULL, "namcoc76", NULL, "1995",
	"Dunk Mania (World, DM2/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, dunkmniaRomInfo, dunkmniaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	DunkmniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Dunk Mania (Japan, DM1/VER.C)

static struct BurnRomInfo dunkmniajcRomDesc[] = {
	{ "dm1verc.2l", 0x100000, 0x6c81654a, 1 | BRF_PRG | BRF_ESS },
	{ "dm1verc.2j", 0x100000, 0x10329b7e, 1 | BRF_PRG | BRF_ESS },
	{ "dm1verc.2k", 0x100000, 0xc8d72f78, 1 | BRF_PRG | BRF_ESS },
	{ "dm1verc.2f", 0x100000, 0xd379dfa9, 1 | BRF_PRG | BRF_ESS },
	DUNKMNIA_COMMON_ROMS
};

STDROMPICKEXT(dunkmniajc, dunkmniajc, namcoc76)
STD_ROM_FN(dunkmniajc)

struct BurnDriver BurnDrvDunkmniajc = {
	"dunkmniajc", "dunkmnia", "namcoc76", NULL, "1995",
	"Dunk Mania (Japan, DM1/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, dunkmniajcRomInfo, dunkmniajcRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	DunkmniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

#undef DUNKMNIA_COMMON_ROMS

#define XEVI3DG_COMMON_ROMS \
	{ "xv31vera.2k",    0x100000, 0x3d58138e, 1 | BRF_PRG  | BRF_ESS }, \
	{ "xv31vera.2f",    0x100000, 0x9e8780a2, 1 | BRF_PRG  | BRF_ESS }, \
	{ "xv31rom0l.ic5",  0x200000, 0x24e1e262, 2 | BRF_PRG  | BRF_ESS }, \
	{ "xv31rom0u.ic6",  0x200000, 0xcae38ef3, 2 | BRF_PRG  | BRF_ESS }, \
	{ "xv31rom1l.ic3",  0x200000, 0x46b4cb72, 2 | BRF_PRG  | BRF_ESS }, \
	{ "xv31rom1u.ic8",  0x200000, 0xbe0eb5d1, 2 | BRF_PRG  | BRF_ESS }, \
	{ "xv31rom2l.ic4",  0x200000, 0x8403a277, 2 | BRF_PRG  | BRF_ESS }, \
	{ "xv31rom2u.ic7",  0x200000, 0xecf70432, 2 | BRF_PRG  | BRF_ESS }, \
	{ "c76.bin",        0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }

// Xevious 3D/G (World, XV32/VER.B)

static struct BurnRomInfo xevi3dgRomDesc[] = {
	{ "xv32verb.2l",  0x100000, 0x6ffcceac, 1 | BRF_PRG | BRF_ESS },
	{ "xv32verb.2j",  0x100000, 0xc096dd18, 1 | BRF_PRG | BRF_ESS },
	XEVI3DG_COMMON_ROMS,
	{ "xv31sprog.6d", 0x040000, 0xe50b856a, 4 | BRF_PRG | BRF_ESS },
	{ "xv31wave.8k",  0x400000, 0x14f25ddd, 5 | BRF_SND }
};

STDROMPICKEXT(xevi3dg, xevi3dg, namcoc76)
STD_ROM_FN(xevi3dg)

struct BurnDriver BurnDrvXevi3dg = {
	"xevi3dg", NULL, "namcoc76", NULL, "1995",
	"Xevious 3D/G (World, XV32/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, xevi3dgRomInfo, xevi3dgRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Xevi3dgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Xevious 3D/G (World, XV32/VER.A)

static struct BurnRomInfo xevi3dgaRomDesc[] = {
	{ "xv32vera.2l",  0x100000, 0xbb5c0f1b, 1 | BRF_PRG | BRF_ESS },
	{ "xv32vera.2j",  0x100000, 0x21e20037, 1 | BRF_PRG | BRF_ESS },
	XEVI3DG_COMMON_ROMS,
	{ "xv31sprog.6d", 0x040000, 0x7e9fc6a0, 4 | BRF_PRG | BRF_ESS },
	{ "xv31wave.8k",  0x400000, 0x14f25ddd, 5 | BRF_SND }
};

STDROMPICKEXT(xevi3dga, xevi3dga, namcoc76)
STD_ROM_FN(xevi3dga)

struct BurnDriver BurnDrvXevi3dga = {
	"xevi3dga", "xevi3dg", "namcoc76", NULL, "1995",
	"Xevious 3D/G (World, XV32/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, xevi3dgaRomInfo, xevi3dgaRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Xevi3dgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Xevious 3D/G (Japan, XV31/VER.A)

static struct BurnRomInfo xevi3dgjRomDesc[] = {
	{ "xv31vera.2l",  0x100000, 0x419e0f13, 1 | BRF_PRG | BRF_ESS },
	{ "xv31vera.2j",  0x100000, 0xdf95373a, 1 | BRF_PRG | BRF_ESS },
	XEVI3DG_COMMON_ROMS,
	{ "xv31sprog.6d", 0x040000, 0xe50b856a, 4 | BRF_PRG | BRF_ESS },
	{ "xv31wave.8k",  0x400000, 0x14f25ddd, 5 | BRF_SND }
};

STDROMPICKEXT(xevi3dgj, xevi3dgj, namcoc76)
STD_ROM_FN(xevi3dgj)

struct BurnDriver BurnDrvXevi3dgj = {
	"xevi3dgj", "xevi3dg", "namcoc76", NULL, "1995",
	"Xevious 3D/G (Japan, XV31/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, xevi3dgjRomInfo, xevi3dgjRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	Xevi3dgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

#undef XEVI3DG_COMMON_ROMS

// Prime Goal EX (Japan, PG1/VER.A)

static struct BurnRomInfo primglexRomDesc[] = {
	{ "pg1vera.2l",   0x100000, 0xfc15fd1a, 1 | BRF_PRG  | BRF_ESS },
	{ "pg1vera.2j",   0x100000, 0x79955553, 1 | BRF_PRG  | BRF_ESS },
	{ "pg1rom0u.ic5", 0x400000, 0x2a503f2f, 2 | BRF_PRG  | BRF_ESS },
	{ "pg1rom0l.ic6", 0x400000, 0x54cef992, 2 | BRF_PRG  | BRF_ESS },
	{ "pg1rom1u.ic3", 0x400000, 0x1ee41152, 2 | BRF_PRG  | BRF_ESS },
	{ "pg1rom1l.ic8", 0x400000, 0x59b5a71c, 2 | BRF_PRG  | BRF_ESS },
	{ "c76.bin",      0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS },
	{ "pg1sprog.6d",  0x040000, 0xe7c3396d, 4 | BRF_PRG  | BRF_ESS },
	{ "pg1wave.8k",   0x400000, 0xfc9ad9eb, 5 | BRF_SND }
};

STDROMPICKEXT(primglex, primglex, namcoc76)
STD_ROM_FN(primglex)

struct BurnDriver BurnDrvPrimglex = {
	"primglex", NULL, "namcoc76", NULL, "1996",
	"Prime Goal EX (Japan, PG1/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, primglexRomInfo, primglexRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	PrimglexInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

#define DANCEYES_COMMON_ROMS \
	{ "dc1rom0l.ic5", 0x200000, 0x8b5b4b13, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dc1rom0u.ic6", 0x200000, 0x93ca9bd0, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dc1rom1l.ic3", 0x200000, 0x380e0282, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dc1rom1u.ic8", 0x200000, 0x47d966a7, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dc1rom2l.ic4", 0x200000, 0x8f130220, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dc1rom2u.ic7", 0x200000, 0x24514dc6, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dc1rom3l.ic1", 0x200000, 0xa76bcd4c, 2 | BRF_PRG  | BRF_ESS }, \
	{ "dc1rom3u.ic9", 0x200000, 0x1405d123, 2 | BRF_PRG  | BRF_ESS }, \
	{ "c76.bin",      0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, \
	{ "dc1sprog.6d",  0x040000, 0x96cd7788, 4 | BRF_PRG  | BRF_ESS }, \
	{ "dc1wave.8k",   0x400000, 0x8ba0f6a7, 5 | BRF_SND }

// Dancing Eyes (World, DC2/VER.B)

static struct BurnRomInfo danceyesRomDesc[] = {
	{ "dc2verb.2l", 0x100000, 0x9f605dd9, 1 | BRF_PRG | BRF_ESS },
	{ "dc2verb.2j", 0x100000, 0x210d7e22, 1 | BRF_PRG | BRF_ESS },
	{ "dc1vera.2k", 0x100000, 0xbdd9484e, 1 | BRF_PRG | BRF_ESS },
	{ "dc1vera.2f", 0x100000, 0x25a2f06f, 1 | BRF_PRG | BRF_ESS },
	DANCEYES_COMMON_ROMS
};

STDROMPICKEXT(danceyes, danceyes, namcoc76)
STD_ROM_FN(danceyes)

struct BurnDriver BurnDrvDanceyes = {
	"danceyes", NULL, "namcoc76", NULL, "1996",
	"Dancing Eyes (World, DC2/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, danceyesRomInfo, danceyesRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	DanceyesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Dancing Eyes (US, DC3/VER.C)

static struct BurnRomInfo danceyesuRomDesc[] = {
	{ "dc3verc.2l", 0x100000, 0xa7a00bc6, 1 | BRF_PRG | BRF_ESS },
	{ "dc3verc.2j", 0x100000, 0x02fc2415, 1 | BRF_PRG | BRF_ESS },
	{ "dc1vera.2k", 0x100000, 0xbdd9484e, 1 | BRF_PRG | BRF_ESS },
	{ "dc1vera.2f", 0x100000, 0x25a2f06f, 1 | BRF_PRG | BRF_ESS },
	DANCEYES_COMMON_ROMS
};

STDROMPICKEXT(danceyesu, danceyesu, namcoc76)
STD_ROM_FN(danceyesu)

struct BurnDriver BurnDrvDanceyesu = {
	"danceyesu", "danceyes", "namcoc76", NULL, "1996",
	"Dancing Eyes (US, DC3/VER.C)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, danceyesuRomInfo, danceyesuRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	DanceyesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

// Dancing Eyes (Japan, DC1/VER.A)

static struct BurnRomInfo danceyesjRomDesc[] = {
	{ "dc1vera.2l", 0x100000, 0xb164ad67, 1 | BRF_PRG | BRF_ESS },
	{ "dc1vera.2j", 0x100000, 0x28e4cb3d, 1 | BRF_PRG | BRF_ESS },
	{ "dc1vera.2k", 0x100000, 0xbdd9484e, 1 | BRF_PRG | BRF_ESS },
	{ "dc1vera.2f", 0x100000, 0x25a2f06f, 1 | BRF_PRG | BRF_ESS },
	DANCEYES_COMMON_ROMS
};

STDROMPICKEXT(danceyesj, danceyesj, namcoc76)
STD_ROM_FN(danceyesj)

struct BurnDriver BurnDrvDanceyesj = {
	"danceyesj", "danceyes", "namcoc76", NULL, "1996",
	"Dancing Eyes (Japan, DC1/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_ACTION, 0,
	NULL, danceyesjRomInfo, danceyesjRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	DanceyesInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	640, 240, 4, 3
};

#undef DANCEYES_COMMON_ROMS

// Pocket Racer (Japan, PKR1/VER.B)

static struct BurnRomInfo pocketrcRomDesc[] = {
	{ "pkr1verb.2l",   0x100000, 0x300d906a, 1 | BRF_PRG  | BRF_ESS },
	{ "pkr1verb.2j",   0x100000, 0xd5f47526, 1 | BRF_PRG  | BRF_ESS },
	{ "pkr1rom0l.ic5", 0x200000, 0x6c9b074c, 2 | BRF_PRG  | BRF_ESS },
	{ "pkr1rom0u.ic6", 0x200000, 0xa55c0906, 2 | BRF_PRG  | BRF_ESS },
	{ "c76.bin",       0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS },
	{ "pkr1verb.6d",   0x040000, 0x9bf08992, 4 | BRF_PRG  | BRF_ESS },
	{ "pkr1wave.8k",   0x400000, 0x72517c46, 5 | BRF_SND }
};

STDROMPICKEXT(pocketrc, pocketrc, namcoc76)
STD_ROM_FN(pocketrc)

struct BurnDriver BurnDrvPocketrc = {
	"pocketrc", NULL, "namcoc76", NULL, "1996",
	"Pocket Racer (Japan, PKR1/VER.B)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_POST90S, GBF_RACING, 0,
	NULL, pocketrcRomInfo, pocketrcRomName, NULL, NULL, NULL, NULL, PocketrcInputInfo, PocketrcDIPInfo,
	PocketrcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

// Star Sweep (World, STP2/VER.A)

static struct BurnRomInfo starswepRomDesc[] = {
	{ "stp2vera.2l", 0x100000, 0xa03f4bac, 1 | BRF_PRG  | BRF_ESS },
	{ "stp2vera.2j", 0x100000, 0x590da032, 1 | BRF_PRG  | BRF_ESS },
	{ "stp2vera.2k", 0x100000, 0xac8717d5, 1 | BRF_PRG  | BRF_ESS },
	{ "stp2vera.2f", 0x100000, 0x42733309, 1 | BRF_PRG  | BRF_ESS },
	{ "c76.bin",     0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS },
	{ "stp2vera.6d", 0x040000, 0x08aaaf6a, 4 | BRF_PRG  | BRF_ESS },
	{ "stp1wave.8k", 0x400000, 0x18f30e92, 5 | BRF_SND }
};

STDROMPICKEXT(starswep, starswep, namcoc76)
STD_ROM_FN(starswep)

struct BurnDriver BurnDrvStarswep = {
	"starswep", NULL, "namcoc76", NULL, "1997",
	"Star Sweep (World, STP2/VER.A)\0", NULL, "Axela / Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, starswepRomInfo, starswepRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	StarswepInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

// Star Sweep (Japan, STP1/VER.A)

static struct BurnRomInfo starswepjRomDesc[] = {
	{ "stp1vera.1j",  0x200000, 0xef83e126, 1 | BRF_PRG  | BRF_ESS },
	{ "stp1vera.1l",  0x200000, 0x0ee7fe1e, 1 | BRF_PRG  | BRF_ESS },
	{ "c76.bin",      0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS },
	{ "stp1sprog.7e", 0x040000, 0x08aaaf6a, 4 | BRF_PRG  | BRF_ESS },
	{ "stp1wave.8k",  0x400000, 0x18f30e92, 5 | BRF_SND }
};

STDROMPICKEXT(starswepj, starswepj, namcoc76)
STD_ROM_FN(starswepj)

struct BurnDriver BurnDrvStarswepj = {
	"starswepj", "starswep", "namcoc76", NULL, "1997",
	"Star Sweep (Japan, STP1/VER.A)\0", NULL, "Axela / Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, starswepjRomInfo, starswepjRomName, NULL, NULL, NULL, NULL, Namcos11InputInfo, Namcos11DIPInfo,
	StarswepjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

// Kosodate Quiz My Angel 3 (Japan, KQT1/VER.A)

static struct BurnRomInfo myangel3RomDesc[] = {
	{ "kqt1vera.1j",    0x200000, 0xdf7aef8a, 1 | BRF_PRG  | BRF_ESS },
	{ "kqt1vera.1l",    0x200000, 0xffc51c01, 1 | BRF_PRG  | BRF_ESS },

	{ "kqt1prg0l.ic2",  0x800000, 0xd67eee66, 2 | BRF_PRG  | BRF_ESS },
	{ "kqt1prg0u.ic5",  0x800000, 0x4d1c7bf3, 2 | BRF_PRG  | BRF_ESS },
	{ "kqt1prg1l.ic3",  0x800000, 0x298d8eeb, 2 | BRF_PRG  | BRF_ESS },
	{ "kqt1prg1u.ic6",  0x800000, 0x911783db, 2 | BRF_PRG  | BRF_ESS },

	{ "c76.bin",        0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS },
	{ "kqt1sprog.7e",   0x040000, 0xbb1888a6, 4 | BRF_PRG  | BRF_ESS },
	{ "kqt1wave.8k",    0x400000, 0x92ca8e4f, 5 | BRF_SND },
};

STDROMPICKEXT(myangel3, myangel3, namcoc76)
STD_ROM_FN(myangel3)

struct BurnDriver BurnDrvMyangel3 = {
	"myangel3", NULL, "namcoc76", NULL, "1998",
	"Kosodate Quiz My Angel 3 (Japan, KQT1/VER.A)\0", NULL, "MOSS / Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, myangel3RomInfo, myangel3RomName, NULL, NULL, NULL, NULL, Myangel3InputInfo, Myangel3DIPInfo,
	Myangel3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

#define PTBLANK2_DATA_ROMS \
	{ "gnb2prg.1",   0x400000, 0x8a8e77c3, 2 | BRF_PRG  | BRF_ESS }, \
	{ "gnb2prg.2",   0x400000, 0x563edc3f, 2 | BRF_PRG  | BRF_ESS }, \
	{ "gnb2prg.3",   0x400000, 0x94fbe733, 2 | BRF_PRG  | BRF_ESS }, \
	{ "gnb2prg.4",   0x400000, 0x1cbe79a6, 2 | BRF_PRG  | BRF_ESS }

#define PTBLANK2_SOUND_ROMS \
	{ "c76.bin",     0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS }, \
	{ "gnb1vera.6d", 0x040000, 0x6461ae77, 4 | BRF_PRG  | BRF_ESS }, \
	{ "gnb1wave.8k", 0x400000, 0x4e19d9d6, 5 | BRF_SND }

// Point Blank 2 (World, GNB2/VER.A)

static struct BurnRomInfo ptblank2aRomDesc[] = {
	{ "gnb2vera.2l", 0x100000, 0x4926599d, 1 | BRF_PRG | BRF_ESS },
	{ "gnb2vera.2j", 0x100000, 0x2aba8c09, 1 | BRF_PRG | BRF_ESS },
	{ "gnb1vera.2k", 0x100000, 0xe6335e4e, 1 | BRF_PRG | BRF_ESS },
	{ "gnb1vera.2f", 0x100000, 0x2bb7eb6d, 1 | BRF_PRG | BRF_ESS },
	PTBLANK2_DATA_ROMS,
	PTBLANK2_SOUND_ROMS
};

STDROMPICKEXT(ptblank2a, ptblank2a, namcoc76)
STD_ROM_FN(ptblank2a)

struct BurnDriver BurnDrvPtblank2a = {
	"ptblank2a", "ptblank2", "namcoc76", NULL, "1999",
	"Point Blank 2 (World, GNB2/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ptblank2aRomInfo, ptblank2aRomName, NULL, NULL, NULL, NULL, Ptblank2InputInfo, Ptblank2DIPInfo,
	Ptblank2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

// Point Blank 2 (World, GNB2/VER.A alt)

static struct BurnRomInfo ptblank2bRomDesc[] = {
	{ "gnb2vera.1j", 0x200000, 0xdf0bc5f0, 1 | BRF_PRG  | BRF_ESS },
	{ "gnb2vera.1l", 0x200000, 0x8a274d96, 1 | BRF_PRG  | BRF_ESS },
	PTBLANK2_DATA_ROMS,
	{ "c76.bin",     0x004000, 0x399faac7, 3 | BRF_BIOS | BRF_PRG | BRF_ESS },
	{ "gnb1vera.7e", 0x040000, 0x6461ae77, 4 | BRF_PRG  | BRF_ESS },
	{ "gnb1wave.8k", 0x400000, 0x4e19d9d6, 5 | BRF_SND }
};

STDROMPICKEXT(ptblank2b, ptblank2b, namcoc76)
STD_ROM_FN(ptblank2b)

struct BurnDriver BurnDrvPtblank2b = {
	"ptblank2b", "ptblank2", "namcoc76", NULL, "1999",
	"Point Blank 2 (World, GNB2/VER.A alt)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ptblank2bRomInfo, ptblank2bRomName, NULL, NULL, NULL, NULL, Ptblank2InputInfo, Ptblank2DIPInfo,
	Ptblank2bInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

// Point Blank 2 (World, unknown version)

static struct BurnRomInfo ptblank2cRomDesc[] = {
	{ "gnb2verx.2l", 0x100000, 0xc2a02ccf, 1 | BRF_PRG  | BRF_ESS },
	{ "gnb2verx.2j", 0x100000, 0x96abd746, 1 | BRF_PRG  | BRF_ESS },
	{ "gnb1vera.2k", 0x100000, 0xe6335e4e, 1 | BRF_PRG  | BRF_ESS },
	{ "gnb1vera.2f", 0x100000, 0x2bb7eb6d, 1 | BRF_PRG  | BRF_ESS },
	PTBLANK2_DATA_ROMS,
	PTBLANK2_SOUND_ROMS
};

STDROMPICKEXT(ptblank2c, ptblank2c, namcoc76)
STD_ROM_FN(ptblank2c)

struct BurnDriver BurnDrvPtblank2c = {
	"ptblank2c", "ptblank2", "namcoc76", NULL, "1999",
	"Point Blank 2 (World, unknown version)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ptblank2cRomInfo, ptblank2cRomName, NULL, NULL, NULL, NULL, Ptblank2InputInfo, Ptblank2DIPInfo,
	Ptblank2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

#define PTBLANK2_64_DATA_ROMS \
	{ "gnb1prg0l.ic2", 0x800000, 0x78746037, 2 | BRF_PRG | BRF_ESS }, \
	{ "gnb1prg0u.ic5", 0x800000, 0x697d3279, 2 | BRF_PRG | BRF_ESS }

// Point Blank 2 (US, GNB3/VER.A)

static struct BurnRomInfo ptblank2uaRomDesc[] = {
	{ "gnb3vera.2l", 0x100000, 0x57ad719a, 1 | BRF_PRG | BRF_ESS },
	{ "gnb3vera.2j", 0x100000, 0x0378af98, 1 | BRF_PRG | BRF_ESS },
	{ "gnb1vera.2k", 0x100000, 0xe6335e4e, 1 | BRF_PRG | BRF_ESS },
	{ "gnb1vera.2f", 0x100000, 0x2bb7eb6d, 1 | BRF_PRG | BRF_ESS },
	PTBLANK2_64_DATA_ROMS,
	PTBLANK2_SOUND_ROMS
};

STDROMPICKEXT(ptblank2ua, ptblank2ua, namcoc76)
STD_ROM_FN(ptblank2ua)

struct BurnDriver BurnDrvPtblank2ua = {
	"ptblank2ua", "ptblank2", "namcoc76", NULL, "1999",
	"Point Blank 2 (US, GNB3/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ptblank2uaRomInfo, ptblank2uaRomName, NULL, NULL, NULL, NULL, Ptblank2InputInfo, Ptblank2DIPInfo,
	Ptblank2uaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

// Gunbarl (Japan, GNB1/VER.A)

static struct BurnRomInfo gunbarlaRomDesc[] = {
	{ "gnb1vera.2l", 0x100000, 0x405e2585, 1 | BRF_PRG | BRF_ESS },
	{ "gnb1vera.2j", 0x100000, 0x2d2af8cf, 1 | BRF_PRG | BRF_ESS },
	{ "gnb1vera.2k", 0x100000, 0xe6335e4e, 1 | BRF_PRG | BRF_ESS },
	{ "gnb1vera.2f", 0x100000, 0x2bb7eb6d, 1 | BRF_PRG | BRF_ESS },
	PTBLANK2_64_DATA_ROMS,
	PTBLANK2_SOUND_ROMS
};

STDROMPICKEXT(gunbarla, gunbarla, namcoc76)
STD_ROM_FN(gunbarla)

struct BurnDriver BurnDrvGunbarla = {
	"gunbarla", "ptblank2", "namcoc76", NULL, "1999",
	"Gunbarl (Japan, GNB1/VER.A)\0", NULL, "Namco", "Namco System 11",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, gunbarlaRomInfo, gunbarlaRomName, NULL, NULL, NULL, NULL, Ptblank2InputInfo, Ptblank2DIPInfo,
	Ptblank2uaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};

#undef PTBLANK2_64_DATA_ROMS
#undef PTBLANK2_SOUND_ROMS
#undef PTBLANK2_DATA_ROMS
