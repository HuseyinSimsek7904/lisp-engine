#ifndef LISP_VALUE_H
#define LISP_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lexer.h"

typedef enum {
  LISP_NIL_T = 0,
  LISP_ERROR_T,
  LISP_STRING_T,
  LISP_INT64_T,
  LISP_BOOL_T,
  LISP_SYMBOL_T,
  LISP_BUILTIN_FUNC_T,
  LISP_FUNC_T,
  LISP_LIST_T,
  LISP_UNPACKED_T,
} LispType;

typedef struct _LispValue _LispValue;
typedef _LispValue *LispValue;

typedef struct {
  bool is_macro;
  bool tail_call;
  LispValue (*callback)(LispValue *, size_t, void *);
  char *name;
} _LispBuiltinFunction;

typedef struct {
  bool is_macro;
  char **args;
  size_t arg_count;
  LispValue *body;
  size_t body_size;
  char *variadic;
} _LispCustomFunction;

typedef struct {
  LispValue *array;
  size_t size;
} _LispView;

struct _LispValue {
  LispType type;
  union {
    char *string;
    char *error_message;
    char *symbol;
    int64_t integer;
    bool boolean;
    _LispBuiltinFunction builtin_function;
    _LispCustomFunction custom_function;
    _LispView items;
  };
};

typedef struct {
  LispValue *array;
  size_t size;
  size_t capacity;
} LispValues;

typedef enum {
  PARSER_SUCCESS = 0,
  PARSER_TOO_MANY_CLOSING,
  PARSER_TOO_MANY_OPENING,
  PARSER_UNKNOWN_TOKEN,
} ParseResult;

typedef struct {
  LispValues *array;
  size_t size;
  size_t capacity;
} LispValuesArray;

// Singleton values.
extern LispValue LISP_NIL;
extern LispValue LISP_TRUE;
extern LispValue LISP_FALSE;

#define BOOL_TO_LISP(x) x ? LISP_TRUE : LISP_FALSE

#define INT64_TO_LISP(x)                                                       \
  _lisp_init(&(_LispValue){.type = LISP_INT64_T, .integer = x})

#define STR_TO_LISP(x)                                                         \
  _lisp_init(&(_LispValue){.type = LISP_STRING_T, .string = x})

#define ERROR_TO_LISP(x)                                                       \
  _lisp_init(&(_LispValue){.type = LISP_ERROR_T, .error_message = x})

LispValue _lisp_init(_LispValue *value);

bool lisp_is_ident(LispValue a, LispValue b);

void lisp_fprint(FILE *file, LispValue value, bool pretty);
void lisp_print(LispValue value, bool pretty);

ParseResult parse(Token *tokens, size_t token_count, LispValues *nodes);

typedef struct {
  char *symbol;
  LispValue value;
} EnvironmentEntry;

typedef struct Environment {
  EnvironmentEntry *array;
  size_t size;
  size_t capacity;
  struct Environment *other;
} Environment;

void env_free(Environment *env);
EnvironmentEntry *env_find_shallow(Environment *env, char *symbol);
EnvironmentEntry *env_find(Environment *env, char *symbol);
EnvironmentEntry *env_set(Environment *env, char *symbol, LispValue value);

#endif
