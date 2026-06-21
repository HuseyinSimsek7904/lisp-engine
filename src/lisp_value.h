#ifndef LISP_VALUE_H
#define LISP_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lexer.h"

typedef enum {
  LISP_NIL = 0,
  LISP_ERROR,
  LISP_STRING,
  LISP_INT64,
  LISP_BOOL,
  LISP_SYMBOL,
  LISP_BUILTIN_FUNC,
  LISP_FUNC,
  LISP_TUPLE,
  LISP_UNPACKED,
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

#define BOOL_TO_LISP(x)                                                        \
  _lisp_init(&(_LispValue){.type = LISP_BOOL, .boolean = x})
#define INT64_TO_LISP(x)                                                       \
  _lisp_init(&(_LispValue){.type = LISP_INT64, .integer = x})
#define ERROR_TO_LISP(x)                                                       \
  _lisp_init(&(_LispValue){.type = LISP_ERROR, .error_message = x})
#define STR_TO_LISP(x)                                                         \
  _lisp_init(&(_LispValue){.type = LISP_STRING, .string = x})
#define NIL_SINGLETON_LISP _lisp_init(&(_LispValue){.type = LISP_NIL})

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
