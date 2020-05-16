#include <assert.h>
volatile int a, b;

void foo()
{
	a = 1;
	b = 1;
}

void bar()
{
	while (b == 0)
		;
	assert (a==1);
	a = 0;
	b = 0;
}

int main()
{
	while (1) {
		foo();
		bar();
	}
}
