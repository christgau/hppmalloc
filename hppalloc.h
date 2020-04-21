#ifndef HPPALLOC_H
#define HPPALLOC_H

#include <stddef.h>

#define HPPA_NOMALLOC (1 << 1)
#define HPPA_NOPMM    (1 << 2)

void* hpp_alloc(size_t n, size_t elem_size);
void hpp_free(void *ptr);

#endif
