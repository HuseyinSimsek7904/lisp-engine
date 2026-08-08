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
#include "parser.h"

#define DEFINE_BUILTIN(n)                                                      \
  LispValue lisp_builtin_##n;                                                  \
  LispValue builtin_##n(LispValue args, Environment *env)

#define RETURN_WRONG_NUMBER_ARGS(m, n, g, c)                                   \
  return ERROR3(m, lisp_builtin_##n, lisp_make_int64(argc), lisp_make_int64(c));

#define RETURN_INVALID_ARG(n, v, m) return ERROR3(WRONG_TYPE_ARG_SYM, n, v, m);

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

#define ARG(n) lisp_head_n(args, n)

#define DEFINE_BUILTIN_FUNCTION(n, c, m, t)                                    \
  lisp_builtin_##c = lisp_make_builtin(m, t, builtin_##c, n);                  \
  env_set_shallow(global_env, n, lisp_builtin_##c);

#define DEFINE_TYPE_REP(n, c, r) env_set(global_env, n, lisp_make_type(r));

DEFINE_BUILTIN(set) {
  REQUIRES_EQ_ARGS(set, 2);
  CHECK_ARG_TYPE(set, ARG(0), LISP_SYMBOL_T);

  const char *symbol = lisp_get_sym(ARG(0));
  LispValue value = evaluate_lisp(ARG(1), env);

  env_set(env, symbol, value);

  return value;
}

// TODO: `let` should be able to do tail calls, but tail calls cannot use new
// environments currently.
DEFINE_BUILTIN(let) {
  REQUIRES_MIN_ARGS(let, 2);

  Environment *new_env = env_make(env);

  CHECK_ARG_TYPE(let, ARG(0), LISP_LIST_T);

  if (!lisp_is_empty(ARG(0))) {
    LispIter defs_iter;
    lisp_iter_init(&defs_iter, ARG(0));

    while (lisp_iter_has_next(&defs_iter)) {
      LispValue def_node = lisp_iter_next(&defs_iter);

      CHECK_ARG_TYPE(let, def_node, LISP_LIST_T);

      if (lisp_get_len(def_node) != 2)
        return ERROR3(
            WRONG_TYPE_ARG_SYM, lisp_builtin_let, def_node,
            lisp_make_cstr(
                "`let` binding list must be a list of key-value pairs"));

      LispValue symbol_lisp = lisp_head(def_node);
      CHECK_ARG_TYPE(let, symbol_lisp, LISP_SYMBOL_T);

      const char *symbol = lisp_get_sym(symbol_lisp);

      if (env_find_shallow(new_env, symbol))
        return ERROR1(REDEF_SYMBOL_SYM, symbol_lisp);

      // TODO: Maybe the passed environment should be the new environment.
      // That way one variable defined can be used in the other one.
      LispValue value = evaluate_lisp(lisp_head_n(def_node, 1), env);

      env_set_shallow(new_env, symbol, value);
    }
  }

  return evaluate_lisp_body(lisp_tail(args), new_env, false);
}

DEFINE_BUILTIN(if) {
  REQUIRES_MIN_ARGS(if, 2);

  LispValue condition = evaluate_lisp(ARG(0), env);

  if (lisp_is_type(condition, LISP_ERROR_T))
    return condition;

  CHECK_ARG_TYPE(if, condition, LISP_BOOL_T);

  if (lisp_get_bool(condition))
    return ARG(1);

  return evaluate_lisp_body(lisp_tail_n(args, 1), env, true);
}

LispValue define_fn_or_macro(LispValue args, Environment *env,
                             LispValue builtin, bool is_macro) {
  size_t argc = lisp_get_len(args);
  if (argc < 2)
    return lisp_make_error(lisp_make_list_4(lisp_make_sym(TOO_FEW_ARGS_SYM),
                                            builtin, lisp_make_int64(argc),
                                            lisp_make_int64(2)));

  bool variadic = false;
  LispValue arg_symbols;

  if (!lisp_is_type(ARG(0), LISP_LIST_T))
    return lisp_make_error(lisp_make_list_4(lisp_make_sym(WRONG_TYPE_ARG_SYM),
                                            builtin, ARG(0),
                                            lisp_make_type(LISP_LIST_T)));

  if (lisp_is_empty(ARG(0))) {
    arg_symbols = LISP_EMPTY;

  } else {
    LispGen args_gen;
    lisp_gen_init(&args_gen);

    LispIter args_iter;
    lisp_iter_init(&args_iter, ARG(0));

    LispValue arg;

    while (lisp_iter_has_next_next(&args_iter)) {
      arg = lisp_iter_next(&args_iter);

      if (!lisp_is_type(arg, LISP_SYMBOL_T))
        return lisp_make_error(lisp_make_list_4(
            lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG(0),
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      lisp_gen_appendr(&args_gen, arg);
    }

    arg = lisp_iter_next(&args_iter);

    if (lisp_is_type(arg, LISP_LIST_T)) {
      if (lisp_get_len(arg) != 2)
        return lisp_make_error(lisp_make_list_4(
            lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG(0),
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      if (!lisp_is_sym(lisp_head(arg), "rest"))
        return lisp_make_error(lisp_make_list_4(
            lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG(0),
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      LispValue rest_arg_symbol = lisp_head_n(arg, 1);

      if (!lisp_is_type(rest_arg_symbol, LISP_SYMBOL_T))
        return lisp_make_error(lisp_make_list_4(
            lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG(0),
            lisp_make_cstr(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      variadic = true;

      lisp_gen_appendr(&args_gen, rest_arg_symbol);

    } else if (lisp_is_type(arg, LISP_SYMBOL_T)) {
      lisp_gen_appendr(&args_gen, arg);

    } else
      return lisp_make_error(lisp_make_list_4(
          lisp_make_sym(WRONG_TYPE_ARG_SYM), builtin, ARG(0),
          lisp_make_cstr(
              "argument list must be in the form ([SYMBOL...] [(rest "
              "SYMBOL)])")));

    arg_symbols = lisp_gen_end(&args_gen);
  }

  return lisp_make_fn_or_macro(arg_symbols, lisp_tail(args), variadic, is_macro,
                               env);
}

DEFINE_BUILTIN(fn) {
  return define_fn_or_macro(args, env, lisp_builtin_fn, false);
}

DEFINE_BUILTIN(macro) {
  return define_fn_or_macro(args, env, lisp_builtin_macro, true);
}

DEFINE_BUILTIN(do) {
  if (lisp_is_empty(args))
    return LISP_NIL;

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
  CHECK_ARG_TYPE(neg_or_sub, ARG(0), LISP_INT64_T);

  if (arg_count > 1)
    CHECK_ARG_TYPE(neg_or_sub, ARG(1), LISP_INT64_T);

  return lisp_make_int64(arg_count == 1
                             ? -lisp_get_int64(ARG(0))
                             : lisp_get_int64(ARG(0)) - lisp_get_int64(ARG(1)));
}

DEFINE_BUILTIN(list) { return args; }

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (= a b c) should check the equality between every item pair.
DEFINE_BUILTIN(equal) {
  REQUIRES_EQ_ARGS(equal, 2);

  return lisp_make_bool(lisp_is_ident(ARG(0), ARG(1)));
}

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (< a b c) should compare a < b < c.
DEFINE_BUILTIN(less_than) {
  REQUIRES_EQ_ARGS(less_than, 2);
  CHECK_ARG_TYPE(less_than, ARG(0), LISP_INT64_T);
  CHECK_ARG_TYPE(less_than, ARG(1), LISP_INT64_T);

  return lisp_make_bool(lisp_get_int64(ARG(0)) < lisp_get_int64(ARG(1)));
}

DEFINE_BUILTIN(read_file) {
  REQUIRES_EQ_ARGS(read_file, 1);
  CHECK_ARG_TYPE(read_file, ARG(0), LISP_STRING_T);

  char *file_name = sv_copy_cstr(lisp_get_str(ARG(0)));
  StringView buffer = read_file_sv(file_name);

  free(file_name);

  if (!buffer.array)
    RETURN_COULD_NOT_READ_FILE(ARG(0));

  return lisp_make_str(buffer);
}

DEFINE_BUILTIN(parse_lisp) {
  REQUIRES_EQ_ARGS(parse_lisp, 1);
  CHECK_ARG_TYPE(parse_lisp, ARG(0), LISP_STRING_T);

  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(lisp_get_str(ARG(0)), &tokens);

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

DEFINE_BUILTIN(quote) {
  REQUIRES_EQ_ARGS(quote, 1);

  return ARG(0);
}

DEFINE_BUILTIN(head) {
  size_t arg_count = lisp_get_len(args);

  REQUIRES_MIN_ARGS(head, 1);
  REQUIRES_MAX_ARGS(head, 2);

  CHECK_ARG_TYPE(head, ARG(0), LISP_LIST_T);

  size_t n = 0;
  if (arg_count == 2) {
    CHECK_ARG_TYPE(head, ARG(1), LISP_INT64_T);
    n = lisp_get_int64(ARG(1));
  }

  if (lisp_get_len(ARG(0)) < n + 1)
    RETURN_INVALID_ARG(lisp_builtin_head, ARG(0), lisp_make_int64(n));

  return lisp_head_n(ARG(0), n);
}

DEFINE_BUILTIN(tail) {
  size_t arg_count = lisp_get_len(args);

  REQUIRES_MIN_ARGS(tail, 1);
  REQUIRES_MAX_ARGS(tail, 2);

  CHECK_ARG_TYPE(tail, ARG(0), LISP_LIST_T);

  size_t n = 0;
  if (arg_count == 2) {
    CHECK_ARG_TYPE(tail, ARG(1), LISP_INT64_T);
    n = lisp_get_int64(ARG(1));
  }

  if (lisp_get_len(ARG(0)) < n + 1)
    RETURN_INVALID_ARG(lisp_builtin_tail, ARG(0), lisp_make_int64(n));

  return lisp_tail_n(ARG(0), n);
}

DEFINE_BUILTIN(typeof) {
  REQUIRES_EQ_ARGS(typeof, 1);

  return lisp_make_type(lisp_get_type(ARG(0)));
}

DEFINE_BUILTIN(error) {
  REQUIRES_EQ_ARGS(error, 1);

  return lisp_make_error(ARG(0));
}

DEFINE_BUILTIN(format) {
  REQUIRES_MIN_ARGS(format, 1);
  CHECK_ARG_TYPE(format, ARG(0), LISP_STRING_T);

  StringBuilder builder;
  da_init(&builder);

  StringView format_str = lisp_get_str(ARG(0));

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
        return ERROR1(TOO_FEW_ARGS_FORMAT_SYM, ARG(0));
      }

      lisp_value_format(&builder, lisp_iter_next(&args_iter), FORMAT_BASIC);
    } break;

    case 'L': {
      if (!lisp_iter_has_next(&args_iter)) {
        da_free(&builder);
        return ERROR1(TOO_FEW_ARGS_FORMAT_SYM, ARG(0));
      }

      lisp_value_format(&builder, lisp_iter_next(&args_iter), FORMAT_PRODUCER);
    } break;

    case 's': {
      if (!lisp_iter_has_next(&args_iter)) {
        da_free(&builder);
        return ERROR1(TOO_FEW_ARGS_FORMAT_SYM, ARG(0));
      }

      LispValue arg = lisp_iter_next(&args_iter);

      if (!lisp_is_type(arg, LISP_STRING_T)) {
        da_free(&builder);
        return ERROR2(WRONG_TYPE_ARG_FORMAT_SYM, arg,
                      lisp_make_type(LISP_STRING_T));
      }

      sb_append_view(&builder, lisp_get_str(arg));
    } break;

    default:
      da_free(&builder);
      return ERROR1(INVALID_FORMAT_SYM, ARG(0));
    }
  }

  return lisp_make_str(sb_to_sv(&builder));
}

DEFINE_BUILTIN(cons) {
  REQUIRES_EQ_ARGS(cons, 2);

  CHECK_ARG_TYPE(cons, ARG(1), LISP_LIST_T);

  return lisp_cons(ARG(0), ARG(1));
}

DEFINE_BUILTIN(read_line) {
  REQUIRES_EQ_ARGS(read_line, 1);
  CHECK_ARG_TYPE(read_line, ARG(0), LISP_STRING_T);

  char *prompt = sv_copy_cstr(lisp_get_str(ARG(0)));
  char *read = readline(prompt);
  free(prompt);

  StringView view = {.array = read, .size = strlen(read)};
  return lisp_make_str(view);
}

LispValue apply_quasi_quote(LispValue value, Environment *env) {
  if (!lisp_is_type(value, LISP_LIST_T))
    return value;

  if (lisp_get_len(value) == 2 && lisp_is_sym(lisp_head(value), "unquote"))
    return evaluate_lisp(lisp_head_n(value, 1), env);

  LispIter iter;
  lisp_iter_init(&iter, value);

  LispGen gen;
  lisp_gen_init(&gen);

  while (lisp_iter_has_next(&iter)) {
    LispValue item = lisp_iter_next(&iter);

    if (lisp_is_type(item, LISP_LIST_T) && lisp_get_len(item) == 2 &&
        lisp_is_sym(lisp_head(item), "unquote-expand")) {
      LispValue result = evaluate_lisp(lisp_head_n(item, 1), env);

      if (!lisp_is_type(result, LISP_LIST_T))
        return ERROR1("result-not-list", lisp_head_n(item, 1));

      lisp_gen_extendr(&gen, result);
    } else
      lisp_gen_appendr(&gen, apply_quasi_quote(item, env));
  }

  return lisp_gen_end(&gen);
}

DEFINE_BUILTIN(quasi_quote) {
  REQUIRES_EQ_ARGS(quasi_quote, 1);

  return apply_quasi_quote(ARG(0), env);
}

// TODO: This should expect a list of symbols, where each symbol is a flag.
// For now, this is satisfactory.
DEFINE_BUILTIN(debug_set_flags) {
  REQUIRES_EQ_ARGS(debug_set_flags, 1);
  CHECK_ARG_TYPE(debug_set_flags, ARG(0), LISP_INT64_T);

  debug_flags = lisp_get_int64(ARG(0));

  return LISP_NIL;
}

DEFINE_BUILTIN(debug_list_env) {
  REQUIRES_EQ_ARGS(debug_list_env, 0);

  debug_list_env(stderr, env);

  return LISP_NIL;
}

const char *history_path = "./.history";

LispValue interactive_repl(Environment *env) {
  char *full_lisp = readline("> ");

  if (!full_lisp) {
    return NULL;
  }

  while (true) {
    StringView view = {.array = full_lisp, .size = strlen(full_lisp)};

    Tokens tokens;
    da_init(&tokens);

    LexerResult lexer_result = generate_tokens(view, &tokens);

    if (lexer_result) {
      da_free(&tokens);

      if (lexer_result != LEXER_UNTERMINATED_STRING) {
        add_history(full_lisp);
        write_history(history_path);
        free(full_lisp);
        RETURN_COULD_NOT_GEN_TOKENS(lexer_result);
      }

    } else {
      LispValue parser_result = parse(tokens.array, tokens.size);
      da_free(&tokens);

      if (!lisp_is_type(parser_result, LISP_ERROR_T)) {
        add_history(full_lisp);
        write_history(history_path);
        free(full_lisp);

        return evaluate_lisp_body(parser_result, env, false);
      }

      if (!lisp_is_sym(lisp_head(lisp_get_error_item(parser_result)),
                       PARSER_TOO_FEW_R_PAREN)) {
        add_history(full_lisp);
        write_history(history_path);
        free(full_lisp);

        return parser_result;
      }
    }

    char *new_line = readline(". ");

    if (!new_line) {
      free(full_lisp);
      return NULL;
    }

    full_lisp = realloc(full_lisp, strlen(full_lisp) + strlen(new_line) + 2);
    assert(full_lisp);
    strcat(full_lisp, "\n");
    strcat(full_lisp, new_line);
  }
}

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

  Environment *global_env = env_make(NULL);

  // Singletons.
  lisp_init_singletons();

  env_set_shallow(global_env, "nil", LISP_NIL);
  env_set_shallow(global_env, "false", LISP_FALSE);
  env_set_shallow(global_env, "true", LISP_TRUE);
  env_set_shallow(global_env, "empty", LISP_EMPTY);

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
  DEFINE_BUILTIN_FUNCTION("head", head, false, false);
  DEFINE_BUILTIN_FUNCTION("tail", tail, false, false);
  DEFINE_BUILTIN_FUNCTION("typeof", typeof, false, false);
  DEFINE_BUILTIN_FUNCTION("error", error, false, false);
  DEFINE_BUILTIN_FUNCTION("format", format, false, false);
  DEFINE_BUILTIN_FUNCTION("cons", cons, false, false);
  DEFINE_BUILTIN_FUNCTION("read-line", read_line, false, false);

  // Builtin macros.
  DEFINE_BUILTIN_FUNCTION("set", set, true, false);
  DEFINE_BUILTIN_FUNCTION("quote", quote, true, false);
  DEFINE_BUILTIN_FUNCTION("quasi-quote", quasi_quote, true, false);
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
  char *startup[] = {
      "core/core.l",
  };

  for (size_t i = 0; i < sizeof(startup) / sizeof(startup[0]); i++) {
    StringView buffer = read_file_sv(startup[i]);

    if (!buffer.array) {
      lisp_fprintf(stderr, "warning: could not read contents of %s\n",
                   startup[i]);
      continue;
    }

    LispValue return_value = repl(buffer, global_env);

    free(buffer.array);

    if (lisp_is_type(return_value, LISP_ERROR_T)) {
      lisp_fprintf(stderr,
                   "warning: startup file %s returned error\n"
                   "%s: ~ %l\n",
                   startup[i], startup[i], return_value);
    }
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

    LispValue return_value = repl(buffer, global_env);

    free(buffer.array);

    if (!test_mode)
      lisp_printf("%s: ~ %l\n", argv[i], return_value);

    else {
      // In test mode, every file must return nil.
      // If they do not, that means that the test failed.

      if (lisp_is_ident(return_value, LISP_NIL)) {
        if (always_show_test_results)
          lisp_printf("info: %s test passed\n", argv[i]);
      } else {
        lisp_fprintf(stderr, "fail: %s test failed\n", argv[i]);
        lisp_fprintf(stderr, "~ %l\n", return_value);
        test_failed = true;
      }
    }
  }

  if (test_failed)
    exit(1);
  else if (test_mode)
    lisp_fprintf(stderr, "test: passed all tests\n");

  // Interactive mode.
  if (interactive)
    read_history(history_path);

  while (interactive) {
    LispValue result = interactive_repl(global_env);

    if (!result) {
      break;
    }

    lisp_printf("~ %l\n", result);
  }
}
