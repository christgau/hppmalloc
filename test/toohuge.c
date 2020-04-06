#include "hppalloc.h"

int main(void)
{
	/* ensure NULL is returned for really too large allocation sizes */
	void* ptr = hpp_alloc(16ULL * (1ULL << 40), 1, HPPA_NOPMM | HPPA_NOMALLOC);

	return (ptr == NULL ? 0 : 1);
}
