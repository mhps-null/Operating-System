#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/cpu/portio.h"

static uint8_t g_cursor_row = 0;
static uint8_t g_cursor_col = 0;

static void scroll_screen(void)
{
    volatile uint16_t *vga_mem = (volatile uint16_t *)FRAMEBUFFER_MEMORY_OFFSET;

    for (uint8_t r = 1; r < FRAMEBUFFER_HEIGHT - 1; r++)
    {
        for (uint8_t c = 0; c < FRAMEBUFFER_WIDTH; c++)
        {
            vga_mem[(r - 1) * FRAMEBUFFER_WIDTH + c] =
                vga_mem[r * FRAMEBUFFER_WIDTH + c];
        }
    }

    uint16_t blank = (' ' | (((DEFAULT_BG_COLOR << 4) | DEFAULT_FG_COLOR) << 8));
    uint8_t terminal_last_row = FRAMEBUFFER_HEIGHT - 2;

    for (uint8_t c = 0; c < FRAMEBUFFER_WIDTH; c++)
    {
        vga_mem[terminal_last_row * FRAMEBUFFER_WIDTH + c] = blank;
    }

    g_cursor_row = terminal_last_row;
    g_cursor_col = 0;
}

void framebuffer_set_cursor(uint8_t r, uint8_t c)
{
    uint16_t pos = r * FRAMEBUFFER_WIDTH + c;

    out(CURSOR_PORT_CMD, 0x0F);
    out(CURSOR_PORT_DATA, (uint8_t)(pos & 0xFF));
    out(CURSOR_PORT_CMD, 0x0E);
    out(CURSOR_PORT_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg)
{
    uint16_t pos = (row * FRAMEBUFFER_WIDTH + col) * 2;
    uint8_t color = (bg << 4) | (fg & 0x0F);

    FRAMEBUFFER_MEMORY_OFFSET[pos] = c;
    FRAMEBUFFER_MEMORY_OFFSET[pos + 1] = color;
}

void framebuffer_clear(void)
{
    uint8_t fg = DEFAULT_FG_COLOR;
    uint8_t bg = DEFAULT_BG_COLOR;

    for (uint8_t row = 0; row < FRAMEBUFFER_HEIGHT; row++)
    {
        for (uint8_t col = 0; col < FRAMEBUFFER_WIDTH; col++)
        {
            framebuffer_write(row, col, ' ', fg, bg);
        }
    }

    framebuffer_set_cursor(0, 0);

    g_cursor_row = 0;
    g_cursor_col = 0;
}

void puts(char *str, uint8_t attribute)
{
    int i = 0;
    while (str[i] != '\0')
    {
        putc(str[i], attribute);
        i++;
    }
}

void putc(char c, uint8_t attribute)
{
    uint8_t fg = attribute & 0x0F;
    uint8_t bg = (attribute & 0xF0) >> 4;

    if (c == '\b')
    {
        if (g_cursor_col > 0)
        {
            g_cursor_col--;
        }
        else if (g_cursor_row > 0)
        {
            g_cursor_row--;
            g_cursor_col = FRAMEBUFFER_WIDTH - 1;
        }
    }
    else if (c == '\n')
    {
        g_cursor_col = 0;
        g_cursor_row++;
    }
    else
    {
        framebuffer_write(g_cursor_row, g_cursor_col, c, fg, bg);
        g_cursor_col++;
    }

    if (g_cursor_col >= FRAMEBUFFER_WIDTH)
    {
        g_cursor_col = 0;
        g_cursor_row++;
    }

    if (g_cursor_row >= FRAMEBUFFER_HEIGHT - 1)
    {
        scroll_screen();
        g_cursor_row = FRAMEBUFFER_HEIGHT - 2;
    }

    framebuffer_set_cursor(g_cursor_row, g_cursor_col);
}

void framebuffer_write_at(int row, int col, char c, uint8_t color)
{
    if (row < 0 || row >= 25 || col < 0 || col >= 80)
        return;

    volatile uint16_t *buffer = (volatile uint16_t *)FRAMEBUFFER_MEMORY_OFFSET;

    uint16_t entry = (uint16_t)c | ((uint16_t)color << 8);
    buffer[row * 80 + col] = entry;
}

void framebuffer_puts_at(int row, int col, char *str, uint8_t color)
{
    int i = 0;
    while (str[i] != '\0')
    {
        framebuffer_write_at(row, col + i, str[i], color);
        i++;
    }
}