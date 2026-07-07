#ifndef DEBUG_H
#define DEBUG_H

typedef enum {
  DEBUG_PRINT_INIT = 1,
  DEBUG_PRINT_EVAL = 2,
} DebugFlags;

extern DebugFlags debug_flags;

#endif
