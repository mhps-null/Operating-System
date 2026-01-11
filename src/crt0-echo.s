global _start
extern main

section .text
_start:
    ; Kernel stack setup:
    ; ESP -> argc
    ; ESP+4 -> argv[0] 
    ; ESP+8 -> argv[1]
    ; ...
    ; ESP+4*(argc+1) -> NULL
    
    ; Base pointer
    mov ebp, esp
    
    ; Clear registers
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    
    ; Call main function
    call main
    
    ; Exit program (terminate process)
    mov eax, 21        ; Syscall number for process termination
    mov ebx, 19         ; Exit code
    int 0x30
    
    ; Is it possible to reach here? idk.
    cli
    hlt