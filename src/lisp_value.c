#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "lexer.h"
#include "lisp_value.h"

LispValue _lisp_init(_LispValue *value) {
  LispValue ptr = malloc(sizeof(_LispValue));
  assert(ptr);
  *ptr = *value;
  return ptr;
}

void lisp_fprint(FILE *file, LispValue value, bool pretty) {
  switch (value->type) {
    // TODO: nil and bools are singletons, and because of this, their
    // representation is their symbol itself. Singleton symbols could be
    // printed better.

  case LISP_NIL_T:
    fprintf(file, "nil");
    break;

  case LISP_BOOL_T:
    fprintf(file, value->boolean ? "true" : "false");
    break;

  case LISP_ERROR_T:
    fprintf(file, pretty ? "<error(%s)>" : "error: %s", value->error_message);
    break;

  case LISP_STRING_T:
    fprintf(file, pretty ? "\"%s\"" : "%s", value->string);
    break;

  case LISP_SYMBOL_T:
    fprintf(file, "%s", value->symbol);
    break;

  case LISP_INT64_T:
    fprintf(file, "%ld", value->integer);
    break;

  case LISP_BUILTIN_FUNC_T:
    fprintf(file,
            value->builtin_function.is_macro ? "<builtin-macro `%s`>"
                                             : "<builtin-function `%s`>",
            value->builtin_function.name);
    break;

  case LISP_FUNC_T:
    fprintf(file, value->custom_function.is_macro ? "<macro(" : "<function(");

    LispValue remaining_args = value->custom_function.args;

    if (remaining_args->type != LISP_NIL_T) {
      assert(remaining_args->type == LISP_CONS_T);

      for (; remaining_args->cons.tail->type == LISP_CONS_T;) {
        fprintf(file, "%s ", remaining_args->cons.head->symbol);
        remaining_args = remaining_args->cons.tail;
      }

      if (value->custom_function.variadic) {
        fprintf(file, "(rest %s)", remaining_args->cons.head->symbol);
      } else {
        fprintf(file, "%s", remaining_args->cons.head->symbol);
      }
    }

    fprintf(file, ")>");
    break;

  case LISP_UNPACKED_T:
    assert(value->item->type == LISP_CONS_T);
    fprintf(file, "<unpacked");
    lisp_fprint(file, value->item, true);
    fprintf(file, ">");
    break;

  case LISP_LIST_T:
    fprintf(file, "(");
    if (value->items.size > 0)
      lisp_fprint(file, value->items.array[0], true);

    for (size_t i = 1; i < value->items.size; i++) {
      fprintf(file, " ");
      lisp_fprint(file, value->items.array[i], true);
    }
    fprintf(file, ")");
    break;

  case LISP_CONS_T:
    fprintf(file, "<cons(");
    lisp_fprint(file, value->cons.head, true);

    while (value->cons.tail->type != LISP_NIL_T) {
      value = value->cons.tail;

      fprintf(file, " ");
      lisp_fprint(file, value->cons.head, true);
    }
    fprintf(file, ")>");
    break;

  default:
    TODO();
  }
}

inline void lisp_print(LispValue value, bool pretty) {
  lisp_fprint(stdout, value, pretty);
}

bool lisp_is_ident(LispValue a, LispValue b) {
  if (a->type != b->type)
    return false;

  switch (a->type) {
  case LISP_NIL_T:
    return true;

  case LISP_BOOL_T:
    return a->boolean == b->boolean;

  case LISP_ERROR_T:
    return a->error_message == b->error_message;

  case LISP_INT64_T:
    return a->integer == b->integer;

  case LISP_STRING_T:
    return strcmp(a->string, b->string);

  case LISP_BUILTIN_FUNC_T:
    return a->builtin_function.is_macro == b->builtin_function.is_macro &&
           a->builtin_function.callback == b->builtin_function.callback;

  case LISP_LIST_T:
    if (a->items.size != b->items.size)
      return false;

    for (size_t i = 0; i < a->items.size; i++)
      if (!lisp_is_ident(a->items.array[i], b->items.array[i]))
        return false;

    return true;

  case LISP_CONS_T:
    do {
      if (lisp_is_ident(a->cons.head, b->cons.head))
        return false;

      a = a->cons.tail;
      b = b->cons.tail;
    } while (a->type == LISP_CONS_T && b->type == LISP_CONS_T);

    return a->type == b->type;

  default:
    TODO();
    return false;
  }
}

LispValue lisp_make_cons(LispValue head, LispValue tail) {
  return _lisp_init(
      &(_LispValue){.type = LISP_CONS_T, .cons = {.head = head, .tail = tail}});
}

LispValue lisp_listify(LispValue lisp) {
  if (lisp->type == LISP_LIST_T)
    return lisp;

  assert(lisp->type == LISP_CONS_T);

  LispValues items;
  da_init(&items);

  while (lisp->type != LISP_NIL_T) {
    assert(lisp->type == LISP_CONS_T);
    da_append(&items, lisp->cons.head);
    lisp = lisp->cons.tail;
  }

  return _lisp_init(
      &(_LispValue){.type = LISP_LIST_T,
                    .items = {.array = items.array, .size = items.size}});
}

LispValue lisp_consify(LispValue lisp) {
  if (lisp->type == LISP_CONS_T)
    return lisp;

  assert(lisp->type == LISP_LIST_T);
  return lisp_consify_array(lisp->items.array, lisp->items.size);
}

LispValue lisp_consify_array(LispValue *array, size_t size) {
  LispValue cons = LISP_NIL;

  for (size_t i = size; i-- > 0;) {
    cons = lisp_make_cons(array[i], cons);
  }

  return cons;
}

LispValue lisp_head(LispValue lisp) {
  if (lisp->type == LISP_CONS_T)
    return lisp->cons.head;

  else if (lisp->type == LISP_LIST_T) {
    if (!lisp->items.size)
      return NULL;

    return lisp->items.array[0];

  } else
    return NULL;
}

LispValue lisp_tail(LispValue lisp) {
  if (lisp->type == LISP_CONS_T)
    return lisp->cons.tail;

  else if (lisp->type == LISP_LIST_T) {
    if (!lisp->items.size)
      return NULL;

    LispValue cons = lisp_consify(lisp);
    return cons->cons.tail;

  } else
    return NULL;
}

LispValue *lisp_cons_find_term_nil(LispValue *lisp) {
  while ((*lisp)->type != LISP_NIL_T) {
    assert((*lisp)->type == LISP_CONS_T);
    lisp = &((*lisp)->cons.tail);
  }

  return lisp;
}

size_t lisp_get_len(LispValue lisp) {
  if (lisp->type == LISP_LIST_T)
    return lisp->items.size;

  assert(lisp->type == LISP_CONS_T);

  size_t count = 0;
  for (; lisp->type != LISP_NIL_T; count++) {
    assert(lisp->type == LISP_CONS_T);
    lisp = lisp->cons.tail;
  }

  return count;
}

LispValue lisp_lappend(LispValue lisp, LispValue value) {
  assert(lisp->type == LISP_CONS_T || lisp->type == LISP_NIL_T);
  return lisp_make_cons(value, lisp);
}

LispValue lisp_rappend(LispValue lisp, LispValue value) {
  assert(lisp->type == LISP_CONS_T || lisp->type == LISP_NIL_T);
  LispValue *last = lisp_cons_find_term_nil(&lisp);
  lisp_rappend_use_last(last, value);
  return lisp;
}

LispValue lisp_rappend_use_last(LispValue *last, LispValue value) {
  assert((*last)->type == LISP_NIL_T);
  *last = lisp_make_cons(value, LISP_NIL);
  return *last;
}

LispValue parse(Token *tokens, size_t token_count) {
  LispValue lisp = LISP_NIL;
  LispValue *last = &lisp;

  size_t depth = 0;

  for (; token_count > 0; token_count--) {
    Token t = *tokens++;

    _LispValue pn;
    switch (t.type) {
    case T_L_PAREN:
      lisp_rappend_use_last(last, LISP_NIL);
      last = &((*last)->cons.head);

      depth++;
      continue;

    case T_R_PAREN:
      if (depth <= 0) {
        return ERROR_TO_LISP("too many closing parenthesis");
      }

      depth--;
      last = &lisp;

      for (size_t i = 0; i < depth; i++) {
        while ((*last)->cons.tail->type == LISP_CONS_T) {
          last = &((*last)->cons.tail);
        }

        assert((*last)->cons.tail->type == LISP_NIL_T);
        last = &((*last)->cons.head);
      }

      last = lisp_cons_find_term_nil(last);
      continue;

    case T_INT64:
      pn = (_LispValue){.type = LISP_INT64_T, .integer = t.integer};
      break;

    case T_STRING:
      pn = (_LispValue){.type = LISP_STRING_T, .string = t.string};
      break;

    case T_SYMBOL:
      pn = (_LispValue){.type = LISP_SYMBOL_T, .symbol = t.symbol};
      break;

    default:
      TODO();
      return ERROR_TO_LISP("unknown token type");
    }

    lisp_rappend_use_last(last, _lisp_init(&pn));
    last = &((*last)->cons.tail);
  }

  if (depth > 0)
    return ERROR_TO_LISP("too many opening parenthesis");

  return lisp;
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

EnvironmentEntry *env_set(Environment *env, char *symbol, LispValue value) {
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
