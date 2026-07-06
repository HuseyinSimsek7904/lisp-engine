#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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

#define WRONG_NUMBER_ARGS_SYM "wrong-number-of-args-to-builtin"
#define TOO_FEW_ARGS_SYM "too-few-args-to-builtin"
#define TOO_MANY_ARGS_SYM "too-many-args-to-builtin"
#define WRONG_TYPE_ARG_SYM "wrong-type-arg-to-builtin"
#define SYMBOL_REDEFINITION_SYM "symbol-redefinition"
#define COULD_NOT_READ_FILE_SYM "could-not-read-file"
#define COULD_NOT_GEN_TOKENS_SYM "could-not-gen-tokens"

#define RETURN_WRONG_NUMBER_ARGS(m, n, g, c)                                   \
  return ERROR(m, 3, SYM_TO_LISP(n), INT64_TO_LISP(argc), INT64_TO_LISP(c));

#define RETURN_INVALID_ARG(n, v, m)                                            \
  return ERROR(WRONG_TYPE_ARG_SYM, 3, SYM_TO_LISP(n), v, m);

#define REQUIRES_EQ_ARGS(n, c)                                                 \
  do {                                                                         \
    size_t argc = lisp_get_len(args);                                          \
    if (argc != c)                                                             \
      RETURN_WRONG_NUMBER_ARGS(WRONG_NUMBER_ARGS_SYM, n, argc, c);             \
  } while (false)

#define REQUIRES_MIN_ARGS(n, c)                                                \
  do {                                                                         \
    size_t argc = lisp_get_len(args);                                          \
    if (argc < c)                                                              \
      RETURN_WRONG_NUMBER_ARGS(TOO_FEW_ARGS_SYM, n, argc, c);                  \
  } while (false)

#define REQUIRES_MAX_ARGS(n, c)                                                \
  do {                                                                         \
    size_t argc = lisp_get_len(args);                                          \
    if (argc > c)                                                              \
      RETURN_WRONG_NUMBER_ARGS(TOO_MANY_ARGS_SYM, n, argc, c);                 \
  } while (false)

#define CHECK_ARG_TYPE(n, v, t)                                                \
  do {                                                                         \
    if (v->type != t)                                                          \
      RETURN_INVALID_ARG(n, v, TYPE_TO_LISP(t));                               \
  } while (false)

#define ARG0 args->cons.head
#define ARG1 args->cons.tail->cons.head

DEFINE_BUILTIN(set) {
  REQUIRES_EQ_ARGS("set", 2);
  CHECK_ARG_TYPE("set", ARG0, LISP_SYMBOL_T);

  char *symbol = ARG0->symbol;
  LispValue value = evaluate_lisp(ARG1, env);

  env_set(env, symbol, value);

  return value;
}

DEFINE_BUILTIN(setg) {
  REQUIRES_EQ_ARGS("setg", 2);
  CHECK_ARG_TYPE("setg", ARG0, LISP_SYMBOL_T);

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
  REQUIRES_MIN_ARGS("let", 2);

  Environment new_env = {.other = env};
  da_init(&new_env);

  if (ARG0->type == LISP_LIST_T) {
    LispValue remaining_defs = ARG0;

    while (remaining_defs->type == LISP_LIST_T) {
      LispValue def_node = remaining_defs->cons.head;
      remaining_defs = remaining_defs->cons.tail;

      CHECK_ARG_TYPE("let", def_node, LISP_LIST_T);

      if (lisp_get_len(def_node) != 2)
        return ERROR(
            WRONG_TYPE_ARG_SYM, 3, SYM_TO_LISP("let"), def_node,
            STR_TO_LISP(
                "`let` binding list must be a list of key-value pairs"));

      CHECK_ARG_TYPE("let", def_node->cons.head, LISP_SYMBOL_T);

      char *symbol = def_node->cons.head->symbol;

      if (env_find_shallow(&new_env, symbol))
        return ERROR("let", 1, SYM_TO_LISP(symbol));

      // TODO: Maybe the passed environment should be the new environment.
      // That way one variable defined can be used in the other one.
      LispValue value = evaluate_lisp(def_node->cons.tail->cons.head, env);

      env_set(&new_env, symbol, value);
    }

    assert(remaining_defs->type == LISP_NIL_T);

  } else if (ARG0->type != LISP_NIL_T)
    RETURN_INVALID_ARG("let", ARG0, TYPE_TO_LISP(LISP_LIST_T));

  LispValue return_value = evaluate_lisp_body(args->cons.tail, &new_env, false);

  env_free(&new_env);

  return return_value;
}

DEFINE_BUILTIN(if) {
  REQUIRES_MIN_ARGS("if", 2);

  LispValue condition = evaluate_lisp(ARG0, env);

  if (condition->type == LISP_ERROR_T)
    return condition;

  CHECK_ARG_TYPE("if", condition, LISP_BOOL_T);

  if (condition->boolean)
    return ARG1;

  return evaluate_lisp_body(args->cons.tail->cons.tail, env, true);
}

LispValue define_fn_or_macro(LispValue args, Environment *env,
                             char *builtin_name, bool is_macro) {
  REQUIRES_MIN_ARGS(builtin_name, 2);

  bool variadic = false;
  LispValue arg_symbols = LISP_NIL;

  if (ARG0->type == LISP_LIST_T) {
    LispValue *last = &arg_symbols;

    LispValue remaining_args = ARG0;
    LispValue arg;

    while (remaining_args->cons.tail->type == LISP_LIST_T) {
      arg = remaining_args->cons.head;
      remaining_args = remaining_args->cons.tail;

      if (arg->type != LISP_SYMBOL_T)
        RETURN_INVALID_ARG(
            builtin_name, ARG0,
            "argument list must be in the form (SYMBOL... [(rest SYMBOL)])");

      lisp_rappend_use_last(last, arg);
      last = &((*last)->cons.tail);
    }

    assert(remaining_args->cons.tail->type == LISP_NIL_T);
    arg = remaining_args->cons.head;

    if (arg->type == LISP_LIST_T) {
      if (lisp_get_len(arg) != 2)
        RETURN_INVALID_ARG(
            builtin_name, ARG0,
            "argument list must be in the form (SYMBOL... [(rest SYMBOL)])");

      if (arg->cons.head->type != LISP_SYMBOL_T ||
          strcmp(arg->cons.head->string, "rest"))
        RETURN_INVALID_ARG(
            builtin_name, ARG0,
            "argument list must be in the form (SYMBOL... [(rest SYMBOL)])");

      if (arg->cons.tail->cons.head->type != LISP_SYMBOL_T)
        RETURN_INVALID_ARG(
            builtin_name, ARG0,
            "argument list must be in the form (SYMBOL... [(rest SYMBOL)])");

      variadic = true;

      lisp_rappend_use_last(last, arg->cons.tail->cons.head);

    } else if (arg->type == LISP_SYMBOL_T) {
      lisp_rappend_use_last(last, arg);

    } else
      RETURN_INVALID_ARG(
          builtin_name, ARG0,
          "argument list must be in the form (SYMBOL... [(rest SYMBOL)])");

  } else if (ARG0->type != LISP_NIL_T)
    RETURN_INVALID_ARG(
        builtin_name, ARG0,
        "argument list must be in the form (SYMBOL... [(rest SYMBOL)])");

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

DEFINE_BUILTIN(fn) { return define_fn_or_macro(args, env, "fn", false); }

DEFINE_BUILTIN(macro) { return define_fn_or_macro(args, env, "macro", true); }

DEFINE_BUILTIN(do) {
  REQUIRES_MIN_ARGS("do", 1);

  return evaluate_lisp_body(args, env, true);
}

DEFINE_BUILTIN(print) {
  LispValue remaining_args = args;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    CHECK_ARG_TYPE("print", arg, LISP_STRING_T);
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

    CHECK_ARG_TYPE("+", arg, LISP_INT64_T);

    sum += arg->integer;
  }

  return INT64_TO_LISP(sum);
}

// TODO: This builtin should allow any number of arguments except zero.
// (- a) should negate.
// (- x a b c) should return x - a - b - c.
DEFINE_BUILTIN(neg_or_sub) {
  size_t arg_count = lisp_get_len(args);

  REQUIRES_MIN_ARGS("-", 1);
  REQUIRES_MAX_ARGS("-", 2);

  CHECK_ARG_TYPE("-", ARG0, LISP_INT64_T);

  if (arg_count > 1)
    CHECK_ARG_TYPE("-", ARG1, LISP_INT64_T);

  return INT64_TO_LISP(arg_count == 1 ? -ARG0->integer
                                      : ARG0->integer - ARG1->integer);
}

DEFINE_BUILTIN(list) { return args; }

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (= a b c) should check the equality between every item pair.
DEFINE_BUILTIN(equal) {
  REQUIRES_EQ_ARGS("=", 2);

  return BOOL_TO_LISP(lisp_is_ident(ARG0, ARG1));
}

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (< a b c) should compare a < b < c.
DEFINE_BUILTIN(less_than) {
  REQUIRES_EQ_ARGS("<", 2);
  CHECK_ARG_TYPE("<", ARG0, LISP_INT64_T);
  CHECK_ARG_TYPE("<", ARG1, LISP_INT64_T);

  return BOOL_TO_LISP(ARG0->integer < ARG1->integer);
}

DEFINE_BUILTIN(read_file) {
  REQUIRES_EQ_ARGS("read-file", 1);
  CHECK_ARG_TYPE("read-file", ARG0, LISP_STRING_T);

  char *buffer = read_file(ARG0->string);

  if (!buffer)
    return ERROR(COULD_NOT_READ_FILE_SYM, 1, ARG0);

  return STR_TO_LISP(buffer);
}

DEFINE_BUILTIN(parse_lisp) {
  REQUIRES_EQ_ARGS("parse-lisp", 1);
  CHECK_ARG_TYPE("parse-lisp", ARG0, LISP_STRING_T);

  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(ARG0->string, &tokens);

  if (lexer_result)
    return ERROR(COULD_NOT_GEN_TOKENS_SYM, 1, ARG0);

  LispValue parser_result = parse(tokens.array, tokens.size);
  da_free(&tokens);

  return parser_result;
}

DEFINE_BUILTIN(eval) {
  REQUIRES_MIN_ARGS("eval", 1);

  return evaluate_lisp_body(args, env, false);
}

DEFINE_BUILTIN(unpack) {
  LispValue items = LISP_NIL;
  LispValue *last = &items;

  LispValue remaining_args = args;
  while (remaining_args->type == LISP_LIST_T) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    if (arg->type != LISP_LIST_T && arg->type != LISP_NIL_T)
      RETURN_INVALID_ARG("unpack", arg, TYPE_TO_LISP(LISP_LIST_T));

    *last = arg;
    last = lisp_cons_find_term_nil(last);
  }

  return _lisp_init(&(_LispValue){.type = LISP_UNPACKED_T, .item = items});
}

DEFINE_BUILTIN(quote) {
  REQUIRES_EQ_ARGS("quote", 1);

  return ARG0;
}

DEFINE_BUILTIN(head) {
  REQUIRES_EQ_ARGS("head", 1);
  CHECK_ARG_TYPE("head", ARG0, LISP_LIST_T);

  return ARG0->cons.head;
}

DEFINE_BUILTIN(tail) {
  REQUIRES_EQ_ARGS("tail", 1);
  CHECK_ARG_TYPE("tail", ARG0, LISP_LIST_T);

  return ARG0->cons.tail;
}

DEFINE_BUILTIN(typeof) {
  REQUIRES_EQ_ARGS("typeof", 1);

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

    if (!lisp) {
      interactive = false;
      continue;
    }

    add_history(lisp);

    LispValue return_value = repl(lisp, &global_env);

    free(lisp);

    lisp_print(return_value, true);
    printf("\n");
  }
}
