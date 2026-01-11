#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/cmos/cmos.h"

#define TOTAL_BLOCKS_X 64 // 64
#define TOTAL_BLOCKS_Y 25 // 25

// URUTAN REGISTER PADA SYSCALL WAJIB SEPERTI INI
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

static inline void int_to_str_zero_pad(uint8_t n, char *buf)
{
    buf[0] = (n / 10) + '0';
    buf[1] = (n % 10) + '0';
    buf[2] = 0;
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
    cmos_reader t;
    char time_str[9];
    int len, col;

    while (1)
    {
        syscall(17, (uint32_t)&t, 0, 0);

        uint8_t sec = t.second;
        uint8_t min = t.minute;
        uint8_t hour = (t.hour + 7) % 24;

        int_to_str_zero_pad(hour, time_str);
        time_str[2] = ':';
        int_to_str_zero_pad(min, time_str + 3);
        time_str[5] = ':';
        int_to_str_zero_pad(sec, time_str + 6);

        len = strlen(time_str);
        col = TOTAL_BLOCKS_X - len;

        syscall(SYS_PUTS_AT, 24, col, (uint32_t)time_str);
    }

    return 0;
}