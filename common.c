/******************************************************************************
 *
 * FILENAME:
 *     common.c
 *
 * DESCRIPTION:
 *     Define some useful functions
 *
 * REVISION(MM/DD/YYYY):
 *     08/16/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"
#include "term.h"


//Return code for system
#define DIAG_SYS_RC(x)    ((WTERMSIG(x) == 0)?(WEXITSTATUS(x)):-1)

void kill_process(char *name)
{
    char kill[260];

    sprintf(kill, "pkill -2 -f %s", name);
    system(kill);
}


/******************************************************************************
 * NAME:
 *      ser_open
 *
 * DESCRIPTION: 
 *      Open a serial port, and return the fd of it.
 *
 * PARAMETERS:
 *      dev -  The serial port to open
 *
 * RETURN:
 *      The fd of serial port
 ******************************************************************************/
int ser_open(char *dev)
{
    int fd;
    int baud = 115200;
    int databits = 8;
    int parity = 0;
    int stopbits = 1;

    fd = tc_init(dev);
    if (fd == -1) {
        return -1;
    }

    /* Set baudrate */
    tc_set_baudrate(fd, baud);

    /* Set databits, stopbits, parity ... */
    if (tc_set_port(fd, databits, stopbits, parity) == -1) {
        tc_deinit(fd);
        return -1;
    }

    return fd;
}


/******************************************************************************
 * NAME:
 *      send_sync_data
 *
 * DESCRIPTION: 
 *      Send a DATA_SYNC char to the serial port.
 *
 * PARAMETERS:
 *      fd - The fd of serial port
 *
 * RETURN:
 *      Bytes wrote
 ******************************************************************************/
static int send_sync_data(int fd)
{
    char buf[2] = {DATA_SYNC, 0};

    while (g_running) {
        int size = write(fd, buf, 1);
        if (size > 0) {
            return 1;
        }
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      recv_sync_data
 *
 * DESCRIPTION: 
 *      Read data from serail port and search the DATA_SYNC char
 *
 * PARAMETERS:
 *      fd - The fd of serail port
 *
 * RETURN:
 *      1 - Reveived the DATA_SYNC char
 *      0 - Not received.
 ******************************************************************************/
static int recv_sync_data(int fd)
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    int size;
    char buf[64];

    /* Watch fd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    /* Wait up to five seconds. */
    tv.tv_sec = 0;
    tv.tv_usec = 5000;

    retval = select(fd+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */
    if (retval > 0) {
        /* FD_ISSET(0, &rfds) will be true. */
        DBG_PRINT("Data is available now.\n");

        memset(buf, 0, sizeof(buf));
        size = read(fd, buf, sizeof(buf)-1);
        if (size > 0) {
            if (strchr(buf, DATA_SYNC)) {
                return 1;
            }
        }
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      wait_other_side_ready
 *
 * DESCRIPTION: 
 *      Wait the test program on the other side(machine) to be ready.
 *
 * PARAMETERS:
 *      None 
 *
 * RETURN:
 *      1 - Ready.
 *      0 - Not Ready(exit the program).
 ******************************************************************************/
int wait_other_side_ready(void)
{
    int rc = 0;

    int fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        printf("Open the serial port of CCM fail!\n");
        return 0;
    }

    if (g_machine == 'A') {
        while (g_running) {
            sleep(1);
            if (send_sync_data(fd) < 1) {
                continue;
            }

            if (recv_sync_data(fd)) {
                rc = 1;
                break;
            }
        }
    } else {
        while (g_running) {
            sleep(1);
            if (recv_sync_data(fd)) {
                if (send_sync_data(fd) < 1) {
                    continue;
                }

                rc = 1;
                break;
            }
        }
    }
    close(fd);
    sleep(3);

    return rc;
}


/******************************************************************************
 * NAME:
 *      sleep_ms
 *
 * DESCRIPTION:
 *      Sleep some number of milli-seconds.
 *
 * PARAMETERS:
 *      ms - Time in milliseconds.
 *
 * RETURN:
 *      0 - OK, Other - interrupted or error
 ******************************************************************************/
int sleep_ms(unsigned int ms)
{
    struct timespec req;

    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    if (nanosleep(&req, NULL) < 0) {
        return -1;
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      is_exe_exist
 *
 * DESCRIPTION: 
 *      Is the EXE file exist.
 *
 * PARAMETERS:
 *      exe - File name or full path of EXE file
 *
 * RETURN:
 *      1 - Exist
 *      0 - Non-exist
 ******************************************************************************/
int is_exe_exist(char *exe)
{
    char cmd[1024];
    int rc;

    if (exe == NULL) {
        return 0;
    }

    snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null", exe);
    rc = DIAG_SYS_RC(system(cmd));
    if (rc == 0) {
        return 1;
    } else {
        return 0;
    }
}
