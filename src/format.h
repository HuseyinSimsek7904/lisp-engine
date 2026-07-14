#ifndef FORMAT_H
#define FORMAT_H

#include <stdio.h>

#include "lisp_value.h"

void lisp_value_format(StringBuilder *builder, LispValue value);
void lisp_fprintf(FILE *file, const char *format, ...);
void lisp_printf(const char *format, ...);

#endif
