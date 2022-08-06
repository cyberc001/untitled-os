#ifndef PROCESS_H
#define PROCESS_H

#include "thread.h"

/* Process API.
*/

#define PROCESS_MEM_CHUNK_SIZE		2 * 1024 * 1024

typedef struct{
	void* memory_hndl;	// handler size is derived from get_mem_hndl_size()

	size_t page_cnt;
	void** page_locs;
} process;

#endif
