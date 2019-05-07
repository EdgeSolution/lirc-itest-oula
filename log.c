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
#include "version.h"

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

char* get_timestamp_str(struct tm *p, char *buf, size_t len)
{
    if (p == NULL || buf == NULL || len == 0) {
        return NULL;
    }

    snprintf(buf, len, "%d-%02d-%02d %02d:%02d:%02d",
        (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
        p->tm_hour, p->tm_min, p->tm_sec);

    return buf;
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
    char tm_str[MSG_BUF_SIZE];

    if (fd < 0) {
        printf("Invalid log file\n");
        return;
    }

    va_start(args, format);
    vsnprintf(dbg_msg, MSG_BUF_SIZE, format, args);
    va_end(args);

    /* print to log file */
    get_current_time(&p);
    snprintf(line, sizeof(line), "| %s | %s",
        get_timestamp_str(&p, tm_str, sizeof(tm_str)),
        dbg_msg);

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


/******************************************************************************
 * NAME:
 *      write_file
 *
 * DESCRIPTION:
 *      Write a string to a file
 *
 * PARAMETERS:
 *      fd    - The fd of a file
 *      Other arguments are the same as printf
 *
 * RETURN:
 *      None
 ******************************************************************************/
void write_file(int fd, char *format, ...)
{
    va_list args;
    char line[MSG_BUF_SIZE];

    if (fd < 0) {
        printf("Invalid fd\n");
        return;
    }

    va_start(args, format);
    vsnprintf(line, MSG_BUF_SIZE, format, args);
    va_end(args);

    int rc = write(fd, line, strlen(line));
    if (rc < 0) {
        printf("Write file error\n");
    }
}


/******************************************************************************
 * NAME:
 *      dump_file
 *
 * DESCRIPTION:
 *      Dump the content of a file to stdout
 *
 * PARAMETERS:
 *      fd    - The fd of a file
 *
 * RETURN:
 *      None
 ******************************************************************************/
void dump_file(char *file)
{
    char line[1024];
    memset(line, 0, sizeof(line));

    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        return;
    }

    printf("\n");
    while (fgets(line, sizeof(line)-1, fp)) {
        printf("%s", line);
    }
    printf("\n");

    fclose(fp);
}

/******************************************************************************
 * NAME:
 *      print_version
 *
 * DESCRIPTION:
 *      Print lirc-itest version into given fd
 *
 * PARAMETERS:
 *      fd    - The fd of a file
 *      name  - Test name
 *
 * RETURN:
 *      None
 ******************************************************************************/
void print_version(int fd, char *name)
{
    if (fd < 0) {
        printf("Invalid file descriptor\n");
        return;
    }

    if (name) {
        write_file(fd, "LiRC-itest v%s - %s test\n", PROGRAM_VERSION, name);
    } else {
        write_file(fd, "LiRC-itest v%s\n", PROGRAM_VERSION);
    }
}
