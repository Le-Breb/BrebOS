#ifndef HEADERS_H
#define HEADERS_H

#define _POSIX_C_SOURCE 200809L
#define NULL 0

#define HERE printf("HERE\n");
#define HERE2 printf("HERE2\n");
#define THERE printf("THERE\n");
#define THERE2 printf("THERE2\n");

#include <ksyscalls.h>
#include <kctype.h>
#include <kerr.h>
//#include <fcntl.h>
//#include <kstdbool.h>
#include <kstddef.h>
#include <kstdio.h>
#include <kstdlib.h>
#include <kstring.h>
//#include <sys/stat.h>
//#include <sys/wait.h>
//#include <unistd.h>

#include "ast/ast.h"
#include "builtins/builtins.h"
#include "execution/execution.h"
#include "expansion/env_vars.h"
#include "expansion/expansion.h"
#include "expansion/shell_vars.h"
#include "io_backend/core.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "list_var/list_func.h"
#include "list_var/list_var.h"
#include "parser/parser.h"

#endif /* !HEADERS_H */
