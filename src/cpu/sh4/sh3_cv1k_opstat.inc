// Opt-in compile-time diagnostic. Count executed dispatcher visits, not
// cycles or instructions elided by the separate poll/data-loop kernels.
#if defined(SH3_CV1K_OPSTAT) && SH3_CV1K_OPSTAT
struct Cv1kOpStat {
	UINT64 ops[SH3_ID_COUNT],fused[SH3_ID_COUNT],indirect[SH3_ID_COUNT];
	UINT64 ram,rom,mapped,idlePage,idleReject,handler,unaligned,p4;
	UINT64 branchReject[7],dtBusy,dtHandler;
};
static Cv1kOpStat cv1kOpStat;
static unsigned cv1kOpStatFrame;
static const TCHAR *const cv1kOpNames[]={
#define SH3_OPCODE(name) _T(#name),
#include "sh3_decode_ops.inc"
#undef SH3_OPCODE
};
#define CV1K_STAT(field) (++cv1kOpStat.field)
#define CV1K_VISIT(op) CV1K_STAT(ops[sh3OpIds[op]])
static void Cv1kOpStatReset()
{
	cv1kOpStat={};cv1kOpStatFrame=0;
}
static void Cv1kOpStatReport()
{
	++cv1kOpStatFrame;
	if(cv1kOpStatFrame%120)return;
	const auto &s=cv1kOpStat;
	bprintf(PRINT_UI,_T("CV1K OPSTAT frame %u: direct RAM %llu ROM %llu mapped %llu idle-page %llu idle-reject %llu handler %llu align %llu P4 %llu DT busy %llu DT read reject %llu\n"),
		cv1kOpStatFrame,(unsigned long long)s.ram,(unsigned long long)s.rom,(unsigned long long)s.mapped,
		(unsigned long long)s.idlePage,(unsigned long long)s.idleReject,(unsigned long long)s.handler,
		(unsigned long long)s.unaligned,(unsigned long long)s.p4,(unsigned long long)s.dtBusy,(unsigned long long)s.dtHandler);
	bprintf(PRINT_UI,_T("CV1K OPSTAT branch rejects: budget %llu data/count probe %llu BRA-self %llu alignment %llu fetch %llu slot opcode %llu slot memory %llu\n"),
		(unsigned long long)s.branchReject[0],(unsigned long long)s.branchReject[1],(unsigned long long)s.branchReject[2],
		(unsigned long long)s.branchReject[3],(unsigned long long)s.branchReject[4],(unsigned long long)s.branchReject[5],
		(unsigned long long)s.branchReject[6]);
	for(unsigned i=0;i<SH3_ID_COUNT;i++)if(s.ops[i]||s.fused[i])
		bprintf(PRINT_UI,_T("CV1K OPSTAT %s visits %llu fused-slot %llu indirect-slot %llu\n"),cv1kOpNames[i],
			(unsigned long long)s.ops[i],(unsigned long long)s.fused[i],(unsigned long long)s.indirect[i]);
	cv1kOpStat={};
}
#else
#define CV1K_STAT(field) ((void)0)
#define CV1K_VISIT(op) ((void)0)
#endif
