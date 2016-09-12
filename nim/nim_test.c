/******************************************************************************
*
* FILENAME:
*     nim_test.c
*
* DESCRIPTION:
*     Define functions for NIM test.
*
* REVISION(MM/DD/YYYY):
*     07/29/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     09/12/2016  Lin Du (lin.du@advantech.com.cn)
*     - Initial version 
*
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <zlib.h>

#include "nim_test.h"


/* Global Variables */
static int net_sockid[TESC_NUM];
static char target_ip[TESC_NUM][20];

static uint32_t udp_cnt_send[TESC_NUM] = {0};
static uint32_t udp_cnt_recv[TESC_NUM] = {0};
static uint32_t udp_fst_cnt[TESC_NUM] = {0};
static int udp_test_flag[TESC_NUM] = {0};

static uint32_t tesc_test_no[TESC_NUM] = {0};
static uint32_t tesc_err_no[TESC_NUM] = {0};
static uint32_t tesc_lost_no[TESC_NUM] = {0};

static int32_t udp_send_task_id[TESC_NUM];
static int32_t udp_recv_task_id[TESC_NUM];


/* Function Defination */
void nim_print_status();
void nim_print_result(int fd);
void *nim_test(void *args);

test_mod_t test_mod_nim = {
    .run = 1,
    .pass = 1,
    .name = "nim",
    .log_fd = -1,
    .test_routine = nim_test,
    .print_status = nim_print_status,
    .print_result = nim_print_result
};


void nim_print_status()
{
    int ethid = 0;
    
    for(ethid=0; ethid < 4; ) {
        printf("Port %d:udp_cnt_recv = %d, tesc_test_no =%d, test_lst_no = %d, test_err_no = %d\n", \
            ethid, udp_cnt_recv[ethid], tesc_test_no[ethid],  tesc_lost_no[ethid], tesc_err_no[ethid]);
        printf("Port %d:udp_cnt_send = %d\n", ethid, udp_cnt_send[ethid]);
        
        ethid++;
        if(ethid == 4) {
            ethid = 0;
            sleep(2);
        }
    }
}
    
void nim_print_result(int fd)
{
    if (test_mod_nim.pass) {
        write_file(fd, "NIM: PASS\n");
    } else {
        write_file(fd, "NIM: FAIL\n");
    }
}


void *nim_test(void *args)
{
    int log_fd = test_mod_nim.log_fd;

    log_print(log_fd, "Begin test!\n\n");

    /* test init */
    udp_test_init(0, TESC0_PORT);
    udp_test_init(1, TESC1_PORT);
    udp_test_init(2, TESC2_PORT);
    udp_test_init(3, TESC3_PORT);

    /* test start */
    udp_test_start(0, TESC0_PORT);
    udp_test_start(1, TESC1_PORT);
    udp_test_start(2, TESC2_PORT);
    udp_test_start(3, TESC3_PORT);
   
    return NULL; 
    //log_print(log_fd, "Test end\n\n");
    //pthread_exit(NULL);
}

void udp_test_start(uint32_t ethid, uint16_t portid)
{
    char task_send[10];
    char task_recv[10];
   
    pthread_t ptid_r;
    pthread_t ptid_s;
 
    ether_port_para net_port_para_send;
    ether_port_para net_port_para_recv;

    memset(target_ip[ethid], 0, 20);
    sprintf(task_send, "tnetsend%d", ethid);
    sprintf(task_recv, "tnetrecv%d", ethid);

    switch(ethid) {
        case 0:
            strcpy(target_ip[0], TESC0_TARGET_IP);
            break;
        case 1:
            strcpy(target_ip[1], TESC1_TARGET_IP);
            break;
        case 2:
            strcpy(target_ip[2], TESC2_TARGET_IP);
            break;
        case 3:
            strcpy(target_ip[3], TESC3_TARGET_IP);
            break;
        default:
            break;
    }

    net_port_para_recv.sockfd = net_sockid[ethid];
    net_port_para_recv.port = portid;
    net_port_para_recv.ethid = ethid;

    net_port_para_send.sockfd = net_sockid[ethid];
    net_port_para_send.port = portid;
    net_port_para_send.ethid = ethid; 
    net_port_para_send.ip = target_ip[ethid];
   
    printf("ethernet port %d, net_sockid[%d] = %d; ", ethid, ethid, net_sockid[ethid]); 
    
    udp_recv_task_id[ethid] = pthread_create(&ptid_r, NULL, (void *)udp_recv_test, &net_port_para_recv);
    if(udp_recv_task_id[ethid] != 0) {
        printf("task %s spawn failed!\n", task_recv);
    }
    
    udp_send_task_id[ethid] = pthread_create(&ptid_s, NULL, (void *)udp_send_test, &net_port_para_send);
    if(udp_send_task_id[ethid] != 0) {
        printf("task %s spawn failed!\n", task_send);
    }
    
    /* Wait udp send packet thread and udp receive packet thread to endup */
    //pthread_join(ptid_r, NULL);
    //pthread_join(ptid_s, NULL);
} 

int udp_test_init(uint32_t ethid, uint16_t portid)
{
    char local_ip[20];
    
    memset(local_ip, 0, 20);
    switch(ethid) {
        case 0:
            strcpy(local_ip, TESC0_LOCAL_IP);
            break;
        case 1:
            strcpy(local_ip, TESC1_LOCAL_IP);
            break;
        case 2:
            strcpy(local_ip, TESC2_LOCAL_IP);
            break;
        case 3:
            strcpy(local_ip, TESC3_LOCAL_IP);
            break;
    }

    if(socket_init((int *)&net_sockid[ethid], local_ip, portid) != 0) {
        printf("socket init failed!\n");
        return -1;
    }
    
    memset(tesc_test_no, 0, TESC_NUM * sizeof(uint32_t));
    memset(tesc_err_no, 0, TESC_NUM * sizeof(uint32_t));
    memset(tesc_lost_no, 0, TESC_NUM * sizeof(uint32_t));

    printf("udp test initial done!\n");

    return 0;
}

int socket_init(int *sockfd, char *ipaddr, uint16_t portid)
{
    struct sockaddr_in hostaddr;
    //uint32_t flag;
    
    *sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP /*0*/);
    if(*sockfd == -1) {
        printf("create socket failed! errno=%d, %s\n", errno, strerror(errno));
        return -1;
    }
    
    memset(&hostaddr, 0, sizeof(struct sockaddr_in));

    printf("socket id = %d, ip address = %s, port number = %d\n", *sockfd, ipaddr, portid);
   
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = htons(portid);
    hostaddr.sin_addr.s_addr = inet_addr(ipaddr);

    if(bind(*sockfd, (struct sockaddr *)(&hostaddr), sizeof(struct sockaddr)) == -1) {
        printf("ip bind error: %s errno=%d, %s\n", ipaddr, errno, strerror(errno));
        return -1;
    }
    
    return 0;
}

void udp_send_test(ether_port_para *net_port_para)
{   
    int sockfd;
    uint32_t ethid;
    uint16_t portid;
    char tgt_ip[255];

    int i, send_num;
    uint8_t send_buf[NET_MAX_NUM];    

    uint32_t calculated_crc;

    sockfd = net_port_para->sockfd;
    ethid = net_port_para->ethid;
    portid = net_port_para->port;

    strcpy(tgt_ip, net_port_para->ip);

    /* Packaging byte 0 - NET_MAX_NUM - 8 */
    for(i = 0; i < NET_MAX_NUM - 8; i++) {
        send_buf[i] = i;
    }
    
    while(1) {
        /* Send packages count */
        send_buf[NET_MAX_NUM - 5] = udp_cnt_send[ethid] & 0xff;
        send_buf[NET_MAX_NUM - 6] = udp_cnt_send[ethid] >> 8 & 0xff;
        send_buf[NET_MAX_NUM - 7] = udp_cnt_send[ethid] >> 16 & 0xff;
        send_buf[NET_MAX_NUM - 8] = udp_cnt_send[ethid] >> 24 & 0xff;

        /* calucate CRC */
        calculated_crc = crc32(0, send_buf, NET_MAX_NUM - 4);
        send_buf[NET_MAX_NUM - 1] = (uint8_t)(calculated_crc & 0xff);
        send_buf[NET_MAX_NUM - 2] = (uint8_t)(calculated_crc >> 8 & 0xff);
        send_buf[NET_MAX_NUM - 3] = (uint8_t)(calculated_crc >> 16 & 0xff);
        send_buf[NET_MAX_NUM - 4] = (uint8_t)(calculated_crc >> 24 & 0xff);
    
        send_num = udp_send(sockfd, (char *)tgt_ip, portid, send_buf, NET_MAX_NUM, ethid);
        if(send_num != NET_MAX_NUM) {
            printf("udp send failed!\n");
        } else {
            udp_cnt_send[ethid]++;   
        }

        usleep(1000);
    }
}

void udp_recv_test(ether_port_para *net_port_para)
{
    int sockfd;
    uint32_t ethid;
    uint16_t portid;
    int recv_num;
    uint8_t recv_buf[NET_MAX_NUM];

    uint32_t stored_crc;
    uint32_t calculated_crc;
    uint32_t udp_cnt_read;

    sockfd = net_port_para->sockfd;
    ethid = net_port_para->ethid;
    portid = net_port_para->port;
    
    memset(recv_buf, 0, NET_MAX_NUM);

    while(1) {
        recv_num = udp_recv(sockfd, portid, recv_buf, NET_MAX_NUM, ethid);
    
        if(recv_num == NET_MAX_NUM) {
            calculated_crc = crc32(0, recv_buf, NET_MAX_NUM - 4);
            //printf("calculated_crc = %d\n", calculated_crc);
            
            stored_crc = (uint32_t)((recv_buf[NET_MAX_NUM - 1]) | (recv_buf[NET_MAX_NUM - 2] << 8)  \
                 | (recv_buf[NET_MAX_NUM -3] << 16) | (recv_buf[NET_MAX_NUM - 4] << 24));
            //printf("stored_crc = %d\n", stored_crc);

            if(calculated_crc != stored_crc) {
                tesc_err_no[ethid]++;
            } else {  /* crc is good */
                udp_cnt_read = (uint32_t)((recv_buf[NET_MAX_NUM - 5]) | (recv_buf[NET_MAX_NUM - 6] << 8)    \
                    | (recv_buf[NET_MAX_NUM - 7] << 16) | (recv_buf[NET_MAX_NUM - 8] << 24));
                /* First package received */
                if(udp_test_flag[ethid] == 0) {
                    udp_fst_cnt[ethid] = udp_cnt_read;
                    udp_cnt_recv[ethid] = udp_cnt_read;   
                    udp_test_flag[ethid] = 1; 
                } else {
                    tesc_test_no[ethid] = udp_cnt_read - udp_fst_cnt[ethid];
                    /* Lost some packages */
                    if(udp_cnt_read > udp_cnt_recv[ethid]) {
                        tesc_lost_no[ethid] += (udp_cnt_read - udp_cnt_recv[ethid]);
                        udp_cnt_recv[ethid] = udp_cnt_read;
                    } else if (udp_cnt_read < udp_cnt_recv[ethid]) {
                        /* printf("receive packet count error, may be not an integrated packet!\n"); */
                    }
                }
            }
            udp_cnt_recv[ethid]++;
        
    //printf("xxxx......Port %d:udp_cnt_recv = %d, test_lst_no = %d, test_err_no = %d\n", ethid, udp_cnt_recv[ethid], tesc_lost_no[ethid], tesc_err_no[ethid]);
        
        } else if ((recv_num > 0) && (recv_num < NET_MAX_NUM)) {
            printf("Ethernet Port %d: receive packet of %d bytes, lost %d bytes!\n", ethid, recv_num, NET_MAX_NUM - recv_num); 
        } else {
            printf("Ethernet Port %d: receive error = %d\n", ethid, recv_num);
        }
    }
}

int32_t udp_send(int sockfd, char *target_ip, uint16_t port, uint8_t *buff, int32_t length, int32_t ethid)
{
    struct sockaddr_in targetaddr;
    int32_t send_num = 0;
    
    if((buff == NULL) || (target_ip == NULL)) {
        printf("udp_send error: NULL pointer!\n");
    }
    
    memset(&targetaddr, 0, sizeof(struct sockaddr_in));
    
    targetaddr.sin_family = AF_INET;
    targetaddr.sin_port = htons(port);
    targetaddr.sin_addr.s_addr = inet_addr(target_ip);
    
    if(is_udp_write_ready(sockfd) == 0) {
        send_num = sendto(sockfd, buff, length, 0, (struct sockaddr *)(&targetaddr), sizeof(struct sockaddr_in));
        if(send_num == -1) {
            printf("sendto: port %d send to %s failed!\n", ethid, target_ip);
            return -1;
        }
    } else {
        printf("udp_send failed: now Ethernet Port %d busy!\n", ethid);
        return -1;
    }

    //printf("socket fd = %d , target ip = %s, port = %d, ethernet port = %d, send_num =%d\n", sockfd, target_ip, port, ethid, send_num );
    return send_num;
}

int32_t udp_recv(int sockfd, uint16_t portid, uint8_t *buff, int32_t length, int32_t ethid) 
{
    struct sockaddr_in recv_from_addr;
    int32_t recv_num = 0;
    socklen_t addrlen;

    memset(&recv_from_addr, 0, sizeof(struct sockaddr_in));
    addrlen = sizeof(struct sockaddr_in);

    if(is_udp_read_ready(sockfd) == 0) {
        recv_num = recvfrom(sockfd, buff, length, 0, (struct sockaddr *)&recv_from_addr, &addrlen);
        
        if(recv_num == -1) {
            printf("udp_recv error: %d!\n", ethid);
        } 
    } else {
        printf("is_udp_read_ready not ready!\n");
    }   
    
    return recv_num; 
}

int is_udp_write_ready(int sockfd)
{
    fd_set wfds;
    int retval = 0, ret = -1;
    
    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);

    /* Wait up to 200000 microseconds
     * tv.tv_sec = 0;
     * tv.tv_usec = 200000; */

    retval = select(sockfd + 1, NULL, &wfds, NULL, NULL/* or no-blocking: &tv */);
    if(retval > 0) {
        if(FD_ISSET(sockfd, &wfds)) 
            ret = 0;
    } else {
        ret = -1;
        if(retval == 0) {
            printf("write select time out!\n");
        } else {
            printf("write select error!\n");
        }
    }
     
    return ret;
}

int is_udp_read_ready(int sockfd)
{
    fd_set rfds;
    int retval = 0, ret = -1;

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    /* Wait up to 200000 microseconds.
     * tv.tv_sec = 0;
     * tv.tv_usec = 200000; */

    retval = select(sockfd + 1, &rfds, NULL, NULL, NULL/* or no-blocking: &tv */);
    if(retval > 0) {
        if(FD_ISSET(sockfd, &rfds)) {
            ret = 0;
        }
    }else {
        ret = -1; 
        if(retval == 0)
            printf("read select timeout!\n");
        else 
            perror("read select error!\n");
    }

    return ret;
}

#if 0
int main(int argc, char *argv[])
{
    nim_test(NULL);
    
    nim_print_status();
     
    printf("hello world\n");
    
    return 0;
}
#endif 
