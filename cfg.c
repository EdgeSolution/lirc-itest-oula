/******************************************************************************
 *
 * FILENAME:
 *     cfg.c
 *
 * DESCRIPTION:
 *     Define functions to load setting from configuration file
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version
 *     12/06/2018  Jia Sui (jia.sui@advantech.com.cn)
 *     - Add HSM test for CIM
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include "common.h"
#include "cfg.h"

enum DEV_SKU g_dev_sku = SKU_CCM;

/* Syncing flag: When this flag is 1, everything operation to the ttyS1 should
 * be stopped, except the syncing routine*/
uint8_t g_syncing = 0;

/* Tester */
char g_tester[MAX_STR_LENGTH];

/* Product Serial Number */
char g_product_sn[MAX_STR_LENGTH];
char g_ccm_sn[MAX_STR_LENGTH];
char g_sim_sn[MAX_SIM_COUNT][MAX_STR_LENGTH];
char g_nim_sn[MAX_STR_LENGTH];
char g_msm_sn[MAX_STR_LENGTH];
char g_hsm_sn[MAX_STR_LENGTH];

/* Test duration(minutes) */
uint64_t g_duration = 60;

/* Test mode(1 means all mode, 0 means selectable mode) */
int g_test_mode = 1;

/* Separate modules settings for running or not */
int g_test_cpu = 1;
int g_test_sim = 1;
int g_test_nim = 1;
int g_test_hsm = 1;
int g_test_msm = 1;
int g_test_led = 1;
int g_test_mem = 1;

/* Machine A or B */
char g_machine = 'A';

/* The flag used to notify the program to exit */
int g_running = 1;

/* Board number (SIM) */
uint8_t g_board_num = 2;

/* Baudrate of serial port (SIM) */
int g_baudrate = 115200;

/* HSM: test loop */
uint64_t g_hsm_test_loop = 100;
uint8_t g_hsm_switching = 0;

/* NIM: port test flag, set by user input */
uint8_t g_nim_test_eth[TESC_NUM] = {0};

//Function Prototype
static char *right_trim(char *str);
static char *left_trim(const char *str);
static int is_board_num_valid(int board_num);
static int is_product_sn_valid(char *psn);
static int is_board_sn_valid(char *bsn);
static int decode_time(unsigned char *value, int vlen);
static int input_str(const char *hint, char *str, uint8_t len);
static int input_num(const char *hint, uint64_t *num);
static int input_machine(char *machine);
static int input_sim_board_num(uint8_t *num);
static int input_product_sn(char *sn, uint8_t len);
static int input_board_sn(const char *board, char *sn, uint8_t len);
static int input_baudrate(int *baud);
static int user_ack(char *hint);
static int input_tester(char *tester, uint8_t len);
static int input_test_duration(uint64_t *duration);
static int input_hsm_loop(uint64_t *loop);

/*
 * Trim whitespace chars from the end of string.
 */
static char *right_trim(char *str)
{
    char *p = str + strlen(str);

    while ((p > str) && isspace(*--p)) {
        *p = '\0';
    }
    return str;
}


/*
 * Return pointer to first non-whitespace char in string.
 */
static char *left_trim(const char *str)
{
    while (*str && isspace(*str)) {
        str++;
    }
    return (char *)str;
}

/*
 * Enhance version of strncpy that ensures dest is null-terminated.
 */
static char *strncpy0(char *dest, const char *src, size_t size)
{
    strncpy(dest, src, size);
    if (size > 0) {
        dest[size - 1] = '\0';
    }
    return dest;
}

/******************************************************************************
 * NAME:
 *      is_board_num_valid
 *
 * DESCRIPTION:
 *      Check if the board number is valid or not.
 *      On hardware,
 *      if the one serial module, the serial device should be "ttyS2-ttyS9";
 *      if another serial module, the serial dvice should be "ttyS10-ttyS17"
 *
 * PARAMETERS:
 *      board_num - The board number to be verified.
 *
 * RETURN:
 *      1 - OK(Valid)
 *      0 - Not valid
 ******************************************************************************/
static int is_board_num_valid(int board_num)
{
    char *ser_prefix = "/dev/ttyS";
    char ser_dev[MAX_STR_LENGTH];
    int i;
    int valid = 0;

    for (i = 2; i <= (8 * board_num + 1); i++) {
        snprintf(ser_dev, sizeof(ser_dev), "%s%d", ser_prefix, i);

        if(access(ser_dev, F_OK) == 0) {
            valid++;
        }
    }

    if (valid >= (8 * board_num))
        return 1;

    return 0;
}

/******************************************************************************
 * NAME:
 *      is_product_sn_valid
 *
 * DESCRIPTION:
 *      Check if the product SN valid or not
 *      The format of the product SN:
 *          BBBBBBBSCYYMMXXXXX
 *      SC: letter
 *      BBBBBBB: digit number
 *      YYMM: digit number
 *      XXXXX: digit number
 *
 * PARAMETERS:
 *      psn - The product SN string.
 *
 * RETURN:
 *      1 - OK(Valid)
 *      0 - Not valid
 ******************************************************************************/
static int is_product_sn_valid(char *psn)
{
    int n;
    unsigned char buf[5];

    if (strcasecmp(psn, "CASCO") == 0) {
        return 1;
    }

    if (strlen(psn) != 18) {
        return 0;
    }

    /* Check BBBBBBB */
    for (n = 0; n < 7; n++) {
        if (!isdigit(psn[n])) {
            return 0;
        }
    }

    /* Check SC */
    for (n = 7; n < 9; n++) {
        if (!isalpha(psn[n])) {
            return 0;
        }
    }

    /* Check YYMMXXXXX */
    for (n = 9; n < 18; n++) {
        if (!isdigit(psn[n])) {
            return 0;
        }
    }

    /* Check YYMM */
    memset(buf, 0, sizeof(buf));
    for (n = 0; n < 4; n++) {
        buf[n] = psn[n+9];
    }
    buf[n] = '\0';
    if (1 != decode_time(buf, 4)) {
        return 0;
    }

    return 1;
}


/******************************************************************************
 * NAME:
 *      is_board_sn_valid
 *
 * DESCRIPTION:
 *      Check if the board SN valid or not
 *      The format of the board SN:
 *          AKCXNNNNNN
 *      AK: "AK"
 *      C: Uppercase letter
 *      X: hexadecimal digit number
 *      NNNNNN: digit number
 *
 * PARAMETERS:
 *      bsn - The board SN string.
 *
 * RETURN:
 *      1 - OK(Valid)
 *      0 - Not valid
 ******************************************************************************/
static int is_board_sn_valid(char *bsn)
{
    int n;

    if (strcasecmp(bsn, "CASCO") == 0) {
        return 1;
    }

    if (strlen(bsn) != 10) {
        return 0;
    }

    /* Check AK */
    if (strncasecmp(bsn, "AK", 2) != 0) {
        return 0;
    }

    /* Check if C is uppercase letter */
    if (!isupper(bsn[2])) {
        return 0;
    }

    /* Check X */
    if (!isxdigit(bsn[3])) {
        return 0;
    }

    /* Check NNNNNN */
    for (n = 4; n < 10; n++) {
        if (!isdigit(bsn[n])) {
            return 0;
        }
    }

    return 1;
}

/******************************************************************************
 * NAME:
 *      decode_time
 *
 * DESCRIPTION:
 *      Decode the time and Check if the time is valid (1970-2049) or not.
 *      The format of time:
 *          YYMM
 *      YY: year
 *      MM: month
 *
 * PARAMETERS:
 *      value - The string of "time"
 *
 * RETURN:
 *      1 - OK(Valid)
 *      0 - Not valid
 ******************************************************************************/
static int decode_time(unsigned char *value, int vlen)
{
    const unsigned char *p = value;
    unsigned year = 0, mon = 0;

#define dec2bin(X) ({ unsigned char x = (X) - '0'; if (x > 9) goto invalid_time; x; })
#define DD2bin(P) ({ unsigned x = dec2bin(P[0]) * 10 + dec2bin(P[1]); P += 2; x; })

    if (vlen != 4)
        goto unsupported_time;

    year = DD2bin(p);
    if (year >= 50)
        year += 1900;
    else
        year += 2000;

    mon = DD2bin(p);

    if (year < 1970 ||
        mon < 1 || mon > 12)
        goto invalid_time;

    DBG_PRINT("year=%u, month=%u\n", year, mon);

    return 1;

unsupported_time:
    DBG_PRINT("unspported time: %d, %s\n", vlen, value);
    return 0;

invalid_time:
    DBG_PRINT("invalid time: %s, year=%u, month=%u\n", value, year, mon);
    return 0;
}

static int input_str(const char *hint, char *str, uint8_t len)
{
    char buf[MAX_STR_LENGTH];
    char *p = NULL;

    if (!str) {
        return -1;
    }

    memset(buf, 0, sizeof(buf));

    printf("%s: ", hint);

    if (fgets(buf, MAX_STR_LENGTH, stdin) <= 0) {
        printf("input error\n");
        return -1;
    }

    p = left_trim(right_trim(buf));
    if (strlen(p) == 0) {
        printf("input error\n");
        return -1;
    }

    strncpy(str, p, len);

    return 0;
}

static int input_num(const char *hint, uint64_t *num)
{
    char buf[MAX_STR_LENGTH];
    int tmp = 0;

    if (!num) {
        return -1;
    }

    memset(buf, 0, sizeof(buf));

    printf("%s: ", hint);
    if (fgets(buf, sizeof(buf)-1, stdin) <= 0) {
        printf("input error\n");
        return -1;
    }

    tmp = atoi(buf);
    if (tmp <= 0) {
        printf("Invalid number\n");
        return -1;
    }

    *num = tmp;

    return 0;
}

static int input_machine(char *machine)
{
    char buf[2];
    int ret = 0;

    if (!machine) {
        return -1;
    }

    do {
        if (0 != input_str("Please input A or B for this machine", buf, sizeof(g_machine))) {
            ret = -1;
            continue;
        }

        if (buf[0] == 'a' || buf[0] == 'b' || buf[0] == 'A' || buf[0] == 'B') {
            machine[0] = toupper(buf[0]);
            return 0;
        } else {
            ret = -1;
            printf("Invalid input\n");
        }
    } while(ret == -1);

    return 0;
}

static int input_sim_board_num(uint8_t *num)
{
    uint64_t tmp;
    int ret = 0;

    if (!num) {
        return -1;
    }

    do {
        /* Get the number of SIM board && check it */
        if (0 != input_num("Please input SIM Board Number(1/2)", &tmp)) {
            ret = -1;
            continue;
        }

        if (tmp != 1 && tmp != 2) {
            ret = -1;
            printf("Invalid Board Number\n");
            continue;
        }

        if (is_board_num_valid(tmp) == 0) {
            ret = -1;
            printf("Invalid Board Number\n");
            continue;
        }

        break;
    } while(ret == -1);

    *num = (uint8_t)tmp&0xff;

    return 0;
}

static int input_product_sn(char *sn, uint8_t len)
{
    char tmp[MAX_STR_LENGTH];
    int ret = 0;

    if (!sn) {
        return -1;
    }

    do {
        if (0 != input_str("Please input the Product SN", tmp, sizeof(tmp))) {
            ret = -1;
            continue;
        }

        if (is_product_sn_valid(tmp) == 0) {
            printf("Illegal Product SN\n");
            ret = -1;
            continue;
        }

        break;
    } while(ret == -1);

    strncpy0(sn, tmp, len);

    return 0;
}

static int input_board_sn(const char *board, char *sn, uint8_t len)
{
    char buf[MAX_STR_LENGTH];
    char tmp[MAX_STR_LENGTH];
    int ret = 0;

    if (!board || !sn) {
        return -1;
    }

    do {
        snprintf(buf, sizeof(buf), "Please input the %s SN", board);

        if (0 != input_str(buf, tmp, sizeof(tmp))) {
            ret = -1;
            continue;
        }

        if(is_board_sn_valid(tmp) == 0) {
            printf("Illegal CCM SN\n");
            ret = -1;
            continue;
        }

        break;
    } while(ret == -1);

    strncpy0(sn, tmp, len);

    return 0;
}

static int input_baudrate(int *baud)
{
    int baudrate_list[] = {
        9600, 19200, 38400, 57600, 115200, 230400, 460800, 576000, 921600
    };
    const int baudrate_count = sizeof(baudrate_list)/sizeof(int);
    char str[MAX_STR_LENGTH];
    int ret = 0;

    uint64_t tmp;
    int i;

    if (!baud) {
        return -1;
    }

    do {
        printf("Please select baudrate:\n");
        for (i = 0; i < baudrate_count; i++) {
            printf("(%d) %d\n", i+1, baudrate_list[i]);
        }

        snprintf(str, MAX_STR_LENGTH, "Please select a baudrate(1 ~ %d)", baudrate_count);
        if (0 != input_num(str, &tmp)) {
            puts("");
            ret = -1;
            continue;
        }

        if ((tmp < 1) || (tmp > baudrate_count)) {
            printf("Invalid baudrate\n\n");
            ret = -1;
            continue;
        }

        break;
    } while (ret == -1);

    *baud = baudrate_list[tmp-1];

    return 0;
}

/******************************************************************************
 * NAME:
 *      user_ack
 *
 * DESCRIPTION:
 *      Get user's ack.
 *
 * PARAMETERS:
 *      default option for user's ack.
 *
 * RETURN:
 *      1 - Yes
 *      0 - No
 ******************************************************************************/
static int user_ack(char *hint)
{
    char buf[2];
    int ret;

    do {
        printf("%s [Y/N] ", hint);

        if (0 != input_str("", buf, sizeof(buf))){
            ret = -1;
            continue;
        }

        switch (buf[0]) {
            case 'y':
            case 'Y':
                ret = 1;
                break;
            case 'n':
            case 'N':
                ret = 0;
                break;
            default:
                ret = -1;
        }

        if (ret == -1) {
            printf("Invalid input\n");
            continue;
        }
    } while(ret == -1);

    return ret;
}

static int input_tester(char *tester, uint8_t len)
{
    int ret = 0;
    char buf[MAX_STR_LENGTH];

    if (!tester) {
        return -1;
    }

    do {
        if (0 != input_str("Please input the Tester", buf, sizeof(buf))) {
            ret = -1;
            continue;
        }

        break;
    } while (ret == -1);

    strncpy0(tester, buf, len);

    return 0;
}

static int input_test_duration(uint64_t *duration)
{
    int ret = 0;
    uint64_t tmp;

    if (!duration) {
        return -1;
    }

    do {
        if (0 != input_num("Please input the Test time(minutes)", &tmp)) {
            ret = -1;
            continue;
        }

        break;
    } while (ret == -1);

    *duration = tmp;

    return 0;
}

static int input_hsm_loop(uint64_t *loop)
{
    int ret = 0;
    uint64_t tmp;

    if (!loop) {
        return -1;
    }

    do {
        if (0 != input_num("Please input HSM test loop", &tmp)) {
            ret = -1;
            continue;
        }

        //Compare HSM switch time with test duration
        if (tmp*(WAIT_IN_MS*2/1000) >= g_duration*60) {
            printf("Invalid HSM test loop, HSM switch duration (%llus)"
                    " must smaller than total test duration (%llus)\n",
                    tmp*(WAIT_IN_MS*2/1000), g_duration*60);
            ret = -1;
            continue;
        }

        break;
    } while (ret == -1);

    *loop = tmp;

    return 0;
}

/******************************************************************************
 * NAME:
 *      get_parameter
 *
 * DESCRIPTION:
 *      Init parameters by the input from user.
 *
 * PARAMETERS:
 *      None
 *
 * RETURN:
 *      0  - OK
 *      <0 - error
 ******************************************************************************/
int get_parameter(void)
{
    int i;
    int eth_num;

    if (g_dev_sku == SKU_CIM) {
        eth_num = TESC_NUM / 2;
    } else {
        eth_num = TESC_NUM;
    }

    /* Get the information of tester */
    if (0 != input_tester(g_tester, sizeof(g_tester)))
        return -1;

    /* Get the test time */
    if (0 != input_test_duration(&g_duration))
        return -1;

    /* Get the machine A or B */
    if (0 != input_machine(&g_machine))
        return -1;

    /* Get the system test mode. */
    g_test_mode = user_ack("Test all modules?");

    /* Run all modules or serval modules. */
    if (g_test_mode == 1) {
        /* Get the product SN */
        if (0 != input_product_sn(g_product_sn, sizeof(g_product_sn)))
            return -1;

        /* Get HSM test loop */
        if (0 != input_hsm_loop(&g_hsm_test_loop))
            return -1;

        if (g_dev_sku != SKU_CIM) {

            //SIM
            /* Get the number of SIM board && check it */
            if (0 != input_sim_board_num(&g_board_num))
                return -1;

            //Baudrate
            if (0 != input_baudrate(&g_baudrate))
                return -1;
        }

        //NIM
        /* Config all ethernet ports to 'Y' */
        for (i = 0; i < eth_num; i++) {
            g_nim_test_eth[i] = 1;
        }
    } else if (g_test_mode == 0) {  /* Get separate modules setting for running. */
        //CCM
        if (0 != input_board_sn((g_dev_sku == SKU_CIM)?"CIM":"CCM",
                    g_ccm_sn, sizeof(g_ccm_sn)))
            return -1;

        g_test_cpu = user_ack("Test CPU?");
        g_test_led = user_ack("Test LED?");
        g_test_mem = user_ack("Test MEM?");

        if (g_dev_sku != SKU_CIM) {
            //SIM
            g_test_sim = user_ack("Test SIM?");
            /* Get the SIM board number and the baudrate of serial */
            if (g_test_sim == 1) {
                /* Get the number of SIM board && check it */
                if (0 != input_sim_board_num(&g_board_num))
                    return -1;

                for (i = 0; i < g_board_num; i++) {
                    if (0 != input_board_sn("SIM", g_sim_sn[i], sizeof(g_sim_sn[i])))
                        return -1;
                }

                //Baudrate
                if (0 != input_baudrate(&g_baudrate))
                    return -1;
            }
        }

        //NIM
        if (g_dev_sku == SKU_CIM) {
            g_test_nim = user_ack("Test ETH?");
        } else {
            g_test_nim = user_ack("Test NIM?");
        }
        /* Get NIM modules setting for which ports should be tested */
        if (g_test_nim == 1) {
            if (g_dev_sku != SKU_CIM) {
                if (0 != input_board_sn("NIM", g_nim_sn, sizeof(g_nim_sn)))
                    return -1;
            }

            memset(g_nim_test_eth, 0, sizeof(g_nim_test_eth));
            for (i = 0; i < eth_num; i++) {
                char buf[MAX_STR_LENGTH];
                snprintf(buf, sizeof(buf), "Test eth%d?", i);
                g_nim_test_eth[i] = user_ack(buf);
            }
        }

        //HSM
        g_test_hsm = user_ack("Test HSM?");
        if(g_test_hsm == 1) {
            if (0 != input_board_sn("HSM", g_hsm_sn, sizeof(g_hsm_sn)))
                return -1;

            /* Get HSM test loop */
            if (0 != input_hsm_loop(&g_hsm_test_loop))
                return -1;
        }

        if (g_dev_sku == SKU_CCM_MSM) {
            //MSM
            g_test_msm = user_ack("Test MSM?");
            if(g_test_msm == 1) {
                if (0 != input_board_sn("MSM", g_msm_sn, sizeof(g_msm_sn)))
                    return -1;
            }
        }
    }

    if (g_dev_sku != SKU_CIM) {
        //If use two SIM board, disable CPU/memory test
        if (g_test_sim && g_board_num == 2) {
            g_test_cpu = 0;
            g_test_mem = 0;
        }
    }

    if (g_test_hsm) {
        g_hsm_switching = 1;
    }

    DBG_PRINT("Tester: %s, Product SN: %s, Test time: %llu\n",
        g_tester, g_product_sn, g_duration);

    DBG_PRINT("machine: %c\n", g_machine);

    DBG_PRINT("Test All modules: %d\n", g_test_mode);

    DBG_PRINT("cpu: %d, sim: %d, nim: %d, hsm: %d, msm: %d, led: %d, mem: %d\n",
            g_test_cpu, g_test_sim, g_test_nim, g_test_hsm, g_test_msm, g_test_led, g_test_mem);

    DBG_PRINT("ccm sn: %s\n", g_ccm_sn);
    DBG_PRINT("sim sn: %s\n", g_sim_sn[0]);
    DBG_PRINT("sim sn: %s\n", g_sim_sn[1]);
    DBG_PRINT("nim sn: %s\n", g_nim_sn);
    DBG_PRINT("msm sn: %s\n", g_msm_sn);
    DBG_PRINT("hsm sn: %s\n", g_hsm_sn);

    DBG_PRINT("sim board num: %d, baudrate: %d\n", g_board_num, g_baudrate);
    DBG_PRINT("hsm test loop: %llu\n", g_hsm_test_loop);

    DBG_PRINT("nim test port: ");
    for (i = 0; i < TESC_NUM; i++) {
        DBG_PRINT(" eth%d: %u,", i, g_nim_test_eth[i]);
    }
    DBG_PRINT("\n");

    return 0;
}

/******************************************************************************
 * NAME:
 *      get_parameter
 *
 * DESCRIPTION:
 *      Init parameters by the input from user.
 *
 * PARAMETERS:
 *      argc    - argument number
 *      argv    - argument vector
 *
 * RETURN:
 *      0  - OK
 *      <0 - error
 ******************************************************************************/
int parse_params(int argc, char **argv)
{
    //Check parameter
    if (argc == 1) {
        g_dev_sku = SKU_CCM;
    } else if (argc == 2){
        if (strcmp("-msm", argv[1]) == 0) {
            g_dev_sku = SKU_CCM_MSM;
        } else if (strcmp("-cim", argv[1]) == 0) {
            g_dev_sku = SKU_CIM;
        } else {
            return -EINVAL;
        }
    } else {
        return -EINVAL;
    }

    switch (g_dev_sku) {
        case SKU_CCM: //without MSM
            g_test_msm = 0;
            break;
        case SKU_CIM: //without SIM, NIM, MSM
            g_test_sim = 0;
            g_test_msm = 0;
            break;
        case SKU_CCM_MSM:
            break;
        default:
            return -EINVAL;
    }

    return 0;
}
