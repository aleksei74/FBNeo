// license:BSD-3-Clause
// copyright-holders:ElSemi, R. Belmont
// MAME 0.289 SCSP adapted for the FB Neo ST-V driver.
#include <algorithm>
#include <iterator>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace stv_scsp {
static unsigned BIT(uint32_t x,unsigned n){return (x>>n)&1;}
namespace util { static int32_t sext(uint32_t x, int bits) { return int32_t(x << (32 - bits)) >> (32 - bits); } }
using u8=uint8_t; using u16=uint16_t; using u32=uint32_t;
using s8=int8_t; using s16=int16_t; using s32=int32_t; using s64=int64_t; using offs_t=uint32_t;
template<class T> static T clamp(T x,T lo,T hi) { return std::max(lo,std::min(hi,x)); }
// Bit widths and truncation follow SCSP_pkg.sv MDCalc.
static s32 Modulation(s16 x, s16 y, unsigned level)
{
	if (level <= 4) return 0;
	s32 sum = s32(x) + s32(y);
	s32 md = util::sext(u32((sum >> 1) * 128), 23) >> ((16 - level) & 15);
	return util::sext(u32(md), 17) & ~1;
}
static void DeviceLog(const char*,...) {}
static void popmessage(const char*,...) {}
struct Ram {
 u8 *data=nullptr;
 u8 read_byte(u32 a)const { return data[(a & 0xfffff) ^ 3]; }
 u16 read_word(u32 a)const { return (read_byte(a)<<8)|read_byte(a+1); }
 void write_word(u32 a,u16 d) { data[(a & 0xfffff)^3]=d>>8;data[((a+1)&0xfffff)^3]=d; }
};
struct Irq {
 int level=0;
 bool isunset()const{return false;}
 void operator()(int l,int state){if(state)level=l;else if(!l||level==l)level=0;}
};
struct MainIrq { int level=0;void operator()(int v){level=v;} };
struct sound_stream {
 int16_t left=0,right=0;
 double gain[2]={0,0};
 int samples()const{return 1;}
 float get(int,int)const{return 0;}
 void put_int_clamp(int ch,int,int v,int scale){int16_t out=clamp(int(double(v)*32768.0/scale*gain[ch]),-32768,32767);if(ch)right=out;else left=out;}
};
#define SCSP_FM_DELAY 0
#define ASSERT_LINE 1
#define CLEAR_LINE 0
#define HOLD_LINE 2
#define COMBINE_DATA(p) (*(p)=(*(p)&~mem_mask)|(data&mem_mask))
struct SCSPDSP
{
//Config
	Ram *space;
	u32 RBP; //Ring buf pointer
	u32 RBL; //Delay ram (Ring buffer) size in words

//context

	s16 COEF[64];     //16 bit signed
	u16 MADRS[32];   //offsets (in words), 16 bit
	u16 MPRO[128*4]; //128 steps 64 bit
	s32 TEMP[128];    //TEMP regs,24 bit signed
	s32 MEMS[32]; //MEMS regs,24 bit signed
	u32 DEC;

//input
	s32 MIXS[16]; //MIXS, 24 bit signed
	s16 EXTS[2];  //External inputs (CDDA)    16 bit signed

//output
	s16 EFREG[16];    //EFREG, 16 bit signed

	bool Stopped;
	int LastStep;

	void Init();
	void SetSample(s32 sample, s32 SEL, s32 MXL);
	void Step();
	void Start();
};

// license:BSD-3-Clause
// copyright-holders:ElSemi, R. Belmont



namespace {

u16 PACK(s32 val)
{
	int const sign = BIT(val, 23);
	u32 temp = (val ^ (val << 1)) & 0xFFFFFF;
	int exponent = 0;
	for (int k = 0; k < 12; k++)
	{
		if (temp & 0x800000)
			break;
		temp <<= 1;
		exponent += 1;
	}
	if (exponent < 12)
		val = (val << exponent) & 0x3FFFFF;
	else
		val <<= 11;
	val >>= 11;
	val &= 0x7FF;
	val |= sign << 15;
	val |= exponent << 11;

	return u16(val);
}

static s32 UNPACK(u16 val)
{
	int const sign = BIT(val, 15);
	int exponent = (val >> 11) & 0xF;
	int const mantissa = val & 0x7FF;
	s32 uval = mantissa << 11;
	if (exponent > 11)
	{
		exponent = 11;
		uval |= sign << 22;
	}
	else
	{
		uval |= (sign ^ 1) << 22;
	}
	uval |= sign << 23;
	uval <<= 8;
	uval >>= 8;
	uval >>= exponent;

	return uval;
}

} // anonymous namespace


void SCSPDSP::Init()
{
	std::memset(this, 0, sizeof(*this));
	RBL = (8*1024); // Initial RBL is 0
	Stopped = true;
}

void SCSPDSP::Step()
{
	if (Stopped)
		return;

	std::fill(std::begin(EFREG), std::end(EFREG), 0);

#if 0
	int dump=0;
	FILE *f=nullptr;
	if(dump)
		f=fopen("dsp.txt","wt");
#endif

	s32 ACC = 0;    //26 bit
	s32 MEMVAL = 0;
	s32 FRC_REG = 0;    //13 bit
	s32 Y_REG = 0;      //24 bit
	u32 ADRS_REG = 0;  //13 bit

	for (int step = 0; step < /*128*/LastStep; ++step)
	{
		u16 *const IPtr = MPRO + (step * 4);

		//if (!IPtr[0] && !IPtr[1] && !IPtr[2] && !IPtr[3])
			//break;

		u32 const TRA   = (IPtr[0] >>  8) & 0x7f;
		u32 const TWT   = (IPtr[0] >>  7) & 0x01;
		u32 const TWA   = (IPtr[0] >>  0) & 0x7f;

		u32 const XSEL  = (IPtr[1] >> 15) & 0x01;
		u32 const YSEL  = (IPtr[1] >> 13) & 0x03;
		u32 const IRA   = (IPtr[1] >>  6) & 0x3f;
		u32 const IWT   = (IPtr[1] >>  5) & 0x01;
		u32 const IWA   = (IPtr[1] >>  0) & 0x1f;

		u32 const TABLE = (IPtr[2] >> 15) & 0x01;
		u32 const MWT   = (IPtr[2] >> 14) & 0x01;
		u32 const MRD   = (IPtr[2] >> 13) & 0x01;
		u32 const EWT   = (IPtr[2] >> 12) & 0x01;
		u32 const EWA   = (IPtr[2] >>  8) & 0x0f;
		u32 const ADRL  = (IPtr[2] >>  7) & 0x01;
		u32 const FRCL  = (IPtr[2] >>  6) & 0x01;
		u32 const SHIFT = (IPtr[2] >>  4) & 0x03;
		u32 const YRL   = (IPtr[2] >>  3) & 0x01;
		u32 const NEGB  = (IPtr[2] >>  2) & 0x01;
		u32 const ZERO  = (IPtr[2] >>  1) & 0x01;
		u32 const BSEL  = (IPtr[2] >>  0) & 0x01;

		u32 const NOFL  = (IPtr[3] >> 15) & 0x01;  //????
		u32 const COEF  = (IPtr[3] >>  9) & 0x3f;

		u32 const MASA  = (IPtr[3] >>  2) & 0x1f;  //???
		u32 const ADREB = (IPtr[3] >>  1) & 0x01;
		u32 const NXADR = (IPtr[3] >>  0) & 0x01;

		//operations are done at 24 bit precision
#if 0
		if (MASA)
			int a=1;
		if (NOFL)
			int a=1;

		//int dump=0;

		if (f)
		{
#define DUMP(v) fprintf(f, " " #v ": %04X", v);
			fprintf(f, "%d: ", step);
			DUMP(ACC);
			DUMP(SHIFTED);
			DUMP(X);
			DUMP(Y);
			DUMP(B);
			DUMP(INPUTS);
			DUMP(MEMVAL);
			DUMP(FRC_REG);
			DUMP(Y_REG);
			DUMP(ADDR);
			DUMP(ADRS_REG);
			fprintf(f, "\n");
#undef DUMP
		}
#endif

		//INPUTS RW
		// colmns97 hits this
		//assert(IRA < 0x32);
		s32 INPUTS; // 24-bit
		if (IRA <= 0x1f)
			INPUTS = MEMS[IRA];
		else if (IRA <= 0x2F)
			INPUTS = MIXS[IRA - 0x20] << 4;  //MIXS is 20 bit
		else if (IRA <= 0x31)
			INPUTS = EXTS[IRA - 0x30] << 8;  //EXTS is 16 bit
		else
			return;

		INPUTS = util::sext(INPUTS, 24);

		if (IWT)
		{
			MEMS[IWA] = MEMVAL;  // MEMVAL was selected in previous MRD
			if (IRA == IWA)
				INPUTS = MEMVAL;
		}

		//Operand sel
		s32 B; // 26-bit
		if (!ZERO)
		{
			if (BSEL)
				B = ACC;
			else
				B = util::sext(TEMP[(TRA + DEC) & 0x7f], 24);
			if (NEGB)
				B = 0 - B;
		}
		else
			B = 0;

		s32 X; // 24-bit
		if (XSEL)
			X = INPUTS;
		else
			X = util::sext(TEMP[(TRA + DEC) & 0x7f], 24);

		s32 Y = 0;  //13 bit
		if (YSEL == 0)
			Y = FRC_REG;
		else if (YSEL == 1)
			Y = this->COEF[COEF] >> 3;   //COEF is 16 bits
		else if (YSEL == 2)
			Y = (Y_REG >> 11) & 0x1fff;
		else if (YSEL == 3)
			Y = (Y_REG >> 4) & 0x0fff;

		if (YRL)
			Y_REG = INPUTS;

		//Shifter
		s32 SHIFTED = 0;    //24 bit
		if (SHIFT == 0)
			SHIFTED = clamp<s32>(ACC, -0x00800000, 0x007fffff);
		else if (SHIFT == 1)
			SHIFTED = clamp<s32>(ACC * 2, -0x00800000, 0x007fffff);
		else if (SHIFT == 2)
			SHIFTED = util::sext(ACC * 2, 24);
		else if (SHIFT == 3)
			SHIFTED = util::sext(ACC, 24);

		//ACCUM
		Y = util::sext(Y, 13);

		s64 const v = (s64(X) * s64(Y)) >> 12;
		ACC = int(v + B);

		if (TWT)
			TEMP[(TWA + DEC) & 0x7f] = SHIFTED;

		if (FRCL)
		{
			if (SHIFT == 3)
				FRC_REG = SHIFTED & 0x0fff;
			else
				FRC_REG = (SHIFTED >> 11) & 0x1fff;
		}

		if (MRD || MWT)
		//if (0)
		{
			u32 ADDR = MADRS[MASA];
			if (!TABLE)
				ADDR += DEC;
			if (ADREB)
				ADDR += ADRS_REG & 0x0FFF;
			if (NXADR)
				ADDR++;
			if (!TABLE)
				ADDR &= RBL - 1;
			else
				ADDR &= 0xffff;
			ADDR += RBP << 12;
			ADDR <<= 1;
			if (MRD && (step & 1)) //memory only allowed on odd? DoA inserts NOPs on even
			{
				if (NOFL)
					MEMVAL = space->read_word(ADDR) << 8;
				else
					MEMVAL = UNPACK(space->read_word(ADDR));
			}
			if (MWT && (step & 1))
			{
				if (NOFL)
					space->write_word(ADDR, SHIFTED >> 8);
				else
					space->write_word(ADDR, PACK(SHIFTED));
			}
		}

		if (ADRL)
		{
			if (SHIFT == 3)
				ADRS_REG = (SHIFTED >> 12) & 0xfff;
			else
				ADRS_REG = INPUTS >> 16;
		}

		if (EWT)
			EFREG[EWA] += SHIFTED >> 8;
	}
	--DEC;
	std::fill(std::begin(MIXS), std::end(MIXS), 0);
	//if (f)
		//fclose(f);
}

void SCSPDSP::SetSample(s32 sample, int SEL, int MXL)
{
	//MIXS[SEL] += sample << (MXL + 1)/*7*/;
	MIXS[SEL] += sample;
	//if (MXL)
		//int a = 1;
}

void SCSPDSP::Start()
{
	Stopped = false;
	int i;
	for (i = 127; i >= 0; --i)
	{
		u16 const *const IPtr = MPRO + (i * 4);
		if (IPtr[0] || IPtr[1] || IPtr[2] || IPtr[3])
			break;
	}
	LastStep = i + 1;
}
#define SHIFT   12
#define LFO_SHIFT   8
#define FIX(v)  ((u32) ((float) (1 << SHIFT) * (v)))


#define EG_SHIFT    16


/*
    SCSP features 32 programmable slots
    that can generate FM and PCM (from ROM/RAM) sound
*/

//SLOT PARAMETERS
#define KEYONEX(slot)   ((slot->udata.data[0x0] >> 0x0) & 0x1000)
#define KEYONB(slot)    ((slot->udata.data[0x0] >> 0x0) & 0x0800)
#define SBCTL(slot)     ((slot->udata.data[0x0] >> 0x9) & 0x0003)
#define SSCTL(slot)     ((slot->udata.data[0x0] >> 0x7) & 0x0003)
#define LPCTL(slot)     ((slot->udata.data[0x0] >> 0x5) & 0x0003)
#define PCM8B(slot)     ((slot->udata.data[0x0] >> 0x0) & 0x0010)

#define SA(slot)        (((slot->udata.data[0x0] & 0xF) << 16) | (slot->udata.data[0x1]))

#define LSA(slot)       (slot->udata.data[0x2])

#define LEA(slot)       (slot->udata.data[0x3])

#define D2R(slot)       ((slot->udata.data[0x4] >> 0xB) & 0x001F)
#define D1R(slot)       ((slot->udata.data[0x4] >> 0x6) & 0x001F)
#define EGHOLD(slot)    ((slot->udata.data[0x4] >> 0x0) & 0x0020)
#define AR(slot)        ((slot->udata.data[0x4] >> 0x0) & 0x001F)

#define LPSLNK(slot)    ((slot->udata.data[0x5] >> 0x0) & 0x4000)
#define KRS(slot)       ((slot->udata.data[0x5] >> 0xA) & 0x000F)
#define DL(slot)        ((slot->udata.data[0x5] >> 0x5) & 0x001F)
#define RR(slot)        ((slot->udata.data[0x5] >> 0x0) & 0x001F)

#define STWINH(slot)    ((slot->udata.data[0x6] >> 0x0) & 0x0200)
#define SDIR(slot)      ((slot->udata.data[0x6] >> 0x0) & 0x0100)
#define TL(slot)        ((slot->udata.data[0x6] >> 0x0) & 0x00FF)

#define MDL(slot)       ((slot->udata.data[0x7] >> 0xC) & 0x000F)
#define MDXSL(slot)     ((slot->udata.data[0x7] >> 0x6) & 0x003F)
#define MDYSL(slot)     ((slot->udata.data[0x7] >> 0x0) & 0x003F)

#define OCT(slot)       ((slot->udata.data[0x8] >> 0xB) & 0x000F)
#define FNS(slot)       ((slot->udata.data[0x8] >> 0x0) & 0x03FF)

#define LFORE(slot)     ((slot->udata.data[0x9] >> 0x0) & 0x8000)
#define LFOF(slot)      ((slot->udata.data[0x9] >> 0xA) & 0x001F)
#define PLFOWS(slot)    ((slot->udata.data[0x9] >> 0x8) & 0x0003)
#define PLFOS(slot)     ((slot->udata.data[0x9] >> 0x5) & 0x0007)
#define ALFOWS(slot)    ((slot->udata.data[0x9] >> 0x3) & 0x0003)
#define ALFOS(slot)     ((slot->udata.data[0x9] >> 0x0) & 0x0007)

#define ISEL(slot)      ((slot->udata.data[0xA] >> 0x3) & 0x000F)
#define IMXL(slot)      ((slot->udata.data[0xA] >> 0x0) & 0x0007)

#define DISDL(slot)     ((slot->udata.data[0xB] >> 0xD) & 0x0007)
#define DIPAN(slot)     ((slot->udata.data[0xB] >> 0x8) & 0x001F)
#define EFSDL(slot)     ((slot->udata.data[0xB] >> 0x5) & 0x0007)
#define EFPAN(slot)     ((slot->udata.data[0xB] >> 0x0) & 0x001F)

//Envelope times in ms
static const double ARTimes[64] = {100000/*infinity*/,100000/*infinity*/,8100.0,6900.0,6000.0,4800.0,4000.0,3400.0,3000.0,2400.0,2000.0,1700.0,1500.0,
					1200.0,1000.0,860.0,760.0,600.0,500.0,430.0,380.0,300.0,250.0,220.0,190.0,150.0,130.0,110.0,95.0,
					76.0,63.0,55.0,47.0,38.0,31.0,27.0,24.0,19.0,15.0,13.0,12.0,9.4,7.9,6.8,6.0,4.7,3.8,3.4,3.0,2.4,
					2.0,1.8,1.6,1.3,1.1,0.93,0.85,0.65,0.53,0.44,0.40,0.35,0.0,0.0};
static const double DRTimes[64] = {100000/*infinity*/,100000/*infinity*/,118200.0,101300.0,88600.0,70900.0,59100.0,50700.0,44300.0,35500.0,29600.0,25300.0,22200.0,17700.0,
					14800.0,12700.0,11100.0,8900.0,7400.0,6300.0,5500.0,4400.0,3700.0,3200.0,2800.0,2200.0,1800.0,1600.0,1400.0,1100.0,
					920.0,790.0,690.0,550.0,460.0,390.0,340.0,270.0,230.0,200.0,170.0,140.0,110.0,98.0,85.0,68.0,57.0,49.0,43.0,34.0,
					28.0,25.0,22.0,18.0,14.0,12.0,11.0,8.5,7.1,6.1,5.4,4.3,3.6,3.1};

#define MEM4B()     ((m_udata.data[0] >> 0x0) & 0x0200)
#define DAC18B()    ((m_udata.data[0] >> 0x0) & 0x0100)
#define MVOL()      ((m_udata.data[0] >> 0x0) & 0x000F)
#define RBL()       ((m_udata.data[1] >> 0x7) & 0x0003)
#define RBP()       ((m_udata.data[1] >> 0x0) & 0x003F)
#define MOFULL()    ((m_udata.data[2] >> 0x0) & 0x1000)
#define MOEMPTY()   ((m_udata.data[2] >> 0x0) & 0x0800)
#define MIOVF()     ((m_udata.data[2] >> 0x0) & 0x0400)
#define MIFULL()    ((m_udata.data[2] >> 0x0) & 0x0200)
#define MIEMPTY()   ((m_udata.data[2] >> 0x0) & 0x0100)

#define SCILV0()    ((m_udata.data[0x24/2] >> 0x0) & 0xff)
#define SCILV1()    ((m_udata.data[0x26/2] >> 0x0) & 0xff)
#define SCILV2()    ((m_udata.data[0x28/2] >> 0x0) & 0xff)

#define SCIEX0  0
#define SCIEX1  1
#define SCIEX2  2
#define SCIMID  3
#define SCIDMA  4
#define SCIIRQ  5
#define SCITMA  6
#define SCITMB  7

#define USEDSP

/* TODO */
//#define dma_transfer_end  ((scsp_regs[0x24/2] & 0x10) >> 4) | (((scsp_regs[0x26/2] & 0x10) >> 4) << 1) | (((scsp_regs[0x28/2] & 0x10) >> 4) << 2)

static const float SDLT[8] = {-1000000.0f,-36.0f,-30.0f,-24.0f,-18.0f,-12.0f,-6.0f,0.0f};

class Core {
public:
 Ram ram;
 int timer_samples[3]={0,0,0};
 uint32_t rng=1;
 uint32_t noise(){rng=rng*1664525+1013904223;return rng;}
 u8 read_byte(u32 a){return ram.read_byte(a);}
 u16 read_word(u32 a){return ram.read_word(a);}
 u32 clock()const{return 22579200;}
 void set_output_gain(int ch,double gain){stream.gain[ch]=gain;}
 void transmit_register_setup(int){} // ST-V has no connected MIDI serial endpoint.
 Core(){memset(this,0,sizeof(*this));rng=1;}
 void start(u8 *data){ram.data=data;init();}
 void sample(){
  for(int i=0;i<3;i++)if(timer_samples[i]>0 && --timer_samples[i]==0){if(i==0)timerA_cb();else if(i==1)timerB_cb();else timerC_cb();}
  sound_stream_update(stream);
 }
 void sound_stream_update(sound_stream &stream);
 u16 read(offs_t offset);
 void write(offs_t offset,u16 data,u16 mem_mask=0xffff);
	enum SCSP_STATE { SCSP_ATTACK, SCSP_DECAY1, SCSP_DECAY2, SCSP_RELEASE };

	struct SCSP_EG_t
	{
		int volume; //
		SCSP_STATE state;
		int step;
		//step vals
		int AR;     //Attack
		int D1R;    //Decay1
		int D2R;    //Decay2
		int RR;     //Release

		int DL;     //Decay level
		u8 EGHOLD;
		u8 LPLINK;
	};

	struct SCSP_LFO_t
	{
		u16 phase;
		u32 phase_step;
		int *table;
		int *scale;
	};

	struct SCSP_SLOT
	{
		union
		{
			u16 data[0x10];  //only 0x1a bytes used
			u8 datab[0x20];
		} udata;

		u8 Backwards;    //the wave is playing backwards
		u8 active;   //this slot is currently playing
		u32 cur_addr;    //current play address (24.8)
		u32 nxt_addr;    //next play address
		u32 step;        //pitch step (24.8)
		SCSP_EG_t EG;            //Envelope
		SCSP_LFO_t PLFO;     //Phase LFO
		SCSP_LFO_t ALFO;     //Amplitude LFO
		int slot;
		s16 Prev;  //Previous sample (for interpolation)
	};

	Irq m_irq_cb;
	MainIrq m_main_irq_cb;

	union
	{
		u16 data[0x30/2];
		u8 datab[0x30];
	} m_udata;

	SCSP_SLOT m_Slots[32];
	s16 m_RINGBUF[128];
	u8 m_BUFPTR;
#if SCSP_FM_DELAY
	s16 m_DELAYBUF[SCSP_FM_DELAY];
	u8 m_DELAYPTR;
#endif
	sound_stream stream;

	u32 m_IrqTimA;
	u32 m_IrqTimBC;
	u32 m_IrqMidi;

	u8 m_latched_MSLC;
	u16 m_latched_MSLC_data;
	u8 m_MidiOutStack[32];
	u8 m_MidiOutW, m_MidiOutR;
	u8 m_MidiStack[32];
	u8 m_MidiW, m_MidiR;

	s32 m_EG_TABLE[0x400];

	int m_LPANTABLE[0x10000];
	int m_RPANTABLE[0x10000];

	int m_TimPris[3];
	int m_TimCnt[3];

	// timers

	// DMA stuff
	struct
	{
		u32 dmea;
		u16 drga;
		u16 dtlg;
		u8 dgate;
		u8 ddir;
	} m_dma;

	u16 m_mcieb;
	u16 m_mcipd;

	int m_ARTABLE[64], m_DRTABLE[64];

	SCSPDSP m_DSP;

	s16 *m_RBUFDST;   //this points to where the sample will be stored in the RingBuf

	//LFO
	int m_PLFO_TRI[256], m_PLFO_SQR[256], m_PLFO_SAW[256], m_PLFO_NOI[256];
	int m_ALFO_TRI[256], m_ALFO_SQR[256], m_ALFO_SAW[256], m_ALFO_NOI[256];
	int m_PSCALES[8][256];
	int m_ASCALES[8][256];

	void exec_dma();       /*state DMA transfer function*/
	u8 DecodeSCI(u8 irq);
	void UpdateSlotMonitor();
	void CheckPendingIRQ();
	void MainCheckPendingIRQ(u16 irq_type);
	void ResetInterrupts();
	void timerA_cb();
	void timerB_cb();
	void timerC_cb();
	int Get_AR(int base, int R);
	int Get_DR(int base, int R);
	void Compute_EG(SCSP_SLOT *slot);
	int EG_Update(SCSP_SLOT *slot);
	u32 Step(SCSP_SLOT *slot);
	void Compute_LFO(SCSP_SLOT *slot);
	void StartSlot(SCSP_SLOT *slot);
	void StopSlot(SCSP_SLOT *slot, int keyoff);
	void init();
	void UpdateSlotReg(int s, int r);
	void UpdateReg(int reg);
	void UpdateSlotRegR(int slot, int reg);
	void UpdateRegR(int reg);
	void w16(u32 addr, u16 val);
	u16 r16(u32 addr);
	inline s32 UpdateSlot(SCSP_SLOT *slot);
	void DoMasterSamples(sound_stream &stream);

	//LFO
	void LFO_Init();
	s32 PLFO_Step(SCSP_LFO_t *LFO);
	s32 ALFO_Step(SCSP_LFO_t *LFO);
	void LFO_ComputeStep(SCSP_LFO_t *LFO, u32 LFOF, u32 LFOWS, u32 LFOS, int ALFO);
};
void Core::sound_stream_update(sound_stream &stream)
{
	DoMasterSamples(stream);
	UpdateSlotMonitor();
	// TODO: 1 sample (1Fs) 44.1 kHz irq here.
}

void Core::UpdateSlotMonitor()
{
	// MSLC     |  CA   |SGC|EG
	// f e d c b a 9 8 7 6 5 4 3 2 1 0

	// latch the new MSLC, updates every 44.1 kHz
	// cfr. vstriker (GK reflecting ball with heavy shots) and srallyc (PowerGames BGM bleeps at end)
	u8 MSLC = m_latched_MSLC;
	SCSP_SLOT *slot = m_Slots + MSLC;
	u32 SGC = (slot->EG.state) & 3;
	u32 CA = (slot->cur_addr >> (SHIFT + 12)) & 0xf;
	u32 EG = (0x1f - (slot->EG.volume >> (EG_SHIFT + 5))) & 0x1f;
	// NOTE: according to the manual MSLC is write only, CA, SGC and EG read only.
	// saturn:toughtrk will hang on Human logo otherwise
	m_latched_MSLC_data =  /*(MSLC << 11) |*/ (CA << 7) | (SGC << 5) | EG;
}

u8 Core::DecodeSCI(u8 irq)
{
	u8 SCI = 0;
	u8 v;
	v = (SCILV0() & (1 << irq)) ? 1 : 0;
	SCI |= v;
	v = (SCILV1() & (1 << irq)) ? 1 : 0;
	SCI |= v << 1;
	v = (SCILV2() & (1 << irq)) ? 1 : 0;
	SCI |= v << 2;
	return SCI;
}

void Core::CheckPendingIRQ()
{
	u32 pend = m_udata.data[0x20/2];
	u32 en = m_udata.data[0x1e/2];
	if (m_MidiW != m_MidiR)
	{
		m_udata.data[0x20/2] |= 8;
		pend |= 8;
	}
	if (!pend)
		return;
	if (pend & 0x40)
		if (en & 0x40)
		{
			m_irq_cb(m_IrqTimA, ASSERT_LINE);
			return;
		}
	if (pend & 0x80)
		if (en & 0x80)
		{
			m_irq_cb(m_IrqTimBC, ASSERT_LINE);
			return;
		}
	if (pend & 0x100)
		if (en & 0x100)
		{
			m_irq_cb(m_IrqTimBC, ASSERT_LINE);
			return;
		}
	if (pend & 8)
		if (en & 8)
		{
			m_irq_cb(m_IrqMidi, ASSERT_LINE);
			return;
		}

	m_irq_cb((offs_t)0, CLEAR_LINE);
}

void Core::MainCheckPendingIRQ(u16 irq_type)
{
	m_mcipd |= irq_type;

	//machine().scheduler().synchronize(); // force resync

	if (m_mcipd & m_mcieb)
		m_main_irq_cb(1);
	else
		m_main_irq_cb(0);
}

void Core::ResetInterrupts()
{
	u32 reset = m_udata.data[0x22/2];

	if (reset & 0x40)
	{
		m_irq_cb(m_IrqTimA, CLEAR_LINE);
	}
	if (reset & 0x180)
	{
		m_irq_cb(m_IrqTimBC, CLEAR_LINE);
	}
	if (reset & 0x8)
	{
		m_irq_cb(m_IrqMidi, CLEAR_LINE);
	}

	CheckPendingIRQ();
}

void Core::timerA_cb()
{
	m_TimCnt[0] = 0xFFFF;
	m_udata.data[0x20/2] |= 0x40;
	m_udata.data[0x18/2] &= 0xff00;
	m_udata.data[0x18/2] |= m_TimCnt[0] >> 8;

	CheckPendingIRQ();
	MainCheckPendingIRQ(0x40);
}

void Core::timerB_cb()
{
	m_TimCnt[1] = 0xFFFF;
	m_udata.data[0x20/2] |= 0x80;
	m_udata.data[0x1a/2] &= 0xff00;
	m_udata.data[0x1a/2] |= m_TimCnt[1] >> 8;

	CheckPendingIRQ();
}

void Core::timerC_cb()
{
	m_TimCnt[2] = 0xFFFF;
	m_udata.data[0x20/2] |= 0x100;
	m_udata.data[0x1c/2] &= 0xff00;
	m_udata.data[0x1c/2] |= m_TimCnt[2] >> 8;

	CheckPendingIRQ();
}

int Core::Get_AR(int base, int R)
{
	int Rate = base + (R << 1);
	return m_ARTABLE[clamp(Rate, 0, 63)];
}

int Core::Get_DR(int base, int R)
{
	int Rate = base + (R << 1);
	return m_DRTABLE[clamp(Rate, 0, 63)];
}

void Core::Compute_EG(SCSP_SLOT *slot)
{
	int octave = (OCT(slot) ^ 8) - 8;
	int rate;
	if (KRS(slot) != 0xf)
		rate = octave + 2 * KRS(slot) + ((FNS(slot) >> 9) & 1);
	else
		rate = 0; //rate = ((FNS(slot) >> 9) & 1);

	slot->EG.volume = 0x17F<<EG_SHIFT;
	slot->EG.AR = Get_AR(rate,AR(slot));
	slot->EG.D1R = Get_DR(rate,D1R(slot));
	slot->EG.D2R = Get_DR(rate,D2R(slot));
	slot->EG.RR = Get_DR(rate,RR(slot));
	slot->EG.DL = 0x1f - DL(slot);
	slot->EG.EGHOLD = EGHOLD(slot);
}

int Core::EG_Update(SCSP_SLOT *slot)
{
	switch (slot->EG.state)
	{
		case SCSP_ATTACK:
			slot->EG.volume += slot->EG.AR;
			if (slot->EG.volume >= (0x3ff<<EG_SHIFT))
			{
				if (!LPSLNK(slot))
				{
					slot->EG.state = SCSP_DECAY1;
					if (slot->EG.D1R >= (1024 << EG_SHIFT)) //Skip SCSP_DECAY1, go directly to SCSP_DECAY2
						slot->EG.state = SCSP_DECAY2;
				}
				slot->EG.volume=0x3ff << EG_SHIFT;
			}
			if (slot->EG.EGHOLD)
				return 0x3ff << (SHIFT - 10);
			break;
		case SCSP_DECAY1:
			slot->EG.volume -= slot->EG.D1R;
			if (slot->EG.volume <= 0)
				slot->EG.volume = 0;
			if (slot->EG.volume >> (EG_SHIFT + 5) <= slot->EG.DL)
				slot->EG.state = SCSP_DECAY2;
			break;
		case SCSP_DECAY2:
			if (D2R(slot) == 0)
				return (slot->EG.volume >> EG_SHIFT) << (SHIFT - 10);
			slot->EG.volume -= slot->EG.D2R;
			if (slot->EG.volume <= 0)
				slot->EG.volume = 0;

			break;
		case SCSP_RELEASE:
			slot->EG.volume -= slot->EG.RR;
			if (slot->EG.volume <= 0)
			{
				slot->EG.volume = 0;
				StopSlot(slot, 0);
				//slot->EG.volume = 0x17F << EG_SHIFT;
				//slot->EG.state = SCSP_ATTACK;
			}
			break;
		default:
			return 1 << SHIFT;
	}
	return (slot->EG.volume >> EG_SHIFT) << (SHIFT - 10);
}

u32 Core::Step(SCSP_SLOT *slot)
{
	int octave = (OCT(slot) ^ 8) - 8 + SHIFT - 10;
	u32 Fn = FNS(slot) + (1 << 10);
	if (octave >= 0)
	{
		Fn <<= octave;
	}
	else
	{
		Fn >>= -octave;
	}

	return Fn;
}


void Core::Compute_LFO(SCSP_SLOT *slot)
{
	if (PLFOS(slot) != 0)
		LFO_ComputeStep(&(slot->PLFO), LFOF(slot), PLFOWS(slot), PLFOS(slot), 0);
	if (ALFOS(slot) != 0)
		LFO_ComputeStep(&(slot->ALFO), LFOF(slot), ALFOWS(slot), ALFOS(slot), 1);
}

void Core::StartSlot(SCSP_SLOT *slot)
{
	slot->active = 1;
	slot->cur_addr = 0;
	slot->nxt_addr = 1 << SHIFT;
	slot->step = Step(slot);
	Compute_EG(slot);
	slot->EG.state = SCSP_ATTACK;
	slot->EG.volume = 0x17F << EG_SHIFT;
	slot->Prev = 0;
	slot->Backwards = 0;

	Compute_LFO(slot);

//  printf("StartSlot[%p]: SA %x PCM8B %x LPCTL %x ALFOS %x STWINH %x TL %x EFSDL %x\n", slot, SA(slot), PCM8B(slot), LPCTL(slot), ALFOS(slot), STWINH(slot), TL(slot), EFSDL(slot));
}

void Core::StopSlot(SCSP_SLOT *slot,int keyoff)
{
	if (keyoff /*&& slot->EG.state!=SCSP_RELEASE*/)
	{
		slot->EG.state = SCSP_RELEASE;
	}
	else
	{
		slot->active = 0;
	}
	slot->udata.data[0] &= ~0x800;
}

void Core::init()
{
	int i;

	m_DSP.Init();

	m_IrqTimA = m_IrqTimBC = m_IrqMidi = 0;
	m_MidiR=m_MidiW = 0;
	m_MidiOutR = m_MidiOutW = 0;

	m_DSP.space = &ram;

	for (i = 0; i < 0x400; ++i)
	{
		float envDB = ((float)(3 * (i - 0x3ff))) / 32.0f;
		float scale = (float)(1 << SHIFT);
		m_EG_TABLE[i] = (s32)(powf(10.0f, envDB / 20.0f) * scale);
	}

	for (i = 0; i < 0x10000; ++i)
	{
		int iTL  = (i >> 0x0) & 0xff;
		int iPAN = (i >> 0x8) & 0x1f;
		int iSDL = (i >> 0xD) & 0x07;
		float TL;
		float SegaDB = 0.0f;
		float fSDL;
		float PAN;
		float LPAN,RPAN;

		if (iTL & 0x01) SegaDB -= 0.4f;
		if (iTL & 0x02) SegaDB -= 0.8f;
		if (iTL & 0x04) SegaDB -= 1.5f;
		if (iTL & 0x08) SegaDB -= 3.0f;
		if (iTL & 0x10) SegaDB -= 6.0f;
		if (iTL & 0x20) SegaDB -= 12.0f;
		if (iTL & 0x40) SegaDB -= 24.0f;
		if (iTL & 0x80) SegaDB -= 48.0f;

		TL=powf(10.0f, SegaDB / 20.0f);

		SegaDB=0;
		if (iPAN & 0x1) SegaDB -= 3.0f;
		if (iPAN & 0x2) SegaDB -= 6.0f;
		if (iPAN & 0x4) SegaDB -= 12.0f;
		if (iPAN & 0x8) SegaDB -= 24.0f;

		if ((iPAN & 0xf) == 0xf) PAN = 0.0;
		else PAN=powf(10.0f, SegaDB / 20.0f);

		if (iPAN < 0x10)
		{
			LPAN = PAN;
			RPAN = 1.0;
		}
		else
		{
			RPAN = PAN;
			LPAN = 1.0;
		}

		if (iSDL)
			fSDL = powf(10.0f, (SDLT[iSDL]) / 20.0f);
		else
			fSDL = 0.0;

		m_LPANTABLE[i] = FIX((4.0f * LPAN * TL * fSDL));
		m_RPANTABLE[i] = FIX((4.0f * RPAN * TL * fSDL));
	}

	m_ARTABLE[0] = m_DRTABLE[0] = 0;    //Infinite time
	m_ARTABLE[1] = m_DRTABLE[1] = 0;    //Infinite time
	for (i = 2; i < 64; ++i)
	{
		double step, scale;
		double t = ARTimes[i];   //In ms
		if (t != 0.0)
		{
			step = (1023 * 1000.0) / (44100.0 * t);
			scale = (double) (1 << EG_SHIFT);
			m_ARTABLE[i] = (int) (step * scale);
		}
		else
			m_ARTABLE[i] = 1024 << EG_SHIFT;

		t = DRTimes[i];   //In ms
		step = (1023 * 1000.0) / (44100.0 * t);
		scale = (double) (1 << EG_SHIFT);
		m_DRTABLE[i] = (int) (step * scale);
	}

	// make sure all the slots are off
	for (i = 0; i < 32; ++i)
	{
		m_Slots[i].slot = i;
		m_Slots[i].active = 0;
		m_Slots[i].EG.state = SCSP_RELEASE;
	}

	LFO_Init();
	// no "pend"
	m_udata.data[0x20/2] = 0;
	m_TimCnt[0] = 0xffff;
	m_TimCnt[1] = 0xffff;
	m_TimCnt[2] = 0xffff;
}

void Core::UpdateSlotReg(int s,int r)
{
	SCSP_SLOT *slot = m_Slots + s;
	switch (r & 0x3f)
	{
		case 0:
		case 1:
			if (KEYONEX(slot))
			{
				for (int sl=0; sl < 32; ++sl)
				{
					SCSP_SLOT *s2 = m_Slots + sl;
					{
						if (KEYONB(s2) && s2->EG.state == SCSP_RELEASE/*&& !s2->active*/)
						{
							StartSlot(s2);
						}
						if (!KEYONB(s2) /*&& s2->active*/)
						{
							StopSlot(s2, 1);
						}
					}
				}
				slot->udata.data[0] &= ~0x1000;
			}
			break;
		case 0x10:
		case 0x11:
			slot->step = Step(slot);
			break;
		case 0xA:
		case 0xB:
			slot->EG.RR = Get_DR(0, RR(slot));
			slot->EG.DL = 0x1f - DL(slot);
			break;
		case 0x12:
		case 0x13:
			Compute_LFO(slot);
			break;
	}
}

void Core::UpdateReg(int reg)
{
	switch (reg & 0x3f)
	{
		case 0x0:
			set_output_gain(0, MVOL() / 15.0);
			set_output_gain(1, MVOL() / 15.0);
			break;
		case 0x2:
		case 0x3:
			{
				m_DSP.RBL = (8 * 1024) << RBL(); // 8 / 16 / 32 / 64 kwords
				m_DSP.RBP = RBP();
			}
			break;
		case 0x6:
		case 0x7:
			{
				u8 data = m_udata.data[0x6 / 2] & 0xff;
				if (m_MidiOutR == m_MidiOutW)
				{
					// not busy, so start transmission
					transmit_register_setup(data);
				}
				m_MidiOutStack[m_MidiOutW++] = data;
				m_MidiOutW &= 31;
			}
			break;
		case 8:
		case 9:
			/* Only MSLC could be written.  */
			// docs claims MSLC to be 0x7800 but saturn:jikkparo doesn't agree,
			// assume doc mistake out of being 0~31 slots
			m_latched_MSLC = (m_udata.data[0x8/2] & 0xf800) >> 11;
			// The hardware updates the monitor in its slot pipeline. Our mixer
			// advances all slots together, so refresh the selected slot here
			// to avoid returning another slot's position before the next sample.
			UpdateSlotMonitor();
			break;
		case 0x12:
		case 0x13:
			m_dma.dmea = (m_udata.data[0x12/2] & 0xfffe) | (m_dma.dmea & 0xf0000);
			break;
		case 0x14:
		case 0x15:
			m_dma.dmea = ((m_udata.data[0x14/2] & 0xf000) << 4) | (m_dma.dmea & 0xfffe);
			m_dma.drga = (m_udata.data[0x14/2] & 0x0ffe);
			break;
		case 0x16:
		case 0x17:
			m_dma.dtlg = (m_udata.data[0x16/2] & 0x0ffe);
			m_dma.ddir = (m_udata.data[0x16/2] & 0x2000) >> 13;
			m_dma.dgate = (m_udata.data[0x16/2] & 0x4000) >> 14;
			if (m_udata.data[0x16/2] & 0x1000) // dexe
				exec_dma();
			break;
		case 0x18:
		case 0x19:
			if (!m_irq_cb.isunset())
			{
				m_TimPris[0] = 1 << ((m_udata.data[0x18/2] >> 8) & 0x7);
				m_TimCnt[0] = (m_udata.data[0x18/2] & 0xff) << 8;

				if ((m_udata.data[0x18/2] & 0xff) != 255)
				{
					u32 time = (clock() / m_TimPris[0]) / (255 - (m_udata.data[0x18/2] & 0xff));
					if (time)
					{
						timer_samples[0] = m_TimPris[0] * (255 - (m_udata.data[(0x18 + 0 * 2) / 2] & 255));
					}
				}
			}
			break;
		case 0x1a:
		case 0x1b:
			if (!m_irq_cb.isunset())
			{
				m_TimPris[1] = 1 << ((m_udata.data[0x1A/2] >> 8) & 0x7);
				m_TimCnt[1] = (m_udata.data[0x1A/2] & 0xff) << 8;

				if ((m_udata.data[0x1A/2] & 0xff) != 255)
				{
					u32 time = (clock() / m_TimPris[1]) / (255 - (m_udata.data[0x1A/2] & 0xff));
					if (time)
					{
						timer_samples[1] = m_TimPris[1] * (255 - (m_udata.data[(0x18 + 1 * 2) / 2] & 255));
					}
				}
			}
			break;
		case 0x1c:
		case 0x1d:
			if (!m_irq_cb.isunset())
			{
				m_TimPris[2] = 1 << ((m_udata.data[0x1C/2] >> 8) & 0x7);
				m_TimCnt[2] = (m_udata.data[0x1C/2] & 0xff) << 8;

				if ((m_udata.data[0x1C/2] & 0xff) != 255)
				{
					u32 time = (clock() / m_TimPris[2]) / (255 - (m_udata.data[0x1C/2] & 0xff));
					if (time)
					{
						timer_samples[2] = m_TimPris[2] * (255 - (m_udata.data[(0x18 + 2 * 2) / 2] & 255));
					}
				}
			}
			break;
		case 0x1e: // SCIEB
		case 0x1f:
			if (!m_irq_cb.isunset())
			{
				CheckPendingIRQ();

				if (m_udata.data[0x1e/2] & 0x610)
					popmessage("SCSP SCIEB enabled %04x",m_udata.data[0x1e/2]);
			}
			break;
		case 0x20: // SCIPD
		case 0x21:
			if (!m_irq_cb.isunset())
			{
				if (m_udata.data[0x1e/2] & m_udata.data[0x20/2] & 0x20)
					popmessage("SCSP SCIPD write %04x", m_udata.data[0x20/2]);
			}
			break;
		case 0x22:  //SCIRE
		case 0x23:
			if (!m_irq_cb.isunset())
			{
				m_udata.data[0x20/2] &= ~m_udata.data[0x22/2];
				ResetInterrupts();

				// behavior from real hardware: if you SCIRE a timer that's expired,
				// it'll immediately pop up again in SCIPD.  cfr. saturn:sakurat
				if (m_TimCnt[0] == 0xffff)
				{
					m_udata.data[0x20/2] |= 0x40;
				}
				if (m_TimCnt[1] == 0xffff)
				{
					m_udata.data[0x20/2] |= 0x80;
				}
				if (m_TimCnt[2] == 0xffff)
				{
					m_udata.data[0x20/2] |= 0x100;
				}
			}
			break;
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x28:
		case 0x29:
			if (!m_irq_cb.isunset())
			{
				m_IrqTimA = DecodeSCI(SCITMA);
				m_IrqTimBC = DecodeSCI(SCITMB);
				m_IrqMidi = DecodeSCI(SCIMID);
			}
			break;
		case 0x2a:
		case 0x2b:
			m_mcieb = m_udata.data[0x2a/2];

			MainCheckPendingIRQ(0);
			if (m_mcieb & ~0x60)
				popmessage("SCSP MCIEB enabled %04x",m_mcieb);
			break;
		case 0x2c:
		case 0x2d:
			if (m_udata.data[0x2c/2] & 0x20)
				MainCheckPendingIRQ(0x20);
			break;
		case 0x2e:
		case 0x2f:
			m_mcipd &= ~m_udata.data[0x2e/2];
			MainCheckPendingIRQ(0);
			break;

	}
}

void Core::UpdateSlotRegR(int slot,int reg)
{
}

void Core::UpdateRegR(int reg)
{
	switch (reg & 0x3f)
	{
		case 4:
		case 5:
			{
				u16 v = m_udata.data[0x4/2];
				v &= 0xff00;
				v |= m_MidiStack[m_MidiR];
				DeviceLog("Read %x from SCSP MIDI\n", v);
				if (m_MidiR != m_MidiW)
				{
					++m_MidiR;
					m_MidiR &= 31;
				}
				if (m_MidiR == m_MidiW)     // if the input FIFO is empty, clear the IRQ
				{
					m_irq_cb(m_IrqMidi, CLEAR_LINE);
					m_udata.data[0x20 / 2] &= ~8;
				}
				m_udata.data[0x4/2] = v;
			}
			break;
		case 8:
		case 9:
			{
				m_udata.data[0x8/2] = m_latched_MSLC_data;
			}
			break;

		case 0x18:
		case 0x19:
			break;

		case 0x1a:
		case 0x1b:
			break;

		case 0x1c:
		case 0x1d:
			break;

		//case 0x20:
		//  m_udata.data[0x20/2] ^= 0x400;
		//  break;

		case 0x2a:
		case 0x2b:
			m_udata.data[0x2a/2] = m_mcieb;
			break;

		case 0x2c:
		case 0x2d:
			m_udata.data[0x2c/2] = m_mcipd;
			break;
	}
}

void Core::w16(u32 addr, u16 val)
{
	addr &= 0xffff;
	if (addr < 0x400)
	{
		int slot = addr / 0x20;
		addr &= 0x1f;
		*((u16 *) (m_Slots[slot].udata.datab + (addr))) = val;
		UpdateSlotReg(slot, addr & 0x1f);
	}
	else if (addr < 0x600)
	{
		if (addr < 0x430)
		{
			*((u16 *) (m_udata.datab + ((addr & 0x3f)))) = val;
			UpdateReg(addr & 0x3f);
		}
	}
	else if (addr < 0x700)
		m_RINGBUF[(addr - 0x600)/2] = val;
	else
	{
		//DSP
		if (addr < 0x780)  //COEF
			*((u16 *) (m_DSP.COEF + (addr - 0x700) / 2)) = val;
		else if (addr < 0x7c0)
			*((u16 *) (m_DSP.MADRS + (addr - 0x780) / 2)) = val;
		else if (addr < 0x800) // MADRS is mirrored twice
			*((u16 *) (m_DSP.MADRS + (addr - 0x7c0) / 2)) = val;
		else if (addr < 0xC00)
		{
			*((uint16_t *) (m_DSP.MPRO + (addr - 0x800) / 2)) = val;

			if (addr == 0xBF0)
			{
				m_DSP.Start();
			}
		}
	}
}

u16 Core::r16(u32 addr)
{
	u16 v = 0;
	addr &= 0xffff;
	if (addr < 0x400)
	{
		int slot = addr / 0x20;
		addr &= 0x1f;
		UpdateSlotRegR(slot, addr & 0x1f);
		v = *((u16 *) (m_Slots[slot].udata.datab + (addr)));
	}
	else if (addr < 0x600)
	{
		if (addr < 0x430)
		{
			UpdateRegR(addr & 0x3f);
			v = *((u16 *) (m_udata.datab + ((addr & 0x3f))));
		}
	}
	else if (addr < 0x700)
		v = m_RINGBUF[(addr-0x600)/2];
	else
	{
		//DSP
		if (addr < 0x780)  //COEF
			v= *((u16 *) (m_DSP.COEF + (addr - 0x700) / 2));
		else if (addr < 0x7c0)
			v= *((u16 *) (m_DSP.MADRS + (addr - 0x780) / 2));
		else if (addr < 0x800)
			v= *((u16 *) (m_DSP.MADRS + (addr - 0x7c0) / 2));
		else if (addr < 0xC00)
			v= *((u16 *) (m_DSP.MPRO + (addr - 0x800) / 2));
		else if (addr < 0xE00)
		{
			if (addr & 2)
				v = m_DSP.TEMP[(addr >> 2) & 0x7f] & 0xffff;
			else
				v = m_DSP.TEMP[(addr >> 2) & 0x7f] >> 16;
		}
		else if (addr < 0xE80)
		{
			if (addr & 2)
				v = m_DSP.MEMS[(addr >> 2) & 0x1f] & 0xffff;
			else
				v = m_DSP.MEMS[(addr >> 2) & 0x1f] >> 16;
		}
		else if (addr < 0xEC0)
		{
			if (addr & 2)
				v = m_DSP.MIXS[(addr >> 2) & 0xf] & 0xffff;
			else
				v = m_DSP.MIXS[(addr >> 2) & 0xf] >> 16;
		}
		else if (addr < 0xEE0)
			v = *((u16 *) (m_DSP.EFREG + (addr - 0xec0) / 2));
		else
		{
			// TODO: kyutnkai reads from 0xee0/0xee2
			// it's tied with EXTS register(s) also used for CD-Rom Player equalizer.
			/*
			This port is actually an external parallel port, directly connected from the CD Block device, hence code is a bit of an hack.

			Code snippet for reference:
			004A3A: 207C 0010 0EE0             movea.l #$100ee0, A0
			004A40: 43EA 0090                  lea     ($90,A2), A1 ;A2=0x700
			004A44: 6100 0254                  bsr     $4c9a
			004A48: 207C 0010 0EE2             movea.l #$100ee2, A0
			004A4E: 43EA 0092                  lea     ($92,A2), A1
			004A52: 6100 0246                  bsr     $4c9a
			004A56: 207C 0010 0ED2             movea.l #$100ed2, A0
			004A5C: 43EA 0094                  lea     ($94,A2), A1
			004A60: 6100 0238                  bsr     $4c9a
			004A64: 3540 0096                  move.w  D0, ($96,A2)
			004A68: 207C 0010 0ED4             movea.l #$100ed4, A0
			004A6E: 43EA 0098                  lea     ($98,A2), A1
			004A72: 6100 0226                  bsr     $4c9a
			004A76: 3540 009A                  move.w  D0, ($9a,A2)
			004A7A: 207C 0010 0ED6             movea.l #$100ed6, A0
			004A80: 43EA 009C                  lea     ($9c,A2), A1
			004A84: 6100 0214                  bsr     $4c9a
			004A88: 3540 009E                  move.w  D0, ($9e,A2)
			004A8C: 4E75                       rts

			    004C9A: 48E7 4000                  movem.l D1, -(A7)
			    004C9E: 3010                       move.w  (A0), D0 ;reads from 0x100ee0/ee2
			    004CA0: 4A40                       tst.w   D0
			    004CA2: 6A00 0004                  bpl     $4ca8
			    004CA6: 4440                       neg.w   D0
			    004CA8: 3211                       move.w  (A1), D1
			    004CAA: D041                       add.w   D1, D0
			    004CAC: E248                       lsr.w   #1, D0
			    004CAE: 3280                       move.w  D0, (A1) ;writes to RAM buffer 0x790/0x792
			    004CB0: 4CDF 0002                  movem.l (A7)+, D1
			    004CB4: 4E75                       rts
			*/
			DeviceLog("%s: SCSP Reading from EXTS register %08x\n", "ST-V SCSP", addr);
			if (addr < 0xEE4)
				v = *((u16 *) (m_DSP.EXTS + (addr - 0xee0) / 2));
		}
	}
	return v;
}


inline s32 Core::UpdateSlot(SCSP_SLOT *slot)
{
	// MiSTer SCSP holds the LFO phase at zero while LFORE is set.
	if (slot->udata.data[0x9] & 0x8000) {
		slot->PLFO.phase = -slot->PLFO.phase_step;
		slot->ALFO.phase = -slot->ALFO.phase_step;
	}

	if (SSCTL(slot) == 3) // manual says cannot be used
	{
		DeviceLog("SCSP: Invaild SSCTL setting at slot %02x\n", slot->slot);
		return 0;
	}

	s32 sample = 0; // NB: Shouldn't be necessary, but GCC 8.2.1 claims otherwise.
	int step = slot->step;
	u32 addr1, addr2, addr_select;                                   // current and next sample addresses
	u32 *addr[2]      = {&addr1, &addr2};                          // used for linear interpolation
	u32 *slot_addr[2] = {&(slot->cur_addr), &(slot->nxt_addr)};    //

	if (PLFOS(slot) != 0)
	{
		// OCT=7 can overflow a signed 32-bit product before the fixed-point shift.
		step = (s64(step) * PLFO_Step(&(slot->PLFO))) >> SHIFT;
	}

	if (PCM8B(slot))
	{
		addr1 = slot->cur_addr >> SHIFT;
		addr2 = slot->nxt_addr >> SHIFT;
	}
	else
	{
		addr1 = (slot->cur_addr >> (SHIFT - 1)) & ~1;
		addr2 = (slot->nxt_addr >> (SHIFT - 1)) & ~1;
	}

	// MiSTer SCSP MDCalc uses the unscaled signed stack samples and returns
	// a signed modulation displacement with six fractional bits. CPU-written
	// stack values (used by Decathlete) must follow the same path as slot output.
	s32 interpolation = slot->cur_addr & ((1 << SHIFT) - 1);
	if (MDL(slot) > 4)
	{
		s32 mod = Modulation(m_RINGBUF[(m_BUFPTR + MDXSL(slot)) & 63],
			m_RINGBUF[(m_BUFPTR + MDYSL(slot)) & 63], MDL(slot));
		mod += interpolation >> (SHIFT - 6);
		s32 displacement = mod >> 6;
		if (!PCM8B(slot)) displacement *= 2;
		addr1 += displacement;
		addr2 += displacement;
		interpolation = (mod & 63) << (SHIFT - 6);
	}

	if (SSCTL(slot) == 0) // External DRAM data
	{
		if (PCM8B(slot)) //8 bit signed
		{
			int8_t p1 = read_byte(SA(slot) + addr1);
			int8_t p2 = read_byte(SA(slot) + addr2);
			s32 s;
			s32 fpart = interpolation;
			s = (int) (p1 << 8) * ((1 << SHIFT) - fpart) + (int) (p2 << 8) * fpart;
			sample = (s >> SHIFT);
		}
		else    //16 bit signed (endianness?)
		{
			s16 p1 = read_word(SA(slot) + addr1);
			s16 p2 = read_word(SA(slot) + addr2);
			s32 s;
			s32 fpart = interpolation;
			s = (int)(p1) * ((1 << SHIFT) - fpart) + (int)(p2) * fpart;
			sample = (s >> SHIFT);
		}
	}
	else if (SSCTL(slot) == 1)  // Internally generated data (Noise)
		sample = (s16)(noise() & 0xffff); // Unknown algorithm
	else if (SSCTL(slot) >= 2)  // Internally generated data (All 0)
		sample = 0;

	if (SBCTL(slot) & 0x1)
		sample ^= 0x7FFF;
	if (SBCTL(slot) & 0x2)
		sample = (s16)(sample ^ 0x8000);

	if (slot->Backwards)
		slot->cur_addr -= step;
	else
		slot->cur_addr += step;
	slot->nxt_addr = slot->cur_addr + (1 << SHIFT);

	addr1 = slot->cur_addr >> SHIFT;
	addr2 = slot->nxt_addr >> SHIFT;

	if (addr1 >= LSA(slot) && !(slot->Backwards))
	{
		if (LPSLNK(slot) && slot->EG.state == SCSP_ATTACK)
			slot->EG.state = SCSP_DECAY1;
	}

	for (addr_select = 0; addr_select < 2; addr_select++)
	{
		s32 rem_addr;
		switch (LPCTL(slot))
		{
		case 0: //no loop
			if (*addr[addr_select] >= LSA(slot) && *addr[addr_select] >= LEA(slot))
			{
				//slot->active=0;
				StopSlot(slot, 0);
			}
			break;
		case 1: //normal loop
			if (*addr[addr_select] >= LEA(slot))
			{
				rem_addr = *slot_addr[addr_select] - (LEA(slot) << SHIFT);
				*slot_addr[addr_select] = (LSA(slot) << SHIFT) + rem_addr;
			}
			break;
		case 2: //reverse loop
			if ((*addr[addr_select] >= LSA(slot)) && !(slot->Backwards))
			{
				rem_addr = *slot_addr[addr_select] - (LSA(slot) << SHIFT);
				*slot_addr[addr_select] = (LEA(slot) << SHIFT) - rem_addr;
				slot->Backwards = 1;
			}
			else if ((*addr[addr_select] < LSA(slot) || (*slot_addr[addr_select] & 0x80000000)) && slot->Backwards)
			{
				rem_addr = (LSA(slot) << SHIFT) - *slot_addr[addr_select];
				*slot_addr[addr_select] = (LEA(slot) << SHIFT) - rem_addr;
			}
			break;
		case 3: //ping-pong
			if (*addr[addr_select] >= LEA(slot)) //reached end, reverse till start
			{
				rem_addr = *slot_addr[addr_select] - (LEA(slot) << SHIFT);
				*slot_addr[addr_select] = (LEA(slot) << SHIFT) - rem_addr;
				slot->Backwards = 1;
			}
			else if ((*addr[addr_select] < LSA(slot) || (*slot_addr[addr_select] & 0x80000000)) && slot->Backwards)//reached start or negative
			{
				rem_addr = (LSA(slot) << SHIFT) - *slot_addr[addr_select];
				*slot_addr[addr_select] = (LSA(slot) << SHIFT) + rem_addr;
				slot->Backwards = 0;
			}
			break;
		}
	}

	if (!SDIR(slot))
	{
		if (ALFOS(slot) != 0)
		{
			sample = sample * ALFO_Step(&(slot->ALFO));
			sample >>= SHIFT;
		}

		if (slot->EG.state == SCSP_ATTACK)
			sample = (sample * EG_Update(slot)) >> SHIFT;
		else
			sample = (sample * m_EG_TABLE[EG_Update(slot) >> (SHIFT - 10)]) >> SHIFT;
	}

	if (!STWINH(slot))
	{
		if (!SDIR(slot))
		{
			u16 Enc = ((TL(slot)) << 0x0) | (0x7 << 0xd);
			*m_RBUFDST = (sample * m_LPANTABLE[Enc]) >> (SHIFT + 2);
		}
		else
		{
			u16 Enc = (0 << 0x0) | (0x7 << 0xd);
			*m_RBUFDST = (sample * m_LPANTABLE[Enc]) >> (SHIFT + 2);
		}
	}

	return sample;
}

void Core::DoMasterSamples(sound_stream &stream)
{
	for (int s = 0; s < stream.samples(); ++s)
	{
		s32 smpl = 0, smpr = 0;

		for (int sl = 0; sl < 32; ++sl)
		{
#if SCSP_FM_DELAY
			m_RBUFDST = m_DELAYBUF + m_DELAYPTR;
#else
			m_RBUFDST = m_RINGBUF + m_BUFPTR;
#endif
			if (m_Slots[sl].active)
			{
				SCSP_SLOT *slot = m_Slots + sl;
				u16 Enc;

				s32 sample = UpdateSlot(slot);

				// SDIR ("sound direct") sends the raw sample straight to the output,
				// bypassing the envelope generator AND the TL attenuator (the EG/ALFO
				// bypass is handled in UpdateSlot). BOTH downstream mixes -- the DSP
				// input feed here and the direct-output mix below -- must therefore
				// zero TL when SDIR is set, otherwise a slot programmed with SDIR=1 +
				// a large TL is wrongly muted.
				// (Flash Beats keys its SFX with SDIR=1, TL=0xff = -95 dB; in
				// particular its in-game/"Voice" SFX route only through the DSP
				// (DISDL=0, IMXL>0), so without this the effect path is starved to
				// near-silence.)
				u16 eff_tl = SDIR(slot) ? 0 : TL(slot);
				Enc = ((eff_tl) << 0x0) | ((IMXL(slot)) << 0xd);
				m_DSP.SetSample((sample*m_LPANTABLE[Enc]) >> (SHIFT-2), ISEL(slot), IMXL(slot));
				u16 dir_tl = SDIR(slot) ? 0 : TL(slot);
				Enc = ((dir_tl) << 0x0) | ((DIPAN(slot)) << 0x8) | ((DISDL(slot)) << 0xd);
				{
					smpl += (sample * m_LPANTABLE[Enc]) >> SHIFT;
					smpr += (sample * m_RPANTABLE[Enc]) >> SHIFT;
				}
			}

#if SCSP_FM_DELAY
			m_RINGBUF[(m_BUFPTR + 64 - (SCSP_FM_DELAY - 1)) & 63] = m_DELAYBUF[(m_DELAYPTR + SCSP_FM_DELAY - (SCSP_FM_DELAY - 1)) % SCSP_FM_DELAY];
#endif
			++m_BUFPTR;
			m_BUFPTR &= 63;
#if SCSP_FM_DELAY
			++m_DELAYPTR;
			if (m_DELAYPTR > SCSP_FM_DELAY-1) m_DELAYPTR = 0;
#endif
		}

		m_DSP.Step();

		for (int i = 0; i < 16; ++i)
		{
			SCSP_SLOT *slot = m_Slots + i;
			if (EFSDL(slot))
			{
				u16 Enc = ((EFPAN(slot)) << 0x8) | ((EFSDL(slot)) << 0xd);
				smpl += (m_DSP.EFREG[i] * m_LPANTABLE[Enc]) >> SHIFT;
				smpr += (m_DSP.EFREG[i] * m_RPANTABLE[Enc]) >> SHIFT;
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			SCSP_SLOT *slot = m_Slots + i + 16; // 100217, 100237 EFSDL, EFPAN for EXTS0/1
			if (EFSDL(slot))
			{
				m_DSP.EXTS[i] = s32(stream.get(i, s) * 32768.0);
				u16 Enc = ((EFPAN(slot)) << 0x8) | ((EFSDL(slot)) << 0xd);
				smpl += (m_DSP.EXTS[i] * m_LPANTABLE[Enc]) >> SHIFT;
				smpr += (m_DSP.EXTS[i] * m_RPANTABLE[Enc]) >> SHIFT;
			}
		}

		if (DAC18B())
		{
			// MiSTer SCSP MVolCalc shifts the 18-bit DAC input left by two.
			// The pan tables already scale the mix by four, so retain that
			// scale here instead of cancelling it as in the 16-bit path.
			stream.put_int_clamp(0, s, smpl, 32768);
			stream.put_int_clamp(1, s, smpr, 32768);
		}
		else
		{
			stream.put_int_clamp(0, s, smpl >> 2, 32768);
			stream.put_int_clamp(1, s, smpr >> 2, 32768);
		}
	}
}

/* TODO: this needs to be timer-ized */
void Core::exec_dma()
{
	static u16 tmp_dma[3];
	int i;

	DeviceLog("SCSP: DMA transfer START\n"
				"DMEA: %04x DRGA: %04x DTLG: %04x\n"
				"DGATE: %d  DDIR: %d\n", m_dma.dmea, m_dma.drga, m_dma.dtlg, m_dma.dgate ? 1 : 0, m_dma.ddir ? 1 : 0);

	/* Copy the dma values in a temp storage for resuming later */
	/* (DMA *can't* overwrite its parameters).                  */
	if (!(m_dma.ddir))
	{
		for (i = 0; i < 3; i++)
			tmp_dma[i] = m_udata.data[(0x12 + (i * 2)) / 2];
	}

	/* note: we don't use space.read_word / write_word because it can happen that SH-2 enables the DMA instead of m68k. */
	/* TODO: don't know if params auto-updates, I guess not ... */
	if (m_dma.ddir)
	{
		if (m_dma.dgate)
		{
			popmessage("Check: SCSP DMA DGATE enabled, contact MAME/MESSdev");
			for (i = 0; i < m_dma.dtlg; i += 2)
			{
				ram.write_word(m_dma.dmea, 0);
				m_dma.dmea += 2;
			}
		}
		else
		{
			for (i = 0; i < m_dma.dtlg; i += 2)
			{
				u16 tmp;
				tmp = r16(m_dma.drga);
				ram.write_word(m_dma.dmea, tmp);
				m_dma.dmea += 2;
				m_dma.drga += 2;
			}
		}
	}
	else
	{
		if (m_dma.dgate)
		{
			popmessage("Check: SCSP DMA DGATE enabled, contact MAME/MESSdev");
			for (i = 0; i < m_dma.dtlg; i += 2)
			{
				w16(m_dma.drga, 0);
				m_dma.drga += 2;
			}
		}
		else
		{
			for (i = 0; i < m_dma.dtlg; i += 2)
			{
				u16 tmp = read_word(m_dma.dmea);
				w16(m_dma.drga, tmp);
				m_dma.dmea += 2;
				m_dma.drga += 2;
			}
		}
	}

	/*Resume the values*/
	if (!(m_dma.ddir))
	{
		for (i = 0; i < 3; i++)
			m_udata.data[(0x12 + (i * 2)) / 2] = tmp_dma[i];
	}

	/* Job done */
	m_udata.data[0x16/2] &= ~0x1000;
	/* request a dma end irq (TODO: make it inside the interface) */
	if (m_udata.data[0x1e/2] & 0x10)
	{
		popmessage("SCSP DMA IRQ triggered");
		m_irq_cb(DecodeSCI(SCIDMA), HOLD_LINE);
	}
}


u16 Core::read(offs_t offset)
{
	return r16(offset * 2);
}

void Core::write(offs_t offset, u16 data, u16 mem_mask)
{

	u16 tmp = r16(offset * 2);
	COMBINE_DATA(&tmp);
	w16(offset * 2, tmp);
}

//LFO handling

#define LFIX(v) ((u32) ((float) (1 << LFO_SHIFT) * (v)))

//Convert DB to multiply amplitude
#define DB(v)   LFIX(powf(10.0f, v / 20.0f))

//Convert cents to step increment
#define CENTS(v) LFIX(powf(2.0f, v / 1200.0f))


static const float LFOFreq[32] =
{
	0.17f,0.19f,0.23f,0.27f,0.34f,0.39f,0.45f,0.55f,0.68f,0.78f,0.92f,1.10f,1.39f,1.60f,1.87f,2.27f,
	2.87f,3.31f,3.92f,4.79f,6.15f,7.18f,8.60f,10.8f,14.4f,17.2f,21.5f,28.7f,43.1f,57.4f,86.1f,172.3f
};
static const float ASCALE[8] = {0.0f,0.4f,0.8f,1.5f,3.0f,6.0f,12.0f,24.0f};
static const float PSCALE[8] = {0.0f,7.0f,13.5f,27.0f,55.0f,112.0f,230.0f,494.0f};


void Core::LFO_Init()
{
	for (int i = 0; i < 256; ++i)
	{
		int a,p;
//      float TL;
		//Saw
		a = 255-i;
		if (i < 128)
			p = i;
		else
			p = i - 256;
		m_ALFO_SAW[i] = a;
		m_PLFO_SAW[i] = p;

		//Square
		if (i < 128)
		{
			a = 255;
			p = 127;
		}
		else
		{
			a = 0;
			p = -128;
		}
		m_ALFO_SQR[i] = a;
		m_PLFO_SQR[i] = p;

		//Tri
		if (i < 128)
			a = 255 - (i * 2);
		else
			a = (i * 2) - 256;
		if (i < 64)
			p = i * 2;
		else if (i < 128)
			p = 255 - i * 2;
		else if (i < 192)
			p = 256 - i * 2;
		else
			p = i * 2 - 511;
		m_ALFO_TRI[i] = a;
		m_PLFO_TRI[i] = p;

		//noise
		//a=lfo_noise[i];
		a = noise() & 0xff;
		p = 128 - a;
		m_ALFO_NOI[i] = a;
		m_PLFO_NOI[i] = p;
	}

	for (int s = 0; s < 8; ++s)
	{
		float limit = PSCALE[s];
		for (int i = -128; i < 128; ++i)
		{
			m_PSCALES[s][i+128] = CENTS(((limit * (float) i) / 128.0f));
		}
		limit = -ASCALE[s];
		for (int i = 0; i < 256; ++i)
		{
			m_ASCALES[s][i] = DB(((limit * (float) i) / 256.0f));
		}
	}
}

s32 Core::PLFO_Step(SCSP_LFO_t *LFO)
{
	int p;
	LFO->phase += LFO->phase_step;
#if LFO_SHIFT!=8
	LFO->phase &= (1 << (LFO_SHIFT + 8)) - 1;
#endif
	p=LFO->table[LFO->phase >> LFO_SHIFT];
	p=LFO->scale[p+128];
	return p << (SHIFT - LFO_SHIFT);
}

s32 Core::ALFO_Step(SCSP_LFO_t *LFO)
{
	int p;
	LFO->phase += LFO->phase_step;
#if LFO_SHIFT!=8
	LFO->phase &= (1 << (LFO_SHIFT + 8)) - 1;
#endif
	p=LFO->table[LFO->phase >> LFO_SHIFT];
	p=LFO->scale[p];
	return p << (SHIFT - LFO_SHIFT);
}

void Core::LFO_ComputeStep(SCSP_LFO_t *LFO,u32 LFOF,u32 LFOWS,u32 LFOS,int ALFO)
{
	float step = (float) LFOFreq[LFOF] * 256.0f / 44100.0f;
	LFO->phase_step = (u32) ((float) (1 << LFO_SHIFT) * step);
	if (ALFO)
	{
		switch (LFOWS)
		{
			case 0: LFO->table = m_ALFO_SAW; break;
			case 1: LFO->table = m_ALFO_SQR; break;
			case 2: LFO->table = m_ALFO_TRI; break;
			case 3: LFO->table = m_ALFO_NOI; break;
		}
		LFO->scale = m_ASCALES[LFOS];
	}
	else
	{
		switch (LFOWS)
		{
			case 0: LFO->table = m_PLFO_SAW; break;
			case 1: LFO->table = m_PLFO_SQR; break;
			case 2: LFO->table = m_PLFO_TRI; break;
			case 3: LFO->table = m_PLFO_NOI; break;
		}
		LFO->scale = m_PSCALES[LFOS];
	}
}

}

#undef SCSP_FM_DELAY
#undef ASSERT_LINE
#undef CLEAR_LINE
#undef HOLD_LINE
#undef COMBINE_DATA
#undef DUMP
#undef SHIFT
#undef LFO_SHIFT
#undef FIX
#undef EG_SHIFT
#undef KEYONEX
#undef KEYONB
#undef SBCTL
#undef SSCTL
#undef LPCTL
#undef PCM8B
#undef SA
#undef LSA
#undef LEA
#undef D2R
#undef D1R
#undef EGHOLD
#undef AR
#undef LPSLNK
#undef KRS
#undef DL
#undef RR
#undef STWINH
#undef SDIR
#undef TL
#undef MDL
#undef MDXSL
#undef MDYSL
#undef OCT
#undef FNS
#undef LFORE
#undef LFOF
#undef PLFOWS
#undef PLFOS
#undef ALFOWS
#undef ALFOS
#undef ISEL
#undef IMXL
#undef DISDL
#undef DIPAN
#undef EFSDL
#undef EFPAN
#undef MEM4B
#undef DAC18B
#undef MVOL
#undef RBL
#undef RBP
#undef MOFULL
#undef MOEMPTY
#undef MIOVF
#undef MIFULL
#undef MIEMPTY
#undef SCILV0
#undef SCILV1
#undef SCILV2
#undef SCIEX0
#undef SCIEX1
#undef SCIEX2
#undef SCIMID
#undef SCIDMA
#undef SCIIRQ
#undef SCITMA
#undef SCITMB
#undef USEDSP
#undef LFIX
#undef DB
#undef CENTS
