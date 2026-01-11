#include "header/cmos/cmos.h"
#include "header/cpu/portio.h"

// referensi https://github.com/MuhamadAjiW/Chuu-Kawaii-OS
// referensi https://wiki.osdev.org/CMOS

/**
 * Static struct to store time information from CMOS
 * Reading should always be done to this variable
 *
 */
static cmos_reader cmos_data = {
    .century = 0,
    .second = 0,
    .minute = 0,
    .hour = 0,
    .day = 0,
    .month = 0,
    .year = 0};

uint8_t get_reg(int reg)
{
    out(CMOS_ADDR, reg);
    return in(CMOS_DATA);
}

void read_rtc()
{
    uint8_t sec, min, hr, day, mon, yr, cent;
    uint8_t nsec, nmin, nhr, nday, nmon, nyr, ncent;

    do
    {
        sec = get_reg(0x00);
        min = get_reg(0x02);
        hr = get_reg(0x04);
        day = get_reg(0x07);
        mon = get_reg(0x08);
        yr = get_reg(0x09);
        cent = get_reg(0x32);

        nsec = get_reg(0x00);
        nmin = get_reg(0x02);
        nhr = get_reg(0x04);
        nday = get_reg(0x07);
        nmon = get_reg(0x08);
        nyr = get_reg(0x09);
        ncent = get_reg(0x32);

    } while (
        sec != nsec ||
        min != nmin ||
        hr != nhr ||
        day != nday ||
        mon != nmon ||
        yr != nyr ||
        cent != ncent);

    // BCD → Binary
    sec = (sec & 0x0F) + (sec >> 4) * 10;
    min = (min & 0x0F) + (min >> 4) * 10;
    hr = (hr & 0x0F) + (hr >> 4) * 10;
    day = (day & 0x0F) + (day >> 4) * 10;
    mon = (mon & 0x0F) + (mon >> 4) * 10;
    yr = (yr & 0x0F) + (yr >> 4) * 10;
    cent = (cent & 0x0F) + (cent >> 4) * 10;

    cmos_data.second = sec;
    cmos_data.minute = min;
    cmos_data.hour = hr;
    cmos_data.day = day;
    cmos_data.month = mon;
    cmos_data.year = cent * 100 + yr; // Tahun lengkap
}

cmos_reader get_cmos_data()
{
    read_rtc();
    return cmos_data;
}