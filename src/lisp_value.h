#ifndef LISP_VALUE_H
#define LISP_VALUE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core.h"
#include "lexer.h"

// TODO: Implement LISP_WITH_META_T, which can be used to pass metadata along
// with data.
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

// TODO: The variadic argument should be allowed to be anywhere in the argument
// list. Any positional argument can be matched from the left and the right and
// every remaining argument can be passed as the variadic argument.

// TODO: In many cases, the argument count checks are not needed as the function
// is single use. Because of this, a new builtin could be implemented (e.g.
// `fn-simple`) that implements the `fn` functionality without the argument
// count check and parameter matching. For example the syntax `(fn-simple BODY)`
// could be used for this, where `fn_simple` is a builtin and the returned value
// is `fn-s-t`. The functionality is equivalent to that of the `(fn ((rest
// args)) BODY)`. The returned value should be a `simple-fn-t` with its own C
// struct.
//
// Then, for `fn`, there are two options.
// Either the current builtin `fn`, the struct `_LispCustomFunction` and the
// type `fn-t` stay as is with their own implementation independant of the
// simple functions.
// This idea should be better in terms of performance.
//
// Or the builtin, struct and the types are all removed as builtin, but the `fn`
// is defined as a custom function in the core library.
// This idea should be better in terms of simplicity, as there are less builtins
// than the first option and the functionality is implemented only as a custom
// function which is less error prune than C code. In the future, if an
// optimizer is implemented, the optimizer will try to eliminate unnecessary
// code. This option should be more favorable for the optimizer, because of the
// simpler builtin definition.
//
// Third option is that simple functions are not implemented and everything
// stays as is.
//
// I like the second option the most.
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
    // TODO: Strings referenced by lisp objects should be kept in a pool, and
    // they should be deallocated like lisp objects.
    StringView string;
    char *symbol;
    int64_t integer;
    LispType ref_type;
    bool boolean;
    _LispBuiltinFunction *builtin_function;
    _LispCustomFunction *custom_function;
    _LispCons cons;
    LispValue item;
  };
};

// Singleton values.
extern LispValue LISP_NIL;
extern LispValue LISP_TRUE;
extern LispValue LISP_FALSE;

static inline LispType lisp_get_type(LispValue value) { return value->type; }
static inline bool lisp_is_type(LispValue value, LispType type) {
  return lisp_get_type(value) == type;
}

static inline LispValue lisp_make_bool(bool boolean) {
  return boolean ? LISP_TRUE : LISP_FALSE;
}

static inline bool lisp_is_nil(LispValue value) { return value == LISP_NIL; }
static inline bool lisp_is_non_empty_list(LispValue value) {
  return lisp_is_type(value, LISP_LIST_T);
}
static inline bool lisp_is_list(LispValue value) {
  return lisp_is_nil(value) || lisp_is_non_empty_list(value);
}

LispValue _lisp_init(LispType type);

static inline LispValue lisp_make_error(LispValue item) {
  LispValue value = _lisp_init(LISP_ERROR_T);
  value->item = item;
  return value;
}

static inline LispValue lisp_make_cons(LispValue head, LispValue tail) {
  assert(lisp_is_list(tail));

  LispValue value = _lisp_init(LISP_LIST_T);
  value->cons.head = head;
  value->cons.tail = tail;
  return value;
}

static inline LispValue lisp_make_unpacked(LispValue item) {
  assert(lisp_is_list(item));

  LispValue value = _lisp_init(LISP_UNPACKED_T);
  value->item = item;
  return value;
}

static inline LispValue lisp_make_fn_or_macro(LispValue args, LispValue body,
                                              bool variadic, bool is_macro) {
  _LispCustomFunction *fn = malloc(sizeof(_LispCustomFunction));
  assert(fn);

  *fn = (_LispCustomFunction){
      .is_macro = is_macro,
      .args = args,
      .variadic = variadic,
      .body = body,
  };

  LispValue value = _lisp_init(LISP_FUNC_T);
  value->custom_function = fn;
  return value;
}

// TODO: Types should be singletons.
static inline LispValue lisp_make_type(LispType ref_type) {
  LispValue value = _lisp_init(LISP_TYPE_T);
  value->ref_type = ref_type;
  return value;
};

static inline LispValue lisp_make_int64(int64_t integer) {
  LispValue value = _lisp_init(LISP_INT64_T);
  value->integer = integer;
  return value;
}

static inline LispValue lisp_make_str(StringView string) {
  LispValue value = _lisp_init(LISP_STRING_T);
  value->string = string;
  return value;
}

static inline LispValue lisp_make_cstr(const char *string) {
  return lisp_make_str(
      (StringView){.array = (char *)string, .size = strlen(string)});
}

static inline LispValue lisp_make_sym(char *symbol) {
  LispValue value = _lisp_init(LISP_SYMBOL_T);
  value->symbol = symbol;
  return value;
}

LispValue lisp_make_list(size_t count, ...);

bool lisp_is_valid_type(LispType value);
bool lisp_is_ident(LispValue a, LispValue b);

static inline bool lisp_is_sym(LispValue value, const char *symbol) {
  return lisp_is_type(value, LISP_SYMBOL_T) && !strcmp(value->symbol, symbol);
}

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

#define ERROR(s, n, ...)                                                       \
  lisp_make_error(lisp_make_list(n + 1, lisp_make_sym(s), __VA_ARGS__))

#define WRONG_NUMBER_ARGS_SYM "wrong-number-of-args-to-builtin"
#define TOO_FEW_ARGS_SYM "too-few-args-to-builtin"
#define TOO_MANY_ARGS_SYM "too-many-args-to-builtin"

#define WRONG_NUMBER_ARGS_CUSTOM_SYM "wrong-number-of-args"
#define TOO_FEW_ARGS_CUSTOM_SYM "too-few-args"
#define TOO_MANY_ARGS_CUSTOM_SYM "too-many-args"

#define WRONG_TYPE_ARG_SYM "wrong-type-arg-to-builtin"

#define TOO_FEW_ARGS_FORMAT_SYM "too-few-args-to-format"
#define WRONG_TYPE_ARG_FORMAT_SYM "wrong-type-arg-to-format"
#define INVALID_FORMAT_SYM "invalid-format-string"

#define SYMBOL_REDEFINITION_SYM "symbol-redefinition"
#define COULD_NOT_READ_FILE_SYM "could-not-read-file"
#define COULD_NOT_GEN_TOKENS_SYM "could-not-gen-tokens"
#define UNDEF_SYMBOL_SYM "undefined-symbol"
#define REDEF_SYMBOL_SYM "redefined-symbol"

#define NOT_CALLABLE_SYM "not-callable"

#define PARSER_TOO_FEW_R_PAREN "parser:too-few-r-paren"
#define PARSER_TOO_MANY_R_PAREN "parser:too-many-r-paren"
#define PARSER_QUOTE_END "parser:reached-end-quote"

extern char *lexer_result_syms[];

#define RETURN_COULD_NOT_READ_FILE(f)                                          \
  return ERROR(COULD_NOT_READ_FILE_SYM, 1, f)

#define RETURN_COULD_NOT_GEN_TOKENS(r)                                         \
  return lisp_make_error(                                                      \
      lisp_make_cons(lisp_make_sym(lexer_result_syms[r]), LISP_NIL))

#endif
