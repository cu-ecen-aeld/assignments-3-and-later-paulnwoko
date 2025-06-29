
#include <stdlib.h>
#include "systemcalls.c"

int main(int argc, char *argv[])
{
    const char *CMD = argv[1]; //get the second argument
    do_system(CMD);
    
    char *args[] = {"/bin/echo", "/bin/echo", NULL};

    if (do_exec(3, args[0], args)) {
        printf("Command executed successfully.\n");
    } else {
        printf("Command failed.\n");
    }
    
    return 0;
}
