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
    "nil-t", "type-t",    "error-t", "str-t",  "int64-t",    "bool-t",
    "sym-t", "builtin-t", "fn-t",    "list-t", "unpacked-t",
};

char *lexer_result_syms[] = {
    [LEXER_SUCCESS] = "lexer:success",
    [LEXER_UNKNOWN_CHAR] = "lexer:unknown-char",
    [LEXER_EXPECTED_SEPARATOR] = "lexer:expected-separator",
    [LEXER_UNTERMINATED_STRING] = "lexer:unterminated-string",
    [LEXER_INVALID_ESCAPE] = "lexer:invalid-escape",
    [LEXER_INTEGER_TOO_LARGE] = "lexer:integer-literal-too-large"
};

LispValue _lisp_init(_LispValue *value) {
  LispValue ptr = malloc(sizeof(_LispValue));
  assert(ptr);
  *ptr = *value;

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

  if (a->type != b->type)
    return false;

  switch (a->type) {
  case LISP_NIL_T:
    return true;

  case LISP_TYPE_T:
    return a->ref_type == b->ref_type;

  case LISP_BOOL_T:
    return a->boolean == b->boolean;

  case LISP_ERROR_T:
    return lisp_is_ident(a->item, b->item);

  case LISP_INT64_T:
    return a->integer == b->integer;

  case LISP_STRING_T:
    return !strcmp(a->string, b->string);

  case LISP_BUILTIN_FUNC_T:
    return a->builtin_function->is_macro == b->builtin_function->is_macro &&
           a->builtin_function->callback == b->builtin_function->callback;

  case LISP_LIST_T:
    do {
      if (!lisp_is_ident(a->cons.head, b->cons.head))
        return false;

      a = a->cons.tail;
      b = b->cons.tail;
    } while (a->type == LISP_LIST_T && b->type == LISP_LIST_T);

    return a->type == b->type;

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

LispValue lisp_make_error(LispValue item) {
  return _lisp_init(&(_LispValue){.type = LISP_ERROR_T, .item = item});
}

LispValue lisp_make_cons(LispValue head, LispValue tail) {
  assert(tail->type == LISP_LIST_T || tail->type == LISP_NIL_T);

  return _lisp_init(
      &(_LispValue){.type = LISP_LIST_T, .cons = {.head = head, .tail = tail}});
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

LispValue lisp_make_unpacked(LispValue item) {
  return _lisp_init(&(_LispValue){.type = LISP_UNPACKED_T, .item = item});
}

LispValue lisp_make_fn_or_macro(LispValue args, LispValue body, bool variadic,
                                bool is_macro) {
  _LispCustomFunction *fn = malloc(sizeof(_LispCustomFunction));
  assert(fn);

  *fn = (_LispCustomFunction){
      .is_macro = is_macro,
      .args = args,
      .variadic = variadic,
      .body = body,
  };

  return _lisp_init(&(_LispValue){.type = LISP_FUNC_T, .custom_function = fn});
}

LispValue lisp_consify_array(LispValue *array, size_t size) {
  LispValue cons = LISP_NIL;

  for (size_t i = size; i-- > 0;) {
    cons = lisp_make_cons(array[i], cons);
  }

  return cons;
}

LispValue lisp_head(LispValue lisp) {
  if (lisp->type == LISP_LIST_T)
    return lisp->cons.head;

  return NULL;
}

LispValue lisp_tail(LispValue lisp) {
  if (lisp->type == LISP_LIST_T)
    return lisp->cons.tail;

  return NULL;
}

LispValue *lisp_cons_find_term_nil(LispValue *lisp) {
  while ((*lisp)->type != LISP_NIL_T) {
    assert((*lisp)->type == LISP_LIST_T);
    lisp = &((*lisp)->cons.tail);
  }

  return lisp;
}

size_t lisp_get_len(LispValue lisp) {
  size_t count = 0;
  for (; lisp->type == LISP_LIST_T; count++) {
    assert(lisp->type == LISP_LIST_T);
    lisp = lisp->cons.tail;
  }

  assert(lisp->type == LISP_NIL_T);

  return count;
}

LispValue lisp_lappend(LispValue lisp, LispValue value) {
  assert(lisp->type == LISP_LIST_T || lisp->type == LISP_NIL_T);
  return lisp_make_cons(value, lisp);
}

LispValue lisp_rappend(LispValue lisp, LispValue value) {
  assert(lisp->type == LISP_LIST_T || lisp->type == LISP_NIL_T);
  LispValue *last = lisp_cons_find_term_nil(&lisp);
  lisp_rappend_use_last(last, value);
  return lisp;
}

LispValue lisp_rappend_use_last(LispValue *last, LispValue value) {
  assert((*last)->type == LISP_NIL_T);
  *last = lisp_make_cons(value, LISP_NIL);
  return *last;
}

LispValue parse_pp_list(LispValue lisp) {
  assert(lisp->type == LISP_LIST_T || lisp->type == LISP_NIL_T);

  LispValue remaining_lisp = lisp;
  while (remaining_lisp->type == LISP_LIST_T) {
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
        if (iter->type == LISP_NIL_T)
          return lisp_make_error(
              lisp_make_cons(SYM_TO_LISP(PARSER_QUOTE_END), LISP_NIL));

        assert(iter->type == LISP_LIST_T);

        count++;

        item = iter->cons.head;
        iter = iter->cons.tail;
      } while (lisp_is_sym(item, "'"));

      // Nest quote lists.
      LispValue result = item;
      while (count--) {
        result = lisp_make_cons(SYM_TO_LISP("quote"),
                                lisp_make_cons(result, LISP_NIL));
      }

      remaining_lisp->cons.head = result;
      remaining_lisp->cons.tail = iter;
    }

    remaining_lisp = remaining_lisp->cons.tail;
  }
  assert(remaining_lisp->type == LISP_NIL_T);

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
            lisp_make_cons(SYM_TO_LISP(PARSER_TOO_MANY_R_PAREN), LISP_NIL));

      depth--;
      last = &lisp;

      for (size_t i = 0; i < depth; i++) {
        while ((*last)->cons.tail->type == LISP_LIST_T) {
          last = &((*last)->cons.tail);
        }

        assert((*last)->cons.tail->type == LISP_NIL_T);
        last = &((*last)->cons.head);
      }

      assert((*last)->type == LISP_LIST_T);
      while ((*last)->cons.tail->type == LISP_LIST_T) {
        last = &((*last)->cons.tail);
      }
      assert((*last)->cons.tail->type == LISP_NIL_T);

      LispValue pp = parse_pp_list((*last)->cons.head);

      if (pp->type == LISP_ERROR_T)
        return pp;

      (*last)->cons.head = pp;
      last = &((*last)->cons.tail);

      continue;

    case T_INT64:
      pn = INT64_TO_LISP(t.integer);
      break;

    case T_STRING:
      pn = STR_TO_LISP(t.string);
      break;

    case T_SYMBOL:
      pn = SYM_TO_LISP(t.symbol);
      break;

    case T_QUOTE:
      pn = SYM_TO_LISP("\'");
      break;
    }

    lisp_rappend_use_last(last, pn);
    last = &((*last)->cons.tail);
  }

  if (depth > 0)
    return lisp_make_error(
        lisp_make_cons(SYM_TO_LISP(PARSER_TOO_FEW_R_PAREN), LISP_NIL));

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
