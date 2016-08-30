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
static int find_fail_str(char *logfile);


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
    char prog_path[1024];
    char cmd[1024];

    snprintf(prog_path, sizeof(prog_path), "%s/%s", g_progam_path, prog);
    if (!is_exe_exist(prog_path)) {
        snprintf(prog_path, sizeof(prog_path), "%s", prog);
        if (!is_exe_exist(prog_path)) {
            log_print(log_fd, "%s not found\n", prog);
            test_mod_mem.pass = 0;
            pthread_exit(NULL);
        }
    }
    DBG_PRINT("memtester: %s\n", prog_path);

    pid_t pid = fork();
    switch (pid) {
    case -1:
        log_print(log_fd, "start %s failed\n", prog);
        test_mod_mem.pass = 0;
        break;

    case 0:
        snprintf(cmd, sizeof(cmd), "%s %dM >%s", prog_path, 3000, log_file);
        system(cmd);
        return NULL;

    default:
        while (g_running) {
            sleep(3);
        }
        kill_process(prog);

        if (find_fail_str(log_file)) {
            test_mod_mem.pass = 0;
        }
        break;
    }

    pthread_exit(NULL);
}


/******************************************************************************
 * NAME:
 *      find_fail_str
 *
 * DESCRIPTION: 
 *      Search "FAIL" string from log file.
 *
 * PARAMETERS:
 *      logfile - The log file.
 *
 * RETURN:
 *      1 - "FAIL" string found or no log file.
 *      0 - No "FAIL" found.
 ******************************************************************************/
static int find_fail_str(char *logfile)
{
    char line[1024];
    int ret = 0;

    FILE *fp = fopen(logfile, "r");
    if (fp == NULL) {
        return 1;
    }

    while (fgets(line, sizeof(line)-1, fp)) {
        if (strstr(line, "FAIL") != NULL) {
            ret = 1;
        }
    }

    fclose(fp);
    return ret;
}
