#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <readline/chardefs.h>
#include <readline/history.h>
#include <readline/readline.h>

#include "core.h"
#include "evaluation.h"
#include "lexer.h"
#include "lisp_value.h"

#define DEFINE_BUILTIN(n)                                                      \
  LispValue builtin_##n(LispValue args, Environment *env)

#define REQUIRES_EQ_ARGS(c)                                                    \
  if (lisp_get_len(args) != c)                                                 \
  return ERROR_TO_LISP("expected exactly " #c " arguments")

#define REQUIRES_MIN_ARGS(c)                                                   \
  if (lisp_get_len(args) < c)                                                  \
  return ERROR_TO_LISP("expected at least " #c " arguments")

#define REQUIRES_MAX_ARGS(c)                                                   \
  if (lisp_get_len(args) > c)                                                  \
  return ERROR_TO_LISP("expected at most " #c " arguments")

#define ARG0 args->cons.head
#define ARG1 args->cons.tail->cons.head

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

  Environment new_env = {.other = env};
  da_init(&new_env);

  if (ARG0->type == LISP_LIST_T) {
    LispValue remaining_defs = ARG0;

    while (remaining_defs->type == LISP_LIST_T) {
      LispValue def_node = remaining_defs->cons.head;
      remaining_defs = remaining_defs->cons.tail;

      if (def_node->type != LISP_LIST_T)
        return ERROR_TO_LISP("first argument to `let` must be a list of lists");

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

  } else if (ARG0->type != LISP_NIL_T)
    return ERROR_TO_LISP("first argument to `let` must be a list");

  LispValue return_value = LISP_NIL;
  LispValue remaining_args = args->cons.tail;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    return_value = evaluate_lisp(arg, &new_env);
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

  LispValue remaining_args = args->cons.tail->cons.tail;
  while (remaining_args->cons.tail->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    evaluate_lisp(arg, env);
  }

  return remaining_args->cons.head;
}

LispValue define_fn_or_macro(LispValue args, Environment *env, bool is_macro) {
  REQUIRES_MIN_ARGS(2);

  bool variadic = false;
  LispValue arg_symbols = LISP_NIL;

  if (ARG0->type == LISP_LIST_T) {
    LispValue *last = &arg_symbols;

    LispValue remaining_args = ARG0;
    LispValue arg;

    while (remaining_args->cons.tail->type == LISP_LIST_T) {
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

    if (arg->type == LISP_LIST_T) {
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

    } else if (arg->type == LISP_SYMBOL_T) {
      lisp_rappend_use_last(last, arg);

    } else {
      return ERROR_TO_LISP("first argument to `fn` must be a list of symbols");
    }

  } else if (ARG0->type != LISP_NIL_T)
    return ERROR_TO_LISP("first argument to `fn` must be a list");

  LispValue body = args->cons.tail;

#ifdef _DEBUG_MAKE_FN
  fprintf(stderr, "make_%s: variadic=%s ", is_macro ? "macro" : "fn",
          variadic ? "true" : "false");
  fprintf(stderr, " args=");
  lisp_fprint(stderr, arg_symbols, true);
  fprintf(stderr, " body=");
  lisp_fprint(stderr, body, true);
  fprintf(stderr, "\n");
#endif

  return _lisp_init(&(_LispValue){.type = LISP_FUNC_T,
                                  .custom_function = {.is_macro = is_macro,
                                                      .args = arg_symbols,
                                                      .variadic = variadic,
                                                      .body = body}});
}

DEFINE_BUILTIN(fn) { return define_fn_or_macro(args, env, false); }

DEFINE_BUILTIN(macro) { return define_fn_or_macro(args, env, true); }

// TODO: Inside `do`, if any of the items return error, the control flow should
// be stopped and the error value should be returned.
DEFINE_BUILTIN(do) {
  REQUIRES_MIN_ARGS(1);

  LispValue remaining_args = args;
  while (remaining_args->cons.tail->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    evaluate_lisp(arg, env);
  }

  assert(remaining_args->cons.tail->type == LISP_NIL_T);

  return remaining_args->cons.head;
}

DEFINE_BUILTIN(print) {
  LispValue remaining_args = args;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    if (arg->type != LISP_STRING_T)
      return ERROR_TO_LISP("invalid type for argument to `print`");
  }

  assert(remaining_args->type == LISP_NIL_T);

  remaining_args = args;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    printf("%s\n", arg->string);
  }

  assert(remaining_args->type == LISP_NIL_T);

  return LISP_NIL;
}

DEFINE_BUILTIN(sum) {
  int64_t sum = 0;

  LispValue remaining_args = args;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    if (arg->type != LISP_INT64_T)
      return ERROR_TO_LISP("invalid type for argument to `+`");

    sum += arg->integer;
  }

  return INT64_TO_LISP(sum);
}

DEFINE_BUILTIN(neg_or_sub) {
  size_t arg_count = lisp_get_len(args);

  REQUIRES_MIN_ARGS(1);
  REQUIRES_MAX_ARGS(2);

  if (ARG0->type != LISP_INT64_T ||
      (arg_count == 2 && ARG1->type != LISP_INT64_T))
    return ERROR_TO_LISP("`-` expects 1 or 2 integers");

  return INT64_TO_LISP(arg_count == 1 ? -ARG0->integer
                                      : ARG0->integer - ARG1->integer);
}

DEFINE_BUILTIN(list) { return args; }

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
  REQUIRES_MIN_ARGS(1);

  LispValue return_value = LISP_NIL;
  LispValue remaining_args = args;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    return_value = evaluate_lisp(arg, env);
  }
  assert(remaining_args->type == LISP_NIL_T);

  return return_value;
}

DEFINE_BUILTIN(unpack) {
  LispValue items = LISP_NIL;
  LispValue *last = &items;

  LispValue remaining_args = args;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    if (arg->type != LISP_LIST_T && arg->type != LISP_NIL_T)
      return ERROR_TO_LISP("`unpack` expects list");

    *last = arg;
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

DEFINE_BUILTIN(typeof) {
  REQUIRES_EQ_ARGS(1);

  return _lisp_init(&(_LispValue){.type = LISP_TYPE_T, .ref_type = ARG0->type});
}

#define DEFINE_BUILTIN_FUNCTION(n, c, m, t)                                    \
  env_set(&global_env, n,                                                      \
          _lisp_init(&(_LispValue){                                            \
              .type = LISP_BUILTIN_FUNC_T,                                     \
              .builtin_function = {                                            \
                  .callback = (LispValue (*)(LispValue, void *))builtin_##c,   \
                  .is_macro = m,                                               \
                  .tail_call = t,                                              \
                  .name = n}}));

#define DEFINE_TYPE_REP(n, r)                                                  \
  env_set(&global_env, n,                                                      \
          _lisp_init(&(_LispValue){.type = LISP_TYPE_T, .ref_type = r}));

LispValue LISP_NIL;
LispValue LISP_TRUE;
LispValue LISP_FALSE;

int main(int argc, char **argv) {
  bool interactive = true;
  bool test_mode = false;

  int opt;

  while ((opt = getopt(argc, argv, "nt")) != -1) {
    switch (opt) {
    case 'n':
      interactive = false;
      break;

    case 't':
      test_mode = true;
      interactive = false;
      break;

    default:
      break;
    }
  }

  Environment global_env = {.other = NULL};
  da_init(&global_env);

  // Singletons.
  LISP_NIL = _lisp_init(&(_LispValue){.type = LISP_NIL_T});
  LISP_FALSE = _lisp_init(&(_LispValue){.type = LISP_BOOL_T, .boolean = false});
  LISP_TRUE = _lisp_init(&(_LispValue){.type = LISP_BOOL_T, .boolean = true});

  env_set(&global_env, "nil", LISP_NIL);
  env_set(&global_env, "false", LISP_FALSE);
  env_set(&global_env, "true", LISP_TRUE);

  // Type representative singletons.
  DEFINE_TYPE_REP("nil-t", LISP_NIL_T);
  DEFINE_TYPE_REP("type-t", LISP_TYPE_T);
  DEFINE_TYPE_REP("error-t", LISP_ERROR_T);
  DEFINE_TYPE_REP("str-t", LISP_STRING_T);
  DEFINE_TYPE_REP("int64-t", LISP_INT64_T);
  DEFINE_TYPE_REP("bool-t", LISP_BOOL_T);
  DEFINE_TYPE_REP("sym-t", LISP_SYMBOL_T);
  DEFINE_TYPE_REP("builtin-t", LISP_BUILTIN_FUNC_T);
  DEFINE_TYPE_REP("fn-t", LISP_FUNC_T);
  DEFINE_TYPE_REP("list-t", LISP_LIST_T);

  // Builtin functions.
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
  DEFINE_BUILTIN_FUNCTION("typeof", typeof, false, false);

  // Builtin macros.
  DEFINE_BUILTIN_FUNCTION("set", set, true, false);
  DEFINE_BUILTIN_FUNCTION("setg", setg, true, false);
  DEFINE_BUILTIN_FUNCTION("quote", quote, true, false);
  DEFINE_BUILTIN_FUNCTION("fn", fn, true, false);
  DEFINE_BUILTIN_FUNCTION("macro", macro, true, false);

  // Builtin macros with tail call optimization.
  DEFINE_BUILTIN_FUNCTION("let", let, true, false);
  DEFINE_BUILTIN_FUNCTION("if", if, true, true);
  DEFINE_BUILTIN_FUNCTION("do", do, true, true);

  // Builtin startup files.
  char *startup[] = {"core/core.l"};

  for (size_t i = 0; i < sizeof(startup) / sizeof(startup[0]); i++) {
    char *buffer = read_file(startup[i]);

    if (!buffer) {
      fprintf(stderr, "warning: could not read contents of %s\n", startup[i]);
      continue;
    }

    repl(buffer, &global_env);

    free(buffer);
  }

  // Custom passed startup files.
  for (size_t i = optind; i < argc; i++) {
    char *buffer = read_file(argv[i]);

    if (!buffer) {
      fprintf(stderr, "warning: could not read contents of %s\n", argv[i]);
      continue;
    }

    LispValue return_value = repl(buffer, &global_env);

    free(buffer);

    // In test mode, every file must return nil.
    // If they do not, that means that the test failed.
    if (test_mode && return_value->type != LISP_NIL_T) {
      fprintf(stderr, "fail: %s test failed\n", argv[i]);
      lisp_fprint(stderr, return_value, true);
      fprintf(stderr, "\n");
    }
  }

  // Interactive mode.
  while (interactive) {
    char *lisp = readline("> ");

    if (!lisp)
      interactive = false;

    add_history(lisp);

    LispValue return_value = repl(lisp, &global_env);

    free(lisp);

    lisp_print(return_value, true);
    printf("\n");
  }
}
