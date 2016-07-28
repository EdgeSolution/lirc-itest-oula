/******************************************************************************
 *
 * FILENAME:
 *     advgpio.h
 *
 * DESCRIPTION:
 *     MIC-3329 GPIO(LED/Watchdog) driver.
 *
 * REVISION(MM/DD/YYYY):
 *     07/03/2015  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _ADV_GPIO_H_
#define _ADV_GPIO_H_
#include <linux/ioctl.h>
#ifndef __KERNEL__
#include <stdint.h>
#endif

#define DEVICE_NAME             "advgpio"
#define ADVGPIO_DEVICE          "/dev/advgpio"


/* The index of LED */
#define LED_U1                  0
#define LED_U2                  1
#define LED_L1                  LED_U1
#define LED_L2                  LED_U2

/* The state of LED */
#define LED_OFF     0
#define LED_ON      1

typedef struct _set_led_t {
    uint8_t led_index;  /* LED_U1(0), LED_U2(1) ... */
    uint8_t led_state;  /* LED_OFF(0), LED_ON(1) */
} set_led_t;

typedef struct _blink_led_t {
    uint8_t led_index;      /* LED_U1(0), LED_U2(1) ... */
    uint8_t blink_freq;     /* 0: 0.5Hz; 1: 1Hz;  2: 3Hz; 3: 10Hz */
    uint8_t blink_state;    /* 0: Stop blink; 1: Start blink */
} blink_led_t;

typedef struct _reg_access_t {
    uint8_t reg;    /* The offset of register */
    uint8_t val;    /* Value of register */
} reg_access_t;

typedef struct _reg_set_t {
    uint8_t reg;    /* The offset of register */
    uint8_t bit;    /* The bit of register */
    uint8_t val;   /* Value of register: 0 or 1 only */
} reg_set_t;

/* The unit of watchdog time count */
#define ENABLE_UNIT_MS          1   /* Enable unit 250ms */
#define ENABLE_UNIT_SEC         2   /* Enalbe unit 1 second */


/********************************************************
 * ioctl commands
 ********************************************************/

/* Magic number, refer ioctl-number.txt */
#define ADVGPIO_IOC_MAGIC       0xA5

/* Init GPIO(LED) pins */
#define ADVGPIO_LED_INIT        _IO(ADVGPIO_IOC_MAGIC, 0)

/* Set state of one LED */
#define ADVGPIO_LED_SET         _IOW(ADVGPIO_IOC_MAGIC, 1, set_led_t)

/* Set state of all LED */
#define ADVGPIO_LED_SET_ALL     _IOW(ADVGPIO_IOC_MAGIC, 2, uint8_t)

/* Set the unit of Watchdog and enable Watchdog */
#define ADVGPIO_WDT_UNIT        _IOW(ADVGPIO_IOC_MAGIC, 3, uint8_t)

/* Set the time count of Watchdog */
#define ADVGPIO_WDT_SET_COUNT   _IOW(ADVGPIO_IOC_MAGIC, 4, uint8_t)

/* Get the current time count of Watchdog */
#define ADVGPIO_WDT_GET_COUNT   _IOR(ADVGPIO_IOC_MAGIC, 5, uint8_t)

/* Feed the Watchdog (keep the system alive) */
#define ADVGPIO_WDT_KEEPALIVE   _IO(ADVGPIO_IOC_MAGIC, 6)

/* Disable the Watchdog */
#define ADVGPIO_WDT_DISABLE     _IO(ADVGPIO_IOC_MAGIC, 7)

/* Write a register */
#define ADVGPIO_REG_WRITE       _IOW(ADVGPIO_IOC_MAGIC, 8, reg_access_t)

/* Read a register */
#define ADVGPIO_REG_READ        _IOWR(ADVGPIO_IOC_MAGIC, 9, reg_access_t)

/* Set a bit of register */
#define ADVGPIO_REG_SET_BIT     _IOW(ADVGPIO_IOC_MAGIC, 10, reg_set_t)

/* Blink LED */
#define ADVGPIO_LED_BLINK       _IOW(ADVGPIO_IOC_MAGIC, 11, blink_led_t)

/* The MAX value of ioctl number */
#define ADVGPIO_IOC_NR_MAX      11


#endif /* _ADV_GPIO_H_ */
