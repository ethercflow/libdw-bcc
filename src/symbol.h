#ifndef __SYMBOL_H_
#define __SYMBOL_H_

#include "utility.h"
#include <libelf.h>
#include <gelf.h>

Elf_Scn *elf_section_by_name(Elf *elf, GElf_Ehdr *ep,
                             GElf_Shdr *shp, const char *name, size_t *idx);

static inline int __symbol__join_symfs(char *bf, size_t size, const char *path)
{
    return path__join(bf, size, "", path);
}

#endif // __SYMBOL_H_
