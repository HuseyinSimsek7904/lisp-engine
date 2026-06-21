#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include <readline/chardefs.h>
#include <readline/history.h>
#include <readline/readline.h>

#include "core.h"
#include "lexer.h"
#include "lisp_value.h"

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

    case LISP_TUPLE_T:
      if (!lisp->items.size) {
        roll_env_back(env, original);
        return NIL_SINGLETON_LISP;
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
              .type = LISP_TUPLE_T,
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

  LispValue return_value = NIL_SINGLETON_LISP;
  for (size_t i = 0; i < nodes.size; i++) {
    return_value = evaluate_lisp(nodes.array[i], env);
  }

  return return_value;
}

LispValue builtin_set(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`set` requires at least 2 arguments");

  if (args[0]->type != LISP_SYMBOL_T)
    return ERROR_TO_LISP("first argument to `set` must be a symbol");

  char *symbol = args[0]->symbol;
  LispValue value = evaluate_lisp(args[1], env);

  env_set(env, symbol, value);

  return value;
}

LispValue builtin_setg(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`setg` requires at least 2 arguments");

  if (args[0]->type != LISP_SYMBOL_T)
    return ERROR_TO_LISP("first argument to `setg` must be a symbol");

  char *symbol = args[0]->symbol;
  LispValue value = evaluate_lisp(args[1], env);

  Environment *global = env;
  while (global->other)
    global = global->other;

  env_set(global, symbol, value);

  return value;
}

// TODO: `let` should be able to do tail calls, but tail calls cannot use new
// environments currently.
LispValue builtin_let(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`let` requires at least 2 arguments");

  LispValue defs_node = args[0];

  if (defs_node->type != LISP_TUPLE_T)
    return ERROR_TO_LISP("first argument to `let` must be a tuple");

  Environment new_env = {.other = env};
  da_init(&new_env);

  for (size_t i = 0; i < defs_node->items.size; i++) {
    LispValue def_node = defs_node->items.array[i];

    if (def_node->type != LISP_TUPLE_T)
      return ERROR_TO_LISP("first argument to `let` must be a tuple of tuples");

    if (def_node->items.size != 2)
      return ERROR_TO_LISP(
          "first argument to `let` must be a tuple of tuples with sizes 2");

    if (def_node->items.array[0]->type != LISP_SYMBOL_T)
      return ERROR_TO_LISP("first argument to `let` must be a tuple of tuples "
                           "with first element being a symbol");

    char *symbol = def_node->items.array[0]->symbol;

    if (env_find_shallow(&new_env, symbol))
      return ERROR_TO_LISP("redefined symbol");

    // TODO: Maybe the passed environment should be the new environment.
    // That way one variable defined can be used in the other one.
    LispValue value = evaluate_lisp(def_node->items.array[1], env);

    env_set(&new_env, symbol, value);
  }

  LispValue return_value = NIL_SINGLETON_LISP;
  for (size_t i = 1; i < arg_count; i++) {
    return_value = evaluate_lisp(args[i], &new_env);
  }

  env_free(&new_env);

  return return_value;
}

LispValue builtin_if(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`if` requires at least 2 arguments");

  LispValue condition = evaluate_lisp(args[0], env);

  if (condition->type == LISP_ERROR_T)
    return condition;

  if (condition->type != LISP_BOOL_T)
    return ERROR_TO_LISP("`if` requires a boolean");

  if (condition->boolean)
    return args[1];

  for (size_t i = 2; i < arg_count - 1; i++) {
    evaluate_lisp(args[i], env);
  }
  return args[arg_count - 1];
}

LispValue define_fn_or_macro(LispValue *args, size_t arg_count,
                             Environment *env, bool is_macro) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`fn` requires at least 2 arguments");

  if (args[0]->type != LISP_TUPLE_T)
    return ERROR_TO_LISP("first argument to `fn` must be a tuple");

  char *variadic = NULL;
  char **new_args = malloc(sizeof(char *) * arg_count);
  for (size_t i = 0; i < args[0]->items.size; i++) {
    LispValue arg = args[0]->items.array[i];

    if (arg->type == LISP_SYMBOL_T) {
      if (args[0]->items.array[i]->type != LISP_SYMBOL_T)
        return ERROR_TO_LISP("`fn` argument list must be a tuple of symbols");

      for (size_t j = 0; j < i; j++) {
        if (strcmp(new_args[j], arg->symbol) == 0)
          return ERROR_TO_LISP("repeated argument");
      }

      new_args[i] = arg->symbol;

    } else if (arg->type == LISP_TUPLE_T) {
      if (i != args[0]->items.size - 1) {
        TODO();
        return ERROR_TO_LISP(
            "only the last item in `fn` arguments list can be a tuple");
      }

      if (arg->items.size != 2)
        return ERROR_TO_LISP(
            "tuple inside `fn` arguments list must be of size 2");

      if (arg->items.array[0]->type != LISP_SYMBOL_T ||
          strcmp(arg->items.array[0]->string, "rest"))
        return ERROR_TO_LISP("first item in the tuple in the `fn` arguments "
                             "list must be a `rest` symbol");

      if (arg->items.array[1]->type != LISP_SYMBOL_T)
        return ERROR_TO_LISP("`rest` argument must be a symbol");

      assert(!variadic);

      variadic = arg->items.array[1]->symbol;
      for (size_t j = 0; j < i; j++)
        if (strcmp(new_args[j], variadic) == 0)
          return ERROR_TO_LISP("repeated `rest` argument");
    }
  }

  return _lisp_init(&(_LispValue){
      .type = LISP_FUNC_T,
      .custom_function = {.is_macro = is_macro,
                          .args = new_args,
                          .arg_count = args[0]->items.size - (variadic ? 1 : 0),
                          .body = args + 1,
                          .body_size = arg_count - 1,
                          .variadic = variadic}});
}

LispValue builtin_fn(LispValue *args, size_t arg_count, Environment *env) {
  return define_fn_or_macro(args, arg_count, env, false);
}

LispValue builtin_macro(LispValue *args, size_t arg_count, Environment *env) {
  return define_fn_or_macro(args, arg_count, env, true);
}

// TODO: Inside `do`, if any of the items return error, the control flow should
// be stopped and the error value should be returned.
LispValue builtin_do(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count - 1; i++) {
    evaluate_lisp(args[i], env);
  }

  return args[arg_count - 1];
}

LispValue builtin_print(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i]->type != LISP_STRING_T)
      return ERROR_TO_LISP("invalid type for argument to `print`");

  for (size_t i = 0; i < arg_count; i++) {
    printf("%s\n", args[i]->string);
  }

  return NIL_SINGLETON_LISP;
}

LispValue builtin_sum(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i]->type != LISP_INT64_T)
      return ERROR_TO_LISP("invalid type for argument to `+`");

  int64_t sum = 0;
  for (size_t i = 0; i < arg_count; i++) {
    sum += args[i]->integer;
  }

  return INT64_TO_LISP(sum);
}

LispValue builtin_neg_or_sub(LispValue *args, size_t arg_count,
                             Environment *env) {
  if (arg_count < 1 || arg_count > 2)
    return ERROR_TO_LISP("`-` expectes either 1 or 2 arguments");

  for (size_t i = 0; i < arg_count; i++)
    if (args[i]->type != LISP_INT64_T)
      return ERROR_TO_LISP("invalid type for argument to `-`");

  return INT64_TO_LISP(arg_count == 1 ? -args[0]->integer
                                      : args[0]->integer - args[1]->integer);
}

LispValue builtin_list(LispValue *args, size_t arg_count, Environment *env) {
  return _lisp_init(&(_LispValue){.type = LISP_TUPLE_T,
                                  .items = {.array = args, .size = arg_count}});
}

LispValue builtin_equal(LispValue *args, size_t arg_count, Environment *env) {
  // NOTE: Later, we can change this function to accept any number of arguments.
  if (arg_count != 2)
    return ERROR_TO_LISP("`=` expects 2 arguments");

  return BOOL_TO_LISP(lisp_is_ident(args[0], args[1]));
}

LispValue builtin_less_than(LispValue *args, size_t arg_count,
                            Environment *env) {
  // NOTE: Later, we can change this function to accept any number of arguments.
  if (arg_count != 2)
    return ERROR_TO_LISP("`<` expects 2 arguments");

  if (args[0]->type != LISP_INT64_T || args[1]->type != LISP_INT64_T)
    return ERROR_TO_LISP("`<` expects 2 integers");

  return BOOL_TO_LISP(args[0]->integer < args[1]->integer);
}

LispValue builtin_read_file(LispValue *args, size_t arg_count,
                            Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`read-file` expects 1 arguments");

  if (args[0]->type != LISP_STRING_T)
    return ERROR_TO_LISP("`read-file` expects strings");

  char *buffer = read_file(args[0]->string);

  if (!buffer)
    return ERROR_TO_LISP("could not read file contents");

  return STR_TO_LISP(buffer);
}

LispValue builtin_parse_lisp(LispValue *args, size_t arg_count,
                             Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`read-file` expects 1 arguments");

  if (args[0]->type != LISP_STRING_T)
    return ERROR_TO_LISP("`read-file` expects strings");

  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(args[0]->string, &tokens);

  if (lexer_result)
    return ERROR_TO_LISP("could not generate tokens for lisp");

  LispValues nodes;
  da_init(&nodes);

  ParseResult parser_result = parse(tokens.array, tokens.size, &nodes);
  da_free(&tokens);

  if (parser_result)
    return ERROR_TO_LISP("could not parse");

  return _lisp_init(
      &(_LispValue){.type = LISP_TUPLE_T,
                    .items = {.array = nodes.array, .size = nodes.size}});
}

LispValue builtin_eval(LispValue *args, size_t arg_count, Environment *env) {
  LispValue return_value = NIL_SINGLETON_LISP;
  for (size_t i = 0; i < arg_count; i++) {
    return_value = evaluate_lisp(args[i], env);
  }
  return return_value;
}

LispValue builtin_unpack(LispValue *args, size_t arg_count, Environment *env) {
  size_t item_count = 0;
  for (size_t i = 0; i < arg_count; i++) {
    if (args[i]->type != LISP_TUPLE_T)
      return ERROR_TO_LISP("`unpack` expects tuples");

    item_count += args[i]->items.size;
  }

  LispValue *array = calloc(item_count, sizeof(LispValue));
  size_t j = 0;
  for (size_t i = 0; i < arg_count; i++) {
    memcpy(&array[j], args[i]->items.array,
           args[i]->items.size * sizeof(LispValue));
    j += args[i]->items.size;
  }

  return _lisp_init(&(_LispValue){
      .type = LISP_UNPACKED_T, .items = {.array = array, .size = item_count}});
}

LispValue builtin_quote(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`quote` expects exactly 1 arguments");

  return args[0];
}

LispValue builtin_head(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`head` expects exactly 1 arguments");

  if (args[0]->type != LISP_TUPLE_T)
    return ERROR_TO_LISP("`head` expects a tuple");

  if (!args[0]->items.size)
    return ERROR_TO_LISP("`head` expects a non-empty tuple");

  return args[0]->items.array[0];
}

LispValue builtin_tail(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`tail` expects exactly 1 arguments");

  if (args[0]->type != LISP_TUPLE_T)
    return ERROR_TO_LISP("`tail` expects a tuple");

  if (!args[0]->items.size)
    return ERROR_TO_LISP("`tail` expects a non-empty tuple");

  return _lisp_init(&(_LispValue){.type = LISP_TUPLE_T,
                                  .items = {.array = args[0]->items.array + 1,
                                            .size = args[0]->items.size - 1}});
}

#define DEFINE_BUILTIN_FUNCTION(n, c, m, t)                                    \
  env_set(&global_env, n,                                                      \
          _lisp_init(&(_LispValue){                                            \
              .type = LISP_BUILTIN_FUNC_T,                                     \
              .builtin_function = {                                            \
                  .callback = (LispValue (*)(LispValue *, size_t, void *))c,   \
                  .is_macro = m,                                               \
                  .tail_call = t,                                              \
                  .name = n}}));

int main(void) {
  Environment global_env = {.other = NULL};
  da_init(&global_env);

  // Singletons
  env_set(&global_env, "nil", NIL_SINGLETON_LISP);
  env_set(&global_env, "true", BOOL_TO_LISP(true));
  env_set(&global_env, "false", BOOL_TO_LISP(false));

  // Builtin functions
  DEFINE_BUILTIN_FUNCTION("print", builtin_print, false, false);
  DEFINE_BUILTIN_FUNCTION("+", builtin_sum, false, false);
  DEFINE_BUILTIN_FUNCTION("-", builtin_neg_or_sub, false, false);
  DEFINE_BUILTIN_FUNCTION("list", builtin_list, false, false);
  DEFINE_BUILTIN_FUNCTION("=", builtin_equal, false, false);
  DEFINE_BUILTIN_FUNCTION("<", builtin_less_than, false, false);
  DEFINE_BUILTIN_FUNCTION("read-file", builtin_read_file, false, false);
  DEFINE_BUILTIN_FUNCTION("parse-lisp", builtin_parse_lisp, false, false);
  DEFINE_BUILTIN_FUNCTION("eval", builtin_eval, false, false);
  DEFINE_BUILTIN_FUNCTION("unpack", builtin_unpack, false, false);
  DEFINE_BUILTIN_FUNCTION("head", builtin_head, false, false);
  DEFINE_BUILTIN_FUNCTION("tail", builtin_tail, false, false);

  // Builtin macros
  DEFINE_BUILTIN_FUNCTION("set", builtin_set, true, false);
  DEFINE_BUILTIN_FUNCTION("setg", builtin_setg, true, false);
  DEFINE_BUILTIN_FUNCTION("quote", builtin_quote, true, false);
  DEFINE_BUILTIN_FUNCTION("fn", builtin_fn, true, false);
  DEFINE_BUILTIN_FUNCTION("macro", builtin_macro, true, false);

  // Builtin macros with tail call optimization
  DEFINE_BUILTIN_FUNCTION("let", builtin_let, true, false);
  DEFINE_BUILTIN_FUNCTION("if", builtin_if, true, true);
  DEFINE_BUILTIN_FUNCTION("do", builtin_do, true, true);

  char *startup[] = {"core.l"};

  for (size_t i = 0; i < sizeof(startup) / sizeof(startup[0]); i++) {
    char *buffer = read_file(startup[i]);
    if (!buffer)
      fprintf(stderr, "warning: could not read contents of %s\n", startup[i]);

    repl(buffer, &global_env);
  }

  while (true) {
    char *lisp = readline("> ");

    if (!lisp)
      exit(0);

    add_history(lisp);

    LispValue return_value = repl(lisp, &global_env);

    free(lisp);

    lisp_print(return_value, true);
    printf("\n");
  }
}
