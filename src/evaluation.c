#include <assert.h>
#include <stdlib.h>

#include "core.h"
#include "evaluation.h"

void roll_env_back(Environment *env, Environment *original) {
  while (env != original && env->other) {
    env_free(env);
    Environment *other = env->other;
    free(env);
    env = other;
  }
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
        roll_env_back(env, original);
        return entry->value;
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
        if (callable->builtin_function.is_macro) {
          lisp = callable->builtin_function.callback(lisp->items.array + 1,
                                                     lisp->items.size - 1, env);
        } else {
          LispValues args;
          da_init(&args);

          for (size_t i = 1; i < lisp->items.size; i++) {
            LispValue arg = evaluate_lisp(lisp->items.array[i], env);
            if (arg->type == LISP_UNPACKED_T)
              da_append_many(&args, arg->items.array, arg->items.size);
            else
              da_append(&args, arg);
          }

          lisp =
              callable->builtin_function.callback(args.array, args.size, env);
        }

        if (!callable->builtin_function.tail_call) {
          roll_env_back(env, original);
          return lisp;
        }
      } break;

      case LISP_FUNC_T: {
        LispValues args;
        da_init(&args);

        if (callable->custom_function.is_macro) {
          for (size_t i = 1; i < lisp->items.size; i++)
            da_append(&args, lisp->items.array[i]);
        } else {
          for (size_t i = 1; i < lisp->items.size; i++) {
            LispValue arg = evaluate_lisp(lisp->items.array[i], env);
            if (arg->type == LISP_UNPACKED_T)
              da_append_many(&args, arg->items.array, arg->items.size);
            else
              da_append(&args, arg);
          }
        }

        if ((callable->custom_function.variadic &&
             args.size < callable->custom_function.arg_count) ||
            (!callable->custom_function.variadic &&
             args.size != callable->custom_function.arg_count)) {

          roll_env_back(env, original);
          return ERROR_TO_LISP("incorrect number of parameters");
        }

        Environment *new_env = malloc(sizeof(Environment));
        *new_env = (Environment){.other = env};
        da_init(new_env);

        for (size_t i = 0; i < callable->custom_function.arg_count; i++) {
          env_set(new_env, callable->custom_function.args[i], args.array[i]);
        }

        if (callable->custom_function.variadic) {
          _LispValue list = {
              .type = LISP_LIST_T,
              .items = {
                  .array =
                      calloc(args.size - callable->custom_function.arg_count,
                             sizeof(LispValue)),
                  .size = args.size - callable->custom_function.arg_count}};

          memcpy(list.items.array,
                 &args.array[callable->custom_function.arg_count],
                 sizeof(LispValue) *
                     (args.size - callable->custom_function.arg_count));

          env_set(new_env, callable->custom_function.variadic,
                  _lisp_init(&list));
        }

        assert(callable->custom_function.body_size > 0);

        for (size_t i = 0; i < callable->custom_function.body_size - 1; i++) {
          evaluate_lisp(callable->custom_function.body[i], new_env);
        }

        lisp = callable->custom_function
                   .body[callable->custom_function.body_size - 1];
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

  LispValues nodes;
  da_init(&nodes);

  ParseResult parser_result = parse(tokens.array, tokens.size, &nodes);
  da_free(&tokens);

  if (parser_result)
    return ERROR_TO_LISP("could not parse");

  LispValue return_value = LISP_NIL;
  for (size_t i = 0; i < nodes.size; i++) {
    return_value = evaluate_lisp(nodes.array[i], env);
  }

  return return_value;
}
