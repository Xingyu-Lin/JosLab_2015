// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

    if (((err & FEC_WR) == 0) || ((uvpd[PDX(addr)] & PTE_P)==0) || 
                    ((uvpt[PGNUM(addr)] & PTE_COW)==0) )
            panic("Page fault in lib/fork.c!\n");
	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

    if ((r = sys_page_alloc(0, (void*)PFTEMP, PTE_U|PTE_P|PTE_W))  <0)
                panic("alloc page error in lib/fork.c\n");
    addr = ROUNDDOWN(addr, PGSIZE);
    memcpy(PFTEMP, addr, PGSIZE);
    if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_U|PTE_P|PTE_W)) <0)
                panic("page map error in lib/fork.c\n");
    if ((r = sys_page_unmap(0, PFTEMP)) <0)
                panic("page unmap error in lib/fork.c\n");
// LAB 4: Your code here.

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
    int perm = PTE_P | PTE_U;
    if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) perm = perm | PTE_COW;     
    
    void* addr = (void*)(pn<<12);
    if ((r = sys_page_map(0, addr, envid, addr, perm)) < 0)
            panic("sys_page_map error in duppage!\n");
	if ((r = sys_page_map(0, addr, 0, addr, perm)) < 0)
            panic("sys_page_map error in duppage!\n");
    // LAB 4: Your code here.
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
    set_pgfault_handler(pgfault);
    int r, childid;
    childid = sys_exofork();
    if (childid <0)
            panic("exofork error in fork()!\n");
    else if (childid ==0)
    {
            thisenv = &envs[ENVX(sys_getenvid())];
            return 0;
    } else
    {
        int addr;
        for (addr = UTEXT; addr<UXSTACKTOP-PGSIZE; addr+=PGSIZE)
        {
                int pn = PGNUM(addr);
                if (((uvpd[PDX(addr)] & PTE_P) >0) &&
                    ((uvpt[pn] & PTE_P) >0) &&
                    ((uvpt[pn] & PTE_U) > 0))
                        duppage(childid, pn);
        }
        extern void _pgfault_upcall();
        sys_page_alloc(childid, (void*) (UXSTACKTOP - PGSIZE),  PTE_U|PTE_W|PTE_P);
        sys_env_set_pgfault_upcall(childid, _pgfault_upcall);
        sys_env_set_status(childid, ENV_RUNNABLE);
        return childid;
    }
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
