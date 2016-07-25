/******************************************************************************
*
* FILENAME:
*     main.c
*
* DESCRIPTION:
*     Integration test software for CPCI-1504
*
* REVISION(MM/DD/YYYY):
*     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version 
*
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>
#include <fcntl.h>
#include <termios.h>
#include "term.h"

/* Version of the program */
#define PROGRAM_VERSION     "0.1"


/* Machine A or B */
char g_machine;

/* The flag used to notify the program to exit */
static int g_running = 1;


/* Prototype */
int get_parameter(void);
int install_sig_handler(void);



int main(int argc, char **argv)
{
    int rc = 0;

    if (argc != 1) {
        printf("LIRC-ITEST v%s\n", PROGRAM_VERSION);
        return -1;
    }

    get_parameter();

    install_sig_handler();

    return rc;
}


/******************************************************************************
 * NAME:
 *      get_parameter
 *
 * DESCRIPTION:
 *      Init parameters by the input from user.
 *
 * PARAMETERS:
 *      None
 *
 * RETURN:
 *      0  - OK
 *      <0 - error
 ******************************************************************************/
int get_parameter(void)
{
    printf("Please input A or B for this machine:\n");
    if (scanf("%c", &g_machine) != 1) {
        printf("input error\n");
        return -1;
    }
    switch (g_machine) {
    case 'a':
    case 'A':
        g_machine = toupper(g_machine);
        break;

    case 'b':
    case 'B':
        g_machine = toupper(g_machine);
        break;

    default:
        printf("input error\n");
        return -1;
        break;
    }


    return 0;
}



void sig_handler(int signo)
{
    g_running = 0;
}

int install_sig_handler(void)
{
    struct sigaction sa;

    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        printf("sigaction(SIGUSR1) error\n");
        return -1;
    }

    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        printf("sigaction(SIGTERM) error\n");
        return -1;
    }

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        printf("sigaction(SIGINT) error\n");
        return -1;
    }

    return 0;
}
