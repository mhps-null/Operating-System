#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/graphics/graphics.h"

// ==================== PROCESS CONTROL BLOCK LIST ====================

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];

struct ProcessManagerState process_manager_state = {
    .process_slot_used = {[0 ... PROCESS_COUNT_MAX - 1] = false},
    .active_process_count = 0,
    .last_pid = 0};

// ==================== HELPER FUNCTIONS ====================

static int32_t process_list_get_inactive_index(void)
{
    for (int32_t i = 0; i < PROCESS_COUNT_MAX; i++)
    {
        if (!process_manager_state.process_slot_used[i])
        {
            return i;
        }
    }
    return -1;
}

static uint32_t process_generate_new_pid(void)
{
    return ++process_manager_state.last_pid;
}

static uint32_t ceil_div(uint32_t a, uint32_t b)
{
    return (a + b - 1) / b;
}

// ==================== MAIN FUNCTIONS ====================

struct ProcessControlBlock *process_get_current_running_pcb_pointer(void)
{
    for (int i = 0; i < PROCESS_COUNT_MAX; i++)
    {
        if (_process_list[i].metadata.state == PROCESS_STATE_RUNNING)
        {
            return &_process_list[i];
        }
    }
    return NULL;
}

int32_t process_create_user_process(struct EXT2DriverRequest request)
{
    int32_t retcode = PROCESS_CREATE_SUCCESS;
    struct PageDirectory *old_page_dir = NULL;
    struct PageDirectory *new_page_dir = NULL;
    struct ProcessControlBlock *new_pcb = NULL;

    // ========== 0. VALIDATION & CHECKS ==========

    if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX)
    {
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }
    if ((uint32_t)request.buf >= KERNEL_VIRTUAL_ADDRESS_BASE)
    {
        retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
        goto exit_cleanup;
    }
    if (request.buffer_size == 0)
    {
        retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
        goto exit_cleanup;
    }
    // Hitung kebutuhan page: executable + 1 untuk user stack
    uint32_t page_frame_count_needed = ceil_div(request.buffer_size, PAGE_FRAME_SIZE) + 1;

    if (!paging_allocate_check(page_frame_count_needed) ||
        page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX)
    {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    int32_t p_index = process_list_get_inactive_index();
    if (p_index == -1)
    {
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    new_pcb = &(_process_list[p_index]);

    // Clear PCB
    memset(new_pcb, 0, sizeof(struct ProcessControlBlock));

    // ========== 4.1.3.1. VIRTUAL ADDRESS SPACE ==========

    // Create new page directory
    new_page_dir = paging_create_new_page_directory();
    if (new_page_dir == NULL)
    {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    new_pcb->context.page_directory_virtual_addr = new_page_dir;

    // Allocate pages for executable code (starting from virtual address 0x0)
    uint32_t exec_page_count = ceil_div(request.buffer_size, PAGE_FRAME_SIZE);
    for (uint32_t i = 0; i < exec_page_count; i++)
    {
        void *virtual_addr = (void *)(i * PAGE_FRAME_SIZE);

        if (!paging_allocate_user_page_frame(new_page_dir, virtual_addr))
        {
            retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
            goto exit_cleanup_page_dir;
        }

        // Track allocated virtual address
        new_pcb->memory.virtual_addr_used[new_pcb->memory.page_frame_used_count] = virtual_addr;
        new_pcb->memory.page_frame_used_count++;
    }

    // Allocate user stack at high memory (last 4 MiB before kernel space)
    // Virtual address: 0xBFC00000 (just below 0xC0000000)
    void *user_stack_virtual = (void *)(KERNEL_VIRTUAL_ADDRESS_BASE - PAGE_FRAME_SIZE);
    if (!paging_allocate_user_page_frame(new_page_dir, user_stack_virtual))
    {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup_page_dir;
    }

    new_pcb->memory.virtual_addr_used[new_pcb->memory.page_frame_used_count] = user_stack_virtual;
    new_pcb->memory.page_frame_used_count++;

    // ========== 4.1.3.2. LOAD EXECUTABLE ==========

    // Save current page directory
    old_page_dir = paging_get_current_page_directory_addr();

    // Switch to new process page directory
    paging_use_page_directory(new_page_dir);

    // Read executable from filesystem to memory at virtual address 0x0
    struct EXT2DriverRequest read_request = {
        .buf = (void *)0x0, // Load at virtual address 0x0
        .name = request.name,
        .name_len = request.name_len,
        .parent_inode = request.parent_inode,
        .buffer_size = request.buffer_size,
        .is_directory = false};

    int8_t read_result = read(read_request);

    // Switch back to old page directory
    paging_use_page_directory(old_page_dir);

    // Check if read was successful
    if (read_result != 0)
    {
        retcode = PROCESS_CREATE_FAIL_FS_READ_FAILURE;
        goto exit_cleanup_page_dir;
    }

    // ========== 4.1.3.3. CONTEXT INITIALIZATION ==========

    // Initialize CPU registers - all start at 0
    new_pcb->context.cpu.index.edi = 0;
    new_pcb->context.cpu.index.esi = 0;
    new_pcb->context.cpu.stack.ebp = 0;
    new_pcb->context.cpu.general.ebx = 0;
    new_pcb->context.cpu.general.edx = 0;
    new_pcb->context.cpu.general.ecx = 0;
    new_pcb->context.cpu.general.eax = 0;

    // Set stack pointer to top of user stack (grows downward)
    // ESP points to last valid address in stack
    new_pcb->context.cpu.stack.esp = (uint32_t)user_stack_virtual + PAGE_FRAME_SIZE - 4;

    // Set segment registers to user data segment with privilege level 3
    // Segment selector format: index | TI | RPL
    // GDT_USER_DATA_SEGMENT_SELECTOR = 0x20, RPL = 3
    uint32_t user_data_segment = GDT_USER_DATA_SEGMENT_SELECTOR | 0x3;
    new_pcb->context.cpu.segment.gs = user_data_segment;
    new_pcb->context.cpu.segment.fs = user_data_segment;
    new_pcb->context.cpu.segment.es = user_data_segment;
    new_pcb->context.cpu.segment.ds = user_data_segment;

    // Set instruction pointer to start of executable (virtual address 0x0)
    new_pcb->context.eip = 0x0;

    // Set eflags with mandatory base flag and interrupt enable
    new_pcb->context.eflags = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;

    // ========== 4.1.3.4. PROCESS METADATA & CLEANUP ==========

    // Initialize metadata
    new_pcb->metadata.process_id = process_generate_new_pid();
    new_pcb->metadata.state = PROCESS_STATE_READY;
    memset(new_pcb->metadata.process_name, 0, PROCESS_NAME_LENGTH_MAX);

    // Copy process name (pastikan tidak overflow)
    uint32_t name_copy_len = request.name_len;
    if (name_copy_len >= PROCESS_NAME_LENGTH_MAX)
    {
        name_copy_len = PROCESS_NAME_LENGTH_MAX - 1;
    }
    memcpy(new_pcb->metadata.process_name, request.name, name_copy_len);

    // Mark process slot as used
    process_manager_state.process_slot_used[p_index] = true;
    process_manager_state.active_process_count++;

    return PROCESS_CREATE_SUCCESS;

exit_cleanup_page_dir:
    // Cleanup: free all allocated page frames
    if (new_pcb != NULL)
    {
        for (uint32_t i = 0; i < new_pcb->memory.page_frame_used_count; i++)
        {
            paging_free_user_page_frame(
                new_pcb->context.page_directory_virtual_addr,
                new_pcb->memory.virtual_addr_used[i]);
        }
    }

    // Free page directory
    if (new_page_dir != NULL)
    {
        paging_free_page_directory(new_page_dir);
    }

    // Restore old page directory if it was changed
    if (old_page_dir != NULL)
    {
        paging_use_page_directory(old_page_dir);
    }

exit_cleanup:
    return retcode;
}

bool process_destroy(uint32_t pid)
{
    int32_t index = -1;

    // Cari PCB
    for (int i = 0; i < PROCESS_COUNT_MAX; i++)
    {
        if (process_manager_state.process_slot_used[i] &&
            _process_list[i].metadata.process_id == pid)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
        return false;

    struct ProcessControlBlock *pcb = &_process_list[index];

    if (strcmp(pcb->metadata.process_name, "clock") == 0)
    {
        // Jam format "HH:MM:SS" = 8 karakter
        char blank[9];
        memset(blank, ' ', 8);
        blank[8] = '\0';

        // row 24 sesuai program clock-mu
        // col = 80 - 8
        graphics_write_string(24, 80 - 8, blank, COLOR_WHITE);
    }

    for (uint32_t i = 0; i < pcb->memory.page_frame_used_count; i++)
    {
        paging_free_user_page_frame(
            pcb->context.page_directory_virtual_addr,
            pcb->memory.virtual_addr_used[i]);
    }

    // Free page directory
    paging_free_page_directory(pcb->context.page_directory_virtual_addr);

    memset(pcb, 0, sizeof(struct ProcessControlBlock));

    // Update state manager
    process_manager_state.process_slot_used[index] = false;
    process_manager_state.active_process_count--;

    return true;
}

int32_t get_process_info(ProcessInfo *buffer, uint32_t bufsize)
{
    uint32_t count = 0;
    for (int i = 0; i < PROCESS_COUNT_MAX && count < bufsize; i++)
    {
        if (_process_list[i].metadata.state != PROCESS_STATE_UNUSED)
        {
            buffer[count].pid = _process_list[i].metadata.process_id;
            buffer[count].state = _process_list[i].metadata.state;
            memcpy(buffer[count].name, _process_list[i].metadata.process_name, PROCESS_NAME_LENGTH_MAX);
            buffer[count].name[PROCESS_NAME_LENGTH_MAX - 1] = '\0';
            buffer[count].name_len = strlen(_process_list[i].metadata.process_name);
            count++;
        }
    }
    return (int32_t)count;
}

struct ProcessControlBlock *process_get_pcb_by_index(uint32_t index)
{
    if (index >= PROCESS_COUNT_MAX)
        return NULL;
    if (!process_manager_state.process_slot_used[index])
        return NULL;
    return &_process_list[index];
}

uint32_t find_pid_by_name(const char *name)
{
    for (int i = 0; i < PROCESS_COUNT_MAX; i++)
    {
        if (process_manager_state.process_slot_used[i] &&
            strcmp(_process_list[i].metadata.process_name, name) == 0)
        {
            return _process_list[i].metadata.process_id;
        }
    }
    return 0;
}

bool is_process_running(const char *name)
{
    for (int i = 0; i < PROCESS_COUNT_MAX; i++)
    {
        if (process_manager_state.process_slot_used[i])
        {
            if (strcmp(_process_list[i].metadata.process_name, name) == 0)
                return true;
        }
    }
    return false;
}