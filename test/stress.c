#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hppalloc.h"

#define N_BLOCKS 128

int main(void)
{
	void *blocks[N_BLOCKS] = { NULL };

	srand(0);

	/* try several random sizes (maybe) including allocations 
	 * smaller than the hugepage size */
	for (int i = 0; i < N_BLOCKS; i++) {
		size_t block_size = (rand() % 128 + 1) * (1U << 19);
		blocks[i] = hpp_alloc(block_size, 1);

		/* free a random block with 10% probability */
		if (rand() % 10 == 0) {
			int block = rand() % N_BLOCKS;
			if (blocks[block]) {
				hpp_free(blocks[block]);
				blocks[i] = NULL;
			}
		}

		if (blocks[i]) {
			memset(blocks[i], 0xC3, block_size);
		}
	}

	/* finally, free all still reserved blocks */
	for (int i = 0; i < N_BLOCKS; i++) {
		if (blocks[i]) {
			hpp_free(blocks[i]);
		}
	}

}
