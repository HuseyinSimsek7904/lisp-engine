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
#include <string.h>

#include "core.h"
#include "debug.h"
#include "evaluation.h"
#include "format.h"
#include "lexer.h"
#include "lisp_value.h"

LispValue lisp_builtin_print;
LispValue lisp_builtin_sum;
LispValue lisp_builtin_neg_or_sub;
LispValue lisp_builtin_list;
LispValue lisp_builtin_equal;
LispValue lisp_builtin_less_than;
LispValue lisp_builtin_read_file;
LispValue lisp_builtin_parse_lisp;
LispValue lisp_builtin_eval;
LispValue lisp_builtin_unpack;
LispValue lisp_builtin_head;
LispValue lisp_builtin_tail;
LispValue lisp_builtin_typeof;
LispValue lisp_builtin_error;
LispValue lisp_builtin_set;
LispValue lisp_builtin_setg;
LispValue lisp_builtin_quote;
LispValue lisp_builtin_fn;
LispValue lisp_builtin_macro;
LispValue lisp_builtin_let;
LispValue lisp_builtin_if;
LispValue lisp_builtin_do;
LispValue lisp_builtin_format;
LispValue lisp_builtin_cons;
LispValue lisp_builtin_read_line;
LispValue lisp_builtin_debug_set_flags;
LispValue lisp_builtin_debug_list_env;

#define DEFINE_BUILTIN(n)                                                      \
  LispValue builtin_##n(LispValue args, Environment *env)

#define RETURN_WRONG_NUMBER_ARGS(m, n, g, c)                                   \
  return ERROR(m, 3, lisp_builtin_##n, lisp_make_int64(argc),                  \
               lisp_make_int64(c));

#define RETURN_INVALID_ARG(n, v, m)                                            \
  return ERROR(WRONG_TYPE_ARG_SYM, 3, n, v, m);

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
    LispValue value = (v);                                                     \
    if (!lisp_is_type(value, t))                                               \
      RETURN_INVALID_ARG(lisp_builtin_##n, value, lisp_make_type(t));          \
  } while (false)

#define ARG0 lisp_head(args)
#define ARG1 lisp_head(lisp_tail(args))

#define DEFINE_BUILTIN_FUNCTION(n, c, m, t)                                    \
  lisp_builtin_##c = lisp_make_builtin(m, t, builtin_##c, n);                  \
  env_set_shallow(&global_env, n, lisp_builtin_##c);

#define DEFINE_TYPE_REP(n, c, r) env_set(&global_env, n, lisp_make_type(r));

DEFINE_BUILTIN(set) {
  REQUIRES_EQ_ARGS(set, 2);
  CHECK_ARG_TYPE(set, ARG0, LISP_SYMBOL_T);

  char *symbol = lisp_get_sym(ARG0);
  LispValue value = evaluate_lisp(ARG1, env);

  env_set(env, symbol, value);

  return value;
}

DEFINE_BUILTIN(setg) {
  REQUIRES_EQ_ARGS(setg, 2);
  CHECK_ARG_TYPE(setg, ARG0, LISP_SYMBOL_T);

  char *symbol = lisp_get_sym(ARG0);
  LispValue value = evaluate_lisp(ARG1, env);

  Environment *global = env;
  while (global->other)
    global = global->other;

  env_set_shallow(global, symbol, value);

  return value;
}

// TODO: `let` should be able to do tail calls, but tail calls cannot use new
// environments currently.
DEFINE_BUILTIN(let) {
  REQUIRES_MIN_ARGS(let, 2);

  Environment new_env = {.other = env};
  da_init(&new_env);

  if (lisp_is_non_empty_list(ARG0)) {
    LispIter defs_iter;
    lisp_iter_init(&defs_iter, ARG0);

    while (lisp_iter_has_next(&defs_iter)) {
      LispValue def_node = lisp_iter_next(&defs_iter);

      CHECK_ARG_TYPE(let, def_node, LISP_LIST_T);

      if (lisp_get_len(def_node) != 2)
        return ERROR(
            WRONG_TYPE_ARG_SYM, 3, lisp_builtin_let, def_node,
            lisp_make_cstr(
                "`let` binding list must be a list of key-value pairs"));

      CHECK_ARG_TYPE(let, lisp_head(def_node), LISP_SYMBOL_T);

      char *symbol = lisp_get_sym(lisp_head(def_node));

      if (env_find_shallow(&new_env, symbol))
        return ERROR(REDEF_SYMBOL_SYM, 1, lisp_head(def_node));

      // TODO: Maybe the passed environment should be the new environment.
      // That way one variable defined can be used in the other one.
      LispValue value = evaluate_lisp(lisp_head(lisp_tail(def_node)), env);

      env_set_shallow(&new_env, symbol, value);
    }

  } else if (!lisp_is_nil(ARG0))
    RETURN_INVALID_ARG(lisp_builtin_let, ARG0, lisp_make_type(LISP_LIST_T));

  LispValue return_value = evaluate_lisp_body(lisp_tail(args), &new_env, false);

  env_free(&new_env);

  return return_value;
}

DEFINE_BUILTIN(if) {
  REQUIRES_MIN_ARGS(if, 2);

  LispValue condition = evaluate_lisp(ARG0, env);

  if (lisp_is_type(condition, LISP_ERROR_T))
    return condition;

  CHECK_ARG_TYPE(if, condition, LISP_BOOL_T);

  if (lisp_get_bool(condition))
    return ARG1;

  return evaluate_lisp_body(lisp_tail(lisp_tail(args)), env, true);
}

LispValue define_fn_or_macro(LispValue args, Environment *env,
                             LispValue builtin, bool is_macro) {
  size_t argc = lisp_get_len(args);
  if (argc < 2)
    return lisp_make_error(lisp_make_list(4, lisp_make_sym(TOO_FEW_ARGS_SYM),
                                          builtin, lisp_make_int64(argc),
                                          lisp_make_int64(2)));

  bool variadic = false;
  LispValue arg_symbols;

  if (lisp_is_nil(ARG0)) {
    arg_symbols = LISP_NIL;

  } else if (lisp_is_non_empty_list(ARG0)) {
    LispGen args_gen;
    lisp_gen_init(&args_gen);

    LispIter args_iter;
    lisp_iter_init(&args_iter, ARG0);

    LispValue arg;

    while (lisp_iter_has_next_next(&args_iter)) {
      arg = lisp_iter_next(&args_iter);

      if (!lisp_is_type(arg, LISP_SYMBOL_T))
        return lisp_make_error(lisp_make_list(
            4, lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      lisp_gen_appendr(&args_gen, arg);
    }

    arg = lisp_iter_next(&args_iter);

    if (lisp_is_non_empty_list(arg)) {
      if (lisp_get_len(arg) != 2)
        return lisp_make_error(lisp_make_list(
            4, lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      if (!lisp_is_sym(lisp_head(arg), "rest"))
        return lisp_make_error(lisp_make_list(
            4, lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      if (!lisp_is_type(lisp_head(lisp_tail(arg)), LISP_SYMBOL_T))
        return lisp_make_error(lisp_make_list(
            4, lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      variadic = true;

      lisp_gen_appendr(&args_gen, lisp_head(lisp_tail(arg)));

    } else if (lisp_is_type(arg, LISP_SYMBOL_T)) {
      lisp_gen_appendr(&args_gen, arg);

    } else
      return lisp_make_error(lisp_make_list(
          4, lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG0,
          lisp_make_cstr(
              "argument list must be in the form ([SYMBOL...] [(rest "
              "SYMBOL)])")));

    arg_symbols = lisp_gen_end(&args_gen);

  } else
    return lisp_make_error(lisp_make_list(4, lisp_make_sym(WRONG_TYPE_ARG_SYM),
                                          builtin, ARG0,
                                          lisp_make_type(LISP_LIST_T)));

  return lisp_make_fn_or_macro(arg_symbols, lisp_tail(args), variadic,
                               is_macro);
}

DEFINE_BUILTIN(fn) {
  return define_fn_or_macro(args, env, lisp_builtin_fn, false);
}

DEFINE_BUILTIN(macro) {
  return define_fn_or_macro(args, env, lisp_builtin_macro, true);
}

DEFINE_BUILTIN(do) {
  REQUIRES_MIN_ARGS(do, 1);

  return evaluate_lisp_body(args, env, true);
}

DEFINE_BUILTIN(print) {
  LispIter args_iter;
  lisp_iter_init(&args_iter, args);

  while (lisp_iter_has_next(&args_iter)) {
    CHECK_ARG_TYPE(print, lisp_iter_next(&args_iter), LISP_STRING_T);
  }

  lisp_iter_init(&args_iter, args);
  while (lisp_iter_has_next(&args_iter)) {
    lisp_printf("%s\n", lisp_get_str(lisp_iter_next(&args_iter)));
  }

  return LISP_NIL;
}

DEFINE_BUILTIN(sum) {
  int64_t sum = 0;

  LispIter args_iter;
  lisp_iter_init(&args_iter, args);
  while (lisp_iter_has_next(&args_iter)) {
    LispValue arg = lisp_iter_next(&args_iter);

    CHECK_ARG_TYPE(sum, arg, LISP_INT64_T);

    sum += lisp_get_int64(arg);
  }

  return lisp_make_int64(sum);
}

// TODO: This builtin should allow any number of arguments except zero.
// (- a) should negate.
// (- x a b c) should return x - a - b - c.
DEFINE_BUILTIN(neg_or_sub) {
  size_t arg_count = lisp_get_len(args);

  REQUIRES_MIN_ARGS(neg_or_sub, 1);
  REQUIRES_MAX_ARGS(neg_or_sub, 2);
  CHECK_ARG_TYPE(neg_or_sub, ARG0, LISP_INT64_T);

  if (arg_count > 1)
    CHECK_ARG_TYPE(neg_or_sub, ARG1, LISP_INT64_T);

  return lisp_make_int64(arg_count == 1
                             ? -lisp_get_int64(ARG0)
                             : lisp_get_int64(ARG0) - lisp_get_int64(ARG1));
}

DEFINE_BUILTIN(list) { return args; }

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (= a b c) should check the equality between every item pair.
DEFINE_BUILTIN(equal) {
  REQUIRES_EQ_ARGS(equal, 2);

  return lisp_make_bool(lisp_is_ident(ARG0, ARG1));
}

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (< a b c) should compare a < b < c.
DEFINE_BUILTIN(less_than) {
  REQUIRES_EQ_ARGS(less_than, 2);
  CHECK_ARG_TYPE(less_than, ARG0, LISP_INT64_T);
  CHECK_ARG_TYPE(less_than, ARG1, LISP_INT64_T);

  return lisp_make_bool(lisp_get_int64(ARG0) < lisp_get_int64(ARG1));
}

DEFINE_BUILTIN(read_file) {
  REQUIRES_EQ_ARGS(read_file, 1);
  CHECK_ARG_TYPE(read_file, ARG0, LISP_STRING_T);

  char *file_name = sv_copy_cstr(lisp_get_str(ARG0));
  StringView buffer = read_file_sv(file_name);

  free(file_name);

  if (!buffer.array)
    RETURN_COULD_NOT_READ_FILE(ARG0);

  return lisp_make_str(buffer);
}

DEFINE_BUILTIN(parse_lisp) {
  REQUIRES_EQ_ARGS(parse_lisp, 1);
  CHECK_ARG_TYPE(parse_lisp, ARG0, LISP_STRING_T);

  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(lisp_get_str(ARG0), &tokens);

  if (lexer_result)
    RETURN_COULD_NOT_GEN_TOKENS(lexer_result);

  LispValue parser_result = parse(tokens.array, tokens.size);
  da_free(&tokens);

  return parser_result;
}

DEFINE_BUILTIN(eval) {
  REQUIRES_MIN_ARGS(eval, 1);

  return evaluate_lisp_body(args, env, false);
}

DEFINE_BUILTIN(unpack) {
  LispGen items_gen;
  lisp_gen_init(&items_gen);

  LispIter args_iter;
  lisp_iter_init(&args_iter, args);

  while (lisp_iter_has_next(&args_iter)) {
    LispValue arg = lisp_iter_next(&args_iter);

    if (!lisp_is_list(arg))
      RETURN_INVALID_ARG(lisp_builtin_unpack, arg, lisp_make_type(LISP_LIST_T));

    lisp_gen_extendr(&items_gen, arg);
  }

  return lisp_make_unpacked(lisp_gen_end(&items_gen));
}

DEFINE_BUILTIN(quote) {
  REQUIRES_EQ_ARGS(quote, 1);

  return ARG0;
}

DEFINE_BUILTIN(head) {
  REQUIRES_EQ_ARGS(head, 1);
  CHECK_ARG_TYPE(head, ARG0, LISP_LIST_T);

  return lisp_head(ARG0);
}

DEFINE_BUILTIN(tail) {
  REQUIRES_EQ_ARGS(tail, 1);
  CHECK_ARG_TYPE(tail, ARG0, LISP_LIST_T);

  return lisp_tail(ARG0);
}

DEFINE_BUILTIN(typeof) {
  REQUIRES_EQ_ARGS(typeof, 1);

  return lisp_make_type(lisp_get_type(ARG0));
}

DEFINE_BUILTIN(error) {
  REQUIRES_EQ_ARGS(error, 1);

  return lisp_make_error(ARG0);
}

DEFINE_BUILTIN(format) {
  REQUIRES_MIN_ARGS(format, 1);
  CHECK_ARG_TYPE(format, ARG0, LISP_STRING_T);

  StringBuilder builder;
  da_init(&builder);

  StringView format_str = lisp_get_str(ARG0);

  LispIter args_iter;
  lisp_iter_init(&args_iter, lisp_tail(args));

  while (format_str.size) {
    // TODO: Rather than calling `da_append` many times for each char, calling
    // `da_append_many` would be faster. The string should be searched for the
    // next % sign and appended at once until that character.
    char c = sv_pop_left(&format_str);
    if (c != '%') {
      da_append(&builder, c);
      continue;
    }

    switch (c = sv_pop_left(&format_str)) {
    case '%':
      da_append(&builder, c);
      break;

    case 'l': {
      if (!lisp_iter_has_next(&args_iter)) {
        da_free(&builder);
        return ERROR(TOO_FEW_ARGS_FORMAT_SYM, 1, ARG0);
      }

      lisp_value_format(&builder, lisp_iter_next(&args_iter), FORMAT_BASIC);
    } break;

    case 'L': {
      if (!lisp_iter_has_next(&args_iter)) {
        da_free(&builder);
        return ERROR(TOO_FEW_ARGS_FORMAT_SYM, 1, ARG0);
      }

      lisp_value_format(&builder, lisp_iter_next(&args_iter), FORMAT_PRODUCER);
    } break;

    case 's': {
      if (!lisp_iter_has_next(&args_iter)) {
        da_free(&builder);
        return ERROR(TOO_FEW_ARGS_FORMAT_SYM, 1, ARG0);
      }

      LispValue arg = lisp_iter_next(&args_iter);

      if (!lisp_is_type(arg, LISP_STRING_T)) {
        da_free(&builder);
        return ERROR(WRONG_TYPE_ARG_FORMAT_SYM, 2, arg,
                     lisp_make_type(LISP_STRING_T));
      }

      sb_append_view(&builder, lisp_get_str(arg));
    } break;

    default:
      da_free(&builder);
      return ERROR(INVALID_FORMAT_SYM, 1, ARG0);
    }
  }

  return lisp_make_str(sb_to_sv(&builder));
}

DEFINE_BUILTIN(cons) {
  REQUIRES_EQ_ARGS(cons, 2);

  if (!lisp_is_nil(ARG1))
    CHECK_ARG_TYPE(cons, ARG1, LISP_LIST_T);

  return lisp_make_cons(ARG0, ARG1);
}

DEFINE_BUILTIN(read_line) {
  REQUIRES_EQ_ARGS(read_line, 1);
  CHECK_ARG_TYPE(read_line, ARG0, LISP_STRING_T);

  char *prompt = sv_copy_cstr(lisp_get_str(ARG0));
  char *read = readline(prompt);
  free(prompt);

  StringView view = {.array = read, .size = strlen(read)};
  return lisp_make_str(view);
}

// TODO: This should expect a list of symbols, where each symbol is a flag.
// For now, this is satisfactory.
DEFINE_BUILTIN(debug_set_flags) {
  REQUIRES_EQ_ARGS(debug_set_flags, 1);
  CHECK_ARG_TYPE(debug_set_flags, ARG0, LISP_INT64_T);

  debug_flags = lisp_get_int64(ARG0);

  return LISP_NIL;
}

DEFINE_BUILTIN(debug_list_env) {
  REQUIRES_EQ_ARGS(debug_list_env, 0);

  debug_list_env(stderr, env);

  return LISP_NIL;
}

LispValue LISP_NIL;
LispValue LISP_TRUE;
LispValue LISP_FALSE;

int main(int argc, char **argv) {
  bool interactive = true;
  bool test_mode = false;
  bool always_show_test_results = false;

  int opt;

  while ((opt = getopt(argc, argv, "nTtd:")) != -1) {
    switch (opt) {
    case 'n':
      interactive = false;
      break;

    case 't':
      test_mode = true;
      interactive = false;
      break;

    case 'T':
      test_mode = true;
      interactive = false;
      always_show_test_results = true;
      break;

    case 'd':
      debug_flags = atoi(optarg);
      break;

    default:
      break;
    }
  }

  Environment global_env = {.other = NULL};
  da_init(&global_env);

  // Singletons.
  LISP_NIL = lisp_make_singleton_nil();
  LISP_TRUE = lisp_make_singleton_bool(true);
  LISP_FALSE = lisp_make_singleton_bool(false);

  env_set_shallow(&global_env, "nil", LISP_NIL);
  env_set_shallow(&global_env, "false", LISP_FALSE);
  env_set_shallow(&global_env, "true", LISP_TRUE);

  // Type representative singletons.
  DEFINE_TYPE_REP("nil-t", nil, LISP_NIL_T);
  DEFINE_TYPE_REP("type-t", type, LISP_TYPE_T);
  DEFINE_TYPE_REP("error-t", error, LISP_ERROR_T);
  DEFINE_TYPE_REP("str-t", string, LISP_STRING_T);
  DEFINE_TYPE_REP("int64-t", int64, LISP_INT64_T);
  DEFINE_TYPE_REP("bool-t", bool, LISP_BOOL_T);
  DEFINE_TYPE_REP("sym-t", symbol, LISP_SYMBOL_T);
  DEFINE_TYPE_REP("builtin-t", builtin_func, LISP_BUILTIN_FUNC_T);
  DEFINE_TYPE_REP("fn-t", func, LISP_FUNC_T);
  DEFINE_TYPE_REP("list-t", list, LISP_LIST_T);

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
  DEFINE_BUILTIN_FUNCTION("error", error, false, false);
  DEFINE_BUILTIN_FUNCTION("format", format, false, false);
  DEFINE_BUILTIN_FUNCTION("cons", cons, false, false);
  DEFINE_BUILTIN_FUNCTION("read-line", read_line, false, false);

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

  // Debug builtins.
  DEFINE_BUILTIN_FUNCTION("debug:set-flags", debug_set_flags, true, true);
  DEFINE_BUILTIN_FUNCTION("debug:list-env", debug_list_env, true, false);

  // Builtin startup files.
  char *startup[] = {"core/core.l"};

  for (size_t i = 0; i < sizeof(startup) / sizeof(startup[0]); i++) {
    StringView buffer = read_file_sv(startup[i]);

    if (!buffer.array) {
      lisp_fprintf(stderr, "warning: could not read contents of %s\n",
                   startup[i]);
      continue;
    }

    repl(buffer, &global_env);

    free(buffer.array);
  }

  // Custom passed startup files.
  bool test_failed = false;
  for (size_t i = optind; i < argc; i++) {
    StringView buffer = read_file_sv(argv[i]);

    if (!buffer.array) {
      lisp_fprintf(stderr, "warning: could not read contents of %s\n", argv[i]);
      continue;
    }

    if (always_show_test_results)
      lisp_fprintf(stderr, "info: testing %s\n", argv[i]);

    LispValue return_value = repl(buffer, &global_env);

    free(buffer.array);

    if (!test_mode)
      lisp_printf("%s: ~ %l\n", argv[i], return_value);

    else {
      // In test mode, every file must return nil.
      // If they do not, that means that the test failed.

      if (lisp_is_nil(return_value)) {
        if (always_show_test_results)
          lisp_printf("info: %s test passed\n", argv[i]);
      } else {
        lisp_fprintf(stderr, "fail: %s test failed\n", argv[i]);
        lisp_fprintf(stderr, "%l\n", return_value);
        test_failed = true;
      }
    }
  }

  if (test_failed)
    exit(1);
  else if (test_mode)
    lisp_fprintf(stderr, "test: passed all tests\n");

  // Interactive mode.
  while (interactive) {
    char *lisp = readline("> ");

    if (!lisp) {
      interactive = false;
      continue;
    }

    StringView view = {.array = lisp, .size = strlen(lisp)};

    add_history(lisp);

    LispValue return_value = repl(view, &global_env);

    free(lisp);

    lisp_printf("~ %l\n", return_value);
  }
}
