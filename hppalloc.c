#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef DEBUG
#define debug_print(...) do {\
	fprintf(stdout, "[hpp] ");\
	fprintf(stdout, ##__VA_ARGS__);\
} while (false)
#else
#define debug_print(...)
#endif

#define ALLOCATION_GROWTH 32

typedef struct {
	void* addr;
	size_t size;
} allocation_t;

static struct {
	allocation_t *items;
	size_t count;
	size_t capacity;
} allocations = { NULL, 0, 0 };


static bool hpp_grow_allocations()
{
	size_t nc = allocations.capacity + ALLOCATION_GROWTH;

	/* reallocarray is not available on CentOS 7 */
	void* ni = realloc(allocations.items,  nc * sizeof(*allocations.items));
	if (!ni) {
		debug_print("fatal: could not increase intenal memory\n");
		return false;
	}

	allocations.items = ni;
	allocations.capacity = nc;

	return true;
}


static allocation_t* hpp_reserve_allocation(void)
{
	if (allocations.count == allocations.capacity) {
		hpp_grow_allocations();
	}
	return &allocations.items[allocations.count++];
}


static allocation_t* hpp_get_allocation(const void* addr)
{
	for (size_t i = 0; i < allocations.count; i++) {
		if (allocations.items[i].addr == addr) {
			return &allocations.items[i];
		}
	}

	return NULL;
}


static bool is_initialized = false;

static void hpp_init(void)
{
	if (is_initialized) {
		return;
	}

	is_initialized = true;
	hpp_grow_allocations();

	debug_print("initialized\n");
}

/* actual allocation/deallocation */

void* hpp_alloc(size_t n, size_t elem_size)
{
	if (!is_initialized) {
		hpp_init();
	}

	/* no overflow checks as in calloc here */
	allocation_t *a = hpp_reserve_allocation();
	if (!a) {
		return NULL;
	}

	a->size = n * elem_size;
	a->addr = malloc(a->size);
	debug_print("allocated %zu * %zu Bytes = %zu Bytes @ %p\n", n, elem_size, a->size, a->addr);

	return a->addr;
}


void hpp_free(void *ptr)
{
	allocation_t* a = hpp_get_allocation(ptr);
	if (!a) {
		debug_print("unknown allocation at %p\n", a->addr);
		return;
	}

	if (a->size == SIZE_MAX) {
		debug_print("double free at %p detected\n", ptr);
		return;
	}

	debug_print("free %zu byte at %p\n", a->size, a->addr);
	free(ptr);

	a->addr = NULL;
	a->size = SIZE_MAX;
}
