// license:BSD-3-Clause
// copyright-holders:David Haywood, Samuel Neves, Peter Wilhelmsen, Morten Shearman Kirkegaard
// 315-5838 Decathlete decompressor, based on MAME 0.289 and MiSTer 5838.sv.
// Unknown Print Club cipher keys are deliberately not replaced with random data.
#pragma once
#include <cstdint>
namespace stv_5838 {
static uint16_t Read(uint32_t address);
struct Core {
	struct Node { uint8_t length, index; uint16_t pattern; } tree[13]{};
	uint8_t dictionary[256]{};
	uint32_t source = 0, start = 0;
	uint16_t mode = 0, compressed = 0, value = 0;
	int tree_position = 0, dictionary_position = 0, compressed_bits = 0, bits = 0;
	bool aborted = false;
	static uint16_t decipher(uint16_t c);

	void address_w(uint32_t data, uint32_t mask)
	{
		source = ((source & ~mask) | (data & mask)) & 0x7fffff;
		start = source;
		compressed = value = 0;
		compressed_bits = bits = 0;
		aborted = false;
	}

	void mode_w(uint16_t data)
	{
		mode = data;
		if (mode & 0x80) dictionary_position = 0;
		else tree_position = 0;
	}

	void table_w(uint16_t data)
	{
		if (mode & 0x80) {
			if (dictionary_position > 254) return;
			dictionary[dictionary_position++] = data >> 8;
			dictionary[dictionary_position++] = data;
		} else {
			if (tree_position >= 24) return;
			Node &n = tree[tree_position / 2];
			if (tree_position & 1) n.pattern = data;
			else { n.length = data >> 8; n.index = data; }
			tree_position++;
		}
	}

	uint8_t byte_r()
	{
		while (!aborted) {
			if (!compressed_bits) {
				compressed = decipher(Read((source * 2) ^ 2));
				source = (source + 1) & 0x7fffff;
				if (source == start) aborted = true;
				compressed_bits = 16;
			}
			value = (value << 1) | ((compressed >> --compressed_bits) & 1);
			if (++bits > 12) { aborted = true; break; }
			for (int i = 0; i < 12; i++) {
				if (bits != tree[i].length) continue;
				int first = tree[i].pattern >> (12 - bits);
				if (value < first) continue;
				if (bits < 12 && value >= (tree[i + 1].pattern >> (12 - bits))) continue;
				int index = tree[i].index + value - first;
				value = 0;
				bits = 0;
				if (index >= 256) { aborted = true; return 0xff; }
				return dictionary[index];
			}
		}
		return 0xff;
	}

	uint16_t data_r()
	{
		uint16_t high = byte_r();
		return (high << 8) | byte_r();
	}
};

uint16_t Core::decipher(uint16_t c)
{
	// TODO: use BIT macros instead of working in bytes
	uint16_t p = 0;
	uint16_t x[16];

	for (int b = 0; b < 16; ++b)
	{
		x[b] = (c >> b) & 1;
	}

	p |= (x[7] ^ x[9] ^ x[14] ? 0 : x[5] ^ x[12]) ^ x[14];
	p |= (((x[7] ^ x[9])&(x[12] ^ x[14] ^ x[5])) ^ x[14] ^ 1) << 1;
	p |= ((x[6] & x[8]) ^ (x[6] & x[15]) ^ (x[8] & x[15]) ^ 1) << 2;
	p |= (x[11] ^ x[14] ^ 1) << 3;
	p |= ((x[7] & (x[1] ^ x[8] ^ x[12])) ^ x[12]) << 4;
	p |= ((x[6] | x[8]) ^ (x[8] & x[15])) << 5;
	p |= (x[4] ^ (x[3] | x[10])) << 6;
	p |= ((x[14] & (x[5] ^ x[12])) ^ x[7] ^ x[9] ^ 1) << 7;
	p |= (x[4] ^ x[13] ^ 1) << 8;
	p |= (x[6] ^ (x[8] | (x[15] ^ 1))) << 9;
	p |= (x[7] ^ (x[12] | (x[1] ^ x[8] ^ x[7] ^ 1))) << 10;
	p |= (x[3] ^ x[10] ^ 1) << 11;
	p |= (x[0] ^ x[2]) << 12;
	p |= (x[8] ^ x[1] ? x[12] : x[7]) << 13;
	p |= (x[0] ^ x[11] ^ x[14] ^ 1) << 14;
	p |= (x[10] ^ 1) << 15;

	return p;
}

} // namespace stv_5838
