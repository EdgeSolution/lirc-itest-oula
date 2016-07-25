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


#endif /* _COMMON_H_ */
