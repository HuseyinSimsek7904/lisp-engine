#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>
#include <stdio.h>

#include "lisp_value.h"

typedef enum {
  DEBUG_PRINT_INIT = 1,
  DEBUG_PRINT_EVAL = 2,
} DebugFlags;

extern DebugFlags debug_flags;

void debug_list_env(FILE *file, Environment *env);

#endif
