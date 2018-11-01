// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/x86.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

// Assembly language pgfault entrypoint defined in lib/pfentry.S.
extern void _pgfault_upcall(void);
void (*_pgfault_handler)(struct UTrapframe *utf);
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
	uintptr_t *p = (uintptr_t*)UVPT;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	uint32_t pte = (uint32_t)addr >> 12;
	uint32_t bits = *(p + pte);
	if((err & FEC_WR) == 0 || (bits & PTE_COW) == 0 || (bits & PTE_P) == 0){
		panic("failed in COW handler: not a COW page, err=0x%x, bits=0x%x\n", err, bits);
	}  
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	// alloc a page at PFTEMP, U/R/W
	if(sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W) < 0){
		panic("failed in COW handler: page alloc failed\n");
	}
	
	// copy the old data to this newly allocated page
	memcpy(PFTEMP, (void*)(pte*PGSIZE), PGSIZE);
	// map the addr to this newly allocated page
	if(sys_page_map(0, PFTEMP, 0, (void*)(pte*PGSIZE), PTE_U | PTE_P | PTE_W) < 0){
		panic("failed in COW handler: map page failed\n");
	}
	// unmap the temp address
	if(sys_page_unmap(0, PFTEMP) < 0){
		panic("failed in COW handler: unmap tmp page failed\n");
	}
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

	// LAB 4: Your code here.
	if(sys_page_map(0, (void*)(pn * PGSIZE), 
					envid, (void*)(pn * PGSIZE), 
					PTE_P | PTE_U | PTE_COW) < 0){
		panic("map failed in %s\n", __func__);
	}
	
	//remap the va, but change the access bits
	if(sys_page_map(0, (void*)(pn * PGSIZE), 
					0, (void*)(pn * PGSIZE), 
					PTE_P | PTE_U | PTE_COW) < 0){
		panic("map failed in %s\n", __func__);
	}
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
	envid_t childenv;

	uintptr_t *p = (uintptr_t*)UVPT;
	uintptr_t *q = (uintptr_t*)UVPD;
	
	uint32_t pde, pte, addr;

	set_pgfault_handler(pgfault);
	childenv = sys_exofork();

	if(childenv < 0){
		panic("fork failed\n");
	}

	if(childenv > 0){
		sys_env_set_pgfault_upcall(childenv, (void*)_pgfault_upcall);

		for(addr = 0; addr < USTACKTOP; addr += PGSIZE){
			//if pde is not existent, continue next
			pde = PDX(addr);
			if(!((*(q + pde)) & PTE_P))
				continue;
			//check pte
			pte = addr >> 12;
			uint32_t bits = *(p + pte);

			// RO page, just map(share)
			if((bits & PTE_P) && (bits & PTE_U) && (bits & PTE_W) == 0 && (bits & PTE_COW) == 0){ 
				sys_page_map(0, (void*)addr, childenv, (void*)addr, PTE_P | PTE_U);
			}

			// R/W page, make it COW
			if((bits & PTE_P) && (bits & PTE_U ) && ((bits & PTE_W) || (bits & PTE_COW))){ 
				duppage(childenv, addr >> 12);
			}
		}
		if(sys_page_alloc(childenv, (void*) (UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W) < 0){
			panic("allocating except handler stack failed in %s", __func__);
		}
		sys_env_set_status(childenv, ENV_RUNNABLE);
		return childenv;
	}
	thisenv = &envs[ENVX(sys_getenvid())];
	return 0;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
