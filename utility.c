#include "utility.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

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

static int vscnprintf(char *buf, size_t size, const char *fmt, va_list args) {
	int i;

	i = vsnprintf(buf, size, fmt, args);

	if (i < size)
		return i;
	if (size != 0)
		return size - 1;
	return 0;
}

static int scnprintf(char *buf, size_t size, const char *fmt, ...) {
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

int path__join(char *bf, size_t size, const char *path1, const char *path2)
{
	return scnprintf(bf, size, "%s%s%s", path1, path1[0] ? "/" : "", path2);
}

bool is_regular_file(const char *file)
{
	struct stat st;

	if (stat(file, &st))
		return false;

	return S_ISREG(st.st_mode);
}
