#ifndef HPPALLOC_H
#define HPPALLOC_H

#include <stddef.h>

/* Try to allocate with malloc (if everything else fails). */
#define HPPA_AS_MALLOC (1 << 0)

/* Allocate memory from the anonymous hugetable heap. */
#define HPPA_AS_ANON   (1 << 1)

/* Allocate memory from the named/file-backed (e.g. pmem) heap. */
/* If the named heap is enabled, it overrides the anoymous heap even if enabled. */
#define HPPA_AS_NAMED  (1 << 2)
#define HPPA_AS_PMEM   HPPA_AS_NAMED

#define HPPA_AS_MAX    (2)
#define HPPA_AS_MASK   ((1 << (HPPA_AS_MAX + 1)) - 1)
#define HPPA_AS_ALL    HPPA_AS_MASK

/* convenience flags */
#define HPPA_AS_NO_MALLOC  (~HPPA_AS_MALLOC & HPPA_AS_MASK)
#define HPPA_AS_NO_ANON    (~HPPA_AS_ANON & HPPA_AS_MASK)
#define HPPA_AS_NO_NAMED   (~HPPA_AS_NAMED & HPPA_AS_MASK)

/* set the mode and return the old one */
int hpp_set_mode(int mode);

void* hpp_alloc(size_t n, size_t elem_size);
void hpp_free(void *ptr);

#endif
