#include "hppalloc.h"

int main(void)
{
	/* ensure NULL is returned for really too large allocation sizes */
	hpp_set_mode(HPPA_AS_NO_MALLOC);
	void* ptr = hpp_alloc(16ULL * (1ULL << 40), 1);

	return (ptr == NULL ? 0 : 1);
}
