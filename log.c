/******************************************************************************
 *
 * FILENAME:
 *     log.c
 *
 * DESCRIPTION:
 *     Define functions to print message to log file
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng(shengkui.leng@advantech.com.cn)
 *     - Initial version
 ******************************************************************************/
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "log.h"

#define MSG_BUF_SIZE        2048


const char g_log_dir_path[] = "./";



/******************************************************************************
 * NAME:
 *      get_current_time
 *
 * DESCRIPTION:
 *      Get the current time
 *
 * PARAMETERS:
 *      p - Output the time.
 *
 * RETURN:
 *      The pointer of time strcuture
 ******************************************************************************/
struct tm* get_current_time(struct tm *p)
{
    struct timeval timep;

    gettimeofday(&timep, NULL);
    localtime_r(&timep.tv_sec, p);
    return p;
}


/******************************************************************************
 * NAME:
 *      log_print
 *
 * DESCRIPTION:
 *      Print a message string to a log file
 *
 * PARAMETERS:
 *      fd    - The fd of log file
 *      Other arguments are the same as printf
 *
 * RETURN:
 *      None
 ******************************************************************************/
void log_print(int fd, char *format, ...)
{
    va_list args;
    struct tm p;
    char line[MSG_BUF_SIZE];
    char dbg_msg[MSG_BUF_SIZE];

    if (fd < 0) {
        printf("Invalid log file\n");
        return;
    }

    va_start(args, format);
    vsnprintf(dbg_msg, MSG_BUF_SIZE, format, args);
    va_end(args);

    /* print to log file */
    get_current_time(&p);
    snprintf(line, sizeof(line), "| %d-%02d-%02d %02d:%02d:%02d | %s",
        (1900 + p.tm_year), (1 + p.tm_mon), p.tm_mday,
        p.tm_hour, p.tm_min, p.tm_sec,
        dbg_msg
        );

    int rc = write(fd, line, strlen(line));
    if (rc < 0) {
        printf("Write log error\n");
    }
}


/******************************************************************************
 * NAME:
 *      log_init
 *
 * DESCRIPTION:
 *      Build the filename of log file
 *
 * PARAMETERS:
 *      log_file - Output the filename of log file.
 *      prefix   - Prefix string of the filename of log file.
 *      dir      - The directory where the log shall be placed.
 *
 * RETURN:
 *      >=0 - fd of the log file
 *      <0  - error
 ******************************************************************************/
int log_init(char *log_file, char *prefix, char *dir)
{
    struct tm p;
    char date_time[64];
    int fd;

    get_current_time(&p);

    /* Build date_time for filename */
    sprintf(date_time, "%d%02d%02d_%02d%02d%02d", (1900 + p.tm_year),
        (1 + p.tm_mon), p.tm_mday, p.tm_hour, p.tm_min, p.tm_sec);

    /* Generate log file name */
    if (dir) {
        /* Create the dir of log file */
        struct stat s;
        if (stat(dir, &s) != 0) {
            mkdir(dir, 0755);
        }

        sprintf(log_file, "%s/%s_%s.log", dir, prefix, date_time);
    } else {
        sprintf(log_file, "%s_%s.log", prefix, date_time);
    }

    fd = open(log_file, O_RDWR | O_CREAT | O_APPEND,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    return fd;
}


/******************************************************************************
 * NAME:
 *      log_close
 *
 * DESCRIPTION: 
 *      Close the opened log file.
 *
 * PARAMETERS:
 *      fd - The fd of log file.
 *
 * RETURN:
 *      None
 ******************************************************************************/
void log_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
