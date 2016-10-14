/******************************************************************************
 *
 * FILENAME:
 *     main.h
 *
 * DESCRIPTION:
 *     Define some structures and functions for main program
 *
 * REVISION(MM/DD/YYYY):
 *     07/29/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _MAIN_H_
#define _MAIN_H_

#include "common.h"
#include "term.h"
#include "led_test.h"
#include "hsm_test.h"
#include "msm_test.h"
#include "nim_test.h"
#include "sim_test.h"
#include "mem_test.h"
#include "cpu_test.h"


#define MAX_STR_LENGTH      100

/* Max counter of test modules */
#define MAX_MOD_COUNT       10

/* Max counter of sim modules */
#define MAX_SIM_COUNT       2

/* Function Prototype */
int is_product_sn_valid(char *psn);
int is_board_sn_valid(char *bsn);
int is_board_num_valid(int board_num);
int get_parameter(void);
int user_ack(int def);
int load_config(char *config_file);
int install_sig_handler(void);
size_t get_exe_path(char *path_buf, size_t len);
int init_path(void);
int move_log_to_error(char *log_file);
int start_test_module(test_mod_t *pmod);
void generate_report(void);
void set_timeout(int sec);


#endif /* _MAIN_H_ */
