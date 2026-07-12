#include "debug.h"
#include "format.h"
#include "lisp_value.h"

DebugFlags debug_flags = 0;

void debug_list_env(FILE *file, Environment *env) {
  for (size_t i = 0; i < env->size; i++) {
    lisp_fprintf(file, "%s: %l\n", env->array[i].symbol, env->array[i].value);
  }
}
