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
#include <libgen.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "common.h"
#include "led_test.h"

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
char g_config_file[PATH_MAX];

/* Full path of program */
char g_progam_path[PATH_MAX];

/* An array to manage all test modules. */
test_mod_t *g_test_module[MOD_COUNT] = {0, };


/* Function Prototype */
int get_parameter(void);
int load_config(char *config_file);
int install_sig_handler(void);
size_t get_exe_path(char *path_buf, size_t len);
int init_path(void);
int move_log_to_error(char *log_file);
int start_test_module(test_mod_t *pmod);
void generate_report(void);
void set_timeout(int sec);


int main(int argc, char **argv)
{
    int rc = 0;
    int i;

    if (argc != 1) {
        printf("LiRC-ITEST v%s\n", PROGRAM_VERSION);
        return -1;
    }

    get_parameter();

    install_sig_handler();

    init_path();

    load_config(g_config_file);

    start_test_module(&test_mod_led);
    set_timeout(g_duration);

    /* Print the status of test module */
    while (g_running) {
        for (i = 0; i < MOD_COUNT; i++) {
            if (g_test_module[i] && g_test_module[i]->run) {
                g_test_module[i]->print_status();
            }
        }
    }

    /* Wait the thread of test module to exit */
    for (i = 0; i < MOD_COUNT; i++) {
        if (g_test_module[i] && g_test_module[i]->run) {
            pthread_join(g_test_module[i]->pid, NULL);
            if (g_test_module[i]->pass == 0) {
                move_log_to_error(g_test_module[i]->log_file);
            }
        }
    }

    /* Generate test report */
    generate_report();
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
            printf("Config error\n");
            return 1;
        }
    }
    if (ini_get_key_value(config_file, "SIM", "baudrate", value) == 0) {
        g_baudrate = atoi(value);
        if (g_baudrate <= 0) {
            printf("Config error\n");
            return 1;
        }
    }
    if (ini_get_key_value(config_file, "NIM", "speed", value) == 0) {
        g_speed = atoi(value);
        if ((g_speed != 1000) && (g_speed != 100) && (g_speed != 10) ) {
            printf("Config error\n");
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

    if (sigaction(SIGALRM, &sa, NULL)) {
        printf("sigaction(SIGALRM) error\n");
        return -1;
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      set_timeout
 *
 * DESCRIPTION: 
 *      Set test duration.
 *
 * PARAMETERS:
 *      sec - Time duration(seconds)
 *
 * RETURN:
 *      None
 ******************************************************************************/
void set_timeout(int sec)
{
    struct itimerval itimer;
    
    /* Configure the timer to expire after sec(seconds)... */
    itimer.it_value.tv_sec = sec;
    itimer.it_value.tv_usec = 0;

    itimer.it_interval.tv_sec = 0;
    itimer.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &itimer, NULL);
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


/******************************************************************************
 * NAME:
 *      init_path
 *
 * DESCRIPTION: 
 *      Init file path of log/error/report and config file.
 *
 * PARAMETERS:
 *      None 
 *
 * RETURN:
 *      None
 ******************************************************************************/
int init_path(void)
{
    struct stat s;

    if (get_exe_path(g_progam_path, sizeof(g_progam_path)-1) <= 0) {
        printf("Get path of program error\n");
        return -1;
    }

    snprintf(g_log_dir, sizeof(g_log_dir), "%s/%s", g_progam_path, "log");
    snprintf(g_error_dir, sizeof(g_error_dir), "%s/%s", g_log_dir, "error");
    snprintf(g_report_dir, sizeof(g_report_dir), "%s/%s", g_log_dir, "report");
    snprintf(g_config_file, sizeof(g_config_file), "%s/%s", g_progam_path, "lirc.cfg");

    if (stat(g_log_dir, &s) != 0) {
        mkdir(g_log_dir, 0755);
    }
    if (stat(g_error_dir, &s) != 0) {
        mkdir(g_error_dir, 0755);
    }
    if (stat(g_report_dir, &s) != 0) {
        mkdir(g_report_dir, 0755);
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      move_log_to_error
 *
 * DESCRIPTION: 
 *      Move the log file to error
 *
 * PARAMETERS:
 *      log_file - The fullpath of log file. 
 *
 * RETURN:
 *      None
 ******************************************************************************/
int move_log_to_error(char *log_file)
{
    char new_file[PATH_MAX];

    snprintf(new_file, sizeof(new_file), "%s/error_%s", g_error_dir, basename(log_file));
    rename(log_file, new_file);
    return 0;
}


/******************************************************************************
 * NAME:
 *      start_test_module
 *
 * DESCRIPTION: 
 *      Start a test module.
 *
 * PARAMETERS:
 *      pmod - The data structure of a test module.
 *
 * RETURN:
 *      0 - OK, others - Error
 ******************************************************************************/
int start_test_module(test_mod_t *pmod)
{
    char value[MAX_LINE_LENGTH];

    /* Init data structure of test module */
    g_test_module[MOD_LED] = pmod;

    /* Does this test module need to be run or not? */
    if (ini_get_key_value(g_config_file, pmod->name, "run", value) == 0) {
        if (strcasecmp(value, "Y") == 0) {
            pmod->run = 1;
        } else {
            pmod->run = 0;
        }
    }

    if (pmod->run) {
        /* Create log file */
        int fd = log_init(pmod->log_file, pmod->name, g_log_dir);
        if (fd < 0) {
            printf("Log init error\n");
            return -1;
        }
        pmod->log_fd = fd;
        log_print(pmod->log_fd, "test module started\n");

        /* Start thread of test module */
        if (pthread_create(&pmod->pid, NULL, pmod->test_routine, NULL) != 0) {
            log_print(pmod->log_fd, "start test routine error\n");
            pmod->pass = 0;
            pmod->pid = -1;
        }
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      generate_report
 *
 * DESCRIPTION: 
 *      Generate a test report
 *
 * PARAMETERS:
 *      None 
 *
 * RETURN:
 *      None
 ******************************************************************************/
void generate_report(void)
{
    int i;
    char report_file[PATH_MAX];

    int fd = log_init(report_file, "report", g_report_dir);
    write_file(fd, "================== Test Report ==================\n");
    write_file(fd, "Tester: %s\n", g_tester);
    write_file(fd, "Product SN: %s\n", g_product_sn);
    write_file(fd, "Test time: %d\n", g_duration);

    for (i = 0; i < MOD_COUNT; i++) {
        if (g_test_module[i] && g_test_module[i]->run) {
            g_test_module[i]->print_result(fd);
        }
    }

    log_close(fd);
}
