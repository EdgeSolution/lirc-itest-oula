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

#define PACKET_SIZE     15

/* Data pattern sent by machine A and B. */
char data_a[PACKET_SIZE+1] = "123456789ABCDEF\0";
char data_b[PACKET_SIZE+1] = "FEDCBA987654321\0";

/* Counter of test times, successful and failed test. */
unsigned long counter_test = 0;
unsigned long counter_success = 0;
unsigned long counter_fail = 0;

/* The flag to inform the program exit from the loop */
int exit_flag = 0;

int write_data(int fd, int addr, char *buf);
int read_data(int fd, int addr, char *cmp_buf);
void print_result(void);


void msm_print_status()
{
    printf("MSM \t\tMODULE is OK\n"
        "TEST:%lu \tERROR:%lu\n",
        counter_test, counter_fail);
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


    if (g_machine == 'A') {
        while (g_running) {
            if (exit_flag) {
                break;
            }

            counter_test++;

            /* Write data */
            bytes = write_data(spi, addr, data_a);
            if (exit_flag) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("write %d bytes: %-15s    FALSE!\n", bytes, " ");
                counter_fail++;
                continue;
            } else {
                printf("write %d bytes: %-15s    OK!\n", bytes, data_a);
            }
            sleep(10);

            bytes = read_data(spi, addr, data_b);
            if (exit_flag) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("read %d bytes: %-15s     FALSE!\n", bytes, " ");
                counter_fail++;
            } else {
                printf("read %d bytes: %-15s     OK!\n", bytes, data_b);
                counter_success++;
            }
            sleep(2);
        }
    } else {
        sleep(10);
        while (g_running) {
            if (exit_flag) {
                break;
            }

            counter_test++;

            bytes = read_data(spi, addr, data_a);
            if (exit_flag) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("read %d bytes: %-15s     FALSE!\n", bytes, " ");
                counter_fail++;
                continue;
            } else {
                printf("read %d bytes: %-15s     OK!\n", bytes, data_a);
                counter_success++;
            }
            sleep(2);

            bytes = write_data(spi, addr, data_b);
            if (exit_flag) {
                break;
            }
            if (bytes != PACKET_SIZE) {
                printf("write %d bytes: %-15s    FALSE!\n", bytes, " ");
                counter_fail++;
                continue;
            } else {
                printf("write %d bytes: %-15s    OK!\n", bytes, data_b);
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
 *      write_data
 *
 * DESCRIPTION:
 *      Write data to storage adapter.
 *
 * PARAMETERS:
 *      fd   - The fd of serial port
 *      addr - The offset to start write
 *      buf  - The buffer of data
 *
 * RETURN:
 *      Number of bytes wrote
 ******************************************************************************/
int write_data(int fd, int addr, char *buf)
{
    int bytes = 0;
    int ret = 0;
    int len = PACKET_SIZE;

    lseek(fd, addr, SEEK_SET);
    while (len > 0) {
        if (exit_flag) {
            break;
        }

        ret = write(fd, buf, len);
        if (ret <= 0) {
            //sleep(1);
            continue;
        }

        bytes += ret;
        len = len - ret;
    }

    return bytes;
}


/******************************************************************************
 * NAME:
 *      read_data
 *
 * DESCRIPTION:
 *      Read data from storage adapter, and compare it with predefined pattern.
 *
 * PARAMETERS:
 *      fd       - The fd of serial port
 *      addr     - The offset to start read
 *      cmp_buf  - The data to compare with the readed data
 *
 * RETURN:
 *      Number of bytes readed
 ******************************************************************************/
int read_data(int fd, int addr, char *cmp_buf)
{
    int ret = 0;
    int bytes = 0;
    int len = PACKET_SIZE;
    char buf[PACKET_SIZE];

    memset(buf, 0, sizeof(buf));

    while (!exit_flag) {
        bytes = 0;
        len = PACKET_SIZE;

        lseek(fd, addr, SEEK_SET);

        while (len > 0) {
            if (exit_flag) {
                break;
            }

            ret = read(fd, buf, len);
            if (ret <= 0) {
                sleep(1);
                continue;
            }

            bytes += ret;
            len = len - ret;
        }

        if (memcmp(cmp_buf, buf, PACKET_SIZE) == 0) {
            break;
        }
        sleep(5);
    }

    return bytes;
}


void print_result(void)
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
