#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "core.h"
#include "debug.h"
#include "evaluation.h"
#include "format.h"
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

  LispGen args_gen;
  lisp_gen_init(&args_gen);

  lisp = lisp_tail(lisp);

  for (; !lisp_is_nil(lisp); lisp = lisp->cons.tail) {
    LispValue arg = evaluate_lisp(lisp->cons.head, env);

    if (lisp_is_type(arg, LISP_UNPACKED_T)) {
      assert(lisp_is_list(arg->item));
      lisp_gen_extendr(&args_gen, arg->item);

    } else {
      lisp_gen_appendr(&args_gen, arg);
    }
  }

  return lisp_gen_end(&args_gen);
}

LispValue evaluate_lisp(LispValue lisp, Environment *env) {
  Environment *original = env;

  do {
    switch (lisp_get_type(lisp)) {
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
      EnvironmentEntry *entry = env_find(env, lisp_get_sym(lisp));

      if (entry) {
        LispValue value = entry->value;
        roll_env_back(env, original);
        return value;
      }

      roll_env_back(env, original);
      return ERROR(UNDEF_SYMBOL_SYM, 1, lisp);
    }

    case LISP_UNPACKED_T:
      // I do not think calling an unpacked type is possible by regular means,
      // but just in case adding this fall through to list.

      lisp = lisp->item;

    case LISP_LIST_T: {
      if (debug_flags & DEBUG_PRINT_EVAL) {
        lisp_fprintf(stderr, "eval: %l\n", lisp);
      }

      LispValue callable = evaluate_lisp(lisp->cons.head, env);

      switch (lisp_get_type(callable)) {
      case LISP_ERROR_T:
        roll_env_back(env, original);
        return callable;

      case LISP_BUILTIN_FUNC_T: {
        _LispBuiltinFunction *builtin = lisp_get_builtin(callable);

        LispValue args = generate_args(lisp, env, builtin->is_macro);

        lisp = builtin->callback(args, env);

        if (!builtin->tail_call) {
          roll_env_back(env, original);
          return lisp;
        }
      } break;

      case LISP_FUNC_T: {
        LispValue args =
            generate_args(lisp, env, callable->custom_function->is_macro);

        Environment *new_env = malloc(sizeof(Environment));
        assert(new_env);

        *new_env = (Environment){.other = env};
        da_init(new_env);

        if (lisp_is_nil(callable->custom_function->args)) {
          if (!lisp_is_nil(args)) {
            roll_env_back(env, original);
            return ERROR(TOO_MANY_ARGS_CUSTOM_SYM, 1, callable);
          }

        } else {
          LispIter params_iter;
          lisp_iter_init(&params_iter, args);
          LispIter args_iter;
          lisp_iter_init(&args_iter, callable->custom_function->args);

          while (lisp_iter_has_next_next(&args_iter)) {
            if (!lisp_iter_has_next(&params_iter)) {
              roll_env_back(env, original);
              return ERROR(TOO_FEW_ARGS_CUSTOM_SYM, 1, callable);
            }

            LispValue arg = lisp_iter_next(&args_iter);
            LispValue param = lisp_iter_next(&params_iter);

            assert(lisp_is_type(arg, LISP_SYMBOL_T));

            env_set_shallow(new_env, lisp_get_sym(arg), param);
          }

          LispValue last_func_arg = lisp_iter_next(&args_iter);
          assert(lisp_is_type(last_func_arg, LISP_SYMBOL_T));

          if (callable->custom_function->variadic) {
            env_set_shallow(new_env, lisp_get_sym(last_func_arg),
                            params_iter.list);
          } else {
            if (!lisp_iter_has_next(&params_iter)) {
              roll_env_back(env, original);
              return ERROR(TOO_FEW_ARGS_CUSTOM_SYM, 1, callable);
            }

            if (lisp_iter_has_next_next(&params_iter)) {
              roll_env_back(env, original);
              return ERROR(TOO_MANY_ARGS_CUSTOM_SYM, 1, callable);
            }

            env_set_shallow(new_env, lisp_get_sym(last_func_arg),
                            lisp_iter_next(&params_iter));
          }
        }

        lisp =
            evaluate_lisp_body(callable->custom_function->body, new_env, true);
        env = new_env;
      } break;

      default:
        roll_env_back(env, original);
        return ERROR("not-callable", 1, callable);
      }
    } break;

    case LISP_TYPE_COUNT:
      lisp_fprintf(stderr, "reached unreachable\n");
      assert(false);
      break;
    }
  } while (true);
}

LispValue evaluate_lisp_body(LispValue lisp, Environment *env, bool tail_call) {
  if (lisp_is_nil(lisp))
    return LISP_NIL;

  assert(lisp_is_non_empty_list(lisp));

  while (lisp_is_non_empty_list(lisp->cons.tail)) {
    LispValue return_value = evaluate_lisp(lisp->cons.head, env);
    lisp = lisp->cons.tail;

    if (lisp_is_type(return_value, LISP_ERROR_T))
      return return_value;
  }
  assert(lisp_is_nil(lisp->cons.tail));

  if (tail_call)
    return lisp->cons.head;

  return evaluate_lisp(lisp->cons.head, env);
}

LispValue repl(StringView lisp, Environment *env) {
  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(lisp, &tokens);

  if (lexer_result)
    RETURN_COULD_NOT_GEN_TOKENS(lexer_result);

  LispValue parser_result = parse(tokens.array, tokens.size);
  da_free(&tokens);

  if (lisp_is_type(parser_result, LISP_ERROR_T))
    return parser_result;

  return evaluate_lisp_body(parser_result, env, false);
}
