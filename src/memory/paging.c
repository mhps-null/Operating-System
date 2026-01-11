#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/process/process.h"

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        [0] = {
            .flag.present_bit = 1,
            .flag.write_bit = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address = 0,
        },
        [0x300] = {
            .flag.present_bit = 1,
            .flag.write_bit = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address = 0,
        },
    }};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0] = true,
        [1 ... PAGE_FRAME_MAX_COUNT - 1] = false},
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT - 1};

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr,
    void *virtual_addr,
    struct PageDirectoryEntryFlag flag)
{
    uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    page_dir->table[page_index].flag = flag;
    page_dir->table[page_index].lower_address = ((uint32_t)physical_addr >> 22) & 0x3FF;
    page_dir->table[page_index].global_page = 0;
    page_dir->table[page_index].ignored_1 = 0;
    page_dir->table[page_index].page_attribute_table_bit = 0;
    page_dir->table[page_index].reserved_1 = 0;
    flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr)
{
    asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr) : "memory");
}

/* --- Memory Management --- */
bool paging_allocate_check(uint32_t amount)
{
    uint32_t free_bytes_available = page_manager_state.free_page_frame_count * PAGE_FRAME_SIZE;
    return free_bytes_available >= amount;
}

bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
    int32_t free_frame_index = -1;
    for (uint32_t i = 0; i < PAGE_FRAME_MAX_COUNT; i++)
    {
        if (!page_manager_state.page_frame_map[i])
        {
            free_frame_index = i;
            break;
        }
    }

    if (free_frame_index == -1)
    {
        return false;
    }

    page_manager_state.page_frame_map[free_frame_index] = true;
    page_manager_state.free_page_frame_count--;

    void *physical_addr = (void *)(free_frame_index * PAGE_FRAME_SIZE);

    struct PageDirectoryEntryFlag user_flags = {0};
    user_flags.present_bit = 1;
    user_flags.write_bit = 1;
    user_flags.user_supervisor_bit = 1;
    user_flags.use_pagesize_4_mb = 1;

    update_page_directory_entry(page_dir, physical_addr, virtual_addr, user_flags);

    return true;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
    uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    volatile struct PageDirectoryEntry *entry = &page_dir->table[page_index];

    if (!entry->flag.present_bit)
    {
        return false;
    }

    uint32_t frame_index = entry->lower_address;

    if (frame_index >= PAGE_FRAME_MAX_COUNT || !page_manager_state.page_frame_map[frame_index])
    {
        return false;
    }

    page_manager_state.page_frame_map[frame_index] = false;
    page_manager_state.free_page_frame_count++;

    *(uint32_t *)entry = 0;

    flush_single_tlb(virtual_addr);

    return true;
}

__attribute__((aligned(0x1000))) static struct PageDirectory page_directory_list[PAGING_DIRECTORY_TABLE_MAX_COUNT] = {0};

static struct
{
    bool page_directory_used[PAGING_DIRECTORY_TABLE_MAX_COUNT];
} page_directory_manager = {
    .page_directory_used = {false},
};

struct PageDirectory *paging_create_new_page_directory(void)
{
    for (int i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++)
    {
        if (!page_directory_manager.page_directory_used[i])
        {
            page_directory_manager.page_directory_used[i] = true;

            struct PageDirectory *page_dir = &page_directory_list[i];

            // Copy entire kernel page directory
            memcpy(
                page_dir,
                &_paging_kernel_page_directory,
                sizeof(struct PageDirectory));

            return page_dir;
        }
    }
    return NULL;
}
bool paging_free_page_directory(struct PageDirectory *page_dir)
{
    // Iterate page_directory_list[] & check &page_directory_list[i] == page_dir
    for (int i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++)
    {
        if (&page_directory_list[i] == page_dir)
        {
            // Mark the page directory as unused
            page_directory_manager.page_directory_used[i] = false;

            // Clear all page directory entry
            memset(page_dir, 0, sizeof(struct PageDirectory));

            return true;
        }
    }

    return false;
}

struct PageDirectory *paging_get_current_page_directory_addr(void)
{
    uint32_t current_page_directory_phys_addr;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_page_directory_phys_addr) : /* <Empty> */);
    uint32_t virtual_addr_page_dir = current_page_directory_phys_addr + KERNEL_VIRTUAL_ADDRESS_BASE;
    return (struct PageDirectory *)virtual_addr_page_dir;
}

void paging_use_page_directory(struct PageDirectory *page_dir_virtual_addr)
{
    uint32_t physical_addr_page_dir = (uint32_t)page_dir_virtual_addr;
    // Additional layer of check & mistake safety net
    if ((uint32_t)page_dir_virtual_addr > KERNEL_VIRTUAL_ADDRESS_BASE)
        physical_addr_page_dir -= KERNEL_VIRTUAL_ADDRESS_BASE;
    __asm__ volatile("mov %0, %%cr3" : /* <Empty> */ : "r"(physical_addr_page_dir) : "memory");
}