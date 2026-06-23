#ifndef EVALUATION_H
#define EVALUATION_H

#include "lisp_value.h"

LispValue evaluate_lisp(LispValue lisp, Environment *env);

LispValue repl(char *lisp, Environment *env);

#endif
