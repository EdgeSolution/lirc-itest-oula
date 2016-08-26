/******************************************************************************
 *
 * FILENAME:
 *     log.h
 *
 * DESCRIPTION:
 *     Define some debug macros.
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

#define MSG_BUF_SIZE        2048


int log_init(char *log_file, char *prefix, char *dir);
void log_print(int fd, char *format, ...);
void log_close(int fd);
void write_file(int fd, char *format, ...);
void dump_file(char *file);


/* Print message to log file */
#define LOG_OUT(fd, format, args...)    log_print(fd, (char*)format, ## args);


/*
 * Macros for debug only
 */
#if DEBUG

#define DBG_TRACE(format, ...)  \
    do { \
        fprintf(stderr, "[%s@%s,%03d]: " format "\n", \
                __func__, __FILE__, __LINE__, ##__VA_ARGS__ ); \
    } while (0)

#define DBG_PRINT(format, ...)  \
    do { \
        fprintf(stderr, format, ##__VA_ARGS__ ); \
    } while (0)
#else

#define DBG_TRACE(format, ...)
#define DBG_PRINT(format, ...)

#endif


#endif /* _LOG_H_ */
