#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef DEBUG
#define debug_print(...) do {\
	fprintf(stdout, "[hpp] ");\
	fprintf(stdout, ##__VA_ARGS__);\
} while (false)
#else
#define debug_print(...)
#endif

#define ALLOCATION_GROWTH 32

#define ALLOC_MMAPPED (1 << 1)

typedef struct {
	void* addr;
	size_t size;
	int flags; /* flags could be stored in addr (see USER_SHIFT) */
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

	size_t i = allocations.count++;
	allocations.items[i].addr = NULL;
	allocations.items[i].size = 0;

	return &allocations.items[i];
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
static size_t base_page_size = 0;

static void hpp_init(void)
{
	if (is_initialized) {
		return;
	}

	is_initialized = true;
	base_page_size = sysconf(_SC_PAGE_SIZE);
	hpp_grow_allocations();

	debug_print("initialized\n");
}

/* actual allocation/deallocation */

#ifndef MAP_HUGE_1GB
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

#define MMAP_PROT (PROT_READ | PROT_WRITE)
#define MMAP_BASE_FLAGS  (MAP_ANONYMOUS)

#define ROUND_TO_MULTIPLE(x, n)  ((((x) + (n) - 1) / (n)) * (n))

/* try to mmap the requested memory, but only if size is larger than the page size */
static void try_mmap(allocation_t *a, int flags, size_t page_size, size_t *offset,
	char* buf, char* descr)
{
	if (a->addr || a->size < page_size) {
		return;
	}

	int fd = -1;
	size_t mmap_size = ROUND_TO_MULTIPLE(a->size, page_size);
	a->addr = mmap(NULL, mmap_size, MMAP_PROT, flags, fd, *offset);
	if (a->addr != MAP_FAILED) {
		a->size = mmap_size;
		a->flags |= ALLOC_MMAPPED;
		if (fd != -1) {
			*offset += mmap_size;
		}
		strncpy(buf, descr, 2);
	} else {
		debug_print("alloc failed for %s: %s (%d)\n", descr, strerror(errno), errno);
		a->addr = NULL;
	}
}

void* hpp_alloc(size_t n, size_t elem_size)
{
	static size_t alloc_offset = 0;
	int mmap_flags = MMAP_BASE_FLAGS;
	char buf[3] = { 0 };

	if (!is_initialized) {
		hpp_init();
	}

	/* no overflow checks as in calloc here */
	allocation_t *a = hpp_reserve_allocation();
	if (!a) {
		return NULL;
	}

	mmap_flags |= MAP_PRIVATE;

	a->size = n * elem_size;
	/* try 1GB huge page, 2MB huge pages, and regular pages size, */
	try_mmap(a, mmap_flags | MAP_HUGETLB | MAP_HUGE_1GB, 1024 * 1024 * 1024, &alloc_offset, buf, "1G");
	try_mmap(a, mmap_flags | MAP_HUGETLB | MAP_HUGE_2MB, 2 * 1024 * 1024, &alloc_offset, buf, "2M");
	try_mmap(a, mmap_flags, base_page_size, &alloc_offset, buf, "4k");

	/* last resort */
	if (a->addr == NULL) {
		a->addr = malloc(a->size);
		strncpy(buf, "m", sizeof(buf));
	}

	if (a->addr == NULL) {
		debug_print("failed to alloc %zu * %zu Bytes = %zu Bytes\n", n, elem_size, a->size);
		allocations.count--;
	} else {
		debug_print("allocated %zu * %zu Bytes => %zu Bytes @ %p (%s)\n", n, elem_size, a->size, a->addr, buf);
	}

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

	if (a->flags & ALLOC_MMAPPED) {
		munmap(a->addr, a->size);
		debug_print("munmap %zu Bytes at %p\n", a->size, a->addr);
	} else {
		debug_print("free %zu Bytes at %p\n", a->size, a->addr);
		free(ptr);
	}

	a->addr = NULL;
	a->size = SIZE_MAX;
}
