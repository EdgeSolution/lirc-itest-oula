/******************************************************************************
 *
 * FILENAME:
 *     adv_hwb.h
 *
 * DESCRIPTION:
 *     Functions for Advantech hardware binding.
 *
 * REVISION(MM/DD/YYYY):
 *     08/09/2018  Jia Sui (jia.sui@advantech.com.cn)
 *     - Initial version
 *
 ******************************************************************************/
#ifndef _ADV_HWB_H_
#define _ADV_HWB_H_ 1

#ifdef __cplusplus
    extern "C" {
#endif

#if defined(__i386__) || defined(__x86_64__)
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define ADVBIOS_OFFSET  0xE0000
#define ADVBIOS_SIZE    0x20000

/******************************************************************************
 * NAME:
 *      adv_hwb
 *
 * DESCRIPTION:
 *      Check if the platform is supported
 *
 * PARAMETERS:
 *      None
 *
 * RETURN:
 *      1: Supported
 *      0: Not supported
 ******************************************************************************/
char adv_hwb(void)
{
    int found = 0;
    ssize_t size = 0;
    int i = 0;

    char buf[ADVBIOS_SIZE] = {0};

    int fd = open("/dev/mem", O_RDONLY);
    if (fd < 0) {
        return found;
    }

    off_t offset = lseek(fd, ADVBIOS_OFFSET, SEEK_SET);
    if (offset == -1) {
        goto exit;
    }

    size = read(fd, buf, ADVBIOS_SIZE);
    if (size <= 0) {
        goto exit;
    }

    for (i=0; i<size; i++) {
        if (buf[i] == '*'
                && buf[i+1] == '*'
                && buf[i+2] == '*'
                && buf[i+3] == '*'
                && buf[i+4] == ' ') {
                found = 1;
                break;
        }
    }

exit:
    close(fd);

    return found;
}
#else
char adv_hwb(void)
{
    return 1;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* ifndef _ADV_HWB_H_ */
