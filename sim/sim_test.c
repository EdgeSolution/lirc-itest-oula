/******************************************************************************
 *
 * FILENAME:
 *     sim_test.c
 *
 * DESCRIPTION:
 *     Define functions for SIM test.
 *
 * REVISION(MM/DD/YYYY):
 *     09/06/2016  Yuechao Zhao (yuechao.zhao@advantech.com.cn)
 *     - Initial version
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <zlib.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include "cfg.h"
#include "common.h"
#include "sim_test.h"
#include "term.h"

#define MAX_RETRY_COUNT 3

#define TAIL 0xfe

#define BUFF_SIZE 265

/*Uart head 0xca5c051111 define*/
static const uint8_t head[5] = {
    0xca,
    0x5c,
    0x05,
    0x11,
    0x11
};

static const uint8_t stop_sign[5] = {
    0x7e,
    0x57,
    0xe1,
    0x1d,
    0xff
};

static char *port_list[MAX_SIM_PORT_COUNT] = {
    "/dev/ttyS2",
    "/dev/ttyS3",
    "/dev/ttyS4",
    "/dev/ttyS5",
    "/dev/ttyS6",
    "/dev/ttyS7",
    "/dev/ttyS8",
    "/dev/ttyS9",
    "/dev/ttyS10",
    "/dev/ttyS11",
    "/dev/ttyS12",
    "/dev/ttyS13",
    "/dev/ttyS14",
    "/dev/ttyS15",
    "/dev/ttyS16",
    "/dev/ttyS17"
};

struct uart_package {
    uint8_t pack_head[5];/*0xca5c051111*/
    uint8_t port_id;
    uint8_t pack_data[250];//0x00->0xF9
    uint8_t pack_tail;
    uint32_t  pack_num;
    uint32_t crc_err;
}__attribute__ ((packed));

struct uart_attr {
    int uart_fd;
    int baudrate;
    int port_id;
}__attribute__ ((packed));

struct uart_count_list {
    uint32_t err_count;//global variable,count packet loss or error
    uint32_t recv_count;//global variable, count received packet
    uint32_t send_count;//global variable, count send packet
    uint32_t target_send_num;//Record target amount of packets sent
    uint32_t lost_count;
    uint32_t timeout_count;
}__attribute__ ((packed));

static struct uart_count_list _uart_array[16];//init uart_count

static int read_pack_head_1_byte(int fd, uint8_t *buff, int port_id);

static void creat_uart_pack(struct uart_package *uart_pack, uint32_t pack_num, uint8_t port_id);
static int send_uart_packet(int fd, struct uart_package * packet_ptr, int len);
static int recv_uart_packet(int fd, uint8_t *buff, int len, int port_id);
static int analysis_packet(uint8_t *buff, int port_id);

static void *port_recv_event(void *args);
static void *port_send_event(void *args);
static void sim_print_status(void);
static void sim_print_result(int fd);
static void *sim_test(void *args);


//test_mod_t no define in this test
test_mod_t test_mod_sim = {
    .run = 1,
    .pass = 1,
    .name = "sim",
    .log_fd = -1,
    .test_routine = sim_test,
    .print_status = sim_print_status,
    .print_result = sim_print_result
};

/*
 * NAME:
 *     creat_uart_pack
 * Description:
 *     creat uart package
 * PARAMETERS:
 *     uart_pack: creat uart packet and save into uart_pack
 *     pack_num: count packet amount
 *     port_id: uart ID
 * Return:
 *
 */
static void creat_uart_pack(struct uart_package *uart_pack, uint32_t pack_num, uint8_t port_id)
{
    uint8_t i = 0;
    uint32_t crc = 0xFFFFFFFF;

    /*creat pack head*/
    for (i=0; i<5; i++) {
        uart_pack->pack_head[i] = head[i];
    }

    /*creat pack data*/
    uart_pack->port_id = port_id;
    for (i=0; i<=0xF9; i++) {
        uart_pack->pack_data[i] = i;
    }
    uart_pack->pack_tail = TAIL;
    uart_pack->pack_num = pack_num;

    /*
     * calculated CRC
     */
    crc = crc32(0, (uint8_t *)uart_pack, 261);
    uart_pack->crc_err = crc;
}

/*
 * Name:
 *      send_uart_packet
 * Description:
 *      send data
 * PARAMETERS:
 *      fd:file point
 *      packet_ptr:data point
len:data length
 * Return:
 *      send bytes
 */
static int send_uart_packet(int fd, struct uart_package * packet_ptr, int len)
{
    uint8_t buff[BUFF_SIZE];
    int ret = 0;
    int i = 0;
    int bytes = 0;
    int seg_len = 25;

    if (packet_ptr == NULL) {
        DBG_PRINT("Have no packet sent\n");
        return -1;
    }
    memcpy(buff, packet_ptr->pack_head, sizeof(packet_ptr->pack_head));

    buff[5] = packet_ptr->port_id;

    memcpy(buff + 6, packet_ptr->pack_data, sizeof(packet_ptr->pack_data));

    buff[256] = packet_ptr->pack_tail;

    memcpy(buff + 257, &packet_ptr->pack_num, sizeof(packet_ptr->pack_num));
    memcpy(buff + 261, &packet_ptr->crc_err, sizeof(packet_ptr->crc_err));

    for (i=0; i<len; ) {
        int j = 0;

        if ((len - i) < seg_len)
            seg_len = len - i;

        for(j=0; j<seg_len; ) {
            ret = write(fd, buff + i, seg_len - j);
            if (ret == -1) {
                sleep(1);
                continue;
            } else {
                bytes += ret;
                i += ret;
                j += ret;
            }
        }
        sleep_ms(4);
    }

    return bytes;
}


/*
 * Name:
 *      read_pack_head_1_byte
 * Description:
 *      read 1 data from uart
 * PARAMETERS:
 *      fd:file point
 *      buff:save data
 *      port_id:array id number
 * Return:
 *       read status
 */
static int read_pack_head_1_byte(int fd, uint8_t *buff, int port_id)
{
    int ret = 0;
    do {
        ret = read(fd, buff, 1);
        if (ret < 0) {
            buff[0] = 0; /* if read error, init buff[0] = 0*/
            DBG_PRINT("Read error\n");
            break;
        } else if (ret == 0) {
            buff[0] = 0;/*if read anything, init buff[0] = 0*/
            _uart_array[port_id].timeout_count++;
            DBG_PRINT("received timeout\n");
            break;
        } else if (ret == 1) {
            _uart_array[port_id].timeout_count = 0;
        }
    } while (ret != 1);
    return ret;
}

/*
 * Name:
 *      recv_uart_packet
 * Description:
 *      receive data
 * PARAMETERS:
 *      fd:file point
 *      buff:save data
 *      len:data length
 *      port_id:array id number
 * Return:
 *       received bytes
 */
static int recv_uart_packet(int fd, uint8_t *buff, int len, int port_id)
{
    int ret = 0;
    int i = 0;
    int bytes = 0;
    int retry_count = 0;

    int log_fd;

    log_fd = test_mod_sim.log_fd;

    /*matching head*/
    read_pack_head_1_byte(fd, buff, port_id);

    while (i < 5) {
        /*check head[i]*/
        if (buff[i] == head[i] || buff[i] == stop_sign[i]) {/*start if*/
            i++;

            if (i < 5) {
                read_pack_head_1_byte(fd, buff + i, port_id);
            }
        } else {
            DBG_PRINT("%s received HEAD%d data = %02x\n",
                    port_list[port_id], i, buff[i]);
            if (i == 0) {
                retry_count++;

                /*if timeout,not read again to avoid data loss*/
                if (retry_count <= MAX_RETRY_COUNT) {
                    read_pack_head_1_byte(fd, buff, port_id);
                }
            } else {
                retry_count++;
                buff[0] = buff[i];
                i = 0;
            }
        }/*end if*/

        /*timeout*/
        if (retry_count > MAX_RETRY_COUNT) {
            if (test_mod_sim.pass && _uart_array[port_id].timeout_count != 0) {
                log_print(log_fd, "COM-%d timeout, please check the port connection\n",
                        port_id+1);
            }

            if(g_running) {
                DBG_PRINT("%s check PACKET HEAD timeout when received %d packet\n",
                        port_list[port_id], _uart_array[port_id].recv_count);

                test_mod_sim.pass = 0;
            }
            return bytes;
        }
    }

    if (buff[4] == stop_sign[4]) {
        log_print(log_fd,"%s received stop signal, sim test will be stop\n", port_list[port_id]);
        return -1;/* means will be stop test*/
    }

    retry_count = 0;

    /*if head is not stop_sign then*/
    _uart_array[port_id].recv_count++;/* Packet Reception count +1*/
    bytes += 5;
    len -= 5;

    while (len > 0) {
        ret = read(fd, buff + bytes, len);
        if (ret < 0) {
            log_print(log_fd,"%s read error\n", port_list[port_id]);
            test_mod_sim.pass = 0;
            break;
        } else if (ret == 0) {
            if (++retry_count > MAX_RETRY_COUNT) {
                if (g_running) {
                    log_print(log_fd, "%s receive %d packet timeout\n",
                            port_list[port_id],
                            _uart_array[port_id].recv_count);
                    _uart_array[port_id].timeout_count += 3;
                    test_mod_sim.pass = 0;
                }
                break;
            } else {
                DBG_PRINT("Retry:%d\n", retry_count);
            }
        } else {
            bytes += ret;
            len = len - ret;
            _uart_array[port_id].timeout_count = 0;
        }
    } /* end while (len > 0) */

    return bytes;
}

/*
 * Name:
 *      analysis_packet
 * Description:
 *      check packet
 * PARAMETERS:
 *      recv_packet: packet
 *      buff:data
 *      port_id:array id number
 * Return:
 *      0: packet ok
 *      -1: packet error
 *
 */
static int analysis_packet(uint8_t *buff, int port_id)
{
    uint32_t crc_check;
    int i;
    int log_fd;
    int tmp;

    struct uart_package *recv_packet;
    recv_packet = (struct uart_package *)buff;

    log_fd = test_mod_sim.log_fd;

    /*
     * check crc and printf which data is error
     */
    crc_check = crc32(0, (uint8_t *)recv_packet, 261);
    if ((uint32_t)crc_check != (uint32_t)recv_packet->crc_err) {
        if (g_running) {
            /*means received error packet*/
            log_print(log_fd, "%s Received \"%d\"packet error\n",
                    port_list[port_id], _uart_array[port_id].recv_count);
            write_file(log_fd, "    ");
            /*dump received data*/
            for (i = 0; i < 257; i++) {/*print received pack_head & port_id &pack_data*/
                write_file(log_fd, " %02X", *((uint8_t *)buff + i));
                if (((i+1) % 16) == 0) {
                    write_file(log_fd, "\n");
                    write_file(log_fd, "    ");
                }
            }
            write_file(log_fd, "\n    Received pack_num = %u\n", (uint32_t)recv_packet->pack_num);
            write_file(log_fd, "    Received crc = %08X\n", (uint32_t)recv_packet->crc_err);
            write_file(log_fd, "    Calculated crc = %08X\n", (uint32_t)crc_check);

            _uart_array[port_id].err_count++;
            test_mod_sim.pass = 0;
        }

        return -1;
    } else {
        _uart_array[port_id].target_send_num = recv_packet->pack_num;
        tmp = _uart_array[port_id].target_send_num - _uart_array[port_id].recv_count;
        if (tmp > 0) {
            test_mod_sim.pass = 0;
            if (tmp != _uart_array[port_id].lost_count) {
                _uart_array[port_id].lost_count = tmp;
                log_print(log_fd, "%s lost %d package\n",
                        port_list[port_id],
                        _uart_array[port_id].lost_count);
            }
        }
    }

    //Check UART ID
    int sender_id = recv_packet->port_id;
    if (port_id != sender_id) {
        test_mod_sim.pass = 0;
        log_print(log_fd,
                "Mismatched port: sender COM-%d, receiver COM-%d\n",
                sender_id+1, port_id+1);
        g_running = 0;
        return -1;
    }

    return 0;
}


/*
 * Name:
 *      port_recv_event
 * Description:
 *      The thread routine to read data from serial port and verify them
 * PARAMETERS:
 *      uart_param: Argument of thread routine
 * Return:
 *      Exit code of thread
 *
 */
static void *port_recv_event(void *args)
{
    struct uart_attr *uart_param;
    uint8_t buff[BUFF_SIZE];
    int fd;
    int log_fd;

    int port_id;

    int n;
    int status;

    uart_param = (struct uart_attr *)args;

    fd = uart_param->uart_fd;
    if (fd < 0) {
        pthread_exit((void *)-1);
    }

    port_id = uart_param->port_id;

    log_fd = test_mod_sim.log_fd;

    while (g_running) {
        n = recv_uart_packet(fd, buff, BUFF_SIZE, port_id);
        if (n != BUFF_SIZE) {
            if (n == -1) { /*received stop signal*/
                g_running = 0;
                pthread_exit((void *)0);
            } else if (n == 0) {
                continue; //Fix unexpect error count increase
            }
            DBG_PRINT("recv_uart_packet data error\n");
        }

        status = analysis_packet(buff, port_id);
        if (status != 0) {
            if (g_running) {
                test_mod_sim.pass = 0;
                log_print(log_fd, "Analyze packet fail\n");
            }
        } else {
            if (_uart_array[port_id].recv_count % 1000 == 0) {
                log_print(log_fd,"%s received %d packet successfully\n",
                    port_list[port_id],
                    (uint32_t)_uart_array[port_id].recv_count);
            }
        }
    } /*end while(g_running)*/

    log_print(log_fd, "COM-%d: send: %lu, recv: %lu, error: %lu\n",
            port_id+1,
            _uart_array[port_id].send_count,
            _uart_array[port_id].recv_count,
            _uart_array[port_id].err_count);

    pthread_exit((void *)0);
}

/*
 * Name:
 *      port_send_event
 * Description:
 *      The thread routine to write data to serial port
 * PARAMETERS:
 *      uart_param: Argument of thread routine
 * Return:
 *      Exit code of thread
 *
 */
static void *port_send_event(void *args)
{
    struct uart_attr *uart_param;

    int fd;
    int log_fd;

    int port_id;

    int n;
    int i;

    struct uart_package uart_pack = {{0}};

    uart_param = (struct uart_attr *)args;

    log_fd = test_mod_sim.log_fd;

    fd = uart_param->uart_fd;

    if (fd < 0) {
        pthread_exit((void *)-1);
    }

    port_id = uart_param->port_id;

    sleep(3);/* waiting received thread ready */

    _uart_array[port_id].send_count = 0;

    while (g_running) {
        _uart_array[port_id].send_count++;
        creat_uart_pack(&uart_pack, _uart_array[port_id].send_count, port_id);

        n = send_uart_packet(fd, &uart_pack, BUFF_SIZE);
        if (n != BUFF_SIZE) {
            log_print(log_fd, "%s send data error\n", port_list[port_id]);
            test_mod_sim.pass = 0;
        } else {
            if (_uart_array[port_id].send_count % 1000 == 0) {
                log_print(log_fd,"%s send %d packet ok\n", port_list[port_id],
                    (uint32_t)_uart_array[port_id].send_count);
            }
        }
    }

    /*if g_running == 0, send stop mark to other machine*/
    if(g_running == 0) {
        for (i=0; i<5; ) {
            n = write(fd, stop_sign + i, 5 - i);
            if (n == -1) {
                sleep(1);
                continue;
            } else {
                i += n;
            }
        }
    }

    pthread_exit((void *)0);
}

void hsm_switch2b(int log_fd)
{
    char *buf = "0123456789ABCDEF";

    int fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        log_print(log_fd, "open mac %c at %s is Failed!\n", g_machine, CCM_SERIAL_PORT);
        test_mod_sim.pass = 0;
        pthread_exit(NULL);
    } else {
        log_print(log_fd, "open mac %c at %s is Successful!\n", g_machine, CCM_SERIAL_PORT);
    }

    //Switch host to B.
    if (g_machine == 'A') {
        tc_set_rts_casco(fd, FALSE);
    } else {
        tc_set_rts_casco(fd, TRUE);

        if (send_packet(fd, buf, sizeof(buf)) < 0) {
            log_print(log_fd, "Switch HOST to B FAIL\n");
            test_mod_sim.pass = 0;
            pthread_exit(NULL);
        } else {
            log_print(log_fd, "Switch HOST to B SUCCESS\n");
        }
    }

    close(fd);
}

/*
 * Name:
 *      sim_test
 * Description:
 *      test uart
 * PARAMETERS:
 *      NULL
 * Return:
 *      NULL
 */
static void *sim_test(void *args)
{

    struct uart_attr uart_param[16];
    pthread_t th_send_id[16];
    long th_send_stat[16];

    pthread_t th_recv_id[16];
    long th_recv_stat[16];

    int port_num;

    int i;
    int fd;

    int log_fd = test_mod_sim.log_fd;

    print_version(log_fd, "SIM");

    //Wait for HSM switch test end
    while (g_running && g_hsm_switching) {
        sleep(2);
    }

    if (!g_running) {
        pthread_exit(NULL);
    }

    if (!g_test_hsm) {
        hsm_switch2b(log_fd);
    }

    //Sleep 2 seconds before start testing
    sleep(2);

    /*init global _uart_array*/
    memset(_uart_array, 0, sizeof(struct uart_count_list));

    port_num = g_port_num;

    /*init uart_param*/
    for (i=0; i<port_num; i++) {
        fd = tc_init(port_list[i]);
        if (fd < 0) {
            log_print(log_fd, "Open %s error\n", port_list[i]);
            test_mod_sim.pass = 0;
            uart_param[i].uart_fd = -1;
            uart_param[i].baudrate = g_baudrate;
            uart_param[i].port_id = i;
            continue;
        }

        /* Set databits, stopbits, parity ... */
        if (tc_set_port(fd, 8, 1, 0) == -1) {
            tc_deinit(fd);
            test_mod_sim.pass = 0;
            log_print(log_fd, "Set port fail\n");
        }

        tc_set_baudrate(fd, g_baudrate);

        /*assigned value to a struct uart_attr*/
        uart_param[i].uart_fd = fd;
        uart_param[i].baudrate = g_baudrate;
        uart_param[i].port_id = i;
    }

    log_print(log_fd, "Begin test!\n\n");

    /*creat pthread for received*/
    for (i=0; i<port_num; i++) {
        pthread_create(&th_recv_id[i], NULL, port_recv_event, &uart_param[i]);
    }

    for (i=0; i<port_num; i++) {
        pthread_create(&th_send_id[i], NULL, port_send_event, &uart_param[i]);
    }

    for (i=0; i<port_num; i++) {
        pthread_join(th_recv_id[i], (void *)&th_recv_stat[i]);
        pthread_join(th_send_id[i], (void *)&th_send_stat[i]);
    }

    /* Waiting read end, not use pthread_join,
     * because it will be blocking and not exits successfully */
    sleep(1);

    for (i = 0; i < port_num; i++) {
        tc_deinit(uart_param[i].uart_fd);
    }

    log_print(log_fd, "Test %s\n", test_mod_sim.pass?"PASS":"FAIL");
    log_print(log_fd, "Test end\n\n");
    pthread_exit(NULL);
}

/*
 * Name:
 *      sim_print_result
 * Description:
 *      print test result
 * PARAMETERS:
 *      fd: log file point
 * Return:
 *      NULL
 */
static void sim_print_result(int fd)
{
    if (test_mod_sim.pass) {
        if (g_hsm_switching) {
            write_file(fd, "SIM: Not Started\n");
        } else {
            write_file(fd, "SIM: PASS\n");
        }
    } else {
        write_file(fd, "SIM: FAIL\n");
    }
}

/*
 * Name:
 *      sim_print_status
 * Description:
 *      print ,pactet loss rate,error rate ..
 * PARAMETERS:
 *      NULL
 * Return:
 *      NULL
 */
static void sim_print_status(void)
{
    int i;
    int port_num = g_port_num;

    if (g_hsm_switching) {
        printf("%-*s %s\n",
                COL_FIX_WIDTH, "SIM", "Awaiting");
        return;
    }

    printf("%-*s %s\n",
        COL_FIX_WIDTH, "SIM", (test_mod_sim.pass) ? STR_MOD_OK : STR_MOD_ERROR);
    for (i=0; i<port_num; i++) {
        if (_uart_array[i].timeout_count > 0) {
            printf("%-*s SENT(PKT):%-*u TIMEOUT(%us)\n",
                COL_FIX_WIDTH, port_list[i],
                COL_FIX_WIDTH-10, _uart_array[i].send_count,
                _uart_array[i].timeout_count * 2);
        } else {
            printf("%-*s SENT(PKT):%-*u LOST(PKT):%-*u ERR(PKT):%-*u\n",
                COL_FIX_WIDTH, port_list[i],
                COL_FIX_WIDTH-10, _uart_array[i].send_count,
                COL_FIX_WIDTH-10, _uart_array[i].lost_count,
                COL_FIX_WIDTH-9, _uart_array[i].err_count);
        }
    }
}
