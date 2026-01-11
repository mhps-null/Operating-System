#include "header/driver/keyboard.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"

const char keyboard_scancode_1_to_ascii_map[256] = {
    0,
    0x1B,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    '\b',
    '\t',
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0,
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    '`',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0,
    '*',
    0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '-',
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

volatile bool terminate_badapple = false;
volatile bool ctrl_down = false;

static volatile struct KeyboardDriverState keyboard_state = {
    .read_extended_mode = false,
    .keyboard_input_on = false,
    .keyboard_buffer = 0};

/* -- Driver Interfaces -- */

// Activate keyboard ISR / start listen keyboard & save to buffer
void keyboard_state_activate(void)
{
  keyboard_state.keyboard_input_on = true;
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void)
{
  keyboard_state.keyboard_input_on = false;
}

void get_keyboard_buffer(char *buf)
{
  if (keyboard_state.head == keyboard_state.tail)
  {
    *buf = '\0'; // buffer kosong → tidak ada char
    return;
  }

  *buf = keyboard_state.buffer[keyboard_state.head];
  keyboard_state.head = (keyboard_state.head + 1) % KEYBOARD_RING_BUFFER_SIZE;
}

/* -- Keyboard Interrupt Service Routine -- */

static void keyboard_buffer_push(char c)
{
  uint8_t next_tail = (keyboard_state.tail + 1) % KEYBOARD_RING_BUFFER_SIZE;

  if (next_tail == keyboard_state.head)
  {
    return;
  }

  keyboard_state.buffer[keyboard_state.tail] = c;
  keyboard_state.tail = next_tail;
}

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 */
void keyboard_isr(void)
{
  uint8_t scancode = in(KEYBOARD_DATA_PORT);

  if (!keyboard_state.keyboard_input_on)
  {
    pic_ack(IRQ_KEYBOARD);
    return;
  }

  if (scancode == EXTENDED_SCANCODE_BYTE) // 0xE0
  {
    keyboard_state.read_extended_mode = true;
  }
  else if (keyboard_state.read_extended_mode)
  {
    keyboard_state.read_extended_mode = false;

    switch (scancode)
    {
    case EXT_SCANCODE_UP:           // 0x48
      keyboard_buffer_push('\033'); // Escape
      keyboard_buffer_push('[');
      keyboard_buffer_push('A');
      break;
    case EXT_SCANCODE_DOWN: // 0x50
      keyboard_buffer_push('\033');
      keyboard_buffer_push('[');
      keyboard_buffer_push('B');
      break;
    case EXT_SCANCODE_RIGHT: // 0x4D
      keyboard_buffer_push('\033');
      keyboard_buffer_push('[');
      keyboard_buffer_push('C');
      break;
    case EXT_SCANCODE_LEFT: // 0x4B
      keyboard_buffer_push('\033');
      keyboard_buffer_push('[');
      keyboard_buffer_push('D');
      break;
    }
  }
  else
  {
    if (scancode == 0x1D)
    { // Ctrl make
      ctrl_down = true;
    }
    else if (scancode == 0x9D)
    { // Ctrl break
      ctrl_down = false;
    }
    else if (scancode < 0x80)
    {
      char ascii_char = keyboard_scancode_1_to_ascii_map[scancode];
      if (ctrl_down && ascii_char == 'c')
      {
        terminate_badapple = true;
        pic_ack(IRQ_KEYBOARD);
        return;
      }
      if (scancode == 0x01)
      {
        keyboard_buffer_push('\033');
      }
      else
      {
        char ascii_char = keyboard_scancode_1_to_ascii_map[scancode];
        if (ascii_char != 0)
        {
          keyboard_buffer_push(ascii_char);
        }
      }
    }
  }

  pic_ack(IRQ_KEYBOARD);
};