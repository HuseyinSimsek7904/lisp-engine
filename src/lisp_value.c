#include <assert.h>
#include <stdbool.h>
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

void lisp_print(LispValue value) {
  switch (value->type) {
  case LISP_NIL:
    printf("NIL");
    break;

  case LISP_BOOL:
    printf(value->boolean ? "true" : "false");
    break;

  case LISP_ERROR:
    printf("<error(%s)>", value->error_message);
    break;

  case LISP_STRING:
    printf("\"%s\"", value->string);
    break;

  case LISP_SYMBOL:
    printf("%s", value->symbol);
    break;

  case LISP_INT64:
    printf("%ld", value->integer);
    break;

  case LISP_BUILTIN_FUNC:
    printf(value->builtin_function.is_macro ? "<builtin-macro>"
                                            : "<builtin-function>");
    break;

  case LISP_FUNC:
    printf(value->custom_function.is_macro ? "<macro(" : "<function(");

    if (value->custom_function.arg_count > 0)
      printf("%s", value->custom_function.args[0]);

    for (size_t i = 1; i < value->custom_function.arg_count; i++) {
      printf(" %s", value->custom_function.args[i]);
    }

    if (value->custom_function.variadic) {
      if (value->custom_function.arg_count > 0)
        printf(" ");
      printf("(rest %s)", value->custom_function.variadic);
    }

    printf(")>");
    break;

  case LISP_UNPACKED:
    printf("unpacked");

  case LISP_TUPLE:
    printf("(");
    if (value->items.size > 0)
      lisp_print(value->items.array[0]);

    for (size_t i = 1; i < value->items.size; i++) {
      printf(" ");
      lisp_print(value->items.array[i]);
    }
    printf(")");
    break;

  default:
    printf("%d\n", value->type);
    TODO();
  }
}

bool lisp_is_ident(LispValue a, LispValue b) {
  if (a->type != b->type)
    return false;

  switch (a->type) {
  case LISP_NIL:
    return true;

  case LISP_BOOL:
    return a->boolean == b->boolean;

  case LISP_ERROR:
    return a->error_message == b->error_message;

  case LISP_INT64:
    return a->integer == b->integer;

  case LISP_STRING:
    return strcmp(a->string, b->string);

  case LISP_BUILTIN_FUNC:
    return a->builtin_function.is_macro == b->builtin_function.is_macro &&
           a->builtin_function.callback == b->builtin_function.callback;

  case LISP_TUPLE:
    if (a->items.size != b->items.size)
      return false;

    for (size_t i = 0; i < a->items.size; i++)
      if (!lisp_is_ident(a->items.array[i], b->items.array[i]))
        return false;

    return true;

  default:
    TODO();
    return false;
  }
}

ParseResult parse(Token *tokens, size_t token_count, LispValues *nodes) {
  LispValuesArray stack;
  da_init(&stack);

  for (; token_count > 0; token_count--) {
    Token t = *tokens++;

    _LispValue pn;
    switch (t.type) {
    case T_L_PAREN:
      da_append(&stack, (LispValues){});
      da_init(&stack.array[stack.size - 1]);
      continue;

    case T_R_PAREN:
      if (!stack.size) {
        da_free(&stack);
        return PARSER_TOO_MANY_CLOSING;
      }

      LispValues *new_nodes = &stack.array[stack.size - 1];
      pn = (_LispValue){
          .type = LISP_TUPLE,
          .items = {.array = new_nodes->array, .size = new_nodes->size}};
      stack.size--;
      break;

    case T_INT64:
      pn = (_LispValue){.type = LISP_INT64, .integer = t.integer};
      break;

    case T_STRING:
      pn = (_LispValue){.type = LISP_STRING, .string = t.string};
      break;

    case T_SYMBOL:
      pn = (_LispValue){.type = LISP_SYMBOL, .symbol = t.symbol};
      break;

    default:
      TODO();
      da_free(&stack);
      return PARSER_UNKNOWN_TOKEN;
    }

    LispValue node = _lisp_init(&pn);

    if (stack.size) {
      da_append(&stack.array[stack.size - 1], node);
    } else {
      da_append(nodes, node);
    }
  }

  da_free(&stack);

  if (stack.size)
    return PARSER_TOO_MANY_OPENING;

  return PARSER_SUCCESS;
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
