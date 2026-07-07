#include "debug.h"
#include "lisp_value.h"

DebugFlags debug_flags = 0;

void debug_list_env(FILE *file, Environment *env) {
  for (size_t i = 0; i < env->size; i++) {
    fprintf(file, "%s: ", env->array[i].symbol);
    lisp_fprint(file, env->array[i].value, true);
    fprintf(file, "\n");
  }
}
