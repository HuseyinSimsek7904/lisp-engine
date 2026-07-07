#ifndef LISP_VALUE_H
#define LISP_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lexer.h"

typedef enum {
  LISP_NIL_T = 0,
  LISP_TYPE_T,
  LISP_ERROR_T,
  LISP_STRING_T,
  LISP_INT64_T,
  LISP_BOOL_T,
  LISP_SYMBOL_T,
  LISP_BUILTIN_FUNC_T,
  LISP_FUNC_T,
  LISP_LIST_T,
  LISP_UNPACKED_T,
  LISP_TYPE_COUNT,
} LispType;

extern char *lisp_type_syms[];

typedef struct _LispValue _LispValue;
typedef _LispValue *LispValue;

typedef struct {
  bool is_macro;
  bool tail_call;
  LispValue (*callback)(LispValue, void *);
  char *name;
} _LispBuiltinFunction;

typedef struct {
  bool is_macro;
  LispValue args;
  bool variadic;
  LispValue body;
} _LispCustomFunction;

typedef struct {
  LispValue head;

  // tail must be either another LISP_LIST_T or LISP_NIL_T.
  LispValue tail;
} _LispCons;

struct _LispValue {
  LispType type;
  union {
    char *string;
    char *symbol;
    int64_t integer;
    LispType ref_type;
    bool boolean;
    _LispBuiltinFunction builtin_function;
    _LispCustomFunction custom_function;
    _LispCons cons;
    LispValue item;
  };
};

typedef struct {
  LispValue *array;
  size_t size;
  size_t capacity;
} LispValues;

// Singleton values.
extern LispValue LISP_NIL;
extern LispValue LISP_TRUE;
extern LispValue LISP_FALSE;

#define BOOL_TO_LISP(x) x ? LISP_TRUE : LISP_FALSE

#define TYPE_TO_LISP(x)                                                        \
  _lisp_init(&(_LispValue){.type = LISP_TYPE_T, .ref_type = x})

#define INT64_TO_LISP(x)                                                       \
  _lisp_init(&(_LispValue){.type = LISP_INT64_T, .integer = x})

#define STR_TO_LISP(x)                                                         \
  _lisp_init(&(_LispValue){.type = LISP_STRING_T, .string = x})

#define SYM_TO_LISP(x)                                                         \
  _lisp_init(&(_LispValue){.type = LISP_SYMBOL_T, .symbol = x})

bool lisp_is_valid_type(LispType value);

LispValue _lisp_init(_LispValue *value);

bool lisp_is_ident(LispValue a, LispValue b);

void lisp_fprint(FILE *file, LispValue value, bool pretty);
void lisp_print(LispValue value, bool pretty);

LispValue lisp_make_error(LispValue item);
LispValue lisp_make_cons(LispValue head, LispValue tail);
LispValue lisp_make_list(size_t count, ...);

LispValue lisp_consify_array(LispValue *array, size_t size);
LispValue lisp_head(LispValue lisp);
LispValue lisp_tail(LispValue lisp);
LispValue *lisp_cons_find_term_nil(LispValue *lisp);
size_t lisp_get_len(LispValue lisp);
LispValue lisp_lappend(LispValue lisp, LispValue value);
LispValue lisp_rappend(LispValue lisp, LispValue value);
LispValue lisp_rappend_use_last(LispValue *last, LispValue value);

LispValue parse(Token *tokens, size_t token_count);

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
EnvironmentEntry *env_set_shallow(Environment *env, char *symbol,
                                  LispValue value);
EnvironmentEntry *env_set(Environment *env, char *symbol, LispValue value);

void env_debug_list(FILE *file, Environment *env);

#define ERROR(s, n, ...)                                                       \
  lisp_make_error(lisp_make_list(n + 1, SYM_TO_LISP(s), __VA_ARGS__))

#define WRONG_NUMBER_ARGS_SYM "wrong-number-of-args-to-builtin"
#define TOO_FEW_ARGS_SYM "too-few-args-to-builtin"
#define TOO_MANY_ARGS_SYM "too-many-args-to-builtin"

#define WRONG_NUMBER_ARGS_CUSTOM_SYM "wrong-number-of-args"
#define TOO_FEW_ARGS_CUSTOM_SYM "too-few-args"
#define TOO_MANY_ARGS_CUSTOM_SYM "too-many-args"

#define WRONG_TYPE_ARG_SYM "wrong-type-arg-to-builtin"

#define SYMBOL_REDEFINITION_SYM "symbol-redefinition"
#define COULD_NOT_READ_FILE_SYM "could-not-read-file"
#define COULD_NOT_GEN_TOKENS_SYM "could-not-gen-tokens"
#define UNDEF_SYMBOL_SYM "undefined-symbol"
#define REDEF_SYMBOL_SYM "redefined-symbol"

#define NOT_CALLABLE_SYM "not-callable"

#define PARSER_TOO_FEW_R_PAREN "parser:too-few-r-paren"
#define PARSER_TOO_MANY_R_PAREN "parser:too-many-r-paren"

extern char *lexer_result_syms[];

#define RETURN_COULD_NOT_READ_FILE(f)                                          \
  return ERROR(COULD_NOT_READ_FILE_SYM, 1, f)

#define RETURN_COULD_NOT_GEN_TOKENS(r)                                         \
  return lisp_make_error(                                                      \
      lisp_make_cons(SYM_TO_LISP(lexer_result_syms[r]), LISP_NIL))

#endif
