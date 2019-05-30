#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

static char buf[4096];

static void __attribute__ ((noinline)) *mymalloc(void)
{
	return buf;
}

static void __attribute__ ((noinline)) boo(void)
{
	void *p = mymalloc();
}

static void __attribute__ ((noinline)) bar(void)
{
	boo();
}

static void __attribute__ ((noinline)) foo(void)
{
	bar();
}

int main(void)
{
	while (1) {
		foo();
	}
}
