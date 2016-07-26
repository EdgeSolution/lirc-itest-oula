/******************************************************************************
*
* FILENAME:
*     main.c
*
* DESCRIPTION:
*     Integration test software for CPCI-1504(LiRC-3)
*
* REVISION(MM/DD/YYYY):
*     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version 
*
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include "common.h"

/* Version of the program */
#define PROGRAM_VERSION     "0.1"


/* Tester */
char g_tester[MAX_STR_LENGTH];

/* Product Serial Number */
char g_product_sn[MAX_STR_LENGTH];

/* Test duration(seconds) */
int g_duration = 600;

/* Machine A or B */
char g_machine = 'A';

/* The flag used to notify the program to exit */
int g_running = 1;

/* Board number (SIM) */
int g_board_num = 2;

/* Baudrate of serial port (SIM) */
int g_baudrate = 115200;

/* Speed of NIC port (NIM) */
int g_speed = 1000;


/* Full path of the directory where the log/error/report files put */
char g_log_dir[PATH_MAX];
char g_error_dir[PATH_MAX];
char g_report_dir[PATH_MAX];

/* Full path of the config file */
char g_config_path[PATH_MAX];

/* Full path of program */
char g_progam_path[PATH_MAX];

/* Prototype */
int get_parameter(void);
int load_config(char *config_file);
int install_sig_handler(void);
size_t get_exe_path(char *path_buf, size_t len);



int main(int argc, char **argv)
{
    int rc = 0;

    if (argc != 1) {
        printf("LiRC-ITEST v%s\n", PROGRAM_VERSION);
        return -1;
    }

    get_parameter();

    install_sig_handler();

    if (get_exe_path(g_progam_path, sizeof(g_progam_path)-1) <= 0) {
        printf("Get path of program error\n");
        return -1;
    }

    snprintf(g_log_dir, sizeof(g_log_dir), "%s/%s", g_progam_path, "log");
    snprintf(g_error_dir, sizeof(g_error_dir), "%s/%s", g_log_dir, "error");
    snprintf(g_report_dir, sizeof(g_report_dir), "%s/%s", g_log_dir, "report");
    snprintf(g_config_path, sizeof(g_config_path), "%s/%s", g_progam_path, "lirc.cfg");

    load_config(g_config_path);

    char log_file[260];
    int fd = log_init(log_file, "lirc", g_log_dir);
    if (fd < 0) {
        printf("Log init error\n");
        return -1;
    }
    log_print(fd, "begin\n");
    log_print(fd, "lirc-itest main program\n");
    log_print(fd, "end\n");
    log_close(fd);

    fd = log_init(log_file, "report", g_report_dir);
    log_print(fd, "report\n");
    log_close(fd);
    return rc;
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
    char buf[MAX_STR_LENGTH];
    char *p;

    /* Get the information of tester */
    printf("Please input the Tester:\n");
    if (fgets(buf, sizeof(buf)-1, stdin) <= 0) {
        printf("input error\n");
        return -1;
    }
    p = left_trim(right_trim(buf));
    strncpy(g_tester, p, sizeof(g_tester));

    /* Get the product SN */
    printf("Please input the Product SN:\n");
    if (fgets(buf, sizeof(buf)-1, stdin) <= 0) {
        printf("input error\n");
        return -1;
    }
    p = left_trim(right_trim(buf));
    strncpy(g_product_sn, p, sizeof(g_product_sn));

    /* Get the test time */
    printf("Please input the Test time(seconds):\n");
    if (fgets(buf, sizeof(buf)-1, stdin) <= 0) {
        printf("input error\n");
        return -1;
    }
    g_duration = atoi(buf);
    if (g_duration <= 0) {
        printf("input error\n");
        return -1;
    }

    /* Get the machine A or B */
    printf("Please input A or B for this machine:\n");
    if (scanf("%c", &g_machine) != 1) {
        printf("input error\n");
        return -1;
    }
    g_machine = toupper(g_machine);
    switch (g_machine) {
    case 'a':
    case 'A':
    case 'b':
    case 'B':
        break;

    default:
        printf("input error\n");
        return -1;
        break;
    }

    DBG_PRINT("Tester: %s, Product SN: %s, Test time: %d\n",
        g_tester, g_product_sn, g_duration);
    return 0;
}


/******************************************************************************
 * NAME:
 *      load_config
 *
 * DESCRIPTION:
 *      Load default parameters of diag function from config file.
 *
 * PARAMETERS:
 *      config_file - The config file.
 *
 * RETURN:
 *      0 - OK
 *      Others - Error
 ******************************************************************************/
int load_config(char *config_file)
{
    char value[MAX_LINE_LENGTH];

    if (ini_get_key_value(config_file, "SIM", "board_num", value) == 0) {
        g_board_num = atoi(value);
        if ((g_board_num < 1) || (g_board_num > 2)) {
            printf("Config file parse error\n");
            return 1;
        }
    }
    if (ini_get_key_value(config_file, "SIM", "baudrate", value) == 0) {
        g_baudrate = atoi(value);
        if (g_baudrate <= 0) {
            printf("Config file parse error\n");
            return 1;
        }
    }

    DBG_PRINT("board_num = %d, baudrate = %d\n", g_board_num, g_baudrate);
    return 0;
}


void sig_handler(int signo)
{
    g_running = 0;
}

int install_sig_handler(void)
{
    struct sigaction sa;

    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        printf("sigaction(SIGUSR1) error\n");
        return -1;
    }

    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        printf("sigaction(SIGTERM) error\n");
        return -1;
    }

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        printf("sigaction(SIGINT) error\n");
        return -1;
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      get_exe_path
 *
 * DESCRIPTION: 
 *      Find the path containing the currently running program executable.
 *
 * PARAMETERS:
 *      path_buf - The buffer to output the path.
 *      len      - The size of buffer.
 *
 * RETURN:
 *      The number of characters in the buffer, or -1 on error.
 ******************************************************************************/
size_t get_exe_path(char *path_buf, size_t len)
{
    char *pos;
    int rc;

    /* Read the target of /proc/self/exe. */
    rc = readlink("/proc/self/exe", path_buf, len);
    if (rc == -1) {
        printf("readlink error\n");
        return -1;
    }
    path_buf[rc] = 0;

    /*
     * Find the last occurrence of a forward slash, the path separator. Obtain
     * the directory containing the program by truncating the path after the
     * last slash.
     */
    pos = strrchr(path_buf, '/');
    if (pos == NULL) {
        printf("No '/' found in path!\n");
        return -1;
    }
    ++pos;
    *pos = '\0';

    /*
     * Return the length of the path.
     */
    return (size_t)(pos - path_buf);
}


