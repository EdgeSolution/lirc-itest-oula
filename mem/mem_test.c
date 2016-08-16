/******************************************************************************
*
* FILENAME:
*     mem_test.c
*
* DESCRIPTION:
*     Define functions for MEM test.
*
* REVISION(MM/DD/YYYY):
*     08/16/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version 
*
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <signal.h>
#include <unistd.h>
#include "mem_test.h"
#include "common.h"

void mem_print_status();
void mem_print_result(int fd);
void *mem_test(void *args);


test_mod_t test_mod_mem = {
    .run = 1,
    .pass = 1,
    .name = "mem",
    .log_fd = -1,
    .test_routine = mem_test,
    .print_status = mem_print_status,
    .print_result = mem_print_result
};


void mem_print_status()
{
    printf("%-*s %s\n", COL_FIX_WIDTH, "MEM",
        test_mod_mem.pass?STR_MOD_OK:STR_MOD_ERROR);
}


void mem_print_result(int fd)
{
    if (test_mod_mem.pass) {
        write_file(fd, "MEM: PASS\n");
    } else {
        write_file(fd, "MEM: FAIL\n");
    }
}


void *mem_test(void *args)
{
    int log_fd = test_mod_mem.log_fd;
    char *log_file = test_mod_mem.log_file;
    char *prog = "memtester";
    char cmd[260];

    pid_t pid = fork();
    switch (pid) {
    case -1:
        log_print(log_fd, "start %s failed\n", prog);
        test_mod_mem.pass = 0;
        break;

    case 0:
        snprintf(cmd, sizeof(cmd), "%s %dM >%s", prog, 3000, log_file);
        system(cmd);
        return NULL;

    default:
        while (g_running) {
            sleep(3);
        }
        kill_process(prog);
        break;
    }

    pthread_exit(NULL);
}
