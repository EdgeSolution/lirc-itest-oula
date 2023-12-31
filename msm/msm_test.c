/******************************************************************************
*
* FILENAME:
*     msm_test.c
*
* DESCRIPTION:
*     Define functions for MSM test.
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
#include <signal.h>
#include "msm_test.h"
#include "adv_spi.h"
#include "term.h"

static void msm_print_status();
static void msm_print_result(int fd);
static void *msm_test(void *args);


test_mod_t test_mod_msm = {
    .run = 1,
    .pass = 1,
    .name = "msm",
    .log_fd = -1,
    .test_routine = msm_test,
    .print_status = msm_print_status,
    .print_result = msm_print_result
};


/* Data size is 64KB(Half size of nvSRAM) */
#define PACKET_SIZE     HALF_SIZE

#define DATA_STEP       64
#define WAIT_TIME_IN_MS 1

/* Data pattern to write. */
static char data_55[PACKET_SIZE];
static char data_aa[PACKET_SIZE];

/* Counter of test times, successful and failed test. */
static unsigned long counter_test = 0;
static unsigned long counter_success = 0;
static unsigned long counter_fail = 0;


static int read_data_a(int fd, char *cmp_buf);
static int read_data_b(int fd, char *cmp_buf);
static int write_data_a(int fd, char *buf, size_t len);
static int write_data_b(int fd, char *buf, size_t len);
static void log_result(int log_fd);
static unsigned char get_data_pattern(int fd);
static int send_data_pattern(int fd, unsigned char pattern);
static void dump_data(int log_fd, char *buf, int len);


static void msm_print_status()
{
    if (g_hsm_switching) {
        printf("%-*s %s\n",
                COL_FIX_WIDTH, "MSM", "Awaiting");
        return;
    }

    printf("%-*s %s\n"
        "TEST:%-*lu ERROR:%-*lu\n",
        COL_FIX_WIDTH, "MSM", (counter_fail == 0) ? STR_MOD_OK : STR_MOD_ERROR,
        COL_FIX_WIDTH-5, counter_test, COL_FIX_WIDTH-6, counter_fail);
}

static void msm_print_result(int fd)
{
    if (test_mod_msm.pass) {
        if (g_hsm_switching) {
            write_file(fd, "MSM: Not started\n");
        } else {
            write_file(fd, "MSM: PASS. Test count:%lu\n", counter_test);
        }
    } else {
        write_file(fd, "MSM: FAIL. Test count:%lu;  Failed:%lu;\n",
            counter_test, counter_fail);
    }
}


static void *msm_test(void *args)
{
    int log_fd = test_mod_msm.log_fd;

    print_version(log_fd, "MSM");
    log_print(log_fd, "Begin test!\n\n");

    int bytes;

    //Wait for HSM switch test end
    while (g_running && g_hsm_switching) {
        sleep(2);
    }

    if (!g_running) {
        pthread_exit(NULL);
    }

    /* Open the device of storage. */
    int spi = open(ADVSPI_DEVICE, O_RDWR);
    if (spi == -1) {
        log_print(log_fd, "open storage device is Failed!\n");
        test_mod_msm.pass = 0;
        pthread_exit(NULL);
    }
    log_print(log_fd, "open storage device is Successful!\n");

    /* Open serial port */
    int com = ser_open(CCM_SERIAL_PORT);
    if (com < 0) {
        log_print(log_fd, "open serial port Failed!\n");
        test_mod_msm.pass = 0;
        pthread_exit(NULL);
    }

    /* Set data pattern to write */
    memset(data_55, 0x55, sizeof(data_55));
    memset(data_aa, 0xAA, sizeof(data_aa));

    char rbuf[PACKET_SIZE];
    char *data = NULL;
    unsigned char pattern = 0;
    char *cbuf;

    if (g_machine == 'A') {
        while (g_running) {
            counter_test++;

            /* Switch the data pattern */
            if (counter_test % 2) {
                data = data_55;
            } else {
                data = data_aa;
            }

            /* Write data */
            bytes = write_data_a(spi, data, PACKET_SIZE);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                if (bytes < 0) {
                    bytes = 0;
                }
                log_print(log_fd, "write %d bytes(%02X)   FALSE!\n", bytes, (unsigned char)data[0]);
                counter_fail++;
                test_mod_msm.pass = 0;
            } else {
                log_print(log_fd, "write %d bytes(%02X)   OK!\n", bytes, (unsigned char)data[0]);
            }

            /* Set data pattern to other side */
            if (send_data_pattern(com, data[0]) <= 0) {
                if (g_running)
                    log_print(log_fd, "send data via serial port FAIL!\n");
            }


            /* Get data pattern from other side */
            pattern = get_data_pattern(com);
            sleep(2);

            /* Read data and verify */
            switch (pattern) {
            case 0xAA:
            case 0x55:
                /* Read data */
                bytes = read_data_b(spi, rbuf);
                if (bytes != PACKET_SIZE) {
                    log_print(log_fd, "read %d bytes(%02X)   FALSE!\n", bytes, pattern);
                } else {
                    log_print(log_fd, "read %d bytes(%02X)   OK!\n", bytes, pattern);
                }

                /* Verify data */
                if (pattern == 0xAA) {
                    cbuf = data_aa;
                } else {
                    cbuf = data_55;
                }
                if (memcmp(cbuf, rbuf, bytes) == 0) {
                    log_print(log_fd, "verify data(%02X)   OK!\n", pattern);
                    counter_success++;
                } else {
                    log_print(log_fd, "verify data(%02X expected)   FALSE!\n", pattern);
                    dump_data(log_fd, rbuf, bytes);
                    counter_fail++;
                    test_mod_msm.pass = 0;
                }
                break;

            default:
                //log_print(log_fd, "Receive invalid pattern data(%02X) from serial port!\n", pattern);
                //counter_fail++;
                break;
            }

            if (!g_running) {
                break;
            }
            sleep(1);
        }
    } else {
        while (g_running) {
            counter_test++;

            /* Switch the data pattern */
            if (counter_test % 2) {
                data = data_aa;
            } else {
                data = data_55;
            }

            /* Get data pattern from other side */
            pattern = get_data_pattern(com);
            sleep(2);

            /* Read data and verify */
            switch (pattern) {
            case 0xAA:
            case 0x55:
                /* Read data */
                bytes = read_data_a(spi, rbuf);
                if (bytes != PACKET_SIZE) {
                    log_print(log_fd, "read %d bytes(%02X)   FALSE!\n", bytes, pattern);
                } else {
                    log_print(log_fd, "read %d bytes(%02X)   OK!\n", bytes, pattern);
                }

                /* Verify data */
                if (pattern == 0xAA) {
                    cbuf = data_aa;
                } else {
                    cbuf = data_55;
                }
                if (memcmp(cbuf, rbuf, bytes) == 0) {
                    log_print(log_fd, "verify data(%02X)   OK!\n", pattern);
                    counter_success++;
                } else {
                    log_print(log_fd, "verify data(%02X expected)   FALSE!\n", pattern);
                    dump_data(log_fd, rbuf, bytes);
                    counter_fail++;
                    test_mod_msm.pass = 0;
                }
                break;

            default:
                //log_print(log_fd, "Receive invalid pattern data(%02X) from serial port!\n", pattern);
                //counter_fail++;
                break;
            }

            if (!g_running) {
                break;
            }
            sleep(1);

            /* Write data */
            bytes = write_data_b(spi, data, PACKET_SIZE);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                if (bytes < 0) {
                    bytes = 0;
                }
                log_print(log_fd, "write %d bytes(%02X)   FALSE!\n", bytes, (unsigned char)data[0]);
                counter_fail++;
                test_mod_msm.pass = 0;
            } else {
                log_print(log_fd, "write %d bytes(%02X)   OK!\n", bytes, (unsigned char)data[0]);
            }

            /* Set data pattern to other side */
            if (send_data_pattern(com, data[0]) <= 0) {
                if (g_running)
                    log_print(log_fd, "send data via serial port FAIL!\n");
            }
        }
    }

    log_result(log_fd);

    /* Close the device of storage. */
    close(spi);
    close(com);

    log_print(log_fd, "Test end\n\n");
    pthread_exit(NULL);
}


/******************************************************************************
 * NAME:
 *      read_data_a
 *
 * DESCRIPTION:
 *      Read data from storage zone A.
 *
 * PARAMETERS:
 *      fd       - The fd of serial port
 *      buf      - The buffer to keep readed data
 *
 * RETURN:
 *      Number of bytes readed
 ******************************************************************************/
static int read_data_a(int fd, char *buf)
{
    int ret = 0;
    int len = PACKET_SIZE;
    int bytes = 0;
    int step = DATA_STEP;

    memset(buf, 0, len);

    while (g_running && (bytes < len)) {
        while (g_running) {
            ret = advspi_read_a(fd, buf+bytes, step, bytes);
            sleep_ms(WAIT_TIME_IN_MS);
            if (ret == step) {
                bytes += step;
                break;
            }
        }
    }

    return bytes;
}


/******************************************************************************
 * NAME:
 *      read_data_b
 *
 * DESCRIPTION:
 *      Read data from storage zone B.
 *
 * PARAMETERS:
 *      fd       - The fd of serial port
 *      buf      - The buffer to keep readed data
 *
 * RETURN:
 *      Number of bytes readed
 ******************************************************************************/
static int read_data_b(int fd, char *buf)
{
    int ret = 0;
    int len = PACKET_SIZE;
    int bytes = 0;
    int step = DATA_STEP;

    memset(buf, 0, len);

    while (g_running && (bytes < len)) {
        while (g_running) {
            ret = advspi_read_b(fd, buf+bytes, step, bytes);
            sleep_ms(WAIT_TIME_IN_MS);
            if (ret == step) {
                bytes += step;
                break;
            }
        }
    }

    return bytes;
}


/******************************************************************************
 * NAME:
 *      write_data_a
 *
 * DESCRIPTION:
 *      Write data to storage adapter.
 *
 * PARAMETERS:
 *      fd   - The fd
 *      buf  - The buffer of data
 *      len  - The length of data
 *
 * RETURN:
 *      Number of bytes wrote
 ******************************************************************************/
static int write_data_a(int fd, char *buf, size_t len)
{
    int bytes = 0;
    int ret = 0;
    int step = DATA_STEP;

    while (g_running && (bytes < len)) {
        while (g_running) {
            ret = advspi_write_a(fd, buf+bytes, step, bytes);
            sleep_ms(WAIT_TIME_IN_MS);
            if (ret == step) {
                bytes += step;
                break;
            }
        }
    }

    return bytes;
}


/******************************************************************************
 * NAME:
 *      write_data_b
 *
 * DESCRIPTION:
 *      Write data to storage adapter.
 *
 * PARAMETERS:
 *      fd   - The fd
 *      buf  - The buffer of data
 *      len  - The length of data
 *
 * RETURN:
 *      Number of bytes wrote
 ******************************************************************************/
static int write_data_b(int fd, char *buf, size_t len)
{
    int bytes = 0;
    int ret = 0;
    int step = DATA_STEP;

    while (g_running && (bytes < len)) {
        while (g_running) {
            ret = advspi_write_b(fd, buf+bytes, step, bytes);
            sleep_ms(WAIT_TIME_IN_MS);
            if (ret == step) {
                bytes += step;
                break;
            }
        }
    }

    return bytes;
}


static void log_result(int log_fd)
{
    if (counter_fail > 0) {
        log_print(log_fd, "Test FALSE. Test count:%lu;  Failed:%lu;\n",
            counter_test, counter_fail);
        log_print(log_fd, "FAIL\n");
        test_mod_msm.pass = 0;
    } else if (counter_success == 0) {
        log_print(log_fd, "Serial port communication error!\n");
        test_mod_msm.pass = 0;
    } else {
        log_print(log_fd, "Test OK. Test count:%lu\n", counter_test);
        log_print(log_fd, "PASS\n");
    }
}


/******************************************************************************
 * NAME:
 *      get_data_pattern
 *
 * DESCRIPTION:
 *      Read data from serail port and search the char 0x55 or 0xAA
 *
 * PARAMETERS:
 *      fd - The fd of serail port
 *
 * RETURN:
 *      The char 0x55/0xAA
 ******************************************************************************/
static unsigned char get_data_pattern(int fd)
{
    char buf[64];

    while (g_running) {
        memset(buf, 0, sizeof(buf));
        int size = read(fd, buf, sizeof(buf)-1);
        if (size > 0) {
            if (strchr(buf, 0x55)) {
                return 0x55;
            } else if (strchr(buf, 0xAA)) {
                return 0xAA;
            } else if (strchr(buf, EXIT_SYNC_A)) {
                g_running = 0;
                return EXIT_SYNC_A;
            } else if (strchr(buf, EXIT_SYNC_B)) {
                g_running = 0;
                return EXIT_SYNC_B;
            }
        }
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      send_data_pattern
 *
 * DESCRIPTION:
 *      Send a char from the serial port.
 *
 * PARAMETERS:
 *      fd - The fd of serial port
 *      pattern - Char to send
 *
 * RETURN:
 *      Bytes wrote
 ******************************************************************************/
static int send_data_pattern(int fd, unsigned char pattern)
{
    char buf[2] = {pattern, 0};

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
 *      dump_data
 *
 * DESCRIPTION:
 *      Dump buffer in HEX mode to log
 *
 * PARAMETERS:
 *      log_fd - The log file
 *      buf    - The buffer
 *      len    - The count of data in buf
 *
 * RETURN:
 *      None
 ******************************************************************************/
static void dump_data(int log_fd, char *buf, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        write_file(log_fd, "%02X ", *((unsigned char *)buf + i));
        if (((i+1) % 16) == 0) {
            write_file(log_fd, "\n");
        }
    }
    write_file(log_fd, "\n");
}
