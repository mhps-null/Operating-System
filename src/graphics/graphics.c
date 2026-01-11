// graphics.c
#include "header/graphics/graphics.h"
#include "header/cpu/portio.h"
#include "header/graphics/font.h"
#include "header/graphics/wallpaper.h"
#include "header/stdlib/string.h"

// Palette (Brightness: 100%)
const uint8_t wallpaper_palette[768] = {
54, 41, 23, 26, 38, 40, 37, 38, 23, 51, 39, 38, 
    11, 25, 37, 21, 25, 38, 36, 40, 50, 24, 19, 6, 
    57, 50, 24, 25, 35, 49, 10, 21, 27, 36, 22, 36, 
    48, 30, 13, 47, 30, 36, 48, 39, 14, 13, 18, 7, 
    25, 34, 23, 40, 51, 36, 19, 7, 8, 32, 11, 34, 
    39, 42, 41, 24, 51, 35, 32, 11, 20, 15, 11, 31, 
    38, 39, 15, 41, 50, 55, 30, 32, 13, 22, 50, 29, 
    32, 21, 9, 37, 30, 47, 57, 51, 39, 11, 2, 7, 
    17, 7, 23, 55, 45, 57, 58, 63, 54, 3, 9, 11, 
    4, 13, 12, 60, 56, 57, 59, 54, 55, 3, 5, 10, 
    3, 13, 16, 57, 52, 54, 6, 5, 17, 2, 6, 6, 
    5, 17, 12, 3, 9, 6, 8, 6, 19, 44, 29, 36, 
    3, 17, 18, 19, 13, 34, 11, 9, 20, 3, 3, 12, 
    34, 14, 22, 18, 12, 28, 42, 26, 34, 53, 48, 51, 
    13, 9, 25, 25, 10, 20, 28, 12, 21, 56, 50, 51, 
    36, 25, 35, 34, 15, 23, 20, 10, 19, 35, 16, 25, 
    27, 17, 27, 34, 22, 33, 44, 33, 41, 45, 32, 38, 
    51, 45, 49, 5, 20, 13, 19, 16, 33, 35, 21, 28, 
    11, 13, 18, 19, 13, 20, 43, 37, 43, 51, 53, 53, 
    10, 9, 13, 3, 10, 16, 3, 13, 6, 53, 56, 57, 
    49, 37, 42, 16, 11, 28, 50, 41, 44, 27, 14, 25, 
    28, 21, 34, 9, 17, 18, 9, 13, 13, 19, 17, 26, 
    11, 21, 18, 28, 21, 29, 44, 41, 44, 59, 59, 60, 
    52, 54, 56, 35, 25, 13, 35, 29, 17, 35, 29, 35, 
    47, 34, 40, 37, 33, 19, 51, 44, 45, 10, 17, 13, 
    29, 25, 13, 10, 6, 12, 59, 53, 28, 36, 29, 41, 
    43, 37, 48, 5, 20, 16, 44, 44, 17, 27, 25, 28, 
    45, 43, 25, 35, 33, 36, 12, 13, 27, 21, 32, 26, 
    28, 25, 34, 19, 17, 21, 51, 33, 17, 28, 21, 12, 
    41, 37, 20, 43, 42, 24, 49, 42, 49, 20, 29, 24, 
    37, 33, 42, 19, 28, 22, 43, 41, 18, 34, 24, 29, 
    0, 2, 5, 29, 25, 17, 44, 41, 49, 27, 18, 33, 
    20, 21, 27, 28, 29, 34, 41, 30, 40, 26, 16, 21, 
    3, 16, 7, 50, 45, 20, 34, 12, 36, 44, 45, 49, 
    42, 37, 37, 48, 32, 37, 26, 9, 29, 18, 25, 20, 
    42, 37, 13, 18, 9, 14, 34, 28, 13, 37, 36, 20, 
    36, 37, 42, 42, 46, 35, 12, 17, 27, 29, 10, 32, 
    34, 16, 22, 34, 25, 40, 20, 25, 27, 19, 21, 13, 
    41, 33, 19, 56, 36, 20, 42, 24, 30, 42, 29, 12, 
    43, 40, 14, 51, 41, 18, 34, 22, 12, 49, 32, 14, 
    52, 52, 38, 32, 26, 18, 53, 49, 56, 5, 13, 26, 
    19, 13, 13, 43, 33, 12, 53, 53, 40, 56, 49, 26, 
    10, 9, 4, 41, 29, 17, 24, 34, 27, 52, 36, 19, 
    13, 21, 27, 40, 22, 29, 50, 49, 37, 18, 17, 13, 
    26, 18, 10, 28, 33, 35, 55, 45, 45, 56, 49, 55, 
    60, 54, 32, 36, 36, 36, 44, 48, 51, 53, 49, 26, 
    11, 21, 35, 19, 21, 19, 20, 25, 34, 29, 25, 41, 
    27, 37, 49, 28, 21, 19, 45, 44, 45, 42, 49, 57, 
    61, 56, 33, 9, 6, 6, 13, 13, 5, 21, 6, 27, 
    18, 29, 34, 30, 28, 13, 26, 29, 28, 36, 28, 29, 
    50, 49, 42, 20, 32, 35, 36, 41, 44, 41, 26, 11, 
    56, 46, 48, 10, 18, 32, 35, 32, 13, 51, 45, 55, 
    16, 6, 12, 20, 29, 43, 24, 6, 29, 28, 29, 18, 
    26, 33, 43, 42, 34, 48, 50, 47, 22, 4, 10, 24, 
    11, 2, 10, 33, 41, 52, 60, 55, 54, 12, 20, 13, 
    14, 24, 20, 18, 13, 5, 21, 24, 12, 36, 45, 56, 
    45, 52, 58, 48, 38, 48, 5, 3, 16, 13, 24, 35, 
    24, 13, 13, 36, 37, 14, 42, 29, 27, 43, 33, 29, 
    41, 37, 25, 56, 48, 45, 10, 6, 24, 17, 10, 32, 
    28, 22, 40, 33, 21, 21, 37, 33, 48, 40, 22, 32, 
    63,63,63,   // WHITE (240)
    0,31,63,    // BLUE_LT (241)
    0,63,0,     // GREEN (242)
    63,0,0,     // RED (243)
    0,63,63,    // CYAN_LT (244)
    24,24,24,   // GRAY_DK (245)
    63,63,0,    // YELLOW (246)
    0,0,0       // BLACK (247)
    };


// -----------------------
// VGA / Palette init
// -----------------------
void initialize_vga()
{
    out(MISC_OUT_REG, 0x63);
    out16(CRT_ADDR_REG, (0x0e << 8) | 0x11);

    out16(CRT_ADDR_REG, (0x5f << 8) | 0x0);
    out16(CRT_ADDR_REG, (0x4f << 8) | 0x1);
    out16(CRT_ADDR_REG, (0x50 << 8) | 0x2);
    out16(CRT_ADDR_REG, (0x82 << 8) | 0x3);
    out16(CRT_ADDR_REG, (0x54 << 8) | 0x4);
    out16(CRT_ADDR_REG, (0x80 << 8) | 0x5);
    out16(CRT_ADDR_REG, (0xbf << 8) | 0x6);
    out16(CRT_ADDR_REG, (0x1f << 8) | 0x7);
    out16(CRT_ADDR_REG, (0x00 << 8) | 0x8);
    out16(CRT_ADDR_REG, (0x41 << 8) | 0x9);
    out16(CRT_ADDR_REG, (0x9c << 8) | 0x10);
    out16(CRT_ADDR_REG, (0x8e << 8) | 0x11);
    out16(CRT_ADDR_REG, (0x8f << 8) | 0x12);
    out16(CRT_ADDR_REG, (0x28 << 8) | 0x13);
    out16(CRT_ADDR_REG, (0x40 << 8) | 0x14);
    out16(CRT_ADDR_REG, (0x96 << 8) | 0x15);
    out16(CRT_ADDR_REG, (0xb9 << 8) | 0x16);
    out16(CRT_ADDR_REG, (0xa3 << 8) | 0x17);

    out16(MEM_MODE_REG, (0x01 << 8) | 0x01);
    out16(MEM_MODE_REG, (0x0f << 8) | 0x02);
    out16(MEM_MODE_REG, (0x00 << 8) | 0x03);
    out16(MEM_MODE_REG, (0x0e << 8) | 0x04);

    out16(GRAP_ADDR_REG, (0x40 << 8) | 0x05);
    out16(GRAP_ADDR_REG, (0x05 << 8) | 0x06);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x10);
    out16(ATTR_ADDR_REG, 0x41);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x11);
    out16(ATTR_ADDR_REG, 0x00);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x12);
    out16(ATTR_ADDR_REG, 0x0f);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x13);
    out16(ATTR_ADDR_REG, 0x00);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x14);
    out16(ATTR_ADDR_REG, 0x00);

    out(DAC_WRITE_REG, 0);
    for (int i = 0; i < 768; ++i)
    {
        out(DAC_DATA_REG, wallpaper_palette[i]);
    }

    out(ATTR_ADDR_REG, 0x20);
}

// -----------------------
// Buffers and state
// -----------------------
static uint8_t backBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

// character-cell buffers
static char screenChar[SCREEN_HEIGHT / BLOCK_HEIGHT][SCREEN_WIDTH / BLOCK_WIDTH];
static uint8_t screenColor[SCREEN_HEIGHT / BLOCK_HEIGHT][SCREEN_WIDTH / BLOCK_WIDTH];

static uint8_t colLimit = SCREEN_WIDTH / BLOCK_WIDTH;
static uint8_t rowLimit = SCREEN_HEIGHT / BLOCK_HEIGHT;
static uint8_t rowDisplayLimit = (SCREEN_HEIGHT / BLOCK_HEIGHT) - 1;

static uint8_t graphics_cursor_row = 0;
static uint8_t graphics_cursor_col = 0;

// -----------------------
// Low-level buffer ops
// -----------------------
void clear_memory_graphics()
{
    // copy wallpaper as initial visible framebuffer
    memcpy(MEMORY_GRAPHICS, wallpaper, sizeof(backBuffer));
}

void clear_back_buffer()
{
    // copy wallpaper to back buffer so everything starts with wallpaper pixels
    memcpy(backBuffer, wallpaper, sizeof(backBuffer));
}

void clear_screen()
{
    // 1. Reset backBuffer & VRAM ke wallpaper
    clear_back_buffer();
    clear_memory_graphics();

    // 2. Clear logical text buffers
    for (uint8_t y = 0; y < rowLimit; y++)
    {
        for (uint8_t x = 0; x < colLimit; x++)
        {
            screenChar[y][x] = 0;
            screenColor[y][x] = DEFAULT_COLOR_BG;
        }
    }

    // 3. Reset cursor position
    graphics_cursor_row = 0;
    graphics_cursor_col = 0;

    // 4. Draw cursor & swap
    draw_cursor();
    swap_buffer();
}

void swap_buffer()
{
    // copy backBuffer into video memory
    memcpy(MEMORY_GRAPHICS, backBuffer, sizeof(backBuffer));
}

void graphics_set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;
    backBuffer[y * SCREEN_WIDTH + x] = color;
}

void graphics_set_block(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= colLimit || y >= rowLimit)
        return;

    uint16_t startX = x * BLOCK_WIDTH;
    uint16_t startY = y * BLOCK_HEIGHT;
    for (uint8_t dx = 0; dx < BLOCK_WIDTH; ++dx)
    {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; ++dy)
        {
            graphics_set_pixel(startX + dx, startY + dy, color);
        }
    }
}

// -----------------------
// Cursor
// -----------------------
void graphics_set_cursor(uint8_t x, uint8_t y)
{
    if (x >= colLimit || y >= rowLimit)
        return;
    graphics_cursor_col = x;
    graphics_cursor_row = y;
}

void draw_cursor()
{
    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;
    for (uint8_t dy = 0; dy < BLOCK_HEIGHT; ++dy)
    {
        graphics_set_pixel(startX, startY + dy, CURSOR_COLOR);
    }
}

void erase_cursor()
{
    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;
    for (uint8_t dy = 0; dy < BLOCK_HEIGHT; ++dy)
    {
        // restore pixel from wallpaper
        graphics_set_pixel(startX, startY + dy, wallpaper[(startY + dy) * SCREEN_WIDTH + startX]);
    }
}

// -----------------------
// Character rendering
// -----------------------
void graphics_write_char(uint8_t x, uint8_t y, char c, uint8_t color)
{
    if (x >= colLimit || y >= rowLimit)
        return;

    uint8_t index = (uint8_t)c;
    uint16_t blockStartX = x * BLOCK_WIDTH;
    uint16_t blockStartY = y * BLOCK_HEIGHT;

    // store in cell buffers
    screenChar[y][x] = c;
    screenColor[y][x] = color;

    // first copy background (from wallpaper) for that block into backBuffer
    for (uint8_t dx = 0; dx < BLOCK_WIDTH; ++dx)
    {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; ++dy)
        {
            uint16_t px = blockStartX + dx;
            uint16_t py = blockStartY + dy;
            graphics_set_pixel(px, py, wallpaper[py * SCREEN_WIDTH + px]);
        }
    }

    // draw glyph pixels using lookup table; lookup[index][0] contains count
    for (int i = 1; i < lookup[index][0]; ++i)
    {
        uint8_t px = lookup[index][i] >> 4;
        uint8_t py = lookup[index][i] & 0x0F;
        graphics_set_pixel(blockStartX + px, blockStartY + py, color);
    }
}

void graphics_write_char_default(uint8_t x, uint8_t y, char c)
{
    graphics_write_char(x, y, c, DEFAULT_COLOR_FG);
}

void graphics_write_string(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    if (x >= colLimit || y >= rowLimit)
        return;

    uint8_t cx = x;
    uint8_t cy = y;
    const char *p = str;

    while (*p)
    {
        if (*p == '\n')
        {
            cx = 0;
            cy++;
            if (cy >= rowLimit)
                break;
        }
        else
        {
            graphics_write_char(cx, cy, *p, color);
            cx++;
            if (cx >= colLimit)
            {
                cx = 0;
                cy++;
                if (cy >= rowLimit)
                    break;
            }
        }
        ++p;
    }
    swap_buffer();
}

void graphics_write_string_default(uint8_t x, uint8_t y, const char *str)
{
    graphics_write_string(x, y, str, DEFAULT_COLOR_FG);
}

// -----------------------
// Terminal-style stream ops
// -----------------------
void graphics_backspace()
{
    // erase cursor, move back, clear block and redraw from wallpaper
    erase_cursor();

    if (graphics_cursor_col > 0)
    {
        graphics_cursor_col--;
    }
    else if (graphics_cursor_row > 0)
    {
        graphics_cursor_row--;
        graphics_cursor_col = colLimit - 1;
    }

    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;

    // restore wallpaper for that block
    for (uint8_t dx = 0; dx < BLOCK_WIDTH; ++dx)
    {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; ++dy)
        {
            uint16_t px = startX + dx;
            uint16_t py = startY + dy;
            graphics_set_pixel(px, py, wallpaper[py * SCREEN_WIDTH + px]);
        }
    }

    // clear char/color buffer
    screenChar[graphics_cursor_row][graphics_cursor_col] = 0;
    screenColor[graphics_cursor_row][graphics_cursor_col] = DEFAULT_COLOR_BG;

    swap_buffer();
}

void graphics_scroll()
{
    // move logical buffers up one row
    for (uint8_t y = 1; y < rowDisplayLimit; ++y)
    {
        for (uint8_t x = 0; x < colLimit; ++x)
        {
            screenChar[y - 1][x] = screenChar[y][x];
            screenColor[y - 1][x] = screenColor[y][x];
        }
    }

    // clear last row
    for (uint8_t x = 0; x < colLimit; ++x)
    {
        screenChar[rowDisplayLimit - 1][x] = 0;
        screenColor[rowDisplayLimit - 1][x] = DEFAULT_COLOR_BG;
    }

    // repaint full backBuffer from wallpaper for visible rows
    for (uint8_t y = 0; y < rowLimit; ++y)
    {
        uint16_t by = y * BLOCK_HEIGHT;
        for (uint8_t x = 0; x < colLimit; ++x)
        {
            uint16_t bx = x * BLOCK_WIDTH;
            for (uint8_t dx = 0; dx < BLOCK_WIDTH; ++dx)
            {
                for (uint8_t dy = 0; dy < BLOCK_HEIGHT; ++dy)
                {
                    uint16_t px = bx + dx;
                    uint16_t py = by + dy;
                    graphics_set_pixel(px, py, wallpaper[py * SCREEN_WIDTH + px]);
                }
            }
        }
    }

    // redraw characters from buffers
    for (uint8_t y = 0; y < rowLimit; ++y)
    {
        for (uint8_t x = 0; x < colLimit; ++x)
        {
            if (screenChar[y][x] != 0)
                graphics_write_char(x, y, screenChar[y][x], screenColor[y][x]);
        }
    }

    // set cursor to last row start
    graphics_cursor_row = rowDisplayLimit - 1;
    graphics_cursor_col = 0;

    swap_buffer();
}

void graphics_putchar(char c, uint8_t color)
{
    // erase old cursor image
    erase_cursor();

    if (c == '\n')
    {
        graphics_cursor_col = 0;
        graphics_cursor_row++;
        if (graphics_cursor_row >= rowDisplayLimit)
            graphics_scroll();
    }
    else if (c == '\r')
    {
        graphics_cursor_col = 0;
    }
    else if (c == '\b')
    {
        graphics_backspace();
        draw_cursor();
        swap_buffer();
        return;
    }
    else if ((uint8_t)c >= 32 && (uint8_t)c <= 126)
    {
        graphics_write_char(graphics_cursor_col, graphics_cursor_row, c, color);
        graphics_cursor_col++;
        if (graphics_cursor_col >= colLimit)
        {
            graphics_cursor_col = 0;
            graphics_cursor_row++;
            if (graphics_cursor_row >= rowDisplayLimit)
                graphics_scroll();
        }
    }

    // draw cursor and present
    draw_cursor();
    swap_buffer();
}

void graphics_puts(const char *str, uint8_t color)
{
    if (str == NULL)
        return;
    const char *p = str;
    while (*p)
    {
        graphics_putchar(*p, color);
        ++p;
    }
}

void graphics_flush()
{
    memcpy(MEMORY_GRAPHICS, backBuffer, sizeof(backBuffer));
}