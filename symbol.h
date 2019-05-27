#ifndef __SYMBOL_H_
#define __SYMBOL_H_

#include <libelf.h>
#include <gelf.h>

Elf_Scn *elf_section_by_name(Elf *elf, GElf_Ehdr *ep,
                             GElf_Shdr *shp, const char *name, size_t *idx);

#endif // __SYMBOL_H_
