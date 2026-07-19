#ifndef FBNEO_PSX_INTF_H
#define FBNEO_PSX_INTF_H

#include "burnint.h"
#include <stdint.h>

typedef UINT8  (*pPsxReadByteHandler)(UINT32 address);
typedef UINT16 (*pPsxReadHalfHandler)(UINT32 address);
typedef UINT32 (*pPsxReadWordHandler)(UINT32 address);
typedef void   (*pPsxWriteByteHandler)(UINT32 address, UINT8 data);
typedef void   (*pPsxWriteHalfHandler)(UINT32 address, UINT16 data);
typedef void   (*pPsxWriteWordHandler)(UINT32 address, UINT32 data);
typedef void   (*pPsxInstructionHook)(UINT32 pc);

INT32 PsxInit();
void PsxExit();
void PsxReset();
INT32 PsxRun(INT32 cycles);
INT32 PsxGetRemainingCycles();
UINT32 PsxGetPC();
UINT32 PsxGetNextPC();
UINT32 PsxGetOpcode();
UINT32 PsxGetReg(INT32 reg);
void PsxSetReg(INT32 reg, UINT32 data);
UINT32 PsxGetCP0(INT32 reg);
UINT32 PsxGetBIU();
UINT8 *PsxGetDCache();
void PsxSetBIU(UINT32 data);
void PsxSetInstructionHook(pPsxInstructionHook hook);
void PsxSetExternalInterruptGteCompletion(INT32 enable);
void PsxBusError();
void PsxSetIRQLine(INT32 line, INT32 state);
void PsxScan(INT32 nAction);

INT32 PsxMapMemory(UINT8 *memory, UINT32 start, UINT32 end, INT32 type);
INT32 PsxMapHandler(uintptr_t handler, UINT32 start, UINT32 end, INT32 type);

INT32 PsxSetReadByteHandler(INT32 i, pPsxReadByteHandler handler);
INT32 PsxSetReadHalfHandler(INT32 i, pPsxReadHalfHandler handler);
INT32 PsxSetReadWordHandler(INT32 i, pPsxReadWordHandler handler);
INT32 PsxSetWriteByteHandler(INT32 i, pPsxWriteByteHandler handler);
INT32 PsxSetWriteHalfHandler(INT32 i, pPsxWriteHalfHandler handler);
INT32 PsxSetWriteWordHandler(INT32 i, pPsxWriteWordHandler handler);

#endif
