/******************************************************************************
*
* FILENAME:
*     msm_test.c
*
* DESCRIPTION:
*     Define functions for MSM test.
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
#include "msm_test.h"

void msm_print_status();
void msm_print_result(int fd);
void *msm_test(void *args);


test_mod_t test_mod_msm = {
    .run = 1,
    .pass = 1,
    .name = "msm",
    .log_fd = -1,
    .test_routine = msm_test,
    .print_status = msm_print_status,
    .print_result = msm_print_result
};


void msm_print_status()
{
}

void msm_print_result(int fd)
{
    if (test_mod_msm.pass) {
        write_file(fd, "MSM: PASS\n");
    } else {
        write_file(fd, "MSM: FAIL\n");
    }
}


void *msm_test(void *args)
{
    int log_fd = test_mod_msm.log_fd;

    log_print(log_fd, "Begin test!\n\n");

    while (g_running) {
    }

    log_print(log_fd, "Test end\n\n");
    return NULL;
}
