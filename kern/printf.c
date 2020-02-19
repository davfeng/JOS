// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <kern/spinlock.h>


struct spinlock print_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "print_lock"
#endif
};
static void
putch(int ch, int *cnt)
{
	cputchar(ch);
	*cnt++;
}

int
vcprintf(const char *fmt, va_list ap)
{
	int cnt = 0;

	vprintfmt((void*)putch, &cnt, fmt, ap);
	return cnt;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	spin_lock(&print_lock);
	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	spin_unlock(&print_lock);
	return cnt;
}

