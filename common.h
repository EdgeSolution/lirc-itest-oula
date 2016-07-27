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


/* Arguments pass to the test_routine of test module */
typedef struct _mod_args {
    char *log_file;
} mod_args;

/* Sturcture for each test module */
typedef struct _test_mod {
    /* 1- Run, 0 -don't run */
    int run;

    /* Name of test module */
    char name[MAX_STR_LENGTH];
    
    /* Log file */
    char log_file[PATH_MAX];

    /* Thread routine and pid of test module */
    pthread_t pid;
    void * (* test_routine)(void *arg);

    /* Interface to print status of test module */
    void (* print_status)(void);
} test_mod_t;

#endif /* _COMMON_H_ */
