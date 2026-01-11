#ifndef _CMOS_H
#define _CMOS_H
// referensi https://github.com/MuhamadAjiW/Chuu-Kawaii-OS
#include <stdint.h>
#include <stdbool.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define TIMEZONE 7

/**
 * Struct to store time information from CMOS
 * Should be self explanatory
 *
 */
typedef struct __attribute__((packed))
{
    uint8_t century; // byte 0
    uint8_t second;  // byte 1
    uint8_t minute;  // byte 2
    uint8_t hour;    // byte 3
    uint8_t day;     // byte 4
    uint8_t month;   // byte 5
    uint16_t year;   // byte 6–7
} cmos_reader;

/**
 * Get values of CMOS registers
 *
 * @param reg requested register index
 *
 * @return value of requested register
 */
uint8_t get_reg(int reg);

/**
 * Update static rtc values
 *
 */
void read_rtc();

/**
 * Return values of stored static rtc values
 *
 * @return static rtc values
 */
cmos_reader get_cmos_data();

#endif //_CMOS_H