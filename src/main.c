#include <libft/stdio.h>
#include <fcntl.h>
#include <elf.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libft/ctype.h>
#include <libft/stdlib.h>
#include <libft/string.h>
#include <libft/stl/list.h>
#include <stdlib.h>
#include <stdio.h>

#define FLAG_DEBUG_SYMBOLS  0b10000
#define FLAG_EXTERN_ONLY    0b01000
#define FLAG_NO_SORT        0b00100
#define FLAG_REV_SORT       0b00010
#define FLAG_UNDEFINED_ONLY 0b00001

static int flags = 0;

struct symbol_entry {
    struct list_head list;
    Elf64_Addr st_value;
    char *st_name;
    char type;
};

static char *mem;
static Elf64_Shdr *symtab, *strtab;
static Elf64_Ehdr *elf_header;
static Elf64_Shdr *shdr;
static char *str;

struct section_to_type {
    const char *section;
    char type;
};

/* Map section names to POSIX/BSD single-character symbol types.
   This table is probably incomplete.  It is sorted for convenience of
   adding entries.  Since it is so short, a linear search is used.  */
static const struct section_to_type stt[] = {
    {".bss",     'b'},
    {"code",     't'},
    {".data",    'd'},
    {"*DEBUG*",  'N'},
    {".debug",   'N'},
    {".drectve", 'i'},
    {".edata",   'e'},
    {".fini",    't'},
    {".idata",   'i'},
    {".init",    't'},
    {".pdata",   'p'},
    {".rdata",   'r'},
    {".rodata",  'r'},
    {".sbss",    's'},
    {".scommon", 'c'},
    {".sdata",   'g'},
    {".text",    't'},
    {"vars",     'd'},
    {"zerovars", 'b'},
    {0,          0}
};

static char coff_section_type(const char *s) {
    for (const struct section_to_type *t = &stt[0]; t->section; t++) {
        if (!ft_strncmp(s, t->section, ft_strlen(t->section))) {
            return t->type;
        }
    }

    return '?';
}

static char decode_section_type(Elf64_Sym *sym, Elf64_Shdr *symbol_header) {
    if (ELF64_ST_TYPE(sym->st_info) == STT_FUNC || symbol_header->sh_flags & SHF_EXECINSTR) {
        return 't';
    }

#ifdef DEBUG
    char *name = str + sym->st_name;
    ft_printf("flags %s type %d flags: %lX\n", name, symbol_header->sh_type, symbol_header->sh_flags);
#endif

    if (
        symbol_header->sh_type == SHT_PROGBITS
        || symbol_header->sh_type == SHT_HASH
        || symbol_header->sh_type == SHT_NOTE
        || symbol_header->sh_type == SHT_INIT_ARRAY
        || symbol_header->sh_type == SHT_FINI_ARRAY
        || symbol_header->sh_type == SHT_PREINIT_ARRAY
        || symbol_header->sh_type == SHT_GNU_LIBLIST
        || symbol_header->sh_type == SHT_GNU_HASH
        || symbol_header->sh_type == SHT_DYNAMIC
    ) {
        if (!(symbol_header->sh_flags & SHF_WRITE)) {
            return 'r';
        } else if (symbol_header->sh_flags & SHF_COMPRESSED) {
            return 'g';
        } else {
            return 'd';
        }
    }

    if (symbol_header->sh_flags & SHF_ALLOC) {
        if (symbol_header->sh_flags & SHF_COMPRESSED) {
            return 's';
        } else {
            return 'b';
        }
    }

//    if (section->fmakelags & SEC_DEBUGGING)
//        return 'N';
//    if ((section->flags & SEC_HAS_CONTENTS) && (section->flags & SEC_READONLY))
//        return 'n';

    return '?';
}

char symbol_get_type(Elf64_Sym *sym) {
    char c = 0;

    if (sym->st_shndx == SHN_COMMON) {
        return 'C';
    }

    if (sym->st_shndx == SHN_UNDEF) {
        if (ELF64_ST_BIND(sym->st_info) == STB_WEAK) {
            if (ELF64_ST_TYPE(sym->st_info) == STT_OBJECT) {
                return 'v';
            } else {
                return 'w';
            }
        } else {
            return 'U';
        }
    }

    if (ELF64_ST_TYPE(sym->st_info) == STT_GNU_IFUNC) {
        return 'i';
    }

    if (ELF64_ST_BIND(sym->st_info) == STB_WEAK) {
        if (ELF64_ST_TYPE(sym->st_info) == STT_OBJECT) {
            return 'V';
        } else {
            return 'W';
        }
    }

    if (ELF64_ST_BIND(sym->st_info) == STB_GNU_UNIQUE) {
        return 'u';
    }

    if (ELF64_ST_BIND(sym->st_info) != STB_GLOBAL && ELF64_ST_BIND(sym->st_info) != STB_LOCAL) {
        return '?';
    }

    if (sym->st_shndx == SHN_ABS) {
        c = 'a';
    } else {
        Elf64_Shdr *symbol_header = &shdr[sym->st_shndx];
        char *name = str + symbol_header->sh_name;

        if (sym->st_shndx < SHN_ABS) {
            c = coff_section_type(name);
        }

        if (!c || c == '?') {
            c = decode_section_type(sym, symbol_header);
        }
    }

    if (ELF64_ST_BIND(sym->st_info) == STB_GLOBAL) {
        if (c >= 'a' && c <= 'z') {
            c = c - 32;
        } else {
            c = c;
        }
    }

    return c;
}

static int compare_types(void *priv, const struct list_head *lhs, const struct list_head *rhs) {
    struct symbol_entry
        *typed_lhs = list_entry(lhs, struct symbol_entry, list),
        *typed_rhs = list_entry(rhs, struct symbol_entry, list);

    if (flags & FLAG_REV_SORT) {
        return -ft_strcmp(typed_lhs->st_name, typed_rhs->st_name);
    } else {
        return ft_strcmp(typed_lhs->st_name, typed_rhs->st_name);
    }
}

static int parse_file(const char *file) {
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

    elf_header = (Elf64_Ehdr *) mem;
    shdr = (Elf64_Shdr *) (mem + elf_header->e_shoff);
    str = (char *) (mem + shdr[elf_header->e_shstrndx].sh_offset);

    for (size_t i = 0; i < elf_header->e_shnum; i++) {
        if (!shdr[i].sh_size) {
            continue;
        }

        if (!ft_strcmp(&str[shdr[i].sh_name], ".symtab")) {
            symtab = (Elf64_Shdr *) &shdr[i];
        }

        if (!ft_strcmp(&str[shdr[i].sh_name], ".strtab")) {
            strtab = (Elf64_Shdr *) &shdr[i];
        }
    }

    if (!symtab || !strtab) {
        ft_printf("No symtab or strtab\n");
        return 1;
    }

    Elf64_Sym *sym = (Elf64_Sym *) (mem + symtab->sh_offset);
    str = (char *) (mem + strtab->sh_offset);

    LIST_HEAD(symbol_list);

    for (size_t i = 0; i < symtab->sh_size / symtab->sh_entsize; i++) {
        char *name = str + sym[i].st_name;

        if (!ft_strnlen(name, 2)) {
            continue;
        }

        size_t len = ft_strlen(name);

        if (len >= 3 && name[len - 2] == '.' && name[len - 1] == 'c') {
            continue;
        }

        struct symbol_entry *entry = malloc(sizeof(struct symbol_entry));
        entry->st_name = name;
        entry->type = symbol_get_type(&sym[i]);
        entry->st_value = sym[i].st_value;
        list_add(&entry->list, &symbol_list);
    }

    if (!(flags & FLAG_NO_SORT)) {
        list_sort(NULL, &symbol_list, &compare_types);
    }

    {
        struct symbol_entry *iter;
        list_for_each_entry(iter, &symbol_list, list) {
            if (iter->type == 'w' || iter->type == 'U') {
                printf("%18c %s\n", iter->type, iter->st_name);
            } else if (iter->st_value != 0 || iter->type == 't' || iter->type == 'n' || iter->type == 'T') {
                printf("%016lx %c %s\n", iter->st_value, iter->type, iter->st_name);
            }
        }
    }

    {
        struct list_head *iter, *q;
        list_for_each_safe(iter, q, &symbol_list) {
            list_del(iter);
            free(iter);
        }
    }

    return 0;
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

            case 'u':flags |= FLAG_UNDEFINED_ONLY;
                break;

            case 'g':flags |= FLAG_EXTERN_ONLY;
                break;

            case 'a':flags |= FLAG_DEBUG_SYMBOLS;
                break;

            default:return 1;
        }
    }

    argc -= args.optind;
    argv += args.optind;

    for (int i = 0; i < argc; i++) {
        if (parse_file(argv[i])) {
            return 1;
        }
    }

    return 0;
}
