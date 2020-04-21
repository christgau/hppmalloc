#include "hppalloc.h"

#define SIZE_2MB (1U << 21)

int main(void)
{
	void* a = hpp_alloc(1 * SIZE_2MB, 1);
	void* b = hpp_alloc(2 * SIZE_2MB, 1);
	void* c = hpp_alloc(2 * SIZE_2MB, 1);
	void* d = hpp_alloc(4 * SIZE_2MB, 1);

	hpp_free(a);
	hpp_free(c);
	hpp_free(b);

	a = hpp_alloc(5 * SIZE_2MB, 1);

	hpp_free(d);
	hpp_free(a);
}
