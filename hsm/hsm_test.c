/******************************************************************************
*
* FILENAME:
*     hsm_test.c
*
* DESCRIPTION:
*     Define functions for HSM test.
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
#include "hsm_test.h"

void hsm_print_status();
void hsm_print_result(int fd);
void *hsm_test(void *args);


test_mod_t test_mod_hsm = {
    .run = 1,
    .pass = 1,
    .name = "hsm",
    .log_fd = -1,
    .test_routine = hsm_test,
    .print_status = hsm_print_status,
    .print_result = hsm_print_result
};


void hsm_print_status()
{
}

void hsm_print_result(int fd)
{
    if (test_mod_hsm.pass) {
        write_file(fd, "HSM: PASS\n");
    } else {
        write_file(fd, "HSM: FAIL\n");
    }
}


void *hsm_test(void *args)
{
    int log_fd = test_mod_hsm.log_fd;

    log_print(log_fd, "Begin test!\n\n");

    while (g_running) {
    }

    log_print(log_fd, "Test end\n\n");
    return NULL;
}
