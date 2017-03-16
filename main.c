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
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "main.h"

/* Version of the program */
#define PROGRAM_VERSION     "0.10"

static int g_runned_minute = 0;
static int g_runned_second = 0;

/* Full path of the directory where the log/error/report files put */
static char g_log_dir[PATH_MAX];
static char g_error_dir[PATH_MAX];
static char g_report_dir[PATH_MAX];

/* Full path of program */
char g_progam_path[PATH_MAX];

/* An array to manage all test modules. */
static test_mod_t *g_test_module[MAX_MOD_COUNT] = {0, };

/* Index of modules in the array of g_test_module */
static int mod_index = 0;

static void print_usage(char *name)
{
    printf("LiRC-ITEST v"PROGRAM_VERSION"\n");

    printf(APPNAME_CCM
            ": Integration Test Utility for CCM on LiRC-3\n"
            "  -msm\n"
            "    Run with MSM test\n"
            "  -cim\n"
            "    Run test on CIM\n");
}

int main(int argc, char **argv)
{
    int rc = 0;
    int i;
    struct tm tm_start;
    struct tm tm_end;
    char report_file[PATH_MAX];
    int report_fd;

    if (0 != parse_params(argc, argv)) {
        print_usage(argv[0]);
        return -1;
    }

    /* Get some input from user */
    if (get_parameter() < 0) {
        return -1;
    }

    install_sig_handler();

    printf("Wait the other side to be ready...\n");
    if (g_dev_sku == SKU_CIM) {
        if (!wait_other_side_ready_eth()) {
            printf("The other side is not ready!\n");
            return -1;
        }
    } else {
        if (!wait_other_side_ready()) {
            printf("The other side is not ready!\n");
            return -1;
        }
    }

    printf("OK\n");

    time_t time_start = time(NULL);
    get_current_time(&tm_start);
    init_path(&tm_start);

    //Generate report file
    report_fd = log_init(report_file, "report", g_report_dir);

    /* Start test modules */
    mod_index = 0;
    start_test_module(&test_mod_led);
    start_test_module(&test_mod_hsm);
    start_test_module(&test_mod_msm);
    start_test_module(&test_mod_nim);
    start_test_module(&test_mod_sim);
    start_test_module(&test_mod_cpu);
    start_test_module(&test_mod_mem);

    /* Set test duration. */
    set_timeout(g_duration*60);

    //IF msm test wasn't enabled, start quit sync in main thread
    if ((!g_test_msm || !g_test_hsm) && g_dev_sku != SKU_CIM) {
        receive_exit_sync();
    }

    /* Print the status of test module */
    while (g_running) {
        sleep(3);

        for (i = 0; i < mod_index; i++) {
            if (g_test_module[i] && g_test_module[i]->run) {
                g_test_module[i]->print_status();
                sleep(1);
            }
        }

        printf("\n");
    }

    /* Wait the thread of test module to exit */
    for (i = 0; i < mod_index; i++) {
        if (g_test_module[i] && g_test_module[i]->run) {
            pthread_join(g_test_module[i]->pid, NULL);
            if (g_test_module[i]->pass == 0) {
                move_log_to_error(g_test_module[i]->log_file);
            }
        }
    }

    /* Compute runned time for test promgam. */
    get_current_time(&tm_end);
    time_t time_end = time(NULL);
    time_t time_total = time_end - time_start;
    if ((time_start != -1) || (time_end != -1)) {
        g_runned_minute = time_total / 60;
        g_runned_second = time_total % 60;
        if (g_runned_minute >= g_duration) {
            g_runned_minute = g_duration;
            g_runned_second = 0;
        } else if (time_total < 0) {
            g_runned_minute = 0;
            g_runned_second = 0;
        }
    }

    /* Generate test report and print to stdout */
    generate_report(report_fd, report_file, &tm_start, &tm_end);
    return rc;
}

void sig_handler(int signo)
{
    g_running = 0;
    send_exit_sync();
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
 *      Set test duration(start a timer to end the test after a period time).
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
    *pos = '\0';

    /*
     * Return the length of the path.
     */
    return (size_t)(pos - path_buf);
}

static int make_dir(char *path)
{
    struct stat s;

    if (path == NULL) {
        printf("Path is NULL\n");
        return -1;
    }

    if (stat(path, &s) == 0) {
        return 0;
    }

    if (0 != mkdir(path, 0755)) {
        return -1;
    } else {
        return 0;
    }
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
int init_path(struct tm *p)
{
    char log_base[PATH_MAX];
    char ts[PATH_MAX];

    //According the requirement of customer
    //The log path will be PROGRAM_PATH/log/YYYYMMDD_HHDDSS/
    snprintf(ts, sizeof(ts), "%d%02d%02d_%02d%02d%02d",
            (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
            p->tm_hour, p->tm_min, p->tm_sec);

    if (get_exe_path(g_progam_path, sizeof(g_progam_path)-1) <= 0) {
        printf("Get path of program error\n");
        return -1;
    }

    snprintf(log_base, sizeof(log_base), "%s/%s", g_progam_path, "log");
    snprintf(g_log_dir, sizeof(g_log_dir), "%s/%s_%c", log_base, ts, g_machine);
    snprintf(g_error_dir, sizeof(g_error_dir), "%s/%s", log_base, "error");
    snprintf(g_report_dir, sizeof(g_report_dir), "%s/%s", log_base, "report");

    if (make_dir(log_base) != 0) {
        return -1;
    }
    if (make_dir(g_log_dir) != 0) {
        return -1;
    }
    if (make_dir(g_error_dir) != 0) {
        return -1;
    }
    if (make_dir(g_report_dir) != 0) {
        return -1;
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
    symlink(log_file, new_file);
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
    /*
    char value[MAX_LINE_LENGTH];
    */

    if (mod_index >= MAX_MOD_COUNT) {
        printf("The number of modules is more than the max counter\n");
        return -1;
    }

    /* Init data structure of test module */
    g_test_module[mod_index++] = pmod;

    /* Does this test module need to be run or not? */
    if (strcmp(pmod->name, "cpu") == 0) {
        pmod->run = g_test_cpu;
    } else if (strcmp(pmod->name, "sim") == 0) {
        pmod->run = g_test_sim;
    } else if (strcmp(pmod->name, "nim") == 0) {
        pmod->run = g_test_nim;
    } else if (strcmp(pmod->name, "hsm") == 0) {
        pmod->run = g_test_hsm;
    } else if (strcmp(pmod->name, "msm") == 0) {
        pmod->run = g_test_msm;
    } else if (strcmp(pmod->name, "led") == 0) {
        pmod->run = g_test_led;
    } else if (strcmp(pmod->name, "mem") == 0) {
        pmod->run = g_test_mem;
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
 *      Generate a test report and print to stdout.
 *
 * PARAMETERS:
 *      fd: Report      file descriptor
 *      report_file:    Report filename
 *      tm_start:       test start time
 *      tm_end:         test end time
 *
 * RETURN:
 *      None
 ******************************************************************************/
void generate_report(int fd, char *report_file, struct tm *tm_start, struct tm *tm_end)
{
    int i;
    char ts_start[MAX_STR_LENGTH];
    char ts_end[MAX_STR_LENGTH];

    write_file(fd, "================== Test Report ==================\n");
    write_file(fd, "%s (%c)\n", (g_dev_sku == SKU_CIM)?"CIM":"CCM", g_machine);
    write_file(fd, "Tester: %s\n", g_tester);

    if (g_test_mode) {
        write_file(fd, "Product SN: %s\n", g_product_sn);
    } else {
        write_file(fd, "%s SN: %s\n", (g_dev_sku == SKU_CIM)?"CIM":"CCM",  g_ccm_sn);

        if (g_test_hsm) {
            write_file(fd, "HSM SN: %s\n", g_hsm_sn);
        }

        if (g_test_msm) {
            write_file(fd, "MSM SN: %s\n", g_msm_sn);
        }

        if (g_dev_sku != SKU_CIM) {
            if (g_test_nim) {
                write_file(fd, "NIM SN: %s\n", g_nim_sn);
            }
        }

        if (g_test_sim) {
            write_file(fd, "SIM1 SN: %s\n", g_sim_sn[0]);
            if (g_board_num == 2) {
                write_file(fd, "SIM2 SN: %s\n", g_sim_sn[1]);
            }
        }
    }

    write_file(fd, "Start time: %s\n",
            get_timestamp_str(tm_start, ts_start, sizeof(ts_start)));
    write_file(fd, "End   time: %s\n",
            get_timestamp_str(tm_end, ts_end, sizeof(ts_end)));
    write_file(fd, "Test time(plan): %d min\n", g_duration);
    write_file(fd, "Test time(real): %d min %d sec\n", g_runned_minute, g_runned_second);
    write_file(fd, "\n");

    for (i = 0; i < mod_index; i++) {
        if (g_test_module[i] && g_test_module[i]->run) {
            g_test_module[i]->print_result(fd);
        }
    }

    log_close(fd);

    /* Print the report to stdout */
    dump_file(report_file);
}
