#include "ft_nm.h"
#include <ar.h>
#include <elf.h>
#include <libft/stdlib.h>
#include <libft/string.h>
#include <libft/stdio.h>
#include <stdlib.h>

void filename(char *name, char *funcs, int err) {
    char *cpy;
    int i = 0;

    while (name[i] != '/') {
        i++;
    }

    if (!i && funcs) {
        filename(funcs + ft_atoi(name + 1), NULL, err);
    } else if (i) {
        cpy = ft_strndup(name, i);

        if (!err) {
            ft_printf("\n%s:\n", cpy);
        } else {
            ft_dprintf(STDERR_FILENO, "ft_nm: %s: file truncated\n", cpy);
        }

        ft_free(cpy);
    }
}

int parse_archive(const char *file, unsigned char *ptr, size_t size, int flags) {
    struct ar_hdr arc;
    int class;

    if (!ptr_in_strict(ptr + SARMAG, sizeof(arc), ptr, size)) {
        return 1;
    }

    ft_memcpy(&arc, ptr + SARMAG, sizeof(arc));
    ptr += SARMAG;
    size -= SARMAG;
    char *func = NULL;

    while (size >= sizeof(arc)) {
        if (!ptr_in_strict(ptr, sizeof(arc), ptr, size)) {
            return 1;
        }

        ft_memcpy(&arc, ptr, sizeof(arc));
        ptr += sizeof(arc);

        if (size < ft_atoi(arc.ar_size) + sizeof(arc)) {
            filename(arc.ar_name, func, 1);
            return 1;
        }

        size -= ft_atoi(arc.ar_size) + sizeof(arc);
        class = parse_magic(ptr, ft_atoi(arc.ar_size));

        if (class == ELF32 || class == ELF64) {
            filename(arc.ar_name, func, 0);
        }

        if (class == ELF32) {
            parse_elf_32(ptr, ft_atoi(arc.ar_size), flags);
        } else if (class == ELF64) {
            parse_elf_64(ptr, ft_atoi(arc.ar_size), flags);
        } else if (!func && !ft_strncmp("//              ", arc.ar_name, 16)) {
            func = ptr;
        }

        ptr += ft_atoi(arc.ar_size);
    }

    return 0;
}