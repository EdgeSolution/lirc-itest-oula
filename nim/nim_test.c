/******************************************************************************
*
* FILENAME:
*     nim_test.c
*
* DESCRIPTION:
*     Define functions for NIM test.
*
* REVISION(MM/DD/YYYY):
*     07/29/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version 
*
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include "nim_test.h"

void nim_print_status();
void nim_print_result(int fd);
void *nim_test(void *args);


test_mod_t test_mod_nim = {
    .run = 1,
    .pass = 1,
    .name = "nim",
    .log_fd = -1,
    .test_routine = nim_test,
    .print_status = nim_print_status,
    .print_result = nim_print_result
};


void nim_print_status()
{
}

void nim_print_result(int fd)
{
    if (test_mod_nim.pass) {
        write_file(fd, "NIM: PASS\n");
    } else {
        write_file(fd, "NIM: FAIL\n");
    }
}


void *nim_test(void *args)
{
    int log_fd = test_mod_nim.log_fd;

    log_print(log_fd, "Begin test!\n\n");

    while (g_running) {
        sleep(1);
    }

    log_print(log_fd, "Test end\n\n");
    pthread_exit(NULL);
}
