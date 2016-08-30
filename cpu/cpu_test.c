/******************************************************************************
*
* FILENAME:
*     cpu_test.c
*
* DESCRIPTION:
*     Define functions for CPU test.
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
#include "cpu_test.h"
#include "common.h"

void cpu_print_status();
void cpu_print_result(int fd);
void *cpu_test(void *args);


test_mod_t test_mod_cpu = {
    .run = 1,
    .pass = 1,
    .name = "cpu",
    .log_fd = -1,
    .test_routine = cpu_test,
    .print_status = cpu_print_status,
    .print_result = cpu_print_result
};


void cpu_print_status()
{
    printf("%-*s %s\n", COL_FIX_WIDTH, "CPU",
        test_mod_cpu.pass?STR_MOD_OK:STR_MOD_ERROR);
}


void cpu_print_result(int fd)
{
    if (test_mod_cpu.pass) {
        write_file(fd, "CPU: PASS\n");
    } else {
        write_file(fd, "CPU: FAIL\n");
    }
}


void *cpu_test(void *args)
{
    int log_fd = test_mod_cpu.log_fd;
    char *log_file = test_mod_cpu.log_file;
    char *prog = "stresscpu2";
    char prog_path[1024];
    char cmd[260];

    snprintf(prog_path, sizeof(prog_path), "%s/%s", g_progam_path, prog);
    if (!is_exe_exist(prog_path)) {
        snprintf(prog_path, sizeof(prog_path), "%s", prog);
        if (!is_exe_exist(prog_path)) {
            log_print(log_fd, "%s not found\n", prog);
            test_mod_cpu.pass = 0;
            pthread_exit(NULL);
        }
    }
    DBG_PRINT("stresscpu: %s\n", prog_path);

    pid_t pid = fork();
    switch (pid) {
    case -1:
        log_print(log_fd, "start %s failed\n", prog);
        test_mod_cpu.pass = 0;
        break;

    case 0:
        snprintf(cmd, sizeof(cmd), "%s -n 3 > %s", prog_path, log_file);
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
