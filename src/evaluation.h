#ifndef EVALUATION_H
#define EVALUATION_H

#include "lisp_value.h"

LispValue evaluate_lisp(LispValue lisp, Environment *env);
LispValue evaluate_lisp_body(LispValue lisp, Environment *env, bool tail_call);

LispValue repl(char *lisp, Environment *env);

#endif
