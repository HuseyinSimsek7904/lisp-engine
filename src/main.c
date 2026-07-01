#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <readline/chardefs.h>
#include <readline/history.h>
#include <readline/readline.h>

#include "core.h"
#include "evaluation.h"
#include "lexer.h"
#include "lisp_value.h"

#define DEFINE_BUILTIN(n)                                                      \
  LispValue builtin_##n(LispValue *args, size_t arg_count, Environment *env)

#define REQUIRES_EQ_ARGS(c)                                                    \
  if (arg_count != c)                                                          \
  return ERROR_TO_LISP("expected exactly " #c " arguments")

#define REQUIRES_MIN_ARGS(c)                                                   \
  if (arg_count < c)                                                           \
  return ERROR_TO_LISP("expected at least " #c " arguments")

#define REQUIRES_MAX_ARGS(c)                                                   \
  if (arg_count > c)                                                           \
  return ERROR_TO_LISP("expected at most " #c " arguments")

#define ARG0 args[0]
#define ARG1 args[1]

DEFINE_BUILTIN(set) {
  REQUIRES_EQ_ARGS(2);

  if (ARG0->type != LISP_SYMBOL_T)
    return ERROR_TO_LISP("first argument to `set` must be a symbol");

  char *symbol = ARG0->symbol;
  LispValue value = evaluate_lisp(ARG1, env);

  env_set(env, symbol, value);

  return value;
}

DEFINE_BUILTIN(setg) {
  REQUIRES_EQ_ARGS(2);

  if (ARG0->type != LISP_SYMBOL_T)
    return ERROR_TO_LISP("first argument to `setg` must be a symbol");

  char *symbol = ARG0->symbol;
  LispValue value = evaluate_lisp(ARG1, env);

  Environment *global = env;
  while (global->other)
    global = global->other;

  env_set(global, symbol, value);

  return value;
}

// TODO: `let` should be able to do tail calls, but tail calls cannot use new
// environments currently.
DEFINE_BUILTIN(let) {
  REQUIRES_MIN_ARGS(2);

  LispValue defs_node = ARG0;

  if (defs_node->type == LISP_LIST_T)
    defs_node = lisp_consify(defs_node);

  else if (defs_node->type != LISP_CONS_T)
    return ERROR_TO_LISP("first argument to `let` must be a list-like");

  Environment new_env = {.other = env};
  da_init(&new_env);

  LispValue remaining_defs = defs_node;

  while (remaining_defs->type == LISP_CONS_T) {
    LispValue def_node = remaining_defs->cons.head;
    remaining_defs = remaining_defs->cons.tail;

    if (def_node->type == LISP_LIST_T)
      def_node = lisp_consify(def_node);

    else if (def_node->type != LISP_CONS_T)
      return ERROR_TO_LISP(
          "first argument to `let` must be a list of list-likes");

    if (lisp_get_len(def_node) != 2)
      return ERROR_TO_LISP(
          "first argument to `let` must be a list of lists with sizes 2");

    if (def_node->cons.head->type != LISP_SYMBOL_T)
      return ERROR_TO_LISP("first argument to `let` must be a list of lists "
                           "with first element being a symbol");

    char *symbol = def_node->cons.head->symbol;

    if (env_find_shallow(&new_env, symbol))
      return ERROR_TO_LISP("redefined symbol");

    // TODO: Maybe the passed environment should be the new environment.
    // That way one variable defined can be used in the other one.
    LispValue value = evaluate_lisp(def_node->cons.tail->cons.head, env);

    env_set(&new_env, symbol, value);
  }

  assert(remaining_defs->type == LISP_NIL_T);

  LispValue return_value = LISP_NIL;
  for (size_t i = 1; i < arg_count; i++) {
    return_value = evaluate_lisp(args[i], &new_env);
  }

  env_free(&new_env);

  return return_value;
}

DEFINE_BUILTIN(if) {
  REQUIRES_MIN_ARGS(2);

  LispValue condition = evaluate_lisp(ARG0, env);

  if (condition->type == LISP_ERROR_T)
    return condition;

  if (condition->type != LISP_BOOL_T)
    return ERROR_TO_LISP("`if` requires a boolean");

  if (condition->boolean)
    return ARG1;

  for (size_t i = 2; i < arg_count - 1; i++) {
    evaluate_lisp(args[i], env);
  }
  return args[arg_count - 1];
}

LispValue define_fn_or_macro(LispValue *args, size_t arg_count,
                             Environment *env, bool is_macro) {
  REQUIRES_MIN_ARGS(2);

  if (ARG0->type == LISP_LIST_T)
    ARG0 = lisp_consify(ARG0);

  else if (ARG0->type != LISP_CONS_T)
    return ERROR_TO_LISP("first argument to `fn` must be a list-like");

  bool variadic = false;

  LispValue arg_symbols = LISP_NIL;

  {
    LispValue *last = &arg_symbols;

    LispValue remaining_args = ARG0;
    LispValue arg;

    while (remaining_args->cons.tail->type == LISP_CONS_T) {
      arg = remaining_args->cons.head;
      remaining_args = remaining_args->cons.tail;

      if (arg->type != LISP_SYMBOL_T) {
        return ERROR_TO_LISP(
            "first argument to `fn` must be a list of symbols");
      }

      lisp_rappend_use_last(last, arg);
      last = &((*last)->cons.tail);
    }

    assert(remaining_args->cons.tail->type == LISP_NIL_T);
    arg = remaining_args->cons.head;

    if (arg->type == LISP_CONS_T) {
      if (lisp_get_len(arg) != 2)
        return ERROR_TO_LISP(
            "list inside `fn` arguments list must be of size 2");

      if (arg->cons.head->type != LISP_SYMBOL_T ||
          strcmp(arg->cons.head->string, "rest"))
        return ERROR_TO_LISP("first item in the list in the `fn` arguments "
                             "list must be a `rest` symbol");

      if (arg->cons.tail->cons.head->type != LISP_SYMBOL_T)
        return ERROR_TO_LISP("`rest` argument must be a symbol");

      variadic = true;

      lisp_rappend_use_last(last, arg->cons.tail->cons.head);
      last = &((*last)->cons.tail);

    } else if (arg->type == LISP_SYMBOL_T) {
      lisp_rappend_use_last(last, arg);
      last = &((*last)->cons.tail);

    } else {
      return ERROR_TO_LISP("first argument to `fn` must be a list of symbols");
    }
  }

  LispValue body = lisp_consify_array(args + 1, arg_count - 1);

  return _lisp_init(&(_LispValue){.type = LISP_FUNC_T,
                                  .custom_function = {.is_macro = is_macro,
                                                      .args = arg_symbols,
                                                      .variadic = variadic,
                                                      .body = body}});
}

LispValue builtin_fn(LispValue *args, size_t arg_count, Environment *env) {
  return define_fn_or_macro(args, arg_count, env, false);
}

LispValue builtin_macro(LispValue *args, size_t arg_count, Environment *env) {
  return define_fn_or_macro(args, arg_count, env, true);
}

// TODO: Inside `do`, if any of the items return error, the control flow should
// be stopped and the error value should be returned.
DEFINE_BUILTIN(do) {
  REQUIRES_MIN_ARGS(1);

  for (size_t i = 0; i < arg_count - 1; i++) {
    evaluate_lisp(args[i], env);
  }

  return args[arg_count - 1];
}

DEFINE_BUILTIN(print) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i]->type != LISP_STRING_T)
      return ERROR_TO_LISP("invalid type for argument to `print`");

  for (size_t i = 0; i < arg_count; i++) {
    printf("%s\n", args[i]->string);
  }

  return LISP_NIL;
}

DEFINE_BUILTIN(sum) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i]->type != LISP_INT64_T)
      return ERROR_TO_LISP("invalid type for argument to `+`");

  int64_t sum = 0;
  for (size_t i = 0; i < arg_count; i++) {
    sum += args[i]->integer;
  }

  return INT64_TO_LISP(sum);
}

DEFINE_BUILTIN(neg_or_sub) {
  REQUIRES_MIN_ARGS(1);
  REQUIRES_MAX_ARGS(2);

  for (size_t i = 0; i < arg_count; i++)
    if (args[i]->type != LISP_INT64_T)
      return ERROR_TO_LISP("invalid type for argument to `-`");

  return INT64_TO_LISP(arg_count == 1 ? -ARG0->integer
                                      : ARG0->integer - ARG1->integer);
}

DEFINE_BUILTIN(list) { return lisp_consify_array(args, arg_count); }

DEFINE_BUILTIN(equal) {
  // NOTE: Later, we can change this function to accept any number of arguments.

  return BOOL_TO_LISP(lisp_is_ident(ARG0, ARG1));
}

DEFINE_BUILTIN(less_than) {
  // NOTE: Later, we can change this function to accept any number of arguments.
  REQUIRES_EQ_ARGS(2);

  if (ARG0->type != LISP_INT64_T || ARG1->type != LISP_INT64_T)
    return ERROR_TO_LISP("`<` expects 2 integers");

  return BOOL_TO_LISP(ARG0->integer < ARG1->integer);
}

DEFINE_BUILTIN(read_file) {
  REQUIRES_EQ_ARGS(1);

  if (ARG0->type != LISP_STRING_T)
    return ERROR_TO_LISP("`read-file` expects a string");

  char *buffer = read_file(ARG0->string);

  if (!buffer)
    return ERROR_TO_LISP("could not read file contents");

  return STR_TO_LISP(buffer);
}

DEFINE_BUILTIN(parse_lisp) {
  REQUIRES_EQ_ARGS(1);

  if (ARG0->type != LISP_STRING_T)
    return ERROR_TO_LISP("`read-file` expects a string");

  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(ARG0->string, &tokens);

  if (lexer_result)
    return ERROR_TO_LISP("could not generate tokens for lisp");

  LispValue parser_result = parse(tokens.array, tokens.size);
  da_free(&tokens);

  return parser_result;
}

DEFINE_BUILTIN(eval) {
  LispValue return_value = LISP_NIL;
  for (size_t i = 0; i < arg_count; i++) {
    return_value = evaluate_lisp(args[i], env);
  }
  return return_value;
}

DEFINE_BUILTIN(unpack) {
  LispValue items = LISP_NIL;
  LispValue *last = &items;

  for (size_t i = 0; i < arg_count; i++) {
    LispValue arg = args[i];

    if (arg->type == LISP_LIST_T) {
      arg = lisp_consify(arg);
    } else if (arg->type != LISP_CONS_T) {
      return ERROR_TO_LISP("`unpack` expects list-likes");
    }

    *last = args[i];
    last = lisp_cons_find_term_nil(last);
  }

  return _lisp_init(&(_LispValue){.type = LISP_UNPACKED_T, .item = items});
}

DEFINE_BUILTIN(quote) {
  REQUIRES_EQ_ARGS(1);
  return ARG0;
}

DEFINE_BUILTIN(head) {
  REQUIRES_EQ_ARGS(1);

  LispValue head = lisp_head(ARG0);

  if (!head)
    return ERROR_TO_LISP("cannot `head` on value");

  return head;
}

DEFINE_BUILTIN(tail) {
  REQUIRES_EQ_ARGS(1);

  LispValue tail = lisp_tail(ARG0);

  if (!tail)
    return ERROR_TO_LISP("cannot `tail` on value");

  return tail;
}

#define DEFINE_BUILTIN_FUNCTION(n, c, m, t)                                    \
  env_set(                                                                     \
      &global_env, n,                                                          \
      _lisp_init(&(_LispValue){                                                \
          .type = LISP_BUILTIN_FUNC_T,                                         \
          .builtin_function = {.callback = (LispValue (*)(LispValue *, size_t, \
                                                          void *))builtin_##c, \
                               .is_macro = m,                                  \
                               .tail_call = t,                                 \
                               .name = n}}));

LispValue LISP_NIL;
LispValue LISP_TRUE;
LispValue LISP_FALSE;

int main(void) {
  Environment global_env = {.other = NULL};
  da_init(&global_env);

  // Singletons
  LISP_NIL = _lisp_init(&(_LispValue){.type = LISP_NIL_T});
  LISP_FALSE = _lisp_init(&(_LispValue){.type = LISP_BOOL_T, .boolean = false});
  LISP_TRUE = _lisp_init(&(_LispValue){.type = LISP_BOOL_T, .boolean = true});

  env_set(&global_env, "nil", LISP_NIL);
  env_set(&global_env, "false", LISP_FALSE);
  env_set(&global_env, "true", LISP_TRUE);

  // Builtin functions
  DEFINE_BUILTIN_FUNCTION("print", print, false, false);
  DEFINE_BUILTIN_FUNCTION("+", sum, false, false);
  DEFINE_BUILTIN_FUNCTION("-", neg_or_sub, false, false);
  DEFINE_BUILTIN_FUNCTION("list", list, false, false);
  DEFINE_BUILTIN_FUNCTION("=", equal, false, false);
  DEFINE_BUILTIN_FUNCTION("<", less_than, false, false);
  DEFINE_BUILTIN_FUNCTION("read-file", read_file, false, false);
  DEFINE_BUILTIN_FUNCTION("parse-lisp", parse_lisp, false, false);
  DEFINE_BUILTIN_FUNCTION("eval", eval, false, false);
  DEFINE_BUILTIN_FUNCTION("unpack", unpack, false, false);
  DEFINE_BUILTIN_FUNCTION("head", head, false, false);
  DEFINE_BUILTIN_FUNCTION("tail", tail, false, false);

  // Builtin macros
  DEFINE_BUILTIN_FUNCTION("set", set, true, false);
  DEFINE_BUILTIN_FUNCTION("setg", setg, true, false);
  DEFINE_BUILTIN_FUNCTION("quote", quote, true, false);
  DEFINE_BUILTIN_FUNCTION("fn", fn, true, false);
  DEFINE_BUILTIN_FUNCTION("macro", macro, true, false);

  // Builtin macros with tail call optimization
  DEFINE_BUILTIN_FUNCTION("let", let, true, false);
  DEFINE_BUILTIN_FUNCTION("if", if, true, true);
  DEFINE_BUILTIN_FUNCTION("do", do, true, true);

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
