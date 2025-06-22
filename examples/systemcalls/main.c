
#include <stdlib.h>
#include "systemcalls.c"

int main(int argc, char *argv[])
{
    const char *CMD = argv[1]; //get the second argument
    do_system(CMD);
    return false;
}
