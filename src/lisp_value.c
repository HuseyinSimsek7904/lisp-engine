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

LispValue _lisp_init(LispType type) {
  LispValue ptr = malloc(sizeof(_LispValue));
  assert(ptr);
  ptr->type = type;

  if (debug_flags & DEBUG_PRINT_INIT) {
    lisp_fprintf(stderr, "init: %l\n", ptr);
  }

  return ptr;
}

bool lisp_is_valid_type(LispType type) {
  return type >= 0 && type < LISP_TYPE_COUNT;
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
    return a->boolean == b->boolean;

  case LISP_ERROR_T:
    return lisp_is_ident(a->item, b->item);

  case LISP_INT64_T:
    return lisp_get_int64(a) == lisp_get_int64(b);

  case LISP_STRING_T:
    return sv_cmp(lisp_get_str(a), lisp_get_str(b)) == 0;

  case LISP_BUILTIN_FUNC_T:
    return a->builtin_function->is_macro == b->builtin_function->is_macro &&
           a->builtin_function->callback == b->builtin_function->callback;

  case LISP_LIST_T:
    do {
      if (!lisp_is_ident(a->cons.head, b->cons.head))
        return false;

      a = a->cons.tail;
      b = b->cons.tail;
    } while (lisp_is_non_empty_list(a) && lisp_is_non_empty_list(b));

    return lisp_get_type(a) == lisp_get_type(b);

  case LISP_SYMBOL_T:
    return !strcmp(a->symbol, b->symbol);

  case LISP_FUNC_T:
    return a->custom_function->is_macro == b->custom_function->is_macro &&
           a->custom_function->variadic == b->custom_function->variadic &&
           lisp_is_ident(a->custom_function->args, b->custom_function->args) &&
           lisp_is_ident(a->custom_function->body, b->custom_function->body);

  case LISP_UNPACKED_T:
    return lisp_is_ident(a->item, b->item);

  case LISP_TYPE_COUNT:
    lisp_fprintf(stderr, "reached unreachable\n");
    assert(false);
    break;
  }

  // dummy return
  return false;
}

LispValue lisp_make_list(size_t count, ...) {
  LispValue list = LISP_NIL;
  LispValue *last = &list;

  va_list va;
  va_start(va, count);
  for (int j = 0; j < count; ++j) {
    LispValue arg = va_arg(va, LispValue);

    lisp_rappend_use_last(last, arg);
    last = &((*last)->cons.tail);
  }
  va_end(va);

  return list;
}

LispValue lisp_consify_array(LispValue *array, size_t size) {
  LispValue cons = LISP_NIL;

  for (size_t i = size; i-- > 0;) {
    cons = lisp_make_cons(array[i], cons);
  }

  return cons;
}

LispValue lisp_head(LispValue lisp) {
  if (lisp_is_non_empty_list(lisp))
    return lisp->cons.head;

  return NULL;
}

LispValue lisp_tail(LispValue lisp) {
  if (lisp_is_non_empty_list(lisp))
    return lisp->cons.tail;

  return NULL;
}

LispValue *lisp_cons_find_term_nil(LispValue *lisp) {
  while (!lisp_is_nil(*lisp)) {
    assert(lisp_is_non_empty_list(*lisp));
    lisp = &((*lisp)->cons.tail);
  }

  return lisp;
}

size_t lisp_get_len(LispValue lisp) {
  size_t count = 0;
  for (; lisp_is_non_empty_list(lisp); count++) {
    lisp = lisp->cons.tail;
  }

  assert(lisp_is_nil(lisp));

  return count;
}

LispValue lisp_lappend(LispValue lisp, LispValue value) {
  assert(lisp_is_list(lisp));
  return lisp_make_cons(value, lisp);
}

LispValue lisp_rappend(LispValue lisp, LispValue value) {
  assert(lisp_is_list(lisp));
  LispValue *last = lisp_cons_find_term_nil(&lisp);
  lisp_rappend_use_last(last, value);
  return lisp;
}

LispValue lisp_rappend_use_last(LispValue *last, LispValue value) {
  assert(lisp_is_nil((*last)));
  *last = lisp_make_cons(value, LISP_NIL);
  return *last;
}

LispValue parse_pp_list(LispValue lisp) {
  assert(lisp_is_list(lisp));

  LispValue remaining_lisp = lisp;
  while (lisp_is_non_empty_list(remaining_lisp)) {
    LispValue item = remaining_lisp->cons.head;

    if (lisp_is_sym(item, "'")) {
      // Quote symbols are processed right-to-left.
      // So ''a => (quote (quote a)) and not ''a => (quote quote) a.
      // Since it is really hard to go back with Lisp lists, we just count the
      // number of quotes and nest that many of them.
      LispValue iter = remaining_lisp->cons.tail;
      size_t count = 0;

      // Count the number of quote symbols.
      do {
        if (lisp_is_nil(iter))
          return lisp_make_error(
              lisp_make_cons(lisp_make_sym(PARSER_QUOTE_END), LISP_NIL));

        assert(lisp_is_non_empty_list(iter));

        count++;

        item = iter->cons.head;
        iter = iter->cons.tail;
      } while (lisp_is_sym(item, "'"));

      // Nest quote lists.
      LispValue result = item;
      while (count--) {
        result = lisp_make_cons(lisp_make_sym("quote"),
                                lisp_make_cons(result, LISP_NIL));
      }

      remaining_lisp->cons.head = result;
      remaining_lisp->cons.tail = iter;
    }

    remaining_lisp = remaining_lisp->cons.tail;
  }
  assert(lisp_is_nil(remaining_lisp));

  return lisp;
}

LispValue parse(Token *tokens, size_t token_count) {
  LispValue lisp = LISP_NIL;
  LispValue *last = &lisp;

  size_t depth = 0;

  for (; token_count > 0; token_count--) {
    Token t = *tokens++;

    LispValue pn = LISP_NIL;
    switch (t.type) {
    case T_L_PAREN:
      lisp_rappend_use_last(last, LISP_NIL);
      last = &((*last)->cons.head);

      depth++;
      continue;

    case T_R_PAREN:
      if (depth <= 0)
        return lisp_make_error(
            lisp_make_cons(lisp_make_sym(PARSER_TOO_MANY_R_PAREN), LISP_NIL));

      depth--;
      last = &lisp;

      for (size_t i = 0; i < depth; i++) {
        while (lisp_is_non_empty_list((*last)->cons.tail)) {
          last = &((*last)->cons.tail);
        }

        assert(lisp_is_nil((*last)->cons.tail));
        last = &((*last)->cons.head);
      }

      assert(lisp_is_non_empty_list(*last));
      while (lisp_is_non_empty_list((*last)->cons.tail)) {
        last = &((*last)->cons.tail);
      }
      assert(lisp_is_nil((*last)->cons.tail));

      LispValue pp = parse_pp_list((*last)->cons.head);

      if (lisp_is_type(pp, LISP_ERROR_T))
        return pp;

      (*last)->cons.head = pp;
      last = &((*last)->cons.tail);

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

    lisp_rappend_use_last(last, pn);
    last = &((*last)->cons.tail);
  }

  if (depth > 0)
    return lisp_make_error(
        lisp_make_cons(lisp_make_sym(PARSER_TOO_FEW_R_PAREN), LISP_NIL));

  return parse_pp_list(lisp);
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
