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
      assert(arg->item->type == LISP_LIST_T || arg->item->type == LISP_NIL_T);
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
    case LISP_TYPE_T:
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

    case LISP_UNPACKED_T:
      // I do not think calling an unpacked type is possible by regular means,
      // but just in case adding this fall through to list.

      lisp = lisp->item;

    case LISP_LIST_T: {
#ifdef _DEBUG_LISP_EVAL
      fprintf(stderr, "eval: ");
      lisp_fprint(stderr, lisp, true);
      fprintf(stderr, "\n");
#endif

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

        Environment *new_env = malloc(sizeof(Environment));
        *new_env = (Environment){.other = env};
        da_init(new_env);

        if (callable->custom_function.args->type == LISP_NIL_T) {
          if (args->type != LISP_NIL_T) {
            roll_env_back(env, original);
            return ERROR_TO_LISP("function or macro expects no arguments, but "
                                 "was called with arguments");
          }

        } else {
          LispValue remaining_args = args;
          LispValue remaining_func_args = callable->custom_function.args;

          while (remaining_func_args->cons.tail->type == LISP_LIST_T) {
            if (remaining_args->type == LISP_NIL_T) {
              roll_env_back(env, original);
              return ERROR_TO_LISP(
                  "insufficient arguments in function or macro call");
            }

            assert(remaining_func_args->type == LISP_LIST_T);
            assert(remaining_func_args->cons.head->type == LISP_SYMBOL_T);

            env_set(new_env, remaining_func_args->cons.head->symbol,
                    remaining_args->cons.head);

            remaining_args = remaining_args->cons.tail;
            remaining_func_args = remaining_func_args->cons.tail;
          }

          LispValue last_func_arg = remaining_func_args->cons.head;
          assert(last_func_arg->type == LISP_SYMBOL_T);

          if (callable->custom_function.variadic) {
            env_set(new_env, last_func_arg->symbol, remaining_args);
          } else {
            if (remaining_args->type == LISP_NIL_T) {
              roll_env_back(env, original);
              return ERROR_TO_LISP(
                  "insufficient arguments in function or macro call");
            }

            if (remaining_args->cons.tail->type != LISP_NIL_T) {
              roll_env_back(env, original);
              return ERROR_TO_LISP(
                  "too many arguments in function or macro call");
            }

            env_set(new_env, last_func_arg->symbol, remaining_args->cons.head);
          }
        }

        lisp =
            evaluate_lisp_body(callable->custom_function.body, new_env, true);
        env = new_env;
      } break;

      default:
        roll_env_back(env, original);
        return ERROR_TO_LISP("tried to call non-callable");
      }
    } break;

    case LISP_TYPE_COUNT:
      fprintf(stderr, "reached unreachable\n");
      assert(false);
      break;
    }
  } while (true);
}

LispValue evaluate_lisp_body(LispValue lisp, Environment *env, bool tail_call) {
  assert(lisp->type == LISP_LIST_T);

  while (lisp->cons.tail->type == LISP_LIST_T) {
    evaluate_lisp(lisp->cons.head, env);
    lisp = lisp->cons.tail;
  }
  assert(lisp->cons.tail->type == LISP_NIL_T);

  if (tail_call)
    return lisp->cons.head;

  return evaluate_lisp(lisp->cons.head, env);
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

  return evaluate_lisp_body(parser_result, env, false);
}
