// C-2 static native links. A generated stub holds a stable descriptor and
// two patchable B instructions. One outer ABI frame; no C++ calls in a
// group. Bodies retain their original leaf/wide ABI and flush guest regs.
#ifndef FBNEO_SH3_ARM64_V2_LINK_H
#define FBNEO_SH3_ARM64_V2_LINK_H
namespace Sh3V2 {
enum : uint32_t { LinkTerminal=1,LinkDelayed=2,LinkCall=4,LinkDynamic=8,
	LinkWide=16,LinkTrusted=32,LinkConditional=64 };
struct LinkDesc {
	const void *body;
	const uint16_t *source;
	uint64_t mapVersion,generation,opaque,slice;
	uint32_t pc,takenPC,fallPC,flags,count,extra,watch,active;
	uint16_t ops[64];
	uint32_t offset,edge[2],shortExits;
};
struct LinkContext {
	Sh3Native::Memory memory;
	uint64_t mapVersion,slice;
	uint32_t remaining,ppc,ops,blocks,taken,notTaken,guards,status;
	uint32_t checks,skips,dirty,guardPC,guardOps,attempts,untracked,limit;
};
static_assert(offsetof(LinkDesc,source)==8 && offsetof(LinkDesc,mapVersion)==16 &&
	offsetof(LinkDesc,pc)==48 && offsetof(LinkDesc,active)==76 &&
	offsetof(LinkDesc,ops)==80 && offsetof(LinkDesc,offset)==208,"link descriptor ABI");
static_assert(sizeof(Sh3Native::Memory)==80 && offsetof(LinkContext,mapVersion)==80 &&
	offsetof(LinkContext,remaining)==96 && offsetof(LinkContext,status)==124 &&
	offsetof(LinkContext,checks)==128 && offsetof(LinkContext,limit)==156,"link context ABI");
}
extern "C" void fbneo_sh3_v2_link_enter(const void *,uint32_t *,uint32_t *,uint32_t *,Sh3V2::LinkContext *);
extern "C" void fbneo_sh3_v2_link_step();
asm(R"ASM(
.pushsection .text.fbneo_sh3_v2_link,"ax",%progbits
.p2align 4
.global fbneo_sh3_v2_link_enter
.hidden fbneo_sh3_v2_link_enter
.type fbneo_sh3_v2_link_enter,%function
fbneo_sh3_v2_link_enter:
.cfi_startproc
.inst 0xd503245f
stp x19,x20,[sp,#-112]!
.cfi_def_cfa_offset 112
.cfi_offset x19,-112
.cfi_offset x20,-104
stp x21,x22,[sp,#16]
.cfi_offset x21,-96
.cfi_offset x22,-88
stp x23,x24,[sp,#32]
.cfi_offset x23,-80
.cfi_offset x24,-72
stp x25,x26,[sp,#48]
.cfi_offset x25,-64
.cfi_offset x26,-56
stp x27,x28,[sp,#64]
.cfi_offset x27,-48
.cfi_offset x28,-40
stp x29,x30,[sp,#80]
.cfi_offset x29,-32
.cfi_offset x30,-24
mov x16,x0
mov x19,x1
mov x20,x2
mov x21,x3
mov x22,x4
adr x17,.Lcv1k_link_return
str x17,[sp,#96]
br x16
.Lcv1k_link_return:
.inst 0xd50324df
ldp x29,x30,[sp,#80]
.cfi_restore x29
.cfi_restore x30
ldp x27,x28,[sp,#64]
.cfi_restore x27
.cfi_restore x28
ldp x25,x26,[sp,#48]
.cfi_restore x25
.cfi_restore x26
ldp x23,x24,[sp,#32]
.cfi_restore x23
.cfi_restore x24
ldp x21,x22,[sp,#16]
.cfi_restore x21
.cfi_restore x22
ldp x19,x20,[sp],#112
.cfi_restore x19
.cfi_restore x20
.cfi_def_cfa_offset 0
ret
.cfi_endproc
.size fbneo_sh3_v2_link_enter,.-fbneo_sh3_v2_link_enter

// x19..x22 = GPR/SR/EA/context; x29 = stable descriptor.
// Entry and exit are private to the outer trampoline's frame.
.p2align 4
.global fbneo_sh3_v2_link_step
.hidden fbneo_sh3_v2_link_step
.type fbneo_sh3_v2_link_step,%function
fbneo_sh3_v2_link_step:
.inst 0xd503245f
ldr w8,[x22,#148]
add w8,w8,#1
str w8,[x22,#148]
ldr w8,[x29,#76]
cbz w8,.Lcv1k_link_stale
ldr x8,[x29,#16]
ldr x7,[x22,#80]
cmp x8,x7
b.ne .Lcv1k_link_stale
ldr w8,[x29,#48]
ldr w7,[x22,#44]
cmp w8,w7
b.ne .Lcv1k_link_stale
ldr w8,[x22,#108]
ldr w7,[x22,#156]
cmp w8,w7
b.hs .Lcv1k_link_budget
ldr w8,[x29,#64]
ldr w7,[x29,#68]
add w8,w8,w7
ldr w7,[x22,#96]
cmp w7,w8
b.lo .Lcv1k_link_budget

// The source pointer is stable during this group: the original bodies
// cannot call devices, run timers, remap memory, or change IRQ state.
// Dirty or external-slice changes compare the exact opcode span.
// Unknown backing pages and VERIFY_ALWAYS compare on every entry.
ldr w14,[x29,#72]
ldr x6,[x22,#72]
ldr x7,[x6,x14,lsl #3]
ldr x8,[x6,#2040]
ldr x9,[x29,#24]
cmp x7,x9
b.ne .Lcv1k_link_dirty
ldr x9,[x29,#32]
cmp x8,x9
b.ne .Lcv1k_link_dirty
mov w14,#0
ldr w9,[x29,#60]
tbz w9,#5,.Lcv1k_link_verify
ldr x9,[x29,#40]
ldr x10,[x22,#88]
cmp x9,x10
b.ne .Lcv1k_link_verify
ldr w9,[x22,#132]
add w9,w9,#1
str w9,[x22,#132]
b .Lcv1k_link_body
.Lcv1k_link_dirty:
mov w14,#1
ldr w9,[x22,#136]
add w9,w9,#1
str w9,[x22,#136]
.Lcv1k_link_verify:
ldr w9,[x22,#128]
add w9,w9,#1
str w9,[x22,#128]
ldr x9,[x29,#8]
add x10,x29,#80
ldr w11,[x29,#64]
cmp w11,#4
b.lo .Lcv1k_link_tail
.Lcv1k_link_words:
ldr x12,[x9],#8
ldr x13,[x10],#8
cmp x12,x13
b.ne .Lcv1k_link_changed
sub w11,w11,#4
cmp w11,#4
b.hs .Lcv1k_link_words
.Lcv1k_link_tail:
cbz w11,.Lcv1k_link_valid
.Lcv1k_link_half:
ldrh w12,[x9],#2
ldrh w13,[x10],#2
cmp w12,w13
b.ne .Lcv1k_link_changed
subs w11,w11,#1
b.ne .Lcv1k_link_half
.Lcv1k_link_valid:
str x7,[x29,#24]
str x8,[x29,#32]
ldr x9,[x22,#88]
str x9,[x29,#40]

.Lcv1k_link_body:
ldr x8,[x29,#8]
str x8,[x22,#24]
ldr w8,[x29,#64]
lsl x8,x8,#1
str x8,[x22,#32]
str x30,[sp,#104]
ldr x16,[x29]
ldr w8,[x29,#60]
tbnz w8,#4,.Lcv1k_link_wide
mov x0,x19
mov x1,x20
mov x2,x21
mov x3,x22
.Lcv1k_link_wide:
blr x16
ldr x30,[sp,#104]

// Original compact exit ABI. Publish only completed guest operations.
// No callbacks occur inside a native group, so the outer C++ call
// applies the aggregate cycle debit before the next interpreter step.
and w6,w0,#0xff
and w7,w0,#0x300
ldr w8,[x22,#108]
add w8,w8,#1
str w8,[x22,#108]
ldr w8,[x29,#48]
add w9,w8,w6,lsl #1
mov w10,w9
mov w11,w6
ldr w12,[x29,#60]
cbz w6,.Lcv1k_link_guard
tbz w12,#0,.Lcv1k_link_linear
cmp w7,#0x100
b.eq .Lcv1k_link_taken
cmp w7,#0x200
b.ne .Lcv1k_link_guard
ldr w13,[x22,#116]
add w13,w13,#1
str w13,[x22,#116]
mov w0,#2
b .Lcv1k_link_commit
.Lcv1k_link_taken:
ldr w9,[x21]
ldr w13,[x29,#68]
add w11,w11,w13
tbz w12,#1,.Lcv1k_link_pr
mov w10,w9
.Lcv1k_link_pr:
tbz w12,#2,.Lcv1k_link_taken_count
ldr w13,[x29,#56]
add w13,w13,#2
str w13,[x22,#60]
.Lcv1k_link_taken_count:
ldr w13,[x22,#112]
add w13,w13,#1
str w13,[x22,#112]
mov w0,#1
tbz w12,#3,.Lcv1k_link_commit
mov w0,#0 // dynamic exit: return to C++, never guess a target
b .Lcv1k_link_commit
.Lcv1k_link_linear:
ldr w13,[x29,#64]
cmp w6,w13
b.ne .Lcv1k_link_guard
mov w0,#1
b .Lcv1k_link_commit
.Lcv1k_link_guard:
ldr w13,[x29,#220]
add w13,w13,#1
cmp w6,#12
csel w13,w13,wzr,lo
str w13,[x29,#220]
ldr w13,[x22,#120]
add w13,w13,#1
str w13,[x22,#120]
mov w13,#2
str w13,[x22,#124]
str w8,[x22,#140]
str w6,[x22,#144]
mov w0,#0
cbz w6,.Lcv1k_link_done
.Lcv1k_link_commit:
ldr w13,[x22,#124]
cbnz w13,.Lcv1k_link_publish
str wzr,[x29,#220]
.Lcv1k_link_publish:
str w9,[x22,#44]
str w10,[x22,#100]
ldr w13,[x22,#104]
add w13,w13,w6
str w13,[x22,#104]
ldr w13,[x22,#96]
sub w13,w13,w11
str w13,[x22,#96]
.Lcv1k_link_done:
ret

.Lcv1k_link_changed:
ldr w9,[x29,#72]
cbz w9,.Lcv1k_link_changed_exit
cbnz w14,.Lcv1k_link_changed_exit
ldr w9,[x22,#152]
add w9,w9,#1
str w9,[x22,#152]
.Lcv1k_link_changed_exit:
mov w8,#1
b .Lcv1k_link_stop
.Lcv1k_link_stale:
mov w8,#3
b .Lcv1k_link_stop
.Lcv1k_link_budget:
mov w8,#4
.Lcv1k_link_stop:
str w8,[x22,#124]
mov w0,#0
ret
.size fbneo_sh3_v2_link_step,.-fbneo_sh3_v2_link_step
.popsection
)ASM");
namespace Sh3V2 {
static constexpr unsigned LinkTakenOffset=7*4,LinkFallOffset=8*4,LinkExitOffset=9*4;
inline bool PublishLink(CodeCache &cache,LinkDesc &desc){
	uint32_t words[16]={
		0xd50324dfu, // bti jc
		0x5800017du, // ldr x29, literal[12]
		0x58000190u, // ldr x16, literal[14]
		0xd63f0200u, // blr x16 (native step)
		0x340000a0u, // cbz w0, exit
		0x7100081fu, // cmp w0,#2
		0x54000040u, // b.eq fall
		0x14000002u, // taken: B exit (patchable)
		0x14000001u, // fall: B exit (patchable)
		0xf94033f0u, // ldr x16,[sp,#96]
		0xd61f0200u, // br x16
		0xd503201fu,0,0,0,0
	};
	const uintptr_t d=(uintptr_t)&desc,step=(uintptr_t)&fbneo_sh3_v2_link_step;
	memcpy(words+12,&d,8);memcpy(words+14,&step,8);
	return cache.Publish(words,16,desc.offset);
}
}
#endif
