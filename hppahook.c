#define _GNU_SOURCE
#include "hppalloc.h"

#include <stdbool.h>
#include <stdlib.h>

#include <dlfcn.h>
#include <assert.h>

#include <unistd.h>

/* Function pointers to real libc function. Used in allocator to 
 * directly call into libc to avoid recursion to the hook */
void* (*hpp_libc_malloc)(size_t n) = NULL;
void (*hpp_libc_free)(void *ptr) = NULL;

#define SCRATCH_POOL_SIZE (1U << 20)
char pool[SCRATCH_POOL_SIZE];
char* next_from_pool = pool;

void* malloc(size_t n)
{
	static bool in_init = false;

	if (!hpp_libc_malloc) {
		if (__atomic_exchange_n(&in_init, true, __ATOMIC_ACQUIRE) == false) {
			/* set safe default mode for  */
			hpp_libc_malloc = dlsym(RTLD_NEXT, "malloc");
			hpp_libc_free = dlsym(RTLD_NEXT, "free");
			if (!hpp_libc_malloc) {
				/* uhoh. */
				exit(7);
				return NULL;
			}
			in_init = false;
		} else {
			char* retval = __atomic_add_fetch(&next_from_pool, n, __ATOMIC_RELEASE);
			assert(retval < pool + SCRATCH_POOL_SIZE);
			return (retval < pool + SCRATCH_POOL_SIZE ? retval : NULL);
		}
	}

	return hpp_alloc(1, n);
}

void free(void *ptr)
{
	if ((char*) ptr >= pool && (char*) ptr < next_from_pool) {
		return;
	}

	hpp_free(ptr);
}
