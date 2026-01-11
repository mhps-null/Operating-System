#include <stdint.h>
#include "header/stdlib/string.h"

// URUTAN REGISTER PADA SYSCALL WAJIB SEPERTI INI
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

#define SYS_PUTS 6
#define SYS_PUTC 5

int main()
{
    char states[] = "|/-\\";
    int i = 0;

    while (1)
    {
        char c = states[i % 4];
        syscall(SYS_PUTC, (uint32_t)&c, 241, 0);

        for (volatile int d = 0; d < 5000000; d++)
            ;

        char bs = '\b';
        syscall(SYS_PUTC, (uint32_t)&bs, 241, 0);

        i++;
    }
    return 0;
}