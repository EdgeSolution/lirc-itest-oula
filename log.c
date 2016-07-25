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
 *      date_time - None
 *
 * RETURN:
 *      The pointer of time strcuture
 ******************************************************************************/
struct tm* get_current_time()
{
    static struct tm *p;
    struct timeval timep;

    gettimeofday(&timep, NULL);
    p = localtime(&timep.tv_sec);
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
    struct tm *p;
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
    p = get_current_time();
    snprintf(line, sizeof(line), "| %d-%02d-%02d %02d:%02d:%02d | %s",
        (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
        p->tm_hour, p->tm_min, p->tm_sec,
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
 *      board    - Prefix string of the filename of log file.
 *
 * RETURN:
 *      fd of the log file
 ******************************************************************************/
int log_init(char *log_file, char *board)
{
    struct tm *p;
    char date_time[64];
    int fd;

    p = get_current_time();

    /* Build date_time for filename */
    sprintf(date_time, "%d%02d%02d_%02d%02d%02d", (1900 + p->tm_year),
        (1 + p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);

    /* Generate log file name */
    sprintf(log_file, "%s/%s_%s.log", g_log_dir_path, board, date_time);

    fd = open(log_file, O_RDWR | O_CREAT | O_APPEND,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd >= 0) {
        return fd;
    } else {
        return -1;
    }
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
