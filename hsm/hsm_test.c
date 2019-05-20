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
*     12/06/2016  Jia Sui (jia.sui@advantech.com.cn)
*     - Add HSM test for CIM
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

static void hsm_print_status();
static void hsm_print_result(int fd);
static void wait_for_cpld_stable(int log_fd, int fd);
static void hsm_test_switch(int fd, int log_fd);
static void hsm_test_hold(int fd, int log_fd);
static void *hsm_test(void *args);
static void tc_set_rts_casco(int fd, char enabled);
static int tc_get_cts_casco(int fd);
static void hsm_send(int fd, int log_fd);
static int hsm_send_switch(int fd);
static uint8_t hsm_wait_switch(int fd);
static void wait_for_cts_change(int fd);
static void check_cts_a(int log_fd);
static void check_cts_b(int log_fd);
static void hsm_test_ccm(int fd, int log_fd);
static void hsm_test_cim(int fd, int log_fd);

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

#define SWITCH_CHAR_A   0xFA
#define SWITCH_CHAR_B   0xFB

#define HOLD_INTERVAL       60
#define WAIT_TIMEOUT        3

#define SENDING_COUNT       2

/* The packet to send */
static char g_packet[PACKET_SIZE];

/* Test Counters */
static uint32_t test_counter = 0;
static uint32_t switch_fail_cntr = 0;
static uint32_t hold_fail_cntr = 0;
static uint32_t timeout_cntr = 0;

static uint8_t g_cur_cts;
static uint8_t g_cur_rts;

static void hsm_print_status()
{
    printf("%-*s %s\n", COL_FIX_WIDTH, "HSM",
            (test_mod_hsm.pass == 1)?STR_MOD_OK:STR_MOD_ERROR);

    if (g_dev_sku == SKU_CIM) {
        if (g_machine == 'A') {
            printf("CTS: %d A is %s, count: %d\n", g_cur_cts,
                    g_cur_cts?"HOST":"SLAVE", test_counter/2);
        } else {
            printf("CTS: %d B is %s, count: %d\n", g_cur_cts,
                    g_cur_cts?"SLAVE":"HOST", test_counter/2);
        }
    } else {
        if (g_machine == 'A') {
            printf("RTS: %d CTS: %d A is %s\n",
                    g_hsm_switching?(!g_cur_rts):g_cur_rts, g_cur_cts,
                    g_cur_cts?"HOST":"SLAVE");
        } else {
            printf("RTS: %d CTS: %d B is %s\n",
                    g_hsm_switching?(!g_cur_rts):g_cur_rts,
                    g_cur_cts, g_cur_cts?"SLAVE":"HOST");
        }
    }
}

static void hsm_print_result(int fd)
{
    //Fix unexpect fail counter increase when break test
    if (switch_fail_cntr == 1)
        switch_fail_cntr--;

    if (switch_fail_cntr || hold_fail_cntr)
        test_mod_hsm.pass = 0;

    //Print HSM test result
    if (test_mod_hsm.pass) {
        write_file(fd, "HSM: PASS. test=%lu\n",
                (g_dev_sku == SKU_CIM)?test_counter/2:test_counter);
    } else {
        if (g_dev_sku == SKU_CIM) {
            write_file(fd, "HSM: FAIL. test=%lu\n", test_counter/2);
        } else {
            write_file(fd, "HSM: FAIL. test=%lu, switch fail=%lu, hold fail=%lu%s\n",
                    test_counter, switch_fail_cntr, hold_fail_cntr,
                    timeout_cntr?" Timeout":"");
        }
    }
}

static void wait_for_cpld_stable(int log_fd, int fd)
{
    int i;
    for (i = 0; i < SENDING_COUNT; i++) {
        hsm_send(fd, log_fd);
        sleep_ms(WAIT_IN_MS);
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
static void hsm_test_switch(int fd, int log_fd)
{
    uint64_t test_loop = g_hsm_test_loop;
    uint8_t pattern;

    if (!g_running) {
        return;
    }

    log_print(log_fd, "Start HSM switch test: will last %lu loop\n", g_hsm_test_loop);
    log_print(log_fd, "In this test HOST will switch between A and B\n");

    //Switch to A, and then start test
    if (g_machine == 'A') {
        g_cur_rts = TRUE;
    } else {
        g_cur_rts = FALSE;
    }

    tc_set_rts_casco(fd, g_cur_rts);
    wait_for_cpld_stable(log_fd, fd);

    if (g_machine == 'A') {
        while (g_running && test_loop > 0) {
            log_print(log_fd, "Switch loop %lu:\n",
                    (g_hsm_test_loop - test_loop) + 1);

            if (g_cur_rts) {
                hsm_send(fd, log_fd);
                sleep_ms(WAIT_IN_MS);
            }

            if (!g_running) {
                break;
            }

            if (g_cur_rts) {
                wait_for_cts_change(fd);
                check_cts_a(log_fd);
            }

            pattern = hsm_wait_switch(fd);
            if (pattern == SWITCH_CHAR_A || pattern == EXIT_SYNC_A) {
                log_print(log_fd, "[ERROR] Receivd %d, please check uart status,"
                        "uart is now in loopback mode\n", SWITCH_CHAR_A);
            } else if (pattern == EXIT_SYNC_B) {
                break;
            }

            g_cur_rts = TRUE;
            tc_set_rts_casco(fd, g_cur_rts);

            test_loop--;
            test_counter++;
        }
    } else {
        while (g_running && test_loop > 0) {
            log_print(log_fd, "Switch loop %lu:\n",
                    (g_hsm_test_loop - test_loop) + 1);

            pattern = hsm_wait_switch(fd);
            if (pattern == SWITCH_CHAR_B || pattern == EXIT_SYNC_B) {
                log_print(log_fd, "[ERROR] Receivd %d, please check uart status,"
                        "uart is now in loopback mode\n", SWITCH_CHAR_B);
            } else if (pattern == EXIT_SYNC_A) {
                break;
            }

            g_cur_rts = TRUE;
            tc_set_rts_casco(fd, g_cur_rts);

            if (g_cur_rts) {
                hsm_send(fd, log_fd);
                sleep_ms(WAIT_IN_MS);
            }

            if (!g_running) {
                break;
            }

            if (g_cur_rts) {
                wait_for_cts_change(fd);
                check_cts_b(log_fd);
            }

            test_loop--;
            test_counter++;
        }
    }

    log_print(log_fd, "Switch fail counter: %lu\n", switch_fail_cntr);
    log_print(log_fd, "End HSM switch test: %s\n\n", (switch_fail_cntr==0)?"PASS":"FAIL");
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
            sleep_ms(WAIT_IN_MS);

            cur_time = time(NULL);
            if (cur_time > (old_time + HOLD_INTERVAL)) {
                g_cur_cts = tc_get_cts_casco(fd);
                log_print(log_fd, "RTS=%d, CTS=%d, A is %s, switch count: %lu\n",
                        g_cur_rts, g_cur_cts, g_cur_cts?"HOST":"SLAVE", hold_fail_cntr);

                old_time = cur_time;
            }

            if (!g_running) {
                break;
            }

            g_cur_cts = tc_get_cts_casco(fd);
            if (g_cur_cts != old_cts) {
                hold_fail_cntr++;
                old_cts = g_cur_cts;
                test_mod_hsm.pass = 0;

                log_print(log_fd, "RTS=%d, CTS=%d, A is %s, switch count: %lu\n",
                        g_cur_rts, g_cur_cts, g_cur_cts?"HOST":"SLAVE", hold_fail_cntr);
            }
        }
    } else {
        while (g_running) {
            hsm_send(fd, log_fd);
            sleep_ms(WAIT_IN_MS);

            cur_time = time(NULL);
            if (cur_time > (old_time + HOLD_INTERVAL)) {
                g_cur_cts = tc_get_cts_casco(fd);
                log_print(log_fd, "RTS=%d, CTS=%d, B is %s, switch count: %lu\n",
                        g_cur_rts, g_cur_cts, g_cur_cts?"SLAVE":"HOST", hold_fail_cntr);

                old_time = cur_time;
            }

            if (!g_running) {
                break;
            }

            g_cur_cts = tc_get_cts_casco(fd);
            if (g_cur_cts != old_cts) {
                hold_fail_cntr++;
                old_cts = g_cur_cts;
                test_mod_hsm.pass = 0;

                log_print(log_fd, "RTS=%d, CTS=%d, B is %s, switch count: %lu\n",
                        g_cur_rts, g_cur_cts, g_cur_cts?"SLAVE":"HOST", hold_fail_cntr);
            }
        }
    }

    log_print(log_fd, "Hold fail counter: %lu\n", hold_fail_cntr);
    log_print(log_fd, "End HSM hold test: %s\n\n", (hold_fail_cntr == 0)?"PASS":"FAIL");
}

static void *hsm_test(void *args)
{
    int log_fd = test_mod_hsm.log_fd;
    int fd;

    print_version(log_fd, "HSM");
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

    if (g_dev_sku == SKU_CIM) {
        hsm_test_cim(fd, log_fd);
    } else {
        hsm_test_ccm(fd, log_fd);
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
        return -2;
    }

    reg_val = inb (m_nBasePort + nReg);

    if (ioperm (m_nBasePort, 7, 0)) {
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

static int hsm_send_switch(int fd)
{
    char buf[2];

    if (g_machine == 'A') {
        buf[0] = SWITCH_CHAR_A;
    } else {
        buf[0] = SWITCH_CHAR_B;
    }

    while (g_running) {
        int size = write(fd, buf, 1);
        if (size > 0) {
            return 1;
        }
    }

    return 0;
}

static uint8_t hsm_wait_switch(int fd)
{
    char buf[64];
    uint8_t cnt = 0;

    while (g_running) {
        memset(buf, 0, sizeof(buf));
        int size = read(fd, buf, sizeof(buf)-1);
        if (size > 0) {
            if (strchr(buf, SWITCH_CHAR_A)) {
                return SWITCH_CHAR_A;
            } else if (strchr(buf, SWITCH_CHAR_B)) {
                return SWITCH_CHAR_B;
            } else if (strchr(buf, EXIT_SYNC_A)) {
                g_running = 0;
                return EXIT_SYNC_A;
            } else if (strchr(buf, EXIT_SYNC_B)) {
                g_running = 0;
                return EXIT_SYNC_B;
            }
        }

        cnt++;
        if (cnt > WAIT_TIMEOUT) {
            log_print(test_mod_hsm.log_fd, "Wait for switch signal timeout\n");
            timeout_cntr++;
            test_mod_hsm.pass = 0;
            break;
        }
    }

    return 0;
}

static void wait_for_cts_change(int fd)
{
    uint8_t cts;
    uint8_t cnt = 0;

    g_cur_cts = tc_get_cts_casco(fd);

    g_cur_rts = FALSE;
    tc_set_rts_casco(fd, g_cur_rts);

    do {
        hsm_send_switch(fd);
        sleep_ms(WAIT_IN_MS);
        cts = tc_get_cts_casco(fd);

        cnt++;
        if (cnt > WAIT_TIMEOUT) {
            log_print(test_mod_hsm.log_fd, "Wait for cts change timeout\n");
            test_mod_hsm.pass = 0;
            timeout_cntr++;
            break;
        }
    } while(cts == g_cur_cts && g_running);

    g_cur_cts = cts;
}

static void check_cts_a(int log_fd)
{
    if (g_cur_rts && g_cur_cts) { //rts=1 cts=1
        log_print(log_fd, "RTS=%d, CTS=%d, A is HOST. OK\n",
                g_cur_rts, g_cur_cts);
    } else if (g_cur_rts && !g_cur_cts) { //rts=1 cts=0
        log_print(log_fd, "RTS=%d, CTS=%d, A is SLAVE. ERROR\n",
                g_cur_rts, g_cur_cts);
        switch_fail_cntr++;
    } else if (!g_cur_rts && g_cur_cts) { //rts=0 cts=1
        log_print(log_fd, "RTS=%d, CTS=%d, A is HOST. ERROR\n",
                g_cur_rts, g_cur_cts);
        switch_fail_cntr++;
    } else if (!g_cur_rts && !g_cur_cts) { //rts=0 cts=0
        log_print(log_fd, "RTS=%d, CTS=%d, A is SLAVE. OK\n",
                g_cur_rts, g_cur_cts);
    } else {
        log_print(log_fd, "Shouldn't be here....\n");
        test_mod_hsm.pass = 0;
    }
}

static void check_cts_b(int log_fd)
{
    if (g_cur_rts && g_cur_cts) { //rts=1 cts=1
        log_print(log_fd, "RTS=%d, CTS=%d, B is SLAVE. ERROR\n",
                g_cur_rts, g_cur_cts);
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
        switch_fail_cntr++;
    } else {
        log_print(log_fd, "Shouldn't be here....\n");
        test_mod_hsm.pass = 0;
    }
}

//Test routine for CCM, it includes switch test and hold test
static void hsm_test_ccm(int fd, int log_fd)
{
    hsm_test_switch(fd, log_fd);

    //Switch host to B.
    if (g_running) {
        int cnt = 0;
        wait_for_cpld_stable(log_fd, fd);
        do {
            if (g_machine == 'A') {
                tc_set_rts_casco(fd, FALSE);
            } else {
                tc_set_rts_casco(fd, TRUE);
            }
            wait_for_cpld_stable(log_fd, fd);

            cnt++;
            if (cnt > WAIT_TIMEOUT) {
                log_print(log_fd, "Switch host to B fail\n\n");
                timeout_cntr++;
                test_mod_hsm.pass = 0;
                break;
            }

        } while (tc_get_cts_casco(fd) != 0 && g_running);
    }

    if (g_running) {
        wait_other_side_ready();
    }

    //Starting SIM/MSM test
    if (g_running) {
        g_hsm_switching = FALSE;
    }

    g_cur_rts = TRUE;
    hsm_test_hold(fd, log_fd);

}

//test routine for CIM
static void hsm_test_cim(int fd, int log_fd)
{
    uint8_t old_cts;
    uint64_t test_loop = g_hsm_test_loop * 2;

    log_print(log_fd, "Start HSM switch test: will last %lu loop\n", g_hsm_test_loop);

    old_cts = tc_get_cts_casco(fd);
    log_print(log_fd, "CTS: %u\n", g_cur_cts);

    do {
        g_cur_cts = tc_get_cts_casco(fd);
        log_print(log_fd, "CTS: %u\n", g_cur_cts);

        //Detect cts change
        if (old_cts != g_cur_cts) {
            test_counter++;
            log_print(log_fd, "Switch Counter: %lu, CTS status changed from %d to %d\n",
                    (test_counter+1)/2, old_cts, g_cur_cts);
            old_cts = g_cur_cts;
        }

        //Check CIM HSM status
        if (test_counter/2 >= g_hsm_test_loop+1) {
            test_mod_hsm.pass = 0;
        }

        sleep_ms(WAIT_IN_MS/3);
    } while (g_running);

    //Check CIM HSM status
    if (test_counter < test_loop) {
        test_mod_hsm.pass = 0;
    }
}
