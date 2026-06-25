#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "core.h"
#include "evaluation.h"
#include "lisp_value.h"

void roll_env_back(Environment *env, Environment *original) {
  while (env != original && env->other) {
    env_free(env);
    Environment *other = env->other;
    free(env);
    env = other;
  }
}

LispValue generate_args(LispValue lisp, Environment *env, bool is_macro) {
  LispValue args = LISP_NIL;
  LispValue *last = &args;

  if (is_macro) {
    for (size_t i = 1; i < lisp->items.size; i++) {
      lisp_rappend_use_last(last, lisp->items.array[i]);
      last = &((*last)->cons.tail);
    }

  } else {
    for (size_t i = 1; i < lisp->items.size; i++) {
      LispValue arg = evaluate_lisp(lisp->items.array[i], env);

      if (arg->type == LISP_UNPACKED_T) {
        for (size_t i = 0; i < arg->items.size; i++) {
          lisp_rappend_use_last(last, arg->items.array[i]);
          last = &((*last)->cons.tail);
        }

      } else {
        lisp_rappend_use_last(last, arg);
        last = &((*last)->cons.tail);
      }
    }
  }

  return args;
}

LispValue evaluate_lisp(LispValue lisp, Environment *env) {
  Environment *original = env;

  do {
    switch (lisp->type) {
    case LISP_STRING_T:
    case LISP_INT64_T:
    case LISP_NIL_T:
    case LISP_BOOL_T:
    case LISP_BUILTIN_FUNC_T:
    case LISP_FUNC_T:
    case LISP_ERROR_T:
      roll_env_back(env, original);
      return lisp;

    case LISP_SYMBOL_T: {
      EnvironmentEntry *entry = env_find(env, lisp->symbol);

      if (entry) {
        LispValue value = entry->value;
        roll_env_back(env, original);
        return value;
      }

      roll_env_back(env, original);
      return ERROR_TO_LISP("undefined symbol");
    }

    case LISP_CONS_T:
      lisp = lisp_listify(lisp);
      break;

    case LISP_LIST_T:
      if (!lisp->items.size) {
        roll_env_back(env, original);
        return LISP_NIL;
      }

      LispValue callable = evaluate_lisp(lisp->items.array[0], env);

      switch (callable->type) {
      case LISP_ERROR_T:
        roll_env_back(env, original);
        return callable;

      case LISP_BUILTIN_FUNC_T: {
        LispValue args =
            generate_args(lisp, env, callable->builtin_function.is_macro);
        LispValue args_list = lisp_listify(args);

        lisp = callable->builtin_function.callback(args_list->items.array,
                                                   args_list->items.size, env);

        if (!callable->builtin_function.tail_call) {
          roll_env_back(env, original);
          return lisp;
        }
      } break;

      case LISP_FUNC_T: {
        LispValue args =
            generate_args(lisp, env, callable->builtin_function.is_macro);
        size_t arg_count = lisp_get_len(args);

        if ((callable->custom_function.variadic &&
             arg_count < callable->custom_function.arg_count) ||
            (!callable->custom_function.variadic &&
             arg_count != callable->custom_function.arg_count)) {

          roll_env_back(env, original);
          return ERROR_TO_LISP("incorrect number of parameters");
        }

        Environment *new_env = malloc(sizeof(Environment));
        *new_env = (Environment){.other = env};
        da_init(new_env);

        LispValue remaining_args = args;
        for (size_t i = 0; i < callable->custom_function.arg_count; i++) {
          assert(remaining_args->type == LISP_CONS_T);

          env_set(new_env, callable->custom_function.args[i],
                  remaining_args->cons.head);
          remaining_args = remaining_args->cons.tail;
        }

        if (callable->custom_function.variadic) {
          env_set(new_env, callable->custom_function.variadic, remaining_args);
        }

        assert(callable->custom_function.body->type == LISP_CONS_T);
        LispValue remaining_body = callable->custom_function.body;

        while (remaining_body->cons.tail->type != LISP_NIL_T) {
          evaluate_lisp(remaining_body->cons.head, new_env);
          remaining_body = remaining_body->cons.tail;
        }

        lisp = remaining_body->cons.head;
        env = new_env;
      } break;

      default:
        roll_env_back(env, original);
        return ERROR_TO_LISP("tried to call non-callable");
      }
      break;

    default:
      TODO();
      roll_env_back(env, original);
      return ERROR_TO_LISP("unknown node while trying to evaluate");
    }
  } while (true);
}

LispValue repl(char *lisp, Environment *env) {
  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(lisp, &tokens);

  if (lexer_result)
    return ERROR_TO_LISP("could not generate tokens for lisp");

  LispValue parser_result = parse(tokens.array, tokens.size);
  da_free(&tokens);

  if (parser_result->type == LISP_ERROR_T)
    return parser_result;

  LispValue remaining_lisp = parser_result;
  LispValue return_value = LISP_NIL;
  while (remaining_lisp->type != LISP_NIL_T) {
    return_value = evaluate_lisp(remaining_lisp->cons.head, env);
    remaining_lisp = remaining_lisp->cons.tail;
  }

  return return_value;
}
