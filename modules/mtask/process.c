#include "process.h"
#include "kernlib/kernmem.h"
#include "modules/vmemory/vmemory.h"

void create_process(process* pr)
{
	pr->memory_hndl = kmalloc(get_mem_hndl_size());
	create_mem_hndl(pr->memory_hndl);
	pr->thread_cnt = 0; pr->threads = NULL;
	pr->priority = 1;
}

thread* process_add_thread(process* pr, thread* th)
{
	++pr->thread_cnt;
	pr->threads = krealloc(pr->threads, pr->thread_cnt * sizeof(thread));
	pr->threads[pr->thread_cnt - 1] = *th;
	th->parent_proc = pr;
	return &pr->threads[pr->thread_cnt - 1];
}
