/******************************************************************************
*
* FILENAME:
*     adv_spi.c
*
* DESCRIPTION:
*     MIC-3962 API.
*
* REVISION(MM/DD/YYYY):
*     01/04/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version
*     03/01/2016  Jia Sui (jia.sui@advantech.com.cn)
*     - Add advspi_cpld_version()
*
******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "adv_spi.h"

#define SYSTEM_A_START    0
#define SYSTEM_B_START    HALF_SIZE


/******************************************************************************
 * NAME:
 *      advspi_open
 *
 * DESCRIPTION:
 *      Open the storage device. This function shall be called before any other
 *      storage functions, and it will return a fd can be used in other storage
 *      functions.
 *
 * PARAMETERS:
 *      None
 *
 * RETURN:
 *      >=0 - The fd of storage device.
 *      <0  - Error
 ******************************************************************************/
int advspi_open(void)
{
    int fd = open(ADVSPI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open driver error");
        return -1;
    }

    return fd;
}


/******************************************************************************
 * NAME:
 *      advspi_read_a
 *
 * DESCRIPTION:
 *      Read data from the half part of storage device of system A.
 *
 * PARAMETERS:
 *      fd    - File description of storage device(returned by advspi_open)
 *      buf   - Buffer to keep readed data
 *      count - Bytes to read (<= HALF_SIZE)
 *      addr  - Address(offset) to start read
 *
 * RETURN:
 *      Bytes readed.
 *      >0   OK
 *      <=0  Error
 ******************************************************************************/
ssize_t advspi_read_a(int fd, void *buf, size_t count, int addr)
{
    if ((addr < 0) || (addr + count > HALF_SIZE)) {
        return -1;
    }

    lseek(fd, SYSTEM_A_START+addr, SEEK_SET);

    ssize_t bytes = read(fd, buf, count);
    return bytes;
}

/******************************************************************************
 * NAME:
 *      advspi_read_b
 *
 * DESCRIPTION:
 *      Read data from the half part of storage device of system B.
 *
 * PARAMETERS:
 *      fd    - File description of storage device(returned by advspi_open)
 *      buf   - Buffer to keep readed data
 *      count - Bytes to read (<= HALF_SIZE)
 *      addr  - Address(offset) to start read
 *
 * RETURN:
 *      Bytes readed.
 *      >0   OK
 *      <=0  Error
 ******************************************************************************/
ssize_t advspi_read_b(int fd, void *buf, size_t count, int addr)
{
    if ((addr < 0) || (addr + count > HALF_SIZE)) {
        return -1;
    }

    lseek(fd, SYSTEM_B_START+addr, SEEK_SET);

    ssize_t bytes = read(fd, buf, count);
    return bytes;
}


/******************************************************************************
 * NAME:
 *      advspi_read
 *
 * DESCRIPTION:
 *      Read data from the whole storage device(system A and B).
 *
 * PARAMETERS:
 *      fd    - File description of storage device(returned by advspi_open)
 *      buf   - Buffer to keep readed data
 *      count - Bytes to read (<= NVSRAM_SIZE)
 *      addr  - Address(offset) to start read
 *
 * RETURN:
 *      Bytes readed.
 *      >0   OK
 *      <=0  Error
 ******************************************************************************/
ssize_t advspi_read(int fd, void *buf, size_t count, int addr)
{
    if ((addr < 0) || (addr + count > NVSRAM_SIZE)) {
        return -1;
    }

    lseek(fd, addr, SEEK_SET);

    ssize_t bytes = read(fd, buf, count);
    return bytes;
}


/******************************************************************************
 * NAME:
 *      advspi_write_a
 *
 * DESCRIPTION:
 *      Write data to the half part of storage device of system A.
 *
 * PARAMETERS:
 *      fd    - File description of storage device(returned by advspi_open)
 *      buf   - Data to write
 *      count - Bytes to write (<= HALF_SIZE)
 *      addr  - Address(offset) to start write
 *
 * RETURN:
 *      Bytes written
 *      >0   OK
 *      <=0  Error
 ******************************************************************************/
ssize_t advspi_write_a(int fd, const void *buf, size_t count, int addr)
{
    if ((addr < 0) || (addr + count > HALF_SIZE)) {
        return -1;
    }

    lseek(fd, SYSTEM_A_START+addr, SEEK_SET);

    ssize_t bytes = write(fd, buf, count);
    return bytes;
}


/******************************************************************************
 * NAME:
 *      advspi_write_b
 *
 * DESCRIPTION:
 *      Write data to the half part of storage device of system B.
 *
 * PARAMETERS:
 *      fd    - File description of storage device(returned by advspi_open)
 *      buf   - Data to write
 *      count - Bytes to write (<= HALF_SIZE)
 *      addr  - Address(offset) to start write
 *
 * RETURN:
 *      Bytes written
 *      >0   OK
 *      <=0  Error
 ******************************************************************************/
ssize_t advspi_write_b(int fd, const void *buf, size_t count, int addr)
{
    if ((addr < 0) || (addr + count > HALF_SIZE)) {
        return -1;
    }

    lseek(fd, SYSTEM_B_START+addr, SEEK_SET);

    ssize_t bytes = write(fd, buf, count);
    return bytes;
}

/******************************************************************************
 * NAME:
 *      advspi_cpld_version
 *
 * DESCRIPTION:
 *      Read CPLD version via ioctl
 *
 * PARAMETERS:
 *      fd    - File description of storage device(returned by advspi_open)
 *
 * RETURN:
 *      >=0   CPLD version
 *      <=0  Error
 ******************************************************************************/
#define SPI_IOC_MAGIC 'k'
#define IOCTL_VERSION   _IOR(SPI_IOC_MAGIC, 0x0C, unsigned char)
ssize_t advspi_cpld_version(int fd)
{
    uint8_t version;
    int err;

    err = ioctl(fd, IOCTL_VERSION, &version);
    if (err < 0) {
        return -1;
    }

    return version;
}

/******************************************************************************
 * NAME:
 *      advspi_close
 *
 * DESCRIPTION:
 *      Close the storage device
 *
 * PARAMETERS:
 *      fd - File description of storage device(returned by advspi_open)
 *
 * RETURN:
 *      0 - OK
 *      -1 - Error
 ******************************************************************************/
int advspi_close(int fd)
{
    return close(fd);
}
