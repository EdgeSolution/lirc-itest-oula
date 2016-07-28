/******************************************************************************
 *
 * FILENAME:
 *     adv_led.h
 *
 * DESCRIPTION:
 *     MIC-3329 LED functions.
 *
 * REVISION(MM/DD/YYYY):
 *     06/12/2015  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _ADV_LED_H_
#define _ADV_LED_H_
#include "advgpio.h"

/*
 * Do initialization work for LED. This function shall be called before
 * any other LED functions, and it will return a fd can be used in other
 * LED functions.
 */
int led_init();

/*
 * Set LED to ON or OFF
 *      led - The index of LED
 *      st  - State of LED: 0 - off, 1 - on
 */
int led_set(int fd, uint8_t led, uint8_t st);

/*
 * Set state of all LEDs
 *   st  - Bitmask for all LEDs, each bit used to control one LED:
 *           1 indicates the LED need to be set(on)
 *           0 indicates the LED need to be clear(off)
 */
int led_set_all(int fd, uint8_t st);

/*
 * Start/stop blinking LED
 *   index - The index of the LED.
 *   st    - Start/stop blinking LED, 1 - start, 0 - stop.
 *   freq  - The frequency of LED blinking,
 *           0: 0.5Hz; 1: 1Hz;  2: 3Hz; 3: 10Hz.
 */
int led_blink(int fd, uint8_t led, uint8_t st, uint8_t freq);

#endif /* _ADV_LED_H_ */
