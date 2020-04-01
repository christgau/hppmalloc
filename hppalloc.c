#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>

#ifdef DEBUG
#define debug_print(...) do {\
	fprintf(stdout, "[hpp] ");\
	fprintf(stdout, ##__VA_ARGS__);\
} while (false)
#else
#define debug_print(...)
#endif

static bool initialized = false;

static void hpp_init(void)
{
	if (initialized) {
		return;
	}

	initialized = true;
	debug_print("initialized\n");
}

void* hpp_alloc(size_t n, size_t elem_size)
{
	if (!initialized) {
		hpp_init();
	}

	/* no overflow checks as in calloc here */
	size_t alloc_size = n * elem_size;
	void* retval = malloc(alloc_size);
	debug_print("allocated %zu * %zu Bytes = %zu Bytes @ %p\n", n, elem_size, alloc_size, retval);

	return retval;
}

void hpp_free(void *ptr)
{
	debug_print("free %p\n", ptr);
	free(ptr);
}
