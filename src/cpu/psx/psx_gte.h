// license:BSD-3-Clause
// copyright-holders:smf
/*
 * PlayStation Geometry Transformation Engine emulator
 *
 * Copyright 2003-2013 smf
 *
 */

#ifndef FBNEO_PSX_GTE_H
#define FBNEO_PSX_GTE_H

#pragma once

#include "burnint.h"
#include <stdint.h>

union GTEPAIR
{
	struct { UINT8 l, h, h2, h3; } b;
	struct { UINT16 l, h; } w;
	struct { INT8 l, h, h2, h3; } sb;
	struct { INT16 l, h; } sw;
	UINT32 d;
	INT32 sd;
};

static inline int64_t gte_sext(int64_t value, int bits)
{
	const int64_t sign = (int64_t)1 << (bits - 1);
	const int64_t mask = ((int64_t)1 << bits) - 1;

	value &= mask;
	return (value ^ sign) - sign;
}

#define GTE_SF( op ) ( ( op >> 19 ) & 1 )
#define GTE_MX( op ) ( ( op >> 17 ) & 3 )
#define GTE_V( op ) ( ( op >> 15 ) & 3 )
#define GTE_CV( op ) ( ( op >> 13 ) & 3 )
#define GTE_LM( op ) ( ( op >> 10 ) & 1 )
#define GTE_FUNCT( op ) ( op & 63 )

class gte
{
public:
	gte() : m_sf(0), m_mac0(0), m_mac1(0), m_mac2(0), m_mac3(0)
	{
	}

	GTEPAIR m_cp2cr[ 32 ];
	GTEPAIR m_cp2dr[ 32 ];

	uint32_t getcp2dr( uint32_t pc, int reg );
	void setcp2dr( uint32_t pc, int reg, uint32_t value );
	uint32_t getcp2cr( uint32_t pc, int reg );
	void setcp2cr( uint32_t pc, int reg, uint32_t value );
	int docop2( uint32_t pc, int gteop );

protected:
	class int44
	{
	public:
		int44( int64_t value ) :
			m_value( value ),
			m_positive_overflow( value > 0x7ffffffffff ),
			m_negative_overflow( value < -0x80000000000 )
		{
		}

		int44( int64_t value, bool positive_overflow, bool negative_overflow ) :
			m_value( value ),
			m_positive_overflow( positive_overflow ),
			m_negative_overflow( negative_overflow )
		{
		}

		int44 operator+( int64_t add )
		{
			int64_t value = gte_sext( m_value + add, 44 );

			return int44( value,
				m_positive_overflow || ( value < 0 && m_value >= 0 && add >= 0 ),
				m_negative_overflow || ( value >= 0 && m_value < 0 && add < 0 ) );
		}

		bool positive_overflow()
		{
			return m_positive_overflow;
		}

		bool negative_overflow()
		{
			return m_negative_overflow;
		}

		int64_t value()
		{
			return m_value;
		}

	private:
		int64_t m_value;
		bool m_positive_overflow;
		bool m_negative_overflow;
	};

	int32_t LIM( int32_t value, int32_t max, int32_t min, uint32_t flag );
	int32_t BOUNDS( int44 a, int max_flag, int min_flag );
	int32_t A1( int44 a );
	int32_t A2( int44 a );
	int32_t A3( int44 a );
	int32_t Lm_B1( int32_t a, int lm );
	int32_t Lm_B2( int32_t a, int lm );
	int32_t Lm_B3( int32_t a, int lm );
	int32_t Lm_B3_sf( int64_t value, int sf, int lm );
	int32_t Lm_C1( int32_t a );
	int32_t Lm_C2( int32_t a );
	int32_t Lm_C3( int32_t a );
	int32_t Lm_D( int64_t a, int sf );
	uint32_t Lm_E( uint32_t result );
	int64_t F( int64_t a );
	int32_t Lm_G1( int64_t a );
	int32_t Lm_G2( int64_t a );
	int32_t Lm_H( int64_t value, int sf );

	int m_sf;
	int64_t m_mac0;
	int64_t m_mac1;
	int64_t m_mac2;
	int64_t m_mac3;
};

#endif // FBNEO_PSX_GTE_H
