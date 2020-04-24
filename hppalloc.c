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

#define HPPA_INT_OPT_BASE        (HPPA_AS_MAX + 1)
#define HPPA_INT_OPT_PRINT_HEAP  (1 << (HPPA_INT_OPT_BASE + 0))

#define ROUND_TO_MULTIPLE(x, n)  ((((x) + (n) - 1) / (n)) * (n))
#define ROUND_DOWN_MULTIPLE(x, n) (((x) / (n)) * (n))

#define ENV_BASEPATH "HPPA_BASE_PATH"
#define ENV_PRINTHEAP "HPPA_PRINT_HEAP"
#define ENV_ALLOCTHRES "HPPA_ALLOC_THRESHOLD"
#define ENV_HEAPSIZE_ANON "HPPA_SIZE_ANON"
#define ENV_HEAPSIZE_NAMED "HPPA_SIZE_NAMED"
#define ENV_INITAS "HPPA_INITIAL_STRATEGY"

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

typedef struct heap_block {
	size_t size;
	struct heap_block* prev;
} heap_block_t;

/* align to cache line */
#define BLOCK_SHIFT         6
#define BLOCK_ALIGN         (1U << BLOCK_SHIFT)
#define BLOCK_MASK_SIZE     (~(BLOCK_ALIGN - 1))
#define BLOCK_MASK_USED     (1U)

#define BLOCK_USED(b)       (((b)->size & BLOCK_MASK_USED) != 0)
#define NEXT_BLOCK(b)       (heap_block_t*) (((char*) (b)) + ((b)->size & BLOCK_MASK_SIZE))

typedef struct heap {
	char *pool;
	char *name; /* just for convenience */
	size_t size;
	heap_block_t *next;
} heap_t;

static heap_t anon_heap = { NULL, "anon", 1U << 31 /* 2GB by default/for testing */, NULL };
static heap_t named_heap = { NULL, "named", 1U << 31, NULL };

static int hpp_mode = HPPA_AS_ALL;
static size_t alloc_threshold = MIN_HUGE_PAGE_SIZE;

#define ADDR_IN_HEAP(x, h)       ((char*) (x) < (h)->pool + (h)->size && (char*) (x) >= (h)->pool)
#define BLOCK_IN_HEAP(b_ptr, h)  ADDR_IN_HEAP(b_ptr, h)
#define BLOCK_FROM_ADDR(x)       (heap_block_t*) ((char*) (x) - ROUND_TO_MULTIPLE(sizeof(heap_block_t), BLOCK_ALIGN))

/* initialize remaining heap stuff, after pool and size have been set */
static void hpp_init_pooled_heap(heap_t *heap)
{
	heap->next = (heap_block_t*) heap->pool;
	heap->next->size = heap->size & BLOCK_MASK_SIZE;
	heap->next->prev = NULL;
}

static void hpp_print_heap(const heap_t *heap)
{
	if (!(hpp_mode & HPPA_INT_OPT_PRINT_HEAP)) {
		return;
	}

	heap_block_t *block = (heap_block_t*) heap->pool;

	int n_blocks = 0;
	size_t total_size = 0;

	debug_print("--- %s %s----------------------------\n", heap->name, !block ? "(empty)" : "");
	while (block && BLOCK_IN_HEAP(block, heap)) {
		debug_print("block @ %p%c used: %d, size: %zu, prev @ %p\n", block,
			(block == heap->next) ? '*' : ' ', BLOCK_USED(block),
			block->size & BLOCK_MASK_SIZE, block->prev);
		total_size += block->size & BLOCK_MASK_SIZE;
		block = NEXT_BLOCK(block);
		n_blocks++;
	}

	debug_print("total: %d blocks, %zu bytes\n", n_blocks, total_size);
}

/* Create single file under base_path which handles all mappings via buddy allocator */
/* That file might be placed on a NVDIMM namespace. */
static bool hpp_init_file_backed_mappings(heap_t* heap)
{
	if (heap->size == 0 || !getenv(ENV_BASEPATH)) {
		return false;
	}

	const size_t fname_size = 256;
	char fname[fname_size];

	strlcpy(fname, getenv(ENV_BASEPATH), fname_size - 1);
	strlcat(fname, "/", fname_size - 1);

	char s[8] = { 0 };
	snprintf(s, 7, "%06d", getpid());
	strlcat(fname, s, fname_size - 1);

	int fd = open(fname, O_CREAT | O_RDWR, 0666);
	if (fd == -1) {
		debug_print("Error opening mapfile %s: %s (%d)\n", fname, strerror(errno), errno);
		return false;
	}

	if (fallocate(fd, 0, 0, heap->size) == -1) {
		debug_print("Cannot reserve space in mapfile %s: %s (%d)\n", fname, strerror(errno), errno);
		return false;
	}

	/* map the whole file into memory and beg for huge pages */
	heap->pool = mmap(NULL, heap->size, MMAP_PROT, MAP_SHARED, fd, 0);
	if (heap->pool == MAP_FAILED) {
		heap->pool = NULL;
		debug_print("mmap of mapfile failed: %s (%d)", strerror(errno), errno);
		close(fd);
		return false;
	}

	if (madvise(heap->pool, heap->size, MADV_WILLNEED | MADV_HUGEPAGE | MADV_DONTFORK)) {
		debug_print("madvise failed: %s (%d), but going ahead", strerror(errno), errno);
	}

 	/* according to mmap(2) it is safe to close the file after mmap */
	close(fd);

	hpp_init_pooled_heap(heap);

	return true;
}

static bool hpp_init_anon_mappings(heap_t* heap)
{
	if (heap->size == 0) {
		return false;
	}

	for (size_t i = 0; i < sizeof(page_types) / sizeof(page_types[0]); i++) {
		struct page_type pt = page_types[i];
		size_t mmap_size = ROUND_DOWN_MULTIPLE(heap->size, pt.size);
		heap->pool = mmap(NULL, mmap_size, MMAP_PROT, pt.map_flags, -1, 0);
		if (heap->pool != MAP_FAILED) {
			heap->size = mmap_size;
			hpp_init_pooled_heap(heap);
			return true;
		} else {
			debug_print("mmap failed for %s: %s (%d)\n", pt.name, strerror(errno), errno);
		}
	}

	heap->pool = NULL;
	return false;
}

static bool is_initialized = false;

static size_t size_from_s(const char *s, size_t def_val)
{
	if (!s) {
		return def_val;
	}

	char *endp;
	long int v = strtol(s, &endp, 10);
	return (v >= 0 && s != endp ? (size_t) v : def_val);
}

static void hpp_init(void)
{
	if (is_initialized) {
		return;
	}

	if (getenv(ENV_PRINTHEAP)) {
		const char* s = getenv(ENV_PRINTHEAP);
		if (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0) {
			hpp_mode |= HPPA_INT_OPT_PRINT_HEAP;
		}
	}

	if (getenv(ENV_INITAS)) {
		/* Replace initial allocation strategy. This is a little unsafe, due to strtol. */
		hpp_mode = (hpp_mode & ~HPPA_AS_MASK) |
			(strtol(getenv(ENV_INITAS), NULL, 10) & HPPA_AS_MASK);
	}

	alloc_threshold = size_from_s(ENV_ALLOCTHRES, MIN_HUGE_PAGE_SIZE);
	anon_heap.size = size_from_s(ENV_HEAPSIZE_ANON, anon_heap.size);
	named_heap.size = size_from_s(ENV_HEAPSIZE_NAMED, named_heap.size);

	if (!hpp_init_file_backed_mappings(&named_heap)) {
		debug_print("Unable to init file based mapping.\n");
	}

	if (!hpp_init_anon_mappings(&anon_heap)) {
		debug_print("Unable to init via hugepage mmap. Please, check available hugepages! Exiting.\n");
		return;
	}

	hpp_print_heap(&named_heap);
	hpp_print_heap(&anon_heap);

	is_initialized = true;

	debug_print("initialized\n");
}

/* actual allocation/deallocation */

static void *hpp_block_alloc(heap_t *heap, const size_t size)
{
	/* waste at least one cacheline for meta data to ensure proper alignment */
	size_t req_size = ROUND_TO_MULTIPLE(size, BLOCK_ALIGN) +
		ROUND_TO_MULTIPLE(sizeof(heap_block_t), BLOCK_ALIGN);

	/* search for free and large enough block */
	heap_block_t* block = heap->next;
	while (BLOCK_IN_HEAP(block, heap) && BLOCK_USED(block) && block->size < req_size) {
		block = NEXT_BLOCK(block);
	}

	if (!BLOCK_IN_HEAP(block, heap) || (BLOCK_IN_HEAP(block, heap) && block->size < req_size)) {
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
		if (heap->next == block) {
			heap->next = new_block;
		}

		if (BLOCK_IN_HEAP(old_next, heap)) {
			old_next->prev = new_block;
		}
	}

	block->size |= (~0 & BLOCK_MASK_USED);

	return (char*) block + ROUND_TO_MULTIPLE(sizeof(heap_block_t), BLOCK_ALIGN);
}

static void hpp_block_free(heap_t *heap, heap_block_t *block)
{
	/* reset flags */
	block->size = block->size & BLOCK_MASK_SIZE;
	if (block < heap->next) {
		heap->next = block;
	}

	/* try to merge with next */
	heap_block_t *next = NEXT_BLOCK(block);
	if (BLOCK_IN_HEAP(next, heap) && !BLOCK_USED(next)) {
		block->size += next->size;
	}

	/* try to merge with prev */
	if (block->prev && !BLOCK_USED(block->prev)) {
		block->prev->size += block->size;
		block = block->prev;
	}

	/* update (new) next's prev pointer */
	next = NEXT_BLOCK(block);
	if (BLOCK_IN_HEAP(next, heap)) {
		next->prev = block;
	}
}

void hpp_set_mode(int mode)
{
	hpp_mode |= (mode & HPPA_AS_MASK);
}

void* hpp_alloc(size_t n, size_t elem_size)
{
	if (!is_initialized) {
		hpp_init();
	}

	/* no overflow checks as in calloc here */
	size_t alloc_size = n * elem_size;
	void *retval = NULL;
	heap_t *heap = NULL;

	if (named_heap.pool && (hpp_mode & HPPA_AS_NAMED)) {
		heap = &named_heap;
	}

	if (!heap && anon_heap.pool && (hpp_mode & HPPA_AS_ANON)) {
		heap = &anon_heap;
	}

	if (heap && alloc_size >= alloc_threshold) {
		retval = hpp_block_alloc(heap, alloc_size);

		if (!retval) {
			debug_print("failed to alloc %zu * %zu Bytes = %zu Bytes\n", n, elem_size, alloc_size);
		} else {
			debug_print("allocated %zu * %zu Bytes => %zu Bytes @ %p\n", n, elem_size, alloc_size, retval);
		}
		hpp_print_heap(heap);
	}

	if (!retval && (hpp_mode & HPPA_AS_MALLOC)) {
		retval = malloc(alloc_size);
	}

	return retval;
}


void hpp_free(void *ptr)
{
	heap_t *heap = NULL;
	if (ADDR_IN_HEAP(ptr, &anon_heap)) {
		heap = &anon_heap;
	}

	if (ADDR_IN_HEAP(ptr, &named_heap)) {
		heap = &named_heap;
	}

	if (heap) {
		heap_block_t *block = BLOCK_FROM_ADDR(ptr);
		debug_print("free block of %zu Bytes at %p\n", block->size & BLOCK_MASK_SIZE, block);
		hpp_block_free(heap, block);
		hpp_print_heap(heap);
	} else {
		free(ptr);
	}
}
