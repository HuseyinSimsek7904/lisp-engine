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
  if (is_macro)
    return lisp->cons.tail;

  LispValue args = LISP_NIL;
  LispValue *last = &args;

  lisp = lisp->cons.tail;

  for (; lisp->type != LISP_NIL_T; lisp = lisp->cons.tail) {

    LispValue arg = evaluate_lisp(lisp->cons.head, env);

    if (arg->type == LISP_UNPACKED_T) {
      assert(arg->item->type == LISP_CONS_T || arg->item->type == LISP_NIL_T);
      *last = arg->item;
      last = lisp_cons_find_term_nil(last);

    } else {
      lisp_rappend_use_last(last, arg);
      last = &((*last)->cons.tail);
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

    case LISP_LIST_T:
      lisp = lisp_consify(lisp);
      break;

    case LISP_CONS_T: {
      LispValue callable = evaluate_lisp(lisp->cons.head, env);

      switch (callable->type) {
      case LISP_ERROR_T:
        roll_env_back(env, original);
        return callable;

      case LISP_BUILTIN_FUNC_T: {
        LispValue args =
            generate_args(lisp, env, callable->builtin_function.is_macro);

        lisp = callable->builtin_function.callback(args, env);

        if (!callable->builtin_function.tail_call) {
          roll_env_back(env, original);
          return lisp;
        }
      } break;

      case LISP_FUNC_T: {
        LispValue args =
            generate_args(lisp, env, callable->builtin_function.is_macro);

        size_t arg_count = lisp_get_len(args);
        size_t function_arg_count =
            lisp_get_len(callable->custom_function.args);

        if ((callable->custom_function.variadic &&
             arg_count < function_arg_count - 1) ||
            (!callable->custom_function.variadic &&
             arg_count != function_arg_count)) {

          roll_env_back(env, original);
          return ERROR_TO_LISP("incorrect number of parameters");
        }

        Environment *new_env = malloc(sizeof(Environment));
        *new_env = (Environment){.other = env};
        da_init(new_env);

        LispValue remaining_args = args;
        LispValue remaining_func_args = callable->custom_function.args;

        while (remaining_args->type != LISP_NIL_T) {
          assert(remaining_args->type == LISP_CONS_T);
          assert(remaining_func_args->type == LISP_CONS_T);

          env_set(new_env, remaining_func_args->cons.head->symbol,
                  remaining_args->cons.head);

          remaining_args = remaining_args->cons.tail;
          remaining_func_args = remaining_func_args->cons.tail;

          if (callable->custom_function.variadic &&
              remaining_func_args->cons.tail->type != LISP_NIL_T)
            break;
        }

        if (callable->custom_function.variadic) {
          env_set(new_env, remaining_args->cons.head->symbol, remaining_args);
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
    } break;

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
