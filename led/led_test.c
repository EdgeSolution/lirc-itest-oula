/******************************************************************************
*
* FILENAME:
*     led_test.c
*
* DESCRIPTION:
*     Define functions for LED test.
*
* REVISION(MM/DD/YYYY):
*     07/28/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
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
#include "adv_led.h"
#include "led_test.h"

void led_print_status();
void led_print_result(int fd);
void *led_test(void *args);


test_mod_t test_mod_led = {
    .run = 1,
    .pass = 1,
    .name = "led",
    .log_fd = -1,
    .test_routine = led_test,
    .print_status = led_print_status,
    .print_result = led_print_result
};


void led_print_status()
{
    printf("%-*s %s\n", COL_FIX_WIDTH, "LED",
        test_mod_led.pass?STR_MOD_OK:STR_MOD_ERROR);
}

void led_print_result(int fd)
{
    if (test_mod_led.pass) {
        write_file(fd, "LED: PASS\n");
    } else {
        write_file(fd, "LED: FAIL\n");
    }
}


void *led_test(void *args)
{
    int log_fd = test_mod_led.log_fd;

    int fd = led_init();
    if (fd < 0) {
        log_print(log_fd, "Device open failure\n");
        test_mod_led.pass = 0;
        pthread_exit(NULL);
    }

    log_print(log_fd, "Begin test!\n\n");

    log_print(log_fd, "Test L1 L2 circularly\n\n");

    while (g_running) {
        //step1: turn on LED1 and LED2
        led_set(fd, LED_L1, LED_ON);
        led_set(fd, LED_L2, LED_ON);
        sleep(1);

        //step2: turn on LED1, turn off LED2
        led_set(fd, LED_L1, LED_ON);
        led_set(fd, LED_L2, LED_OFF);
        sleep(1);

        //step3: turn off LED1, turn on LED2
        led_set(fd, LED_L1, LED_OFF);
        led_set(fd, LED_L2, LED_ON);
        sleep(1);

        //step4: turn off LED1 and LED2
        led_set(fd, LED_L1, LED_OFF);
        led_set(fd, LED_L2, LED_OFF);
        sleep(1);
    }

    close(fd);
    log_print(log_fd, "Test end\n\n");
    pthread_exit(NULL);
}
