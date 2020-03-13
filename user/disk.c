#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	uint8_t b[512];
	sys_disk_read(1, b, 0, 1);
	// Check that we see environments running on different CPUs
	cprintf("[%08x] disk read %08x\n", *(uint32_t*)b);
}

