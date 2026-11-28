// license:BSD-3-Clause
// copyright-holders:Angelo Salese, Mariusz Wojcieszek
// SCU DSP instruction engine adapted from MAME 0.289 scudsp.cpp.
// Host buses and save-state integration are provided by d_stv.cpp.
namespace stv_dsp {
using s64 = int64_t; using u16 = uint16_t;
static uint32_t rotl_32(uint32_t v, unsigned n) { n &= 31; return (v << n) | (v >> ((32 - n) & 31)); }
static unsigned Bit(uint32_t v, unsigned n) { return (v >> n) & 1; }
static int32_t SignExtend(uint32_t v, unsigned n) { return int32_t(v << (32 - n)) >> (32 - n); }
struct DspRam {
	uint32_t words[256];
	uint32_t read_dword(uint32_t a) { return words[a & 255]; }
	void write_dword(uint32_t a, uint32_t d) { words[a & 255] = d; }
};
class Core {
public:
	enum {
		LEF = 15, // 0000'8000 program counter transfer enable
		EXF,      // 0001'0000 program execute control flag
		ESF,      // 0002'0000 step execute control
		EF,       // 0004'0000 program end interrupt
		VF,       // 0008'0000 oVerflow flag
		CF,       // 0010'0000 Carry flag
		ZF,       // 0020'0000 Zero flag
		SF,       // 0040'0000 Sign flag
		T0F,      // 0080'0000 D0-Bus DMA execution flag
		EPF = 25, // 0200'0000 Execute pause flag
		PRF       // 0400'0000 Execute Pause Reset flag
	};

	union SCUDSPREG32 {
		int32_t  si;
		uint32_t ui;
	};

	union SCUDSPREG16 {
		int16_t  si;
		uint16_t ui;
	};


	uint8_t   m_pc;   /* registers */
	uint32_t  m_flags;  /* flags */
	uint8_t   m_ra;
	uint8_t   m_ct0,m_ct1,m_ct2,m_ct3;
	uint8_t   m_delay;                                   /* Delay */
	uint8_t   m_top;                                     /*Jump Command memory*/
	uint16_t  m_lop;                                    /*Counter Register*/   /*12-bits*/
	SCUDSPREG32 m_rx;                                /*X-Bus register*/
	int64_t   m_mul;                                     /*Multiplier register*//*48-bits*/
	SCUDSPREG32 m_ry;                                /*Y-Bus register*/
	int64_t   m_alu;                                    /*ALU register*/       /*48-bits*/
	SCUDSPREG16 m_ph;                                /*ALU high register*/
	SCUDSPREG32 m_pl;                                /*ALU low register*/
	SCUDSPREG16 m_ach;                               /*ALU external high register*/
	SCUDSPREG32 m_acl;                               /*ALU external low register*/
	uint32_t  m_ra0,m_wa0;                                /*DSP DMA registers*/
	struct{
		uint32_t src, dst;
		uint16_t add;
		uint16_t size, update, ex, dir, count;
	}m_dma;
	DspRam m_program;
	DspRam m_data;
	int m_icount;
	uint8_t m_update_mul;

	uint32_t get_source_mem_reg_value( uint32_t mode );
	uint32_t get_source_mem_value(uint8_t mode);
	void set_dest_mem_reg( uint32_t mode, uint32_t value );
	void set_dest_mem_reg_2( uint32_t mode, uint32_t value );
	uint32_t compute_condition( uint32_t condition );
	uint32_t get_mem_source_dma( uint32_t memcode, uint32_t counter );
	void set_dest_dma_mem( uint32_t memcode, uint32_t value, uint32_t counter );

	void op_illegal(uint32_t opcode);
	void op_alu(uint32_t opcode);
	void op_move_immediate(uint32_t opcode);
	void op_dma(uint32_t opcode);
	void op_jump(uint32_t opcode);
	void op_loop(uint32_t opcode);
	void op_end(uint32_t opcode);
	void exec_dma();
	uint8_t m_illegal;
	void reset() { memset(this, 0, sizeof(*this)); }
	void run(int cycles) { if (Bit(m_flags, EXF)) { m_icount = cycles; execute_run(); } }
	void execute_run();
	uint32_t program_control_r();
	void program_control_w(uint32_t offset, uint32_t data, uint32_t mem_mask = ~0U);
	void program_w(uint32_t data);
	void ram_address_control_w(uint32_t data);
	uint32_t ram_address_r();
	void ram_address_w(uint32_t data);
	void m_out_irq_cb(int state) { if (state) ScuRequest(5); }
	uint16_t m_in_dma_cb(uint32_t a) { return (Sh2ReadByte(a) << 8) | Sh2ReadByte(a + 1); }
	void m_out_dma_cb(uint32_t a, uint16_t d) { Sh2WriteByte(a, d >> 8); Sh2WriteByte(a + 1, d); }
};
/* FLAGS */

#define SET_C(_val) (m_flags = ((m_flags & ~0x00100000) | ((_val) ? 0x00100000 : 0)))
#define SET_S(_val) (m_flags = ((m_flags & ~0x00400000) | ((_val) ? 0x00400000 : 0)))
#define SET_Z(_val) (m_flags = ((m_flags & ~0x00200000) | ((_val) ? 0x00200000 : 0)))
#define SET_V(_val) (m_flags = ((m_flags & ~0x00080000) | ((_val) ? 0x00080000 : 0)))


#define FLAGS_MASK 0x06ff8000
#define INSTA_DMA_HACK 1

#define scudsp_readop(A) m_program.read_dword(A)
#define scudsp_writeop(A, B) m_program.write_dword(A, B)
#define scudsp_readmem(A,MD) m_data.read_dword(A | (MD << 6))
#define scudsp_writemem(A,MD,B) m_data.write_dword(A | (MD << 6), B)

constexpr uint64_t concat_64(uint32_t hi, uint32_t lo) { return (uint64_t(hi) << 32) | lo; }

uint32_t Core::get_source_mem_reg_value( uint32_t mode )
{
	if ( mode < 0x8 )
	{
		return get_source_mem_value( mode );
	}
	else
	{
		switch( mode )
		{
			case 0x9:
				return uint32_t((m_alu & 0x00000000ffffffffU) >> 0);
			case 0xA:
				return uint32_t((m_alu & 0x0000ffffffff0000U) >> 16);
		}
	}
	return 0;
}

uint32_t Core::get_source_mem_value(uint8_t mode)
{
	uint32_t value = 0;

	switch( mode )
	{
		case 0x0:   /* M0 */
			value = scudsp_readmem(m_ct0,0);
			break;
		case 0x1:   /* M1 */
			value = scudsp_readmem(m_ct1,1);
			break;
		case 0x2:   /* M2 */
			value = scudsp_readmem(m_ct2,2);
			break;
		case 0x3:   /* M3 */
			value = scudsp_readmem(m_ct3,3);
			break;
		case 0x4:   /* MC0 */
			value = scudsp_readmem(m_ct0++,0);
			m_ct0 &= 0x3f;
			break;
		case 0x5:   /* MC1 */
			value = scudsp_readmem(m_ct1++,1);
			m_ct1 &= 0x3f;
			break;
		case 0x6:   /* MC2 */
			value = scudsp_readmem(m_ct2++,2);
			m_ct2 &= 0x3f;
			break;
		case 0x7:   /* MC3 */
			value = scudsp_readmem(m_ct3++,3);
			m_ct3 &= 0x3f;
			break;
	}

	return value;
}

void Core::set_dest_mem_reg( uint32_t mode, uint32_t value )
{
	switch( mode )
	{
		case 0x0:   /* MC0 */
			scudsp_writemem(m_ct0++,0,value);
			m_ct0 &= 0x3f;
			break;
		case 0x1:   /* MC1 */
			scudsp_writemem(m_ct1++,1,value);
			m_ct1 &= 0x3f;
			break;
		case 0x2:   /* MC2 */
			scudsp_writemem(m_ct2++,2,value);
			m_ct2 &= 0x3f;
			break;
		case 0x3:   /* MC3 */
			scudsp_writemem(m_ct3++,3,value);
			m_ct3 &= 0x3f;
			break;
		case 0x4:   /* RX */
			m_rx.ui = value;
			break;
		case 0x5:   /* PL */
			m_pl.ui = value;
			m_ph.si = (m_pl.si < 0) ? -1 : 0;
			break;
		case 0x6:   /* RA0 */
			m_ra0 = value;
			break;
		case 0x7:   /* WA0 */
			m_wa0 = value;
			break;
		case 0x8:
		case 0x9:
			/* ??? */
			break;
		case 0xa:   /* LOP */
			m_lop = value;
			break;
		case 0xb:   /* TOP */
			m_top = value;
			break;
		case 0xc:   /* CT0 */
			m_ct0 = value & 0x3f;
			break;
		case 0xd:   /* CT1 */
			m_ct1 = value & 0x3f;
			break;
		case 0xe:   /* CT2 */
			m_ct2 = value & 0x3f;
			break;
		case 0xf:   /* CT3 */
			m_ct3 = value & 0x3f;
			break;
	}
}

void Core::set_dest_mem_reg_2( uint32_t mode, uint32_t value )
{
	if ( mode < 0xb )
	{
		set_dest_mem_reg( mode, value );
	}
	else
	{
		switch( mode )
		{
			case 0xc:   /* PC */
				m_delay = m_pc;  /* address next after this command will be executed twice */
				m_top = m_pc;
				m_pc = value;
				break;
		}
	}
}

uint32_t Core::compute_condition( uint32_t condition )
{
	uint32_t result = 0;

	switch( condition & 0xf )
	{
		case 0x1:   /* Z */
			result = Bit(m_flags, ZF);
			break;
		case 0x2:   /* S */
			result = Bit(m_flags, SF);
			break;
		case 0x3:   /* ZS */
			result = Bit(m_flags, ZF) | Bit(m_flags, SF);
			break;
		case  0x4:  /* C */
			result = Bit(m_flags, CF);
			break;
		case 0x8:   /* T0 */
			result = Bit(m_flags, T0F);
			break;
	}
	if ( !(condition & 0x20) )
	{
		result = !result;
	}

	return result;
}

void Core::set_dest_dma_mem( uint32_t memcode, uint32_t value, uint32_t counter )
{
	if ( memcode < 4 )
	{
		switch(memcode)
		{
			case 0x0:   /* MC0 */
				scudsp_writemem(((m_ct0 + counter) & 0x3f),0,value);
				break;
			case 0x1:   /* MC1 */
				scudsp_writemem(((m_ct1 + counter) & 0x3f),1,value);
				break;
			case 0x2:   /* MC2 */
				scudsp_writemem(((m_ct2 + counter) & 0x3f),2,value);
				break;
			case 0x3:   /* MC3 */
				scudsp_writemem(((m_ct3 + counter) & 0x3f),3,value);
				break;
		}
	}
	else if ( memcode == 4 )
	{
		m_illegal = true;
		/* caused a stack overflow for sure ... */
		//dsp_reg.internal_prg[ counter & 0x100 ] = value;
	}
}

uint32_t Core::get_mem_source_dma( uint32_t memcode, uint32_t counter )
{
	switch( memcode & 0x3 )
	{
		case 0x0:
			return scudsp_readmem(((m_ct0 + counter) & 0x3f),0);
		case 0x1:
			return scudsp_readmem(((m_ct1 + counter) & 0x3f),1);
		case 0x2:
			return scudsp_readmem(((m_ct2 + counter) & 0x3f),2);
		case 0x3:
			return scudsp_readmem(((m_ct3 + counter) & 0x3f),3);
	}
	return 0;
}


uint32_t Core::program_control_r()
{
	const uint32_t flags = m_flags & FLAGS_MASK;

	if (true)
	{
		// clear overflow and end flag on host reads of this port
		m_flags &= ~(1 << VF);
		m_flags &= ~(1 << EF);
		m_out_irq_cb(0);
	}

	return ((m_pc + 1) & 0xff) | flags;
}

void Core::program_control_w(uint32_t offset, uint32_t data, uint32_t mem_mask)
{
	uint32_t oldval, newval;

	oldval = (m_flags & 0xffffff00) | (m_pc & 0xff);
	newval = oldval;
	newval = (newval & ~mem_mask) | (data & mem_mask);

	m_flags = (newval & 0x00630000) | (m_flags & ~0x00630000);



	// set new PC if transfer enable is set
	// NOTE: doesn't get transfered in flags
	if (Bit(data, LEF) && (mem_mask & 0xffff))
		m_pc = newval & 0xff;

	//printf("%08x PRG CTRL\n",data);
	// run DSP if EXF is on

}

void Core::program_w(uint32_t data)
{
	//printf("%02x %08x PRG\n",m_pc,data);
	scudsp_writeop(m_pc++, data);
}

void Core::ram_address_control_w(uint32_t data)
{
	//printf("%02x %08x PRG\n",m_pc,data);
	// NOTE: RA has no relationship with CT0 ~ CT3, can upload out of high 2 bits bounds
	m_ra = data & 0xff;
}

uint32_t Core::ram_address_r()
{
	uint32_t data = m_data.read_dword(m_ra);

	if (true)
		m_ra = (m_ra + 1) & 0xff;

	return data;
}

void Core::ram_address_w(uint32_t data)
{
//  set_dest_mem_reg( (m_ra & 0xc0) >> 6, data );
	m_data.write_dword(m_ra, data);

	m_ra = (m_ra + 1) & 0xff;

}

void Core::op_alu(uint32_t opcode)
{
	int64_t i1,i2;
	int32_t i3;
	int update_ct[4] = {0,0,0,0};
	int dsp_mem;


	/* ALU */
	// NOTE: anything but AD2 doesn't update upper 16-bit ALU part
	switch( (opcode & 0x3c000000) >> 26 )
	{
		case 0x0:   /* NOP */
			break;

		case 0x1:   /* AND */
			i3 = m_acl.si & m_pl.si;
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_C(0);
			SET_S(i3 < 0);
			break;

		case 0x2:   /* OR */
			i3 = m_acl.si | m_pl.si;
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_C(0);
			SET_S(i3 < 0);
			/* TODO: Croc and some early Psygnosis games wants Z to be 1 when the result of this one is negative.
			         Needs HW tests ... */
			if(i3 < 0)
				i3 = 0;
			SET_Z(i3 == 0);
			break;

		case 0x3:   /* XOR */
			i3 = m_acl.si ^ m_pl.si;
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_C(0);
			SET_S(i3 < 0);
			break;

		case 0x4:   /* ADD */
			i3 = m_acl.si + m_pl.si;
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z( (i3 & s64(0xffffffffffffU)) == 0 );
			SET_S( i3 & s64(0x1000000000000U));
			SET_C(i3 & s64(0x100000000U));
			SET_V((i3 ^ m_acl.si) & (i3 ^ m_pl.si) & 0x80000000);
			break;

		case 0x5:   /* SUB */
			i3 = m_acl.si - m_pl.si;
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_C(i3 & s64(0x100000000U));
			SET_S(i3 < 0);
			SET_V(((m_pl.si) ^ (m_acl.si)) & ((m_pl.si) ^ (i3)) & 0x80000000);
			break;

		case 0x6:   /* AD2 */
			i1 = concat_64(int32_t(m_ph.si), m_pl.si);
			i2 = concat_64(int32_t(m_ach.si), m_acl.si);
			m_alu = i1 + i2;
			SET_Z((m_alu & s64(0xffffffffffffU)) == 0);
			SET_S((m_alu & s64(0x800000000000U)) > 0);
			SET_C(m_alu & s64(0x1000000000000U));
			SET_V((m_alu ^ i1) & (m_alu ^ i2) & s64(0x800000000000U));
			break;

		case 0x7:   /* ??? */
			/* Unrecognized opcode */
			break;

		case 0x8:   /* SR */
			// MSB does not change
			i3 = (m_acl.si >> 1) | (m_acl.si & 0x80000000);
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_S(i3 < 0);
			SET_C(m_acl.ui & 0x80000000);
			break;

		case 0x9:   /* RR */
			i3 = ((m_acl.ui >> 1) & 0x7fffffff) | ((m_acl.ui << 31) & 0x80000000);
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_S(i3 < 0);
			SET_C(m_acl.ui & 0x1);
			break;

		case 0xa:   /* SL */
			i3 = m_acl.si << 1;
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_S(i3 < 0);
			SET_C(m_acl.ui & 0x80000000);
			break;

		case 0xb:   /* RL */
			i3 = ((m_acl.si << 1) & 0xfffffffe) | ((m_acl.si >> 31) & 0x1);
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_S(i3 < 0);
			SET_C(m_acl.ui & 0x80000000);
			break;

		case 0xc:
		case 0xd:
		case 0xe:
			/* Unrecognized opcodes */
			break;

		case 0xf:   /* RL8 */
			i3 = rotl_32(m_acl.si, 8);
			m_alu = uint32_t(i3 & 0xffffffff) | (m_alu & 0xffff00000000);
			SET_Z(i3 == 0);
			SET_S(i3 < 0);
			SET_C(m_acl.si & 0x01000000);
			break;
	}

	/* X-Bus */
	if (opcode & 0x2000000)
	{
		/* MOV [s],X */
		dsp_mem = (opcode & 0x700000) >> 20;
		if (dsp_mem & 4)
		{
			dsp_mem &= 3;
			update_ct[dsp_mem] = 1;
		}
		m_rx.ui = get_source_mem_value( dsp_mem );
		m_update_mul = 1;
	}
	switch ((opcode & 0x1800000) >> 23)
	{
		case 0x0:   /* NOP */
		case 0x1:   /* NOP ? */
			break;
		case 0x2:   /* MOV MUL,P */
			m_ph.ui = u16((m_mul & 0x0000ffff00000000U) >> 32);
			m_pl.ui = uint32_t((m_mul & 0x00000000ffffffffU) >> 0);
			break;
		case 0x3:   /* MOV [s],P */
			dsp_mem = (opcode & 0x700000) >> 20;
			if (dsp_mem & 4)
			{
				dsp_mem &= 3;
				update_ct[dsp_mem] = 1;
			}
			m_pl.ui = get_source_mem_value(  dsp_mem );
			m_ph.si = (m_pl.si < 0) ? -1 : 0;
			break;
	}

	/* Y-Bus */
	if (opcode & 0x80000)
	{
		/* MOV [s],Y */
		dsp_mem = (opcode & 0x1C000 ) >> 14;
		if (dsp_mem & 4)
		{
			dsp_mem &= 3;
			update_ct[dsp_mem] = 1;
		}
		m_ry.ui = get_source_mem_value( dsp_mem );
		m_update_mul = 1;
	}
	switch ((opcode & 0x60000) >> 17)
	{
		case 0x0:   /* NOP */
			break;
		case 0x1:   /* CLR A */
			m_acl.ui = 0;
			m_ach.ui = 0;
			break;
		case 0x2:   /* MOV ALU,A */
			m_ach.ui = u16((m_alu & 0x0000ffff00000000U) >> 32);
			m_acl.ui = uint32_t((m_alu & 0x00000000ffffffffU) >> 0);
			break;
		case 0x3:   /* MOV [s], A */
			dsp_mem = (opcode & 0x1C000 ) >> 14;
			if (dsp_mem & 4)
			{
				dsp_mem &= 3;
				update_ct[dsp_mem] = 1;
			}
			m_acl.ui = get_source_mem_value( dsp_mem );
			m_ach.si = ((m_acl.si < 0) ? -1 : 0);
			break;
	}

	/* update CT registers */
	if (update_ct[0]) { m_ct0++; m_ct0 &= 0x3f; };
	if (update_ct[1]) { m_ct1++; m_ct1 &= 0x3f; };
	if (update_ct[2]) { m_ct2++; m_ct2 &= 0x3f; };
	if (update_ct[3]) { m_ct3++; m_ct3 &= 0x3f; };


	/* D1-Bus */
	switch( (opcode & 0x3000) >> 12 )
	{
		case 0x0:   /* NOP */
			break;
		case 0x1:   /* MOV SImm,[d] */
			set_dest_mem_reg((opcode & 0xf00) >> 8, int32_t(int8_t(opcode & 0xff)));
			break;
		case 0x2:
			/* ??? */
			break;
		case 0x3:   /* MOV [s],[d] */
			set_dest_mem_reg((opcode & 0xf00) >> 8, get_source_mem_reg_value(opcode & 0xf));
			break;
	}

	m_icount -= 1;
}

void Core::op_move_immediate( uint32_t opcode )
{
	uint32_t value;

	if ( opcode & 0x2000000 )
	{
		if ( compute_condition( (opcode & 0x3F80000 ) >> 19 ) )
		{
			value = SignExtend( opcode, 19 );
			set_dest_mem_reg_2( (opcode & 0x3C000000) >> 26, value );
		}
	}
	else
	{
		value = SignExtend( opcode, 25 );
		set_dest_mem_reg_2( (opcode & 0x3C000000) >> 26, value );
	}
	m_icount -= 1;
}

void Core::op_dma( uint32_t opcode )
{
	uint8_t hold = (opcode &  0x4000) >> 14;
	uint32_t add = (opcode & 0x38000) >> 15;
	uint32_t dir_from_D0 = (opcode & 0x1000 ) >> 12;
	uint32_t dsp_mem = (opcode & 0x300) >> 8;

	m_flags |= 1 << T0F;

	if ( opcode & 0x2000 )
	{
		m_dma.size = get_source_mem_value( opcode & 0xf );
		switch ( add & 0x7 )
		{
			case 0: m_dma.add = 0; break;
			case 1: m_dma.add = 4; break;
			default: m_dma.add = 4; break;
		}
	}
	else
	{
		m_dma.size = opcode & 0xff;
		switch( add )
		{
			case 0: m_dma.add = 0; break;  /* 0 */
			case 1: m_dma.add = 4; break;  /* 1 */
			case 2: m_dma.add = 4; break;  /* 2 */
			case 3: m_dma.add = 16; break; /* 4 */
			case 4: m_dma.add = 16; break;  /* 8 */
			case 5: m_dma.add = 64; break; /* 16 */
			case 6: m_dma.add = 128; break; /* 32 */
			case 7: m_dma.add = 256; break; /* 64 */
		}
	}

	m_dma.dir = dir_from_D0;
	if ( m_dma.dir == 0 )
	{
		m_dma.src = (m_ra0 << 2) & 0x07ffffff;
		m_dma.dst = dsp_mem;
	}
	else
	{
		m_dma.src = dsp_mem;
		m_dma.dst = (m_wa0 << 2) & 0x07ffffff;
	}

	m_dma.update = ( hold == 0 );
	m_dma.ex = 1;
	m_dma.count = 0;
	// HACK: It looks like that scheduling craps out the m_dma parameters
	// this can be verified with stv:vfremix, where the DMA transfers are very small
	// and no T0F is checked (?)
	#if INSTA_DMA_HACK
	{
		uint32_t data;
		if ( m_dma.dir == 0 )
		{
			for(m_dma.count = 0;m_dma.count < m_dma.size; m_dma.count++)
			{
				data = (m_in_dma_cb(m_dma.src)<<16) | m_in_dma_cb(m_dma.src+2);
				set_dest_dma_mem( m_dma.dst, data, m_dma.count );

				m_dma.src += m_dma.add;

				if ( m_dma.update )
				{
					m_ra0 += ((1 * m_dma.add) >> 2);
				}
			}
		}
		else
		{
			for(m_dma.count = 0;m_dma.count < m_dma.size; m_dma.count++)
			{
				data = get_mem_source_dma( m_dma.src, m_dma.count );

				m_out_dma_cb(m_dma.dst, data >> 16 );
				m_out_dma_cb(m_dma.dst+2, data & 0xffff );

				m_dma.dst += m_dma.add;

				if ( m_dma.update )
				{
					m_wa0 += ((1 * m_dma.add) >> 2);
				}
			}
		}

		//if(m_dma.count >= m_dma.size)
		{
			m_dma.ex = 0;
			m_flags &= ~(1 << T0F);
		}

		m_icount -= m_dma.size;
	}
	#endif


	//printf("SRC %08x DST %08x SIZE %08x UPDATE %08x DIR %08x ADD %08x\n",m_dma.src,m_dma.dst,m_dma.size,m_dma.update,m_dma.dir,m_dma.add);

	m_icount -= 1;
}

void Core::op_jump( uint32_t opcode )
{
	if ( opcode & 0x3f80000 )
	{
		if ( compute_condition( (opcode & 0x3f80000) >> 19 ) )
		{
			m_delay = m_pc;
			m_pc = opcode & 0xff;
		}
	}
	else
	{
		m_delay = m_pc;
		m_pc = opcode & 0xff;
	}

	m_icount -= 1;
}

void Core::op_loop(uint32_t opcode)
{
	if ( opcode & 0x8000000 )
	{
		/* LPS */
		if ( m_lop != 0 )
		{
			m_lop--;
			m_delay = m_pc;
			m_pc--;
		}
	}
	else
	{
		/* BTM */
		if ( m_lop != 0 )
		{
			m_lop--;
			m_delay = m_pc;
			m_pc = m_top;
		}
	}
	m_icount -= 1;
}

void Core::op_end(uint32_t opcode)
{
	if(opcode & 0x08000000)
	{
		// set program end irq flag
		m_flags |= (1 << EF);
		m_out_irq_cb(1);
	}

	// clear the execute control flag (not running anymore)
	m_flags &= ~(1 << EXF);
	m_icount = 0;
	m_icount -= 1;
}

void Core::op_illegal(uint32_t opcode)
{
	m_flags &= ~(1U << EXF);
	m_icount = 0;
	m_illegal = 1;
	m_icount -= 1;
}

// TODO: unused really
void Core::exec_dma()
{
	uint32_t data;
	if ( m_dma.dir == 0 )
	{
		data = (m_in_dma_cb(m_dma.src)<<16) | m_in_dma_cb(m_dma.src+2);
		set_dest_dma_mem( m_dma.dst, data, m_dma.count );

		m_dma.src += m_dma.add;

		if ( m_dma.update )
		{
			m_ra0 += ((1 * m_dma.add) >> 2);
		}
	}
	else
	{
		data = get_mem_source_dma( m_dma.src, m_dma.count );

		m_out_dma_cb(m_dma.dst, data >> 16 );
		m_out_dma_cb(m_dma.dst+2, data & 0xffff );

		m_dma.dst += m_dma.add;

		if ( m_dma.update )
		{
			m_wa0 += ((1 * m_dma.add) >> 2);
		}
	}

	m_dma.count++;
	if(m_dma.count >= m_dma.size)
	{
		m_dma.ex = 0;
		m_flags &= ~(1 << T0F);
	}

	m_icount -= 1;
}

/* Execute cycles */
void Core::execute_run()
{
	uint32_t opcode;

	do
	{
		m_update_mul = 0;

		if ( m_delay )
		{
			opcode = scudsp_readop(m_delay);
			m_delay = 0;
		}
		else
		{
			opcode = scudsp_readop(m_pc);
			m_pc++;
		}

		switch( (opcode & 0xc0000000) >> 30 )
		{
			case 0x00: /* 00 */
				op_alu(opcode);
				break;
			case 0x01: /* 01 */
				op_illegal(opcode);
				break;
			case 0x02: /* 10 */
				op_move_immediate(opcode);
				break;
			case 0x03: /* 11 */
				switch( (opcode & 0x30000000) >> 28 )
				{
					case 0x00:
						op_dma(opcode);
						break;
					case 0x01:
						op_jump(opcode);
						break;
					case 0x02:
						op_loop(opcode);
						break;
					case 0x03:
						op_end(opcode);
						break;
				}
				break;
		}

		if ( m_update_mul == 1 )
		{
			m_mul = (int64_t)m_rx.si * (int64_t)m_ry.si;
			m_update_mul = 0;
		}

		if (m_dma.ex == 1)
		{
			exec_dma();
		}

	} while (m_icount > 0 && Bit(m_flags, EXF));
}


#undef SET_C
#undef SET_S
#undef SET_Z
#undef SET_V
#undef FLAGS_MASK
#undef INSTA_DMA_HACK
#undef scudsp_readop
#undef scudsp_writeop
#undef scudsp_readmem
#undef scudsp_writemem
} // namespace stv_dsp
