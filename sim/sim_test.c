/******************************************************************************
*
* FILENAME:
*     sim_test.c
*
* DESCRIPTION:
*     Define functions for SIM test.
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
#include "sim_test.h"

void sim_print_status();
void sim_print_result(int fd);
void *sim_test(void *args);


test_mod_t test_mod_sim = {
    .run = 1,
    .pass = 1,
    .name = "sim",
    .log_fd = -1,
    .test_routine = sim_test,
    .print_status = sim_print_status,
    .print_result = sim_print_result
};


void sim_print_status()
{
}

void sim_print_result(int fd)
{
    if (test_mod_sim.pass) {
        write_file(fd, "SIM: PASS\n");
    } else {
        write_file(fd, "SIM: FAIL\n");
    }
}


void *sim_test(void *args)
{
    int log_fd = test_mod_sim.log_fd;

    log_print(log_fd, "Begin test!\n\n");

    while (g_running) {
        sleep(1);
    }

    log_print(log_fd, "Test end\n\n");
    return NULL;
}
