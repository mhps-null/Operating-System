#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/driver/disk.h"
#include "header/filesystem/ext2.h"

static uint8_t buffer[BLOCK_SIZE];
static struct EXT2Superblock g_superblock;
static struct EXT2BlockGroupDescriptor g_bgd_table[GROUPS_COUNT];

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C',
    'o',
    'u',
    'r',
    's',
    'e',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'D',
    'e',
    's',
    'i',
    'g',
    'n',
    'e',
    'd',
    ' ',
    'b',
    'y',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'L',
    'a',
    'b',
    ' ',
    'S',
    'i',
    's',
    't',
    'e',
    'r',
    ' ',
    'I',
    'T',
    'B',
    ' ',
    ' ',
    'M',
    'a',
    'd',
    'e',
    ' ',
    'w',
    'i',
    't',
    'h',
    ' ',
    '<',
    '3',
    ' ',
    ' ',
    ' ',
    ' ',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '2',
    '0',
    '2',
    '5',
    '\n',
    [BLOCK_SIZE - 2] = 'O',
    [BLOCK_SIZE - 1] = 'k',
};

char *get_entry_name(void *entry)
{
    return (char *)entry + sizeof(struct EXT2DirectoryEntry);
};

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset)
{
    return (struct EXT2DirectoryEntry *)((char *)ptr + offset);
};

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry)
{
    return (struct EXT2DirectoryEntry *)((char *)entry + entry->rec_len);
};

uint16_t get_entry_record_len(uint8_t name_len)
{
    uint16_t unaligned_size = sizeof(struct EXT2DirectoryEntry) + name_len;
    return (unaligned_size + 3) & ~3; // pembulatan 4 keatas untuk alignment 4 byte
};

uint32_t get_dir_first_child_offset(void *ptr)
{
    struct EXT2DirectoryEntry *entry_dot = (struct EXT2DirectoryEntry *)ptr;
    uint32_t dot_rec_len = entry_dot->rec_len;

    struct EXT2DirectoryEntry *entry_dot_dot = (struct EXT2DirectoryEntry *)((char *)ptr + dot_rec_len);
    uint32_t dot_dot_rec_len = entry_dot_dot->rec_len;

    return dot_rec_len + dot_dot_rec_len;
};

uint32_t inode_to_bgd(uint32_t inode)
{
    return (inode - 1) / INODES_PER_GROUP;
};

uint32_t inode_to_local(uint32_t inode)
{
    return (inode - 1) % INODES_PER_GROUP;
};

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode)
{
    node->i_mode = EXT2_S_IFDIR;
    node->i_size = BLOCK_SIZE;
    node->i_blocks = BLOCK_SIZE / 512;

    uint32_t bgd_index = inode_to_bgd(inode);
    uint32_t new_block = allocate_block(bgd_index);
    if (new_block == 0)
    {
        return; // Gagal alokasi
    }
    node->i_block[0] = new_block;

    uint8_t local_buffer[BLOCK_SIZE];
    memset(local_buffer, 0, BLOCK_SIZE);

    struct EXT2DirectoryEntry *entry_dot = (struct EXT2DirectoryEntry *)local_buffer;
    entry_dot->inode = inode;
    entry_dot->name_len = 1;
    entry_dot->file_type = EXT2_FT_DIR;
    entry_dot->rec_len = 12;
    memcpy(get_entry_name(entry_dot), ".", 1);

    struct EXT2DirectoryEntry *entry_dot_dot = (struct EXT2DirectoryEntry *)(local_buffer + 12);
    entry_dot_dot->inode = parent_inode;
    entry_dot_dot->name_len = 2;
    entry_dot_dot->file_type = EXT2_FT_DIR;
    entry_dot_dot->rec_len = BLOCK_SIZE - 12;
    memcpy(get_entry_name(entry_dot_dot), "..", 2);

    write_blocks(local_buffer, new_block, 1);
};

bool is_empty_storage(void)
{
    uint8_t boot_sector_buffer[BLOCK_SIZE];
    read_blocks(boot_sector_buffer, BOOT_SECTOR, 1);
    int result = memcmp(boot_sector_buffer, fs_signature, sizeof(fs_signature));
    return (result != 0);
};

static void set_bit(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] |= (1 << (bit % 8));
};

static void clear_bit(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] &= ~(1 << (bit % 8));
};

static bool get_bit(uint8_t *bitmap, uint32_t bit)
{
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
};

void create_ext2(void)
{
    uint32_t i, b;
    uint32_t total_free_blocks = 0;
    uint32_t total_free_inodes = 0;

    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, fs_signature, sizeof(fs_signature));
    write_blocks(buffer, BOOT_SECTOR, 1);

    memset(g_bgd_table, 0, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);

    for (i = 0; i < GROUPS_COUNT; i++)
    {
        uint32_t group_base_block = i * BLOCKS_PER_GROUP;
        uint32_t blocks_used_for_meta = 0;

        if (i == 0)
        {
            g_bgd_table[i].bg_block_bitmap = 3;
            g_bgd_table[i].bg_inode_bitmap = 4;
            g_bgd_table[i].bg_inode_table = 5;
            blocks_used_for_meta = 20;
        }
        else
        {
            g_bgd_table[i].bg_block_bitmap = group_base_block;
            g_bgd_table[i].bg_inode_bitmap = group_base_block + 1;
            g_bgd_table[i].bg_inode_table = group_base_block + 2;
            blocks_used_for_meta = 18;
        }

        g_bgd_table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - blocks_used_for_meta;
        g_bgd_table[i].bg_free_inodes_count = INODES_PER_GROUP;
        g_bgd_table[i].bg_used_dirs_count = 0;

        total_free_blocks += g_bgd_table[i].bg_free_blocks_count;
        total_free_inodes += g_bgd_table[i].bg_free_inodes_count;

        memset(buffer, 0, BLOCK_SIZE);
        write_blocks(buffer, g_bgd_table[i].bg_inode_bitmap, 1);
        write_blocks(buffer, g_bgd_table[i].bg_inode_table, INODES_TABLE_BLOCK_COUNT);

        if (i == 0)
        {
            set_bit(buffer, 0);
            for (b = 1; b <= 20; b++)
            {
                set_bit(buffer, b);
            }

            blocks_used_for_meta = 21;
        }
        else
        {
            for (b = 0; b < 18; b++)
            {
                set_bit(buffer, b);
            }
        }
        write_blocks(buffer, g_bgd_table[i].bg_block_bitmap, 1);
    }

    memset(&g_superblock, 0, sizeof(struct EXT2Superblock));
    g_superblock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT;
    g_superblock.s_blocks_count = BLOCKS_PER_GROUP * GROUPS_COUNT;
    g_superblock.s_r_blocks_count = 0;
    g_superblock.s_free_blocks_count = total_free_blocks;
    g_superblock.s_free_inodes_count = total_free_inodes;
    g_superblock.s_first_data_block = 1;
    g_superblock.s_blocks_per_group = BLOCKS_PER_GROUP;
    g_superblock.s_frags_per_group = BLOCKS_PER_GROUP;
    g_superblock.s_inodes_per_group = INODES_PER_GROUP;
    g_superblock.s_magic = EXT2_SUPER_MAGIC;
    g_superblock.s_first_ino = 1;

    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, &g_superblock, sizeof(struct EXT2Superblock));
    write_blocks(buffer, 1, 1);

    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, g_bgd_table, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);
    write_blocks(buffer, 2, 1);

    uint32_t root_inode_num = 1;
    uint32_t root_group = 0;
    uint32_t root_local_idx = 0;

    read_blocks(buffer, g_bgd_table[root_group].bg_inode_bitmap, 1);
    set_bit(buffer, root_local_idx);
    write_blocks(buffer, g_bgd_table[root_group].bg_inode_bitmap, 1);

    g_bgd_table[root_group].bg_free_inodes_count--;
    g_bgd_table[root_group].bg_used_dirs_count++;
    g_superblock.s_free_inodes_count--;

    struct EXT2Inode root_inode;
    memset(&root_inode, 0, sizeof(struct EXT2Inode));

    init_directory_table(&root_inode, root_inode_num, root_inode_num);

    g_bgd_table[root_group].bg_free_blocks_count--;
    g_superblock.s_free_blocks_count--;

    sync_node(&root_inode, root_inode_num);

    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, &g_superblock, sizeof(struct EXT2Superblock));
    write_blocks(buffer, 1, 1);

    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, g_bgd_table, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);
    write_blocks(buffer, 2, 1);
};

void initialize_filesystem_ext2(void)
{
    if (is_empty_storage())
    {
        create_ext2();
    }

    memset(buffer, 0, BLOCK_SIZE);

    read_blocks(buffer, 1, 1);
    memcpy(&g_superblock, buffer, sizeof(struct EXT2Superblock));

    read_blocks(buffer, 2, 1);
    memcpy(g_bgd_table, buffer, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);

    // if (g_superblock.s_magic != EXT2_SUPER_MAGIC)
    // {
    //     while (1)
    //         ;
    // }
};

void read_inode(uint32_t inode_num, struct EXT2Inode *out_node)
{
    uint32_t group = inode_to_bgd(inode_num);
    uint32_t local_idx = inode_to_local(inode_num);
    uint32_t table_start_block = g_bgd_table[group].bg_inode_table;

    uint32_t block_offset = local_idx / INODES_PER_TABLE;
    uint32_t index_in_block = local_idx % INODES_PER_TABLE;

    uint32_t inode_block_to_read = table_start_block + block_offset;

    memset(buffer, 0, BLOCK_SIZE);
    read_blocks(buffer, inode_block_to_read, 1);

    struct EXT2Inode *inode_table_in_block = (struct EXT2Inode *)buffer;

    memcpy(out_node, &inode_table_in_block[index_in_block], sizeof(struct EXT2Inode));
}

bool is_directory_empty(uint32_t inode)
{
    struct EXT2Inode dir_inode;
    read_inode(inode, &dir_inode);

    if ((dir_inode.i_mode & EXT2_S_IFDIR) == 0)
    {
        return false;
    }

    uint32_t data_block_num = dir_inode.i_block[0];

    memset(buffer, 0, BLOCK_SIZE);
    read_blocks(buffer, data_block_num, 1);

    uint32_t first_child_offset = get_dir_first_child_offset(buffer);

    return (first_child_offset >= BLOCK_SIZE);
};

uint32_t find_inode_by_name(struct EXT2Inode *parent_inode, const char *name, uint8_t name_len)
{

    memset(buffer, 0, BLOCK_SIZE);
    uint32_t current_block_size;

    for (int i = 0; i < 12; i++)
    {
        uint32_t block_num = parent_inode->i_block[i];
        if (block_num == 0)
        {
            continue;
        }

        if (parent_inode->i_size < (uint32_t)(i + 1) * BLOCK_SIZE)
        {
            current_block_size = parent_inode->i_size % BLOCK_SIZE;
        }
        else
        {
            current_block_size = BLOCK_SIZE;
        }

        read_blocks(buffer, block_num, 1);

        uint32_t offset = 0;
        while (offset < current_block_size)
        {
            struct EXT2DirectoryEntry *entry = get_directory_entry(buffer, offset);

            if (entry->inode == 0)
            {
                offset += entry->rec_len;
                continue;
            }

            if (entry->name_len == name_len &&
                memcmp(name, get_entry_name(entry), name_len) == 0)
            {
                return entry->inode;
            }

            offset += entry->rec_len;
        }
    }

    return 0;
}

int8_t read_directory(struct EXT2DriverRequest *prequest)
{
    struct EXT2Inode parent_inode;
    read_inode(prequest->parent_inode, &parent_inode);

    if ((parent_inode.i_mode & EXT2_S_IFDIR) == 0)
    {
        return 3; // 3: parent folder invalid
    }

    uint32_t target_inode_num = find_inode_by_name(
        &parent_inode, prequest->name, prequest->name_len);

    if (target_inode_num == 0)
    {
        return 2; // 2: not found
    }

    struct EXT2Inode target_inode;
    read_inode(target_inode_num, &target_inode);

    if ((target_inode.i_mode & EXT2_S_IFDIR) == 0)
    {
        return 1; // 1: not a folder
    }

    if (prequest->buffer_size < target_inode.i_size)
    {
        return -1; // -1: unknown
    }

    uint32_t bytes_copied = 0;
    uint8_t temp_buffer[BLOCK_SIZE];

    for (int i = 0; i < 12; i++)
    {
        uint32_t block_num = target_inode.i_block[i];
        if (block_num == 0 || bytes_copied >= target_inode.i_size)
        {
            break;
        }

        read_blocks(temp_buffer, block_num, 1);

        uint32_t bytes_to_copy = BLOCK_SIZE;
        if (bytes_copied + BLOCK_SIZE > target_inode.i_size)
        {
            bytes_to_copy = target_inode.i_size - bytes_copied;
        }

        memcpy((char *)prequest->buf + bytes_copied, temp_buffer, bytes_to_copy);
        bytes_copied += bytes_to_copy;
    }

    if (bytes_copied < target_inode.i_size)
    {
        return -1; // -1: unknown
    }

    return 0; // 0: success
};

int8_t read(struct EXT2DriverRequest request)
{
    struct EXT2Inode parent_inode;
    read_inode(request.parent_inode, &parent_inode);

    if ((parent_inode.i_mode & EXT2_S_IFDIR) == 0)
    {
        return 4; // 4: parent folder invalid
    }

    uint32_t target_inode_num = find_inode_by_name(
        &parent_inode, request.name, request.name_len);

    if (target_inode_num == 0)
    {
        return 3; // 3: not found
    }

    struct EXT2Inode target_inode;
    read_inode(target_inode_num, &target_inode);

    if ((target_inode.i_mode & EXT2_S_IFREG) == 0)
    {
        return 1; // 1: not a file
    }

    uint32_t bytes_to_read = (request.buffer_size < target_inode.i_size)
                                 ? request.buffer_size
                                 : target_inode.i_size;

    if (request.buffer_size < target_inode.i_size)
    {
        return 2; // 2: not enough buffer
    }
    bytes_to_read = target_inode.i_size;

    uint32_t bytes_copied = 0;
    uint8_t temp_buffer[BLOCK_SIZE];
    uint32_t pointers_per_block = BLOCK_SIZE / sizeof(uint32_t);

    for (int i = 0; i < 12; i++)
    {
        if (bytes_copied >= bytes_to_read)
            break;

        uint32_t block_num = target_inode.i_block[i];
        if (block_num == 0)
            break;

        read_blocks(temp_buffer, block_num, 1);

        uint32_t bytes_this_block = (bytes_to_read - bytes_copied > BLOCK_SIZE)
                                        ? BLOCK_SIZE
                                        : (bytes_to_read - bytes_copied);

        memcpy((char *)request.buf + bytes_copied, temp_buffer, bytes_this_block);
        bytes_copied += bytes_this_block;
    }

    if (bytes_copied < bytes_to_read && target_inode.i_block[12] != 0)
    {
        uint32_t indirect_block[pointers_per_block];
        read_blocks(indirect_block, target_inode.i_block[12], 1);

        for (int j = 0; j < (int)pointers_per_block; j++)
        {
            if (bytes_copied >= bytes_to_read)
                break;

            uint32_t block_num = indirect_block[j];
            if (block_num == 0)
                break;

            read_blocks(temp_buffer, block_num, 1);

            uint32_t bytes_this_block = (bytes_to_read - bytes_copied > BLOCK_SIZE)
                                            ? BLOCK_SIZE
                                            : (bytes_to_read - bytes_copied);

            memcpy((char *)request.buf + bytes_copied, temp_buffer, bytes_this_block);
            bytes_copied += bytes_this_block;
        }
    }

    if (bytes_copied < bytes_to_read && target_inode.i_block[13] != 0)
    {
        uint32_t d_indirect_block[pointers_per_block];
        read_blocks(d_indirect_block, target_inode.i_block[13], 1);

        for (int j = 0; j < (int)pointers_per_block; j++)
        {
            if (bytes_copied >= bytes_to_read)
                break;
            if (d_indirect_block[j] == 0)
                continue;

            uint32_t indirect_block[pointers_per_block];
            read_blocks(indirect_block, d_indirect_block[j], 1);

            for (int k = 0; k < (int)pointers_per_block; k++)
            {
                if (bytes_copied >= bytes_to_read)
                    break;

                uint32_t block_num = indirect_block[k];
                if (block_num == 0)
                    break;

                read_blocks(temp_buffer, block_num, 1);

                uint32_t bytes_this_block = (bytes_to_read - bytes_copied > BLOCK_SIZE)
                                                ? BLOCK_SIZE
                                                : (bytes_to_read - bytes_copied);

                memcpy((char *)request.buf + bytes_copied, temp_buffer, bytes_this_block);
                bytes_copied += bytes_this_block;
            }
        }
    }

    if (bytes_copied < bytes_to_read)
    {
        return -1; // -1 unknown
    }

    return 0; // 0: success
};

uint32_t allocate_block(uint32_t prefered_bgd)
{
    uint8_t bitmap_buffer[BLOCK_SIZE];

    if (g_bgd_table[prefered_bgd].bg_free_blocks_count > 0)
    {
        read_blocks(bitmap_buffer, g_bgd_table[prefered_bgd].bg_block_bitmap, 1);
        for (uint32_t i = 0; i < BLOCKS_PER_GROUP; i++)
        {
            if (get_bit(bitmap_buffer, i) == 0)
            {
                set_bit(bitmap_buffer, i);
                write_blocks(bitmap_buffer, g_bgd_table[prefered_bgd].bg_block_bitmap, 1);

                g_bgd_table[prefered_bgd].bg_free_blocks_count--;
                g_superblock.s_free_blocks_count--;

                return (prefered_bgd * BLOCKS_PER_GROUP) + i;
            }
        }
    }

    for (uint32_t g = 0; g < GROUPS_COUNT; g++)
    {
        if (g_bgd_table[g].bg_free_blocks_count > 0)
        {
            read_blocks(bitmap_buffer, g_bgd_table[g].bg_block_bitmap, 1);
            for (uint32_t i = 0; i < BLOCKS_PER_GROUP; i++)
            {
                if (get_bit(bitmap_buffer, i) == 0)
                {
                    set_bit(bitmap_buffer, i);
                    write_blocks(bitmap_buffer, g_bgd_table[g].bg_block_bitmap, 1);
                    g_bgd_table[g].bg_free_blocks_count--;
                    g_superblock.s_free_blocks_count--;
                    return (g * BLOCKS_PER_GROUP) + i;
                }
            }
        }
    }

    return 0; // Disk penuh
}

static int8_t add_entry_to_directory(struct EXT2Inode *parent_inode, uint32_t parent_inode_num,
                                     uint32_t new_inode_num, const char *name, uint8_t name_len, uint8_t file_type)
{
    uint16_t needed_len = get_entry_record_len(name_len);

    memset(buffer, 0, BLOCK_SIZE);

    for (int i = 0; i < 12; i++)
    {
        uint32_t block_num = parent_inode->i_block[i];
        if (block_num == 0)
            continue;

        read_blocks(buffer, block_num, 1);
        uint32_t offset = 0;

        while (offset < BLOCK_SIZE)
        {
            struct EXT2DirectoryEntry *entry = get_directory_entry(buffer, offset);

            if (entry->rec_len == 0)
                break;

            uint16_t actual_len = get_entry_record_len(entry->name_len);
            uint16_t padding = entry->rec_len - actual_len;

            if (padding >= needed_len)
            {
                uint16_t old_rec_len = entry->rec_len;
                entry->rec_len = actual_len;

                struct EXT2DirectoryEntry *new_entry = get_directory_entry(buffer, offset + actual_len);
                new_entry->inode = new_inode_num;
                new_entry->name_len = name_len;
                new_entry->file_type = file_type;
                new_entry->rec_len = old_rec_len - actual_len;
                memcpy(get_entry_name(new_entry), name, name_len);

                write_blocks(buffer, block_num, 1);
                return 0;
            }
            offset += entry->rec_len;
        }
    }

    for (int i = 0; i < 12; i++)
    {
        if (parent_inode->i_block[i] == 0)
        {
            uint32_t new_block = allocate_block(inode_to_bgd(parent_inode_num));
            if (new_block == 0)
                return -1;

            parent_inode->i_block[i] = new_block;
            parent_inode->i_size += BLOCK_SIZE;
            parent_inode->i_blocks++;
            sync_node(parent_inode, parent_inode_num);

            memset(buffer, 0, BLOCK_SIZE);
            struct EXT2DirectoryEntry *new_entry = (struct EXT2DirectoryEntry *)buffer;
            new_entry->inode = new_inode_num;
            new_entry->name_len = name_len;
            new_entry->file_type = file_type;
            new_entry->rec_len = BLOCK_SIZE;
            memcpy(get_entry_name(new_entry), name, name_len);

            write_blocks(buffer, new_block, 1);
            return 0;
        }
    }

    return -1;
}

static void deallocate_node_data_blocks(struct EXT2Inode *node)
{
    struct BlockBuffer temp_buffer;
    uint32_t last_bgd_cache = 0;

    uint32_t i_block_copy[15];
    memcpy(i_block_copy, node->i_block, sizeof(i_block_copy));

    deallocate_block(
        i_block_copy, 12,
        &temp_buffer, 0,
        &last_bgd_cache, false);
    deallocate_block(
        &i_block_copy[12], 1,
        &temp_buffer, 1,
        &last_bgd_cache, false);
    deallocate_block(
        &i_block_copy[13], 1,
        &temp_buffer, 2,
        &last_bgd_cache, false);
    deallocate_block(
        &i_block_copy[14], 1,
        &temp_buffer, 3,
        &last_bgd_cache, false);

    memset(node->i_block, 0, sizeof(node->i_block));
    node->i_size = 0;
    node->i_blocks = 0;
}

int8_t write(struct EXT2DriverRequest *request)
{
    struct EXT2Inode parent_inode;
    read_inode(request->parent_inode, &parent_inode);

    if ((parent_inode.i_mode & EXT2_S_IFDIR) == 0)
    {
        return 2; // 2: invalid parent folder
    }

    uint32_t existing_inode_num = find_inode_by_name(
        &parent_inode, request->name, request->name_len);

    if (request->is_directory)
    {
        if (existing_inode_num != 0)
        {
            return 1; // 1: file/folder already exist
        }

        uint32_t new_inode_num = allocate_node();
        if (new_inode_num == 0)
            return -1;
        struct EXT2Inode new_inode;
        memset(&new_inode, 0, sizeof(struct EXT2Inode));
        uint32_t prefered_bgd = inode_to_bgd(new_inode_num);
        new_inode.i_mode = EXT2_S_IFDIR;
        init_directory_table(&new_inode, new_inode_num, request->parent_inode);
        g_bgd_table[prefered_bgd].bg_used_dirs_count++;
        int8_t add_result = add_entry_to_directory(
            &parent_inode, request->parent_inode,
            new_inode_num, request->name, request->name_len, EXT2_FT_DIR);
        if (add_result != 0)
        {
            deallocate_node(new_inode_num);
            return -1;
        }
        sync_node(&parent_inode, request->parent_inode);
        sync_node(&new_inode, new_inode_num);
    }
    else
    {
        struct EXT2Inode target_inode;
        uint32_t target_inode_num;
        uint32_t prefered_bgd;

        if (existing_inode_num != 0)
        {
            target_inode_num = existing_inode_num;
            read_inode(target_inode_num, &target_inode);
            prefered_bgd = inode_to_bgd(target_inode_num);

            if ((target_inode.i_mode & EXT2_S_IFDIR) != 0)
            {
                return 1;
            }

            deallocate_node_data_blocks(&target_inode);

            target_inode.i_size = request->buffer_size;
            if (target_inode.i_size > 0)
            {
                allocate_node_blocks(request->buf, &target_inode, prefered_bgd);
            }
            sync_node(&target_inode, target_inode_num);
        }
        else
        {
            target_inode_num = allocate_node();
            if (target_inode_num == 0)
                return -1;
            memset(&target_inode, 0, sizeof(struct EXT2Inode));
            prefered_bgd = inode_to_bgd(target_inode_num);
            target_inode.i_mode = EXT2_S_IFREG;
            target_inode.i_size = request->buffer_size;
            if (target_inode.i_size > 0)
            {
                allocate_node_blocks(request->buf, &target_inode, prefered_bgd);
            }
            int8_t add_result = add_entry_to_directory(
                &parent_inode, request->parent_inode,
                target_inode_num, request->name, request->name_len, EXT2_FT_REG_FILE);
            if (add_result != 0)
            {
                deallocate_node(target_inode_num);
                return -1;
            }
            sync_node(&parent_inode, request->parent_inode);
            sync_node(&target_inode, target_inode_num);
        }
    }

    uint8_t temp_buffer[BLOCK_SIZE];
    memset(temp_buffer, 0, BLOCK_SIZE);
    memcpy(temp_buffer, &g_superblock, sizeof(struct EXT2Superblock));
    write_blocks(temp_buffer, 1, 1);
    memset(temp_buffer, 0, BLOCK_SIZE);
    memcpy(temp_buffer, g_bgd_table, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);
    write_blocks(temp_buffer, 2, 1);

    return 0; // 0: success
}

static void update_dot_dot_pointer(uint32_t dir_inode_num, uint32_t new_parent_ino)
{
    struct EXT2Inode dir_inode;
    read_inode(dir_inode_num, &dir_inode);

    uint32_t block_num = dir_inode.i_block[0];
    if (block_num == 0)
        return;

    uint8_t local_buffer[BLOCK_SIZE];
    read_blocks(local_buffer, block_num, 1);

    struct EXT2DirectoryEntry *entry_dot = get_directory_entry(local_buffer, 0);

    struct EXT2DirectoryEntry *entry_dot_dot = get_directory_entry(local_buffer, entry_dot->rec_len);

    entry_dot_dot->inode = new_parent_ino;

    write_blocks(local_buffer, block_num, 1);
}

static uint8_t get_file_type_from_inode(struct EXT2Inode *node)
{
    if ((node->i_mode & EXT2_S_IFDIR) != 0)
    {
        return EXT2_FT_DIR;
    }
    if ((node->i_mode & EXT2_S_IFREG) != 0)
    {
        return EXT2_FT_REG_FILE;
    }
    return EXT2_FT_UNKNOWN;
}

static int8_t remove_entry_from_directory(struct EXT2Inode *parent_inode, const char *name, uint8_t name_len)
{
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);

    for (int i = 0; i < 12; i++)
    {
        uint32_t block_num = parent_inode->i_block[i];
        if (block_num == 0)
            continue;

        read_blocks(buffer, block_num, 1);
        uint32_t offset = 0;
        struct EXT2DirectoryEntry *prev_entry = NULL;

        while (offset < BLOCK_SIZE)
        {
            struct EXT2DirectoryEntry *entry = get_directory_entry(buffer, offset);
            if (entry->rec_len == 0)
                break;

            char *target_name = get_entry_name(entry);
            if (entry->inode != 0 && entry->name_len == name_len &&
                memcmp(name, target_name, name_len) == 0)
            {
                uint16_t this_len = entry->rec_len;

                if (prev_entry == NULL)
                {
                    memset(entry, 0, this_len);
                }
                else
                {
                    prev_entry->rec_len += this_len;
                    memset(entry, 0, this_len);
                }

                write_blocks(buffer, block_num, 1);
                return 0; // sukses
            }

            prev_entry = entry;
            offset += entry->rec_len;
        }
    }

    return 1; // tidak ditemukan
}

int8_t rename_entry(uint32_t old_parent_ino, const char *old_name, uint32_t new_parent_ino, const char *new_name)
{
    uint8_t old_name_len = strlen(old_name);
    uint8_t new_name_len = strlen(new_name);

    if (old_name_len == 1 && old_name[0] == '.')
        return -1;
    if (old_name_len == 2 && old_name[0] == '.' && old_name[1] == '.')
        return -1;

    struct EXT2Inode old_parent_inode;
    read_inode(old_parent_ino, &old_parent_inode);

    struct EXT2Inode new_parent_inode;
    if (old_parent_ino == new_parent_ino)
    {
        memcpy(&new_parent_inode, &old_parent_inode, sizeof(struct EXT2Inode));
    }
    else
    {
        read_inode(new_parent_ino, &new_parent_inode);
    }

    uint32_t target_inode_num = find_inode_by_name(&old_parent_inode, old_name, old_name_len);
    if (target_inode_num == 0)
    {
        return 1; // 1: Not found
    }

    if (find_inode_by_name(&new_parent_inode, new_name, new_name_len) != 0)
    {
        return -1; // Gagal: Nama tujuan sudah ada
    }

    struct EXT2Inode target_inode;
    read_inode(target_inode_num, &target_inode);
    uint8_t file_type = get_file_type_from_inode(&target_inode);

    int8_t rm_result = remove_entry_from_directory(&old_parent_inode, old_name, old_name_len);
    if (rm_result != 0)
    {
        return -1; // Gagal menghapus entri lama
    }
    sync_node(&old_parent_inode, old_parent_ino);

    int8_t add_result = add_entry_to_directory(
        &new_parent_inode, new_parent_ino,
        target_inode_num, new_name, new_name_len, file_type);

    if (add_result != 0)
    {
        add_entry_to_directory(
            &old_parent_inode, old_parent_ino,
            target_inode_num, old_name, old_name_len, file_type);
        return -1; // Gagal menambah entri baru
    }

    if (file_type == EXT2_FT_DIR && old_parent_ino != new_parent_ino)
    {
        update_dot_dot_pointer(target_inode_num, new_parent_ino);
    }

    sync_node(&new_parent_inode, new_parent_ino);

    uint8_t temp_buffer[BLOCK_SIZE];

    memset(temp_buffer, 0, BLOCK_SIZE);
    memcpy(temp_buffer, &g_superblock, sizeof(struct EXT2Superblock));
    write_blocks(temp_buffer, 1, 1);

    memset(temp_buffer, 0, BLOCK_SIZE);
    memcpy(temp_buffer, g_bgd_table, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);
    write_blocks(temp_buffer, 2, 1);

    return 0; // Sukses
}

int8_t delete(struct EXT2DriverRequest request)
{
    if (request.name_len == 1 && request.name[0] == '.')
    {
        return -1; // -1 unknown (operasi tidak valid)
    }
    if (request.name_len == 2 && request.name[0] == '.' && request.name[1] == '.')
    {
        return -1; // -1 unknown (operasi tidak valid)
    }

    struct EXT2Inode parent_inode;
    read_inode(request.parent_inode, &parent_inode);

    if ((parent_inode.i_mode & EXT2_S_IFDIR) == 0)
    {
        return 3; // 3: parent folder invalid
    }

    uint32_t target_inode_num = find_inode_by_name(
        &parent_inode, request.name, request.name_len);

    if (target_inode_num == 0)
    {
        return 1; // 1: not found
    }

    struct EXT2Inode target_inode;
    read_inode(target_inode_num, &target_inode);

    bool is_target_dir = (target_inode.i_mode & EXT2_S_IFDIR) != 0;

    if (request.is_directory != is_target_dir)
    {
        return 1; // 1: not found (atau error "tipe tidak cocok")
    }

    if (is_target_dir)
    {
        if (!is_directory_empty(target_inode_num))
        {
            return 2; // 2: folder is not empty
        }
    }

    int8_t remove_result = remove_entry_from_directory(
        &parent_inode, request.name, request.name_len);

    if (remove_result != 0)
    {
        return -1; // -1 unknown
    }

    deallocate_node(target_inode_num);

    uint8_t temp_buffer[BLOCK_SIZE];

    memset(temp_buffer, 0, BLOCK_SIZE);
    memcpy(temp_buffer, &g_superblock, sizeof(struct EXT2Superblock));
    write_blocks(temp_buffer, 1, 1);

    memset(temp_buffer, 0, BLOCK_SIZE);
    memcpy(temp_buffer, g_bgd_table, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);
    write_blocks(temp_buffer, 2, 1);

    return 0; // 0: success
};

uint32_t allocate_node(void)
{
    uint8_t bitmap_buffer[BLOCK_SIZE];

    for (uint32_t g = 0; g < GROUPS_COUNT; g++)
    {
        if (g_bgd_table[g].bg_free_inodes_count > 0)
        {
            read_blocks(bitmap_buffer, g_bgd_table[g].bg_inode_bitmap, 1);
            for (uint32_t i = 0; i < INODES_PER_GROUP; i++)
            {
                if (get_bit(bitmap_buffer, i) == 0)
                {
                    set_bit(bitmap_buffer, i);
                    write_blocks(bitmap_buffer, g_bgd_table[g].bg_inode_bitmap, 1);

                    g_bgd_table[g].bg_free_inodes_count--;
                    g_superblock.s_free_inodes_count--;

                    return (g * INODES_PER_GROUP) + i + 1;
                }
            }
        }
    }

    return 0; // Disk penuh
};

void deallocate_node(uint32_t inode)
{
    struct BlockBuffer temp_buffer;
    uint32_t last_bgd_cache = 0;

    struct EXT2Inode node_to_delete;
    read_inode(inode, &node_to_delete);

    uint32_t i_block_copy[15];
    memcpy(i_block_copy, node_to_delete.i_block, sizeof(i_block_copy));

    deallocate_block(
        i_block_copy, 12,
        &temp_buffer, 0,
        &last_bgd_cache, false);

    deallocate_block(
        &i_block_copy[12], 1,
        &temp_buffer, 1,
        &last_bgd_cache, false);

    deallocate_block(
        &i_block_copy[13], 1,
        &temp_buffer, 2,
        &last_bgd_cache, false);

    deallocate_block(
        &i_block_copy[14], 1,
        &temp_buffer, 3,
        &last_bgd_cache, false);

    uint32_t group = inode_to_bgd(inode);
    uint32_t local_idx = inode_to_local(inode);

    read_blocks(temp_buffer.buf, g_bgd_table[group].bg_inode_bitmap, 1);
    clear_bit(temp_buffer.buf, local_idx);
    write_blocks(temp_buffer.buf, g_bgd_table[group].bg_inode_bitmap, 1);

    g_superblock.s_free_inodes_count++;
    g_bgd_table[group].bg_free_inodes_count++;

    if ((node_to_delete.i_mode & EXT2_S_IFDIR) != 0)
    {
        g_bgd_table[group].bg_used_dirs_count--;
    }

    memset(&node_to_delete, 0, sizeof(struct EXT2Inode));
    sync_node(&node_to_delete, inode);

    memset(temp_buffer.buf, 0, BLOCK_SIZE);
    memcpy(temp_buffer.buf, &g_superblock, sizeof(struct EXT2Superblock));
    write_blocks(temp_buffer.buf, 1, 1);

    memset(temp_buffer.buf, 0, BLOCK_SIZE);
    memcpy(temp_buffer.buf, g_bgd_table, sizeof(struct EXT2BlockGroupDescriptor) * GROUPS_COUNT);
    write_blocks(temp_buffer.buf, 2, 1);
};

void deallocate_blocks(void *loc, uint32_t blocks)
{
    uint32_t *locations = (uint32_t *)loc;

    struct BlockBuffer temp_bitmap_buffer;
    uint32_t last_bgd_cache = 0;

    deallocate_block(
        locations,
        blocks,
        &temp_bitmap_buffer,
        0,
        &last_bgd_cache,
        false);
};

uint32_t deallocate_block(uint32_t *locations, uint32_t blocks, struct BlockBuffer *bitmap, uint32_t depth, uint32_t *last_bgd, bool bgd_loaded)
{
    if (blocks == 0 || locations == NULL)
    {
        return *last_bgd;
    }

    if (depth == 0)
    {
        for (uint32_t i = 0; i < blocks; i++)
        {
            uint32_t blk = locations[i];
            if (blk == 0)
                continue;

            uint32_t grp = blk / BLOCKS_PER_GROUP;
            if (!bgd_loaded || grp != *last_bgd)
            {
                read_blocks(bitmap,
                            g_bgd_table[grp].bg_block_bitmap,
                            1);
                *last_bgd = grp;
                bgd_loaded = true;
            }

            uint32_t local = blk % BLOCKS_PER_GROUP;
            uint32_t byte = local / 8;
            uint32_t bit = local % 8;
            bitmap->buf[byte] &= ~(1 << bit);

            g_bgd_table[grp].bg_free_blocks_count++;
            g_superblock.s_free_blocks_count++;

            write_blocks(bitmap,
                         g_bgd_table[grp].bg_block_bitmap,
                         1);
            write_blocks(&g_superblock, 1, 1);
            write_blocks(&g_bgd_table, 2, 1);
        }
        return *last_bgd;
    }

    uint32_t ptr_blk = locations[0];
    if (ptr_blk == 0)
    {
        return *last_bgd;
    }

    struct BlockBuffer ptr_buf = {0};
    read_blocks(&ptr_buf, ptr_blk, 1);
    uint32_t *ptrs = (uint32_t *)ptr_buf.buf;

    uint32_t max_ptrs = BLOCK_SIZE / sizeof(uint32_t);
    uint32_t cnt = (blocks < max_ptrs ? blocks : max_ptrs);

    *last_bgd = deallocate_block(ptrs,
                                 cnt,
                                 bitmap,
                                 depth - 1,
                                 last_bgd,
                                 true);

    uint32_t grp = ptr_blk / BLOCKS_PER_GROUP;
    if (!bgd_loaded || grp != *last_bgd)
    {
        read_blocks(bitmap,
                    g_bgd_table[grp].bg_block_bitmap,
                    1);
        *last_bgd = grp;
        bgd_loaded = true;
    }
    uint32_t local = ptr_blk % BLOCKS_PER_GROUP;
    uint32_t byte = local / 8;
    uint32_t bit = local % 8;
    bitmap->buf[byte] &= ~(1 << bit);

    g_bgd_table[grp].bg_free_blocks_count++;
    g_superblock.s_free_blocks_count++;

    write_blocks(bitmap,
                 g_bgd_table[grp].bg_block_bitmap,
                 1);
    write_blocks(&g_superblock, 1, 1);
    write_blocks(&g_bgd_table, 2, 1);

    return *last_bgd;
};

void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd)
{
    uint32_t bytes_to_write = node->i_size;
    uint32_t bytes_written = 0;
    uint32_t blocks_allocated = 0;
    uint8_t *data_ptr = (uint8_t *)ptr;

    uint32_t pointers_per_block = BLOCK_SIZE / sizeof(uint32_t);

    for (int i = 0; i < 12 && bytes_written < bytes_to_write; i++)
    {
        uint32_t new_block = allocate_block(prefered_bgd);
        if (new_block == 0)
            return; // Disk penuh
        node->i_block[i] = new_block;
        blocks_allocated++;

        uint32_t write_size = (bytes_to_write - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (bytes_to_write - bytes_written);

        if (ptr)
        {
            write_blocks(data_ptr + bytes_written, new_block, 1);
        }
        else
        {
            uint8_t empty_buffer[BLOCK_SIZE];
            memset(empty_buffer, 0, BLOCK_SIZE);
            write_blocks(empty_buffer, new_block, 1);
        }
        bytes_written += write_size;
    }

    // 2. Single Indirect Block
    if (bytes_written < bytes_to_write)
    {
        uint32_t indirect_table[pointers_per_block];
        memset(indirect_table, 0, BLOCK_SIZE);

        uint32_t indirect_block_ptr = allocate_block(prefered_bgd);
        if (indirect_block_ptr == 0)
            return;
        node->i_block[12] = indirect_block_ptr;
        blocks_allocated++;

        for (int j = 0; j < (int)pointers_per_block && bytes_written < bytes_to_write; j++)
        {
            uint32_t new_block = allocate_block(prefered_bgd);
            if (new_block == 0)
                break; // Disk penuh, hentikan alokasi
            indirect_table[j] = new_block;
            blocks_allocated++;

            uint32_t write_size = (bytes_to_write - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (bytes_to_write - bytes_written);
            if (ptr)
                write_blocks(data_ptr + bytes_written, new_block, 1);
            bytes_written += write_size;
        }
        write_blocks(indirect_table, indirect_block_ptr, 1);
    }

    // 3. Doubly Indirect Block
    if (bytes_written < bytes_to_write)
    {
        uint32_t d_indirect_table[pointers_per_block];
        memset(d_indirect_table, 0, BLOCK_SIZE);

        uint32_t d_indirect_block_ptr = allocate_block(prefered_bgd);
        if (d_indirect_block_ptr == 0)
            return;
        node->i_block[13] = d_indirect_block_ptr;
        blocks_allocated++;

        for (int j = 0; j < (int)pointers_per_block && bytes_written < bytes_to_write; j++)
        {
            uint32_t indirect_table[pointers_per_block];
            memset(indirect_table, 0, BLOCK_SIZE);

            uint32_t indirect_block_ptr = allocate_block(prefered_bgd);
            if (indirect_block_ptr == 0)
                break;
            d_indirect_table[j] = indirect_block_ptr;
            blocks_allocated++;

            for (int k = 0; k < (int)pointers_per_block && bytes_written < bytes_to_write; k++)
            {
                uint32_t new_block = allocate_block(prefered_bgd);
                if (new_block == 0)
                    break;
                indirect_table[k] = new_block;
                blocks_allocated++;

                uint32_t write_size = (bytes_to_write - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (bytes_to_write - bytes_written);
                if (ptr)
                    write_blocks(data_ptr + bytes_written, new_block, 1);
                bytes_written += write_size;
            }
            write_blocks(indirect_table, indirect_block_ptr, 1);
        }
        write_blocks(d_indirect_table, d_indirect_block_ptr, 1);
    }

    node->i_blocks = blocks_allocated;
};

void sync_node(struct EXT2Inode *node, uint32_t inode)
{
    uint32_t group = inode_to_bgd(inode);
    uint32_t local_idx = inode_to_local(inode);
    uint32_t table_start_block = g_bgd_table[group].bg_inode_table;

    uint32_t block_offset = local_idx / INODES_PER_TABLE;
    uint32_t index_in_block = local_idx % INODES_PER_TABLE;
    uint32_t block_to_rw = table_start_block + block_offset;

    memset(buffer, 0, BLOCK_SIZE);
    read_blocks(buffer, block_to_rw, 1);

    struct EXT2Inode *inode_table_in_block = (struct EXT2Inode *)buffer;

    memcpy(&inode_table_in_block[index_in_block], node, sizeof(struct EXT2Inode));

    write_blocks(buffer, block_to_rw, 1);
};