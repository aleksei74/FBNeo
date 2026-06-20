// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    tmsops.cpp

    TMS57002 "DASP" emulator.

***************************************************************************/

#include "burnint.h"
#include "tms57002.h"
#include <algorithm>
#include <stdint.h>

// Kept separate from tms57002.cpp to avoid the optimizer eating all the memory

#define CINTRP
#include "tms57002.hxx"
#undef CINTRP
