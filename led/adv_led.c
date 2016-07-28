/******************************************************************************
*
* FILENAME:
*     adv_led.c
*
* DESCRIPTION:
*     MIC-3329 LED functions.
*
* REVISION(MM/DD/YYYY):
*     06/12/2015  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version 
*
******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "adv_led.h"


/******************************************************************************
 * NAME:
 *      led_init
 *
 * DESCRIPTION: 
 *      Do initialization work for LED. This function shall be called before
 *      any other LED functions.
 *
 * PARAMETERS:
 *      None 
 *
 * RETURN:
 *      >=0 - The fd of GPIO device.
 *      <0  - Error
 ******************************************************************************/
int led_init()
{
    int fd = open(ADVGPIO_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open driver error");
        return -1;
    }

    if (ioctl(fd, ADVGPIO_LED_INIT, NULL) < 0) {
        perror("LED init error");
        close(fd);
        return -1;
    }
    return fd;
}


/******************************************************************************
 * NAME:
 *      led_set
 *
 * DESCRIPTION: 
 *      Set one LED to ON or OFF
 *
 * PARAMETERS:
 *      led - The index of LED
 *      st  - State of LED: 0 - off, 1 - on
 *
 * RETURN:
 *      0 - OK
 *      Others - Error
 ******************************************************************************/
int led_set(int fd, uint8_t led, uint8_t st)
{
    set_led_t led_arg;
    led_arg.led_index = led; 
    led_arg.led_state = st;

    if (ioctl(fd, ADVGPIO_LED_SET, &led_arg) < 0) {
        perror("Set LED error");
        return -1;
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      led_set_all
 *
 * DESCRIPTION: 
 *      Set state of all LEDs
 *
 * PARAMETERS:
 *      st  - Bitmask for all LEDs, each bit used to control one LED:
 *              1 indicates the LED need to be set(on)
 *              0 indicates the LED need to be clear(off)
 *
 * RETURN:
 *      0 - OK
 *      Others - Error
 ******************************************************************************/
int led_set_all(int fd, uint8_t st)
{
    if (ioctl(fd, ADVGPIO_LED_SET_ALL, &st) < 0) {
        perror("Set LED(ALL) error");
        return -1;
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      led_blink
 *
 * DESCRIPTION: 
 *      Start/stop blinking LED
 *
 * PARAMETERS:
 *      index - The index of the LED.
 *      st    - Start/stop blinking LED, 1 - start, 0 - stop.
 *      freq  - The frequency of LED blinking,
 *              0: 0.5Hz; 1: 1Hz;  2: 3Hz; 3: 10Hz.
 *
 * RETURN:
 *      0 - OK
 *      Others - Error
 ******************************************************************************/
int led_blink(int fd, uint8_t led, uint8_t st, uint8_t freq)
{
    blink_led_t led_arg;
    led_arg.led_index = led; 
    led_arg.blink_state = st;
    led_arg.blink_freq = freq;

    if (ioctl(fd, ADVGPIO_LED_BLINK, &led_arg) < 0) {
        perror("Set LED error");
        return -1;
    }

    return 0;
}
