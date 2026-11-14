// SPDX-License-Identifier: GPL-3.0-or-later
// Konami 056734 instruction-level model, adapted for FBNeo on 2026-11-04.
// Behavioural references: Paul Priest's Arcade-KonamiGX_MiSTer,
// rtl/esc/k056734.sv and scripts/k056734/{chip,escemu,esccipher}.py.
// See gx_esc_provenance.md and gx_esc_LICENSE.txt for attribution/licence.
// Runs ROM firmware, not per-game sprite synthesis. The FPGA's boot-loader
// shortcut is retained; host bus/FPGA clock edges are not cycle-emulated here.
#ifndef FBNEO_GX_ESC_CHIP_H
#define FBNEO_GX_ESC_CHIP_H
#include <stdint.h>
#include <string.h>
class GxEscChip
{
  public:
	typedef uint32_t (*Read)(uint32_t, unsigned);
	typedef void (*Write)(uint32_t, unsigned, uint32_t);
	struct State {
		uint32_t local[4096], r[64];
		uint16_t pc;
		uint8_t z, n, c, v, valid, fault, irq;
		uint64_t steps;
	} state;
	GxEscChip() : read(0), write(0), dxor(0), lanes(0)
	{
		memset(&state, 0, sizeof(state));
	}
	bool Reset(Read rd, Write wr, uint16_t s10, unsigned nib, uint32_t x, uint64_t map)
	{
		read = rd;
		write = wr;
		dxor = x;
		lanes = map;
		memset(&state, 0, sizeof(state));
		state.r[42] = s10;
		state.r[43] = (nib << 24) | 0x200100;
		if (read(0x200a6c, 4) != 0xfef724fb)
			return false;
		unsigned a = read(0x200a70, 2) ^ 0x3c96, b = read(0x200a72, 2) ^ 0xb9e5, c = read(0x200a74, 2) ^ 0x67a5;
		if (!a || a + b > 4096)
			return false;
		const uint32_t key = (nib * 0x01010101U) ^ 0x36ae2592U ^ (s10 * 0x00010001U);
		uint32_t ka = key ^ 0x6e8ef8fcU, kb = key, checksum = 0;
		for (unsigned i = 0; i < a + b; i++) {
			if (i == a) {
				ka = key ^ 0x6e8ef8fcU;
				kb = key;
			}
			const uint32_t w = read(0x200a84 + 4 * i, 4) ^ ka;
			state.local[i] = w;
			checksum += (w >> 16) + (w & 0xffff);
			ka += kb;
			kb ^= (ka >> 16) | (ka << 16);
		}
		if ((checksum & 0xffff) != read(0x200a7e, 2))
			return false;
		state.r[3] = a;
		state.r[2] = a + b;
		state.r[24] = (a + b + c + 4) & ~3U;
		state.r[25] = state.r[33] = 0x2000;
		state.valid = 1;
		return RunUntilIdle();
	}
	bool Post(uint32_t pointer)
	{
		state.r[39] = pointer & 0xffffff;
		return RunUntilIdle();
	}
	bool TakeIrq()
	{
		bool irq = state.irq != 0;
		state.irq = 0;
		return irq;
	}
	uint32_t Decode(uint32_t w) const
	{
		uint32_t x = w ^ dxor, out = 0;
		for (unsigned k = 0; k < 16; k++)
			out |= ((x >> (2 * ((lanes >> (4 * k)) & 15))) & 3) << (2 * k);
		return out;
	}
	bool Step()
	{
		const uint16_t pc = state.pc;
		const uint32_t w = Decode(state.local[pc & 4095]);
		state.pc++;
		state.steps++;
		if (!w)
			return true;
		unsigned cls = L(w, 0), sz = L(w, 7), a = Pack(w, 5, 15, 9), b = Pack(w, 2, 1, 4);
		unsigned off10 = (L(w, 6) << 8) | (L(w, 14) << 6) | (L(w, 13) << 4) | (L(w, 12) << 2) | L(w, 11);
		int off = (off10 & 512) ? (int)off10 - 1024 : (int)off10;
		unsigned imm = (b << 10) | off10, c = (off10 >> 3) & 63;
		unsigned o10 = L(w, 10), o8 = L(w, 8), o3 = L(w, 3), op = (o10 << 4) | (o8 << 2) | o3;
		unsigned d = ((o8 & 1) << 4) | (o3 << 2) | ((o10 >> 1) << 1) | (1 - (o10 & 1));
		uint32_t va = Get(a, pc), vb = Get(b, pc), vc = Get(c, pc);
		unsigned bits = sz == 1 ? 8 : sz == 2 ? 16 : 32, nb = bits / 8;
		uint32_t mask = bits == 32 ? 0xffffffffU : (1U << bits) - 1;
		if (cls == 0) {
			if (L(w, 5) == 2 && sz == 2 && L(w, 9) == 2 && L(w, 15) == 0) {
				bool take = false;
				switch (op) {
					case 0x10:
					case 0x14:
						take = true;
						break;
					case 0x20:
						take = state.z;
						break;
					case 0x30:
						take = !state.z;
						break;
					case 1:
						take = state.n;
						break;
					case 0x11:
						take = !state.n;
						break;
					case 0x21:
						take = state.c;
						break;
					case 0x31:
						take = !state.c;
						break;
					case 0x12:
						take = !state.z && state.n == state.v;
						break;
					case 0x32:
						take = !state.c && !state.z;
						break;
					case 3:
						take = state.n != state.v;
						break;
				}
				if (take) {
					if (op == 0x14)
						Push(state.pc);
					state.pc = (uint16_t)(state.pc + (int16_t)imm);
				}
				return true;
			}
			if (o10 == 1 && sz == 2 && o3 == 0 && o8 < 2) {
				if (o8)
					Push(state.pc);
				state.pc = (uint16_t)(va + (o8 ? 0 : off));
				return true;
			}
			Put(d, (sz << 22) | (a << 16) | imm);
			return true;
		}
		if (cls == 1) {
			uint32_t addr = (sz << 22) | (a << 16) | imm;
			if (o8 & 2)
				Hw(addr, 4, Get(d, pc));
			else
				Put(d, Hr(addr, 4));
			return true;
		}
		if (cls == 3) {
			uint32_t result = 0;
			if (op == 0x18 || op == 0x19 || op == 0x1a || op == 0x1b || op == 0x39 || op == 0x3b) {
				result = Alu(op, vb, vc, mask, bits);
				Put(a, (vb & ~mask) | (result & mask));
				return true;
			}
			if (op == 0x1c || op == 0x1d || op == 0x1e || op == 0x1f || op == 0x3d) {
				result = Alu(op - 4, va, imm, mask, bits);
				Put(a, (va & ~mask) | (result & mask));
				return true;
			}
			uint32_t ea = vb + off, &lv = state.local[ea & 4095];
			switch (op) {
				case 0x23:
					Put(a, Hr(ea, nb));
					return true;
				case 0x33:
					Hw(ea, nb, va & mask);
					return true;
				case 0x27:
					Put(a, lv & mask);
					return true;
				case 0x37:
					lv = (lv & ~mask) | (va & mask);
					return true;
				case 0x05:
					Flags(va - lv, mask, bits, va, lv, true, true);
					return true;
				case 0x04:
				case 0x06:
				case 0x07:
				case 0x25:
					result = Alu(op == 0x04 ? 0x18 : op == 0x06 ? 0x1a : op == 0x07 ? 0x1b : 0x39, va, lv, mask, bits);
					Put(a, (va & ~mask) | (result & mask));
					return true;
				case 0x14:
				case 0x15:
				case 0x16:
				case 0x17:
					result = Alu(op == 0x14 ? 0x18 : op == 0x15 ? 0x19 : op == 0x16 ? 0x1a : 0x1b, lv, va, mask, bits);
					lv = (lv & ~mask) | (result & mask);
					return true;
				case 1: {
					uint32_t v = Hr(ea, nb);
					Flags(va - v, mask, bits, va, v, true, true);
					return true;
				}
				case 0x11: {
					uint32_t v = Hr(ea, nb);
					Flags(v - va, mask, bits, v, va, true, true);
					return true;
				}
			}
		}
		if (cls == 2) {
			unsigned p = (o10 << 6) | (o8 << 4) | (sz << 2) | o3;
			switch (p) {
				case 0x50:
					Push(va);
					return true;
				case 0xd0:
					Put(a, Pop());
					return true;
				case 0x51:
				case 0xd1:
					state.pc = (uint16_t)Pop();
					return true;
				case 0x43:
					Put(a, (va & 0xffff) | (imm << 16));
					return true;
				case 0xc1:
					Put(a, (vb << 16) | (vb >> 16));
					return true;
				case 0xc0:
					Put(a, 0 - vb);
					return true;
				case 0x49:
					Put(a, (uint32_t)(int32_t)(int16_t)vb);
					return true;
				case 0x45:
					Put(a, (uint32_t)(int32_t)(int8_t)vb);
					return true;
				case 0x42:
					state.r[40] = va * vb;
					return true;
				case 0x4a:
					state.r[40] = (uint32_t)((int32_t)(int16_t)va * (int32_t)(int16_t)vb);
					return true;
				case 0xca:
					state.r[40] = (vb & 0xffff) ? (va & 0xffff) / (vb & 0xffff) : 0xffff;
					return true;
			}
			if ((o10 == 1 || o10 == 3) && o8 == 2 && o3 <= 2) {
				uint32_t v = vb & mask, result;
				if (o10 == 1)
					result = (v << 1) | (o3 == 2 ? (v >> (bits - 1)) : 0);
				else if (o3 == 0)
					result = v >> 1;
				else if (o3 == 1)
					result = (v >> 1) | (v & (1U << (bits - 1)));
				else
					result = (v >> 1) | ((v & 1) << (bits - 1));
				Flags(result, mask, bits);
				Put(a, (vb & ~mask) | (result & mask));
				return true;
			}
		}
		state.fault = 1;
		return false;
	}

  private:
	Read read;
	Write write;
	uint32_t dxor;
	uint64_t lanes;
	static unsigned L(uint32_t w, unsigned n)
	{
		return (w >> (2 * n)) & 3;
	}
	static unsigned Pack(uint32_t w, unsigned a, unsigned b, unsigned c)
	{
		return (L(w, a) << 4) | (L(w, b) << 2) | L(w, c);
	}
	uint32_t Get(unsigned n, uint16_t pc) const
	{
		return n == 0 ? 0 : n == 34 ? (uint32_t)pc + 2 : state.r[n];
	}
	void Put(unsigned n, uint32_t v)
	{
		if (n == 0 || n == 34 || n == 39 || n == 42 || n == 43)
			return;
		if (n == 38 && (v & 2) && !(state.r[38] & 2))
			state.irq = 1;
		state.r[n] = v;
	}
	void Push(uint32_t v)
	{
		state.local[(--state.r[33]) & 4095] = v;
	}
	uint32_t Pop()
	{
		uint32_t v = state.local[state.r[33] & 4095];
		state.r[33]++;
		return v;
	}
	uint32_t Hr(uint32_t a, unsigned n)
	{
		return read(a & 0xffffff, n);
	}
	void Hw(uint32_t a, unsigned n, uint32_t v)
	{
		if ((a & 0xfffffc) == 0xcc0000)
			state.r[39] = 0;
		else
			write(a & 0xffffff, n, v);
	}
	void Flags(uint32_t result, uint32_t mask, unsigned bits, uint32_t a = 0, uint32_t b = 0, bool sub = false, bool arithmetic = false)
	{
		result &= mask;
		state.z = result == 0;
		state.n = (result >> (bits - 1)) & 1;
		if (arithmetic) {
			a &= mask;
			b &= mask;
			unsigned sa = a >> (bits - 1), sb = b >> (bits - 1);
			state.c = sub ? a < b : (uint64_t)a + b > mask;
			state.v = sub ? (sa != sb && state.n != sa) : (sa == sb && state.n != sa);
		}
	}
	uint32_t Alu(unsigned op, uint32_t a, uint32_t b, uint32_t mask, unsigned bits)
	{
		uint32_t v;
		switch (op) {
			case 0x18:
				v = a + b;
				Flags(v, mask, bits, a, b, false, true);
				return v;
			case 0x19:
				v = a - b;
				Flags(v, mask, bits, a, b, true, true);
				return v;
			case 0x1a:
				v = a & b;
				break;
			case 0x1b:
				v = a ^ b;
				break;
			case 0x39:
				v = a | b;
				break;
			default:
				v = ~(a | b);
				break;
		}
		Flags(v, mask, bits);
		return v;
	}
	bool RunUntilIdle()
	{
		if (!state.valid || state.fault)
			return false;
		unsigned idle = 0;
		// The kernel's polling instruction is the same for every chip. This
		// is idle detection, not a game-PC speed hack or a synthesized reply.
		for (unsigned i = 0; i < 30000000; i++) {
			bool waiting = state.r[39] == 0 &&
			               (Decode(state.local[state.pc & 4095]) == 0x00120367 || Decode(state.local[(state.pc - 1) & 4095]) == 0x00120367);
			if (!Step())
				return false;
			idle = waiting ? idle + 1 : 0;
			if (idle > 200)
				return true;
		}
		state.fault = 1;
		return false;
	}
};
#endif
