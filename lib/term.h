#ifndef __TERM_H__
#define __TERM_H__

#ifndef TRUE
#define TRUE        1
#endif
#ifndef FALSE
#define FALSE       0
#endif

void tc_set_baudrate(int fd, int speed);
int tc_set_port(int fd, int databits, int stopbits, int parity);
void tc_set_rts(int fd, char enabled);
void tc_set_dtr(int fd, char enabled);
int tc_init(char *dev);
void tc_deinit(int fd);
int tc_get_cts(int fd);
int tc_get_rts(int fd);

#endif /* __TERM_H__ */
