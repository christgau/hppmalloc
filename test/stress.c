#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hppalloc.h"

int main(int argc, char** argv)
{
	int n_blocks = 64;
	if (argc > 1) {
		n_blocks = atoi(argv[1]);
	}

	void *blocks[n_blocks];
	int failed_allocs = 0;

	srand(0);
	for (int i = 0; i < n_blocks; i++) {
		blocks[i] = NULL;
	}

	/* try several random sizes (maybe) including allocations
	 * smaller than the hugepage size */
	for (int i = 0; i < n_blocks; i++) {
		size_t block_size = (rand() % 128 + 1) * (1U << 19);

		blocks[i] = hpp_alloc(block_size, 1);
		if (blocks[i] == NULL) {
			failed_allocs++;
			fprintf(stderr, "allocation of block %i (%zu Bytes) failed\n", i, block_size);
		}

		/* free a random block with 10% probability */
		if (rand() % 10 == 0) {
			int block = rand() % n_blocks;
			if (blocks[block]) {
				hpp_free(blocks[block]);
				blocks[block] = NULL;
			}
		}

		if (blocks[i]) {
			memset(blocks[i], 0xC3, block_size);
		}
	}

	/* finally, free all still reserved blocks */
	for (int i = 0; i < n_blocks; i++) {
		if (blocks[i]) {
			hpp_free(blocks[i]);
		}
	}

	if (failed_allocs > 0) {
		fprintf(stderr, "%d allocations failed\n", failed_allocs);
	}
	return failed_allocs > 0 ? 1 : 0;
}
