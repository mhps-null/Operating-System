#include "header/cpu/idt.h"
#include "header/cpu/gdt.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/process/scheduler.h"
#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/cmos/cmos.h"
#include "header/stdlib/string.h"
#include "header/graphics/graphics.h"

static uint8_t g_adapter_buffer[BLOCK_SIZE];

void io_wait(void)
{
    out(0x80, 0);
}

void pic_ack(uint8_t irq)
{
    if (irq >= 8)
        out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void)
{
    // Starts the initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
    io_wait();
    out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
    io_wait();
    out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    // Disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame)
{
    switch (frame.int_number)
    {
    case PIC1_OFFSET + IRQ_TIMER:
        pic_ack(IRQ_TIMER);
        struct Context ctx = {
            .cpu = frame.cpu,
            .eip = frame.int_stack.eip,
            .eflags = frame.int_stack.eflags,
            .page_directory_virtual_addr = paging_get_current_page_directory_addr()};
        scheduler_save_context_to_current_running_pcb(ctx);
        scheduler_switch_to_next_process();
        break;

    case PIC1_OFFSET + IRQ_KEYBOARD:
        keyboard_isr();
        break;

    case 0x30:
        syscall(frame);
        break;
    default:
        break;
    }
}

void activate_timer_interrupt(void)
{
    __asm__ volatile("cli");
    uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
    out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t)(pit_timer_counter_to_fire & 0xFF));
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t)((pit_timer_counter_to_fire >> 8) & 0xFF));

    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
    __asm__ volatile("sti");
}

void activate_keyboard_interrupt(void)
{
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

struct TSSEntry _interrupt_tss_entry = {
    .ss0 = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void)
{
    uint32_t stack_ptr;
    // Reading base stack frame instead esp
    __asm__ volatile("mov %%ebp, %0" : "=r"(stack_ptr) : /* <Empty> */);
    // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
    _interrupt_tss_entry.esp0 = stack_ptr + 8;
}

static uint32_t find_inode_by_path(const char *path)
{
    if (strcmp(path, "/") == 0)
    {
        return ROOT_INODE_NUM; // Root inode
    }

    struct EXT2Inode current_inode;
    uint32_t current_inode_num = ROOT_INODE_NUM;
    read_inode(current_inode_num, &current_inode);

    char path_copy[1024];
    strcpy(path_copy, path);

    char *token = strtok(path_copy, "/");
    while (token != NULL)
    {
        uint32_t next_inode_num = find_inode_by_name(&current_inode, token, strlen(token));
        if (next_inode_num == 0)
        {
            return 0; // Entry tidak ditemukan
        }

        current_inode_num = next_inode_num;
        read_inode(current_inode_num, &current_inode);

        token = strtok(NULL, "/");
    }

    return current_inode_num;
}

static int32_t find_parent_inode_and_name(const char *full_path, uint32_t *parent_inode_out, char *name_out)
{
    char path_copy[1024];
    strcpy(path_copy, full_path);

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash == NULL)
    {
        *parent_inode_out = ROOT_INODE_NUM;
        strcpy(name_out, path_copy);
        return 0;
    }

    if (last_slash == path_copy)
    {
        *parent_inode_out = ROOT_INODE_NUM;
        strcpy(name_out, last_slash + 1);
        return 0;
    }

    *last_slash = '\0';
    strcpy(name_out, last_slash + 1);

    *parent_inode_out = find_inode_by_path(path_copy);
    if (*parent_inode_out == 0)
    {
        return -1; // Parent path tidak ditemukan
    }
    return 0;
}

int32_t ext2_read(const char *path, char *buffer)
{
    struct EXT2DriverRequest req;
    char name_buf[256];
    uint32_t parent_ino;

    if (find_parent_inode_and_name(path, &parent_ino, name_buf) != 0)
    {
        return 3; // 3: not found (parent invalid)
    }

    req.buf = buffer;
    req.name = name_buf;
    req.name_len = strlen(name_buf);
    req.parent_inode = parent_ino;
    req.buffer_size = 0x200000; // 2MB buffer size to accommodate large files like badapple
    req.is_directory = false;

    return (int32_t)read(req);
}

int32_t ext2_ls(const char *path, char *buffer)
{
    uint32_t dir_inode_num = find_inode_by_path(path);
    if (dir_inode_num == 0)
    {
        return -1; // Not found
    }

    struct EXT2Inode dir_inode;
    read_inode(dir_inode_num, &dir_inode);

    if ((dir_inode.i_mode & EXT2_S_IFDIR) == 0)
    {
        return -2; // Not a directory
    }

    *buffer = '\0';

    for (int i = 0; i < 12; i++)
    {
        uint32_t block_num = dir_inode.i_block[i];
        if (block_num == 0)
            continue;

        read_blocks(g_adapter_buffer, block_num, 1);

        uint32_t offset = 0;
        while (offset < BLOCK_SIZE)
        {
            struct EXT2DirectoryEntry *entry = get_directory_entry(g_adapter_buffer, offset);
            if (entry->inode == 0 || entry->rec_len == 0)
            {
                break;
            }

            char *entry_name_ptr = get_entry_name(entry);
            char entry_name[256];
            memcpy(entry_name, entry_name_ptr, entry->name_len);
            entry_name[entry->name_len] = '\0';

            if (strcmp(entry_name, ".") != 0 && strcmp(entry_name, "..") != 0)
            {
                strcat(buffer, entry_name);
                strcat(buffer, "\n");
            }

            offset += entry->rec_len;
        }
    }
    // belum indirect blocks
    return 0; // Sukses
}

int32_t ext2_stat_dir(const char *path)
{
    uint32_t inode_num = find_inode_by_path(path);
    if (inode_num == 0)
    {
        return -1; // Not found
    }

    struct EXT2Inode inode;
    read_inode(inode_num, &inode);

    if ((inode.i_mode & EXT2_S_IFDIR) != 0)
    {
        return 0; // Sukses, ini adalah direktori
    }
    else
    {
        return -2; // Bukan direktori
    }
}

int32_t ext2_mkdir(const char *path, const char *name)
{
    uint32_t parent_ino = find_inode_by_path(path);
    if (parent_ino == 0)
    {
        return 2; // invalid parent folder
    }

    struct EXT2DriverRequest req;
    req.buf = NULL;
    req.name = (char *)name;
    req.name_len = strlen(name);
    req.parent_inode = parent_ino;
    req.buffer_size = 0;
    req.is_directory = true;

    return (int32_t)write(&req);
}

int32_t ext2_write(const char *path, const char *buffer, uint32_t size)
{
    struct EXT2DriverRequest req;
    char name_buf[256];
    uint32_t parent_ino;

    if (find_parent_inode_and_name(path, &parent_ino, name_buf) != 0)
    {
        return 2; // invalid parent folder
    }

    req.buf = (void *)buffer;
    req.name = name_buf;
    req.name_len = strlen(name_buf);
    req.parent_inode = parent_ino;
    req.buffer_size = size;
    req.is_directory = false;

    return (int32_t)write(&req);
}

int32_t ext2_rm(const char *path, const char *name)
{
    uint32_t parent_ino = find_inode_by_path(path);
    if (parent_ino == 0)
    {
        return 3; // parent folder invalid
    }

    struct EXT2DriverRequest req;
    req.buf = NULL;
    req.name = (char *)name;
    req.name_len = strlen(name);
    req.parent_inode = parent_ino;
    req.buffer_size = 0;

    req.is_directory = false;
    int8_t ret = delete(req);

    if (ret == 1) // 1: not found
    {
        req.is_directory = true;
        ret = delete(req);
    }

    return (int32_t)ret;
}

int32_t ext2_rename(const char *old_path, const char *new_path)
{
    char old_name_buf[256];
    uint32_t old_parent_ino;

    char new_name_buf[256];
    uint32_t new_parent_ino;

    if (find_parent_inode_and_name(old_path, &old_parent_ino, old_name_buf) != 0)
    {
        return -1; // Gagal: sumber tidak ditemukan
    }

    if (find_parent_inode_and_name(new_path, &new_parent_ino, new_name_buf) != 0)
    {
        return -1; // Gagal: parent tujuan tidak valid
    }

    return (int32_t)rename_entry(old_parent_ino, old_name_buf, new_parent_ino, new_name_buf);
}

void sleep(uint32_t ticks)
{
    // This is a simple busy-wait loop, not accurate and will block the CPU
    for (uint32_t i = 0; i < ticks * 10000; i++)
    {
        __asm__ volatile("nop");
    }
}

extern bool terminate_badapple;
extern bool ctrl_down;

void syscall(struct InterruptFrame frame)
{
    uint32_t ebx = frame.cpu.general.ebx;
    uint32_t ecx = frame.cpu.general.ecx;
    uint32_t edx = frame.cpu.general.edx;

    int32_t *retcode_ptr = (int32_t *)edx;

    switch (frame.cpu.general.eax)
    {
    case 0: // read
        *retcode_ptr = ext2_read((const char *)ebx, (char *)ecx);
        break;
    case 4: // input keyboard
        get_keyboard_buffer((char *)ebx);
        break;
    case 5: // show character
    {
        char *buf_ptr = (char *)ebx;
        uint8_t color = (uint8_t)ecx;
        // putc(*buf_ptr, color);
        graphics_putchar(*buf_ptr, color);
        break;
    }
    case 6: // show string
        // puts((char *)ebx, (uint8_t)ecx);
        char *str = (char *)ebx;
        uint8_t color = (uint8_t)ecx;
        // puts(str, color);
        graphics_puts(str, color);
        break;
    case 7: // activate keyboard
        keyboard_state_activate();
        activate_keyboard_interrupt();
        break;
    case 10: // clear screen
        // framebuffer_clear();
        clear_screen();
        break;
    case 11: // ls
        *retcode_ptr = ext2_ls((const char *)ebx, (char *)ecx);
        break;
    case 12: // stat
        *((int32_t *)ecx) = ext2_stat_dir((const char *)ebx);
        break;
    case 13: // mkdir, buat folder kosong
        *retcode_ptr = ext2_mkdir((const char *)ebx, (const char *)ecx);
        break;
    case 14: // write
        const char *buffer_ptr = (const char *)ecx;
        uint32_t size = edx;
        *retcode_ptr = ext2_write((const char *)ebx, buffer_ptr, size);
        break;
    case 15: // rm, menghapus
        *retcode_ptr = ext2_rm((const char *)ebx, (const char *)ecx);
        break;
    case 16: // rename
        *retcode_ptr = ext2_rename((const char *)ebx, (const char *)ecx);
        break;
    case 17: // cmos read
        cmos_reader data = get_cmos_data();
        memcpy((void *)frame.cpu.general.ebx, &data, sizeof(cmos_reader));
        break;
    case 18: // create process
    {
        char *path = (char *)ebx;
        uint32_t inode_num = find_inode_by_path(path);

        if (inode_num == 0)
        {
            *retcode_ptr = -1; // File executable tidak ditemukan
        }
        else
        {
            struct EXT2Inode inode;
            read_inode(inode_num, &inode);

            char name_buf[256];
            uint32_t parent_inode;

            if (find_parent_inode_and_name(path, &parent_inode, name_buf) == 0)
            {
                if (is_process_running(name_buf))
                {
                    *retcode_ptr = -2; // error: duplicate daemon
                    break;
                }

                struct EXT2DriverRequest req = {
                    .buf = NULL,
                    .name = name_buf,
                    .name_len = strlen(name_buf),
                    .parent_inode = parent_inode,
                    .buffer_size = inode.i_size,
                    .is_directory = false};
                *retcode_ptr = process_create_user_process(req);
            }
            else
            {
                *retcode_ptr = -1; // Path parent invalid
            }
        }
        break;
    }
    case 19: // exit process
    {
        struct ProcessControlBlock *current_pcb = process_get_current_running_pcb_pointer();
        if (current_pcb != NULL)
        {
            process_destroy(current_pcb->metadata.process_id);

            scheduler_switch_to_next_process();
        }
        break;
    }
    case 20: // get info process
    {
        ProcessInfo *user_buffer = (ProcessInfo *)ebx;
        uint32_t bufsize = (uint32_t)ecx;

        int32_t count = get_process_info(user_buffer, bufsize);

        *((int32_t *)edx) = count;
        break;
    }
    case 21: // destroy process
    {
        uint32_t target_pid = (uint32_t)ebx;
        bool result = process_destroy(target_pid);
        *retcode_ptr = result ? 0 : -1; // 0 success, -1 fail
        break;
    }
    case 22: // puts_at(row, col, str, color)
    {
        uint32_t row = frame.cpu.general.ebx;
        uint32_t col = frame.cpu.general.ecx;
        char *str = (char *)frame.cpu.general.edx;
        // framebuffer_puts_at(row, col, str, 0xC);
        graphics_write_string(col, row, str, COLOR_WHITE);
        break;
    }
    case 23: // get_pid_by_name(name, 0, out_pid)
    {
        const char *name = (const char *)frame.cpu.general.ebx;
        uint32_t *out_pid = (uint32_t *)frame.cpu.general.ecx;
        uint32_t pid = find_pid_by_name(name);
        *out_pid = pid;
        break;
    }
    case 24: // badapple
    {
        // Extract syscall parameters first
        uint32_t ebx = frame.cpu.general.ebx;
        uint32_t ecx = frame.cpu.general.ecx;
        uint32_t edx = frame.cpu.general.edx;

        const char *frame_buffer = (const char *)ebx;
        uint32_t width = ecx;
        uint32_t height = edx;

        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                char c = frame_buffer[y * width + x];
                if (c == '#')
                {
                    graphics_set_block(x, y, COLOR_WHITE);
                }
                else
                {
                    graphics_set_block(x, y, COLOR_BLACK);
                }
            }
        }
        swap_buffer();
        break;
    }
    case 25:
        sleep(frame.cpu.general.ebx);
        break;
    case 26:
        *retcode_ptr = terminate_badapple;
        break;
    case 27: // CLEAR_TERMINATE
    {
        terminate_badapple = false;
        ctrl_down = false;
        frame.cpu.general.eax = 0;
        break;
    }
    default:
        graphics_puts("Unknown Syscall\n", COLOR_RED);
    }
}
