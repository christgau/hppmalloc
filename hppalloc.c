#define _GNU_SOURCE

#include "hppalloc.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <bsd/string.h>

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

#define VM_HUGE_PAGES_BASEPATH "/sys/kernel/mm/hugepages/hugepages-"

#ifndef MAP_HUGE_1GB
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

#define HPP_MAP_BASEFLAGS (MAP_PRIVATE | MAP_ANONYMOUS)

static struct page_type {
	size_t size;
	size_t n_avail;
	size_t offset;
	int map_fd;
	int map_flags;
	char *map_fn;
	char *base_path;
} page_types[] = {
	{ 1U << 30, 0, 0, -1, HPP_MAP_BASEFLAGS | MAP_HUGETLB | MAP_HUGE_1GB, "1G", VM_HUGE_PAGES_BASEPATH "1048576kB" },
	{ 1U << 21, 0, 0, -1, HPP_MAP_BASEFLAGS | MAP_HUGETLB | MAP_HUGE_2MB, "2M", VM_HUGE_PAGES_BASEPATH "2048kB" },
	{ 1U << 12, 0, 0, -1, HPP_MAP_BASEFLAGS, "4k", NULL }
};

static bool is_initialized = false;

static void hpp_init(void)
{
	if (is_initialized) {
		return;
	}

	/* if a basepath is given, the allocations are backed by files which are created first */
	for (size_t i = 0; i < sizeof(page_types) / sizeof(page_types[0]); i++) {
		if (!page_types[i].base_path) {
			/* one GB by default */
			page_types[i].n_avail = (1U << 30) / page_types[i].size;
		}

		char fname[256] = { 0 };

		if (page_types[i].base_path) {
			strlcat(fname, page_types[i].base_path, sizeof(fname) - 1);
			strlcat(fname, "/nr_hugepages", sizeof(fname) - 1);

			FILE* f = fopen(fname, "r");
			if (f != NULL) {
				fscanf(f, "%zu", &page_types[i].n_avail);
				fclose(f);
			}
		}

		if (page_types[i].n_avail == 0) {
			continue;
		}

		if (getenv("HPPA_BASEPATH")) {
			memset(fname, 0, sizeof(fname));
			strlcpy(fname, getenv("HPPA_BASEPATH"), sizeof(fname) - 1);
			strlcat(fname, "/", sizeof(fname) - 1);

			char s[16] = { 0 };
			snprintf(s, 15, "%d-%s", getpid(), page_types[i].map_fn);
			strlcat(fname, s, sizeof(fname) - 1);

			int fd = open(fname, O_CREAT | O_RDWR, 0666);
			if (fd == -1) {
					debug_print("error opening %s for pagesize %zu: %s (%d)\n", fname, page_types[i].size, strerror(errno), errno);
					continue;
			}

			if (fallocate(fd, 0, 0, page_types[i].size * page_types[i].n_avail) == -1) {
					debug_print("cannot reserve space for pagesize %s: %s (%d)\n", fname, strerror(errno), errno);
					continue;
			}

			page_types[i].map_flags = (page_types[i].map_flags & ~(MAP_ANONYMOUS | MAP_PRIVATE)) | MAP_SHARED;
			page_types[i].map_fd = fd;
		}
	}

	is_initialized = true;
	hpp_grow_allocations();

	debug_print("initialized\n");
}

/* actual allocation/deallocation */

#define MMAP_PROT (PROT_READ | PROT_WRITE)
#define ROUND_TO_MULTIPLE(x, n)  ((((x) + (n) - 1) / (n)) * (n))

/* try to mmap the requested memory, but only if size is larger than the page size */
static void try_mmap(allocation_t *a, struct page_type *page_type, char *buf)
{
	if (a->size < page_type->size || page_type->n_avail == 0) {
		return;
	}

	size_t mmap_size = ROUND_TO_MULTIPLE(a->size, page_type->size);
	a->addr = mmap(NULL, mmap_size, MMAP_PROT, page_type->map_flags, page_type->map_fd, page_type->offset);
	if (a->addr != MAP_FAILED) {
		a->size = mmap_size;
		a->flags |= ALLOC_MMAPPED;
		if (page_type->map_fd != -1) {
			page_type->offset += mmap_size;
		}
		strncpy(buf, page_type->map_fn, 2);
	} else {
		debug_print("alloc failed for %s: %s (%d)\n", page_type->map_fn, strerror(errno), errno);
		a->addr = NULL;
	}
}

void* hpp_alloc(size_t n, size_t elem_size, int flags)
{
	char buf[3] = { 0 };

	if (!is_initialized) {
		hpp_init();
	}

	/* no overflow checks as in calloc here */
	allocation_t *a = hpp_reserve_allocation();
	if (!a) {
		return NULL;
	}

	a->size = n * elem_size;
	for (size_t i = 0; !a->addr && i < sizeof(page_types) / sizeof(page_types[0]); i++) {
		try_mmap(a, &page_types[i], buf);
	}

	/* last resort */
	if ((a->addr == NULL) && !(flags & HPPA_NOMALLOC)) {
		a->addr = malloc(a->size);
		strncpy(buf, "m", sizeof(buf));
	}

	if (a->addr == NULL) {
		debug_print("failed to alloc %zu * %zu Bytes = %zu Bytes\n", n, elem_size, a->size);
		allocations.count--;
	} else {
		debug_print("allocated %zu * %zu Bytes => %zu Bytes @ %p via %s\n", n, elem_size, a->size, a->addr, buf);
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
