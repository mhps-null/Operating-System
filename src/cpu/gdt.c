#include "header/cpu/idt.h"
#include "header/cpu/gdt.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
// struct GlobalDescriptorTable global_descriptor_table = {
//     .table = {
//         {// null descriptor
//          .segment_low = 0,
//          .base_low = 0,
//          .base_mid = 0,
//          .type_bit = 0,
//          .non_system = 0,
//          .DPL = 0,
//          .present = 0,
//          .limit_high = 0,
//          .available = 0,
//          .long_mode = 0,
//          .default_op = 0,
//          .granularity = 0,
//          .base_high = 0},
//         {// code descriptor
//          .segment_low = 0xFFFF,
//          .base_low = 0,
//          .base_mid = 0,
//          .type_bit = 0b1010,
//          .non_system = 1,
//          .DPL = 0,
//          .present = 1,
//          .limit_high = 0xF,
//          .available = 0,
//          .long_mode = 0,
//          .default_op = 1,
//          .granularity = 1,
//          .base_high = 0},
//         {// data descriptor
//          .segment_low = 0xFFFF,
//          .base_low = 0,
//          .base_mid = 0,
//          .type_bit = 0b0010,
//          .non_system = 1,
//          .DPL = 0,
//          .present = 1,
//          .limit_high = 0xF,
//          .available = 0,
//          .long_mode = 0,
//          .default_op = 1,
//          .granularity = 1,
//          .base_high = 0}}};

/**
 * _gdt_gdtr, predefined system GDTR.
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */

static struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {.segment_low = 0, // null descriptor
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0,
         .non_system = 0,
         .DPL = 0,
         .present = 0,
         .limit_high = 0,
         .available = 0,
         .long_mode = 0,
         .default_op = 0,
         .granularity = 0,
         .base_high = 0},
        {.segment_low = 0xFFFF, // kernel code descriptor
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0b1010,
         .non_system = 1,
         .DPL = 0,
         .present = 1,
         .limit_high = 0xF,
         .available = 0,
         .long_mode = 0,
         .default_op = 1,
         .granularity = 1,
         .base_high = 0},
        {.segment_low = 0xFFFF, // kernel data descriptor
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0b0010,
         .non_system = 1,
         .DPL = 0,
         .present = 1,
         .limit_high = 0xF,
         .available = 0,
         .long_mode = 0,
         .default_op = 1,
         .granularity = 1,
         .base_high = 0},
        {.segment_low = 0xFFFF, // user code descriptor
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0b1010,
         .non_system = 1,
         .DPL = 0x3,
         .present = 1,
         .limit_high = 0xF,
         .available = 0,
         .long_mode = 0,
         .default_op = 1,
         .granularity = 1,
         .base_high = 0},
        {.segment_low = 0xFFFF, // user data descriptor
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0b0010,
         .non_system = 1,
         .DPL = 0x3,
         .present = 1,
         .limit_high = 0xF,
         .available = 0,
         .long_mode = 0,
         .default_op = 1,
         .granularity = 1,
         .base_high = 0},
        {
            .limit_high = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            .segment_low = sizeof(struct TSSEntry),
            .base_high = 0,
            .base_mid = 0,
            .base_low = 0,
            .non_system = 0, // S bit
            .type_bit = 0x9,
            .DPL = 0,         // DPL
            .present = 1,     // P bit
            .default_op = 1,  // D/B bit
            .long_mode = 0,   // L bit
            .granularity = 0, // G bit
        },
        {0}}};

void gdt_install_tss(void)
{
    uint32_t base = (uint32_t)&_interrupt_tss_entry;
    global_descriptor_table.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_descriptor_table.table[5].base_mid = (base & (0xFF << 16)) >> 16;
    global_descriptor_table.table[5].base_low = base & 0xFFFF;
}

struct GDTR _gdt_gdtr = {
    // Implement, this GDTR will point to global_descriptor_table.
    //        Use sizeof operator
    .size = sizeof(struct GlobalDescriptorTable) - 1,
    .address = &global_descriptor_table};