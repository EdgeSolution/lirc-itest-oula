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

void msm_print_status();
void msm_print_result(int fd);
void *msm_test(void *args);
static int open_port(void);


test_mod_t test_mod_msm = {
    .run = 1,
    .pass = 1,
    .name = "msm",
    .log_fd = -1,
    .test_routine = msm_test,
    .print_status = msm_print_status,
    .print_result = msm_print_result
};

#define DEVICE_NAME     "/dev/ttyS1"

#define ADVSPI_DEVICE   "/dev/advspi3962"

/* Data size if 64KB(Half size of nvSRAM) */
#define PACKET_SIZE     HALF_SIZE

/* Data pattern to write. */
char data_55[PACKET_SIZE];
char data_aa[PACKET_SIZE];

/* Counter of test times, successful and failed test. */
unsigned long counter_test = 0;
unsigned long counter_success = 0;
unsigned long counter_fail = 0;


static int read_data_a(int fd, int addr, char *cmp_buf);
static int read_data_b(int fd, int addr, char *cmp_buf);
static void print_result(void);
static int open_port(void);


void msm_print_status()
{
    printf("%-*s %s\n"
        "TEST:%-*lu ERROR:%-*lu\n",
        COL_FIX_WIDTH, "MSM", (counter_fail == 0) ? STR_MOD_OK : STR_MOD_ERROR,
        COL_FIX_WIDTH-5, counter_test, COL_FIX_WIDTH-6, counter_fail);
}

void msm_print_result(int fd)
{
    if (test_mod_msm.pass) {
        write_file(fd, "MSM: PASS\n");
    } else {
        write_file(fd, "MSM: FAIL\n");
    }
}


void *msm_test(void *args)
{
    int log_fd = test_mod_msm.log_fd;

    log_print(log_fd, "Begin test!\n\n");

    int addr = 0;
    int bytes;

    /* Open the device of storage. */
    int spi = open(ADVSPI_DEVICE, O_RDWR);
    if (spi == -1) {
        log_print(log_fd, "open storage device is Failed!\n");
        return NULL;
    }
    log_print(log_fd, "open storage device is Successful!\n");

    memset(data_55, 0x55, sizeof(data_55));
    memset(data_aa, 0xAA, sizeof(data_aa));

    if (g_machine == 'A') {
        while (g_running) {
            counter_test++;

            /* Write data */
            bytes = advspi_write_a(spi, data_55, PACKET_SIZE, 0);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("write %d bytes(%02X)   FALSE!\n", bytes, data_55[0]);
                counter_fail++;
                continue;
            } else {
                printf("write %d bytes(%02X)   OK!\n", bytes, data_55[0]);
            }
            sleep(10);

            bytes = read_data_b(spi, addr, data_aa);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("read %d bytes(%02X)   FALSE!\n", bytes, data_aa[0]);
                counter_fail++;
            } else {
                printf("read %d bytes(%02X)   OK!\n", bytes, data_aa[0]);
                counter_success++;
            }
            sleep(2);
        }
    } else {
        sleep(10);
        while (g_running) {
            counter_test++;

            bytes = read_data_a(spi, addr, data_55);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("read %d bytes(%02X)   FALSE!\n", bytes, data_55[0]);
                counter_fail++;
                continue;
            } else {
                printf("read %d bytes(%02X)   OK!\n", bytes, data_55[0]);
                counter_success++;
            }
            sleep(2);

            bytes = advspi_write_b(spi, data_aa, PACKET_SIZE, 0);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("write %d bytes(%02X)   FALSE!\n", bytes, data_aa[0]);
                counter_fail++;
                continue;
            } else {
                printf("write %d bytes(%02X)   OK!\n", bytes, data_aa[0]);
            }
            sleep(10);
        }
    }

    print_result();

    /* Close the device of storage. */
    close(spi);

    log_print(log_fd, "Test end\n\n");
    return NULL;
}


/******************************************************************************
 * NAME:
 *      read_data_a
 *
 * DESCRIPTION:
 *      Read data from storage zone A, and compare it with predefined pattern.
 *
 * PARAMETERS:
 *      fd       - The fd of serial port
 *      addr     - The offset to start read
 *      cmp_buf  - The data to compare with the readed data
 *
 * RETURN:
 *      Number of bytes readed
 ******************************************************************************/
static int read_data_a(int fd, int addr, char *cmp_buf)
{
    int ret = 0;
    int len = PACKET_SIZE;
    char buf[PACKET_SIZE];

    memset(buf, 0, sizeof(buf));

    ret = advspi_read_a(fd, buf, len, 0);
    if (ret <= 0) {
        return ret;
    }

    if (memcmp(cmp_buf, buf, len) == 0) {
        return -1;
    }

    return ret;
}


/******************************************************************************
 * NAME:
 *      read_data_b
 *
 * DESCRIPTION:
 *      Read data from storage zone B, and compare it with predefined pattern.
 *
 * PARAMETERS:
 *      fd       - The fd of serial port
 *      addr     - The offset to start read
 *      cmp_buf  - The data to compare with the readed data
 *
 * RETURN:
 *      Number of bytes readed
 ******************************************************************************/
static int read_data_b(int fd, int addr, char *cmp_buf)
{
    int ret = 0;
    int len = PACKET_SIZE;
    char buf[PACKET_SIZE];

    memset(buf, 0, sizeof(buf));

    ret = advspi_read_b(fd, buf, len, 0);
    if (ret <= 0) {
        return ret;
    }

    if (memcmp(cmp_buf, buf, len) == 0) {
        return -1;
    }

    return ret;
}


static void print_result(void)
{
    printf("\n");
    if (counter_fail > 0) {
        printf("Test FALSE. Test time:%lu;  Failed:%lu;\n",
            counter_test, counter_fail);
        printf("FAIL\n");
    } else {
        printf("Test OK. Test time:%lu\n", counter_test);
        printf("PASS\n");
    }
}


/* Open and setup serial port */
static int open_port(void)
{
    int fd;
    char *dev = DEVICE_NAME;
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
