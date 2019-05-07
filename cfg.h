/******************************************************************************
 *
 * FILENAME:
 *     cfg.h
 *
 * DESCRIPTION:
 *     Define functions to load setting from configuration file
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version
 *
 ******************************************************************************/
#ifndef _CFG_H_
#define _CFG_H_

#ifndef TRUE
#define TRUE        1
#endif
#ifndef FALSE
#define FALSE       0
#endif

#define MAX_STR_LENGTH      100

/* Max counter of sim modules */
#define MAX_SIM_COUNT       2

#define TESC_NUM 4

#define APPNAME_CCM         "lirc-itest"

enum DEV_SKU {
    SKU_CCM = 0,
    SKU_CCM_LEGACY,
    SKU_CIM,
    SKU_CCM_MSM,
};

extern enum DEV_SKU g_dev_sku;

int get_parameter(void);
int parse_params(int argc, char **argv);
int get_eth_num(enum DEV_SKU sku);

#endif /* _CFG_H_ */
