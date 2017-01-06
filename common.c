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
 *      1 - DATA_SYNC char has been sent
 *      0 - ERROR.
 ******************************************************************************/
static int send_sync_data(int fd, char snt_char)
{
    char buf[2] = {snt_char, 0};

    int size = write(fd, buf, 1);
    if (size > 0) {
        return 1;
    } else {
        return 0;
    }
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
 *      1  - Reveived a right SYNC char
 *      -1 - Reveived a wrong SYNC char
 *      0  - Not received.
 ******************************************************************************/
static int recv_sync_data(int fd, char rcv_char, char snt_char)
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    int size;
    char buf[64];

    /* Watch fd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    /* Wait up to 3 seconds. */
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    retval = select(fd+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */
    if (retval > 0) {
        /* FD_ISSET(0, &rfds) will be true. */
        DBG_PRINT("Data is available now.\n");

        memset(buf, 0, sizeof(buf));
        size = read(fd, buf, sizeof(buf)-1);
        if (size > 0) {
            /* Search the sync char. */
            if (strchr(buf, rcv_char)) {
                return 1;
            } else if (strchr(buf, snt_char)) {
                return -1;
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
 *      0 - Not Ready(fail).
 ******************************************************************************/
int wait_other_side_ready(void)
{
    int rc = 0;
    char snt_char, rcv_char;

    if (g_machine == 'A') {
        snt_char = DATA_SYNC_A;
        rcv_char = DATA_SYNC_B;
    } else /*if (g_machine == 'B')*/ {
        snt_char = DATA_SYNC_B;
        rcv_char = DATA_SYNC_A;
    }

    int fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        printf("Open the serial port of CCM fail!\n");
        return 0;
    }

    while (g_running) {
        /* Send sync request */
        if (send_sync_data(fd, snt_char) == 0) {
            continue;
        }

        /* Wait for response in five seconds. */
        int ch = recv_sync_data(fd, rcv_char, snt_char);
        if (ch == 1) {
            send_sync_data(fd, snt_char);
            rc = 1;
            break;
        } else if (ch == -1) {
            printf("Invalid machine!\n");
            rc = 0;
            break;
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

/* Send sync data on exit */
static void *send_exit_data(void *args)
{
    char snt_char;
    int i;

    if (g_machine == 'A') {
        snt_char = EXIT_SYNC_A;
    } else {
        snt_char = EXIT_SYNC_B;
    }

    int fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        pthread_exit(NULL);
    }

    //Send exit data 3 times
    for(i=0; i < 3; i++) {
        write(fd, &snt_char, 1);
        sleep_ms(500);
    }

    close(fd);

    pthread_exit(NULL);
}

/* Receive sync data on exit */
static void *receive_exit_data(void *args)
{
    char buf[64];

    /* Open serial port */
    int fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        pthread_exit(NULL);
    }

    while (g_running) {
        memset(buf, 0, sizeof(buf));
        int size = read(fd, buf, sizeof(buf)-1);
        if (size > 0) {
            if (strchr(buf, EXIT_SYNC_A) || strchr(buf, EXIT_SYNC_B)) {
                g_running = 0;
                break;
            }
        }

        sleep_ms(200);
    }

    close(fd);

    pthread_exit(NULL);
}

/******************************************************************************
 * NAME:
 *      send_exit_sync
 *
 * DESCRIPTION:
 *      Create thread to send exit sync data
 *
 ******************************************************************************/
void send_exit_sync(void)
{
    pthread_t pid;

    pthread_create(&pid, NULL, send_exit_data, NULL);
}

/******************************************************************************
 * NAME:
 *      receive_exit_sync
 *
 * DESCRIPTION:
 *      Create thread to receive exit sync data
 ******************************************************************************/
void receive_exit_sync(void)
{
    pthread_t pid;

    pthread_create(&pid, NULL, receive_exit_data, NULL);
}

/******************************************************************************
 * NAME:
 *      send_packet
 *
 * DESCRIPTION:
 *      Send serial data, used by hsm and sim module
 *
 * PARAMETERS:
 *      fd  - The fd of serial port
 *      buf - memory buffer
 *      len - buffer length
 *
 * RETURN:
 *      0   - success
 *      -1  - on error
 ******************************************************************************/
int send_packet(int fd, char *buf, uint8_t len)
{
    int rc = 0;
    int bytes_left = len;
    int bytes_sent = 0;

    if (!buf) {
        return -1;
    }

    while (bytes_left > 0) {
        int size = write(fd, &buf[bytes_sent], bytes_left);
        if (size >= 0) {
            bytes_left -= size;
            bytes_sent += size;
        } else {
            rc = -1;
            break;
        }
    }

    return rc;
}
