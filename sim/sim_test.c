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
#include <malloc.h>
#include <signal.h>
#include <pthread.h>
#include "sim_test.h"
#include "term.h"

#define MAX_RETRY_COUNT 20

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

static char *port_list[16] = {
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

static const uint8_t uart_id_list[16] = {
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17
};

struct uart_package {
    uint8_t pack_head[5];/*0xca5c051111*/
    uint8_t uart_id;
    uint8_t pack_data[250];//0x00->0xF9
    uint8_t pack_tail;
    uint32_t  pack_num;
    uint32_t crc_err;
}__attribute__ ((packed));

struct uart_attr {
    int uart_fd;
    int baudrate;
    int list_id;
}__attribute__ ((packed));

struct uart_count_list {
	uint32_t err_count;//global variable,count packet loss or error
	uint32_t recv_pack_count;//global variable, count received packet
	uint32_t send_pack_count;//global variable, count send packet
	uint32_t target_send_pack_num;//Record target amount of packets sent
}__attribute__ ((packed));

static struct uart_count_list _uart_array[16];//init uart_count
static float _rate[16];

int read_pack_head_1_byte(int fd, uint8_t *buff);

void creat_uart_pack(struct uart_package *uart_pack, uint32_t pack_num, uint8_t uart_id);
int send_uart_packet(int fd, struct uart_package * packet_ptr, int len);
int recv_uart_packet(int fd, uint8_t *buff, int len, int list_id);
int analysis_packet(uint8_t *buff, int list_id);

void *port_recv_event(void *args);
void *port_send_event(void *args);
void sim_print_status(void);
void sim_print_result(int fd);
void *sim_test(void *args);


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
 *     uart_id: uart ID
 * Return:
 *
 */
void creat_uart_pack(struct uart_package *uart_pack, uint32_t pack_num, uint8_t uart_id)
{
    uint8_t i = 0;
    uint32_t crc = 0xFFFFFFFF;

    /*creat pack head*/
    for (i=0; i<5; i++) {
        uart_pack->pack_head[i] = head[i];
    }

    /*creat pack data*/
    uart_pack->uart_id = uart_id;
    for (i=0; i<=0xF9; i++) {
        uart_pack->pack_data[i] = i;
    }
    uart_pack->pack_tail = TAIL;
    uart_pack->pack_num = pack_num;

    /*
     * calculated CRC
     */
    crc = crc32(0, uart_pack->pack_head, 5);
    crc = crc32(crc, &uart_pack->uart_id, 1);
    crc = crc32(crc, uart_pack->pack_data, 250);
    crc = crc32(crc, &uart_pack->pack_tail, 1);
    crc = crc32(crc, (uint8_t *)&uart_pack->pack_num, 4);
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
int send_uart_packet(int fd, struct uart_package * packet_ptr, int len)
{
    uint8_t buff[BUFF_SIZE];
    int ret = 0;
    int i = 0;
    int bytes = 0;

    if (packet_ptr == NULL) {
        DBG_PRINT("Have no packet sent\n");
        return -1;
    }
    memcpy(buff + 0, packet_ptr->pack_head, sizeof(packet_ptr->pack_head));

    buff[5] = packet_ptr->uart_id;

    memcpy(buff + 6, packet_ptr->pack_data, sizeof(packet_ptr->pack_data));

    buff[256] = packet_ptr->pack_tail;

    memcpy(buff + 257, &packet_ptr->pack_num, sizeof(packet_ptr->pack_num));
    memcpy(buff + 261, &packet_ptr->crc_err, sizeof(packet_ptr->crc_err));

    for (i=0; i<len; ) {
        ret = write(fd, buff + i, len - i);
        if (ret == -1) {
            sleep(1);
            continue;
        } else {
            bytes += ret;
            i += ret;
        }
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
 * Return:
 *       read status
 */
int read_pack_head_1_byte(int fd, uint8_t *buff)
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
            DBG_PRINT("received timeout\n");
            break;
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
 *      list_id:array id number
 * Return:
 *       received bytes
 */
int recv_uart_packet(int fd, uint8_t *buff, int len, int list_id)
{
    int ret = 0;
    int i = 0;
    int bytes = 0;
    int retry_count = 0;

    int log_fd;

    log_fd = test_mod_sim.log_fd;

    /*matching head*/
    ret = read_pack_head_1_byte(fd, buff + 0);
    while (i < 5) {
        /*check head[i]*/
        if (buff[i] == head[i] || buff[i] == stop_sign[i]) {/*start if*/
            i++;

            if (i < 5) {
                ret = read_pack_head_1_byte(fd, buff + i);
            }
        } else {
            if (i == 0) {
                retry_count++;
                ret = read_pack_head_1_byte(fd, buff + 0);
            } else {
                retry_count++;
                buff[0] = buff[i];
                i = 0;
            }
        }/*end if*/

        /*timeout*/
        if (retry_count > MAX_RETRY_COUNT) {
            if(g_running) {
                log_print(log_fd,"%s check PACKET HEAD timeout\n", port_list[list_id]);
                _uart_array[list_id].target_send_pack_num++;/*timeout so estimate the target_send_pack_num+1*/
                _rate[list_id] = (float)(_uart_array[list_id].target_send_pack_num - _uart_array[list_id].recv_pack_count) / _uart_array[list_id].target_send_pack_num; test_mod_sim.pass = 0;
            }
            return bytes;
        }
    }

    if (buff[4] == stop_sign[4]) {
        log_print(log_fd,"%s received stop signal, sim test will be stop\n", port_list[list_id]);
        return -1;/* means will be stop test*/
    }

    /*if head is not stop_sign then*/
    _uart_array[list_id].recv_pack_count++;/* Packet Reception count +1*/
    bytes += 5;
    len -= 5;

    while (len > 0) {
        ret = read(fd, buff + bytes, len);
        if (ret < 0) {
            DBG_PRINT("Read error\n");
            test_mod_sim.pass = 0;
            break;
        } else if (ret == 0) {
            if (++retry_count > MAX_RETRY_COUNT) {
                if(g_running) {
                    _uart_array[list_id].target_send_pack_num++;/*timeout so estimate the target_send_pack_num+1*/
                    _rate[list_id] = (float)(_uart_array[list_id].target_send_pack_num - _uart_array[list_id].recv_pack_count) / _uart_array[list_id].target_send_pack_num;
                    DBG_PRINT("received timeout\n");
                    test_mod_sim.pass = 0;
                }
            break;
            } else {
                sleep(1);
                DBG_PRINT("Retry:%d\n", retry_count);
            }
        } else {
            bytes += ret;
            len = len - ret;
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
 *      list_id:array id number
 * Return:
 *      0: packet ok
 *      -1: packet error
 *
 */
int analysis_packet(uint8_t *buff, int list_id)
{
    uint32_t crc_check = 0xFFFFFFFF;
    int log_fd;

    struct uart_package *recv_packet;
    recv_packet = (struct uart_package *)buff;

    log_fd = test_mod_sim.log_fd;
    /*
     * check crc and printf which data is error
     */
    crc_check = crc32(0, recv_packet->pack_head, 5);
    crc_check = crc32(crc_check, &recv_packet->uart_id, 1);
    crc_check = crc32(crc_check, recv_packet->pack_data, 250);
    crc_check = crc32(crc_check, &recv_packet->pack_tail, 1);
    crc_check = crc32(crc_check, (uint8_t *)&recv_packet->pack_num, 4);
    if ((uint32_t)crc_check != (uint32_t)recv_packet->crc_err) {

        if (g_running) {
            /*means received error packet*/
            log_print(log_fd, "%s Received packet error\n", port_list[list_id]);
            _uart_array[list_id].err_count++;
            test_mod_sim.pass = 0;
        }

        /*
         * check HEAD
         */

        /*
         * check data
         */

        /*
         * check uart_id
         */
        //to do....

        /*
         * check packet number
         */
        //to do....
        return -1;
    } else {
        _uart_array[list_id].target_send_pack_num = recv_packet->pack_num;
        _rate[list_id] = (float)(_uart_array[list_id].target_send_pack_num - _uart_array[list_id].recv_pack_count) / _uart_array[list_id].target_send_pack_num;
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
void *port_recv_event(void *args)
{
    struct uart_attr *uart_param;
    uint8_t buff[BUFF_SIZE];
    int fd;
    int log_fd;

    int list_id;

    int n;
    int status;

    uart_param = (struct uart_attr *)args;

    fd = uart_param->uart_fd;
    list_id = uart_param->list_id;

    log_fd = test_mod_sim.log_fd;

    while (g_running) {
        n = recv_uart_packet(fd, buff, BUFF_SIZE, list_id);
        if (n != BUFF_SIZE) {
            if (n == -1) { /*received stop signal*/
                g_running = 0;
                pthread_exit((void *)0);
            }
            DBG_PRINT("recv_uart_packet data error\n");
        }
    
        status = analysis_packet(buff,list_id);
        if (status != 0) {
            if (g_running) {
                test_mod_sim.pass = 0;
            }
            //to do 
            //break while(1)
            //pthread_exit((void *)-1);
        } else {
            if (_uart_array[list_id].recv_pack_count % 1000 == 0) {
                log_print(log_fd,"%s received %d packet successfully \n", port_list[list_id], (uint32_t)_uart_array[list_id].recv_pack_count);
            }
        }
    } /*end while(g_running)*/

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
void *port_send_event(void *args)
{
    struct uart_attr *uart_param;

    int fd;
    int log_fd;

    int list_id;
    int uart_id;

    int n;
    int i;

    struct uart_package * uart_pack;

    uart_param = (struct uart_attr *)args;

    log_fd = test_mod_sim.log_fd;

    fd = uart_param->uart_fd;
    list_id = uart_param->list_id;
    uart_id = uart_id_list[list_id];

    sleep(1);/* waiting received thread ready */

    _uart_array[list_id].send_pack_count = 0;

    while (g_running) {
        uart_pack = (struct uart_package *)malloc(sizeof(struct uart_package));
        if (!uart_pack) {
            DBG_PRINT("not enough memory\n");
            free(uart_pack);
            test_mod_sim.pass = 0;
            pthread_exit((void *)-1);
        }

        _uart_array[list_id].send_pack_count++;
        creat_uart_pack(uart_pack, _uart_array[list_id].send_pack_count, uart_id);

        n = send_uart_packet(fd, uart_pack, BUFF_SIZE);
        if (n != BUFF_SIZE) {
            DBG_PRINT("send data error\n");
            test_mod_sim.pass = 0;
        } else {
            if (_uart_array[list_id].send_pack_count % 1000 == 0) {
                log_print(log_fd,"%s send %d packet ok \n", port_list[list_id], (uint32_t)_uart_array[list_id].send_pack_count);
            }
        }
        free(uart_pack);
        sleep_ms(1000/(uart_param->baudrate/10/264));
    //    sleep_ms(20);

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
void *sim_test(void *args)
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

    memset(_uart_array, 0, sizeof(struct uart_count_list));/*init global _uart_array*/
    memset(_rate, 0, 16 * sizeof(float));

    port_num = 8 * g_board_num;

    /*init uart_param*/
    for (i=0; i<port_num; i++) {
        fd = tc_init(port_list[i]);
        if (fd < 0) {
            DBG_PRINT("Open %s error\n",port_list[i]);
            test_mod_sim.pass = 0;
            return NULL;
        }

        /* Set databits, stopbits, parity ... */
        if (tc_set_port(fd, 8, 1, 0) == -1) {
            tc_deinit(fd);
            test_mod_sim.pass = 0;
            return NULL;
        }

        tc_set_baudrate(fd, g_baudrate);

        /*assigned value to a struct uart_attr*/
        uart_param[i].uart_fd = fd;
        uart_param[i].baudrate = g_baudrate;
        uart_param[i].list_id = i;
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

    sleep(1);/*waiting read end, not use pthread_join, because it will be blocking and not exits successfully */

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
void sim_print_result(int fd)
{
    if (test_mod_sim.pass) {
        write_file(fd, "SIM: PASS\n");
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
void sim_print_status(void)
{
    int i;
    int port_num = 8 * g_board_num;

    printf("%-*s %s\n",
        COL_FIX_WIDTH, "SIM", (test_mod_sim.pass) ? STR_MOD_OK : STR_MOD_ERROR);
    for (i=0; i<port_num; i++) {
        printf("%-*s SENT:%-*u RECV:%-*u ERR:%-*u\n",
            COL_FIX_WIDTH, port_list[i],
            COL_FIX_WIDTH-5, _uart_array[i].send_pack_count,
            COL_FIX_WIDTH-5, _uart_array[i].recv_pack_count,
            COL_FIX_WIDTH-4, _uart_array[i].err_count);
    }

    //for (i=0; i<port_num; i++) {
    //    printf("%s test result:\n", port_list[i]);
    //    printf("                \\-- Packet loss rate = %.2f\n", _rate[i]);
    //    printf("                \\-- Error packet = %d\n",(uint32_t)_uart_array[i].err_count);
    //    printf("                \\-- sent %d packet\n", (uint32_t)_uart_array[i].send_pack_count);
    //    printf("                \\-- Recived %d packet\n",(uint32_t)_uart_array[i].recv_pack_count);
    //}
}
