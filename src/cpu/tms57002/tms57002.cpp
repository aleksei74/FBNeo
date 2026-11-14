// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    tms57002.cpp

    TMS57002 "DASP" emulator, adapted for FB Neo.

***************************************************************************/

#include "burnint.h"
#include "tms57002.h"
#include <string.h>

tms57002_device::tms57002_device()
{
	memset(this, 0, sizeof(*this));
	sti = S_IDLE;
}

void tms57002_device::init(UINT8 *ram)
{
	data_ram = ram;
	sti = S_IDLE;
	reset();
}

void tms57002_device::pload_w(int state)
{
	u8 olds = sti;
	if (state)
		sti &= ~IN_PLOAD;
	else
		sti |= IN_PLOAD;
	if (olds ^ sti)
	{
		if (sti & IN_PLOAD)
		{
			hidx = 0;
			pc = 0;
			ca = 0;
			sti &= ~(SU_MASK);
		}
	}
}

void tms57002_device::cload_w(int state)
{
	u8 olds = sti;
	if (state)
		sti &= ~IN_CLOAD;
	else
		sti |= IN_CLOAD;
	if (olds ^ sti)
	{
		if (sti & IN_CLOAD)
		{
			hidx = 0;
			//ca = 0; // Seems extremely dubious
		}
	}
}

void tms57002_device::reset()
{
	sti = (sti & ~(SU_MASK|S_READ|S_WRITE|S_BRANCH|S_HOST|S_UPDATE)) | (SU_ST0|S_IDLE);
	pc = 0;
	ca = 0;
	hidx = 0;
	id = 0;
	ba0 = 0;
	ba1 = 0;
	sa = 0;
	rptc = 0;
	rptc_next = 0;
	update_counter_tail = 0;
	update_counter_head = 0;
	st0 &= ~(ST0_INCS | ST0_DIRI | ST0_FI | ST0_SIM | ST0_PLRI |
			ST0_PBCI | ST0_DIRO | ST0_FO | ST0_SOM | ST0_PLRO |
			ST0_PBCO | ST0_CNS);
	st1 &= ~(ST1_AOV | ST1_SFAI | ST1_SFAO | ST1_AOVM | ST1_MOVM | ST1_MOV |
			ST1_SFMA | ST1_SFMO | ST1_RND | ST1_CRM | ST1_DBP);
	update_dready();
	update_pc0();
	update_empty();

	xba = 0;
	xoa = 0;
	cache_flush();
}

void tms57002_device::data_w(u8 data)
{
	switch (sti & (IN_PLOAD|IN_CLOAD))
	{
	case 0:
		hidx = 0;
		sti &= ~SU_CVAL;
		break;
	case IN_PLOAD:
		host[hidx++] = data;
		if (hidx >= 3)
		{
			u32 val = (host[0]<<16) | (host[1]<<8) | host[2];
			hidx = 0;

			switch (sti & SU_MASK)
			{
			case SU_ST0:
				st0 = val;
				sti = (sti & ~SU_MASK) | SU_ST1;
				break;
			case SU_ST1:
				st1 = val;
				sti = (sti & ~SU_MASK) | SU_PRG;
				break;
			case SU_PRG:
				program_ram[pc++] = val;
				update_pc0();
				break;
			}
		}
		break;
	case IN_CLOAD:
		if (sti & SU_CVAL)
		{
			host[hidx++] = data;
			if (hidx >= 4)
			{
				u32 val = (host[0]<<24) | (host[1]<<16) | (host[2]<<8) | host[3];
				sti &= ~SU_CVAL;
				update[update_counter_head] = val;
				update_counter_head = (update_counter_head + 1) & 0x0f;
				hidx = 1; // the write shouldn't really happen until CLOAD is high though
				update_empty();
			}
		}
		else
		{
			sa = data;
			hidx = 0;
			sti |= SU_CVAL;
		}

		break;
	case IN_PLOAD|IN_CLOAD:
		host[hidx++] = data;
		if (hidx >= 4)
		{
			u32 val = (host[0]<<24) | (host[1]<<16) | (host[2]<<8) | host[3];
			hidx = 0;
			cmem[ca++] = val;
		}
		break;
	};
}

u8 tms57002_device::data_r()
{
	u8 res;
	if (!(sti & S_HOST))
		return 0xff;

	res = host[hidx];
	hidx++;
	if (hidx == 4)
	{
		hidx = 0;
		sti &= ~S_HOST;
		update_dready();
	}

	return res;
}

int tms57002_device::dready_r()
{
	return sti & S_HOST ? 0 : 1;
}

void tms57002_device::update_dready()
{
}

int tms57002_device::pc0_r()
{
	return pc == 0 ? 0 : 1;
}

int tms57002_device::empty_r()
{
	return (update_counter_head == update_counter_tail);
}

void tms57002_device::update_empty()
{
}

void tms57002_device::sync_w(int state)
{
	if (sti & (IN_PLOAD /*| IN_CLOAD*/))
		return;

	allow_update = 1;
	pc = 0;
	ca = 0;
	id = 0;
	if (!(st0 & ST0_INCS))
	{
		ba0--;
		ba1++;
	}
	xba = (xba-1) & 0x7ffff;
	st1 &= ~(ST1_AOV | ST1_MOV);
	sti &= ~S_IDLE;
}

void tms57002_device::cache_flush()
{
	int i;
	cache.hused = cache.iused = 0;
	for (i = 0; i != 256; i++)
		cache.hashbase[i] = -1;
	for (i = 0; i != HBS; i++)
	{
		cache.hashnode[i].st1 = 0;
		cache.hashnode[i].ipc = -1;
		cache.hashnode[i].next = -1;
	}
	for (i = 0; i != IBS; i++)
	{
		cache.inst[i].op = 0;
		cache.inst[i].next = -1;
		cache.inst[i].param = 0;
	}
}

void tms57002_device::add_one(cstate *cs, u16 op, u8 param)
{
	s16 ipc = cache.iused++;
	cache.inst[ipc].op = op;
	cache.inst[ipc].param = param;
	cache.inst[ipc].next = -1;
	if (cs->ipc != -1)
		cache.inst[cs->ipc].next = ipc;
	cs->ipc = ipc;
	if (cs->hnode != -1)
	{
		cache.hashnode[cs->hnode].ipc = ipc;
		cs->hnode = -1;
	}
}

void tms57002_device::decode_one(u32 opcode, cstate *cs, void (tms57002_device::*dec)(u32 opcode, u16 *op, cstate *cs))
{
	u16 op = 0;
	(this->*dec)(opcode, &op, cs);
	if (!op)
		return;
	add_one(cs, op, opcode & 0xff);
}

s16 tms57002_device::get_hash(u8 adr, u32 st1, s16 *pnode)
{
	s16 hnode;
	st1 &= ST1_CACHE;
	*pnode = -1;
	hnode = cache.hashbase[adr];
	while(hnode != -1)
	{
		if (cache.hashnode[hnode].st1 == st1)
			return cache.hashnode[hnode].ipc;
		*pnode = hnode;
		hnode = cache.hashnode[hnode].next;
	}
	return -1;
}

s16 tms57002_device::get_hashnode(u8 adr, u32 st1, s16 pnode)
{
	s16 hnode = cache.hused++;
	cache.hashnode[hnode].st1 = st1 & ST1_CACHE;
	cache.hashnode[hnode].ipc = -1;
	cache.hashnode[hnode].next = -1;
	if (pnode == -1)
		cache.hashbase[adr] = hnode;
	else
		cache.hashnode[pnode].next = hnode;
	return hnode;
}

int tms57002_device::decode_get_pc()
{
	s16 pnode, res;
	cstate cs;
	u8 adr = pc;

	res = get_hash(adr, st1, &pnode);
	if (res != -1)
		return res;

	if (HBS - cache.hused < 256 || IBS - cache.iused < 256*3)
	{
		cache_flush();
		pnode = -1;
	}

	cs.hnode = res = get_hashnode(adr, st1, pnode);
	cs.ipc = -1;
	cs.branch = 0;

	for (;;)
	{
		s16 ipc;
		u32 opcode = program_ram[adr];

		cs.inc = 0;

		if ((opcode & 0xfc0000) == 0xfc0000)
			decode_one(opcode, &cs, &tms57002_device::decode_cat3);
		else {
			decode_one(opcode, &cs, &tms57002_device::decode_cat2_pre);
			decode_one(opcode, &cs, &tms57002_device::decode_cat1);
			decode_one(opcode, &cs, &tms57002_device::decode_cat2_post);
		}
		add_one(&cs, cs.inc, 0);

		if (cs.branch)
			break;

		adr++;
		ipc = get_hash(adr, st1, &pnode);
		if (ipc != -1)
		{
			cache.inst[cs.ipc].next = ipc;
			break;
		}
		cs.hnode = get_hashnode(adr, st1, pnode);
	}

	return cache.hashnode[res].ipc;
}

void tms57002_device::process_sample(const INT16 *input, INT16 *output, INT32 cycles)
{
	INT32 in_scale = (st0 & ST0_SIM) ? 256 : 1;

	for (INT32 i = 0; i < 4; i++) {
		si[i] = (INT32(input[i]) * in_scale) & 0xffffff;
	}

	for (INT32 i = 0; i < 4; i++) {
		INT32 sample = INT32(so[i] << 8) >> 16;
		output[i] = BURN_SND_CLIP(sample);
	}

	sync_w(1);
	execute_run(cycles);
}

void tms57002_device::scan(INT32 nAction)
{
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}

	SCAN_VAR(macc);
	SCAN_VAR(macc_read);
	SCAN_VAR(macc_write);
	SCAN_VAR(cmem);
	SCAN_VAR(dmem0);
	SCAN_VAR(dmem1);
	SCAN_VAR(si);
	SCAN_VAR(so);
	SCAN_VAR(st0);
	SCAN_VAR(st1);
	SCAN_VAR(sti);
	SCAN_VAR(aacc);
	SCAN_VAR(xoa);
	SCAN_VAR(xba);
	SCAN_VAR(xwr);
	SCAN_VAR(xrd);
	SCAN_VAR(txrd);
	SCAN_VAR(creg);
	SCAN_VAR(pc);
	SCAN_VAR(hpc);
	SCAN_VAR(ca);
	SCAN_VAR(id);
	SCAN_VAR(ba0);
	SCAN_VAR(ba1);
	SCAN_VAR(rptc);
	SCAN_VAR(rptc_next);
	SCAN_VAR(sa);
	SCAN_VAR(xm_adr);
	SCAN_VAR(host);
	SCAN_VAR(hidx);
	SCAN_VAR(allow_update);
	SCAN_VAR(update);
	SCAN_VAR(update_counter_head);
	SCAN_VAR(update_counter_tail);
	SCAN_VAR(program_ram);
	SCAN_VAR(icount);
	SCAN_VAR(unsupported_inst_warning);

	if (nAction & ACB_WRITE) {
		cache_flush();
		update_dready();
		update_pc0();
		update_empty();
	}
}
