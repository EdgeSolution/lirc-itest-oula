#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "term.h"

void tc_set_baudrate(int fd, int speed)
{
    struct termios opt;

    tcgetattr(fd, &opt);

    switch(speed)
    {
        case 300:
            cfsetispeed(&opt,B300);
            cfsetospeed(&opt,B300);
            break;

        case 1200:
            cfsetispeed(&opt,B1200);
            cfsetospeed(&opt,B1200);
            break;

        case 2400:
            cfsetispeed(&opt,B2400);
            cfsetospeed(&opt,B2400);
            break;

        case 4800:
            cfsetispeed(&opt,B4800);
            cfsetospeed(&opt,B4800);
            break;

        case 9600:
            cfsetispeed(&opt,B9600);
            cfsetospeed(&opt,B9600);
            break;

        case 19200:
            cfsetispeed(&opt,B19200);
            cfsetospeed(&opt,B19200);
            break;

        case 38400:
            cfsetispeed(&opt,B38400);
            cfsetospeed(&opt,B38400);
            break;

        case 57600:
            cfsetispeed(&opt,B57600);
            cfsetospeed(&opt,B57600);
            break;

        case 115200:
            cfsetispeed(&opt,B115200);
            cfsetospeed(&opt,B115200);
            break;

        case 230400:
            cfsetispeed(&opt,B230400);
            cfsetospeed(&opt,B230400);
            break;

#ifndef __APPLE__
        case 460800:
            cfsetispeed(&opt,B460800);
            cfsetospeed(&opt,B460800);
            break;

        case 576000:
            cfsetispeed(&opt,B576000);
            cfsetospeed(&opt,B576000);
            break;

        case 921600:
            cfsetispeed(&opt,B921600);
            cfsetospeed(&opt,B921600);
            break;
#endif

        default:
            printf("Invalid baudrate(%d), use 9600 instead.\n", speed);
            cfsetispeed(&opt,B9600);
            cfsetospeed(&opt,B9600);
            break;
    }

    tcsetattr(fd, TCSAFLUSH, &opt);
}

int tc_set_port(int fd, int databits, int stopbits, int parity)
{
    tcflush(fd, TCIOFLUSH);

    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        printf("Fail to get setup of serial port\n");
        return -1;
    }

    //Data bits
    options.c_cflag &= ~CSIZE;
    switch (databits) {
        case 5:
            options.c_cflag |= CS5;
            break;
        case 6:
            options.c_cflag |= CS6;
            break;
        case 7:
            options.c_cflag |= CS7;
            break;
        case 8:
        default:
            options.c_cflag |= CS8;
    }

    //Parity
    switch (parity) {
        case 1:
            options.c_cflag |= (PARENB | PARODD);
            break;
        case 2:
            options.c_cflag |= PARENB;
            options.c_cflag &= ~(PARODD);
            break;
        case 0:
        default:
            options.c_cflag &= ~PARENB;
    }

    //Stop bits
    switch (stopbits) {
        case 2:
            options.c_cflag |= CSTOPB;
            break;
        case 1:
        default:
            options.c_cflag &= ~CSTOPB;
    }

    tcsetattr(fd, TCSANOW, &options);

    return 0;
}

void tc_set_rts(int fd, char enabled)
{
    int flags;

    ioctl(fd, TIOCMGET, &flags);

    if (enabled == TRUE) {
        flags |= TIOCM_RTS;
    } else {
        flags &= ~TIOCM_RTS;
    }

    ioctl(fd, TIOCMSET, &flags);
}

void tc_set_dtr(int fd, char enabled)
{
    int flags;

    ioctl(fd, TIOCMGET, &flags);

    if (enabled == TRUE) {
        flags |= TIOCM_DTR;
    } else {
        flags &= ~TIOCM_DTR;
    }

    ioctl(fd, TIOCMSET, &flags);
}


int tc_init(char *dev)
{
    int fd;

    //Open device
    fd = open(dev, O_RDWR);
    if (fd == -1) {
        perror("open error");
        return -1;
    }

    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        printf("Fail to get setup of serial port\n");
        return -1;
    }

    cfmakeraw(&options);

    options.c_cc[VTIME] = 20;
    options.c_cc[VMIN] = 0;

    tcsetattr(fd, TCSAFLUSH, &options);

    return fd;
}

void tc_deinit(int fd)
{
    close(fd);
}

int tc_get_cts(int fd)
{
    int flags = 0;

    ioctl(fd, TIOCMGET, &flags);

    if (flags & TIOCM_CTS) {
        return TRUE;
    } else {
        return FALSE;
    }
}

int tc_get_rts(int fd)
{
    int flags = 0;

    ioctl(fd, TIOCMGET, &flags);

    if (flags & TIOCM_RTS) {
        return TRUE;
    } else {
        return FALSE;
    }
}
