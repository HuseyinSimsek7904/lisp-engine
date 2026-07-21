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
  return ERROR(m, 3, lisp_builtin_##n, INT64_TO_LISP(argc), INT64_TO_LISP(c));

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
    if (v->type != t)                                                          \
      RETURN_INVALID_ARG(lisp_builtin_##n, v, TYPE_TO_LISP(t));                \
  } while (false)

#define ARG0 args->cons.head
#define ARG1 args->cons.tail->cons.head

#define DEFINE_BUILTIN_FUNCTION(n, c, m, t)                                    \
  lisp_builtin_##c = &(_LispValue){                                            \
      .type = LISP_BUILTIN_FUNC_T,                                             \
      .builtin_function = &(_LispBuiltinFunction){                             \
          .callback = (LispValue (*)(LispValue, void *))builtin_##c,           \
          .is_macro = m,                                                       \
          .tail_call = t,                                                      \
          .name = n}};                                                         \
  env_set_shallow(&global_env, n, lisp_builtin_##c)

#define DEFINE_TYPE_REP(n, r)                                                  \
  env_set(&global_env, n, &(_LispValue){.type = LISP_TYPE_T, .ref_type = r});

DEFINE_BUILTIN(set) {
  REQUIRES_EQ_ARGS(set, 2);
  CHECK_ARG_TYPE(set, ARG0, LISP_SYMBOL_T);

  char *symbol = ARG0->symbol;
  LispValue value = evaluate_lisp(ARG1, env);

  env_set(env, symbol, value);

  return value;
}

DEFINE_BUILTIN(setg) {
  REQUIRES_EQ_ARGS(setg, 2);
  CHECK_ARG_TYPE(setg, ARG0, LISP_SYMBOL_T);

  char *symbol = ARG0->symbol;
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
    LispValue remaining_defs = ARG0;

    while (lisp_is_non_empty_list(remaining_defs)) {
      LispValue def_node = remaining_defs->cons.head;
      remaining_defs = remaining_defs->cons.tail;

      CHECK_ARG_TYPE(let, def_node, LISP_LIST_T);

      if (lisp_get_len(def_node) != 2)
        return ERROR(
            WRONG_TYPE_ARG_SYM, 3, lisp_builtin_let, def_node,
            STATIC_STR_TO_LISP(
                "`let` binding list must be a list of key-value pairs"));

      CHECK_ARG_TYPE(let, def_node->cons.head, LISP_SYMBOL_T);

      char *symbol = def_node->cons.head->symbol;

      if (env_find_shallow(&new_env, symbol))
        return ERROR(REDEF_SYMBOL_SYM, 1, def_node->cons.head);

      // TODO: Maybe the passed environment should be the new environment.
      // That way one variable defined can be used in the other one.
      LispValue value = evaluate_lisp(def_node->cons.tail->cons.head, env);

      env_set_shallow(&new_env, symbol, value);
    }

    assert(lisp_is_nil(remaining_defs));

  } else if (!lisp_is_nil(ARG0))
    RETURN_INVALID_ARG(lisp_builtin_let, ARG0, TYPE_TO_LISP(LISP_LIST_T));

  LispValue return_value = evaluate_lisp_body(args->cons.tail, &new_env, false);

  env_free(&new_env);

  return return_value;
}

DEFINE_BUILTIN(if) {
  REQUIRES_MIN_ARGS(if, 2);

  LispValue condition = evaluate_lisp(ARG0, env);

  if (condition->type == LISP_ERROR_T)
    return condition;

  CHECK_ARG_TYPE(if, condition, LISP_BOOL_T);

  if (condition->boolean)
    return ARG1;

  return evaluate_lisp_body(args->cons.tail->cons.tail, env, true);
}

LispValue define_fn_or_macro(LispValue args, Environment *env,
                             LispValue builtin, bool is_macro) {
  size_t argc = lisp_get_len(args);
  if (argc < 2)
    return lisp_make_error(lisp_make_list(4, SYM_TO_LISP(TOO_FEW_ARGS_SYM),
                                          builtin, INT64_TO_LISP(argc),
                                          INT64_TO_LISP(2)));

  bool variadic = false;
  LispValue arg_symbols = LISP_NIL;

  if (lisp_is_non_empty_list(ARG0)) {
    LispValue *last = &arg_symbols;

    LispValue remaining_args = ARG0;
    LispValue arg;

    while (lisp_is_non_empty_list(remaining_args->cons.tail)) {
      arg = remaining_args->cons.head;
      remaining_args = remaining_args->cons.tail;

      if (arg->type != LISP_SYMBOL_T)
        return lisp_make_error(lisp_make_list(
            4, SYM_TO_LISP(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            STATIC_STR_TO_LISP(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      lisp_rappend_use_last(last, arg);
      last = &((*last)->cons.tail);
    }

    assert(lisp_is_nil(remaining_args->cons.tail));
    arg = remaining_args->cons.head;

    if (lisp_is_non_empty_list(arg)) {
      if (lisp_get_len(arg) != 2)
        return lisp_make_error(lisp_make_list(
            4, SYM_TO_LISP(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            STATIC_STR_TO_LISP(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      if (!lisp_is_sym(arg->cons.head, "rest"))
        return lisp_make_error(lisp_make_list(
            4, SYM_TO_LISP(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            STATIC_STR_TO_LISP(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      if (arg->cons.tail->cons.head->type != LISP_SYMBOL_T)
        return lisp_make_error(lisp_make_list(
            4, SYM_TO_LISP(WRONG_TYPE_ARG_SYM), builtin, ARG0,
            STATIC_STR_TO_LISP(
                "argument list must be in the form ([SYMBOL...] [(rest "
                "SYMBOL)])")));

      variadic = true;

      lisp_rappend_use_last(last, arg->cons.tail->cons.head);

    } else if (arg->type == LISP_SYMBOL_T) {
      lisp_rappend_use_last(last, arg);

    } else
      return lisp_make_error(lisp_make_list(
          4, SYM_TO_LISP(WRONG_TYPE_ARG_SYM), builtin, ARG0,
          STATIC_STR_TO_LISP(
              "argument list must be in the form ([SYMBOL...] [(rest "
              "SYMBOL)])")));

  } else if (!lisp_is_nil(ARG0))
    return lisp_make_error(lisp_make_list(4, SYM_TO_LISP(WRONG_TYPE_ARG_SYM),
                                          builtin, ARG0,
                                          TYPE_TO_LISP(LISP_LIST_T)));

  return lisp_make_fn_or_macro(arg_symbols, args->cons.tail, variadic,
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
  LispValue remaining_args = args;
  while (lisp_is_non_empty_list(remaining_args)) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    CHECK_ARG_TYPE(print, arg, LISP_STRING_T);
  }

  assert(lisp_is_nil(remaining_args));

  remaining_args = args;
  while (lisp_is_non_empty_list(remaining_args)) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    lisp_printf("%s\n", arg->string);
  }

  assert(lisp_is_nil(remaining_args));

  return LISP_NIL;
}

DEFINE_BUILTIN(sum) {
  int64_t sum = 0;

  LispValue remaining_args = args;
  while (lisp_is_non_empty_list(remaining_args)) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    CHECK_ARG_TYPE(sum, arg, LISP_INT64_T);

    sum += arg->integer;
  }

  return INT64_TO_LISP(sum);
  assert(lisp_is_nil(remaining_args));
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

  return INT64_TO_LISP(arg_count == 1 ? -ARG0->integer
                                      : ARG0->integer - ARG1->integer);
}

DEFINE_BUILTIN(list) { return args; }

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (= a b c) should check the equality between every item pair.
DEFINE_BUILTIN(equal) {
  REQUIRES_EQ_ARGS(equal, 2);

  return BOOL_TO_LISP(lisp_is_ident(ARG0, ARG1));
}

// TODO: This builtin should allow for any number of arguments except zero and
// one.
// (< a b c) should compare a < b < c.
DEFINE_BUILTIN(less_than) {
  REQUIRES_EQ_ARGS(less_than, 2);
  CHECK_ARG_TYPE(less_than, ARG0, LISP_INT64_T);
  CHECK_ARG_TYPE(less_than, ARG1, LISP_INT64_T);

  return BOOL_TO_LISP(ARG0->integer < ARG1->integer);
}

DEFINE_BUILTIN(read_file) {
  REQUIRES_EQ_ARGS(read_file, 1);
  CHECK_ARG_TYPE(read_file, ARG0, LISP_STRING_T);

  char *file_name = sv_copy_cstr(ARG0->string);
  StringView buffer = read_file_sv(file_name);

  free(file_name);

  if (!buffer.array)
    RETURN_COULD_NOT_READ_FILE(ARG0);

  return STR_TO_LISP(buffer);
}

DEFINE_BUILTIN(parse_lisp) {
  REQUIRES_EQ_ARGS(parse_lisp, 1);
  CHECK_ARG_TYPE(parse_lisp, ARG0, LISP_STRING_T);

  Tokens tokens;
  da_init(&tokens);
  LexerResult lexer_result = generate_tokens(ARG0->string, &tokens);

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
  LispValue items = LISP_NIL;
  LispValue *last = &items;

  LispValue remaining_args = args;
  while (lisp_is_non_empty_list(remaining_args)) {
    LispValue arg = remaining_args->cons.head;
    remaining_args = remaining_args->cons.tail;

    if (!lisp_is_list(arg))
      RETURN_INVALID_ARG(lisp_builtin_unpack, arg, TYPE_TO_LISP(LISP_LIST_T));

    *last = arg;
    last = lisp_cons_find_term_nil(last);
  }

  assert(lisp_is_nil(remaining_args));

  return lisp_make_unpacked(items);
}

DEFINE_BUILTIN(quote) {
  REQUIRES_EQ_ARGS(quote, 1);

  return ARG0;
}

DEFINE_BUILTIN(head) {
  REQUIRES_EQ_ARGS(head, 1);
  CHECK_ARG_TYPE(head, ARG0, LISP_LIST_T);

  return ARG0->cons.head;
}

DEFINE_BUILTIN(tail) {
  REQUIRES_EQ_ARGS(tail, 1);
  CHECK_ARG_TYPE(tail, ARG0, LISP_LIST_T);

  return ARG0->cons.tail;
}

DEFINE_BUILTIN(typeof) {
  REQUIRES_EQ_ARGS(typeof, 1);

  return TYPE_TO_LISP(ARG0->type);
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

  StringView format_str = ARG0->string;
  LispValue remaining_args = args->cons.tail;

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
      if (lisp_is_nil(remaining_args)) {
        da_free(&builder);
        return ERROR(TOO_FEW_ARGS_FORMAT_SYM, 1, ARG0);
      }

      assert(lisp_is_non_empty_list(remaining_args));

      lisp_value_format(&builder, remaining_args->cons.head, FORMAT_BASIC);
      remaining_args = remaining_args->cons.tail;
    } break;

    case 'L': {
      if (lisp_is_nil(remaining_args)) {
        da_free(&builder);
        return ERROR(TOO_FEW_ARGS_FORMAT_SYM, 1, ARG0);
      }

      assert(lisp_is_non_empty_list(remaining_args));

      lisp_value_format(&builder, remaining_args->cons.head, FORMAT_PRODUCER);
      remaining_args = remaining_args->cons.tail;
    } break;

    case 's': {
      if (lisp_is_nil(remaining_args)) {
        da_free(&builder);
        return ERROR(TOO_FEW_ARGS_FORMAT_SYM, 1, ARG0);
      }

      assert(lisp_is_non_empty_list(remaining_args));

      if (remaining_args->cons.head->type != LISP_STRING_T) {
        da_free(&builder);
        return ERROR(WRONG_TYPE_ARG_FORMAT_SYM, 2, remaining_args->cons.head,
                     TYPE_TO_LISP(LISP_STRING_T));
      }

      sb_append_view(&builder, remaining_args->cons.head->string);
      remaining_args = remaining_args->cons.tail;
    } break;

    default:
      da_free(&builder);
      return ERROR(INVALID_FORMAT_SYM, 1, ARG0);
    }
  }

  return STR_TO_LISP(sb_to_sv(&builder));
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

  char *prompt = sv_copy_cstr(ARG0->string);
  char *read = readline(prompt);
  free(prompt);

  StringView view = {.array = read, .size = strlen(read)};
  return STR_TO_LISP(view);
}

// TODO: This should expect a list of symbols, where each symbol is a flag.
// For now, this is satisfactory.
DEFINE_BUILTIN(debug_set_flags) {
  REQUIRES_EQ_ARGS(debug_set_flags, 1);
  CHECK_ARG_TYPE(debug_set_flags, ARG0, LISP_INT64_T);

  debug_flags = ARG0->integer;

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

  int opt;

  while ((opt = getopt(argc, argv, "ntd:")) != -1) {
    switch (opt) {
    case 'n':
      interactive = false;
      break;

    case 't':
      test_mode = true;
      interactive = false;
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
  LISP_NIL = &(_LispValue){.type = LISP_NIL_T};
  LISP_FALSE = &(_LispValue){.type = LISP_BOOL_T, .boolean = false};
  LISP_TRUE = &(_LispValue){.type = LISP_BOOL_T, .boolean = true};

  env_set_shallow(&global_env, "nil", LISP_NIL);
  env_set_shallow(&global_env, "false", LISP_FALSE);
  env_set_shallow(&global_env, "true", LISP_TRUE);

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

    LispValue return_value = repl(buffer, &global_env);

    free(buffer.array);

    if (!test_mode)
      lisp_printf("%s: ~ %l\n", argv[i], return_value);

    else if (!lisp_is_nil(return_value)) {
      // In test mode, every file must return nil.
      // If they do not, that means that the test failed.

      lisp_fprintf(stderr, "fail: %s test failed\n", argv[i]);
      lisp_fprintf(stderr, "%l\n", return_value);
      test_failed = true;
    }
  }

  if (test_failed)
    exit(1);
  else if (test_mode)
    lisp_fprintf(stderr, "test: passed all tests\n");

  // Interactive mode.
  while (interactive) {
    char *lisp = readline("> ");
    StringView view = {.array = lisp, .size = strlen(lisp)};

    if (!lisp) {
      interactive = false;
      continue;
    }

    add_history(lisp);

    LispValue return_value = repl(view, &global_env);

    free(lisp);

    lisp_printf("~ %l\n", return_value);
  }
}
