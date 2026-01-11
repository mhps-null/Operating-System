#include "header/process/scheduler.h"
#include "header/memory/paging.h"

static int32_t current_process_index = -1;

static int32_t find_next_ready_process(int32_t start)
{
    for (int32_t i = 1; i <= PROCESS_COUNT_MAX; i++)
    {
        int32_t idx = (start + i) % PROCESS_COUNT_MAX;
        if (_process_list[idx].metadata.state == PROCESS_STATE_READY)
            return idx;
    }
    return -1;
}

void scheduler_init(void)
{
    current_process_index = -1;
}

/**
 * Dipanggil dari interrupt handler
 */
void scheduler_save_context_to_current_running_pcb(struct Context ctx)
{
    struct ProcessControlBlock *pcb = process_get_current_running_pcb_pointer();
    if (pcb != NULL)
    {
        pcb->context = ctx;
    }
}

__attribute__((noreturn)) void scheduler_switch_to_next_process(void)
{

    int32_t next_index;

    // Kasus pertama: belum ada process berjalan
    if (current_process_index == -1)
    {
        for (int32_t i = 0; i < PROCESS_COUNT_MAX; i++)
        {
            if (_process_list[i].metadata.state == PROCESS_STATE_READY)
            {
                next_index = i;
                goto switch_process;
            }
        }
        while (1)
            ; // tidak ada process
    }

    // Kembalikan process lama ke READY
    struct ProcessControlBlock *old =
        &_process_list[current_process_index];
    if (old->metadata.state == PROCESS_STATE_RUNNING)
        old->metadata.state = PROCESS_STATE_READY;

    next_index = find_next_ready_process(current_process_index);

    if (next_index == -1)
        next_index = current_process_index;

switch_process:
    struct ProcessControlBlock *next =
        &_process_list[next_index];

    next->metadata.state = PROCESS_STATE_RUNNING;
    current_process_index = next_index;

    paging_use_page_directory(
        next->context.page_directory_virtual_addr);

    process_context_switch(next->context);
}
