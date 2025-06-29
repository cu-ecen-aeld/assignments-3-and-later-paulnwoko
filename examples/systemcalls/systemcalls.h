#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>

#include <unistd.h>//for exec

#include <sys/types.h>//for wait
#include <sys/wait.h>//for wait

bool do_system(const char *command);

bool do_exec(int count, ...);

bool do_exec_redirect(const char *outputfile, int count, ...);
