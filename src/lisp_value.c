#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "debug.h"
#include "format.h"
#include "lexer.h"
#include "lisp_value.h"

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

char *lisp_type_syms[] = {
    [LISP_NIL_T] = "nil-t",           [LISP_TYPE_T] = "type-t",
    [LISP_ERROR_T] = "error-t",       [LISP_STRING_T] = "str-t",
    [LISP_INT64_T] = "int64-t",       [LISP_BOOL_T] = "bool-t",
    [LISP_SYMBOL_T] = "sym-t",        [LISP_BUILTIN_FUNC_T] = "builtin-t",
    [LISP_FUNC_T] = "fn-t",           [LISP_LIST_T] = "list-t",
    [LISP_UNPACKED_T] = "unpacked-t",
};

char *lexer_result_syms[] = {
    [LEXER_SUCCESS] = "lexer:success",
    [LEXER_UNKNOWN_CHAR] = "lexer:unknown-char",
    [LEXER_EXPECTED_SEPARATOR] = "lexer:expected-separator",
    [LEXER_UNTERMINATED_STRING] = "lexer:unterminated-string",
    [LEXER_INVALID_ESCAPE] = "lexer:invalid-escape",
    [LEXER_INTEGER_TOO_LARGE] = "lexer:integer-literal-too-large",
};

extern inline LispType lisp_get_type(LispValue value) { return value->type; }
extern inline bool lisp_is_type(LispValue value, LispType type) {
  return lisp_get_type(value) == type;
}

extern inline bool lisp_is_nil(LispValue value) { return value == LISP_NIL; }
extern inline bool lisp_is_non_empty_list(LispValue value) {
  return lisp_is_type(value, LISP_LIST_T);
}
extern inline bool lisp_is_list(LispValue value) {
  return lisp_is_nil(value) || lisp_is_non_empty_list(value);
}

extern inline bool lisp_is_sym(LispValue value, const char *symbol) {
  return lisp_is_type(value, LISP_SYMBOL_T) &&
         !strcmp(lisp_get_sym(value), symbol);
}

extern inline StringView lisp_get_str(LispValue value) {
  assert(lisp_is_type(value, LISP_STRING_T));
  return value->string;
}
extern inline int64_t lisp_get_int64(LispValue value) {
  assert(lisp_is_type(value, LISP_INT64_T));
  return value->integer;
}
extern inline char *lisp_get_sym(LispValue value) {
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
extern inline LispValue lisp_get_unpacked_item(LispValue value) {
  assert(lisp_is_type(value, LISP_UNPACKED_T));
  assert(lisp_is_list(value->item));
  return value->item;
}

LispValue _lisp_init(LispType type) {
  LispValue ptr = malloc(sizeof(_LispValue));
  assert(ptr);
  ptr->type = type;

  if (debug_flags & DEBUG_PRINT_INIT) {
    lisp_fprintf(stderr, "init: %l\n", ptr);
  }

  return ptr;
}

extern inline LispValue lisp_make_singleton_nil() {
  return _lisp_init(LISP_NIL_T);
}

extern inline LispValue lisp_make_singleton_bool(bool boolean) {
  LispValue value = _lisp_init(LISP_BOOL_T);
  value->boolean = boolean;
  return value;
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

extern inline LispValue lisp_make_cons(LispValue head, LispValue tail) {
  assert(lisp_is_list(tail));

  LispValue value = _lisp_init(LISP_LIST_T);
  value->cons.head = head;
  value->cons.tail = tail;
  return value;
}

extern inline LispValue lisp_make_unpacked(LispValue item) {
  assert(lisp_is_list(item));

  LispValue value = _lisp_init(LISP_UNPACKED_T);
  value->item = item;
  return value;
}

extern inline LispValue lisp_make_fn_or_macro(LispValue args, LispValue body,
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
  LispValue value = _lisp_init(LISP_STRING_T);
  value->string = string;
  return value;
}

extern inline LispValue lisp_make_cstr(const char *string) {
  return lisp_make_str(
      (StringView){.array = (char *)string, .size = strlen(string)});
}

extern inline LispValue lisp_make_sym(char *symbol) {
  LispValue value = _lisp_init(LISP_SYMBOL_T);
  value->symbol = symbol;
  return value;
}

extern inline LispValue lisp_make_list_1(LispValue item0) {
  return lisp_make_cons(item0, LISP_NIL);
}
extern inline LispValue lisp_make_list_2(LispValue item0, LispValue item1) {
  return lisp_make_cons(item0, lisp_make_list_1(item1));
}
extern inline LispValue lisp_make_list_3(LispValue item0, LispValue item1,
                                         LispValue item2) {
  return lisp_make_cons(item0, lisp_make_list_2(item1, item2));
}
extern inline LispValue lisp_make_list_4(LispValue item0, LispValue item1,
                                         LispValue item2, LispValue item3) {
  return lisp_make_cons(item0, lisp_make_list_3(item1, item2, item3));
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

  case LISP_LIST_T:
    do {
      if (!lisp_is_ident(lisp_head(a), lisp_head(b)))
        return false;

      a = lisp_tail(a);
      b = lisp_tail(b);
    } while (lisp_is_non_empty_list(a) && lisp_is_non_empty_list(b));

    return lisp_get_type(a) == lisp_get_type(b);

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

  case LISP_UNPACKED_T:
    return lisp_is_ident(lisp_get_unpacked_item(a), lisp_get_unpacked_item(b));

  case LISP_TYPE_COUNT:
    lisp_fprintf(stderr, "reached unreachable\n");
    assert(false);
    break;
  }

  // dummy return
  return false;
}

LispValue lisp_consify_array(LispValue *array, size_t size) {
  LispValue cons = LISP_NIL;

  for (size_t i = size; i-- > 0;) {
    cons = lisp_make_cons(array[i], cons);
  }

  return cons;
}

LispValue lisp_head(LispValue lisp) {
  assert(lisp_is_non_empty_list(lisp));
  return lisp->cons.head;
}

LispValue lisp_tail(LispValue lisp) {
  assert(lisp_is_non_empty_list(lisp));
  return lisp->cons.tail;
}

LispValue lisp_head_n(LispValue lisp, size_t n) {
  while (n--) {
    lisp = lisp_tail(lisp);
  }
  return lisp_head(lisp);
}

LispValue lisp_tail_n(LispValue lisp, size_t n) {
  while (n--) {
    lisp = lisp_tail(lisp);
  }
  return lisp_tail(lisp);
}

size_t lisp_get_len(LispValue lisp) {
  size_t count = 0;
  for (; lisp_is_non_empty_list(lisp); count++) {
    lisp = lisp_tail(lisp);
  }

  assert(lisp_is_nil(lisp));

  return count;
}

// Lisp Generators
extern inline void lisp_gen_init(LispGen *generator) {
  generator->list = LISP_NIL;
  generator->last = &generator->list;
}

extern inline void _lisp_gen_move(LispGen *generator) {
  assert(lisp_is_non_empty_list(*generator->last));
  generator->last = &((*generator->last)->cons.tail);
}

extern inline void lisp_gen_appendr(LispGen *generator, LispValue item) {
  assert(lisp_is_nil(*generator->last));
  *generator->last = lisp_make_cons(item, LISP_NIL);
  _lisp_gen_move(generator);
}

extern inline void lisp_gen_extendr(LispGen *generator, LispValue list) {
  assert(lisp_is_list(list));
  *generator->last = list;

  // Find the nil element and move generator to point to this element.
  while (!lisp_is_nil(*generator->last)) {
    assert(lisp_is_non_empty_list(*generator->last));
    _lisp_gen_move(generator);
  }
}

extern inline LispValue lisp_gen_end(LispGen *generator) {
  return generator->list;
}

// Lisp Iterators
extern inline void lisp_iter_init(LispIter *iter, LispValue list) {
  assert(lisp_is_list(list));
  iter->list = list;
}

extern inline bool lisp_iter_has_next(LispIter *iter) {
  assert(lisp_is_list(iter->list));
  return lisp_is_non_empty_list(iter->list);
}

extern inline bool lisp_iter_has_next_next(LispIter *iter) {
  assert(lisp_is_non_empty_list(iter->list));
  return lisp_is_non_empty_list(lisp_tail(iter->list));
}

extern inline LispValue lisp_iter_next(LispIter *iter) {
  assert(lisp_is_non_empty_list(iter->list));
  LispValue next = lisp_head(iter->list);
  iter->list = lisp_tail(iter->list);
  assert(lisp_is_list(iter->list));
  return next;
}

LispValue parse_pp_list(LispValue lisp) {
  assert(lisp_is_list(lisp));

  LispValue remaining_lisp = lisp;
  while (lisp_is_non_empty_list(remaining_lisp)) {
    LispValue item = lisp_head(remaining_lisp);

    if (lisp_is_sym(item, "'")) {
      // Quote symbols are processed right-to-left.
      // So ''a => (quote (quote a)) and not ''a => (quote quote) a.
      // Since it is really hard to go back with Lisp lists, we just count the
      // number of quotes and nest that many of them.
      LispValue iter = lisp_tail(remaining_lisp);
      size_t count = 0;

      // Count the number of quote symbols.
      do {
        if (lisp_is_nil(iter))
          return lisp_make_error(
              lisp_make_list_1(lisp_make_sym(PARSER_QUOTE_END)));

        assert(lisp_is_non_empty_list(iter));

        count++;

        item = lisp_head(iter);
        iter = lisp_tail(iter);
      } while (lisp_is_sym(item, "'"));

      // Nest quote lists.
      LispValue result = item;
      while (count--) {
        result = lisp_make_list_2(lisp_make_sym("quote"), result);
      }

      remaining_lisp->cons.head = result;
      remaining_lisp->cons.tail = iter;
    }

    remaining_lisp = lisp_tail(remaining_lisp);
  }
  assert(lisp_is_nil(remaining_lisp));

  return lisp;
}

// TODO: The use of generators in this function is criminal. This function
// should be made human readable.
LispValue parse(Token *tokens, size_t token_count) {
  LispGen gen;
  lisp_gen_init(&gen);

  size_t depth = 0;

  for (; token_count > 0; token_count--) {
    Token t = *tokens++;

    LispValue pn = LISP_NIL;
    switch (t.type) {
    case T_L_PAREN:
      *gen.last = lisp_make_cons(LISP_NIL, LISP_NIL);
      gen.last = &((*gen.last)->cons.head);

      depth++;
      continue;

    case T_R_PAREN:
      if (depth <= 0)
        return lisp_make_error(
            lisp_make_list_1(lisp_make_sym(PARSER_TOO_MANY_R_PAREN)));

      depth--;
      gen.last = &gen.list;

      for (size_t i = 0; i < depth; i++) {
        while (lisp_is_non_empty_list(lisp_tail(*gen.last))) {
          _lisp_gen_move(&gen);
        }

        assert(lisp_is_nil(lisp_tail(*gen.last)));
        gen.last = &((*gen.last)->cons.head);
      }

      assert(lisp_is_non_empty_list(*gen.last));
      while (lisp_is_non_empty_list(lisp_tail(*gen.last))) {
        _lisp_gen_move(&gen);
      }
      assert(lisp_is_nil(lisp_tail(*gen.last)));

      LispValue pp = parse_pp_list(lisp_head(*gen.last));

      if (lisp_is_type(pp, LISP_ERROR_T))
        return pp;

      (*gen.last)->cons.head = pp;
      _lisp_gen_move(&gen);

      continue;

    case T_INT64:
      pn = lisp_make_int64(t.integer);
      break;

    case T_STRING:
      pn = lisp_make_str(t.string);
      break;

    case T_SYMBOL:
      pn = lisp_make_sym(t.symbol);
      break;

    case T_QUOTE:
      pn = lisp_make_sym("\'");
      break;
    }

    assert(lisp_is_nil((*gen.last)));
    *gen.last = lisp_make_cons(pn, LISP_NIL);
    _lisp_gen_move(&gen);
  }

  if (depth > 0)
    return lisp_make_error(
        lisp_make_list_1(lisp_make_sym(PARSER_TOO_FEW_R_PAREN)));

  return parse_pp_list(lisp_gen_end(&gen));
}

void env_free(Environment *env) {
  for (size_t i = 0; i < env->size; i++) {
    free(env->array[i].symbol);
  }

  da_free(env);
}

EnvironmentEntry *env_find_shallow(Environment *env, char *symbol) {
  for (size_t i = 0; i < env->size; i++) {
    if (strcmp(env->array[i].symbol, symbol) == 0)
      return &env->array[i];
  }

  return NULL;
}

EnvironmentEntry *env_find(Environment *env, char *symbol) {
  EnvironmentEntry *search = env_find_shallow(env, symbol);

  if (search)
    return search;

  if (!env->other)
    return NULL;

  return env_find(env->other, symbol);
}

EnvironmentEntry *env_set_shallow(Environment *env, char *symbol,
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

EnvironmentEntry *env_set(Environment *env, char *symbol, LispValue value) {
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
