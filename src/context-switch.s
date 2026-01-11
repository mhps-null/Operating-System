global process_context_switch

; void process_context_switch(struct ProcessContext ctx);
; ctx dikirim by value, berada di stack mulai dari [esp+4]

process_context_switch:
    ; Simpan esp awal sebagai base untuk mengakses ctx
    mov     ebp, esp              ; ebp = esp_entry

    ; --- Ambil nilai kontrol untuk iret lebih dulu ---
    ; ctx.cpu.stack.esp (user ESP) di offset 0x0C
    mov     eax, [ebp + 4 + 0x0C] ; eax = user ESP

    ; ctx.eip di offset 0x30
    mov     edx, [ebp + 4 + 0x30] ; edx = EIP

    ; ctx.eflags di offset 0x34
    mov     ecx, [ebp + 4 + 0x34] ; ecx = EFLAGS

    ; --- Susun iret frame: SS, ESP, EFLAGS, CS, EIP ---
    push    dword (0x20 | 0x3)    ; SS = GDT_USER_DATA | RPL=3
    push    eax                   ; ESP (user stack)
    push    ecx                   ; EFLAGS
    push    dword (0x18 | 0x3)    ; CS = GDT_USER_CODE | RPL=3
    push    edx                   ; EIP

    ; --- Load segment registers dari ctx ---
    ; urutan offset sesuai struct kamu:
    ; gs: 0x20, fs: 0x24, es: 0x28, ds: 0x2C

    mov     bx, [ebp + 4 + 0x2C]  ; ds
    mov     ds, bx
    mov     bx, [ebp + 4 + 0x28]  ; es
    mov     es, bx
    mov     bx, [ebp + 4 + 0x24]  ; fs
    mov     fs, bx
    mov     bx, [ebp + 4 + 0x20]  ; gs
    mov     gs, bx

    ; --- Load general & index registers ---
    ; edi:0x00, esi:0x04, ebp:0x08, esp:0x0C (sudah dipakai untuk iret),
    ; ebx:0x10, edx:0x14, ecx:0x18, eax:0x1C

    mov     edi, [ebp + 4 + 0x00]
    mov     esi, [ebp + 4 + 0x04]
    mov     ebp, [ebp + 4 + 0x08] ; ebp user (base gak dipakai lagi s/d iret)
    mov     ebx, [esp + 4 + 0x10] ; boleh juga pakai [ebp+4+0x10] kalau mau konsisten
    mov     edx, [esp + 4 + 0x14]
    mov     ecx, [esp + 4 + 0x18]
    mov     eax, [esp + 4 + 0x1C]

    ; --- Lompat ke user ---
    iret