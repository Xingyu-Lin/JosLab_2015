
#include "fs.h"
#include "inc/lib.h"
#include "lib/syscall.c"
#define MAXFILECACHE (1 * 10* PGSIZE)    //1M, max size of memory for disk mapping
#define MAXBLK (DISKSIZE/BLKSIZE)
// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

static struct blk_list
{
        uint32_t count;
        uint32_t tstamp;
        bool valid;
} plist[MAXBLK];

static void update_blk_count()
{
        int i;
        void* va = (void*)DISKMAP;
        for (i=0; i<MAXBLK; ++i)
        {
                if (plist[i].valid)
                {
                        if (uvpt[PGNUM(va)] & PTE_A) plist[i].count++;
                }
                va += BLKSIZE;
        }
}

static uint32_t timestamp = 0;
static void update_time_stamp()
{
        int i;
        void* va = (void*)DISKMAP;
        for (i=0; i<MAXBLK; ++i)
        {
                if (plist[i].valid)
                {
                        if (uvpt[PGNUM(va)] & PTE_A) plist[i].tstamp = timestamp;
                }
                va += BLKSIZE;
        }
}

static void print_block_list()
{
       cprintf("\n");
       cprintf("==============block usage list==============\n");
       int i;
       for (i=0; i<MAXBLK; ++i)
               if (plist[i].valid)
                    cprintf("+block at %x, used %d times, last used at time %d\n", diskaddr(i), plist[i].count, plist[i].tstamp);
       cprintf("++++++++++++++++++end list++++++++++++++++++\n");
}

static uint32_t curr_disk_size = 0;
// Fault any disk block that is read in to memory by
// loading it from disk.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	//
	// LAB 5: you code here:
    addr = ROUNDDOWN(addr, PGSIZE);
    update_blk_count();
    update_time_stamp();
    print_block_list();
    if (curr_disk_size == MAXFILECACHE) //no memory for disk mapping, have to evict
    {
            uint32_t i, min_blk = 0, min_count = 0xffffffff;
            for (i=0; i<MAXBLK; ++i)
            if (plist[i].valid && plist[i].tstamp<min_count && i!=1) 
            //use LRU policy to evict a block
            {
                    assert(i!=blockno);
                    min_blk = i;
                    min_count = plist[i].tstamp;
            }
            cprintf("evict block at %x, used %d times, last used at time %x, load block at %x\n",
                            diskaddr(min_blk), plist[min_blk].count, plist[min_blk].tstamp, addr);
            flush_block(diskaddr(min_blk));
            curr_disk_size -=PGSIZE;
            plist[min_blk].valid =0;
            sys_page_unmap(0, diskaddr(min_blk));
    } else 
    {
            cprintf("load block at %x, no eviction\n", addr);
    }
    cprintf("total miss: %d\n", timestamp);
    curr_disk_size += PGSIZE;
    if ((r = sys_page_alloc(0, addr, PTE_SYSCALL)) <0) 
            panic("in bc_pgfault, sys_page_alloc: %e", r);
    if ((r = ide_read(blockno * BLKSECTS, addr, BLKSECTS)) < 0)
            panic("in bc_pgfault, ideread: %e", r);
	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);
    plist[blockno].valid = 1;
    plist[blockno].count = 0;
    plist[blockno].tstamp = ++timestamp;
	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
    sys_ptea_flush();
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);
    
	// LAB 5: Your code here.
    int r;
    addr = ROUNDDOWN(addr, PGSIZE);
    if (!va_is_mapped(addr) || !va_is_dirty(addr)) return;
    if ((r = ide_write(blockno * BLKSECTS, addr, BLKSECTS )) < 0)
            panic("in flush_block: %e", r);
    if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) <0)
            panic("in flush_block: %e", r);
    return;
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	check_bc();

	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);
}

