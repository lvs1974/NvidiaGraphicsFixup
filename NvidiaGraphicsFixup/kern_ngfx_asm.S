//
//  kern_ngfx_asm.S
//  NvidiaGraphicsFixup
//
//  Copyright © 2018 lvs1974. All rights reserved.
//

#define PRESUBMIT_FROM_REG(x, y) \
	push %rdi; \
	push %rsi; \
	push %rdx; \
	push %rcx; \
	push %r8; \
	push %r9; \
	push %r10; \
	push %r11; \
	mov x, %rdi; \
	call y; \
	pop %r11; \
	pop %r10; \
	pop %r9; \
	pop %r8; \
	pop %rcx; \
	pop %rdx; \
	pop %rsi; \
	pop %rdi; \
	ret;

.globl _preSubmitHandlerOfficial
_preSubmitHandlerOfficial:
	// Choose the routing
	test %al, %al
	// Standard routing (for normal calls)
	jz __ZN4NGFX39nvVirtualAddressSpace_PreSubmitOfficialEPv
	// Wrapped routing (for patched calls)
	cmp $1,%al
	jz handle_rbx_off
	cmp $2,%al
	jz handle_r13_off
	cmp $3,%al
	jz handle_r12_off
	// Do we need more registers?
	ud2
handle_rbx_off:
	PRESUBMIT_FROM_REG(%rbx, __ZN4NGFX39nvVirtualAddressSpace_PreSubmitOfficialEPv)
handle_r13_off:
	PRESUBMIT_FROM_REG(%r13, __ZN4NGFX39nvVirtualAddressSpace_PreSubmitOfficialEPv)
handle_r12_off:
	PRESUBMIT_FROM_REG(%r12, __ZN4NGFX39nvVirtualAddressSpace_PreSubmitOfficialEPv)

.globl _orgSubmitHandlerOfficial
_orgSubmitHandlerOfficial:
	// This is the prologue we patched out
	push %rbp
	mov %rsp, %rbp
	// Jump to the original code
	mov _orgVaddrPresubmitOfficial(%rip), %rax
	jmp *%rax
