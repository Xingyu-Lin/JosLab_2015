// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
    { "showmappings", "Display the corresponding physical page address of an interver", mon_showmappings},
    { "setperm", "Set the permission bit of a page table of a virtual address", mon_setperm},
    { "dumpvm", "Dump contents from start to end of virtual address", mon_dumpvm}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}


uint32_t get_num(char* str);

int 
mon_setperm(int argc, char**argv, struct Trapframe *tf)
{
    if (argc != 4)
    {
            cprintf("Usage: setperm va [p/t/w] [0/1]\n");
            return 0;
    }
    uint32_t va = get_num(argv[1]);
    pte_t* pte = pgdir_walk(kern_pgdir, (const void*)va, 0);
    char bit = *argv[2];
    int perm = *argv[3]-'0';
    if (pte == NULL)
    {
            cprintf("Address %x havn't been mapped yet.\n", va);
            return 0;
    }
    switch (bit)
    {
            case 'p' : if (perm) *pte = *pte | PTE_P; else *pte = *pte & ~ PTE_P; break;
            case 'w' : if (perm) *pte = *pte | PTE_W; else *pte = *pte & ~ PTE_W; break;
            case 'u' : if (perm) *pte = *pte | PTE_U; else *pte = *pte & ~ PTE_U; break;
    }
    return 0;
}

int
mon_dumpvm(int argc, char**argv, struct Trapframe *tf)
{
    if (argc != 3)
    {
            cprintf("Usage: dumpvm st en\n");
            return 0;
    }
    void** st = (void**) get_num(argv[1]);
    void** en = (void**) get_num(argv[2]);
    void** ptr = st;
    while (ptr <= en) 
    {
            cprintf("%x : %x\n", ptr, *ptr);
            ++ptr;
    }
    return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
   if (argc != 3)
   {
           cprintf("Usage: shwmappings st_va en_va\n");
           return 0;
   }
   uint32_t st,en,va;
   pte_t *pte;
   va = st = get_num(argv[1]);
   en = get_num(argv[2]);
   while (va <= en)
   {
        pte = pgdir_walk(kern_pgdir, (const void*)va, 0);
        if (pte == NULL)
                cprintf("virtual page address %x maps to NULL\n", va);
        else
                cprintf("virtual page address %x maps to %x, PTE_W %x, PTE_P %x, PTE_U %x\n",
                                va, PTE_ADDR(*pte), *pte & PTE_W, *pte & PTE_P, *pte & PTE_U);
        va += PGSIZE;
   }
   return 0;
}

uint32_t
get_num(char* str)
{
    char* now = str + 2; // jump over 0x
    uint32_t res = 0;
    while (*now)
    {
            if ('0' <= *now && *now <='9') res = res * 16 + (*now - '0');
            else res = res * 16 + (*now - 'a' + 10);
            ++now;
    }
    return res;
}
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	struct Eipdebuginfo t;
	cprintf("%s\n","Stack backtrace:");
	uint32_t *_ebp;
	for (_ebp = (uint32_t*)read_ebp(); _ebp !=0; _ebp =(uint32_t*)*_ebp)
	{
		cprintf("ebp %x  ",(uint32_t)_ebp);
		cprintf("eip %x  ",*(_ebp+1));
		uint32_t eip=*(_ebp+1);
		cprintf("args");
		int i;
		for (i=0; i<5; ++i)
			cprintf(" %08x",*(_ebp+i+2));
		cprintf("\n");
		debuginfo_eip(eip, &t);
		cprintf("%s:%d: %.*s+%d\n", t.eip_file, t.eip_line, t.eip_fn_namelen, t.eip_fn_name, eip - (uint32_t)t.eip_fn_addr);
	}
		
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;
	unsigned int i = 0x00646c72;
	cprintf("H%x Wo%s\n", 57616, &i);

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");
	if (tf != NULL)
		print_trapframe(tf);
	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
