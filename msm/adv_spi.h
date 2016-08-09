/******************************************************************************
 *
 * FILENAME:
 *     adv_spi.h
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
#ifndef _ADV_ST_H_
#define _ADV_ST_H_

#define ADVSPI_DEVICE   "/dev/advspi3962"

/* The size of SPI nvSRAM */
#define NVSRAM_SIZE     (128*1024)
#define HALF_SIZE       (NVSRAM_SIZE/2)

/*
 * Open the storage device. This function shall be called before any other
 * storage functions. It will return a fd can be used in other storage
 * functions, or -1 if an error occurred.
 */
int advspi_open(void);

/*
 * Read data from the half part of storage device of system A.
 * On success, the number of bytes readed is returned(this number may be smaller
 * than the number of bytes requested). On error, -1 is returned.
 */
ssize_t advspi_read_a(int fd, void *buf, size_t count, int addr);

/*
 * Read data from the half part of storage device of system B.
 * On success, the number of bytes readed is returned(this number may be smaller
 * than the number of bytes requested). On error, -1 is returned.
 */
ssize_t advspi_read_b(int fd, void *buf, size_t count, int addr);

/*
 * Read data from the whole storage device(system A and B).
 * On success, the number of bytes readed is returned(this number may be smaller
 * than the number of bytes requested). On error, -1 is returned.
 */
ssize_t advspi_read(int fd, void *buf, size_t count, int addr);

/*
 * Write data to the half part of storage device of system A.
 * On success, the number of bytes written is returned (zero indicates nothing
 * was written). On error, -1 is returned.
 */
ssize_t advspi_write_a(int fd, const void *buf, size_t count, int addr);

/*
 * Write data to the half part of storage device of system B.
 * On success, the number of bytes written is returned (zero indicates nothing
 * was written). On error, -1 is returned.
 */
ssize_t advspi_write_b(int fd, const void *buf, size_t count, int addr);

/*
 * Read CPLD version via ioctl
 * On success, the version of CPLD is returned.
 * On error, -1 is returned.
 */
ssize_t advspi_cpld_version(int fd);

/*
 * Close the storage device.
 */
int advspi_close(int fd);

#endif /* _ADV_ST_H_ */
