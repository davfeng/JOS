#include <inc/x86.h>
#include <kern/pmap.h>
#include <kern/e1000.h>

// LAB 6: Your driver code here
int e1000_enable(struct pci_func *f)
{
	volatile char* p;
	pci_func_enable(f);
	p = mmio_map_region(f->reg_base[0], f->reg_size[0]);
	cprintf("status = %08x\n", *(uint32_t*)(p+E1000_STATUS));
	return 0;
}
