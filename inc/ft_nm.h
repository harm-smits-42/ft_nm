#pragma once

#define ERR_NO_SYMS 1
#define ERR_NO_MEM  2

#include <elf.h>
#include <libft/stdlib.h>
#include <libft/stl/list.h>
#include <stdlib.h>

#define INVCL  -2
#define NOTELF -1
#define ISERR  0
#define ARCH   3
#define ELF32  32
#define ELF64  64

#define FLAG_DEBUG_SYMBOLS  0b10000
#define FLAG_EXTERN_ONLY    0b01000
#define FLAG_NO_SORT        0b00100
#define FLAG_REV_SORT       0b00010
#define FLAG_UNDEFINED_ONLY 0b00001

struct symbol_entry {
    struct list_head list;
    unsigned long st_value;
    char *st_name;
    char type;
};

#define swap16(number, endian) ((endian) == ELFDATA2MSB ? __builtin_bswap16(number) : (number))
#define swap32(number, endian) ((endian) == ELFDATA2MSB ? __builtin_bswap32(number) : (number))
#define swap64(number, endian) ((endian) == ELFDATA2MSB ? __builtin_bswap64(number) : (number))

#define ptr_in(ptr, mem, size)             ((void *) ptr >= (void *) mem && (void *) ptr <= (void *) mem + size)
#define ptr_in_strict(ptr, min, mem, size) ((void *) ptr >= (void *) mem && (void *) ptr <= (void *) mem + size && (void *) ptr + min <= (void *) mem + size)
#define ptr_max_size(ptr, mem, size)       (((void *) ptr >= (void *) mem && (void *) ptr <= (void *) mem + size) ? (void *) mem + size - (void *) ptr : 0)

int parse_elf_64(unsigned char *mem, size_t size, int flags);

int parse_elf_32(unsigned char *mem, size_t size, int flags);

int parse_archive(const char *file, unsigned char *mem, size_t size, int flags);

int parse_magic(char *ptr, size_t size);

void print_symbols(struct list_head *symbol_list);

void free_symbols(struct list_head *symbol_list);