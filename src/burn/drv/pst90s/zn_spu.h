// license:BSD-3-Clause
// copyright-holders:pSXAuthor, R. Belmont
// ZN SPU voices: ADPCM and ADSR rates adapted from MAME 0.289 spu.cpp /
// spu_tables.cpp. TGM uses these voices for effects, in addition to QSound music.
// Noise timing and volume envelopes also follow the ZN-1/ZN-2 MiSTer SPU.
// Capcom ZN has no connected CD/XA audio source. Reverb remains MAME's
// preset approximation; this is not a complete PSX SPU replacement.
#pragma once

#include <vector>
namespace ZnSpuAudio
{
static float linear_rate[108], pos_exp_rate[100], neg_exp_rate[108];
static float decay_rate[16], linear_release_rate[27], exp_release_rate[27];
static const float freq_multiplier = 1.0f;
static float ms_to_rate(float ms)
{
	return 1.0f / (ms * 44.1f);
}
static float s_to_rate(float s)
{
	return ms_to_rate(s * 1000.0f);
}
static float get_linear_rate(const int n);
static float get_pos_exp_rate(const int n);
static float get_neg_exp_rate(const int n);
static void generate_linear_rate_table()
{
	linear_rate[0] = ms_to_rate(0.05f);
	linear_rate[1] = ms_to_rate(0.06f);
	linear_rate[2] = ms_to_rate(0.07f);
	linear_rate[3] = ms_to_rate(0.09f);
	linear_rate[4] = ms_to_rate(0.10f);
	linear_rate[5] = ms_to_rate(0.12f);
	linear_rate[6] = ms_to_rate(0.15f);
	linear_rate[7] = ms_to_rate(0.18f);
	linear_rate[8] = ms_to_rate(0.21f);
	linear_rate[9] = ms_to_rate(0.24f);
	linear_rate[10] = ms_to_rate(0.29f);
	linear_rate[11] = ms_to_rate(0.36f);
	linear_rate[12] = ms_to_rate(0.41f);
	linear_rate[13] = ms_to_rate(0.48f);
	linear_rate[14] = ms_to_rate(0.58f);
	linear_rate[15] = ms_to_rate(0.73f);
	linear_rate[16] = ms_to_rate(0.83f);
	linear_rate[17] = ms_to_rate(0.97f);
	linear_rate[18] = ms_to_rate(1.2f);
	linear_rate[19] = ms_to_rate(1.5f);
	linear_rate[20] = ms_to_rate(1.7f);
	linear_rate[21] = ms_to_rate(1.9f);
	linear_rate[22] = ms_to_rate(2.3f);
	linear_rate[23] = ms_to_rate(2.9f);
	linear_rate[24] = ms_to_rate(3.3f);
	linear_rate[25] = ms_to_rate(3.9f);
	linear_rate[26] = ms_to_rate(4.6f);
	linear_rate[27] = ms_to_rate(5.8f);
	linear_rate[28] = ms_to_rate(6.6f);
	linear_rate[29] = ms_to_rate(7.7f);
	linear_rate[30] = ms_to_rate(9.3f);
	linear_rate[31] = ms_to_rate(12.0f);
	linear_rate[32] = ms_to_rate(13.0f);
	linear_rate[33] = ms_to_rate(15.0f);
	linear_rate[34] = ms_to_rate(19.0f);
	linear_rate[35] = ms_to_rate(23.0f);
	linear_rate[36] = ms_to_rate(27.0f);
	linear_rate[37] = ms_to_rate(31.0f);
	linear_rate[38] = ms_to_rate(37.0f);
	linear_rate[39] = ms_to_rate(46.0f);
	linear_rate[40] = ms_to_rate(53.0f);
	linear_rate[41] = ms_to_rate(62.0f);
	linear_rate[42] = ms_to_rate(74.0f);
	linear_rate[43] = ms_to_rate(93.0f);
	linear_rate[44] = s_to_rate(0.11f);
	linear_rate[45] = s_to_rate(0.12f);
	linear_rate[46] = s_to_rate(0.15f);
	linear_rate[47] = s_to_rate(0.19f);
	linear_rate[48] = s_to_rate(0.21f);
	linear_rate[49] = s_to_rate(0.25f);
	linear_rate[50] = s_to_rate(0.30f);
	linear_rate[51] = s_to_rate(0.37f);
	linear_rate[52] = s_to_rate(0.42f);
	linear_rate[53] = s_to_rate(0.50f);
	linear_rate[54] = s_to_rate(0.59f);
	linear_rate[55] = s_to_rate(0.74f);
	linear_rate[56] = s_to_rate(0.85f);
	linear_rate[57] = s_to_rate(0.99f);
	linear_rate[58] = s_to_rate(1.2f);
	linear_rate[59] = s_to_rate(1.5f);
	linear_rate[60] = s_to_rate(1.7f);
	linear_rate[61] = s_to_rate(2.0f);
	linear_rate[62] = s_to_rate(2.4f);
	linear_rate[63] = s_to_rate(3.0f);
	linear_rate[64] = s_to_rate(3.4f);
	linear_rate[65] = s_to_rate(4.0f);
	linear_rate[66] = s_to_rate(4.8f);
	linear_rate[67] = s_to_rate(5.9f);
	linear_rate[68] = s_to_rate(6.8f);
	linear_rate[69] = s_to_rate(7.9f);
	linear_rate[70] = s_to_rate(9.5f);
	linear_rate[71] = s_to_rate(12.0f);
	linear_rate[72] = s_to_rate(14.0f);
	linear_rate[73] = s_to_rate(16.0f);
	linear_rate[74] = s_to_rate(19.0f);
	linear_rate[75] = s_to_rate(24.0f);
	linear_rate[76] = s_to_rate(27.0f);
	linear_rate[77] = s_to_rate(32.0f);
	linear_rate[78] = s_to_rate(38.0f);
	linear_rate[79] = s_to_rate(48.0f);
	linear_rate[80] = s_to_rate(54.0f);
	linear_rate[81] = s_to_rate(63.0f);
	linear_rate[82] = s_to_rate(76.0f);
	linear_rate[83] = s_to_rate(95.0f);
	linear_rate[84] = s_to_rate(109.0f);
	linear_rate[85] = s_to_rate(127.0f);
	linear_rate[86] = s_to_rate(152.0f);
	linear_rate[87] = s_to_rate(190.0f);
	linear_rate[88] = s_to_rate(218.0f);
	linear_rate[89] = s_to_rate(254.0f);
	linear_rate[90] = s_to_rate(304.0f);
	linear_rate[91] = s_to_rate(380.0f);
	linear_rate[92] = s_to_rate(436.0f);
	linear_rate[93] = s_to_rate(508.0f);
	linear_rate[94] = s_to_rate(608.0f);
	linear_rate[95] = s_to_rate(760.0f);
	linear_rate[96] = s_to_rate(872.0f);
	linear_rate[97] = s_to_rate(1016.0f);
	linear_rate[98] = s_to_rate(1216.0f);
	linear_rate[99] = s_to_rate(1520.0f);
	linear_rate[100] = s_to_rate(1744.0f);
	linear_rate[101] = s_to_rate(2032.0f);
	linear_rate[102] = s_to_rate(2432.0f);
	linear_rate[103] = s_to_rate(3040.0f);
	linear_rate[104] = s_to_rate(3488.0f);
	linear_rate[105] = s_to_rate(4064.0f);
	linear_rate[106] = s_to_rate(4864.0f);
	linear_rate[107] = s_to_rate(6080.0f);
}

static void generate_pos_exp_rate_table()
{
	pos_exp_rate[0] = ms_to_rate(0.09f);
	pos_exp_rate[1] = ms_to_rate(0.11f);
	pos_exp_rate[2] = ms_to_rate(0.13f);
	pos_exp_rate[3] = ms_to_rate(0.16f);
	pos_exp_rate[4] = ms_to_rate(0.18f);
	pos_exp_rate[5] = ms_to_rate(0.21f);
	pos_exp_rate[6] = ms_to_rate(0.25f);
	pos_exp_rate[7] = ms_to_rate(0.32f);
	pos_exp_rate[8] = ms_to_rate(0.36f);
	pos_exp_rate[9] = ms_to_rate(0.42f);
	pos_exp_rate[10] = ms_to_rate(0.51f);
	pos_exp_rate[11] = ms_to_rate(0.64f);
	pos_exp_rate[12] = ms_to_rate(0.73f);
	pos_exp_rate[13] = ms_to_rate(0.85f);
	pos_exp_rate[14] = ms_to_rate(1.0f);
	pos_exp_rate[15] = ms_to_rate(1.3f);
	pos_exp_rate[16] = ms_to_rate(1.5f);
	pos_exp_rate[17] = ms_to_rate(1.7f);
	pos_exp_rate[18] = ms_to_rate(2.0f);
	pos_exp_rate[19] = ms_to_rate(2.5f);
	pos_exp_rate[20] = ms_to_rate(2.9f);
	pos_exp_rate[21] = ms_to_rate(3.4f);
	pos_exp_rate[22] = ms_to_rate(4.1f);
	pos_exp_rate[23] = ms_to_rate(5.1f);
	pos_exp_rate[24] = ms_to_rate(5.8f);
	pos_exp_rate[25] = ms_to_rate(6.8f);
	pos_exp_rate[26] = ms_to_rate(8.1f);
	pos_exp_rate[27] = ms_to_rate(10.0f);
	pos_exp_rate[28] = ms_to_rate(12.0f);
	pos_exp_rate[29] = ms_to_rate(14.0f);
	pos_exp_rate[30] = ms_to_rate(16.0f);
	pos_exp_rate[31] = ms_to_rate(20.0f);
	pos_exp_rate[32] = ms_to_rate(23.0f);
	pos_exp_rate[33] = ms_to_rate(27.0f);
	pos_exp_rate[34] = ms_to_rate(33.0f);
	pos_exp_rate[35] = ms_to_rate(41.0f);
	pos_exp_rate[36] = ms_to_rate(46.0f);
	pos_exp_rate[37] = ms_to_rate(54.0f);
	pos_exp_rate[38] = ms_to_rate(65.0f);
	pos_exp_rate[39] = ms_to_rate(81.0f);
	pos_exp_rate[40] = ms_to_rate(93.0f);
	pos_exp_rate[41] = s_to_rate(0.11f);
	pos_exp_rate[42] = s_to_rate(0.13f);
	pos_exp_rate[43] = s_to_rate(0.16f);
	pos_exp_rate[44] = s_to_rate(0.19f);
	pos_exp_rate[45] = s_to_rate(0.22f);
	pos_exp_rate[46] = s_to_rate(0.26f);
	pos_exp_rate[47] = s_to_rate(0.33f);
	pos_exp_rate[48] = s_to_rate(0.37f);
	pos_exp_rate[49] = s_to_rate(0.43f);
	pos_exp_rate[50] = s_to_rate(0.52f);
	pos_exp_rate[51] = s_to_rate(0.65f);
	pos_exp_rate[52] = s_to_rate(0.74f);
	pos_exp_rate[53] = s_to_rate(0.87f);
	pos_exp_rate[54] = s_to_rate(1.0f);
	pos_exp_rate[55] = s_to_rate(1.3f);
	pos_exp_rate[56] = s_to_rate(1.5f);
	pos_exp_rate[57] = s_to_rate(1.7f);
	pos_exp_rate[58] = s_to_rate(2.1f);
	pos_exp_rate[59] = s_to_rate(2.6f);
	pos_exp_rate[60] = s_to_rate(3.0f);
	pos_exp_rate[61] = s_to_rate(3.5f);
	pos_exp_rate[62] = s_to_rate(4.2f);
	pos_exp_rate[63] = s_to_rate(5.2f);
	pos_exp_rate[64] = s_to_rate(5.9f);
	pos_exp_rate[65] = s_to_rate(6.9f);
	pos_exp_rate[66] = s_to_rate(8.3f);
	pos_exp_rate[67] = s_to_rate(10.0f);
	pos_exp_rate[68] = s_to_rate(12.0f);
	pos_exp_rate[69] = s_to_rate(14.0f);
	pos_exp_rate[70] = s_to_rate(17.0f);
	pos_exp_rate[71] = s_to_rate(21.0f);
	pos_exp_rate[72] = s_to_rate(24.0f);
	pos_exp_rate[73] = s_to_rate(28.0f);
	pos_exp_rate[74] = s_to_rate(33.0f);
	pos_exp_rate[75] = s_to_rate(42.0f);
	pos_exp_rate[76] = s_to_rate(48.0f);
	pos_exp_rate[77] = s_to_rate(55.0f);
	pos_exp_rate[78] = s_to_rate(67.0f);
	pos_exp_rate[79] = s_to_rate(83.0f);
	pos_exp_rate[80] = s_to_rate(95.0f);
	pos_exp_rate[81] = s_to_rate(111.0f);
	pos_exp_rate[82] = s_to_rate(133.0f);
	pos_exp_rate[83] = s_to_rate(166.0f);
	pos_exp_rate[84] = s_to_rate(190.0f);
	pos_exp_rate[85] = s_to_rate(222.0f);
	pos_exp_rate[86] = s_to_rate(266.0f);
	pos_exp_rate[87] = s_to_rate(333.0f);
	pos_exp_rate[88] = s_to_rate(380.0f);
	pos_exp_rate[89] = s_to_rate(444.0f);
	pos_exp_rate[90] = s_to_rate(532.0f);
	pos_exp_rate[91] = s_to_rate(666.0f);
	pos_exp_rate[92] = s_to_rate(760.0f);
	pos_exp_rate[93] = s_to_rate(888.0f);
	pos_exp_rate[94] = s_to_rate(1064.0f);
	pos_exp_rate[95] = s_to_rate(1332.0f);
	pos_exp_rate[96] = s_to_rate(1520.0f);
	pos_exp_rate[97] = s_to_rate(1776.0f);
	pos_exp_rate[98] = s_to_rate(2128.0f);
	pos_exp_rate[99] = s_to_rate(2664.0f);
}

static void generate_neg_exp_rate_table()
{
	neg_exp_rate[0] = ms_to_rate(0.07f);
	neg_exp_rate[1] = ms_to_rate(0.09f);
	neg_exp_rate[2] = ms_to_rate(0.11f);
	neg_exp_rate[3] = ms_to_rate(0.14f);
	neg_exp_rate[4] = ms_to_rate(0.18f);
	neg_exp_rate[5] = ms_to_rate(0.21f);
	neg_exp_rate[6] = ms_to_rate(0.25f);
	neg_exp_rate[7] = ms_to_rate(0.31f);
	neg_exp_rate[8] = ms_to_rate(0.39f);
	neg_exp_rate[9] = ms_to_rate(0.45f);
	neg_exp_rate[10] = ms_to_rate(0.53f);
	neg_exp_rate[11] = ms_to_rate(0.64f);
	neg_exp_rate[12] = ms_to_rate(0.81f);
	neg_exp_rate[13] = ms_to_rate(0.93f);
	neg_exp_rate[14] = ms_to_rate(1.1f);
	neg_exp_rate[15] = ms_to_rate(1.3f);
	neg_exp_rate[16] = ms_to_rate(1.6f);
	neg_exp_rate[17] = ms_to_rate(1.9f);
	neg_exp_rate[18] = ms_to_rate(2.2f);
	neg_exp_rate[19] = ms_to_rate(2.6f);
	neg_exp_rate[20] = ms_to_rate(3.3f);
	neg_exp_rate[21] = ms_to_rate(3.8f);
	neg_exp_rate[22] = ms_to_rate(4.4f);
	neg_exp_rate[23] = ms_to_rate(5.3f);
	neg_exp_rate[24] = ms_to_rate(6.7f);
	neg_exp_rate[25] = ms_to_rate(7.6f);
	neg_exp_rate[26] = ms_to_rate(8.9f);
	neg_exp_rate[27] = ms_to_rate(11.0f);
	neg_exp_rate[28] = ms_to_rate(13.0f);
	neg_exp_rate[29] = ms_to_rate(15.0f);
	neg_exp_rate[30] = ms_to_rate(18.0f);
	neg_exp_rate[31] = ms_to_rate(21.0f);
	neg_exp_rate[32] = ms_to_rate(27.0f);
	neg_exp_rate[33] = ms_to_rate(31.0f);
	neg_exp_rate[34] = ms_to_rate(36.0f);
	neg_exp_rate[35] = ms_to_rate(43.0f);
	neg_exp_rate[36] = ms_to_rate(53.0f);
	neg_exp_rate[37] = ms_to_rate(61.0f);
	neg_exp_rate[38] = ms_to_rate(71.0f);
	neg_exp_rate[39] = ms_to_rate(86.0f);
	neg_exp_rate[40] = s_to_rate(0.11f);
	neg_exp_rate[41] = s_to_rate(0.12f);
	neg_exp_rate[42] = s_to_rate(0.14f);
	neg_exp_rate[43] = s_to_rate(0.17f);
	neg_exp_rate[44] = s_to_rate(0.21f);
	neg_exp_rate[45] = s_to_rate(0.24f);
	neg_exp_rate[46] = s_to_rate(0.29f);
	neg_exp_rate[47] = s_to_rate(0.34f);
	neg_exp_rate[48] = s_to_rate(0.43f);
	neg_exp_rate[49] = s_to_rate(0.49f);
	neg_exp_rate[50] = s_to_rate(0.57f);
	neg_exp_rate[51] = s_to_rate(0.68f);
	neg_exp_rate[52] = s_to_rate(0.86f);
	neg_exp_rate[53] = s_to_rate(0.98f);
	neg_exp_rate[54] = s_to_rate(1.1f);
	neg_exp_rate[55] = s_to_rate(1.4f);
	neg_exp_rate[56] = s_to_rate(1.7f);
	neg_exp_rate[57] = s_to_rate(2.0f);
	neg_exp_rate[58] = s_to_rate(2.3f);
	neg_exp_rate[59] = s_to_rate(2.7f);
	neg_exp_rate[60] = s_to_rate(3.4f);
	neg_exp_rate[61] = s_to_rate(3.9f);
	neg_exp_rate[62] = s_to_rate(4.6f);
	neg_exp_rate[63] = s_to_rate(5.5f);
	neg_exp_rate[64] = s_to_rate(6.8f);
	neg_exp_rate[65] = s_to_rate(7.8f);
	neg_exp_rate[66] = s_to_rate(9.1f);
	neg_exp_rate[67] = s_to_rate(11.0f);
	neg_exp_rate[68] = s_to_rate(14.0f);
	neg_exp_rate[69] = s_to_rate(16.0f);
	neg_exp_rate[70] = s_to_rate(18.0f);
	neg_exp_rate[71] = s_to_rate(22.0f);
	neg_exp_rate[72] = s_to_rate(27.0f);
	neg_exp_rate[73] = s_to_rate(31.0f);
	neg_exp_rate[74] = s_to_rate(36.0f);
	neg_exp_rate[75] = s_to_rate(44.0f);
	neg_exp_rate[76] = s_to_rate(55.0f);
	neg_exp_rate[77] = s_to_rate(63.0f);
	neg_exp_rate[78] = s_to_rate(73.0f);
	neg_exp_rate[79] = s_to_rate(88.0f);
	neg_exp_rate[80] = s_to_rate(109.0f);
	neg_exp_rate[81] = s_to_rate(125.0f);
	neg_exp_rate[82] = s_to_rate(146.0f);
	neg_exp_rate[83] = s_to_rate(175.0f);
	neg_exp_rate[84] = s_to_rate(219.0f);
	neg_exp_rate[85] = s_to_rate(250.0f);
	neg_exp_rate[86] = s_to_rate(292.0f);
	neg_exp_rate[87] = s_to_rate(350.0f);
	neg_exp_rate[88] = s_to_rate(438.0f);
	neg_exp_rate[89] = s_to_rate(500.0f);
	neg_exp_rate[90] = s_to_rate(584.0f);
	neg_exp_rate[91] = s_to_rate(700.0f);
	neg_exp_rate[92] = s_to_rate(876.0f);
	neg_exp_rate[93] = s_to_rate(1000.0f);
	neg_exp_rate[94] = s_to_rate(1168.0f);
	neg_exp_rate[95] = s_to_rate(1400.0f);
	neg_exp_rate[96] = s_to_rate(1752.0f);
	neg_exp_rate[97] = s_to_rate(2000.0f);
	neg_exp_rate[98] = s_to_rate(2336.0f);
	neg_exp_rate[99] = s_to_rate(2800.0f);
	neg_exp_rate[100] = s_to_rate(3504.0f);
	neg_exp_rate[101] = s_to_rate(4000.0f);
	neg_exp_rate[102] = s_to_rate(4672.0f);
	neg_exp_rate[103] = s_to_rate(5600.0f);
	neg_exp_rate[104] = s_to_rate(7008.0f);
	neg_exp_rate[105] = s_to_rate(8000.0f);
	neg_exp_rate[106] = s_to_rate(9344.0f);
	neg_exp_rate[107] = s_to_rate(11200.0f);
}

static void generate_decay_rate_table()
{
	decay_rate[0] = ms_to_rate(0.07f);
	decay_rate[1] = ms_to_rate(0.18f);
	decay_rate[2] = ms_to_rate(0.39f);
	decay_rate[3] = ms_to_rate(0.81f);
	decay_rate[4] = ms_to_rate(1.6f);
	decay_rate[5] = ms_to_rate(3.3f);
	decay_rate[6] = ms_to_rate(6.7f);
	decay_rate[7] = ms_to_rate(13.0f);
	decay_rate[8] = ms_to_rate(27.0f);
	decay_rate[9] = ms_to_rate(53.0f);
	decay_rate[10] = s_to_rate(0.11f);
	decay_rate[11] = s_to_rate(0.21f);
	decay_rate[12] = s_to_rate(0.43f);
	decay_rate[13] = s_to_rate(0.86f);
	decay_rate[14] = s_to_rate(1.7f);
	decay_rate[15] = s_to_rate(3.4f);
}

static void generate_linear_release_rate_table()
{
	linear_release_rate[0] = ms_to_rate(0.04f);
	linear_release_rate[1] = ms_to_rate(0.09f);
	linear_release_rate[2] = ms_to_rate(0.18f);
	linear_release_rate[3] = ms_to_rate(0.36f);
	linear_release_rate[4] = ms_to_rate(0.73f);
	linear_release_rate[5] = ms_to_rate(1.5f);
	linear_release_rate[6] = ms_to_rate(2.9f);
	linear_release_rate[7] = ms_to_rate(5.8f);
	linear_release_rate[8] = ms_to_rate(12.0f);
	linear_release_rate[9] = ms_to_rate(23.0f);
	linear_release_rate[10] = ms_to_rate(46.0f);
	linear_release_rate[11] = ms_to_rate(93.0f);
	linear_release_rate[12] = s_to_rate(0.19f);
	linear_release_rate[13] = s_to_rate(0.37f);
	linear_release_rate[14] = s_to_rate(1.74f);
	linear_release_rate[15] = s_to_rate(1.5f);
	linear_release_rate[16] = s_to_rate(3.0f);
	linear_release_rate[17] = s_to_rate(5.9f);
	linear_release_rate[18] = s_to_rate(12.0f);
	linear_release_rate[19] = s_to_rate(24.0f);
	linear_release_rate[20] = s_to_rate(48.0f);
	linear_release_rate[21] = s_to_rate(95.0f);
	linear_release_rate[22] = s_to_rate(190.0f);
	linear_release_rate[23] = s_to_rate(380.0f);
	linear_release_rate[24] = s_to_rate(760.0f);
	linear_release_rate[25] = s_to_rate(1520.0f);
	linear_release_rate[26] = s_to_rate(3040.0f);
}

static void generate_exp_release_rate_table()
{
	exp_release_rate[0] = ms_to_rate(0.07f);
	exp_release_rate[1] = ms_to_rate(0.18f);
	exp_release_rate[2] = ms_to_rate(0.39f);
	exp_release_rate[3] = ms_to_rate(0.81f);
	exp_release_rate[4] = ms_to_rate(1.6f);
	exp_release_rate[5] = ms_to_rate(3.3f);
	exp_release_rate[6] = ms_to_rate(6.7f);
	exp_release_rate[7] = ms_to_rate(13.0f);
	exp_release_rate[8] = ms_to_rate(27.0f);
	exp_release_rate[9] = ms_to_rate(53.0f);
	exp_release_rate[10] = s_to_rate(0.11f);
	exp_release_rate[11] = s_to_rate(0.21f);
	exp_release_rate[12] = s_to_rate(0.43f);
	exp_release_rate[13] = s_to_rate(0.86f);
	exp_release_rate[14] = s_to_rate(1.7f);
	exp_release_rate[15] = s_to_rate(3.4f);
	exp_release_rate[16] = s_to_rate(6.8f);
	exp_release_rate[17] = s_to_rate(14.0f);
	exp_release_rate[18] = s_to_rate(27.0f);
	exp_release_rate[19] = s_to_rate(55.0f);
	exp_release_rate[20] = s_to_rate(109.0f);
	exp_release_rate[21] = s_to_rate(219.0f);
	exp_release_rate[22] = s_to_rate(438.0f);
	exp_release_rate[23] = s_to_rate(876.0f);
	exp_release_rate[24] = s_to_rate(1752.0f);
	exp_release_rate[25] = s_to_rate(3504.0f);
	exp_release_rate[26] = s_to_rate(7008.0f);
}

static float get_linear_rate(const int n)
{
	int num_linear_rates = ARRAY_SIZE(linear_rate);
	if (n >= num_linear_rates)
		return 0.0f;
	return linear_rate[n] * freq_multiplier;
}

static float get_pos_exp_rate(const int n)
{
	int num_pos_exp_rates = ARRAY_SIZE(pos_exp_rate);
	if (n >= num_pos_exp_rates)
		return 0.0f;
	return pos_exp_rate[n] * freq_multiplier;
}

static float get_neg_exp_rate(const int n)
{
	int num_neg_exp_rates = ARRAY_SIZE(neg_exp_rate);
	if (n >= num_neg_exp_rates)
		return 0.0f;
	return -neg_exp_rate[n] * freq_multiplier;
}

static float get_decay_rate(const int n)
{
	return decay_rate[n] * freq_multiplier;
}

static float get_sustain_level(const int n)
{
	return ((float)(n + 1)) / 16.0f;
}

static float get_linear_release_rate(const int n)
{
	int num_linear_release_rates = ARRAY_SIZE(linear_release_rate);
	if (n >= num_linear_release_rates)
		return 0.0f;
	return linear_release_rate[n] * freq_multiplier;
}

static float get_exp_release_rate(const int n)
{
	int num_exp_release_rates = ARRAY_SIZE(exp_release_rate);
	if (n >= num_exp_release_rates)
		return 0.0f;
	return exp_release_rate[n] * freq_multiplier;
}

struct Voice {
	UINT32 address, loop, fraction;
	INT32 history[2], sample, next, nibble, flags, active, stage;
	float level, delta, attack, decay, sustain, sustainRate, release;
	UINT32 remaining;
};
static Voice voices[24];
static std::vector<INT32> frame;
static INT32 position, last[2];
static UINT32 resample;
static void Sync();
static UINT32 capturePosition;
static INT32 frameStart[2];

static void CheckIrq(UINT32 address)
{
	if ((ZnSpuRegs[0x1aa / 2] & 0x8040) == 0x8040 && !(ZnSpuRegs[0x1ae / 2] & 0x40) && ((address & 0x7ffff) >> 3) == ZnSpuRegs[0x1a4 / 2]) {
		ZnSpuRegs[0x1ae / 2] |= 0x40;
		ZnSpuIrq();
	}
}

static void Capture(UINT32 address, INT32 value)
{
	if (ZnSpuRegs[0x1ac / 2] & 0x0c)
		CheckIrq(address);
	ZnSpuRam[address] = (UINT16)value;
	ZnSpuRam[address + 1] = (UINT16)value >> 8;
}
static INT32 Clip(INT32 x)
{
	return x < -32768 ? -32768 : x > 32767 ? 32767 : x;
}
// MiSTer spu.vhd REVERB_READ1/WRITE1/READ2/WRITE2 address sequencer.
// Keep this bus/IRQ model separate from MAME's preset-based audio filter.
// Addresses are 16-bit words; parameter offsets have eight-byte units.
static UINT32 reverbAddress;
static UINT32 reverbChannel;
static UINT32 ReverbRamAddress(UINT16 offset, INT32 previous = 0)
{
	UINT32 relative = ((UINT32)offset * 4 - previous) & 0x3ffff;
	UINT32 address = reverbAddress + relative;
	if (address & 0x40000)
		address += (UINT32)ZnSpuRegs[0x1a2 / 2] * 4;
	return (address & 0x3ffff) * 2;
}
static void ReverbRamClock()
{
	const UINT16 *r = ZnSpuRegs + 0x1c0 / 2;
	const UINT32 ch = reverbChannel;
	if (ZnSpuRegs[0x1aa / 2] & 0x80) {
		CheckIrq(ReverbRamAddress(r[16 + ch]));       // dSAME
		CheckIrq(ReverbRamAddress(r[24 + (ch ^ 1)])); // cross-channel dDIFF
		CheckIrq(ReverbRamAddress(r[10 + ch], 1));    // mSAME[-1]
		CheckIrq(ReverbRamAddress(r[18 + ch], 1));    // mDIFF[-1]
		CheckIrq(ReverbRamAddress(r[10 + ch]));
		CheckIrq(ReverbRamAddress(r[18 + ch]));
	}
	CheckIrq(ReverbRamAddress(r[12 + ch]));
	CheckIrq(ReverbRamAddress(r[14 + ch]));
	CheckIrq(ReverbRamAddress(r[20 + ch]));
	CheckIrq(ReverbRamAddress(r[22 + ch]));
	CheckIrq(ReverbRamAddress((UINT16)(r[26 + ch] - r[0])));
	CheckIrq(ReverbRamAddress((UINT16)(r[28 + ch] - r[1])));
	if (ZnSpuRegs[0x1aa / 2] & 0x80) {
		CheckIrq(ReverbRamAddress(r[26 + ch]));
		CheckIrq(ReverbRamAddress(r[28 + ch]));
	}
	if (ch)
		reverbAddress = reverbAddress == 0x3ffff ? (UINT32)ZnSpuRegs[0x1a2 / 2] * 4 : reverbAddress + 1;
	reverbChannel ^= 1;
}

#include "zn_spu_reverb.h"

static UINT32 Duration(float distance, float delta)
{
	if (delta == 0.0f)
		return 0xffffffff;
	double n = distance / delta;
	return n <= 0 ? 0 : n >= 4294967295.0 ? 0xffffffff : (UINT32)n;
}
static void Envelope(Voice &v)
{
	while (v.remaining == 0 && v.active) {
		switch (++v.stage) {
			case 1:
				v.level = 1;
				v.delta = v.decay;
				v.remaining = Duration(v.sustain - 1, v.delta);
				break;
			case 2:
				v.level = v.sustain;
				v.delta = v.sustainRate;
				v.remaining = Duration(v.delta > 0 ? 1 - v.level : -v.level, v.delta);
				break;
			case 3:
				v.level = v.sustainRate > 0 ? 1.0f : 0.0f;
				v.delta = 0;
				v.remaining = 0xffffffff;
				if (!v.level)
					v.active = 0;
				break;
			case 4:
				v.level = v.level < 0 ? 0 : v.level > 1 ? 1 : v.level;
				v.delta = v.release;
				v.remaining = Duration(-v.level, v.delta);
				break;
			default:
				v.level = v.delta = 0;
				v.active = 0;
				v.remaining = 0xffffffff;
				break;
		}
	}
}
static INT32 Decode(Voice &v, INT32 channel)
{
	static const INT32 filter[5][2] = { { 0, 0 }, { 60, 0 }, { 115, -52 }, { 98, -55 }, { 122, -60 } };
	if (v.nibble == 28) {
		if (v.flags & 1) {
			ZnSpuRegs[0x19c / 2 + channel / 16] |= 1 << (channel % 16);
			if (!(v.flags & 2) && !(ZnSpuRegs[0x194 / 2 + channel / 16] & (1 << (channel % 16)))) {
				v.active = 0;
				return 0;
			}
			v.address = v.loop;
		} else
			v.address = (v.address + 16) & 0x7ffff;
		v.nibble = 0;
	}
	if (!v.nibble) {
		// ADPCM blocks are fetched in two IRQ-address units (8 bytes).
		CheckIrq(v.address);
		CheckIrq(v.address + 8);
		v.flags = ZnSpuRam[(v.address + 1) & 0x7ffff];
		if (v.flags & 4) {
			v.loop = v.address;
			ZnSpuRegs[channel * 8 + 7] = v.loop >> 3;
		}
	}
	INT32 info = ZnSpuRam[v.address], f = info >> 4, shift = info & 15;
	if (f > 4)
		f = 0;
	UINT8 packed = ZnSpuRam[(v.address + 2 + v.nibble / 2) & 0x7ffff];
	INT32 sample = (INT16)(((packed >> ((v.nibble & 1) * 4)) & 15) << 12);
	// MAME's decoder stores each reconstructed sample as signed 16 bit.
	sample = (INT16)((sample >> shift) + ((v.history[0] * filter[f][0] + v.history[1] * filter[f][1] + 32) >> 6));
	v.history[1] = v.history[0];
	v.history[0] = sample;
	v.nibble++;
	return sample;
}
static void KeyOn(INT32 c)
{
	Voice &v = voices[c];
	memset(&v, 0, sizeof(v));
	UINT16 *r = ZnSpuRegs + c * 8;
	v.address = (r[3] << 3) & 0x7ffff;
	v.loop = (r[7] << 3) & 0x7ffff;
	v.active = 1;
	v.attack = (r[4] & 0x8000) ? get_pos_exp_rate((r[4] >> 8) & 127) : get_linear_rate((r[4] >> 8) & 127);
	v.decay = -get_decay_rate((r[4] >> 4) & 15);
	v.sustain = get_sustain_level(r[4] & 15);
	INT32 rate = (r[5] >> 6) & 127;
	v.sustainRate = (r[5] & 0x8000) ? ((r[5] & 0x4000) ? get_neg_exp_rate(rate) : get_pos_exp_rate(rate))
	                                : ((r[5] & 0x4000) ? -get_linear_rate(rate) : get_linear_rate(rate));
	v.release = -((r[5] & 0x20) ? get_exp_release_rate(r[5] & 31) : get_linear_release_rate(r[5] & 31));
	v.delta = v.attack;
	v.remaining = Duration(1, v.attack);
	v.sample = Decode(v, c);
	v.next = Decode(v, c);
	r[6] = 1;
	ZnSpuRegs[0x19c / 2 + c / 16] &= ~(1 << (c % 16));
}
// Independent of key-on: volume sweeps continue even on an inactive voice.
struct VolumeEnvelope {
	INT32 value;
	UINT32 counter;
};
static VolumeEnvelope volumes[50]; // 24 stereo voices, then stereo master
static UINT32 noiseCounter;
static UINT16 noiseLevel;

static INT32 Volume(INT32 index, UINT16 setting)
{
	VolumeEnvelope &v = volumes[index];
	if (!(setting & 0x8000)) {
		v.value = (INT16)(setting << 1);
		v.counter = 0;
		return v.value;
	}
	const INT32 rate = setting & 127, shift = rate >> 2;
	if (rate == 127)
		return v.value; // Stopped sweep does not change the sign.
	const bool decrease = (setting & 0x2000) != 0;
	const bool exponential = (setting & 0x4000) != 0;
	// Exponential decay follows the current sign; its phase bit is ignored.
	const bool negative = (setting & 0x1000) != 0 && !(exponential && decrease);
	INT32 step = 7 - (rate & 3);
	if (decrease != negative)
		step = ~step;
	if (shift < 11)
		step *= 1 << (11 - shift);
	UINT32 increment = 0x8000U >> (shift > 11 ? shift - 11 : 0);
	if (exponential && decrease)
		step = (step * v.value) >> 15;
	else if (exponential && v.value > 0x6000) {
		if (shift < 10)
			step >>= 2;
		else if (shift >= 11)
			increment >>= 2;
		else {
			step >>= 1;
			increment >>= 1;
		}
	}
	if (!increment)
		increment = 1;
	v.counter += increment;
	if (v.counter < 0x8000)
		return v.value;
	v.counter = 0;
	INT32 value = v.value + step;
	if (!decrease)
		v.value = Clip(value);
	else if (negative || (exponential && v.value < 0))
		v.value = value > 0 ? 0 : value < -32768 ? -32768 : value;
	else
		v.value = value < 0 ? 0 : value > 32767 ? 32767 : value;
	return v.value;
}

static void NoiseClock()
{
	// MiSTer uses a fractional accumulator, not MAME's approximate clock.
	static const UINT32 fraction[4] = { 0, 84, 140, 180 };
	UINT16 control = ZnSpuRegs[0x1aa / 2];
	noiseCounter += fraction[(control >> 8) & 3];
	noiseCounter += (noiseCounter & 65535) >= 210 ? 0x20000 - 210 : 0x10000;
	UINT32 threshold = (0x8000U >> ((control >> 10) & 15)) << 16;
	if (noiseCounter >= threshold) {
		noiseCounter &= threshold - 1;
		noiseLevel =
		    (UINT16)((noiseLevel << 1) | ((1 ^ (noiseLevel >> 15) ^ (noiseLevel >> 12) ^ (noiseLevel >> 11) ^ (noiseLevel >> 10)) & 1));
	}
}

static UINT16 CurrentVolume(UINT32 offset)
{
	if (offset == 0x1b8 || offset == 0x1ba)
		return (UINT16)volumes[48 + (offset - 0x1b8) / 2].value;
	if (offset >= 0x200 && offset < 0x260)
		return (UINT16)volumes[(offset - 0x200) / 2].value;
	return ZnSpuRegs[offset / 2];
}

static void Sample()
{
	INT32 out[2] = { 0, 0 }, wet[2] = { 0, 0 }, previous = 0, captured[2] = { 0, 0 };
	for (INT32 c = 0; c < 24; c++) {
		Voice &v = voices[c];
		UINT16 *r = ZnSpuRegs + c * 8;
		INT32 volume[2] = { Volume(c * 2, r[0]), Volume(c * 2 + 1, r[1]) };
		UINT32 pitch = r[2];
		if (c && (ZnSpuRegs[0x190 / 2 + c / 16] & (1 << (c % 16))))
			pitch = (pitch * (previous + 32768)) >> 15;
		if (pitch > 0x3fff)
			pitch = 0x3fff;
		previous = 0;
		if (!v.active) {
			r[6] = 0;
			continue;
		}
		Envelope(v);
		if (!v.active) {
			r[6] = 0;
			continue;
		}
		INT32 sample = (ZnSpuRegs[0x194 / 2 + c / 16] & (1 << (c % 16))) ? (INT16)noiseLevel
		                                                                 : v.sample + (((v.next - v.sample) * (INT32)v.fraction) >> 12);
		previous = Clip((INT32)(sample * v.level));
		if (c == 1 || c == 3)
			captured[c / 2] = previous;
		INT32 *mix = (ZnSpuRegs[0x198 / 2 + c / 16] & (1 << (c % 16))) ? wet : out;
		for (INT32 ch = 0; ch < 2; ch++) {
			mix[ch] = Clip(mix[ch] + (INT32)(((INT64)sample * (INT32)(v.level * volume[ch])) >> 15));
		}
		v.level += v.delta;
		if (v.remaining != 0xffffffff)
			v.remaining--;
		r[6] = (UINT16)(v.level < 0 ? 0 : v.level > 1 ? 32767 : v.level * 32767);
		v.fraction += pitch;
		while (v.fraction >= 4096 && v.active) {
			v.fraction -= 4096;
			v.sample = v.next;
			v.next = Decode(v, c);
		}
	}
	// Four 1 KiB capture rings, advanced at the native 44.1 kHz clock.
	// Capcom ZN has no CD input, so its two capture channels contain zero.
	Capture(capturePosition, 0);
	Capture(0x400 + capturePosition, 0);
	Capture(0x800 + capturePosition, captured[0]);
	Capture(0xc00 + capturePosition, captured[1]);
	capturePosition = (capturePosition + 2) & 0x3ff;
	ZnSpuRegs[0x1ae / 2] = (ZnSpuRegs[0x1ae / 2] & ~0x800) | ((capturePosition & 0x200) << 2);
	NoiseClock();
	ReverbRamClock();
	ReverbSample(out, wet);
	for (INT32 ch = 0; ch < 2; ch++)
		out[ch] = (out[ch] * Volume(48 + ch, ZnSpuRegs[0x180 / 2 + ch])) >> 15;
	// MAME's ZN speaker route is 0.35 for each SPU channel.
	last[0] = (ZnSpuRegs[0x1aa / 2] & 0x4000) ? out[0] * 35 / 100 : 0;
	last[1] = (ZnSpuRegs[0x1aa / 2] & 0x4000) ? out[1] * 35 / 100 : 0;
}
// The driver runs at 60 Hz: exactly 735 native samples per frame. Hardware
// RAM/IRQ progress must not depend on host output rate or sound being enabled.
static void Render(INT32 end)
{
	if (end > 735)
		end = 735;
	while (position < end) {
		Sample();
		frame[position * 2] = last[0];
		frame[position * 2 + 1] = last[1];
		position++;
	}
}
static void NewFrame()
{
	frame.resize(735 * 2);
	position = 0;
	frameStart[0] = last[0];
	frameStart[1] = last[1];
}
static void Mix()
{
	Render(735);
	if (nBurnSoundRate <= 0 || nBurnSoundLen <= 0)
		return;
	INT32 native = 0;
	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		resample += 44100;
		while (resample >= (UINT32)nBurnSoundRate) {
			resample -= nBurnSoundRate;
			native++;
		}
		for (INT32 ch = 0; ch < 2; ch++) {
			INT32 value = native ? frame[((native > 735 ? 735 : native) - 1) * 2 + ch] : frameStart[ch];
			if (pBurnSoundOut)
				pBurnSoundOut[i * 2 + ch] = Clip(pBurnSoundOut[i * 2 + ch] + value);
		}
	}
}
static void Reset()
{
	memset(&reverb, 0, sizeof(reverb));
	reverb.dirty = 1;
	reverbAddress = reverbChannel = 0;
	capturePosition = 0;
	frameStart[0] = frameStart[1] = 0;
	memset(volumes, 0, sizeof(volumes));
	noiseCounter = 0;
	noiseLevel = 1;
	memset(voices, 0, sizeof(voices));
	resample = position = 0;
	last[0] = last[1] = 0;
	generate_linear_rate_table();
	generate_pos_exp_rate_table();
	generate_neg_exp_rate_table();
	generate_decay_rate_table();
	generate_linear_release_rate_table();
	generate_exp_release_rate_table();
}
static void Write(UINT32 offset, UINT16 data)
{
	if (offset == 0x1a2)
		reverbAddress = (UINT32)data * 4;
	if (offset == 0x1aa) {
		ZnSpuRegs[0x1ae / 2] = (ZnSpuRegs[0x1ae / 2] & ~0x3f) | (data & 0x3f);
		if (!(data & 0x40))
			ZnSpuRegs[0x1ae / 2] &= ~0x40;
	}
	if ((offset < 0x180 && (offset & 15) < 4) || offset == 0x180 || offset == 0x182) {
		INT32 index = offset < 0x180 ? (offset / 16) * 2 + (offset & 2) / 2 : 48 + (offset & 2) / 2;
		volumes[index].counter = 0;
		if (!(data & 0x8000))
			volumes[index].value = (INT16)(data << 1);
	}
	if (offset >= 0x1c0)
		reverb.dirty = 1;
	if (offset >= 0x188 && offset <= 0x18e) {
		INT32 base = (offset & 2) ? 16 : 0;
		for (INT32 b = 0; b < 16 && base + b < 24; b++)
			if (data & (1 << b)) {
				if (offset < 0x18c)
					KeyOn(base + b);
				else if (voices[base + b].stage < 4) {
					voices[base + b].stage = 3;
					voices[base + b].remaining = 0;
				}
			}
	}
	if (offset < 0x180 && (offset & 15) == 14)
		voices[offset / 16].loop = (data << 3) & 0x7ffff;
}
static void Scan()
{
	SCAN_VAR(reverbAddress);
	SCAN_VAR(reverbChannel);
	SCAN_VAR(capturePosition);
	SCAN_VAR(volumes);
	SCAN_VAR(noiseCounter);
	SCAN_VAR(noiseLevel);
	SCAN_VAR(reverb);
	SCAN_VAR(voices);
	SCAN_VAR(resample);
	SCAN_VAR(last);
}
}
