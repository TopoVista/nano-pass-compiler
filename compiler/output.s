	.text
	.file	"nano_module"
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3, 0x0                          # -- Begin function mix
.LCPI0_0:
	.quad	0x4000000000000000              # double 2
	.text
	.globl	mix
	.p2align	4, 0x90
	.type	mix,@function
mix:                                    # @mix
	.cfi_startproc
# %bb.0:                                # %entry
	movsd	%xmm0, -16(%rsp)
	movl	%edi, -4(%rsp)
	cmpl	$6, %edi
	jl	.LBB0_2
# %bb.1:                                # %then
	movsd	-16(%rsp), %xmm0                # xmm0 = mem[0],zero
	addsd	%xmm0, %xmm0
	retq
.LBB0_2:                                # %ifcont
	movsd	-16(%rsp), %xmm0                # xmm0 = mem[0],zero
	divsd	.LCPI0_0(%rip), %xmm0
	retq
.Lfunc_end0:
	.size	mix, .Lfunc_end0-mix
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3, 0x0                          # -- Begin function main
.LCPI1_0:
	.quad	0x4020000000000000              # double 8
	.text
	.globl	main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	subq	$24, %rsp
	.cfi_def_cfa_offset 32
	movsd	.LCPI1_0(%rip), %xmm0           # xmm0 = [8.0E+0,0.0E+0]
	movl	$10, %edi
	callq	mix@PLT
	movsd	%xmm0, 8(%rsp)
	movl	$.L__unnamed_1, %edi
	movb	$1, %al
	callq	printf@PLT
	movsd	.LCPI1_0(%rip), %xmm0           # xmm0 = [8.0E+0,0.0E+0]
	movl	$2, %edi
	callq	mix@PLT
	movsd	%xmm0, 16(%rsp)
	movl	$.L__unnamed_2, %edi
	movb	$1, %al
	callq	printf@PLT
	xorl	%eax, %eax
	addq	$24, %rsp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
	.cfi_endproc
                                        # -- End function
	.type	.L__unnamed_1,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.asciz	"%f\n"
	.size	.L__unnamed_1, 4

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"%f\n"
	.size	.L__unnamed_2, 4

	.section	".note.GNU-stack","",@progbits
