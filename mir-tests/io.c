#include "../mir.h"

#include "scan-sieve.h"

static int write_byte (MIR_context_t ctx, uint8_t byte) {
  return fputc (byte, MIR_get_user_data (ctx));
}

static int read_byte (MIR_context_t ctx) { return fgetc (MIR_get_user_data (ctx)); }

int main (void) {
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
