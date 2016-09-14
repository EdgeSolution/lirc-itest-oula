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
	uint32_t recived_pack_count;//global variable, count recived packet
	uint32_t send_pack_count;//global variable, count send packet
	uint32_t target_send_pack_num;//Record target amount of packets sent
}__attribute__ ((packed));

static struct uart_count_list _uart_array[16] = { {0} };//init uart_count

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
    int retry_count = 0;
    do {
        ret = read(fd, buff, 1);
        if (ret < 0) {
            printf("Read error\n");
            break;
        } else if (ret == 0) {
            if (++retry_count > MAX_RETRY_COUNT) {
                printf("received timeout\n");
                break;
            }

            sleep(1);/* do not read anything then stop 1 second and read again*/
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

    /*matching head*/
    ret = read_pack_head_1_byte(fd, buff + 0);
    while (i < 5) {
        /*check head[i]*/
        if (buff[i] == head[i]) {/*start if*/
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
            printf("check PACKET HEAD timeout\n");
            return bytes;
        }
    }

    _uart_array[list_id].recived_pack_count++;/* Packet Reception count +1*/
    bytes += 5;
    len -= 5;

    while (len > 0) {
        ret = read(fd, buff + bytes, len);
        if (ret < 0) {
            DBG_PRINT("Read error\n");
            break;
        } else if (ret == 0) {
            if (++retry_count > MAX_RETRY_COUNT) {
            DBG_PRINT("received timeout\n");
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
    uint8_t i = 0;
    uint32_t crc_check = 0xFFFFFFFF;

    struct uart_package *recv_packet;
    recv_packet = (struct uart_package *)buff;

    /*
     * check crc and printf which data is error
     */
    crc_check = crc32(0, recv_packet->pack_head, 5);
    crc_check = crc32(crc_check, &recv_packet->uart_id, 1);
    crc_check = crc32(crc_check, recv_packet->pack_data, 250);
    crc_check = crc32(crc_check, &recv_packet->pack_tail, 1);
    crc_check = crc32(crc_check, (uint8_t *)&recv_packet->pack_num, 4);
    if ((uint32_t)crc_check != (uint32_t)recv_packet->crc_err) {
        /*means received error packet*/
        DBG_PRINT("Received packet error\n");
        _uart_array[list_id].err_count++;

        /*
         * check HEAD
         */

        /*
         * check data
         */
        for (i=0; i<=0xF9; i++) {
            if ((uint8_t)recv_packet->pack_data[i] != i) {
                DBG_PRINT("Received the %d data error .The true data = %x, received data = %x\n",
                                    (uint8_t)i, (uint8_t)i, (uint8_t)recv_packet->pack_data[i]);
            }
        }

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
    }
    return 0;

}


/*
 * Name:
 *      port_recived_event
 * Description:
 *      The thread routine to read data from serial port and verify them
 * PARAMETERS:
 *      uart_param: Argument of thread routine
 * Return:
 *      Exit code of thread
 *
 */
void *port_recived_event(void *args)
{
    struct uart_attr *uart_param;
    uint8_t buff[BUFF_SIZE];
    int fd;

    int list_id;

    int n;
    int status;

    uart_param = (struct uart_attr *)args;

    fd = uart_param->uart_fd;
    list_id = uart_param->list_id;

    while (g_running) {
        n = recv_uart_packet(fd, buff, BUFF_SIZE, list_id);
        if (n != BUFF_SIZE) {
            DBG_PRINT("recv_uart_packet data error\n");
        }
    
        status = analysis_packet(buff,list_id);
        if (status != 0) {
            test_mod_sim.pass = 0;
            //to do 
            //break while(1)
            //pthread_exit((void *)-1);
        }
    }

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

    int list_id;
    int uart_id;

    int n;

    struct uart_package * uart_pack;

    uart_param = (struct uart_attr *)args;

    fd = uart_param->uart_fd;
    list_id = uart_param->list_id;
    uart_id = uart_id_list[list_id];

    _uart_array[list_id].send_pack_count = 0;

    while (g_running) {
        uart_pack = (struct uart_package *)malloc(sizeof(struct uart_package));
        if (!uart_pack) {
            DBG_PRINT("Not Enough Memory\n");
            free(uart_pack);
            test_mod_sim.pass = 0;
            pthread_exit((void *)-1);
        }

        _uart_array[list_id].send_pack_count++;
        creat_uart_pack(uart_pack, _uart_array[list_id].send_pack_count++, uart_id);

        n = send_uart_packet(fd, uart_pack, BUFF_SIZE);
        if (n != BUFF_SIZE) {
            DBG_PRINT("send data error\n");
            test_mod_sim.pass = 0;
        }
        free(uart_pack);
        sleep_ms(1000/(uart_param->baudrate/10/264));

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
        pthread_create(&th_recv_id[i], NULL, port_recived_event, &uart_param[i]);
    }

    for (i=0; i<port_num; i++) {
        pthread_create(&th_send_id[i], NULL, port_send_event, &uart_param[i]);
    }

    for (i=0; i<port_num; i++) {
        pthread_join(th_recv_id[i], (void *)&th_recv_stat[i]);
        pthread_join(th_send_id[i], (void *)&th_send_stat[i]);
    }

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

    int i;
    int port_num = 8 * g_board_num;
    for (i=0; i<port_num; i++) {
        if ((_uart_array[i].err_count) > 0 || (_uart_array[i].recived_pack_count != _uart_array[i].target_send_pack_num)) {
            test_mod_sim.pass = 0;
        }
    }

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
    float rate;

    for (i=0; i<port_num; i++) {
        rate = (float)(_uart_array[i].target_send_pack_num - _uart_array[i].recived_pack_count) / _uart_array[i].recived_pack_count;

        printf("%s test result:\n", port_list[i]);
        printf("                \\-- Packet loss rate = %.2f\n", rate);
        printf("                \\-- Error packet = %d\n",_uart_array[i].err_count);
        printf("                \\-- Send %d packet\n", (uint32_t)_uart_array[i].send_pack_count);
        printf("                \\-- target send packet = %d\n",(uint32_t)_uart_array[i].target_send_pack_num);
    }
}