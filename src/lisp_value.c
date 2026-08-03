#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "format.h"
#include "lexer.h"
#include "lisp_value.h"

struct _LispValue {
  LispType type;
  union {
    // TODO: Strings referenced by lisp objects should be kept in a pool, and
    // they should be deallocated like lisp objects.
    StringView *string;
    const char *symbol;
    int64_t integer;
    LispType ref_type;
    bool boolean;
    _LispBuiltinFunction *builtin_function;
    _LispCustomFunction *custom_function;
    _LispCons *cons;
    LispValue item;
  };
};

char *lisp_type_syms[] = {
    [LISP_NIL_T] = "nil-t",     [LISP_TYPE_T] = "type-t",
    [LISP_ERROR_T] = "error-t", [LISP_STRING_T] = "str-t",
    [LISP_INT64_T] = "int64-t", [LISP_BOOL_T] = "bool-t",
    [LISP_SYMBOL_T] = "sym-t",  [LISP_BUILTIN_FUNC_T] = "builtin-t",
    [LISP_FUNC_T] = "fn-t",     [LISP_LIST_T] = "list-t",
};

char *lexer_result_syms[] = {
    [LEXER_SUCCESS] = "lexer:success",
    [LEXER_UNKNOWN_CHAR] = "lexer:unknown-char",
    [LEXER_EXPECTED_SEPARATOR] = "lexer:expected-separator",
    [LEXER_UNTERMINATED_STRING] = "lexer:unterminated-string",
    [LEXER_INVALID_ESCAPE] = "lexer:invalid-escape",
    [LEXER_INTEGER_TOO_LARGE] = "lexer:integer-literal-too-large",
};

_LispCons *lisp_cons_make(LispValue head, _LispCons *tail);

extern inline LispType lisp_get_type(LispValue value) { return value->type; }
extern inline bool lisp_is_type(LispValue value, LispType type) {
  return lisp_get_type(value) == type;
}

extern inline bool lisp_is_empty(LispValue value) {
  assert(lisp_is_type(value, LISP_LIST_T));
  return value == LISP_EMPTY;
}

extern inline bool lisp_is_sym(LispValue value, const char *symbol) {
  return lisp_is_type(value, LISP_SYMBOL_T) &&
         !strcmp(lisp_get_sym(value), symbol);
}

extern inline StringView lisp_get_str(LispValue value) {
  assert(lisp_is_type(value, LISP_STRING_T));
  return *value->string;
}
extern inline int64_t lisp_get_int64(LispValue value) {
  assert(lisp_is_type(value, LISP_INT64_T));
  return value->integer;
}
extern inline const char *lisp_get_sym(LispValue value) {
  assert(lisp_is_type(value, LISP_SYMBOL_T));
  return value->symbol;
}
extern inline char *lisp_get_ref_type_sym(LispValue value) {
  assert(lisp_is_type(value, LISP_TYPE_T));
  assert(value->ref_type >= 0 && value->ref_type < LISP_TYPE_COUNT);
  return lisp_type_syms[value->ref_type];
}
extern inline bool lisp_get_bool(LispValue value) {
  assert(lisp_is_type(value, LISP_BOOL_T));
  return value->boolean;
}
extern inline _LispBuiltinFunction *lisp_get_builtin(LispValue value) {
  assert(lisp_is_type(value, LISP_BUILTIN_FUNC_T));
  return value->builtin_function;
}
extern inline _LispCustomFunction *lisp_get_custom(LispValue value) {
  assert(lisp_is_type(value, LISP_FUNC_T));
  return value->custom_function;
}
extern inline LispValue lisp_get_error_item(LispValue value) {
  assert(lisp_is_type(value, LISP_ERROR_T));
  return value->item;
}
extern inline _LispCons *lisp_get_cons(LispValue value) {
  assert(lisp_is_type(value, LISP_LIST_T));
  return value->cons;
}

LispValue _lisp_init(LispType type) {
  LispValue ptr = malloc(sizeof(_LispValue));
  assert(ptr);
  ptr->type = type;

  return ptr;
}

LispValue LISP_NIL;
LispValue LISP_TRUE;
LispValue LISP_FALSE;
LispValue LISP_EMPTY;

extern inline void lisp_init_singletons() {
  LISP_NIL = _lisp_init(LISP_NIL_T);

  LISP_TRUE = _lisp_init(LISP_BOOL_T);
  LISP_TRUE->boolean = true;

  LISP_FALSE = _lisp_init(LISP_BOOL_T);
  LISP_FALSE->boolean = false;

  LISP_EMPTY = _lisp_init(LISP_LIST_T);
  LISP_EMPTY->cons = NULL;
  ;
}

extern inline LispValue
lisp_make_builtin(bool is_macro, bool tail_call,
                  LispValue (*callback)(LispValue, Environment *), char *name) {

  _LispBuiltinFunction *fn = malloc(sizeof(_LispBuiltinFunction));
  *fn = (_LispBuiltinFunction){.is_macro = is_macro,
                               .tail_call = tail_call,
                               .callback = callback,
                               .name = name};

  LispValue value = _lisp_init(LISP_BUILTIN_FUNC_T);
  value->builtin_function = fn;
  return value;
}

extern inline LispValue lisp_make_bool(bool boolean) {
  return boolean ? LISP_TRUE : LISP_FALSE;
}

extern inline LispValue lisp_make_error(LispValue item) {
  LispValue value = _lisp_init(LISP_ERROR_T);
  value->item = item;
  return value;
}

extern inline LispValue lisp_make_cons(_LispCons *cons) {
  if (!cons)
    return LISP_EMPTY;

  LispValue value = _lisp_init(LISP_LIST_T);
  value->cons = cons;
  return value;
}

extern inline LispValue lisp_make_fn_or_macro(LispValue args, LispValue body,
                                              bool variadic, bool is_macro,
                                              Environment *env) {
  _LispCustomFunction *fn = malloc(sizeof(_LispCustomFunction));
  assert(fn);

  *fn = (_LispCustomFunction){.is_macro = is_macro,
                              .args = args,
                              .variadic = variadic,
                              .body = body,
                              .env = env};

  LispValue value = _lisp_init(LISP_FUNC_T);
  value->custom_function = fn;
  return value;
}

// TODO: Types should be singletons.
extern inline LispValue lisp_make_type(LispType ref_type) {
  assert(ref_type >= 0 && ref_type < LISP_TYPE_COUNT);
  LispValue value = _lisp_init(LISP_TYPE_T);
  value->ref_type = ref_type;
  return value;
};

extern inline LispValue lisp_make_int64(int64_t integer) {
  LispValue value = _lisp_init(LISP_INT64_T);
  value->integer = integer;
  return value;
}

extern inline LispValue lisp_make_str(StringView string) {
  StringView *view = malloc(sizeof(StringView));
  *view = string;

  LispValue value = _lisp_init(LISP_STRING_T);
  value->string = view;
  return value;
}

extern inline LispValue lisp_make_cstr(const char *string) {
  return lisp_make_str(
      (StringView){.array = (char *)string, .size = strlen(string)});
}

extern inline LispValue lisp_make_sym(const char *symbol) {
  LispValue value = _lisp_init(LISP_SYMBOL_T);
  value->symbol = symbol;
  return value;
}

extern inline LispValue lisp_make_list_1(LispValue item0) {
  _LispCons *cons = lisp_cons_make(item0, NULL);
  return lisp_make_cons(cons);
}
extern inline LispValue lisp_make_list_2(LispValue item0, LispValue item1) {
  _LispCons *cons = lisp_cons_make(item0, lisp_cons_make(item1, NULL));
  return lisp_make_cons(cons);
}
extern inline LispValue lisp_make_list_3(LispValue item0, LispValue item1,
                                         LispValue item2) {
  _LispCons *cons =
      lisp_cons_make(item0, lisp_cons_make(item1, lisp_cons_make(item2, NULL)));
  return lisp_make_cons(cons);
}
extern inline LispValue lisp_make_list_4(LispValue item0, LispValue item1,
                                         LispValue item2, LispValue item3) {
  _LispCons *cons = lisp_cons_make(
      item0, lisp_cons_make(
                 item1, lisp_cons_make(item2, lisp_cons_make(item3, NULL))));
  return lisp_make_cons(cons);
}

bool lisp_is_ident(LispValue a, LispValue b) {
  if (a == b)
    return true;

  if (lisp_get_type(a) != lisp_get_type(b))
    return false;

  switch (lisp_get_type(a)) {
  case LISP_NIL_T:
    return true;

  case LISP_TYPE_T:
    return a->ref_type == b->ref_type;

  case LISP_BOOL_T:
    return lisp_get_bool(a) == lisp_get_bool(b);

  case LISP_ERROR_T:
    return lisp_is_ident(lisp_get_error_item(a), lisp_get_error_item(b));

  case LISP_INT64_T:
    return lisp_get_int64(a) == lisp_get_int64(b);

  case LISP_STRING_T:
    return sv_cmp(lisp_get_str(a), lisp_get_str(b)) == 0;

  case LISP_BUILTIN_FUNC_T:
    // Just check for reference identity.
    return lisp_get_builtin(a) == lisp_get_builtin(b);

  case LISP_LIST_T: {
    LispIter a_iter;
    LispIter b_iter;

    lisp_iter_init(&a_iter, a);
    lisp_iter_init(&b_iter, b);

    while (lisp_iter_has_next(&a_iter) && lisp_iter_has_next(&b_iter)) {
      if (!lisp_is_ident(lisp_iter_next(&a_iter), lisp_iter_next(&b_iter)))
        return false;
    }

    return !lisp_iter_has_next(&a_iter) && !lisp_iter_has_next(&b_iter);
  }

  case LISP_SYMBOL_T:
    return !strcmp(lisp_get_sym(a), lisp_get_sym(b));

  case LISP_FUNC_T: {
    _LispCustomFunction *a_custom = lisp_get_custom(a);
    _LispCustomFunction *b_custom = lisp_get_custom(b);

    return a_custom->is_macro == b_custom->is_macro &&
           a_custom->variadic == b_custom->variadic &&
           lisp_is_ident(a_custom->args, b_custom->args) &&
           lisp_is_ident(a_custom->body, b_custom->body);
  }

  case LISP_TYPE_COUNT:
    lisp_fprintf(stderr, "reached unreachable\n");
    assert(false);
    break;
  }

  // dummy return
  return false;
}

LispValue lisp_consify_array(LispValue *array, size_t size) {
  LispGen gen;
  lisp_gen_init(&gen);

  for (size_t i = size; i-- > 0;) {
    lisp_gen_appendr(&gen, array[i]);
  }

  return lisp_gen_end(&gen);
}

LispValue lisp_cons(LispValue head, LispValue tail) {
  return lisp_make_cons(lisp_cons_make(head, lisp_get_cons(tail)));
}

LispValue lisp_head(LispValue lisp) {
  assert(lisp_is_type(lisp, LISP_LIST_T));
  assert(!lisp_is_empty(lisp));
  return lisp->cons->head;
}

LispValue lisp_tail(LispValue lisp) {
  assert(lisp_is_type(lisp, LISP_LIST_T));
  assert(!lisp_is_empty(lisp));
  return lisp_make_cons(lisp->cons->tail);
}

LispValue lisp_head_n(LispValue lisp, size_t n) {
  LispIter iter;
  lisp_iter_init(&iter, lisp);

  while (n--) {
    lisp_iter_next(&iter);
  }

  return lisp_iter_next(&iter);
}

LispValue lisp_tail_n(LispValue lisp, size_t n) {
  LispIter iter;
  lisp_iter_init(&iter, lisp);

  while (n--) {
    lisp_iter_next(&iter);
  }

  lisp_iter_next(&iter);
  return lisp_iter_remaining(&iter);
}

size_t lisp_get_len(LispValue lisp) {
  size_t count = 0;

  LispIter iter;
  lisp_iter_init(&iter, lisp);

  while (lisp_iter_has_next(&iter)) {
    count++;
    lisp_iter_next(&iter);
  }

  return count;
}

// Lisp Cons
extern inline _LispCons *lisp_cons_make(LispValue head, _LispCons *tail) {
  _LispCons *cons = malloc(sizeof(_LispCons));
  assert(cons);

  *cons = (_LispCons){.head = head, .tail = tail};

  return cons;
}

// Lisp Generators
extern inline void lisp_gen_init(LispGen *generator) {
  generator->list = NULL;
  generator->last = &(generator->list);
}

extern inline void _lisp_gen_move(LispGen *generator) {
  generator->last = &((*generator->last)->tail);
}

extern inline void lisp_gen_appendr(LispGen *generator, LispValue item) {
  *generator->last = lisp_cons_make(item, NULL);
  _lisp_gen_move(generator);
}

extern inline void lisp_gen_extendr(LispGen *generator, LispValue list) {
  assert(lisp_is_type(list, LISP_LIST_T));

  LispIter iter;
  lisp_iter_init(&iter, list);

  while (lisp_iter_has_next(&iter)) {
    lisp_gen_appendr(generator, lisp_iter_next(&iter));
  }
}

extern inline LispValue lisp_gen_end(LispGen *generator) {
  return lisp_make_cons(generator->list);
}

// Lisp Iterators
extern inline void lisp_iter_init(LispIter *iter, LispValue list) {
  iter->cons = lisp_get_cons(list);
}

extern inline bool lisp_iter_has_next(LispIter *iter) { return iter->cons; }

extern inline bool lisp_iter_has_next_next(LispIter *iter) {
  assert(iter->cons);
  return iter->cons->tail;
}

extern inline LispValue lisp_iter_next(LispIter *iter) {
  assert(iter->cons);
  LispValue next = iter->cons->head;
  iter->cons = iter->cons->tail;
  return next;
}

extern inline LispValue lisp_iter_remaining(LispIter *iter) {
  return lisp_make_cons(iter->cons);
}

// Environments
Environment *env_make(Environment *other) {
  Environment *new_env = malloc(sizeof(Environment));
  assert(new_env);
  *new_env = (Environment){.other = other};
  da_init(new_env);

  return new_env;
}

void env_free(Environment *env) {
  for (size_t i = 0; i < env->size; i++) {
    free(env->array[i].symbol);
  }

  da_free(env);
}

EnvironmentEntry *env_find_shallow(Environment *env, const char *symbol) {
  for (size_t i = 0; i < env->size; i++) {
    if (strcmp(env->array[i].symbol, symbol) == 0)
      return &env->array[i];
  }

  return NULL;
}

EnvironmentEntry *env_find(Environment *env, const char *symbol) {
  EnvironmentEntry *search = env_find_shallow(env, symbol);

  if (search)
    return search;

  if (!env->other)
    return NULL;

  return env_find(env->other, symbol);
}

EnvironmentEntry *env_set_shallow(Environment *env, const char *symbol,
                                  LispValue value) {
  EnvironmentEntry *entry = env_find_shallow(env, symbol);

  if (!entry) {
    EnvironmentEntry entry =
        (EnvironmentEntry){.symbol = strdup(symbol), .value = value};
    da_append(env, entry);
    return &env->array[env->size - 1];
  }

  entry->value = value;
  return entry;
}

EnvironmentEntry *env_set(Environment *env, const char *symbol,
                          LispValue value) {
  EnvironmentEntry *entry = env_find(env, symbol);

  if (!entry) {
    EnvironmentEntry entry =
        (EnvironmentEntry){.symbol = strdup(symbol), .value = value};
    da_append(env, entry);
    return &env->array[env->size - 1];
  }

  entry->value = value;
  return entry;
}
