#pragma once

#include "burnint.h"

typedef UINT8 (__fastcall *h83002_read8_handler)(UINT32 address);
typedef void (__fastcall *h83002_write8_handler)(UINT32 address, UINT8 data);

void H83002Init(h83002_read8_handler read, h83002_write8_handler write);
void H83002Exit();
void H83002Reset();
INT32 H83002Run(INT32 cycles);
void H83002SetIRQLine(INT32 line, INT32 state);
INT32 H83002Scan(INT32 nAction);

