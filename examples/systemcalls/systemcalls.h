#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>

#include <unistd.h>//for fork, exec, dup2
#include <sys/types.h>//for fork, exec, wait
#include <sys/wait.h>//for wait
#include <fcntl.h>//for file io (open, close), dup2

bool do_system(const char *command);

bool do_exec(int count, ...);

bool do_exec_redirect(const char *outputfile, int count, ...);
