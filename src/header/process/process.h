#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "header/cpu/interrupt.h"
#include "header/memory/paging.h"
#include "header/filesystem/ext2.h"

#define PROCESS_NAME_LENGTH_MAX 32
#define PROCESS_PAGE_FRAME_COUNT_MAX 8
#define PROCESS_COUNT_MAX 16

#define KERNEL_RESERVED_PAGE_FRAME_COUNT 4
#define KERNEL_VIRTUAL_ADDRESS_BASE 0xC0000000

#define CPU_EFLAGS_BASE_FLAG 0x2
#define CPU_EFLAGS_FLAG_CARRY 0x1
#define CPU_EFLAGS_FLAG_PARITY 0x4
#define CPU_EFLAGS_FLAG_AUX_CARRY 0x10
#define CPU_EFLAGS_FLAG_ZERO 0x40
#define CPU_EFLAGS_FLAG_SIGN 0x80
#define CPU_EFLAGS_FLAG_TRAP 0x100
#define CPU_EFLAGS_FLAG_INTERRUPT_ENABLE 0x200
#define CPU_EFLAGS_FLAG_DIRECTION 0x400
#define CPU_EFLAGS_FLAG_OVERFLOW 0x800
#define CPU_EFLAGS_FLAG_IO_PRIVILEGE 0x3000
#define CPU_EFLAGS_FLAG_NESTED_TASK 0x4000
#define CPU_EFLAGS_FLAG_MODE 0x8000
#define CPU_EFLAGS_FLAG_RESUME 0x10000
#define CPU_EFLAGS_FLAG_VIRTUAL_8086 0x20000
#define CPU_EFLAGS_FLAG_ALIGNMENT_CHECK 0x40000
#define CPU_EFLAGS_FLAG_VINTERRUPT_FLAG 0x80000
#define CPU_EFLAGS_FLAG_VINTERRUPT_PENDING 0x100000
#define CPU_EFLAGS_FLAG_CPUID_INSTRUCTION 0x200000
#define CPU_EFLAGS_FLAG_AES_SCHEDULE_LOAD 0x40000000
#define CPU_EFLAGS_FLAG_ALTER_INSTRUCTION 0x80000000

// Return code constant for process_create_user_process()
#define PROCESS_CREATE_SUCCESS 0
#define PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED 1
#define PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT 2
#define PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY 3
#define PROCESS_CREATE_FAIL_FS_READ_FAILURE 4

// ==================== TAMBAHAN UNTUK PCB ====================

void paging_map_higher_half_to_phys(uint32_t kernel_phys_base);

/**
 * PROCESS_STATE - enum untuk status process
 */
typedef enum PROCESS_STATE
{
    PROCESS_STATE_UNUSED,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_READY,
    PROCESS_STATE_BLOCKED
} PROCESS_STATE;

/**
 * Context - Berisi informasi yang diperlukan untuk melakukan context switch
 *
 * @param cpu                        Semua CPU register state
 * @param eip                        Instruction pointer untuk resume execution
 * @param eflags                     Flag register untuk resume execution
 * @param page_directory_virtual_addr Page directory yang digunakan process ini
 */
struct Context
{
    struct CPURegister cpu;
    uint32_t eip;
    uint32_t eflags;
    struct PageDirectory *page_directory_virtual_addr;
} __attribute__((packed));

/**
 * ProcessControlBlock - Struktur data yang merepresentasikan sebuah process
 *
 * @param metadata Informasi metadata tentang process
 * @param context  Context untuk context saving & switching
 * @param memory   Informasi memory yang digunakan process
 */
struct ProcessControlBlock
{
    // Metadata process
    struct
    {
        uint32_t process_id;                        // PID
        PROCESS_STATE state;                        // Status process
        char process_name[PROCESS_NAME_LENGTH_MAX]; // Nama process
    } metadata;

    // Context untuk context switching
    struct Context context;

    // Memory management
    struct
    {
        void *virtual_addr_used[PROCESS_PAGE_FRAME_COUNT_MAX];
        uint32_t page_frame_used_count;
    } memory;
} __attribute__((packed));

/**
 * ProcessManagerState - State dari process manager
 */
struct ProcessManagerState
{
    bool process_slot_used[PROCESS_COUNT_MAX];
    uint32_t active_process_count;
    uint32_t last_pid;
} __attribute__((packed));

extern struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];

extern struct ProcessManagerState process_manager_state;

// ==================== END TAMBAHAN PCB ====================

/**
 * Get currently running process PCB pointer
 *
 * @return Will return NULL if there's no running process
 */
struct ProcessControlBlock *process_get_current_running_pcb_pointer(void);

/**
 * Create new user process and setup the virtual address space.
 * All available return code is defined with macro "PROCESS_CREATE_*"
 *
 * @note          This procedure assumes no reentrancy in ISR
 * @param request Appropriate read request for the executable
 * @return        Process creation return code
 */
int32_t process_create_user_process(struct EXT2DriverRequest request);

/**
 * Destroy process then release page directory and process control block
 *
 * @param pid Process ID to delete
 * @return    True if process destruction success
 */
bool process_destroy(uint32_t pid);

/**
 * Get process control block by index (for testing/debugging)
 *
 * @param index Index in process list
 * @return PCB pointer or NULL if invalid/unused
 */
struct ProcessControlBlock *process_get_pcb_by_index(uint32_t index);

typedef struct
{
    uint32_t pid;
    PROCESS_STATE state;
    char name[32];
    uint8_t name_len;
} ProcessInfo;

int32_t get_process_info(ProcessInfo *buffer, uint32_t bufsize);

uint32_t find_pid_by_name(const char *name);

bool is_process_running(const char *name);

#endif