#ifndef LISP_VALUE_H
#define LISP_VALUE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core.h"

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

typedef struct Environment Environment;

typedef struct {
  bool is_macro;
  bool tail_call;
  LispValue (*callback)(LispValue, Environment *);
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

typedef struct _LispCons _LispCons;

// Singleton values.
extern LispValue LISP_NIL;
extern LispValue LISP_TRUE;
extern LispValue LISP_FALSE;

LispType lisp_get_type(LispValue value);

bool lisp_is_type(LispValue value, LispType type);
bool lisp_is_nil(LispValue value);
bool lisp_is_non_empty_list(LispValue value);
bool lisp_is_list(LispValue value);
bool lisp_is_sym(LispValue value, const char *symbol);

StringView lisp_get_str(LispValue value);
int64_t lisp_get_int64(LispValue value);
const char *lisp_get_sym(LispValue value);
char *lisp_get_ref_type_sym(LispValue value);
bool lisp_get_bool(LispValue value);
_LispBuiltinFunction *lisp_get_builtin(LispValue value);
_LispCustomFunction *lisp_get_custom(LispValue value);
LispValue lisp_get_error_item(LispValue value);
LispValue lisp_get_unpacked_item(LispValue value);

LispValue _lisp_init(LispType type);

LispValue lisp_make_singleton_nil();
LispValue lisp_make_singleton_bool(bool boolean);
LispValue lisp_make_builtin(bool is_macro, bool tail_call,
                            LispValue (*callback)(LispValue, Environment *),
                            char *name);

LispValue lisp_make_bool(bool boolean);
LispValue lisp_make_error(LispValue item);
LispValue lisp_make_cons(_LispCons *cons);
LispValue lisp_make_unpacked(LispValue item);
LispValue lisp_make_fn_or_macro(LispValue args, LispValue body, bool variadic,
                                bool is_macro);
LispValue lisp_make_type(LispType ref_type);
LispValue lisp_make_int64(int64_t integer);
LispValue lisp_make_str(StringView string);
LispValue lisp_make_cstr(const char *string);
LispValue lisp_make_sym(const char *symbol);

LispValue lisp_make_list_1(LispValue item0);
LispValue lisp_make_list_2(LispValue item0, LispValue item1);
LispValue lisp_make_list_3(LispValue item0, LispValue item1, LispValue item2);
LispValue lisp_make_list_4(LispValue item0, LispValue item1, LispValue item2,
                           LispValue item3);

bool lisp_is_ident(LispValue a, LispValue b);

// Lisp Cons are used for the linked list representation of the lisp lists.
struct _LispCons {
  LispValue head;
  _LispCons *tail;
};

_LispCons *lisp_cons_make(LispValue head, _LispCons *tail);
LispValue lisp_cons(LispValue head, LispValue tail);
LispValue lisp_consify_array(LispValue *array, size_t size);
LispValue lisp_head(LispValue lisp);
LispValue lisp_tail(LispValue lisp);
LispValue lisp_head_n(LispValue lisp, size_t n);
LispValue lisp_tail_n(LispValue lisp, size_t n);
size_t lisp_get_len(LispValue lisp);

// Lisp Generators are used for generating lists without directly accessing list
// cons values. They can append items at the right end of the lists.
typedef struct {
  _LispCons *list;
  _LispCons **last;
} LispGen;

void lisp_gen_init(LispGen *generator);
void _lisp_gen_move(LispGen *generator);
void lisp_gen_appendr(LispGen *generator, LispValue item);
void lisp_gen_extendr(LispGen *generator, LispValue list);
LispValue lisp_gen_end(LispGen *generator);

// Lisp Iterators are used for iterating through lists.
typedef struct {
  _LispCons *cons;
} LispIter;

void lisp_iter_init(LispIter *iter, LispValue list);
bool lisp_iter_has_next(LispIter *iter);
bool lisp_iter_has_next_next(LispIter *iter);
LispValue lisp_iter_next(LispIter *iter);
LispValue lisp_iter_remaining(LispIter *iter);

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
EnvironmentEntry *env_find_shallow(Environment *env, const char *symbol);
EnvironmentEntry *env_find(Environment *env, const char *symbol);
EnvironmentEntry *env_set_shallow(Environment *env, const char *symbol,
                                  LispValue value);
EnvironmentEntry *env_set(Environment *env, const char *symbol,
                          LispValue value);

#define ERROR0(s) lisp_make_error(lisp_make_list_1(lisp_make_sym(s)))
#define ERROR1(s, i0) lisp_make_error(lisp_make_list_2(lisp_make_sym(s), i0))
#define ERROR2(s, i0, i1)                                                      \
  lisp_make_error(lisp_make_list_3(lisp_make_sym(s), i0, i1))
#define ERROR3(s, i0, i1, i2)                                                  \
  lisp_make_error(lisp_make_list_4(lisp_make_sym(s), i0, i1, i2))

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
#define PARSER_REACHED_END "parser:reached-end"
#define PARSER_QUOTE_END "parser:reached-end-quote"

extern char *lexer_result_syms[];

#define RETURN_COULD_NOT_READ_FILE(f) return ERROR1(COULD_NOT_READ_FILE_SYM, f)

#define RETURN_COULD_NOT_GEN_TOKENS(r)                                         \
  return lisp_make_error(lisp_make_list_1(lisp_make_sym(lexer_result_syms[r])))

#endif
