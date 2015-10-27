/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
.data;\
    .long name;\
.text;\
    .globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 4;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
.data;\
    .long name;\
.text;\
	.globl name;							\
	.type name, @function;						\
	.align 4;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps


#define FUNPADDING \
        .data;\
            .long 0
.data
    .align 4
    .globl funs

funs:
.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
	TRAPHANDLER_NOEC(routine_divde,T_DIVIDE)    // 0		 divide error
	TRAPHANDLER_NOEC(routine_debug,T_DEBUG)     // 1		 debug exception
	TRAPHANDLER_NOEC(routine_nmi,T_NMI)      // 2		 non-maskable interrupt
	TRAPHANDLER_NOEC(routine_brkpt,T_BRKPT)     // 3		 breakpoint
	TRAPHANDLER_NOEC(routine_oflow,T_OFLOW)     // 4		 overflow
	TRAPHANDLER_NOEC(routine_bound,T_BOUND)     // 5		 bounds check
	TRAPHANDLER_NOEC(routine_illop,T_ILLOP)     // 6		 illegal opcode
	TRAPHANDLER_NOEC(routine_device,T_DEVICE)    // 7		 device not available
	TRAPHANDLER(routine_dblft,T_DBLFLT)         // 8		 double fault
    FUNPADDING
	TRAPHANDLER(routine_tss,T_TSS)           //10		 invalid task switch segment
	TRAPHANDLER(routine_segnp,T_SEGNP)          //11		 segment not present
	TRAPHANDLER(routine_stack,T_STACK)          //12		 stack exception
	TRAPHANDLER(routine_gpflt,T_GPFLT)          //13		 general protection fault
	TRAPHANDLER(routine_pgflt,T_PGFLT)          //14		 page fault
    FUNPADDING
    TRAPHANDLER_NOEC(routine_fperr,T_FPERR)     //16		 floating point error
    .data           // padding
        .space 124
.text
    TRAPHANDLER_NOEC(routine_syscall,T_SYSCALL)    //48         syscall
/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
    pushl %ds;
    pushl %es;
    pushal;
    pushl $GD_KD;
    popl %ds;
    pushl $GD_KD;
    popl %es;
    pushl %esp;
    call trap;
