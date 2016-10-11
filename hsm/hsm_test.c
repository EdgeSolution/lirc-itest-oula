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
static void hsm_test_switch(fd, log_fd);
static void hsm_test_hold(int fd, int log_fd);
void *hsm_test(void *args);
static void tc_set_rts_casco(int fd, char enabled);
static int tc_get_cts_casco(int fd);
static int send_packet(int fd);
static void hsm_send(int fd, int log_fd);


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

#define SWITCH_TIME     10

/* The packet to send */
static char g_packet[PACKET_SIZE];

static uint64_t counter_fail = 0;

//
static uint8_t cur_cts;
static uint8_t cur_rts;

void hsm_print_status()
{
    printf("%-*s %s\n", COL_FIX_WIDTH, "HSM",
            (test_mod_hsm.pass == 1)?STR_MOD_OK:STR_MOD_ERROR);

    if (g_machine == 'A') {
        printf("RTS: %d CTS: %d A is %s\n", cur_rts, cur_cts, !cur_cts?"HOST":"SLAVE");
    } else {
        printf("RTS: %d CTS: %d B is %s\n", cur_rts, cur_cts, !cur_cts?"SLAVE":"HOST");
    }
}

void hsm_print_result(int fd)
{
    if (test_mod_hsm.pass) {
        write_file(fd, "HSM: PASS\n");
    } else {
        write_file(fd, "HSM: FAIL\n");
    }
}

//In this test host will switch between A and B
static void hsm_test_switch(fd, log_fd)
{
    uint64_t test_loop = g_hsm_test_loop;
    time_t old_time = 0, cur_time;

    if (!g_running) {
        return;
    }

    log_print(log_fd, "Start HSM switch test: will last %d loop\n", g_hsm_test_loop);
    log_print(log_fd, "In this test HOST will switch between A and B\n");

    if (g_machine == 'A') {
        cur_rts = TRUE;
        tc_set_rts_casco(fd, cur_rts);

        while (g_running && test_loop > 0) {
            hsm_send(fd, log_fd);

            cur_time = time(NULL);
            if (cur_time > (old_time + SWITCH_TIME)) {
                if ((g_hsm_test_loop - test_loop)%2 == 0) {
                    log_print(log_fd, "Switch loop %d\n:", (g_hsm_test_loop - test_loop) / 2 + 1);
                }
                cur_rts = !cur_rts;
                tc_set_rts_casco(fd, cur_rts);
                test_loop--;

                old_time = cur_time;

                sleep(1);

                cur_cts = tc_get_cts_casco(fd);

                printf("RTS=%d, CTS=%d\n", cur_rts, cur_cts);
                log_print(log_fd, "RTS=%d, CTS=%d\n", cur_rts, cur_cts);

                if (!cur_rts) {
                    if (!cur_cts) {
                        log_print(log_fd, "RTS=%d, CTS=%d, A is HOST. OK\n",
                                cur_rts, !cur_cts);
                    } else {
                        log_print(log_fd, "RTS=%d, CTS=%d, A is SLAVE. ERROR\n",
                                cur_rts, !cur_cts);
                        test_mod_hsm.pass = 0;
                        counter_fail++;
                    }
                } else {
                    if (!cur_cts) {
                        log_print(log_fd, "RTS=%d, CTS=%d, A is HOST. ERROR\n",
                                cur_rts, !cur_cts);
                        test_mod_hsm.pass = 0;
                        counter_fail++;
                    } else {
                        log_print(log_fd, "RTS=%d, CTS=%d, A is SLAVE. OK\n",
                                cur_rts, !cur_cts);
                    }
                }
            }
        }
    } else {
        cur_rts = FALSE;
        tc_set_rts_casco(fd, cur_rts);

        while (g_running && test_loop > 0) {
            hsm_send(fd, log_fd);

            cur_time = time(NULL);
            if (cur_time > (old_time + SWITCH_TIME)) {
                if ((g_hsm_test_loop - test_loop)%2 == 0) {
                    log_print(log_fd, "Switch loop %d\n:", (g_hsm_test_loop - test_loop) / 2 + 1);
                }
                cur_rts = !cur_rts;
                tc_set_rts_casco(fd, cur_rts);
                test_loop--;

                old_time = cur_time;

                sleep(1);

                cur_cts = tc_get_cts_casco(fd);

                printf("RTS=%d, CTS=%d\n", cur_rts, cur_cts);
                log_print(log_fd, "RTS=%d, CTS=%d\n", cur_rts, cur_cts);

                if (!cur_rts) {
                    if (!cur_cts) {
                        log_print(log_fd, "RTS=%d, CTS=%d, B is SLAVE. OK\n",
                                cur_rts, !cur_cts);
                    } else {
                        log_print(log_fd, "RTS=%d, CTS=%d, B is HOST. ERROR\n",
                                cur_rts, !cur_cts);
                        test_mod_hsm.pass = 0;
                        counter_fail++;
                    }
                } else {
                    if (!cur_cts) {
                        log_print(log_fd, "RTS=%d, CTS=%d, B is SLAVE. ERROR\n",
                                cur_rts, !cur_cts);
                        test_mod_hsm.pass = 0;
                        counter_fail++;
                    } else {
                        log_print(log_fd, "RTS=%d, CTS=%d, B is HOST. OK\n",
                                cur_rts, !cur_cts);
                    }
                }
            }
        }
    }

    log_print(log_fd, "End HSM switch test\n\n");
}

//In this test host will hold at B
static void hsm_test_hold(int fd, int log_fd)
{
    int switch_count = 0;
    time_t old_time = 0, cur_time;
    int old_cts, cur_cts;

    if (!g_running) {
        return;
    }

    log_print(log_fd, "Start HSM hold test\n");
    log_print(log_fd, "In this test HOST will hold at B\n");

    //Get original CTS status
    old_cts = tc_get_cts_casco(fd);

    if (!reverse) {
        tc_set_rts_casco(fd, TRUE);
    } else {
        tc_set_rts_casco(fd, FALSE);
    }

    if (g_machine == 'A') {
        while (g_running) {
            hsm_send(fd, log_fd);

            cur_cts = tc_get_cts_casco(fd);
            if (cur_cts != old_cts) {
                switch_count++;
                old_cts = cur_cts;
                test_mod_hsm.pass = 0;
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
            hsm_send(fd, log_fd);

            cur_cts = tc_get_cts_casco(fd);
            if (cur_cts != old_cts) {
                switch_count++;
                old_cts = cur_cts;
                test_mod_hsm.pass = 0;
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

    log_print(log_fd, "switch_count: %d\n", switch_count);
    log_print(log_fd, "End HSM hold test: %s\n\n", (switch_count == 0)?"PASS":"FAIL");
}

void *hsm_test(void *args)
{
    int log_fd = test_mod_hsm.log_fd;
    int fd;

    log_print(log_fd, "Begin test!\n\n");

    /* Init the packet to be sent */
    int i;
    for (i = 0; i < PACKET_SIZE; ++i) {
        g_packet[i] = (i % 64) + 0x30;
    }

    fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        log_print(log_fd, "open mac %c at %s is Failed!\n", g_machine, CCM_SERIAL_PORT);
        test_mod_hsm.pass = 0;
        pthread_exit(NULL);
    } else {
        log_print(log_fd, "open mac %c at %s is Successful!\n", g_machine, CCM_SERIAL_PORT);
    }

    sleep(2);

    hsm_test_switch(fd, log_fd);

    //Machine A wait for a while and let host switch to B.
    if (g_machine == 'A') {
        sleep(2);
    }

    //Starting SIM test
    g_sim_starting = TRUE;

    hsm_test_hold(fd, log_fd);

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

static void hsm_send(int fd, int log_fd)
{
    if (send_packet(fd) < 0) {
        log_print(log_fd, "Send packet error\n");
        test_mod_hsm.pass = 0;
    }
}
