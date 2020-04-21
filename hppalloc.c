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

#define ROUND_TO_MULTIPLE(x, n)  ((((x) + (n) - 1) / (n)) * (n))
#define ROUND_DOWN_MULTIPLE(x, n) (((x) / (n)) * (n))

#ifndef MAP_HUGE_1GB
#define MAP_HUGE_SHIFT 26
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

#define MIN_HUGE_PAGE_SIZE (1U << 21)

#define MAP_BASEFLAGS (MAP_PRIVATE | MAP_ANONYMOUS)
#define MMAP_PROT (PROT_READ | PROT_WRITE)

static struct page_type {
	size_t size;
	int map_flags;
	char *name;
} page_types[] = {
	{ 1U << 30, MAP_BASEFLAGS | MAP_HUGETLB | MAP_HUGE_1GB, "1G" },
	{ 1U << 21, MAP_BASEFLAGS | MAP_HUGETLB | MAP_HUGE_2MB, "2M" }
};

#define ENV_BASEPATH "HPPA_BASEPATH"
#define ENV_POOLSIZE "HPPA_POOLSIZE"
#define ENV_MINSIZE  "HPPA_MINSIZE"

typedef struct heap_block {
	size_t size;
	struct heap_block* prev;
} heap_block_t;

/* align to cache line */
#define BLOCK_SHIFT         6
#define BLOCK_ALIGN         (1U << BLOCK_SHIFT)
#define BLOCK_MASK_SIZE     (~(BLOCK_ALIGN - 1))
#define BLOCK_MASK_USED     (1U)

#define BLOCK_USED(b)       (b->size & BLOCK_MASK_USED)
#define NEXT_BLOCK(b)       (heap_block_t*) (((char*) (b)) + ((b)->size & BLOCK_MASK_SIZE))


static struct heap {
	char *mapping;
	size_t pool_size;
	heap_block_t *next;
} mapped_heap = { NULL, 1U << 30 /* 1GB by default/for testing */, NULL };

#define ADDR_IN_HEAP(x, h)       ((char*) (x) < (h).mapping + (h).pool_size && (char*) (x) >= (h).mapping)
#define BLOCK_IN_HEAP(b_ptr, h)  ADDR_IN_HEAP(b_ptr, h)
#define BLOCK_FROM_ADDR(x)       (heap_block_t*) ((char*) (x) - ROUND_TO_MULTIPLE(sizeof(heap_block_t), BLOCK_ALIGN))


/* Create single file under base_path which handles all mappings via buddy allocator */
/* That file might be placed on a NVDIMM namespace. */
static bool hpp_init_file_backed_mappings(const char* base_path)
{
	const size_t fname_size = 256;
	char fname[fname_size];

	strlcpy(fname, base_path, fname_size - 1);
	strlcat(fname, "/", fname_size - 1);

	char s[8] = { 0 };
	snprintf(s, 7, "%06d", getpid());
	strlcat(fname, s, fname_size - 1);

	int fd = open(fname, O_CREAT | O_RDWR, 0666);
	if (fd == -1) {
		debug_print("Error opening mapfile %s: %s (%d)\n", fname, strerror(errno), errno);
		return false;
	}

	if (fallocate(fd, 0, 0, mapped_heap.pool_size) == -1) {
		debug_print("Cannot reserve space in mapfile %s: %s (%d)\n", fname, strerror(errno), errno);
		return false;
	}

	/* map the whole file into memory and beg for huge pages */
	mapped_heap.mapping = mmap(NULL, mapped_heap.pool_size, MMAP_PROT, MAP_SHARED, fd, 0);
	if (mapped_heap.mapping == MAP_FAILED) {
		debug_print("mmap of mapfile failed: %s (%d)", strerror(errno), errno);
		close(fd);
		return false;
	}

	if (madvise(mapped_heap.mapping, mapped_heap.pool_size, MADV_WILLNEED | MADV_HUGEPAGE | MADV_DONTFORK)) {
		debug_print("madvise failed: %s (%d), but going ahead", strerror(errno), errno);
	}

 	/* according to mmap(2) it is safe to close the file after mmap */
	close(fd);

	return true;
}


static bool hpp_init_anon_mappings()
{
	for (size_t i = 0; i < sizeof(page_types) / sizeof(page_types[0]); i++) {
		struct page_type pt = page_types[i];
		size_t mmap_size = ROUND_DOWN_MULTIPLE(mapped_heap.pool_size, pt.size);
		mapped_heap.mapping = mmap(NULL, mmap_size, MMAP_PROT, pt.map_flags, -1, 0);
		if (mapped_heap.mapping != MAP_FAILED) {
			mapped_heap.pool_size = mmap_size;
			return true;
		} else {
			debug_print("mmap failed for %s: %s (%d)\n", pt.name, strerror(errno), errno);
		}
	}

	return false;
}

static bool is_initialized = false;

static void hpp_init(void)
{
	if (is_initialized) {
		return;
	}

	if (getenv(ENV_POOLSIZE)) {
		size_t size = strtol(getenv(ENV_POOLSIZE), NULL, 10);
		if (size > mapped_heap.pool_size) {
			mapped_heap.pool_size = ROUND_DOWN_MULTIPLE(size, MIN_HUGE_PAGE_SIZE);
		}
	}

	if (getenv(ENV_BASEPATH)) {
		if (!hpp_init_file_backed_mappings(getenv(ENV_BASEPATH))) {
			debug_print("Unable to init file based mapping. Exiting.\n");
			exit(99);
			/* just in case */
			return;
		}
	} else {
		if (!hpp_init_anon_mappings()) {
			debug_print("Unable to init via hugepage mmap. Please, check available hugepages! Exiting.\n");
			exit(99);
			return;
		}
	}

	/* init heap */
	mapped_heap.next = (heap_block_t*) mapped_heap.mapping;
	mapped_heap.next->size = mapped_heap.pool_size;
	mapped_heap.next->prev = NULL;

	is_initialized = true;

	debug_print("initialized\n");
}

/* actual allocation/deallocation */

static void *hpp_block_alloc(size_t size)
{
	/* waste at least one cacheline for meta data to ensure proper alignment */
	size_t req_size = ROUND_TO_MULTIPLE(size, BLOCK_ALIGN) +
		ROUND_TO_MULTIPLE(sizeof(heap_block_t), BLOCK_ALIGN);

	/* search for free and large enough block */
	heap_block_t* block = mapped_heap.next;
	while (BLOCK_IN_HEAP(block, mapped_heap) && BLOCK_USED(block) && block->size < req_size) {
		block = NEXT_BLOCK(block);
	}

	if (!BLOCK_IN_HEAP(block, mapped_heap)) {
		return NULL;
	}

	size_t remain = block->size - req_size;
	/* Do not care about small remaining blocks, just waste them.
	 * We won't allocate such small chunks anyways */
	if (remain > MIN_HUGE_PAGE_SIZE) {
		heap_block_t *old_next = NEXT_BLOCK(block);

		block->size = req_size;

		heap_block_t *new_block = NEXT_BLOCK(block);
		new_block->size = remain;
		new_block->prev = block;

		/* The splitted block is a free block. The next search starts there,
		 * if the current search started at the current block. */
		if (mapped_heap.next == block) {
			mapped_heap.next = new_block;
		}

		if (BLOCK_IN_HEAP(old_next, mapped_heap)) {
			old_next->prev = new_block;
		}
	}

	block->size |= (~0 & BLOCK_MASK_USED);

	return (char*) block + ROUND_TO_MULTIPLE(sizeof(heap_block_t), BLOCK_ALIGN);
}

static void hpp_block_free(heap_block_t *block)
{
	/* reset flags */
	block->size = block->size & BLOCK_MASK_SIZE;
	if (block < mapped_heap.next) {
		mapped_heap.next = block;
	}

	/* try to merge with next */
	heap_block_t *next = NEXT_BLOCK(block);
	if (BLOCK_IN_HEAP(next, mapped_heap) && !BLOCK_USED(next)) {
		block->size += next->size;
	}

	/* try to merge with prev */
	if (block->prev && !BLOCK_USED(block->prev)) {
		block->prev->size += block->size;
	}
}

void* hpp_alloc(size_t n, size_t elem_size)
{
	if (!is_initialized) {
		hpp_init();
	}

	/* no overflow checks as in calloc here */
	size_t alloc_size = n * elem_size;
	void* retval = hpp_block_alloc(alloc_size);

	if (retval == NULL) {
		debug_print("failed to alloc %zu * %zu Bytes = %zu Bytes\n", n, elem_size, alloc_size);
	} else {
		debug_print("allocated %zu * %zu Bytes => %zu Bytes @ %p\n", n, elem_size, alloc_size, retval);
	}

	return retval;
}


void hpp_free(void *ptr)
{
	if (!ADDR_IN_HEAP(ptr, mapped_heap)) {
		return;
	}

	heap_block_t* block = BLOCK_FROM_ADDR(ptr);
	debug_print("free block of %zu Bytes at %p\n", block->size & BLOCK_MASK_SIZE, block);
	hpp_block_free(block);
}
