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
*     10/12/2016  Jia Sui (jia.sui@advantech.com.cn)
*     - Implement new requirement
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
static void hsm_test_switch(int fd, int log_fd);
static void hsm_test_hold(int fd, int log_fd);
void *hsm_test(void *args);
static void tc_set_rts_casco(int fd, char enabled);
static int tc_get_cts_casco(int fd);
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

/* The size of packet to send */
#define PACKET_SIZE     4

#define SWITCH_INTERVAL     10
#define WAIT_TIME_IN_MS     200

/* The packet to send */
static char g_packet[PACKET_SIZE];

/* Test Counters */
static uint32_t test_counter = 0;
static uint32_t switch_fail_cntr = 0;
static uint32_t hold_fail_cntr = 0;

static uint8_t g_cur_cts;
static uint8_t g_cur_rts;

void hsm_print_status()
{
    printf("%-*s %s\n", COL_FIX_WIDTH, "HSM",
            (test_mod_hsm.pass == 1)?STR_MOD_OK:STR_MOD_ERROR);

    if (g_machine == 'A') {
        printf("RTS: %d CTS: %d A is %s\n", g_cur_rts, g_cur_cts, g_cur_cts?"HOST":"SLAVE");
    } else {
        printf("RTS: %d CTS: %d B is %s\n", g_cur_rts, g_cur_cts, g_cur_cts?"SLAVE":"HOST");
    }
}

void hsm_print_result(int fd)
{
    if (test_mod_hsm.pass) {
        write_file(fd, "HSM: PASS, test=%lu\n", test_counter);
    } else {
        write_file(fd, "HSM: FAIL, test=%lu, switch fail=%lu, hold fail=%lu\n",
                test_counter, switch_fail_cntr, hold_fail_cntr);
    }
}

/*
 * hsm_test_switch()
 * In this test host will switch between A and B
 *
 * State Machine:
 * H means Host, S means Slave, (E) means error
 * RTS  |   CTS  |  A    |  B
 * -----+--------+-------+-------
 *  1   |   1    |  H    |  S(E)
 *  1   |   0    |  S(E) |  H
 *  0   |   1    |  H(E) |  S
 *  0   |   0    |  S    |  H(E)
 */
static void hsm_test_switch(fd, log_fd)
{
    uint64_t test_loop = g_hsm_test_loop;
    time_t old_time = 0, cur_time;

    if (!g_running) {
        return;
    }

    log_print(log_fd, "Start HSM switch test: will last %lu loop\n", g_hsm_test_loop);
    log_print(log_fd, "In this test HOST will switch between A and B\n");

    //Get old_time and will use it later
    old_time = time(NULL);

    //Get original CTS status
    g_cur_cts = tc_get_cts_casco(fd);

    //NOTE: cts: 1 means A is host, cts: 0 means B is host
    //We need to set rts signal according the current cts status.
    if (g_machine == 'A') {
        if (g_cur_cts) {
            g_cur_rts = FALSE;
        } else {
            g_cur_rts = TRUE;
        }
        tc_set_rts_casco(fd, g_cur_rts);

        while (g_running && test_loop > 0) {
            hsm_send(fd, log_fd);
            sleep_ms(WAIT_TIME_IN_MS);

            cur_time = time(NULL);
            if (cur_time < (old_time + SWITCH_INTERVAL)) {
                continue;
            }

            log_print(log_fd, "Switch loop %lu:\n",
                    (g_hsm_test_loop - test_loop) + 1);

            //Check if CTS changed
            g_cur_cts = tc_get_cts_casco(fd);

            if (g_cur_rts && g_cur_cts) { //rts=1 cts=1
                log_print(log_fd, "RTS=%d, CTS=%d, A is HOST. OK\n",
                        g_cur_rts, g_cur_cts);
            } else if (g_cur_rts && !g_cur_cts) { //rts=1 cts=0
                log_print(log_fd, "RTS=%d, CTS=%d, A is SLAVE. ERROR\n",
                        g_cur_rts, g_cur_cts);
                test_mod_hsm.pass = 0;
                switch_fail_cntr++;
            } else if (!g_cur_rts && g_cur_cts) { //rts=0 cts=1
                log_print(log_fd, "RTS=%d, CTS=%d, A is HOST. ERROR\n",
                        g_cur_rts, g_cur_cts);
                test_mod_hsm.pass = 0;
                switch_fail_cntr++;
            } else if (!g_cur_rts && !g_cur_cts) { //rts=0 cts=0
                log_print(log_fd, "RTS=%d, CTS=%d, A is SLAVE. OK\n",
                        g_cur_rts, g_cur_cts);
            } else {
                log_print(log_fd, "Shouldn't be here....\n");
                test_mod_hsm.pass = 0;
            }

            test_loop--;
            test_counter++;

            old_time = cur_time;

            //Reverse rts flag and signal
            g_cur_rts = !g_cur_rts;
            tc_set_rts_casco(fd, g_cur_rts);
        }
    } else {
        if (g_cur_cts) {
            g_cur_rts = TRUE;
        } else {
            g_cur_rts = FALSE;
        }
        tc_set_rts_casco(fd, g_cur_rts);

        while (g_running && test_loop > 0) {
            hsm_send(fd, log_fd);
            sleep_ms(WAIT_TIME_IN_MS);

            cur_time = time(NULL);
            if (cur_time < (old_time + SWITCH_INTERVAL)) {
                continue;
            }

            log_print(log_fd, "Switch loop %lu:\n",
                    (g_hsm_test_loop - test_loop) + 1);

            g_cur_cts = tc_get_cts_casco(fd);

            if (g_cur_rts && g_cur_cts) { //rts=1 cts=1
                log_print(log_fd, "RTS=%d, CTS=%d, B is SLAVE. ERROR\n",
                        g_cur_rts, g_cur_cts);
                test_mod_hsm.pass = 0;
                switch_fail_cntr++;
            } else if (g_cur_rts && !g_cur_cts) { //rts=1 cts=0
                log_print(log_fd, "RTS=%d, CTS=%d, B is HOST. OK\n",
                        g_cur_rts, g_cur_cts);
            } else if (!g_cur_rts && g_cur_cts) { //rts=0 cts=1
                log_print(log_fd, "RTS=%d, CTS=%d, B is SLAVE. OK\n",
                        g_cur_rts, g_cur_cts);
            } else if (!g_cur_rts && !g_cur_cts) { //rts=0 cts=0
                log_print(log_fd, "RTS=%d, CTS=%d, B is HOST. ERROR\n",
                        g_cur_rts, g_cur_cts);
                test_mod_hsm.pass = 0;
                switch_fail_cntr++;
            } else {
                log_print(log_fd, "Shouldn't be here....\n");
                test_mod_hsm.pass = 0;
            }

            test_loop--;
            test_counter++;

            old_time = cur_time;

            //Reverse rts flag and signal
            g_cur_rts = !g_cur_rts;
            tc_set_rts_casco(fd, g_cur_rts);
        }
    }

    log_print(log_fd, "End HSM switch test\n\n");
}

//In this test host will hold at B
static void hsm_test_hold(int fd, int log_fd)
{
    time_t old_time = 0, cur_time;
    int old_cts;

    if (!g_running) {
        log_print(log_fd, "Exit flag detected, return\n");
        return;
    }

    log_print(log_fd, "Start HSM hold test\n");
    log_print(log_fd, "In this test HOST will hold at B\n");

    //Get original CTS status
    old_cts = tc_get_cts_casco(fd);

    tc_set_rts_casco(fd, g_cur_rts);

    if (g_machine == 'A') {
        while (g_running) {
            hsm_send(fd, log_fd);
            sleep_ms(WAIT_TIME_IN_MS);

            cur_time = time(NULL);
            if (cur_time > (old_time + 1)) {
                g_cur_cts = tc_get_cts_casco(fd);
                log_print(log_fd, "RTS=%d, CTS=%d, A is %s, switch count: %lu\n",
                        g_cur_rts, g_cur_cts, g_cur_cts?"HOST":"SLAVE", hold_fail_cntr);

                old_time = cur_time;
            }

            g_cur_cts = tc_get_cts_casco(fd);
            if (g_cur_cts != old_cts) {
                hold_fail_cntr++;
                old_cts = g_cur_cts;
                test_mod_hsm.pass = 0;
            }
        }
    } else {
        while (g_running) {
            hsm_send(fd, log_fd);
            sleep_ms(WAIT_TIME_IN_MS);

            cur_time = time(NULL);
            if (cur_time > (old_time + 1)) {
                g_cur_cts = tc_get_cts_casco(fd);
                log_print(log_fd, "RTS=%d, CTS=%d, B is %s, switch count: %lu\n",
                        g_cur_rts, g_cur_cts, g_cur_cts?"SLAVE":"HOST", hold_fail_cntr);

                old_time = cur_time;
            }

            g_cur_cts = tc_get_cts_casco(fd);
            if (g_cur_cts != old_cts) {
                hold_fail_cntr++;
                old_cts = g_cur_cts;
                test_mod_hsm.pass = 0;
            }
        }
    }

    log_print(log_fd, "Hold fail counter: %lu\n", hold_fail_cntr);
    log_print(log_fd, "End HSM hold test: %s\n\n", (hold_fail_cntr == 0)?"PASS":"FAIL");
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

    g_cur_rts = TRUE;

    //Switch host to B.
    if (g_machine == 'B') {
        tc_set_rts_casco(fd, g_cur_rts);
        hsm_send(fd, log_fd);
    }

    sleep_ms(500);

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

    return (reg_val & 0x10) ? 0 : 1;
}

static void hsm_send(int fd, int log_fd)
{
    if (send_packet(fd, g_packet, sizeof(g_packet)) < 0) {
        log_print(log_fd, "Send packet error\n");
        test_mod_hsm.pass = 0;
    }
}
