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

#include "log.h"
#include "cfg.h"

#define MAX_STR_LENGTH      100

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
    /* 1- Run, 0 -don't run */
    int run;

    /* 0 - pass, 1 - fail */
    int result;

    /* Name of test module */
    char name[MAX_STR_LENGTH];

    /* Log file */
    char log_file[PATH_MAX];

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


/* Arguments pass to the test_routine of test module */
typedef struct _mod_args {
    test_mod_t *mod;
} mod_args;


#endif /* _COMMON_H_ */
