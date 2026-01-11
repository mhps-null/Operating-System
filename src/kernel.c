#include <stdint.h>
#include <stdbool.h>
#include "header/kernel-entrypoint.h"
#include "header/cpu/gdt.h"
#include "header/cpu/portio.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/text/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/driver/disk.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"
#include "header/memory/paging.h"
#include "header/process/process.h"
#include "header/process/scheduler.h"
#include "header/graphics/graphics.h"

void kernel_setup(void)
{
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();

    initialize_vga();
    clear_screen();
    draw_cursor();

    initialize_filesystem_ext2();
    gdt_install_tss();
    set_tss_register();

    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

    struct EXT2DriverRequest request = {
        .buf = (uint8_t *)0,
        .name = "shell",
        .parent_inode = 1,
        .buffer_size = 0x10000,
        .name_len = 5,
        .is_directory = false};

    read(request);

    set_tss_kernel_current_stack();
    process_create_user_process(request);
    paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr);
    scheduler_init();
    activate_timer_interrupt();
    scheduler_switch_to_next_process();
}

// void kernel_setup(void)
// {
//     load_gdt(&_gdt_gdtr);
//     pic_remap();
//     initialize_idt();
//     activate_keyboard_interrupt();
//     framebuffer_clear();
//     framebuffer_set_cursor(0, 0);
//     initialize_filesystem_ext2();
//     gdt_install_tss();
//     set_tss_register();

//     paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

//     // Write shell into memory
//     struct EXT2DriverRequest request = {
//         .buf = (uint8_t *)0,
//         .name = "shell",
//         .parent_inode = 1,
//         .buffer_size = 0x100000,
//         .name_len = 5,
//     };

//     struct EXT2DriverRequest request2 = {
//         .buf = (uint8_t *)0,
//         .name = "clock",
//         .parent_inode = 1,
//         .buffer_size = 0x10000,
//         .name_len = 5,
//         .is_directory = false};

//     read(request);

// set_tss_kernel_current_stack();
// process_create_user_process(request);
// process_create_user_process(request2);
// paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr);
// scheduler_init();
// activate_timer_interrupt();
// scheduler_switch_to_next_process();
// }

// void kernel_setup(void)
// {
//     gdt_install_tss();
//     load_gdt(&_gdt_gdtr);
//     pic_remap();
//     initialize_idt();
//     activate_keyboard_interrupt();
//     framebuffer_clear();
//     framebuffer_set_cursor(0, 0);
//     initialize_filesystem_ext2();
//     set_tss_register();

//     // Allocate first 4 MiB virtual memory
//     paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

//     // Write shell into memory
//     struct EXT2DriverRequest request = {
//         .buf = (uint8_t *)0,
//         .name = "shell",
//         .parent_inode = 1,
//         .buffer_size = 0x100000,
//         .name_len = 5,
//     };
//     read(request);

//     char file_content[] = "Ini adalah isi file di dalam folder parent.";
//     struct EXT2DriverRequest request_file;
//     request_file.buf = file_content;
//     request_file.name = "file.txt";
//     request_file.name_len = 8;
//     request_file.parent_inode = 1;
//     request_file.buffer_size = sizeof(file_content);
//     request_file.is_directory = false;

//     write(&request_file);

//     // Set TSS $esp pointer and jump into shell
//     set_tss_kernel_current_stack();
//     kernel_execute_user_program((uint8_t *)0);

//     while (true)
//         ;
// }

// void kernel_setup(void)
// {
//     load_gdt(&_gdt_gdtr);
//     pic_remap();
//     initialize_idt();
//     framebuffer_clear();
//     framebuffer_set_cursor(0, 0);
//     __asm__("int $0x4");
//     while (true)
//         ;
// }

// void kernel_setup(void)
// {
//     load_gdt(&_gdt_gdtr);
//     pic_remap();
//     initialize_idt();
//     activate_keyboard_interrupt();
//     framebuffer_clear();
//     framebuffer_set_cursor(0, 0);

//     int row = 0, col = 0;
//     keyboard_state_activate();
//     while (true)
//     {
//         char c;
//         get_keyboard_buffer(&c);
//         if (c)
//         {
//             if (c == '\n')
//             {
//                 row++;
//                 col = 0;
//             }
//             else if (c == '\b')
//             {
//                 if (col > 0)
//                 {
//                     col--;
//                 }
//                 else if (row > 0)
//                 {
//                     row--;
//                     col = FRAMEBUFFER_WIDTH - 1;
//                 }
//                 framebuffer_write(row, col, ' ', 0x0F, 0);
//             }
//             else if (c == KEY_UP_ARROW)
//             {
//                 if (row > 0)
//                     row--;
//             }
//             else if (c == KEY_RIGHT_ARROW)
//             {
//                 if (col < FRAMEBUFFER_WIDTH - 1)
//                     col++;
//             }
//             else if (c == KEY_DOWN_ARROW)
//             {
//                 if (row < FRAMEBUFFER_HEIGHT - 1)
//                     row++;
//             }
//             else if (c == KEY_LEFT_ARROW)
//             {
//                 if (col > 0)
//                     col--;
//             }
//             else
//             {
//                 framebuffer_write(row, col, c, 0xF, 0);
//                 col++;
//             }
//             if (col >= FRAMEBUFFER_WIDTH)
//             {
//                 col = 0;
//                 row++;
//             }
//             if (row >= FRAMEBUFFER_HEIGHT)
//             {
//                 row = FRAMEBUFFER_HEIGHT - 1;
//             }
//             framebuffer_set_cursor(row, col);
//         }
//     }
// }

// void kernel_setup(void)
// {
//     initialize_filesystem_ext2();

//     // struct EXT2DriverRequest request1;
//     // request1.buf = NULL;
//     // request1.name = "parent";
//     // request1.name_len = 6;
//     // request1.parent_inode = 1;
//     // request1.buffer_size = 0;
//     // request1.is_directory = true;

//     // struct EXT2DriverRequest request2;
//     // request2.buf = NULL;
//     // request2.name = "parent2";
//     // request2.name_len = 7;
//     // request2.parent_inode = 1;
//     // request2.buffer_size = 0;
//     // request2.is_directory = true;

//     // uint8_t res1 = write(&request1);
//     // uint8_t res2 = write(&request2);

//     // if (res1 != 0 || res2 != 0)
//     // {
//     //     return;
//     // }

//     // struct EXT2Inode root_inode;
//     // read_inode(1, &root_inode);

//     // uint32_t parent_inode_num = find_inode_by_name(&root_inode, "parent", 6);
//     // if (parent_inode_num == 0)
//     // {
//     //     return;
//     // }

//     // char file_content[] = "Ini adalah isi file di dalam folder parent.";
//     // struct EXT2DriverRequest request_file;
//     // request_file.buf = file_content;
//     // request_file.name = "file.txt";
//     // request_file.name_len = 8;
//     // request_file.parent_inode = parent_inode_num;
//     // request_file.buffer_size = sizeof(file_content);
//     // request_file.is_directory = false;

//     // uint8_t res3 = write(&request_file);

//     // struct EXT2DriverRequest request_folder;
//     // request_folder.buf = NULL;
//     // request_folder.name = "child_folder";
//     // request_folder.name_len = 12;
//     // request_folder.parent_inode = parent_inode_num;
//     // request_folder.buffer_size = 0;
//     // request_folder.is_directory = true;

//     // uint8_t res4 = write(&request_folder);

//     // if (res3 != 0 || res4 != 0)
//     // {
//     //     return;
//     // }

//     // struct EXT2DriverRequest request_del;
//     // request_del.name = "file.txt";
//     // request_del.name_len = 8;
//     // request_del.parent_inode = parent_inode_num;
//     // request_del.is_directory = false;

//     // int8_t res_del = delete(request_del);

//     // struct EXT2Inode verify_parent;
//     // read_inode(parent_inode_num, &verify_parent);
//     // uint32_t check_file = find_inode_by_name(&verify_parent, "file.txt", 8);

//     // uint32_t check_folder = find_inode_by_name(&verify_parent, "child_folder", 12);

//     // if (check_file + check_folder == 90)
//     // {
//     //     return;
//     // }

//     // if (res_del != 0)
//     // {
//     //     return;
//     // }

//     // struct EXT2DriverRequest request_del_nonempty;
//     // request_del_nonempty.name = "parent";
//     // request_del_nonempty.name_len = 6;
//     // request_del_nonempty.parent_inode = 1;
//     // request_del_nonempty.is_directory = true;

//     // int8_t res_del_fail = delete(request_del_nonempty);
//     // if (res_del_fail != 2)
//     // {
//     //     return;
//     // }

//     // struct EXT2DriverRequest request_del_empty;
//     // request_del_empty.name = "parent2";
//     // request_del_empty.name_len = 7;
//     // request_del_empty.parent_inode = 1;
//     // request_del_empty.is_directory = true;

//     // int8_t res_del_ok = delete(request_del_empty);
//     // if (res_del_ok != 0)
//     // {
//     //     return;
//     // }
// }