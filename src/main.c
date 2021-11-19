#include <ar.h>
#include <elf.h>
#include <fcntl.h>
#include <libft/ctype.h>
#include <libft/stdbool.h>
#include <libft/stdio.h>
#include <libft/stdlib.h>
#include <libft/stl/list.h>
#include <libft/string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "ft_nm.h"

static int flags = 0;
static char *mem = NULL;

int parse_magic(char *ptr, size_t size) {
    if (size > SARMAG && !ft_strncmp(ptr, ARMAG, SARMAG)) {
        return ARCH;
    }

    if (size < EI_CLASS) {
        return INVCL;
    }

    if (ft_strncmp(ptr, ELFMAG, SELFMAG)) {
        return NOTELF;
    }

    if (ptr[EI_CLASS] == ELFCLASS32) {
        return ELF32;
    } else if (ptr[EI_CLASS] == ELFCLASS64) {
        return ELF64;
    } else {
        return INVCL;
    }
}

void free_symbols(struct list_head *symbol_list) {
    struct list_head *iter, *q;
    list_for_each_safe(iter, q, symbol_list) {
        list_del(iter);
        ft_free(iter);
    }
}

void print_symbols(struct list_head *symbol_list) {
    struct symbol_entry *iter;
    list_for_each_entry(iter, symbol_list, list) {
        if (iter->type == 'w' || iter->type == 'U') {
            ft_printf("%18c %s\n", iter->type, iter->st_name);
            continue;
        }

        if (flags & FLAG_EXTERN_ONLY && !ft_isupper(iter->type)) {
            continue;
        }

        if (flags & FLAG_UNDEFINED_ONLY) {
            continue;
        }

        ft_printf("%016lx %c %s\n", iter->st_value, iter->type, iter->st_name);
    }
}

static int parse_file(const char *file, bool is_multiple) {
    const int fd = open(file, O_RDONLY);

    if (fd < 0) {
        ft_perror("Unable to open file");
    }

    struct stat file_info;

    if (fstat(fd, &file_info) < 0) {
        ft_perror("Unable to get buffer data");
    }

    if (!(mem = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
        ft_perror("Unable to mmap memory");
    }

    int class = parse_magic(mem, file_info.st_size), parse_result = 0;

    if (is_multiple) {
        ft_printf("\n%s:\n", file);
    }

    if (class == ELF32) {
        parse_result = parse_elf_32(mem, file_info.st_size, flags);
    } else if (class == ELF64) {
        parse_result = parse_elf_64(mem, file_info.st_size, flags);
    } else if (class == ARCH) {
        parse_result = parse_archive(file, mem, file_info.st_size, flags);
    } else if (class == NOTELF) {
        ft_dprintf(2, "ft_nm: %s: file format not recognized\n", file);
    }

    int result = 0;

    if (parse_result == ERR_NO_SYMS) {
        ft_printf("ft_nm: %s: no symbols\n", file);
    } else if (parse_result == ERR_NO_MEM) {
        result = 1;
        ft_printf("ft_nm: not enough memory\n");
    }

    munmap(mem, file_info.st_size);

    return result;
}

int main(int argc, char **argv) {
    int ch;
    getopt_args_t args = FT_GETOPT_INITIALIAZER;

    while ((ch = ft_getopt_arg(argc, argv, "argpu", &args)) != EOF) {
        switch (ch) {
            case 'r':
                if (flags & FLAG_NO_SORT) {
                    ft_printf("ft_nm: conflicting option -p\n");
                    return 1;
                }

                flags |= FLAG_REV_SORT;
                break;

            case 'p':
                if (flags & FLAG_REV_SORT) {
                    ft_printf("ft_nm: conflicting option -r\n");
                    return 1;
                }

                flags |= FLAG_NO_SORT;
                break;

            case 'u':
                flags |= FLAG_UNDEFINED_ONLY;
                break;

            case 'g':
                flags |= FLAG_EXTERN_ONLY;
                break;

            case 'a':
                flags |= FLAG_DEBUG_SYMBOLS;
                break;

            default:
                return 1;
        }
    }

    argc -= args.optind;
    argv += args.optind;

    bool is_multiple = argc > 1;

    if (!argc) {
        return parse_file("a.out", false);
    }

    int result = 0;

    for (int i = 0; i < argc; i++) {
        result |= parse_file(argv[i], is_multiple);
    }

    return result;
}
