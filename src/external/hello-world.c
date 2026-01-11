#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/graphics/graphics.h"

// BELUM GW HANDLE MAKEFILENYA HEHE
// URUTAN REGISTER PADA SYSCALL WAJIB SEPERTI INI
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

enum SYSCALL_NUMBERS
{
    SYS_READ = 0,         // read(path, buffer, retcode)
    SYS_GET_KB = 4,       // get_keyboard_buffer(char* buf)
    SYS_PUTC = 5,         // putc(char, color)
    SYS_PUTS = 6,         // puts(str, color)
    SYS_KBACT = 7,        // activate_keyboard()
    SYS_CLEAR = 10,       // framebuffer_clear()
    SYS_LS = 11,          // ls(path, buffer, retcode)
    SYS_STAT = 12,        // stat(path, stat_buf, retcode)
    SYS_MKDIR = 13,       // mkdir(path, new_name, retcode)
    SYS_WRITE = 14,       // write(path, buffer, size, retcode)
    SYS_RM = 15,          // rm(path, retcode)
    SYS_RENAME = 16,      // rename(old_path, new_path, retcode)
    SYS_CMOS = 17,        // cmos_read(retcode)
    SYS_EXEC = 18,        // exec(path, 0, retcode)
    SYS_EXIT = 19,        // exit() - Shell jarang memanggil ini langsung kecuali exit shell
    SYS_GET_INFO = 20,    // get_info(0, 0, retcode_pid)
    SYS_DESTROY = 21,     // destroy(pid, 0, retcode)
    SYS_PUTS_AT = 22,     // puts_at(row, col, str, color)
    SYS_GETPIDBYNAME = 23 // get_pid_by_name(name, 0, retcode)
};

int main(void)
{
    syscall(SYS_PUTS, (uint32_t)"Hello world!\n", COLOR_WHITE, 0);
}
