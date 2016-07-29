/******************************************************************************
 *
 * FILENAME:
 *     common.h
 *
 * DESCRIPTION:
 *     Define common structures and functions for all modules
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _COMMON_H_
#define _COMMON_H_

#include <limits.h>
#include <pthread.h>
#include "log.h"
#include "cfg.h"


/* Global variables **/
extern char g_tester[];
extern char g_product_sn[];
extern int g_duration;
extern char g_machine;
extern int g_running;
extern int g_board_num;
extern int g_baudrate;
extern int g_speed;


/* Sturcture for each test module */
typedef struct _test_mod {
    /* 1- Run, 0 -don't run, this flag is loaded from config file */
    int run;

    /* test result: 1 - pass, 0 - fail */
    int pass;

    /*
     * Name of test module(Lowercase Letters). It shall be the same as the
     * section name in config file, and it will also be a part of the name
     * of log file.
     */
    char *name;

    /* Log file */
    char log_file[PATH_MAX];
    int log_fd;

    /*
     * Thread routine and pid of test module, this routine shall be implemented
     * by test module itself.
     */
    pthread_t pid;
    void * (* test_routine)(void *arg);

    /*
     * Routine to print status of test module, this routine shall be implemented
     * by test module itself. And called by main process to update status. 
     */
    void (* print_status)(void);

    /*
     * Routine to print result of test module, this routine shall be implemented
     * by test module itself.  And called by main process to create report. 
     */
    void (* print_result)(int fd);
} test_mod_t;


#endif /* _COMMON_H_ */
