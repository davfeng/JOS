// Simple PIO-based (non-DMA) IDE driver code.

#include "inc/x86.h"
#include "inc/assert.h"
#include "kern/spinlock.h"
#include "kern/fs.h"
#include "kern/buf.h"
#include "kern/env.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

#define IDE_DATA_ERR_PORT      0x1F0
#define IDE_SECT_COUNT_PORT    0x1F2
#define IDE_SECT_NUMBER_PORT  0x1F3
#define IDE_SECT_CYL_LSB       0x1F4
#define IDE_SECT_CYL_MSB       0x1F5
#define IDE_DRIVE_SEL_PORT     0x1F6
#define IDE_STATUS_CMD_PORT    0x1F7
// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock = {
#ifdef DEBUG_SPINLOCK
	.name = "ide"
#endif
};
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf*);

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
	int r;

	while(((r = inb(IDE_STATUS_CMD_PORT)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
		;
	if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
		return -1;
	return 0;
}

void
ideinit(void)
{
	int i;

	idewait(0);

	// Check if disk 1 is present
	outb(IDE_DRIVE_SEL_PORT, 0xe0 | (1<<4));
	for (i=0; i<1000; i++) {
		if (inb(IDE_STATUS_CMD_PORT) != 0) {
			havedisk1 = 1;
			break;
		}
	}
	if (havedisk1)
		cprintf("disk1 found\n");
	// Switch back to disk 0.
	outb(IDE_DRIVE_SEL_PORT, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.
static void
idestart(struct buf *b)
{
	if (b == 0)
		panic("idestart");

	if (b->blockno >= FSSIZE)
		panic("incorrect blockno");

	int sector_per_block =  BSIZE/SECTOR_SIZE;
	int sector = b->blockno * sector_per_block;
	int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
	int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

	if (sector_per_block > 7) 
		panic("idestart");
	
	cprintf("cpu%d, env %08x reads sector %d in %s\n", thiscpu->cpu_id, curenv->env_id, sector, __func__);
	idewait(0);
	outb(0x3f6, 0);  // generate interrupt
	outb(IDE_SECT_COUNT_PORT, sector_per_block);  // number of sectors
	outb(IDE_SECT_NUMBER_PORT, sector & 0xff);
	outb(IDE_SECT_CYL_LSB, (sector >> 8) & 0xff);
	outb(IDE_SECT_CYL_MSB, (sector >> 16) & 0xff);
	outb(IDE_DRIVE_SEL_PORT, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));

	if (b->flags & B_DIRTY) {
		outb(IDE_STATUS_CMD_PORT, write_cmd);
		outsl(IDE_DATA_ERR_PORT, b->data, BSIZE/4);
	} else {
		outb(IDE_STATUS_CMD_PORT, read_cmd);
	}
}

// Interrupt handler.
void
ideintr(void)
{
	struct buf *b;

	// First queued buffer is the active request.
	spin_lock(&idelock);

	if ((b = idequeue) == 0) {
		spin_unlock(&idelock);
		return;
	}

	idequeue = b->qnext;

	// Read data if needed.
	if (!(b->flags & B_DIRTY) && idewait(1) >= 0)
		insl(0x1f0, b->data, BSIZE/4);

	// Wake process waiting for this buf.
	b->flags |= B_VALID;
	b->flags &= ~B_DIRTY;
	cprintf("curenv=%08x, wakeup process waiting on %08x\n", curenv->env_id, b);
	wakeup(b);

	// Start disk on next buf in queue.
	if (idequeue != 0)
 		idestart(idequeue);

	spin_unlock(&idelock);
}

// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
void
iderw(struct buf *b)
{
	struct buf **pp;

	spin_lock(&idelock);
	cprintf("process %08x reads block %d now\n", curenv->env_id, b->blockno);
	if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
		panic("iderw: nothing to do");
	if(b->dev != 0 && !havedisk1)
		panic("iderw: ide disk 1 not present");


	// Append b to idequeue.
	b->qnext = 0;
	for (pp=&idequeue; *pp; pp=&(*pp)->qnext)
    	;
	*pp = b;

	// Start disk if necessary.
	if (idequeue == b)
		idestart(b);

	// Wait for request to finish.
	while ((b->flags & (B_VALID|B_DIRTY)) != B_VALID) {
		sleep(b, &idelock);
	}
}
