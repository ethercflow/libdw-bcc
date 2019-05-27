#include "utility.h"
#include <stdlib.h>
#include <assert.h>

bool singlethreaded = true;

void *xmalloc(size_t size)
{
	void *ret = malloc(size);
	assert(ret);
	return ret;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ret = calloc(nmemb, size);
	assert(ret);
	return ret;
}
