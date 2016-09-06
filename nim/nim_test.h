/******************************************************************************
 *
 * FILENAME:
 *     nim_test.h
 *
 * DESCRIPTION:
 *     Header file for test module: NIM
 *
 * REVISION(MM/DD/YYYY):
 *     07/29/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _NIM_TEST_H_
#define _NIM_TEST_H_

#include <stdint.h>

#include "../common.h"

extern test_mod_t test_mod_nim;

typedef struct _ether_port_para {
    int sockfd;
    char * ip;
    uint16_t port;
    uint32_t ethid;
} ether_port_para;

#define TESC_NUM 4

#define TESC0_LOCAL_IP "192.100.1.2"
#define TESC1_LOCAL_IP "192.100.2.2"
#define TESC2_LOCAL_IP "192.100.3.2"
#define TESC3_LOCAL_IP "192.100.4.2"

#define TESC0_TARGET_IP "192.100.1.3"
#define TESC1_TARGET_IP "192.100.2.3"
#define TESC2_TARGET_IP "192.100.3.3"
#define TESC3_TARGET_IP "192.100.4.3"

#define TESC0_PORT  0x7000
#define TESC1_PORT  0x8000
#define TESC2_PORT  0x9000
#define TESC3_PORT  0xa000
#define NET_MAX_NUM 2048

void udp_test_start(uint32_t ethid, uint16_t portid);
int udp_test_init(uint32_t ethid, uint16_t portid);
int socket_init(int *sockfd, char *ipaddr, uint16_t portid);

void udp_send_test(ether_port_para *net_port_para);
void udp_recv_test(ether_port_para *net_port_para);

int32_t udp_send(int sockfd, char *target_ip, uint16_t port, uint8_t *buff, int32_t length, int32_t ethid);
int32_t udp_recv(int sockfd, uint16_t portid, uint8_t *buff, int32_t length, int32_t ethid);

int is_udp_write_ready(int *sockfd);
int is_udp_read_ready(int *sockfd);

#endif /* _NIM_TEST_H_ */
