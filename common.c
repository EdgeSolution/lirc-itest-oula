/******************************************************************************
 *
 * FILENAME:
 *     common.c
 *
 * DESCRIPTION:
 *     Define some useful functions
 *
 * REVISION(MM/DD/YYYY):
 *     08/16/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"


//Return code for system
#define DIAG_SYS_RC(x)    ((WTERMSIG(x) == 0)?(WEXITSTATUS(x)):-1)

void kill_process(char *name)
{
    char kill[260];

    sprintf(kill, "pkill -2 -f %s", name);
    system(kill);
}
