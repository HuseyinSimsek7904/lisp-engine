#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "core.h"
#include "debug.h"
#include "evaluation.h"
#include "format.h"
#include "lisp_value.h"
#include "parser.h"

LispValue generate_args(LispValue lisp, Environment *env, bool is_macro) {
  if (is_macro)
    return lisp_tail(lisp);

  LispGen args_gen;
  lisp_gen_init(&args_gen);

  LispIter iter;
  lisp_iter_init(&iter, lisp);
  lisp_iter_next(&iter);

  while (lisp_iter_has_next(&iter)) {
    lisp_gen_appendr(&args_gen, evaluate_lisp(lisp_iter_next(&iter), env));
  }

  return lisp_gen_end(&args_gen);
}

LispValue evaluate_lisp(LispValue lisp, Environment *env) {
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
      return lisp;

    case LISP_SYMBOL_T: {
      EnvironmentEntry *entry = env_find(env, lisp_get_sym(lisp));

      if (entry) {
        LispValue value = entry->value;
        return value;
      }

      return ERROR1(UNDEF_SYMBOL_SYM, lisp);
    }

    case LISP_LIST_T: {
      if (lisp_is_empty(lisp))
        return LISP_NIL;

      if (debug_flags & DEBUG_PRINT_EVAL) {
        lisp_fprintf(stderr, "eval: %l\n", lisp);
      }

      LispValue callable = evaluate_lisp(lisp_head(lisp), env);

      switch (lisp_get_type(callable)) {
      case LISP_ERROR_T:
        return callable;

      case LISP_BUILTIN_FUNC_T: {
        if (debug_flags & DEBUG_PRINT_EVAL) {
          lisp_fprintf(stderr, "eval: %l\n", lisp);
        }

        _LispBuiltinFunction *builtin = lisp_get_builtin(callable);

        LispValue args = generate_args(lisp, env, builtin->is_macro);

        LispValue return_value = builtin->callback(args, env);

        if (debug_flags & DEBUG_PRINT_EVAL) {
          lisp_fprintf(stderr, ": %l\n", lisp);
          lisp_fprintf(stderr, ". return: %l\n", return_value);
        }

        lisp = return_value;

        if (!builtin->tail_call) {
          return lisp;
        }
      } break;

      case LISP_FUNC_T: {
        if (debug_flags & DEBUG_PRINT_EVAL) {
          lisp_fprintf(stderr, "eval: %l\n", lisp);
        }

        _LispCustomFunction *custom = lisp_get_custom(callable);

        LispValue args = generate_args(lisp, env, custom->is_macro);

        Environment *new_env = env_make(custom->env);

        if (lisp_is_empty(custom->args)) {
          if (!lisp_is_empty(args)) {
            return ERROR1(TOO_MANY_ARGS_CUSTOM_SYM, callable);
          }

        } else {
          LispIter params_iter;
          lisp_iter_init(&params_iter, args);
          LispIter args_iter;
          lisp_iter_init(&args_iter, custom->args);

          while (lisp_iter_has_next_next(&args_iter)) {
            if (!lisp_iter_has_next(&params_iter)) {
              return ERROR1(TOO_FEW_ARGS_CUSTOM_SYM, callable);
            }

            LispValue arg = lisp_iter_next(&args_iter);
            LispValue param = lisp_iter_next(&params_iter);

            assert(lisp_is_type(arg, LISP_SYMBOL_T));

            env_set_shallow(new_env, lisp_get_sym(arg), param);
          }

          LispValue last_func_arg = lisp_iter_next(&args_iter);
          assert(lisp_is_type(last_func_arg, LISP_SYMBOL_T));

          if (custom->variadic) {
            env_set_shallow(new_env, lisp_get_sym(last_func_arg),
                            lisp_iter_remaining(&params_iter));
          } else {
            if (!lisp_iter_has_next(&params_iter)) {
              return ERROR1(TOO_FEW_ARGS_CUSTOM_SYM, callable);
            }

            if (lisp_iter_has_next_next(&params_iter)) {
              return ERROR1(TOO_MANY_ARGS_CUSTOM_SYM, callable);
            }

            env_set_shallow(new_env, lisp_get_sym(last_func_arg),
                            lisp_iter_next(&params_iter));
          }
        }

        if (custom->is_macro) {
          LispValue expand = evaluate_lisp_body(custom->body, new_env, false);

          if (debug_flags & DEBUG_PRINT_EVAL) {
            lisp_fprintf(stderr, ": %l\n", lisp);
            lisp_fprintf(stderr, ". expand: %l\n", expand);
          }

          lisp = expand;

        } else {
          lisp = evaluate_lisp_body(custom->body, new_env, true);
          env = new_env;
        }
      } break;

      default:
        return ERROR1("not-callable", callable);
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
  if (lisp_is_empty(lisp))
    return LISP_NIL;

  LispIter iter;
  lisp_iter_init(&iter, lisp);

  while (lisp_iter_has_next_next(&iter)) {
    LispValue return_value = evaluate_lisp(lisp_iter_next(&iter), env);

    if (lisp_is_type(return_value, LISP_ERROR_T))
      return return_value;
  }

  if (tail_call)
    return lisp_iter_next(&iter);

  return evaluate_lisp(lisp_iter_next(&iter), env);
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
