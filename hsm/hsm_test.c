/******************************************************************************
*
* FILENAME:
*     hsm_test.c
*
* DESCRIPTION:
*     Define functions for HSM test.
*
* REVISION(MM/DD/YYYY):
*     07/29/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version 
*
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <sys/io.h>
#include "term.h"
#include "hsm_test.h"

void hsm_print_status();
void hsm_print_result(int fd);
void *hsm_test(void *args);
static void tc_set_rts_casco(int fd, char enabled);
static int tc_get_cts_casco(int fd);
static int send_packet(int fd);
static int open_port();


test_mod_t test_mod_hsm = {
    .run = 1,
    .pass = 1,
    .name = "hsm",
    .log_fd = -1,
    .test_routine = hsm_test,
    .print_status = hsm_print_status,
    .print_result = hsm_print_result
};

int reverse = 0;


/* The size of packet to send */
#define PACKET_SIZE     4

/* The packet to send */
char g_packet[PACKET_SIZE];



void hsm_print_status()
{
    printf("%-*s %s\n", COL_FIX_WIDTH, "HSM", STR_MOD_OK);
}

void hsm_print_result(int fd)
{
    if (test_mod_hsm.pass) {
        write_file(fd, "HSM: PASS\n");
    } else {
        write_file(fd, "HSM: FAIL\n");
    }
}


void *hsm_test(void *args)
{
    int log_fd = test_mod_hsm.log_fd;
    int fd;
    time_t old_time = 0, cur_time;
    int switch_count = 0;
    int old_cts, cur_cts;

    log_print(log_fd, "Begin test!\n\n");

    /* Init the packet to be sent */
    int i;
    for (i = 0; i < PACKET_SIZE; ++i) {
        g_packet[i] = (i % 64) + 0x30;
    }

    fd = open_port();
    if (fd < 0) {
        log_print(log_fd, "open mac %c at %s is Failed!\n", g_machine, CCM_SERIAL_PORT);
        test_mod_hsm.pass = 0;
        pthread_exit(NULL);
    } else {
        log_print(log_fd, "open mac %c at %s is Successful!\n", g_machine, CCM_SERIAL_PORT);
    }
    sleep(2);

    //Get original CTS status
    old_cts = tc_get_cts_casco(fd);

    if (!reverse) {
        tc_set_rts_casco(fd, TRUE);
    } else {
        tc_set_rts_casco(fd, FALSE);
    }

    if (g_machine == 'A') {
        while (g_running) {
            if (send_packet(fd) < 0) {
                log_print(log_fd, "Send packet error\n");
                test_mod_hsm.pass = 0;
            }

            cur_cts = tc_get_cts_casco(fd);
            if (cur_cts != old_cts) {
                switch_count++;
                old_cts = cur_cts;
            }

            cur_time = time(NULL);
            if (cur_time > (old_time + 1)) {
                log_print(log_fd, "COMA-RTS -> 1     OK!\n");

                cur_cts = tc_get_cts_casco(fd);
                if (!cur_cts) {
                    log_print(log_fd, "CTS=%d, A is HOST\n", !cur_cts);
                } else {
                    log_print(log_fd, "CTS=%d, A is SLAVE\n", !cur_cts);
                }

                log_print(log_fd, "Switch count: %d\n", switch_count);

                old_time = cur_time;
            }

            sleep_ms(100);
        }
    } else {
        while (g_running) {
            if (send_packet(fd) < 0) {
                log_print(log_fd, "Send packet error\n");
                test_mod_hsm.pass = 0;
            }

            cur_cts = tc_get_cts_casco(fd);
            if (cur_cts != old_cts) {
                switch_count++;
                old_cts = cur_cts;
            }

            cur_time = time(NULL);
            if (cur_time > (old_time + 1)) {
                log_print(log_fd, "COMB-RTS -> 1     OK!\n");

                cur_cts = tc_get_cts_casco(fd);
                if (!cur_cts) {
                    log_print(log_fd, "CTS=%d, B is SLAVE\n", !cur_cts);
                } else {
                    log_print(log_fd, "CTS=%d, B is HOST\n", !cur_cts);
                }

                log_print(log_fd, "Switch count: %d\n", switch_count);

                old_time = cur_time;
            }

            sleep_ms(100);
        }
    }

    tc_deinit(fd);

    log_print(log_fd, "Test end\n\n");
    pthread_exit(NULL);
}


static void tc_set_rts_casco(int fd, char enabled)
{
    unsigned char flags = TIOCM_RTS;

    if (enabled == TRUE) {
        ioctl(fd, TIOCMBIC, &flags);
    } else {
        ioctl(fd, TIOCMBIS, &flags);
    }
}

static int tc_get_cts_casco(int fd)
{
    uint8_t reg_val;
    uint16_t m_nBasePort = 0x2f8;
    int nReg = 0x06;

    if (ioperm (m_nBasePort, 7, 1)) {
        //perror ("ioperm");
        return -2;
    }

    reg_val = inb (m_nBasePort + nReg);

    if (ioperm (m_nBasePort, 7, 0)) {
        //perror ("ioperm");
        return -2;
    }

    return (reg_val & 0x10) ? 1 : 0;
}


/* Send data */
static int send_packet(int fd)
{
    int rc = 0;
    char *buf = g_packet;
    int bytes_left = PACKET_SIZE;
    int bytes_sent = 0;

    while (bytes_left > 0) {
        int size = write(fd, &buf[bytes_sent], bytes_left);
        if (size >= 0) {
            bytes_left -= size;
            bytes_sent += size;
        } else {
            //perror("write error");
            rc = -1;
            break;
        }
    }

    return rc;
}


/* Open and setup serial port */
static int open_port()
{
    int fd;
    char *dev = CCM_SERIAL_PORT;
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
