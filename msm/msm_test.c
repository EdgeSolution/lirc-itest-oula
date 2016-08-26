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


test_mod_t test_mod_msm = {
    .run = 1,
    .pass = 1,
    .name = "msm",
    .log_fd = -1,
    .test_routine = msm_test,
    .print_status = msm_print_status,
    .print_result = msm_print_result
};


#define ADVSPI_DEVICE   "/dev/advspi3962"

/* Data size is 64KB(Half size of nvSRAM) */
#define PACKET_SIZE     HALF_SIZE

/* Data pattern to write. */
char data_55[PACKET_SIZE];
char data_aa[PACKET_SIZE];

/* Counter of test times, successful and failed test. */
unsigned long counter_test = 0;
unsigned long counter_success = 0;
unsigned long counter_fail = 0;


static int read_data_a(int fd, char *cmp_buf);
static int read_data_b(int fd, char *cmp_buf);
static void log_result(int log_fd);
static int open_port(void);
static unsigned char get_data_pattern(int fd);
static int send_data_pattern(int fd, unsigned char pattern);


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
        write_file(fd, "MSM: PASS. Test time:%lu\n", counter_test);
    } else {
        write_file(fd, "MSM: FAIL. Test time:%lu;  Failed:%lu;\n",
            counter_test, counter_fail);
    }
}


void *msm_test(void *args)
{
    int log_fd = test_mod_msm.log_fd;

    log_print(log_fd, "Begin test!\n\n");

    int bytes;

    /* Open the device of storage. */
    int spi = open(ADVSPI_DEVICE, O_RDWR);
    if (spi == -1) {
        log_print(log_fd, "open storage device is Failed!\n");
        test_mod_msm.pass = 0;
        pthread_exit(NULL);
    }
    log_print(log_fd, "open storage device is Successful!\n");

    /* Open serial port */
    int com = open_port();
    if (com < 0) {
        log_print(log_fd, "open serial port Failed!\n");
        test_mod_msm.pass = 0;
        pthread_exit(NULL);
    }

    /* Set data pattern to write */
    memset(data_55, 0x55, sizeof(data_55));
    memset(data_aa, 0xAA, sizeof(data_aa));

    char *data = NULL;
    unsigned char pattern = 0;
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
            bytes = advspi_write_a(spi, data, PACKET_SIZE, 0);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                log_print(log_fd, "write %d bytes(%02X)   FALSE!\n", bytes, (unsigned char)data[0]);
                counter_fail++;
                test_mod_msm.pass = 0;
                continue;
            } else {
                log_print(log_fd, "write %d bytes(%02X)   OK!\n", bytes, (unsigned char)data[0]);
            }

            /* Set data pattern to other side */
            send_data_pattern(com, data[0]);
            
            sleep(10);

            /* Get data pattern from other side */
            pattern = get_data_pattern(com);

            /* Read data and verify */
            switch (pattern) {
            case 0xAA:
                bytes = read_data_b(spi, data_aa);
                break;
            
            case 0x55:
                bytes = read_data_b(spi, data_55);
                break;

            default:
                bytes = 0;
                break;
            }

            if (!g_running) {
                break;
            }

            /* Update counter */
            if (bytes != PACKET_SIZE) {
                log_print(log_fd, "read %d bytes(%02X)   FALSE!\n", bytes, pattern);
                counter_fail++;
                test_mod_msm.pass = 0;
            } else {
                log_print(log_fd, "read %d bytes(%02X)   OK!\n", bytes, pattern);
                counter_success++;
            }

            sleep(2);
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

            /* Read data and verify */
            switch (pattern) {
            case 0xAA:
                bytes = read_data_a(spi, data_aa);
                break;
            
            case 0x55:
                bytes = read_data_a(spi, data_55);
                break;

            default:
                bytes = 0;
                break;
            }

            if (!g_running) {
                break;
            }

            /* Update counter */
            if (bytes != PACKET_SIZE) {
                log_print(log_fd, "read %d bytes(%02X)   FALSE!\n", bytes, pattern);
                counter_fail++;
                test_mod_msm.pass = 0;
                continue;
            } else {
                log_print(log_fd, "read %d bytes(%02X)   OK!\n", bytes, pattern);
                counter_success++;
            }
            sleep(2);

            /* Write data */
            bytes = advspi_write_b(spi, data, PACKET_SIZE, 0);
            if (!g_running) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                log_print(log_fd, "write %d bytes(%02X)   FALSE!\n", bytes, (unsigned char)data[0]);
                counter_fail++;
                test_mod_msm.pass = 0;
                continue;
            } else {
                log_print(log_fd, "write %d bytes(%02X)   OK!\n", bytes, (unsigned char)data[0]);
            }
            
            /* Set data pattern to other side */
            send_data_pattern(com, data[0]);

            sleep(10);
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
 *      Read data from storage zone A, and compare it with predefined pattern.
 *
 * PARAMETERS:
 *      fd       - The fd of serial port
 *      cmp_buf  - The data to compare with the readed data
 *
 * RETURN:
 *      Number of bytes readed
 ******************************************************************************/
static int read_data_a(int fd, char *cmp_buf)
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
        return len;
    }

    return 0;
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
 *      cmp_buf  - The data to compare with the readed data
 *
 * RETURN:
 *      Number of bytes readed
 ******************************************************************************/
static int read_data_b(int fd, char *cmp_buf)
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
        return len;
    }

    return 0;
}


static void log_result(int log_fd)
{
    if (counter_fail > 0) {
        log_print(log_fd, "Test FALSE. Test time:%lu;  Failed:%lu;\n",
            counter_test, counter_fail);
        log_print(log_fd, "FAIL\n");
        test_mod_msm.pass = 0;
    } if (counter_success == 0) {
        log_print(log_fd, "Serial port communication error!\n");
        test_mod_msm.pass = 0;
    } else {
        log_print(log_fd, "Test OK. Test time:%lu\n", counter_test);
        log_print(log_fd, "PASS\n");
    }
}


/******************************************************************************
 * NAME:
 *      open_port
 *
 * DESCRIPTION: 
 *      Open the serial port of CCM,and return the fd of it.
 *
 * PARAMETERS:
 *      None 
 *
 * RETURN:
 *      The fd of serial port
 ******************************************************************************/
static int open_port(void)
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
