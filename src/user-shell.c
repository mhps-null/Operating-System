#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"

#define BLOCK_COUNT 16
#define MAX_PATH_LEN 1024
#define MAX_CMD_LEN (MAX_PATH_LEN * 2 + MAX_ARGS)
#define MAX_ARGS 16
#define FILE_BUFFER_SIZE 4096

#define MAX_HISTORY 16

// Variabel Global untuk Command History
static char g_history[MAX_HISTORY][MAX_CMD_LEN];
static int g_history_count = 0;

// 240–255 dipakai untuk warna teks (diset di Python)
#define COLOR_WHITE 240   // 0x0F
#define COLOR_BLUE_LT 241 // 0x0B
#define COLOR_GREEN 242   // 0x0A
#define COLOR_RED 243     // 0x0C
#define COLOR_CYAN_LT 244 // 0x03
#define COLOR_GRAY_DK 245 // 0x08
#define COLOR_YELLOW 246  // 0x0E
#define COLOR_BLACK 247   // 0x00

// Remap warna LAMA ke palette BARU
#define COLOR_FILE COLOR_WHITE
#define COLOR_FOLDER COLOR_BLUE_LT
#define COLOR_PROMPT COLOR_BLUE_LT
#define COLOR_TEXT COLOR_WHITE

#define DEFAULT_COLOR_FG COLOR_WHITE
#define DEFAULT_COLOR_BG COLOR_BLACK

enum SYSCALL_NUMBERS
{
    SYS_READ = 0,             // read(path, buffer, retcode)
    SYS_GET_KB = 4,           // get_keyboard_buffer(char* buf)
    SYS_PUTC = 5,             // putc(char, color)
    SYS_PUTS = 6,             // puts(str, color)
    SYS_KBACT = 7,            // activate_keyboard()
    SYS_CLEAR = 10,           // framebuffer_clear()
    SYS_LS = 11,              // ls(path, buffer, retcode)
    SYS_STAT = 12,            // stat(path, stat_buf, retcode)
    SYS_MKDIR = 13,           // mkdir(path, new_name, retcode)
    SYS_WRITE = 14,           // write(path, buffer, size, retcode)
    SYS_RM = 15,              // rm(path, retcode)
    SYS_RENAME = 16,          // rename(old_path, new_path, retcode)
    SYS_CMOS = 17,            // cmos_read(retcode)
    SYS_EXEC = 18,            // exec(path, 0, retcode)
    SYS_EXIT = 19,            // exit() - Shell jarang memanggil ini langsung kecuali exit shell
    SYS_GET_INFO = 20,        // get_info(0, 0, retcode_pid)
    SYS_DESTROY = 21,         // destroy(pid, 0, retcode)
    SYS_PUTS_AT = 22,         // puts_at(row, col, str, color)
    SYS_GETPIDBYNAME = 23,    // get_pid_by_name(name, 0, retcode)
    SYS_BADAPPLE = 24,        // bad_apple(frame_buffer, width, height)
    SYS_SLEEP = 25,           // sleep(milliseconds)
    SYS_CHECK_TERMINATE = 26, // check_terminate_badapple(retcode)
    SYS_RESET_TERMINAL = 27   // reset_terminal()
};

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

char g_cwd[MAX_PATH_LEN]; // current working directory
char newline = '\n';
char backspace = '\b';
char space = ' ';

static int matchstar(int c, const char *regexp, const char *text);
static int matchhere(const char *regexp, const char *text);

void build_full_path(char *full_path, const char *path)
{
    if (path[0] == '/')
    {
        strcpy(full_path, path);
    }
    else
    {
        strcpy(full_path, g_cwd);
        if (strcmp(g_cwd, "/") != 0)
        {
            strcat(full_path, "/");
        }
        strcat(full_path, path);
    }
}

uint32_t str_to_uint(const char *str)
{
    uint32_t result = 0;
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

// ==================== 3. COMMAND HANDLERS ====================

void handle_exec(int argc, char *argv[])
{
    if (argc != 2)
    {
        syscall(SYS_PUTS, (uint32_t)"Penggunaan: exec <path_program>\n", COLOR_RED, 0);
        return;
    }

    char full_path[MAX_PATH_LEN];
    // Menggunakan fungsi build_full_path yang sudah ada di shell.c
    build_full_path(full_path, argv[1]);

    int32_t retcode = -1;
    // Syscall 18: ebx = path
    syscall(SYS_EXEC, (uint32_t)full_path, 0, (uint32_t)&retcode);

    if (retcode == 0)
    {
        syscall(SYS_PUTS, (uint32_t)"Proses berhasil dibuat.\n", COLOR_GREEN, 0); // Hijau
    }
    else if (retcode == -2)
    {
        syscall(SYS_PUTS, (uint32_t)"Proses sudah berjalan. Duplicate tidak diperbolehkan.\n", COLOR_RED, 0);
        return;
    }
    else
    {
        syscall(SYS_PUTS, (uint32_t)"Gagal membuat proses (Error Code: ", COLOR_RED, 0);

        char err_buf[16];
        int_to_str((uint32_t)retcode, err_buf); // Tampilkan kode error (misal 1, 2, 3)
        syscall(SYS_PUTS, (uint32_t)err_buf, COLOR_RED, 0);

        syscall(SYS_PUTS, (uint32_t)")\n", COLOR_RED, 0);
    }
}
typedef enum PROCESS_STATE
{
    PROCESS_STATE_UNUSED,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_READY,
    PROCESS_STATE_BLOCKED
} PROCESS_STATE;

// 2. Copy struct ProcessInfo
// Pastikan urutan dan tipe datanya PERSIS sama dengan di kernel
typedef struct
{
    uint32_t pid;
    PROCESS_STATE state;
    char name[32]; // Pointer ke memori kernel
    uint8_t name_len;
} ProcessInfo;

// =============================================================
// HELPER DAN FUNGSI SHELL LAINNYA
// =============================================================

// Helper untuk mengubah state menjadi string
const char *get_process_state_str(PROCESS_STATE state)
{
    switch (state)
    {
    case PROCESS_STATE_RUNNING:
        return "RUNNING";
    case PROCESS_STATE_READY:
        return "READY  ";
    case PROCESS_STATE_BLOCKED:
        return "BLOCKED";
    default:
        return "UNKNOWN";
    }
}

void handle_ps(void)
{
    ProcessInfo proc_list[16];
    int32_t process_count = 0;

    syscall(SYS_GET_INFO, (uint32_t)proc_list, 16, (uint32_t)&process_count);

    if (process_count < 0)
    {
        syscall(SYS_PUTS, (uint32_t)"Error getting process info.\n", COLOR_RED, 0);
        return;
    }

    syscall(SYS_PUTS, (uint32_t)"PID  | STATE    | NAME\n", COLOR_BLUE_LT, 0);
    syscall(SYS_PUTS, (uint32_t)"-----+----------+------------------\n", COLOR_GRAY_DK, 0);

    char pid_buf[16];

    for (int i = 0; i < process_count; i++)
    {
        ProcessInfo proc = proc_list[i];

        uint_to_str(proc.pid, pid_buf);
        syscall(SYS_PUTS, (uint32_t)pid_buf, COLOR_WHITE, 0);

        int pid_len = strlen(pid_buf);
        for (int j = pid_len; j < 5; j++)
            syscall(SYS_PUTC, (uint32_t)&space, COLOR_WHITE, 0);
        syscall(SYS_PUTS, (uint32_t)"| ", COLOR_GRAY_DK, 0);

        const char *state_str = get_process_state_str(proc.state);
        syscall(SYS_PUTS, (uint32_t)state_str, COLOR_YELLOW, 0);
        syscall(SYS_PUTS, (uint32_t)"  | ", COLOR_GRAY_DK, 0);

        syscall(SYS_PUTS, (uint32_t)proc.name, COLOR_WHITE, 0);

        syscall(SYS_PUTC, (uint32_t)&newline, COLOR_WHITE, 0);
    }
}

static bool is_number(const char *s)
{
    if (*s == '\0')
        return false;
    for (int i = 0; s[i]; i++)
        if (s[i] < '0' || s[i] > '9')
            return false;
    return true;
}

void handle_kill(int argc, char *argv[])
{
    if (argc != 2)
    {
        syscall(SYS_PUTS, (uint32_t)"Penggunaan: kill <pid | nama>\n", COLOR_RED, 0);
        return;
    }

    uint32_t target_pid = 0;

    if (is_number(argv[1]))
    {
        target_pid = str_to_uint(argv[1]);
    }
    else
    {
        syscall(SYS_GETPIDBYNAME, (uint32_t)argv[1], (uint32_t)&target_pid, 0);
    }

    if (target_pid == 0)
    {
        syscall(SYS_PUTS, (uint32_t)"PID tidak valid atau proses tidak ditemukan.\n", COLOR_RED, 0);
        return;
    }

    int32_t retcode = -1;
    syscall(SYS_DESTROY, target_pid, 0, (uint32_t)&retcode);

    if (retcode == 0)
    {
        syscall(SYS_PUTS, (uint32_t)"Proses dihentikan.\n", COLOR_GREEN, 0);
    }
    else
    {
        syscall(SYS_PUTS, (uint32_t)"Gagal menghentikan proses.\n", COLOR_RED, 0);
    }
}

int parse_input(char *input_buffer, char *argv[])
{
    int argc = 0;
    char *token = strtok(input_buffer, " ");
    while (token != NULL && argc < MAX_ARGS - 1)
    {
        argv[argc] = token;
        argc++;
        token = strtok(NULL, " ");
    }
    argv[argc] = NULL;
    return argc;
}

static void clear_line_on_screen(int cursor_pos, int len)
{
    int i;
    for (i = 0; i < cursor_pos; i++)
    {
        syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);
    }
    for (i = 0; i < len; i++)
    {
        syscall(SYS_PUTC, (uint32_t)&space, COLOR_TEXT, 0);
    }
    for (i = 0; i < len; i++)
    {
        syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);
    }
}

static void redraw_line(const char *buffer, int len, int cursor_pos)
{
    syscall(SYS_PUTS, (uint32_t)buffer, COLOR_TEXT, 0);

    int i;
    for (i = 0; i < (len - cursor_pos); i++)
    {
        syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);
    }
}

void read_line(char *buffer)
{
    char c = 0;

    int buf_len = 0;
    int cursor_pos = 0;

    enum
    {
        STATE_NORMAL,
        STATE_ESC,
        STATE_BRACKET
    } state = STATE_NORMAL;

    int history_index = g_history_count;
    char temp_buffer[MAX_CMD_LEN];
    temp_buffer[0] = '\0';

    memset(buffer, 0, MAX_CMD_LEN);

    while (true)
    {
        syscall(SYS_GET_KB, (uint32_t)&c, 0, 0);

        if (c == 0)
            continue;

        if (state == STATE_NORMAL)
        {
            if (c == '\n')
            {
                buffer[buf_len] = '\0';
                syscall(SYS_PUTC, (uint32_t)&newline, COLOR_TEXT, 0);
                return;
            }
            else if (c == '\b')
            {
                if (cursor_pos > 0)
                {
                    memmove(&buffer[cursor_pos - 1], &buffer[cursor_pos], buf_len - cursor_pos);
                    cursor_pos--;
                    buf_len--;
                    buffer[buf_len] = '\0';

                    syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);
                    syscall(SYS_PUTS, (uint32_t)&buffer[cursor_pos], COLOR_TEXT, 0);
                    syscall(SYS_PUTC, (uint32_t)&space, COLOR_TEXT, 0);
                    syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);

                    int i;
                    for (i = 0; i < (buf_len - cursor_pos); i++)
                    {
                        syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);
                    }
                }
            }
            else if (c == '\033')
            {
                state = STATE_ESC;
            }
            else if (c >= 32 && c <= 126)
            {
                if (buf_len < MAX_CMD_LEN - 1)
                {
                    if (cursor_pos == buf_len)
                    {
                        buffer[cursor_pos] = c;
                        syscall(SYS_PUTC, (uint32_t)&c, COLOR_TEXT, 0);
                    }
                    else
                    {
                        memmove(&buffer[cursor_pos + 1], &buffer[cursor_pos], buf_len - cursor_pos);
                        buffer[cursor_pos] = c;

                        syscall(SYS_PUTS, (uint32_t)&buffer[cursor_pos], COLOR_TEXT, 0);

                        int i;
                        for (i = 0; i < (buf_len - cursor_pos); i++)
                        {
                            syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);
                        }
                    }
                    buf_len++;
                    cursor_pos++;
                }
            }
        }
        else if (state == STATE_ESC)
        {
            if (c == '[')
                state = STATE_BRACKET;
            else
                state = STATE_NORMAL;
        }
        else if (state == STATE_BRACKET)
        {
            state = STATE_NORMAL;
            if (c == 'A')
            {
                if (history_index == g_history_count)
                {
                    strcpy(temp_buffer, buffer);
                }
                if (history_index > 0)
                {
                    history_index--;
                    clear_line_on_screen(cursor_pos, buf_len);
                    strcpy(buffer, g_history[history_index]);
                    buf_len = strlen(buffer);
                    cursor_pos = buf_len;
                    redraw_line(buffer, buf_len, cursor_pos);
                }
            }
            else if (c == 'B')
            {
                if (history_index < g_history_count)
                {
                    history_index++;
                    clear_line_on_screen(cursor_pos, buf_len);
                    if (history_index == g_history_count)
                    {
                        strcpy(buffer, temp_buffer);
                    }
                    else
                    {
                        strcpy(buffer, g_history[history_index]);
                    }
                    buf_len = strlen(buffer);
                    cursor_pos = buf_len;
                    redraw_line(buffer, buf_len, cursor_pos);
                }
            }
            else if (c == 'C')
            {
                if (cursor_pos < buf_len)
                {
                    syscall(SYS_PUTC, (uint32_t)&buffer[cursor_pos], COLOR_TEXT, 0);
                    cursor_pos++;
                }
            }
            else if (c == 'D')
            {
                if (cursor_pos > 0)
                {
                    syscall(SYS_PUTC, (uint32_t)&backspace, COLOR_TEXT, 0);
                    cursor_pos--;
                }
            }
        }
    }
}

void find_recursive(const char *current_path, const char *target_name)
{
    char ls_buffer[MAX_CMD_LEN];
    char path_buffer[MAX_PATH_LEN];
    char full_path[MAX_PATH_LEN];
    int32_t retcode = -1;

    memset(ls_buffer, 0, MAX_CMD_LEN);
    syscall(SYS_LS, (uint32_t)current_path, (uint32_t)ls_buffer, (uint32_t)&retcode);

    if (retcode != 0)
    {
        return; // Gagal ls di path ini
    }

    char *entry_name = strtok(ls_buffer, "\n");
    while (entry_name != NULL)
    {
        if (strcmp(entry_name, target_name) == 0)
        {
            strcpy(full_path, current_path);
            if (strcmp(current_path, "/") != 0)
            {
                strcat(full_path, "/");
            }
            strcat(full_path, entry_name);

            syscall(SYS_PUTS, (uint32_t)full_path, COLOR_WHITE, 0);
            char newline = '\n';
            syscall(SYS_PUTC, (uint32_t)&newline, COLOR_WHITE, 0);
        }

        if (strcmp(entry_name, ".") != 0 && strcmp(entry_name, "..") != 0)
        {
            strcpy(path_buffer, current_path);
            if (strcmp(current_path, "/") != 0)
            {
                strcat(path_buffer, "/");
            }
            strcat(path_buffer, entry_name);

            retcode = -1;
            syscall(SYS_STAT, (uint32_t)path_buffer, (uint32_t)&retcode, 0);

            if (retcode == 0)
            {
                find_recursive(path_buffer, target_name);
            }
        }
        entry_name = strtok(NULL, "\n");
    }
}

int32_t copy_file(const char *source_path, const char *dest_path)
{
    char read_buf[FILE_BUFFER_SIZE];
    memset(read_buf, 0, FILE_BUFFER_SIZE);
    int32_t ret_read = -1;

    syscall(SYS_READ, (uint32_t)source_path, (uint32_t)read_buf, (uint32_t)&ret_read);
    if (ret_read != 0)
    {
        syscall(SYS_PUTS, (uint32_t)"Gagal membaca file sumber: ", COLOR_RED, 0);
        syscall(SYS_PUTS, (uint32_t)source_path, COLOR_RED, 0);
        syscall(SYS_PUTC, (uint32_t)&newline, COLOR_RED, 0);
        return -1;
    }

    int32_t ret_write = -1;
    syscall(SYS_WRITE, (uint32_t)dest_path, (uint32_t)read_buf, (uint32_t)&ret_write);
    if (ret_write != 0)
    {
        syscall(SYS_PUTS, (uint32_t)"Gagal menulis file tujuan: ", COLOR_RED, 0);
        syscall(SYS_PUTS, (uint32_t)dest_path, COLOR_RED, 0);
        syscall(SYS_PUTC, (uint32_t)&newline, COLOR_RED, 0);
        return -1;
    }

    return 0; // Sukses
}

int32_t parse_path_for_parent(const char *full_path, char *parent_buf, char *name_buf)
{
    strcpy(parent_buf, full_path);
    char *last_slash = strrchr(parent_buf, '/');

    if (last_slash == NULL)
    {
        return -1; // Not valid path
    }

    if (last_slash == parent_buf)
    {
        strcpy(name_buf, last_slash + 1);
        parent_buf[1] = '\0';
    }
    else
    {
        strcpy(name_buf, last_slash + 1);
        *last_slash = '\0';
    }
    return 0;
}

int32_t copy_recursive(const char *source_path, const char *dest_path)
{
    int32_t stat_ret = -1;
    syscall(SYS_STAT, (uint32_t)source_path, (uint32_t)&stat_ret, 0);

    if (stat_ret != 0)
    {
        return copy_file(source_path, dest_path);
    }
    else
    {
        char parent_path[MAX_PATH_LEN];
        char name[MAX_PATH_LEN];
        if (parse_path_for_parent(dest_path, parent_path, name) != 0)
        {
            syscall(SYS_PUTS, (uint32_t)"Gagal parse path mkdir.\n", COLOR_RED, 0);
            return -1;
        }

        int32_t ret_mkdir = -1;
        syscall(SYS_MKDIR, (uint32_t)parent_path, (uint32_t)name, (uint32_t)&ret_mkdir);
        if (ret_mkdir != 0)
        {
            syscall(SYS_PUTS, (uint32_t)"Gagal membuat direktori: ", COLOR_RED, 0);
            syscall(SYS_PUTS, (uint32_t)dest_path, COLOR_RED, 0);
            syscall(SYS_PUTC, (uint32_t)&newline, COLOR_RED, 0);
            return -1;
        }

        char ls_buffer[FILE_BUFFER_SIZE];
        int32_t ret_ls = -1;
        syscall(SYS_LS, (uint32_t)source_path, (uint32_t)ls_buffer, (uint32_t)&ret_ls);
        if (ret_ls != 0)
        {
            syscall(SYS_PUTS, (uint32_t)"Gagal membaca direktori: ", COLOR_RED, 0);
            syscall(SYS_PUTS, (uint32_t)source_path, COLOR_RED, 0);
            syscall(SYS_PUTC, (uint32_t)&newline, COLOR_RED, 0);
            return -1;
        }

        char *token = strtok(ls_buffer, "\n");
        while (token != NULL)
        {
            char new_src_path[MAX_PATH_LEN];
            char new_dest_path[MAX_PATH_LEN];

            strcpy(new_src_path, source_path);
            if (strcmp(source_path, "/") != 0)
                strcat(new_src_path, "/");
            strcat(new_src_path, token);

            strcpy(new_dest_path, dest_path);
            if (strcmp(dest_path, "/") != 0)
                strcat(new_dest_path, "/");
            strcat(new_dest_path, token);

            copy_recursive(new_src_path, new_dest_path);

            token = strtok(NULL, "\n");
        }
    }
    return 0;
}

static const char *get_basename(const char *path)
{
    const char *basename = strrchr(path, '/');
    if (basename == NULL)
    {
        return path;
    }
    else
    {
        return basename + 1;
    }
}

static int matchstar(int c, const char *regexp, const char *text)
{
    if (matchhere(regexp, text))
        return 1;

    while (*text != '\0' && (*text == c || c == '.'))
    {
        text++;
        if (matchhere(regexp, text))
            return 1;
    }

    return 0;
}

static int matchhere(const char *regexp, const char *text)
{
    if (regexp[0] == '\0')
        return 1;

    if (regexp[1] == '*')
        return matchstar(regexp[0], regexp + 2, text);

    if (regexp[0] == '$' && regexp[1] == '\0')
        return (*text == '\0');

    if (*text != '\0' && (regexp[0] == '.' || regexp[0] == *text))
        return matchhere(regexp + 1, text + 1);

    return 0; // Tidak cocok
}

int regex_match(const char *pattern, const char *text)
{
    if (pattern[0] == '^')
    {
        return matchhere(pattern + 1, text);
    }

    do
    {
        if (matchhere(pattern, text))
            return 1;
    } while (*text++ != '\0');

    return 0; // Tidak ada kecocokan di mana pun
}

void handle_cd(int argc, char *argv[])
{
    if (argc != 2)
    {
        syscall(SYS_PUTS, (uint32_t)"Penggunaan: cd <direktori>\n", COLOR_RED, 0);
        return;
    }

    char *target = argv[1];
    char new_path[MAX_PATH_LEN];

    if (strcmp(target, ".") == 0)
    {
        return;
    }

    if (strcmp(target, "..") == 0)
    {
        if (strcmp(g_cwd, "/") == 0)
            return;

        int i = 0;
        int last_slash = -1;
        while (g_cwd[i] != '\0')
        {
            if (g_cwd[i] == '/')
                last_slash = i;
            i++;
        }

        if (last_slash == 0)
        {
            strcpy(new_path, "/");
        }
        else
        {
            strcpy(new_path, g_cwd);
            new_path[last_slash] = '\0';
        }
    }
    else
    {
        strcpy(new_path, g_cwd);
        if (strcmp(g_cwd, "/") != 0)
        {
            strcat(new_path, "/");
        }
        strcat(new_path, target);
    }

    int32_t retcode = -1;
    syscall(SYS_STAT, (uint32_t)new_path, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        strcpy(g_cwd, new_path);
    }
    else
    {
        syscall(SYS_PUTS, (uint32_t)"Direktori tidak ditemukan.\n", COLOR_RED, 0);
    }
}

void handle_cp(int argc, char *argv[])
{
    if (argc != 3)
    {
        syscall(SYS_PUTS, (uint32_t)"Penggunaan: cp <sumber> <tujuan>\n", COLOR_RED, 0);
        return;
    }

    char source_full_path[MAX_PATH_LEN];
    char dest_full_path[MAX_PATH_LEN];
    build_full_path(source_full_path, argv[1]);
    build_full_path(dest_full_path, argv[2]);

    int32_t stat_ret = -1;
    syscall(SYS_STAT, (uint32_t)dest_full_path, (uint32_t)&stat_ret, 0);

    if (stat_ret == 0)
    {
        const char *source_basename = get_basename(source_full_path);
        if (strcmp(dest_full_path, "/") != 0)
        {
            strcat(dest_full_path, "/"); // Tambah slash
        }
        strcat(dest_full_path, source_basename); // Tambah nama file
    }

    syscall(SYS_PUTS, (uint32_t)"Menyalin...\n", COLOR_WHITE, 0);
    if (copy_recursive(source_full_path, dest_full_path) == 0)
    {
        syscall(SYS_PUTS, (uint32_t)"Selesai.\n", COLOR_WHITE, 0);
    }
    else
    {
        syscall(SYS_PUTS, (uint32_t)"Penyalinan gagal.\n", COLOR_RED, 0);
    }
}

void handle_cat(int argc, char *argv[])
{
    if (argc != 2)
    {
        syscall(SYS_PUTS, (uint32_t)"Penggunaan: cat <file>\n", COLOR_RED, 0);
        return;
    }

    char file_buffer[FILE_BUFFER_SIZE];
    int32_t retcode = -1;

    char full_path[MAX_PATH_LEN];
    strcpy(full_path, g_cwd);
    if (strcmp(g_cwd, "/") != 0)
        strcat(full_path, "/");
    strcat(full_path, argv[1]);

    syscall(SYS_READ, (uint32_t)full_path, (uint32_t)file_buffer, (uint32_t)&retcode);

    if (retcode == 0)
    {
        syscall(SYS_PUTS, (uint32_t)file_buffer, COLOR_WHITE, 0);
        syscall(SYS_PUTC, (uint32_t)&newline, COLOR_WHITE, 0);
    }
    else
    {
        syscall(SYS_PUTS, (uint32_t)"File tidak ditemukan.\n", COLOR_RED, 0);
    }
}

void handle_grep(int argc, char *argv[])
{
    if (argc != 3)
    {
        syscall(SYS_PUTS, (uint32_t)"Penggunaan: grep <pattern> <file>\n", COLOR_RED, 0);
        return;
    }

    char *pattern = argv[1];
    char *file_path = argv[2];

    char file_buffer[FILE_BUFFER_SIZE];
    memset(file_buffer, 0, FILE_BUFFER_SIZE);
    int32_t retcode = -1;

    char full_path[MAX_PATH_LEN];
    build_full_path(full_path, file_path);

    syscall(SYS_READ, (uint32_t)full_path, (uint32_t)file_buffer, (uint32_t)&retcode);

    if (retcode != 0)
    {
        syscall(SYS_PUTS, (uint32_t)"File tidak ditemukan.\n", COLOR_RED, 0);
        return;
    }

    char *line = strtok(file_buffer, "\n");
    char newline = '\n';
    while (line != NULL)
    {
        if (regex_match(pattern, line) == 1)
        {
            syscall(SYS_PUTS, (uint32_t)line, COLOR_WHITE, 0);
            syscall(SYS_PUTC, (uint32_t)&newline, COLOR_WHITE, 0);
        }

        line = strtok(NULL, "\n");
    }
}

void handle_mv(int argc, char *argv[])
{
    if (argc != 3)
    {
        syscall(SYS_PUTS, (uint32_t)"Penggunaan: mv <sumber> <tujuan>\n", COLOR_RED, 0);
        return;
    }

    char source_full_path[MAX_PATH_LEN];
    char dest_full_path[MAX_PATH_LEN];

    build_full_path(source_full_path, argv[1]);
    build_full_path(dest_full_path, argv[2]);

    int32_t ret_rename = -1;

    syscall(SYS_RENAME, (uint32_t)source_full_path, (uint32_t)dest_full_path, (uint32_t)&ret_rename);

    if (ret_rename != 0)
    {
        syscall(SYS_PUTS, (uint32_t)"Gagal memindahkan/merename.\n", COLOR_RED, 0);
    }
}

void handle_ls(void)
{

    char ls_buffer[FILE_BUFFER_SIZE];
    int32_t retcode = -1;

    syscall(SYS_LS, (uint32_t)g_cwd, (uint32_t)ls_buffer, (uint32_t)&retcode);

    if (retcode == 0)
    {
        char *token = strtok(ls_buffer, "\n");
        char newline = '\n';

        while (token != NULL)
        {
            char full_path[MAX_PATH_LEN];
            build_full_path(full_path, token);

            int32_t stat_ret = -1;
            syscall(SYS_STAT, (uint32_t)full_path, (uint32_t)&stat_ret, 0);

            uint32_t color;
            if (stat_ret == 0)
            {
                color = COLOR_FOLDER;
            }
            else
            {
                color = COLOR_FILE;
            }

            syscall(SYS_PUTS, (uint32_t)token, color, 0);
            syscall(SYS_PUTC, (uint32_t)&newline, 0, 0);

            token = strtok(NULL, "\n");
        }
    }
    else
    {
        syscall(SYS_PUTS, (uint32_t)"Gagal membaca direktori.\n", COLOR_RED, 0);
    }
}

void handle_help(void)
{
    syscall(SYS_PUTS, (uint32_t)"Daftar Perintah yang Tersedia:\n", COLOR_CYAN_LT, 0);
    syscall(SYS_PUTS, (uint32_t)"  cd <direktori>      : Ganti direktori saat ini\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  ls                  : Daftar isi direktori saat ini\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  cat <file>          : Tampilkan isi file\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  mkdir <nama_dir>    : Buat direktori baru\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  cp <sumber> <tujuan>: Salin file atau direktori\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  rm <file>           : Hapus file\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  mv <sumber> <tujuan>: Pindah atau ganti nama file/direktori\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  grep <pola> <file>  : Cari pola dalam file\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  find <nama>         : Cari file atau direktori secara rekursif\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  exec <path_program> : Jalankan program baru\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  ps                  : Tampilkan daftar proses berjalan\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  kill <pid|nama>     : Hentikan proses berdasarkan PID atau nama\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  clear               : Bersihkan layar terminal\n", COLOR_WHITE, 0);
    syscall(SYS_PUTS, (uint32_t)"  help                : Tampilkan menu bantuan\n", COLOR_WHITE, 0);
}

int main(void)
{
    char line_buffer[MAX_CMD_LEN];
    char *argv[MAX_ARGS];
    int argc;

    syscall(SYS_KBACT, 0, 0, 0);
    syscall(SYS_CLEAR, 0, 0, 0);

    strcpy(g_cwd, "/");

    while (true)
    {
        syscall(SYS_PUTS, (uint32_t)"rOSes_are_red", COLOR_GREEN, 0);
        syscall(SYS_PUTS, (uint32_t)":", COLOR_WHITE, 0);
        syscall(SYS_PUTS, (uint32_t)"~", COLOR_CYAN_LT, 0);
        if (strcmp(g_cwd, "/"))
        {
            syscall(SYS_PUTS, (uint32_t)g_cwd, COLOR_BLUE_LT, 0);
        }
        syscall(SYS_PUTS, (uint32_t)"$ ", COLOR_WHITE, 0);

        read_line(line_buffer);

        if (line_buffer[0] == '\0')
        {
            continue;
        }

        if (g_history_count < MAX_HISTORY)
        {
            strcpy(g_history[g_history_count], line_buffer);
            g_history_count++;
        }
        else
        {
            int i;
            for (i = 1; i < MAX_HISTORY; i++)
            {
                strcpy(g_history[i - 1], g_history[i]);
            }
            strcpy(g_history[MAX_HISTORY - 1], line_buffer);
        }

        argc = parse_input(line_buffer, argv);

        if (strcmp(argv[0], "cd") == 0)
        {
            handle_cd(argc, argv);
        }
        else if (strcmp(argv[0], "ls") == 0)
        {
            handle_ls();
        }
        else if (strcmp(argv[0], "cat") == 0)
        {
            handle_cat(argc, argv);
        }
        else if (strcmp(argv[0], "mkdir") == 0)
        {
            if (argc != 2)
            {
                syscall(SYS_PUTS, (uint32_t)"Penggunaan: mkdir <nama_dir>\n", COLOR_RED, 0);
            }
            else
            {
                int32_t retcode = -1;
                syscall(SYS_MKDIR, (uint32_t)g_cwd, (uint32_t)argv[1], (uint32_t)&retcode);
                if (retcode != 0)
                {
                    syscall(SYS_PUTS, (uint32_t)"Gagal membuat direktori.\n", COLOR_RED, 0);
                }
            }
        }
        else if (strcmp(argv[0], "cp") == 0)
        {
            handle_cp(argc, argv);
        }
        else if (strcmp(argv[0], "rm") == 0)
        {
            if (argc != 2)
            {
                syscall(SYS_PUTS, (uint32_t)"Penggunaan: rm <file>\n", COLOR_RED, 0);
            }
            else
            {
                int32_t retcode = -1;
                syscall(SYS_RM, (uint32_t)g_cwd, (uint32_t)argv[1], (uint32_t)&retcode);
                if (retcode != 0)
                {
                    syscall(SYS_PUTS, (uint32_t)"Gagal menghapus file.\n", COLOR_RED, 0);
                }
            }
        }
        else if (strcmp(argv[0], "mv") == 0)
        {
            handle_mv(argc, argv);
        }
        else if (strcmp(argv[0], "grep") == 0)
        {
            handle_grep(argc, argv);
        }
        else if (strcmp(argv[0], "find") == 0)
        {
            if (argc != 2)
            {
                syscall(SYS_PUTS, (uint32_t)"Penggunaan: find <nama>\n", COLOR_RED, 0);
            }
            find_recursive("/", argv[1]);
        }
        else if (strcmp(argv[0], "exec") == 0)
        {
            handle_exec(argc, argv);
        }
        else if (strcmp(argv[0], "ps") == 0)
        {
            handle_ps();
        }
        else if (strcmp(argv[0], "kill") == 0)
        {
            handle_kill(argc, argv);
            syscall(SYS_PUTS_AT, 24, 44, (uint32_t)"                    ");
        }
        else if (strcmp(argv[0], "clear") == 0)
        {
            syscall(SYS_CLEAR, 0, 0, 0);
        }
        else if (strcmp(argv[0], "help") == 0)
        {
            handle_help();
        }
        else if (strcmp(argv[0], "badapple") == 0)
        {
#define SIZE 1400000
#define FRAME_WIDTH 64
#define FRAME_HEIGHT 24
#define BYTES_PER_FRAME (FRAME_WIDTH * FRAME_HEIGHT / 8)

            char buffer[SIZE] = {0};
            int32_t ret = -1;
            syscall(SYS_READ, (uint32_t)"/badapplebit", (uint32_t)buffer, (uint32_t)&ret);

            if (ret != 0)
            {
                syscall(SYS_PUTS, (uint32_t)"File badapplebit.bin tidak ditemukan.\n", COLOR_RED, 0);
                continue;
            }

            syscall(SYS_RESET_TERMINAL, 0, 0, 0); // SYS_CLEAR_TERMINATE

            char frame[FRAME_HEIGHT * FRAME_WIDTH] = {0};
            uint32_t num_frames_in_buffer = SIZE / BYTES_PER_FRAME;

            for (uint32_t frame_idx = 0; frame_idx < num_frames_in_buffer; frame_idx++)
            {
                int32_t terminate_ret = 0;
                syscall(SYS_CHECK_TERMINATE, 0, 0, (uint32_t)&terminate_ret);
                if (terminate_ret)
                    break;

                uint32_t current_frame_data_offset = frame_idx * BYTES_PER_FRAME;

                // Convert packed binary to frame of chars
                for (uint32_t j = 0; j < BYTES_PER_FRAME; j++)
                {
                    char current_packed_byte = buffer[current_frame_data_offset + j];
                    for (uint32_t k = 0; k < 8; k++)
                    {
                        uint32_t char_array_pos = j * 8 + k;
                        if (char_array_pos < (FRAME_HEIGHT * FRAME_WIDTH))
                        {
                            frame[char_array_pos] = (current_packed_byte & (1 << (7 - k))) ? '#' : ' ';
                        }
                    }
                }

                // Use syscall to draw the frame
                syscall(SYS_BADAPPLE, (uint32_t)frame, FRAME_WIDTH, FRAME_HEIGHT);

                // Sleep between frames
                syscall(SYS_SLEEP, 100, 0, 0);
            }
            syscall(SYS_CLEAR, 0, 0, 0);          // Clear screen after playing
            syscall(SYS_RESET_TERMINAL, 0, 0, 0); // Reset terminal
        }
        else
        {
            syscall(SYS_PUTS, (uint32_t)"Perintah tidak dikenal: ", COLOR_RED, 0);
            syscall(SYS_PUTS, (uint32_t)argv[0], COLOR_RED, 0);
            syscall(SYS_PUTC, (uint32_t)&newline, COLOR_RED, 0);
        }
    }

    return 0;
}