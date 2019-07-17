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
#include <stdint.h>
#include "log.h"
#include "cfg.h"

extern uint8_t g_syncing;


/* Global variables **/
extern char g_tester[];

/* Product Serial Number */
extern char g_product_sn[];
extern char g_ccm_sn[];
extern char g_nim_sn[];
extern char g_msm_sn[];
extern char g_hsm_sn[];
extern char g_sim_sn[][MAX_STR_LENGTH];

extern char g_machine;

extern uint64_t g_duration;
extern int g_running;
extern uint8_t g_board_num;
extern int g_baudrate;
extern char g_progam_path[];

extern uint64_t g_hsm_test_loop;
extern uint8_t g_hsm_switching;

uint8_t g_nim_test_eth[MAX_NIC_COUNT];

extern int g_test_mode;

/* Separate modules settings for running or not */
extern int g_test_cpu;
extern int g_test_sim;
extern int g_test_nim;
extern int g_test_hsm;
extern int g_test_msm;
extern int g_test_led;
extern int g_test_mem;

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


#define COL_FIX_WIDTH       20
#define STR_MOD_OK          "MODULE is OK"
#define STR_MOD_ERROR       "MODULE is ERROR"

#define CCM_SERIAL_PORT     "/dev/ttyS1"
#define DATA_SYNC_A         0xFA
#define DATA_SYNC_B         0xFB

#define EXIT_SYNC_A         0xEA
#define EXIT_SYNC_B         0xEB

//Switch interval for hsm_test
#define WAIT_IN_MS          500

void kill_process(char *name);
int wait_other_side_ready(int fd);
int sleep_ms(unsigned int ms);
int is_exe_exist(char *exe);
int ser_open(char *dev);
void send_exit_sync(void);
void receive_exit_sync(void);
int send_packet(int fd, char *buf, uint8_t len);

int set_ipaddr(uint32_t ethid, char *ipaddr, char *netmask);
int socket_init(int *sockfd, char *ipaddr, uint16_t portid);
int wait_other_side_ready_eth(void);
void set_if_up_all(void);
void wait_link_status_all(uint8_t num);
void tc_set_rts_casco(int fd, char enabled);

#endif /* _COMMON_H_ */
