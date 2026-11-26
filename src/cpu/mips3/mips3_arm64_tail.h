// SPDX-License-Identifier: BSD-3-Clause
#ifndef FBNEO_MIPS3_ARM64_TAIL_H
#define FBNEO_MIPS3_ARM64_TAIL_H

// Generated spans use x0..x17 and do not call helpers. Keep the runner
// state in callee-saved registers. The shared machine-code continuation
// commits completed blocks and branches to a warmed successor without
// returning to the C++ dispatcher. Side exits are handled by C++ exactly
// once, using the last span's unmodified return value.
struct Mips3TailNode {
	uint64_t gen;
	uint32_t pc[2],off[2];
	uint32_t cycles,fall,count,code;
};
struct Mips3TailContext {
	uint64_t *regs;
	const void *read,*write;
	const uint8_t *arena,*codeBase;
	uint64_t gen,budget;
	uint64_t cycles,jumps,ram,branches;
	Mips3TailNode *node;
	uint64_t result;
	uint32_t pc,nodeOffset;
};
static_assert(offsetof(Mips3TailNode,gen)==0 && offsetof(Mips3TailNode,pc)==8 &&
	offsetof(Mips3TailNode,off)==16 && offsetof(Mips3TailNode,cycles)==24 &&
	offsetof(Mips3TailNode,fall)==28 && offsetof(Mips3TailNode,count)==32 &&
	offsetof(Mips3TailNode,code)==36,"tail node ABI");
static_assert(offsetof(Mips3TailContext,regs)==0 && offsetof(Mips3TailContext,read)==8 &&
	offsetof(Mips3TailContext,write)==16 && offsetof(Mips3TailContext,arena)==24 &&
	offsetof(Mips3TailContext,codeBase)==32 && offsetof(Mips3TailContext,gen)==40 &&
	offsetof(Mips3TailContext,budget)==48 && offsetof(Mips3TailContext,cycles)==56 &&
	offsetof(Mips3TailContext,jumps)==64 && offsetof(Mips3TailContext,ram)==72 &&
	offsetof(Mips3TailContext,branches)==80 && offsetof(Mips3TailContext,node)==88 &&
	offsetof(Mips3TailContext,result)==96 && offsetof(Mips3TailContext,pc)==104 &&
	offsetof(Mips3TailContext,nodeOffset)==108,"tail context ABI");
extern "C" void Mips3TailRun(Mips3TailContext *);
asm(
".pushsection .text.Mips3TailRun,\"ax\",%progbits\n"
".p2align 4\n"
".global Mips3TailRun\n"
".hidden Mips3TailRun\n"
".type Mips3TailRun,%function\n"
"Mips3TailRun:\n"
" .inst 0xd503245f\n" // BTI c
" stp x29,x30,[sp,#-96]!\n"
" stp x19,x20,[sp,#16]\n"
" stp x21,x22,[sp,#32]\n"
" stp x23,x24,[sp,#48]\n"
" stp x25,x26,[sp,#64]\n"
" stp x27,x28,[sp,#80]\n"
" mov x19,x0\n"
" ldr x20,[x19,#88]\n"
" ldr w21,[x19,#104]\n"
" ldr x22,[x19,#48]\n"
" mov x23,xzr\n"
" mov x24,xzr\n"
" mov x25,xzr\n"
" mov x26,xzr\n"
" ldr x27,[x19,#32]\n"
" ldr x28,[x19,#24]\n"
".Lfb_mips_tail_enter:\n"
" ldr w16,[x20,#36]\n"
" add x16,x27,x16\n"
" ldr x0,[x19]\n"
" mov x1,xzr\n"
" mov x2,xzr\n"
" ldp x3,x4,[x19,#8]\n"
" and w5,w21,#0xfffff000\n"
" mov w6,#-1\n"
" cmp x22,x6\n"
" csel x6,x22,x6,ls\n"
" adr x30,.Lfb_mips_tail_done\n"
" br x16\n" // call-compatible branch into BTI c JIT entry
".Lfb_mips_tail_done:\n"
" .inst 0xd50324df\n" // BTI jc, shared return continuation
// Guard failures always return to the original handlers. An early
// superblock exit (taken branch, annulled slot, or budget boundary)
// already has an exact target and completed-entry/pseudo-op count.
" tbnz w0,#31,.Lfb_mips_tail_exit\n"
" and w9,w0,#0x1ff\n"
" cbz w9,.Lfb_mips_tail_exit\n"
" lsr x11,x0,#32\n"
" tbnz w0,#15,.Lfb_mips_tail_early\n"
" ldr w10,[x20,#32]\n"
" cmp w9,w10\n"
" b.ne .Lfb_mips_tail_exit\n"
" ldr w10,[x20,#24]\n"
" cmp w11,#1\n"
" b.ne .Lfb_mips_tail_target\n"
" and w11,w21,#0xfffff000\n"
" ldr w12,[x20,#28]\n"
" add w11,w11,w12\n"
" b .Lfb_mips_tail_target\n"
".Lfb_mips_tail_early:\n"
" ubfx w10,w0,#9,#6\n"
" subs w10,w9,w10\n"
" b.ls .Lfb_mips_tail_exit\n"
".Lfb_mips_tail_target:\n"
" cmp x22,x10\n"
" b.ls .Lfb_mips_tail_exit\n"
" ldr x12,[x20]\n"
" ldr x13,[x19,#40]\n"
" cmp x12,x13\n"
" b.ne .Lfb_mips_tail_exit\n"
" ldr w12,[x20,#8]\n"
" cmp w11,w12\n"
" b.ne .Lfb_mips_tail_second\n"
" ldr w12,[x20,#16]\n"
" b .Lfb_mips_tail_link\n"
".Lfb_mips_tail_second:\n"
" ldr w12,[x20,#12]\n"
" cmp w11,w12\n"
" b.ne .Lfb_mips_tail_exit\n"
" ldr w12,[x20,#20]\n"
".Lfb_mips_tail_link:\n"
" add x12,x28,x12\n"
" ldr w13,[x19,#108]\n"
" add x12,x12,x13\n"
" ldr w13,[x12,#32]\n"
" cbz w13,.Lfb_mips_tail_exit\n"
" sub x22,x22,x10\n"
" add x23,x23,x10\n"
" add x24,x24,#1\n"
" ubfx w13,w0,#16,#9\n"
" add x25,x25,x13\n"
" ubfx w13,w0,#25,#6\n"
" add x26,x26,x13\n"
" mov x20,x12\n"
" mov w21,w11\n"
" b .Lfb_mips_tail_enter\n"
".Lfb_mips_tail_exit:\n"
" stp x23,x24,[x19,#56]\n"
" stp x25,x26,[x19,#72]\n"
" str x20,[x19,#88]\n"
" str x0,[x19,#96]\n"
" str w21,[x19,#104]\n"
" ldp x27,x28,[sp,#80]\n"
" ldp x25,x26,[sp,#64]\n"
" ldp x23,x24,[sp,#48]\n"
" ldp x21,x22,[sp,#32]\n"
" ldp x19,x20,[sp,#16]\n"
" ldp x29,x30,[sp],#96\n"
" ret\n"
".size Mips3TailRun,.-Mips3TailRun\n"
".popsection\n"
);
#endif
