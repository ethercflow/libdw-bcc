#include <stdlib.h>
#include <assert.h>

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
