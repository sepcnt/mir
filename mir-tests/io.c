#undef NDEBUG
#include "../mir.h"
#include "../mir-gen.h"
#include "../real-time.h"
#include <string.h>
#include <stdlib.h>

#include "scan-sieve.h"

static int write_byte (MIR_context_t ctx, uint8_t byte) {
  return fputc (byte, MIR_get_user_data (ctx));
}

static int read_byte (MIR_context_t ctx) { return fgetc (MIR_get_user_data (ctx)); }

#if defined(__x86_64__) || defined(_M_AMD64)
static int64_t plus_one (int64_t n) { return n + 1; }
static int64_t plus_two (int64_t n) { return n + 2; }

static MIR_item_t named_item (MIR_context_t ctx, const char *name) {
  MIR_module_t m = DLIST_HEAD (MIR_module_t, *MIR_get_module_list (ctx));
  for (MIR_item_t i = DLIST_HEAD (MIR_item_t, m->items); i != NULL; i = DLIST_NEXT (MIR_item_t, i))
    if (MIR_item_name (ctx, i) != NULL && strcmp (MIR_item_name (ctx, i), name) == 0) return i;
  return MIR_native_builtin (ctx, m, name);
}

/* The test format stores symbol names, never MIR_item_t or process addresses. */
static void save_native (MIR_context_t ctx, MIR_item_t func MIR_UNUSED, const uint8_t *code,
                         size_t len, const MIR_native_reloc_t *rs, size_t nr, void *data) {
  FILE *f = data;
  mir_assert (fwrite (&len, sizeof (len), 1, f) == 1);
  mir_assert (fwrite (&nr, sizeof (nr), 1, f) == 1);
  mir_assert (fwrite (code, 1, len, f) == len);
  for (size_t i = 0; i < nr; i++) {
    const char *name = rs[i].item == NULL ? "" : MIR_item_name (ctx, rs[i].item);
    mir_assert (fwrite (&rs[i].offset, sizeof (size_t), 1, f) == 1);
    mir_assert (fwrite (&rs[i].call_offset, sizeof (size_t), 1, f) == 1);
    mir_assert (fwrite (name, 1, strlen (name) + 1, f) == strlen (name) + 1);
  }
}

static void native_roundtrip (FILE *f, int writing) {
  MIR_context_t ctx = MIR_init ();
  FILE *module_file = tmpfile ();
  mir_assert (module_file != NULL);
  MIR_set_user_data (ctx, module_file);
  if (writing) MIR_scan_string (ctx,
                   "native: module\n import ext\n p: proto i64, i64:n\n"
                   "slot: bss 8\n text: string \"A\"\n"
                   "init: func i64\n ret 5\n endfunc\n expr_slot: expr init\n"
                   "floating: func d\n ret 1.5\n endfunc\n"
                   "helper: func i64, i64:n\n local d:d\n ui2d d,n\n d2i n,d\n"
                   "add n,n,3\n ret n\n endfunc\n"
                   "entry: func i64, i64:n\n local i64:r,i64:b,i64:s,i64:a,d:d\n"
                   "bstart s\n add r,n,16\n alloca b,r\n mov i64:(b),n\n mov r,i64:(b)\n bend s\n"
                   "mov b,helper\n call p,b,r,r\n call p,ext,r,r\n mov a,slot\n mov i64:(a),r\n"
                   "mov b,text\n mov r,u8:(b)\n add r,r,i64:(a)\n dmov d,1.5\n d2i b,d\n add "
                   "r,r,b\n switch n,L0,L1,L2\n"
                   "L0: add r,r,10\n ret r\n L1: add r,r,20\n ret r\n L2: ret r\n"
                   "endfunc\n endmodule\n");
  if (!writing) {
    size_t size;
    mir_assert (fread (&size, sizeof (size), 1, f) == 1);
    for (size_t i = 0; i < size; i++) fputc (fgetc (f), module_file);
    rewind (module_file);
    MIR_read_with_func (ctx, read_byte);
  }
  MIR_load_module (ctx, DLIST_HEAD (MIR_module_t, *MIR_get_module_list (ctx)));
  MIR_load_external (ctx, "ext", writing ? plus_one : plus_two);
  MIR_link (ctx, NULL, NULL);
  if (writing) {
    MIR_write_module_data_with_func (ctx, write_byte, NULL);
    size_t size = (size_t) ftell (module_file);
    mir_assert (fwrite (&size, sizeof (size), 1, f) == 1);
    rewind (module_file);
    for (size_t i = 0; i < size; i++) fputc (fgetc (module_file), f);
  }
  fclose (module_file);
  mir_assert (*(int64_t *) named_item (ctx, "expr_slot")->addr == 5);
  double start = real_usec_time ();
  if (writing) {
    MIR_gen_init (ctx);
    MIR_gen_set_host_ir (ctx);
  }
  const char *names[] = {"entry", "helper", "floating"};
  for (size_t k = 0; k < 3; k++) {
    MIR_item_t func = named_item (ctx, names[k]);
    if (writing) {
      mir_assert (MIR_gen_native (ctx, func, save_native, f));
      mir_assert (!MIR_gen_native (ctx, func, save_native, f));
    } else {
      size_t len, nr;
      mir_assert (fread (&len, sizeof (len), 1, f) == 1);
      mir_assert (fread (&nr, sizeof (nr), 1, f) == 1);
      uint8_t *code = malloc (len);
      MIR_native_reloc_t *rs = malloc ((nr + 1) * sizeof (*rs));
      mir_assert (fread (code, 1, len, f) == len);
      for (size_t i = 0; i < nr; i++) {
        char name[128];
        size_t n = 0;
        mir_assert (fread (&rs[i].offset, sizeof (size_t), 1, f) == 1);
        mir_assert (fread (&rs[i].call_offset, sizeof (size_t), 1, f) == 1);
        do {
          mir_assert (n < sizeof (name));
          name[n++] = (char) fgetc (f);
        } while (name[n - 1] != 0);
        rs[i].item = name[0] == 0 ? NULL : named_item (ctx, name);
        mir_assert (name[0] == 0 || rs[i].item != NULL);
      }
      mir_assert (MIR_load_native (ctx, func, code, len, rs, nr) != NULL);
      rs[0].offset = len;
      mir_assert (MIR_load_native (ctx, func, code, len, rs, 1) == NULL);
      free (rs);
      free (code);
    }
  }
  fprintf (stderr, "native %s: %.0f us\n", writing ? "generate" : "load",
           real_usec_time () - start);
  int64_t (*entry) (int64_t) = named_item (ctx, "entry")->addr;
  for (int64_t n = 0; n < 3; n++)
    mir_assert (entry (n) == n + 3 + (writing ? 1 : 2) + 'A' + 1 + (n == 0 ? 10 : n == 1 ? 20 : 0));
  double (*floating) (void) = named_item (ctx, "floating")->addr;
  mir_assert (floating () == 1.5);
  if (writing) MIR_gen_finish (ctx);
  MIR_finish (ctx);
}
#endif

int main (int argc, char **argv) {
#if defined(__x86_64__) || defined(_M_AMD64)
  if (argc == 3) {
    int writing = strcmp (argv[1], "--native-write") == 0;
    FILE *native = fopen (argv[2], writing ? "wb" : "rb");
    mir_assert (native != NULL);
    native_roundtrip (native, writing);
    fclose (native);
    return 0;
  }
  FILE *native = tmpfile ();
  mir_assert (native != NULL);
  native_roundtrip (native, TRUE);
  rewind (native);
  native_roundtrip (native, FALSE);
  fclose (native);
#else
  (void) argc;
  (void) argv;
#endif

  FILE *f;
#ifndef _WIN32
  const char *fname = "/tmp/__tmp.mirb";
#else
  const char *fname = "__tmp.mirb";
#endif
  MIR_context_t ctx = MIR_init ();
  mir_assert (MIR_get_user_data (ctx) == NULL);

  create_mir_func_sieve (ctx, NULL, NULL);
  MIR_output (ctx, stderr);
  f = fopen (fname, "wb");
  mir_assert (f != NULL);
  MIR_set_user_data (ctx, f);
  MIR_write_with_func (ctx, write_byte);
  fclose (f);
  f = fopen (fname, "rb");
  mir_assert (f != NULL);
  MIR_set_user_data (ctx, f);
  MIR_read_with_func (ctx, read_byte);
  fclose (f);
  MIR_set_user_data (ctx, NULL);
  fprintf (stderr, "+++++++++++++After reading:\n");
  MIR_output (ctx, stderr);
  remove (fname);
  MIR_finish (ctx);
  return 0;
}
