#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "header/filesystem/ext2.h"
#include "header/driver/disk.h"
#include "header/stdlib/string.h"

uint8_t *image_storage;
uint8_t *file_buffer;
uint8_t *read_buffer;

void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count)
{
    for (int i = 0; i < block_count; i++)
    {
        memcpy(
            (uint8_t *)ptr + BLOCK_SIZE * i,
            image_storage + BLOCK_SIZE * (logical_block_address + i),
            BLOCK_SIZE);
    }
}

void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count)
{
    for (int i = 0; i < block_count; i++)
    {
        memcpy(
            image_storage + BLOCK_SIZE * (logical_block_address + i),
            (uint8_t *)ptr + BLOCK_SIZE * i,
            BLOCK_SIZE);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "inserter: ./inserter <file to insert> <parent cluster index> <storage>\n");
        exit(1);
    }

    image_storage = malloc(4 * 1024 * 1024);
    file_buffer = malloc(4 * 1024 * 1024);
    read_buffer = malloc(4 * 1024 * 1024);

    if (image_storage == NULL || file_buffer == NULL || read_buffer == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory.\n");
        exit(1);
    }

    FILE *fptr = fopen(argv[3], "r");
    if (fptr == NULL)
    {
        fprintf(stderr, "Error: Could not open storage file %s\n", argv[3]);
        exit(1);
    }
    fread(image_storage, 4 * 1024 * 1024, 1, fptr);
    fclose(fptr);

    FILE *fptr_target = fopen(argv[1], "r");
    size_t filesize = 0;
    if (fptr_target == NULL)
    {
        fprintf(stderr, "Warning: Could not open file %s. Assuming 0 filesize.\n", argv[1]);
        exit(1);
    }
    else
    {
        filesize = fread(file_buffer, 1, 4 * 1024 * 1024, fptr_target);
        if (filesize == 4 * 1024 * 1024)
        {
            fprintf(stderr, "Warning: File may be larger than 4MB and truncated.\n");
        }
        fclose(fptr_target);
    }

    printf("Filename : %s\n", argv[1]);
    printf("Filesize : %ld bytes\n", filesize);

    initialize_filesystem_ext2();

    char *name = argv[1];
    uint8_t filename_length = (uint8_t)strlen(name);

    bool is_replace = false;

    struct EXT2DriverRequest request;
    struct EXT2DriverRequest reqread;
    printf("Filename       : %s\n", name);
    printf("Filename length: %d\n", filename_length);

    request.buf = file_buffer;
    request.buffer_size = filesize;
    request.name = name;
    request.name_len = filename_length;
    request.is_directory = false;
    sscanf(argv[2], "%u", &request.parent_inode);

    reqread = request;
    reqread.buf = read_buffer;
    int retcode = read(reqread);
    if (retcode == 0)
    {
        bool same = true;
        for (uint32_t i = 0; i < filesize; i++)
        {
            if (read_buffer[i] != file_buffer[i])
            {
                printf("not same\n");
                same = false;
                break;
            }
        }
        if (same)
        {
            printf("same\n");
        }
    }

    retcode = write(&request);
    if (retcode == 1 && is_replace)
    {
        printf("Note: File exists, replace logic is defined but commented out for safety.\n");
    }

    if (retcode == 0)
        puts("Write success");
    else if (retcode == 1)
        puts("Error: File/folder name already exist");
    else if (retcode == 2)
        puts("Error: Invalid parent node index");
    else
        puts("Error: Unknown error");

    fptr = fopen(argv[3], "w");
    if (fptr == NULL)
    {
        fprintf(stderr, "Error: Could not open storage file %s for writing.\n", argv[3]);
        exit(1);
    }
    fwrite(image_storage, 4 * 1024 * 1024, 1, fptr);
    fclose(fptr);

    free(image_storage);
    free(file_buffer);
    free(read_buffer);

    return 0;
}