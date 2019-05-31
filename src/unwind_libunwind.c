#include "unwind.h"
#include "ptrace.h"
#include "thread.h"
#include "symbol.h"
#include "utility.h"
#include "dso.h"
#include "map.h"
#include "libdw_bcc.h"
#include <libunwind.h>
#include <libunwind-x86_64.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <gelf.h>

#ifdef debug
#undef debug
#define debug(args...) ""
#endif

extern int
UNW_OBJ(dwarf_search_unwind_table) (unw_addr_space_t as,
                                    unw_word_t ip,
                                    unw_dyn_info_t *di,
                                    unw_proc_info_t *pi,
                                    int need_unwind_info, void *arg);

#define dwarf_search_unwind_table UNW_OBJ(dwarf_search_unwind_table)

#define DW_EH_PE_FORMAT_MASK    0x0f    /* format of the encoded value */
#define DW_EH_PE_APPL_MASK      0x70    /* how the value is to be applied */

/* Pointer-encoding formats: */
#define DW_EH_PE_omit           0xff
#define DW_EH_PE_ptr            0x00    /* pointer-sized unsigned value */
#define DW_EH_PE_udata4         0x03    /* unsigned 32-bit value */
#define DW_EH_PE_udata8         0x04    /* unsigned 64-bit value */
#define DW_EH_PE_sdata4         0x0b    /* signed 32-bit value */
#define DW_EH_PE_sdata8         0x0c    /* signed 64-bit value */


/* Pointer-encoding application: */
#define DW_EH_PE_absptr         0x00    /* absolute value */
#define DW_EH_PE_pcrel          0x10    /* rel. to addr. of encoded value */

/*
 * The following are not documented by LSB v1.3, yet they are used by
 * GCC, presumably they aren't documented by LSB since they aren't
 * used on Linux:
 */
#define DW_EH_PE_funcrel        0x40    /* start-of-procedure-relative */
#define DW_EH_PE_aligned        0x50    /* aligned pointer */

struct unwind_ctx;

struct table_entry {
     u32 start_ip_offset;
     u32 fde_offset;
};

struct eh_frame_hdr {
     unsigned char version;
     unsigned char eh_frame_ptr_enc;
     unsigned char fde_count_enc;
     unsigned char table_enc;

     /*
      * The rest of the header is variable-length and consists of the
      * following members:
      *
      *   encoded_t eh_frame_ptr;
      *   encoded_t fde_count;
      */

     /* A single encoded pointer should not be more than 8 bytes. */
     u64 enc[2];

     /*
      * struct {
      *    encoded_t start_ip;
      *    encoded_t fde_addr;
      * } binary_search_table[fde_count];
      */
     char data[0];
} __packed;

struct unwind_info {
     struct unwind_ctx *uc;
     struct machine *machine;
     struct thread *thread;
};

#define dw_read(ptr, type, end) ({              \
               type *__p = (type *) ptr;        \
               type  __v;                       \
               if ((__p + 1) > (type *) end)    \
                    return -EINVAL;             \
               __v = *__p++;                    \
               ptr = (typeof(ptr)) __p;         \
               __v;                             \
          })

static int __dw_read_encoded_value(u8 **p, u8 *end, u64 *val, u8 encoding)
{
     u8 *cur = *p;
     *val = 0;

     switch (encoding) {
          case DW_EH_PE_omit:
               *val = 0;
               goto out;
          case DW_EH_PE_ptr:
               *val = dw_read(cur, unsigned long, end);
               goto out;
          default:
               break;
     }

     switch (encoding & DW_EH_PE_APPL_MASK) {
          case DW_EH_PE_absptr:
               break;
          case DW_EH_PE_pcrel:
               *val = (unsigned long) cur;
               break;
          default:
               return -EINVAL;
     }

     if ((encoding & 0x07) == 0x00)
          encoding |= DW_EH_PE_udata4;

     switch (encoding & DW_EH_PE_FORMAT_MASK) {
          case DW_EH_PE_sdata4:
               *val += dw_read(cur, s32, end);
               break;
          case DW_EH_PE_udata4:
               *val += dw_read(cur, u32, end);
               break;
          case DW_EH_PE_sdata8:
               *val += dw_read(cur, s64, end);
               break;
          case DW_EH_PE_udata8:
               *val += dw_read(cur, u64, end);
               break;
          default:
               return -EINVAL;
     }

out:
     *p = cur;
     return 0;
}

#define dw_read_encoded_value(ptr, end, enc) ({                       \
               u64 __v;                                               \
               if (__dw_read_encoded_value(&ptr, end, &__v, enc)) {   \
                    return -EINVAL;                                   \
               }                                                      \
               __v;                                                   \
          })

static u64 elf_section_offset(int fd, const char *name)
{
     Elf *elf;
     GElf_Ehdr ehdr;
     GElf_Shdr shdr;
     u64 offset = 0;

     elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
     if (elf == NULL)
          return 0;

     do {
          if (gelf_getehdr(elf, &ehdr) == NULL)
               break;

          if (!elf_section_by_name(elf, &ehdr, &shdr, name, NULL))
               break;

          offset = shdr.sh_offset;
     } while (0);

     elf_end(elf);
     return offset;
}

static inline struct map *find_map(unw_word_t ip, struct unwind_info *ui)
{
     /**
      * TODO:
      * 1. add a cache
      * 2. maybe need to handle dlopen's so here
      */
     struct map *map = maps__find(ui->thread->maps, ip);
     if (map)
          debug("find_map's name: %s\n", map->dso->name);
     return map;
}

static int unwind_spec_ehframe(struct dso *dso, struct machine *machine,
                               u64 offset, u64 *table_data, u64 *segbase,
                               u64 *fde_count)
{
     struct eh_frame_hdr hdr;
     u8 *enc = (u8 *) &hdr.enc;
     u8 *end = (u8 *) &hdr.data;
     ssize_t r;

     r = dso__data_read_offset(dso, machine, offset,
                               (u8 *) &hdr, sizeof(hdr));
     if (r != sizeof(hdr))
          return -EINVAL;

     /* We dont need eh_frame_ptr, just skip it. */
     dw_read_encoded_value(enc, end, hdr.eh_frame_ptr_enc);

     *fde_count  = dw_read_encoded_value(enc, end, hdr.fde_count_enc);
     *segbase    = offset;
     *table_data = (enc - (u8 *) &hdr) + offset;
     return 0;
}

static int read_unwind_spec_eh_frame(struct dso *dso, struct machine *machine,
                                     u64 *table_data, u64 *segbase,
                                     u64 *fde_count)
{
     int ret = -EINVAL, fd;
     u64 offset = dso->data.eh_frame_hdr_offset;

     if (offset == 0) {
          fd = dso__data_get_fd(dso, machine);
          if (fd < 0)
               return -EINVAL;

          /* Check the .eh_frame section for unwinding info */
          offset = elf_section_offset(fd, ".eh_frame_hdr");
          dso->data.eh_frame_hdr_offset = offset;
          dso__data_put_fd(dso);
     }

     if (offset)
          ret = unwind_spec_ehframe(dso, machine, offset,
                                    table_data, segbase,
                                    fde_count);

     return ret;
}

static int
find_proc_info(unw_addr_space_t as, unw_word_t ip, unw_proc_info_t *pi,
               int need_unwind_info, void *arg)
{
     struct unwind_info *ui = arg;
     struct map *map;
     unw_dyn_info_t di;
     u64 table_data, segbase, fde_count;
     int ret = -EINVAL;

     debug("find_proc_info called\n");

     map = find_map(ip, ui);
     if (!map || !map->dso)
          return -EINVAL;

     if (!read_unwind_spec_eh_frame(map->dso, ui->machine,
                                    &table_data, &segbase, &fde_count)) {
          memset(&di, 0, sizeof(di));
          di.format   = UNW_INFO_FORMAT_REMOTE_TABLE;
          di.start_ip = map->start;
          di.end_ip   = map->end;
          di.u.rti.segbase    = map->start + segbase - map->pgoff;
          di.u.rti.table_data = map->start + table_data - map->pgoff;
          di.u.rti.table_len  = fde_count * sizeof(struct table_entry)
               / sizeof(unw_word_t);
          ret = dwarf_search_unwind_table(as, ip, &di, pi,
                                          need_unwind_info, arg);
     }

     return ret;
}

static void put_unwind_info(unw_addr_space_t as __maybe_unused,
                            unw_proc_info_t *pi __maybe_unused,
                            void *arg __maybe_unused)
{
     fprintf(stderr, "unwind: put_unwind_info called\n");
}

static int get_dyn_info_list_addr(unw_addr_space_t as __maybe_unused,
                                  unw_word_t *dil_addr __maybe_unused,
                                  void __maybe_unused *arg)
{
     return -UNW_ENOINFO;
}

static int access_dso_mem(struct unwind_info *ui, unw_word_t addr,
                          unw_word_t *data)
{
     struct map *map;
     ssize_t size;

     map = find_map(addr, ui);
     if (!map) {
          fprintf(stderr, "unwind: no map for %lx\n", (unsigned long)addr);
          return -1;
     }

     if (!map->dso)
          return -1;

     size = dso__data_read_addr(map->dso, map, ui->machine,
                                addr, (u8 *)data, sizeof(*data));

     return !(size == sizeof(*data));
}

static int access_mem(unw_addr_space_t as __maybe_unused,
                      unw_word_t addr, unw_word_t *valp,
                      int __write, void *arg)
{
     struct unwind_info *ui = arg;
     const char * const stack = ui->uc->data;
     int ss = ui->uc->size;
     u64 start, end;
     int offset;
     int ret;

     if (__write || !stack || ss <= 0) {
          *valp = 0;
          return 0;
     }

     start = reg_value(&ui->uc->uregs, LIBUNWIND__ARCH_REG_SP);
     end = start + ss;
     debug("start: 0x%" PRIx64 " end: 0x%" PRIx64 "ss: %d\n",
           start, end, ss);

     if (addr + sizeof(unw_word_t) < addr)
          return -EINVAL;

     if (addr < start || addr + sizeof(unw_word_t) >= end) {
          ret = access_dso_mem(ui, addr, valp);
          if (ret) {
               *valp = 0;
               return ret;
          }
          return 0;
     }

     offset = addr - start;
     *valp = *(unw_word_t*)&stack[offset];
     debug("access addr: 0x%lx, stack[%d]: 0x%lx\n", addr, offset, *valp);

     return 0;
}

static int access_reg(unw_addr_space_t as __maybe_unused,
                      unw_regnum_t regnum, unw_word_t *valp,
                      int __write, void *arg)
{
     struct unwind_info *ui = arg;
     int id;

     if (__write) {
          return 0;
     }

     id = LIBUNWIND__ARCH_REG_ID(regnum);
     *valp = reg_value(&ui->uc->uregs, id);
     debug("access_reg %d: %lx\n", id, *valp);

     return 0;
}

static int access_fpreg(unw_addr_space_t as __maybe_unused,
                        unw_regnum_t num __maybe_unused,
                        unw_fpreg_t *val __maybe_unused,
                        int __write __maybe_unused,
                        void *arg __maybe_unused)
{
     fprintf(stderr, "unwind: access_fpreg unsupported\n");
     return -UNW_EINVAL;
}

static int resume(unw_addr_space_t as __maybe_unused,
                  unw_cursor_t *cu __maybe_unused,
                  void *arg __maybe_unused)
{
     fprintf(stderr, "unwind: resume unsupported\n");
     return -UNW_EINVAL;
}

static int
get_proc_name(unw_addr_space_t as __maybe_unused,
              unw_word_t addr __maybe_unused,
              char *bufp __maybe_unused, size_t buf_len __maybe_unused,
              unw_word_t *offp __maybe_unused, void *arg __maybe_unused)
{
     fprintf(stderr, "unwind: get_proc_name unsupported\n");
     return -UNW_EINVAL;
}

static unw_accessors_t accessors = {
    .find_proc_info         = find_proc_info,
    .put_unwind_info        = put_unwind_info,
    .get_dyn_info_list_addr = get_dyn_info_list_addr,
    .access_mem             = access_mem,
    .access_reg             = access_reg,
    .access_fpreg           = access_fpreg,
    .resume                 = resume,
    .get_proc_name          = get_proc_name,
};

static int _prepare_access(struct thread *thread)
{
     thread->addr_space = unw_create_addr_space(&accessors, 0);
     if (!thread->addr_space) {
          fprintf(stderr, "unwind: create unwind as failed.\n");
          return -ENOMEM;
     }

     unw_set_caching_policy(thread->addr_space, UNW_CACHE_GLOBAL);
     return 0;
}

static void _flush_access(struct thread *thread)
{
     unw_flush_cache(thread->addr_space, 0, 0);
}

static void _finish_access(struct thread *thread)
{
     unw_destroy_addr_space(thread->addr_space);
}

static void display_error(int err)
{
     switch (err) {
          case UNW_EINVAL:
               fprintf(stderr, "unwind: Only supports local.\n");
               break;
          case UNW_EUNSPEC:
               fprintf(stderr, "unwind: Unspecified error.\n");
               break;
          case UNW_EBADREG:
               fprintf(stderr, "unwind: Register unavailable.\n");
               break;
          default:
               break;
     }
}

static int get_entries(struct unwind_info *ui,
                       unwind_entry_cb_t cb __maybe_unused,
                       void *arg __maybe_unused,
                       struct stacktrace *st)
{
     unw_addr_space_t addr_space;
     unw_cursor_t c;
     u64 val;
     int ret, i = 0;

     if (!st || !st->ips || st->depth < 1) {
          fprintf(stderr, "stacktrace not init\n");
          return EINVAL;
     }

     val = reg_value(&ui->uc->uregs, LIBUNWIND__ARCH_REG_IP);
     st->ips[i++] = val;
     debug("get_entries, ip: 0x%" PRIx64 "\n", val);

     addr_space = ui->thread->addr_space;
     if (!addr_space)
          return -1;

     ret = unw_init_remote(&c, addr_space, ui);
     if (ret)
          display_error(ret);

     debug("ready to run unw_step...\n");

     while (!ret && (unw_step(&c) > 0) && i < st->depth) {
          unw_get_reg(&c, UNW_REG_IP, &st->ips[i]);

          /*
           * Decrement the IP for any non-activation frames.
           * this is required to properly find the srcline
           * for caller frames.
           * See also the documentation for dwfl_frame_pc(),
           * which this code tries to replicate.
           */
          if (unw_is_signal_frame(&c) <= 0)
               --st->ips[i];

          ++i;
     }

     st->depth = i;
     debug("update st->depth: %d\n", st->depth);

     return ret;
}

static int _get_entries(unwind_entry_cb_t cb,
                        void *arg,
                        struct thread *thread,
                        struct unwind_ctx *uc,
                        struct stacktrace *st)
{
     struct unwind_info ui = {
         .uc = uc,
         .machine = thread->maps->machine,
         .thread = thread,
     };
     return get_entries(&ui, cb, arg, st);
}

static struct unwind_libunwind_ops unwind_libunwind_ops = {
    .prepare_access = _prepare_access,
    .flush_access   = _flush_access,
    .finish_access  = _finish_access,
    .get_entries    = _get_entries,
};

int LIBUNWIND__ARCH_REG_ID(int regnum)
{
     int id;

     switch (regnum) {
          case UNW_X86_64_RAX:
               id = X86_AX;
               break;
          case UNW_X86_64_RDX:
               id = X86_DX;
               break;
          case UNW_X86_64_RCX:
               id = X86_CX;
               break;
          case UNW_X86_64_RBX:
               id = X86_BX;
               break;
          case UNW_X86_64_RSI:
               id = X86_SI;
               break;
          case UNW_X86_64_RDI:
               id = X86_DI;
               break;
          case UNW_X86_64_RBP:
               id = X86_BP;
               break;
          case UNW_X86_64_RSP:
               id = X86_SP;
               break;
          case UNW_X86_64_R8:
               id = X86_R8;
               break;
          case UNW_X86_64_R9:
               id = X86_R9;
               break;
          case UNW_X86_64_R10:
               id = X86_R10;
               break;
          case UNW_X86_64_R11:
               id = X86_R11;
               break;
          case UNW_X86_64_R12:
               id = X86_R12;
               break;
          case UNW_X86_64_R13:
               id = X86_R13;
               break;
          case UNW_X86_64_R14:
               id = X86_R14;
               break;
          case UNW_X86_64_R15:
               id = X86_R15;
               break;
          case UNW_X86_64_RIP:
               id = X86_IP;
               break;
          default:
               fprintf(stderr, "unwind: invalid reg id %d\n", regnum);
               return -EINVAL;
     }

     return id;
}

static inline void unwind_register_ops(struct thread *thread,
                                       struct unwind_libunwind_ops *ops)
{
     thread->ulops = ops;
}

int unwind__prepare_access(struct thread *thread, struct map *map,
                          bool *initialized)
{
     int err;

     if (thread->addr_space) {
          if (!map)
               debug("thread map already set, dso=%s, thread=%p\n",
                     map->dso->name, thread);
          if (initialized)
               *initialized = true;
          return 0;
     }

     unwind_register_ops(thread, &unwind_libunwind_ops);
     err = thread->ulops->prepare_access(thread);
     if (initialized)
          *initialized = err ? false : true;

     return err;
}

void unwind__flush_access(struct thread *thread)
{
     if (thread->ulops)
          thread->ulops->flush_access(thread);
}

void unwind__finish_access(struct thread *thread)
{
     if (thread->ulops)
          thread->ulops->finish_access(thread);
}

int unwind__get_entries(unwind_entry_cb_t cb, void *arg,
                       struct thread *thread,
                       struct unwind_ctx *data,
                       struct stacktrace *st)
{
     if (thread->ulops)
          return thread->ulops->get_entries(cb, arg, thread, data, st);
     return 0;
}
