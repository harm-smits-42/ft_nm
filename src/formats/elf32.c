#include <elf.h>
#include <libft/stdlib.h>
#include <libft/stl/list.h>
#include <libft/string.h>

#ifdef DEBUG
#include <libft/stdio.h>
#endif

#include "ft_nm.h"

static unsigned char *mem;
static size_t size;
static Elf32_Shdr *symtab = NULL, *strtab = NULL;
static Elf32_Ehdr *elf_header = NULL;
static Elf32_Shdr *shdr = NULL;
static char *str = NULL;
static char endian = 0;

struct section_to_type {
    const char *section;
    char type;
};

static const struct section_to_type stt[] = {
    {".bss", 'b'},
    {"*DEBUG*", 'N'},
    {".debug", 'N'},
    {".drectve", 'i'},
    {".edata", 'e'},
    {".fini", 't'},
    {".idata", 'i'},
    {".init", 't'},
    {".pdata", 'p'},
    {".rdata", 'r'},
    {".rodata", 'r'},
    {".sbss", 's'},
    {".scommon", 'c'},
    {".sdata", 'g'},
    {"vars", 'd'},
    {"zerovars", 'b'},
    {0, 0}
};

static char coff_section_type(const char *s) {
    for (const struct section_to_type *t = &stt[0]; t->section; t++) {
        if (!ft_strncmp(s, t->section, ft_strlen(t->section))) {
            return t->type;
        }
    }

    return '?';
}

static char decode_section_type(Elf32_Sym *sym, Elf32_Shdr *symbol_header) {
    if (ELF32_ST_TYPE(sym->st_info) == STT_FUNC || symbol_header->sh_flags & SHF_EXECINSTR) {
        return 't';
    }

    int sh_type = swap32(symbol_header->sh_type, endian),
        sh_flags = swap32(symbol_header->sh_flags, endian);

#ifdef DEBUG
    char *name = str + sym->st_name;
    ft_printf("flags %s type %d flags: %lX\n", name, symbol_header->sh_type, symbol_header->sh_flags);
#endif

    if (
        sh_type == SHT_PROGBITS
        || sh_type == SHT_HASH
        || sh_type == SHT_NOTE
        || sh_type == SHT_INIT_ARRAY
        || sh_type == SHT_FINI_ARRAY
        || sh_type == SHT_PREINIT_ARRAY
        || sh_type == SHT_GNU_LIBLIST
        || sh_type == SHT_GNU_HASH
        || sh_type == SHT_DYNAMIC
    ) {
        if (!(sh_flags & SHF_WRITE)) {
            return 'r';
        } else if (sh_flags & SHF_COMPRESSED) {
            return 'g';
        } else {
            return 'd';
        }
    }

    if (sh_flags & SHF_ALLOC) {
        if (sh_flags & SHF_COMPRESSED) {
            return 's';
        } else {
            return 'b';
        }
    }

    if (swap32(symbol_header->sh_offset, endian) && !(sh_flags & SHF_WRITE)) {
        return 'n';
    }

    return '?';
}

static char symbol_get_type(Elf32_Sym *sym) {
    char c = 0;

    if (swap16(sym->st_shndx, endian) == SHN_COMMON) {
        return 'C';
    }

    if (swap16(sym->st_shndx, endian) == SHN_UNDEF) {
        if (ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
            if (ELF32_ST_TYPE(sym->st_info) == STT_OBJECT) {
                return 'v';
            } else {
                return 'w';
            }
        } else {
            return 'U';
        }
    }

    if (ELF32_ST_TYPE(sym->st_info) == STT_GNU_IFUNC) {
        return 'i';
    }

    if (ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
        if (ELF32_ST_TYPE(sym->st_info) == STT_OBJECT) {
            return 'V';
        } else {
            return 'W';
        }
    }

    if (ELF32_ST_BIND(sym->st_info) != STB_GLOBAL && ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
        return 'u';
    }

    if (swap16(sym->st_shndx, endian) != SHN_ABS) {
        Elf32_Shdr *symbol_header = shdr + swap16(sym->st_shndx, endian);

        if (!ptr_in_strict(symbol_header, sizeof(Elf32_Shdr), mem, size)) {
            return '?';
        }

        char *name = str + swap32(symbol_header->sh_name, endian);

        if (!ptr_in(name, mem, size)) {
            return '?';
        }

        c = coff_section_type(name);
        if (c == '?') {
            c = decode_section_type(sym, symbol_header);
        }
    }

    if (ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) {
        if (c >= 'a' && c <= 'z') {
            c = c - 32;
        }
    }

    return c;
}

static int compare_types(void *priv, const struct list_head *lhs, const struct list_head *rhs) {
    struct symbol_entry
        *typed_lhs = list_entry(lhs, struct symbol_entry, list),
        *typed_rhs = list_entry(rhs, struct symbol_entry, list);

    int flags = *(int *) priv, result;

    if (flags & FLAG_REV_SORT) {
        result = -ft_strcmp(typed_lhs->st_name, typed_rhs->st_name);
    } else {
        result = ft_strcmp(typed_lhs->st_name, typed_rhs->st_name);
    }

    return result;
}

int parse_elf_32(unsigned char *_mem, size_t _size, int flags) {
    mem = _mem;
    size = _size;

    int err = 0;
    LIST_HEAD(symbol_list);

    if (size < sizeof(Elf32_Ehdr)) {
        err = ERR_NO_SYMS;
        goto err_out;
    }

    endian = mem[EI_DATA];
    elf_header = (Elf32_Ehdr *) mem;
    shdr = (Elf32_Shdr *) (mem + swap32(elf_header->e_shoff, endian));

    if (!ptr_in_strict(shdr + swap32(elf_header->e_shstrndx, endian), sizeof(Elf32_Shdr), mem, size)) {
        err = ERR_NO_SYMS;
        goto err_out;
    }

    str = (char *) (mem + swap64(shdr[swap32(elf_header->e_shstrndx, endian)].sh_offset, endian));

    if (!ptr_in(str, mem, size)) {
        err = ERR_NO_SYMS;
        goto err_out;
    }

    for (size_t i = 0; i < elf_header->e_shnum; i++) {
        if (!ptr_in_strict(shdr + i, sizeof(Elf32_Shdr), mem, size)) {
            err = ERR_NO_SYMS;
            goto err_out;
        }

        if (!shdr[i].sh_size || !ptr_in_strict(str + shdr[i].sh_name, 8, mem, size)) {
            continue;
        }

        if (!ft_strncmp(&str[shdr[i].sh_name], ".symtab", sizeof(".symtab"))) {
            symtab = (Elf32_Shdr *) &shdr[i];
        }

        if (!ft_strncmp(&str[shdr[i].sh_name], ".strtab", sizeof(".strtab"))) {
            strtab = (Elf32_Shdr *) &shdr[i];
        }
    }

    if (!ptr_in(symtab, mem, size) || !ptr_in(strtab, mem, size)) {
        err = ERR_NO_SYMS;
        goto err_out;
    }

    Elf32_Sym *sym = (Elf32_Sym *) (mem + swap32(symtab->sh_offset, endian));
    str = (char *) (mem + swap32(strtab->sh_offset, endian));

    if (!ptr_in(sym, mem, size) || !ptr_in(str, mem, size)) {
        err = ERR_NO_SYMS;
        goto err_out;
    }

    size_t entries = symtab->sh_size / symtab->sh_entsize;
    for (size_t i = 0; i < entries; i++) {
        if (!ptr_in(sym + i, mem, size)) {
            err = ERR_NO_SYMS;
            goto err_out;
        }

        if (ELF32_ST_TYPE(sym[i].st_info) == STT_FILE) {
            continue;
        }

        char *name = str + swap32(sym[i].st_name, endian);

        if (!ptr_in(name, mem, size)) {
            err = ERR_NO_SYMS;
            goto err_out;
        }

        size_t len = ft_strnlen(name, ptr_max_size(name, mem, size));

        if (!len || len == ptr_max_size(name, mem, size)) {
            continue;
        }

        char type = symbol_get_type(&sym[i]);

        if (!type) {
            continue;
        }

        struct symbol_entry *entry = ft_malloc(sizeof(struct symbol_entry));

        if (!entry) {
            err = ERR_NO_MEM;
            goto err_out;
        }

        entry->st_name = name;
        entry->type = type;

        if (swap16(sym[i].st_shndx, endian) == SHN_COMMON) {
            entry->st_value = swap32(sym[i].st_size, endian);
        } else {
            entry->st_value = swap32(sym[i].st_value, endian);
        }

        list_add_tail(&entry->list, &symbol_list);
    }

    if (!(flags & FLAG_NO_SORT)) {
        list_sort(&flags, &symbol_list, &compare_types);
    }

    print_symbols(&symbol_list);

err_out:;
    free_symbols(&symbol_list);

    return err;
}
