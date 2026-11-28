// C-3 resident region ABI. No calls inside a published region.
#ifndef FBNEO_SH3_ARM64_V3_RESIDENT_H
#define FBNEO_SH3_ARM64_V3_RESIDENT_H
namespace Sh3V2 {
struct ResidentReturn {uint64_t guest,host;};
struct ResidentContext {
	Sh3Native::Memory memory;
	uint32_t remaining,ppc,ops,blocks;
	ResidentReturn ras[8];
	uint32_t dynamicSite,dynamicTarget;
};
static_assert(offsetof(ResidentContext,remaining)==80 &&
	offsetof(ResidentContext,ras)==96 && offsetof(ResidentContext,dynamicSite)==224,
	"resident context ABI");
}
extern "C" void fbneo_sh3_v3_enter(const void *,uint32_t *,uint32_t *,
	uint32_t *,Sh3V2::ResidentContext *);
asm(R"ASM(
.pushsection .text.fbneo_sh3_v3_enter,"ax",%progbits
.p2align 4
.global fbneo_sh3_v3_enter
.hidden fbneo_sh3_v3_enter
.type fbneo_sh3_v3_enter,%function
fbneo_sh3_v3_enter:
.cfi_startproc
.inst 0xd503245f
stp x19,x20,[sp,#-96]!
.cfi_def_cfa_offset 96
.cfi_offset x19,-96
.cfi_offset x20,-88
stp x21,x22,[sp,#16]
.cfi_offset x21,-80
.cfi_offset x22,-72
stp x23,x24,[sp,#32]
.cfi_offset x23,-64
.cfi_offset x24,-56
stp x25,x26,[sp,#48]
.cfi_offset x25,-48
.cfi_offset x26,-40
stp x27,x28,[sp,#64]
.cfi_offset x27,-32
.cfi_offset x28,-24
stp x29,x30,[sp,#80]
.cfi_offset x29,-16
.cfi_offset x30,-8
mov x16,x0
mov x19,x1
mov x20,x2
mov x21,x3
mov x22,x4
ldp w23,w24,[x19]
ldp w25,w26,[x19,#8]
ldr w27,[x19,#16]
ldr w28,[x19,#60]
ldr w5,[x20]
ldp w29,w1,[x22,#80]
ldr w2,[x22,#60]
mov w0,wzr
mov w3,wzr
mov w4,wzr
blr x16
str w16,[x22,#44]
str w2,[x22,#60]
stp w29,w1,[x22,#80]
stp w3,w4,[x22,#88]
stp w23,w24,[x19]
stp w25,w26,[x19,#8]
str w27,[x19,#16]
str w28,[x19,#60]
str w5,[x20]
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
ldp x19,x20,[sp],#96
.cfi_restore x19
.cfi_restore x20
.cfi_def_cfa_offset 0
ret
.cfi_endproc
.size fbneo_sh3_v3_enter,.-fbneo_sh3_v3_enter
.popsection
)ASM");
#endif
