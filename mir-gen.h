/* This file is a part of MIR project.
   Copyright (C) 2018-2024 Vladimir Makarov <vmakarov.gcc@gmail.com>.
*/

#ifndef MIR_GEN_H

#define MIR_GEN_H

#include "mir.h"

#ifndef MIR_NO_GEN_DEBUG
#define MIR_NO_GEN_DEBUG 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Native images are same-target/ABI artifacts, not portable MIR bytecode.
   A NULL item denotes a code-base relocation (the code contains its offset).
   Otherwise the caller must serialize item identity, not its pointer, and
   supply the corresponding linked item when loading. call_offset is the
   offset of an x86 call/jump instruction, or SIZE_MAX for an ordinary reference. */
typedef struct MIR_native_reloc {
  size_t offset, call_offset;
  MIR_item_t item;
} MIR_native_reloc_t;
typedef void (*MIR_native_func_t) (MIR_context_t, MIR_item_t, const uint8_t *, size_t,
                                   const MIR_native_reloc_t *, size_t, void *);
/* Generate normally, additionally delivering a borrowed, address-free image.
   Returns zero without calling output when capture is unsupported or the function
   was already generated. Explicit integer process addresses are not serializable.
   Currently supports x86-64 and AArch64; output must not reenter the generator. */
extern int MIR_gen_native (MIR_context_t ctx, MIR_item_t func, MIR_native_func_t output,
                           void *data);
/* Load a trusted image after MIR_load_module/MIR_link; no MIR_gen_init required.
   Returns NULL for unresolved items or invalid relocation bounds. */
extern void *MIR_load_native (MIR_context_t ctx, MIR_item_t func, const uint8_t *code, size_t len,
                              const MIR_native_reloc_t *relocs, size_t nrelocs);

/* Reconstruct a generator builtin import without initializing the generator. */
extern MIR_item_t MIR_native_builtin (MIR_context_t ctx, MIR_module_t module, const char *name);

extern void MIR_gen_init (MIR_context_t ctx);
extern void MIR_gen_set_debug_file (MIR_context_t ctx, FILE *f);
extern void MIR_gen_set_debug_level (MIR_context_t ctx, int debug_level);
extern void MIR_gen_set_optimize_level (MIR_context_t ctx, unsigned int level);
/* Scalar host dispatch policy, independent of the general optimization levels. */
extern void MIR_gen_set_host_ir (MIR_context_t ctx);
extern void *MIR_gen (MIR_context_t ctx, MIR_item_t func_item);
extern void MIR_set_gen_interface (MIR_context_t ctx, MIR_item_t func_item);
extern void MIR_set_lazy_gen_interface (MIR_context_t ctx, MIR_item_t func_item);
extern void MIR_set_lazy_bb_gen_interface (MIR_context_t ctx, MIR_item_t func_item);
extern void MIR_gen_finish (MIR_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef MIR_GEN_H */
