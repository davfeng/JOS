// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	char buf[512];
	for (int i = 0; i < 100;i++)
	{
		cprintf("i am environment %08x on cpu %d\n", thisenv->env_id, thisenv->env_cpunum);
		cprintf("hello, world\n");
		//sys_disk_read(0, buf, i, 1);
		sys_sleep(5000);
	}
}
