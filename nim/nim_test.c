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
//#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "nim_test.h"

/* Global Variables */
int net_sockid[TESC_NUM];
char target_ip[TESC_NUM][20];

uint32_t udp_cnt_send[TESC_NUM] = {0};
uint32_t udp_cnt_recv[TESC_NUM] = {0};
uint32_t udp_fst_cnt[TESC_NUM] = {0};
int udp_test_flag[TESC_NUM] = {0};

uint32_t tesc_test_no[TESC_NUM];
uint32_t tesc_err_no[TESC_NUM];
uint32_t tesc_lost_no[TESC_NUM];

int32_t udp_send_task_id[TESC_NUM];
int32_t udp_recv_task_id[TESC_NUM];

/* Local Variables */

/* Extern Variables */

/* Function Defination */
void nim_print_status();
void nim_print_result(int fd);
void *nim_test(void *args);

#if 0
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

    while (g_running) {
        sleep(1);
    }

    log_print(log_fd, "Test end\n\n");
    pthread_exit(NULL);
}
#endif

void udp_test_start(uint32_t ethid, uint16_t portid)
{
    int8_t task_send[10];
    int8_t task_recv[10];
   
    int test;

    pthread_t ptid_r;
    pthread_t ptid_s;
 
    ether_port_para net_port_para_send;
    ether_port_para net_port_para_recv;

    memset(target_ip[ethid], 0, 20);
    sprintf(task_send, "net send %d", ethid);
    sprintf(task_recv, "net recv %d", ethid);

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
            break;  //bug??
    }

    net_port_para_recv.sockfd = net_sockid[ethid];
    net_port_para_recv.port = portid;
    net_port_para_recv.ethid = ethid;

    net_port_para_send.sockfd = net_sockid[ethid];
    net_port_para_send.port = portid;
    net_port_para_send.ethid = ethid; 
    net_port_para_send.ip = target_ip[ethid];

    printf("now, create thread receive\n");
    udp_recv_task_id[ethid] = pthread_create(&ptid_r, NULL, (void *)udp_recv_test, &net_port_para_recv);
    if(udp_recv_task_id[ethid] != 0) {
        printf("task %s spawn failed!\n", task_recv);
    }

    printf("now, create thread send\n");
    udp_send_task_id[ethid] = pthread_create(&ptid_s, NULL, (void *)udp_send_test, &net_port_para_send);
    if(udp_send_task_id[ethid] != 0) {
        printf("task %s spawn failed!\n", task_send);
    }

    /* Wait udp send packet thread and udp receive packet thread to endup */
    pthread_join(ptid_r, NULL);
    pthread_join(ptid_s, NULL);
    
    printf("ENDUP~\n");
} 

int udp_test_init(uint32_t ethid, uint16_t portid)
{
    int8_t local_ip[20];

    memset(local_ip, 0, 20);
    switch(ethid) {
        case 0:
            strcpy(local_ip, TESC0_TARGET_IP);
            break;
        case 1:
            strcpy(local_ip, TESC1_TARGET_IP);
            break;
        case 2:
            strcpy(local_ip, TESC2_TARGET_IP);
            break;
        case 3:
            strcpy(local_ip, TESC3_TARGET_IP);
            break;
    }

    //tesc_init(ethid, local_ip);

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
    printf("socket id = %d\n", sockfd);
    
    memset(&hostaddr, 0, sizeof(struct sockaddr_in));

    printf("ip address = %s, port number = %d\n", ipaddr, portid);
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = htons(portid);
    hostaddr.sin_addr.s_addr = htonl(*ipaddr);
    //hostaddr.sin_addr.s_addr = inet_addr(ipaddr); // error: Cannot assign requested address
    printf("hostaddr.sin_addr.s_addr = %#x\n", hostaddr.sin_addr.s_addr);

    if(bind(*sockfd, (struct sockaddr *)(&hostaddr), sizeof(struct sockaddr)) == -1) {
        printf("ip bind error: %s errno=%d, %s\n", ipaddr, errno, strerror(errno));
        return -1;
    }
    
    //set no-blocking mode ??
    
    printf("socket initail done!\n");
 
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

    pid_t pid;
    pthread_t tid;    

    sockfd = net_port_para->sockfd;
    ethid = net_port_para->ethid;
    portid = net_port_para->port;

    strcpy(tgt_ip, net_port_para->ip);

    //??
    for(i = 4; i < NET_MAX_NUM - 4; i++) {
        send_buf[i] = i;
    }
    
    while(1) {
        pid = getpid();
        tid = pthread_self();
        printf("send thread: pid %u tid %u (0x%x)\n", (unsigned int)pid, 
            (unsigned int)tid, (unsigned int)tid);

        send_buf[0] = udp_cnt_send[ethid] & 0xff;
        send_buf[1] = udp_cnt_send[ethid] >> 8 & 0xff;
        send_buf[2] = udp_cnt_send[ethid] >> 16 & 0xff;
        send_buf[3] = udp_cnt_send[ethid] >> 24 & 0xff;

        //caluate crc
        //crc_code_get(send_buf, NET_MAX_NUM - 4, &crcget);
        //
        //

        send_num = udp_send(sockfd, (char *)tgt_ip, portid, send_buf, NET_MAX_NUM, ethid);
        if(send_num != NET_MAX_NUM) {
            printf("udp send failed!\n");
        } else {
            udp_cnt_send[ethid]++;   
        }
        
        /* sleep for perf ?? */
        usleep(1000);
    }
}

void udp_recv_test(ether_port_para *net_port_para)
{
    int sockfd;
    uint32_t ethid;
    int8_t *ip;
    uint16_t portid;
    int i = 0, recv_num;
    uint8_t recv_buf[NET_MAX_NUM];

    pid_t pid;
    pthread_t tid;

    sockfd = net_port_para->sockfd;
    ethid = net_port_para->ethid;
    portid = net_port_para->port;
    
    memset(recv_buf, 0, NET_MAX_NUM);

    while(1) {
        pid = getpid();
        tid = pthread_self();
        printf("receive thread: pid %u tid %u (0x%x)\n", (unsigned int)pid,
            (unsigned int)tid, (unsigned int)tid);

        recv_num = udp_recv(sockfd, portid, recv_buf, NET_MAX_NUM, ethid);
    
        if(recv_num == NET_MAX_NUM) {
            //calulate crc
        } else if ((recv_num > 0) && (recv_num < NET_MAX_NUM)) {
            printf("Ethernet Port %d: receive packet of %d bytes, lost %d bytes!\n", ethid, recv_num, NET_MAX_NUM - recv_num); 
        } else {
            printf("Ethernet Port %d: receive error = %d\n", ethid, recv_num);
        }
    }
    
    /* Task delay */
    //task_delay(xxx);
}

int32_t udp_send(int sockfd, char *target_ip, uint16_t port, uint8_t *buff, int32_t length, int32_t ethid)
{
    struct sockaddr_in targetaddr;
    int32_t send_num = 0;
    
    if((buff == NULL) || (target_ip == NULL)) {
        printf("udp_send error: NULL pointer!\n");
    }
    
    printf("target ip = %s\n", target_ip);

    memset(&targetaddr, 0, sizeof(struct sockaddr_in));
    
    targetaddr.sin_family = AF_INET;
    targetaddr.sin_port = htons(port);
    targetaddr.sin_addr.s_addr = inet_addr(target_ip);
    
    printf("targetaddr.sin_addr.s_addr = %#x\n", targetaddr.sin_addr.s_addr);

    if(is_udp_write_ready((int *)&sockfd) == 0) {
        send_num = sendto(sockfd, buff, length, 0, (struct sockaddr *)(&targetaddr), sizeof(struct sockaddr_in));
        if(send_num == -1) {
            printf("sendto: %d send to %s failed!\n", ethid, target_ip);
            return -1;
        } else {
            printf("udp_send failed: now %d busy!\n", ethid);
            return -1;
        }

        return send_num;
    }
}

int32_t udp_recv(int sockfd, uint16_t portid, uint8_t *buff, int32_t length, int32_t ethid) 
{
    struct sockaddr_in recv_from_addr;
    int32_t recv_length = 0, recv_num = 0;

    memset(&recv_from_addr, 0, sizeof(struct sockaddr_in));
    recv_length = sizeof(struct sockaddr_in);

    if(is_udp_read_ready((int *)&sockfd) == 0) {
        recv_num = recvfrom(sockfd, buff, length, 0, (struct sockaddr *)&recv_from_addr, &recv_length);
        
        if(recv_num == -1) {
            printf("udp_recv error: %d receive from %s!\n", ethid, inet_ntoa(recv_from_addr.sin_addr.s_addr)    );
        } 
    } else {
        printf("is_udp_read_ready not ready!\n");
    }    
}

int is_udp_write_ready(int *sockfd)
{
    fd_set rfds;
    struct timeval tv;
    int retval, ret;
    
    FD_ZERO(&rfds);
    FD_SET(*sockfd, &rfds);

    /* Wait up to 200000 microseconds */
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    retval = select(*sockfd + 1, &rfds, NULL, NULL, NULL/* or no-blocking: &tv */);
    if(retval == -1) {
        perror("select()");
        ret = -1;
    }else if(FD_ISSET(*sockfd, &rfds)) {
        printf("Write select ok now.\n");
        ret = 0;
    }else {
        printf("Write select timeout.\n");
        ret = -1;
    }
     
    return ret;
}

int is_udp_read_ready(int *sockfd)
{
    fd_set rfds;
    struct timeval tv;
    int retval, ret;

    FD_ZERO(&rfds);
    FD_SET(*sockfd, &rfds);

    /* Wait up to 200000 microseconds */
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    retval = select(*sockfd + 1, &rfds, NULL, NULL, NULL/* or no-blocking: &tv */);
    if(retval == -1) {
        perror("select()");
        ret = -1;
    }else if(FD_ISSET(*sockfd, &rfds)) {
        printf("Data is available now.\n");
        ret = 0;
    }else {
        printf("No data before timeout.\n");
        ret = -1;
    }

    return ret;
}


int main(int argc, char *argv[])
{   
    int ret;

    ret = udp_test_init(1, 0x9001);
    
    udp_test_start(1, 0x9001);
    printf("hello world\n");
    
    return 0;
}
