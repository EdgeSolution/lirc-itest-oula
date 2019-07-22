/******************************************************************************
*
* FILENAME:
*     nim_test.c
*
* DESCRIPTION:
*     Define functions for NIM test.
*
* REVISION(MM/DD/YYYY):
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
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <zlib.h>

#include "nim_test.h"

#define LOG_INTERVAL_TIME  10000

typedef struct _ether_port_para {
    int sockfd;
    char * ip;
    uint16_t port;
    uint32_t ethid;
} __attribute__((packed)) ether_port_para;

/* CCM IP */
struct ip_addrs {
    char *ip1;
    char *ip2;
};

struct ip_addrs ccm_ip_lists[] = {
    {"192.100.1.2", "192.100.1.3"},
    {"192.100.2.2", "192.100.2.3"},
    {"192.100.3.2", "192.100.3.3"},
    {"192.100.4.2", "192.100.3.3"},
};

struct ip_addrs cim_ip_lists[] = {
    {"192.101.1.2", "192.101.1.3"},
    {"192.101.2.2", "192.101.2.3"},
};

#define NETMASK "255.255.255.0"

#define UDP_PORT  9527

/* package size */
#define NET_MAX_NUM 1024

#define MAX_RETRY 5
#define FRAME_LOSS_RATE 100000

/* Global Variables */
static int net_sockid[MAX_NIC_COUNT];

static uint32_t udp_cnt_send[MAX_NIC_COUNT] = {0};
static uint32_t udp_cnt_recv[MAX_NIC_COUNT] = {0};
static uint32_t timeout_rst_cnt[MAX_NIC_COUNT] = {0};

static uint32_t tesc_err_no[MAX_NIC_COUNT] = {0};
static uint32_t tesc_lost_no[MAX_NIC_COUNT] = {0};

static int32_t udp_send_task_id[MAX_NIC_COUNT];
static int32_t udp_recv_task_id[MAX_NIC_COUNT];

static ether_port_para net_port_para_recv[MAX_NIC_COUNT];
static ether_port_para net_port_para_send[MAX_NIC_COUNT];

static int log_fd;

/* Function Defination */
static void ether_port_init(uint32_t ethid, uint16_t portid);
static int udp_test_init(uint32_t ethid, uint16_t portid);
static void udp_send_test(ether_port_para *net_port_para);
static void udp_recv_test(ether_port_para *net_port_para);
static int32_t udp_send(int sockfd, char *target_ip, uint16_t port, uint8_t *buff, int32_t length, int32_t ethid);
static int32_t udp_recv(int sockfd, uint16_t portid, uint8_t *buff, int32_t length, int32_t ethid);
static int is_udp_write_ready(int sockfd);
static int is_udp_read_ready(int sockfd);

static void nim_print_status();
static void nim_print_result(int fd);
static void nim_check_pass(void);
static void *nim_test(void *args);

test_mod_t test_mod_nim = {
    .run = 1,
    .pass = 1,
    .name = "nim",
    .log_fd = -1,
    .test_routine = nim_test,
    .print_status = nim_print_status,
    .print_result = nim_print_result
};

static void nim_print_status()
{
    uint8_t i = 0;

    nim_check_pass();

    printf("%-*s %s\n",
        COL_FIX_WIDTH, "ETH",
        (test_mod_nim.pass) ? STR_MOD_OK : STR_MOD_ERROR);

    for (i = 0; i < MAX_NIC_COUNT; i++) {
        if (!g_nim_test_eth[i]) {
            continue;
        }

        if (timeout_rst_cnt[i] >= MAX_RETRY) {
            printf("eth%-*u SENT(PKT):%-*u TIMEOUT(%us)\n",
            COL_FIX_WIDTH-3, i, COL_FIX_WIDTH-10, udp_cnt_send[i],
            timeout_rst_cnt[i] * 1);
        } else {
            printf("eth%-*u SENT(PKT):%-*u LOST(PKT):%-*u ERR(PKT):%-*u\n",
            COL_FIX_WIDTH-3, i, COL_FIX_WIDTH-10, udp_cnt_send[i],
            COL_FIX_WIDTH-10, tesc_lost_no[i], COL_FIX_WIDTH-9, tesc_err_no[i]);
        }
    }
}

static void nim_print_result(int fd)
{
    nim_check_pass();

    write_file(fd, "%s: %s\n", "ETH",
            test_mod_nim.pass?"PASS":"FAIL");
}

static void nim_check_pass(void)
{
    int i = 0;
    uint8_t flag = 1;

    for (i = 0; i < MAX_NIC_COUNT; i++) {
        if (!g_nim_test_eth[i]) {
            continue;
        }

        /* Check retry count */
        if (timeout_rst_cnt[i] >= MAX_RETRY) {
            flag = 0;
            break;
        }

        /* Check packetloss rate */
        if ((float)tesc_lost_no[i] > (float)udp_cnt_recv[i] / FRAME_LOSS_RATE) {
            flag = 0;
            break;
        }
    }

    test_mod_nim.pass = flag;
}

static void *nim_test(void *args)
{
    int i = 0;
    log_fd = test_mod_nim.log_fd;

    pthread_t ptid_r[MAX_NIC_COUNT];
    pthread_t ptid_s[MAX_NIC_COUNT];

    print_version(log_fd, "NIM");
    log_print(log_fd, "Begin test!\n\n");

    /* Initial global variable for statistics */
    memset(udp_cnt_send, 0, MAX_NIC_COUNT * sizeof(uint32_t));
    memset(udp_cnt_recv, 0, MAX_NIC_COUNT * sizeof(uint32_t));
    memset(timeout_rst_cnt, 0, MAX_NIC_COUNT * sizeof(uint32_t));
    memset(tesc_err_no, 0, MAX_NIC_COUNT * sizeof(uint32_t));
    memset(tesc_lost_no, 0, MAX_NIC_COUNT * sizeof(uint32_t));

    /* test init & ethernet port init*/
    for (i = 0; i < MAX_NIC_COUNT; i++) {
        if (g_nim_test_eth[i] == 0) {
            continue;
        }

        int ret;
        ret = udp_test_init(i, UDP_PORT);
        if (ret == 0) {
            ether_port_init(i, UDP_PORT);
        } else {
            log_print(log_fd, "NIC%d init error!\n", i);
            test_mod_nim.pass = 0;
        }
    }

    //NOTE: if port initial fail, exit the test
    if (test_mod_nim.pass == 0) {
        log_print(log_fd, "Port initial failed, exit\n");
        g_running = 0;
        goto exit;
    }

    for (i = 0; i < MAX_NIC_COUNT; i++) {
        if (g_nim_test_eth[i] == 0) {
            continue;
        }

        udp_recv_task_id[i] = pthread_create(&ptid_r[i], NULL, (void *)udp_recv_test, &net_port_para_recv[i]);
        if (udp_recv_task_id[i] != 0) {
            log_print(log_fd, "Port %d recv spawn failed!\n", i);
            test_mod_nim.pass = 0;
        }

        udp_send_task_id[i] = pthread_create(&ptid_s[i], NULL, (void *)udp_send_test, &net_port_para_send[i]);
        if (udp_send_task_id[i] != 0) {
            log_print(log_fd, "Port %d send spawn failed!\n", i);
            test_mod_nim.pass = 0;
        }
    }

    /* Wait all udp send packet thread and all udp receive packet thread to endup */
    for (i = 0; i < MAX_NIC_COUNT; i++) {
        if (g_nim_test_eth[i] == 0) {
            continue;
        }

        pthread_join(ptid_r[i], NULL);
        pthread_join(ptid_s[i], NULL);
    }

    log_print(log_fd, "Test end\n\n");

exit:
    pthread_exit(NULL);
}

static void ether_port_init(uint32_t ethid, uint16_t portid)
{
    char *target_ip = NULL;

    if (g_dev_sku == SKU_CIM) {
        if (g_machine == 'A') {
            target_ip = cim_ip_lists[ethid].ip2;
        } else {    /* Machine B */
            target_ip = cim_ip_lists[ethid].ip1;
        }
    } else {
        if (g_machine == 'A') {
            target_ip = ccm_ip_lists[ethid].ip2;
        } else {    /* Machine B */
            target_ip = ccm_ip_lists[ethid].ip1;
        }
    }

    net_port_para_recv[ethid].sockfd = net_sockid[ethid];
    net_port_para_recv[ethid].port = portid;
    net_port_para_recv[ethid].ethid = ethid;

    net_port_para_send[ethid].sockfd = net_sockid[ethid];
    net_port_para_send[ethid].port = portid;
    net_port_para_send[ethid].ethid = ethid;
    net_port_para_send[ethid].ip = target_ip;
}

static int udp_test_init(uint32_t ethid, uint16_t portid)
{
    char *local_ip = NULL;

    //Initial IP address
    if (g_dev_sku == SKU_CIM) {
        if (g_machine == 'A') {
            local_ip = cim_ip_lists[ethid].ip1;
        } else {
            local_ip = cim_ip_lists[ethid].ip2;
        }
    } else {
        if (g_machine == 'A') {
            local_ip = ccm_ip_lists[ethid].ip1;
        } else {
            local_ip = ccm_ip_lists[ethid].ip2;
        }
    }

    if (set_ipaddr(ethid, local_ip, NETMASK) == -1) {
        return -1;
    }

    if (socket_init((int *)&net_sockid[ethid], local_ip, portid) != 0) {
        log_print(log_fd, "socket init failed!\n");

        return -1;
    }

    log_print(log_fd, "NIC%d test init done !\n", ethid);

    return 0;
}

static void udp_send_test(ether_port_para *net_port_para)
{
    int sockfd;
    uint32_t ethid;
    uint16_t portid;
    char *tgt_ip = NULL;

    int i, j = 0, send_num;
    uint8_t send_buf[NET_MAX_NUM];

    uint32_t calculated_crc;

    sockfd = net_port_para->sockfd;
    ethid = net_port_para->ethid;
    portid = net_port_para->port;

    tgt_ip = net_port_para->ip;

    /* Packaging byte 0 - NET_MAX_NUM - 8 */
    for (i = 0; i < NET_MAX_NUM - 8; i++) {
        send_buf[i] = i;
    }

    /* Wait receive thread to ready */
    sleep_ms(500);

    while (g_running) {
        /* Send packages count */
        send_buf[NET_MAX_NUM - 5] = (uint8_t)(udp_cnt_send[ethid] & 0xff);
        send_buf[NET_MAX_NUM - 6] = (uint8_t)(udp_cnt_send[ethid] >> 8 & 0xff);
        send_buf[NET_MAX_NUM - 7] = (uint8_t)(udp_cnt_send[ethid] >> 16 & 0xff);
        send_buf[NET_MAX_NUM - 8] = (uint8_t)(udp_cnt_send[ethid] >> 24 & 0xff);

        /* calucate CRC */
        calculated_crc = crc32(0, send_buf, NET_MAX_NUM - 4);
        send_buf[NET_MAX_NUM - 1] = (uint8_t)(calculated_crc & 0xff);
        send_buf[NET_MAX_NUM - 2] = (uint8_t)(calculated_crc >> 8 & 0xff);
        send_buf[NET_MAX_NUM - 3] = (uint8_t)(calculated_crc >> 16 & 0xff);
        send_buf[NET_MAX_NUM - 4] = (uint8_t)(calculated_crc >> 24 & 0xff);

        send_num = udp_send(sockfd, tgt_ip, portid, send_buf, NET_MAX_NUM, ethid);
        if (send_num != NET_MAX_NUM) {
            log_print(log_fd, "udp send failed!\n");
        } else {
            udp_cnt_send[ethid]++;

            j++;
            /* print log after given times */
            if (j >= LOG_INTERVAL_TIME) {
                log_print(log_fd, "NIC%d: send udp packets count %u.\n", ethid, udp_cnt_send[ethid]);
                j = 0;
            }
        }

        sleep_ms(1);
    }

    /* sync for stopping*/
    if (g_running == 0) {
        for (i = 0; i < 4; i++) {
            send_buf[i] = 0x55;
        }

        send_num = udp_send(sockfd, tgt_ip, portid, send_buf, NET_MAX_NUM, ethid);
        if (send_num != NET_MAX_NUM) {
            log_print(log_fd, "udp send failed!\n");
        }

        log_print(log_fd, "send sync packet for stopping!\n");
    }
}

static void udp_recv_test(ether_port_para *net_port_para)
{
    int sockfd;
    uint32_t ethid;
    uint16_t portid;
    int recv_num;
    uint8_t recv_buf[NET_MAX_NUM];

    uint32_t stored_crc;
    uint32_t calculated_crc;
    uint32_t udp_cnt_read;

    int i = 0, j = 0;

    sockfd = net_port_para->sockfd;
    ethid = net_port_para->ethid;
    portid = net_port_para->port;

    memset(recv_buf, 0, NET_MAX_NUM);

    while (g_running) {
        recv_num = udp_recv(sockfd, portid, recv_buf, NET_MAX_NUM, ethid);

        if (recv_num == NET_MAX_NUM) {
            /* sync for stopping */
            if (((recv_buf[i] & 0xaa) || (recv_buf[i + 1] & 0xaa) || (recv_buf[i + 2] & 0xaa) || (recv_buf[i + 3] & 0xaa)) == 0) {
                g_running = 0;
                break;
            }

            /* reset flag for timeout */
            timeout_rst_cnt[ethid] = 0;

            calculated_crc = crc32(0, recv_buf, NET_MAX_NUM - 4);

            stored_crc = (uint32_t)((recv_buf[NET_MAX_NUM - 1]) | (recv_buf[NET_MAX_NUM - 2] << 8)  \
                 | (recv_buf[NET_MAX_NUM -3] << 16) | (recv_buf[NET_MAX_NUM - 4] << 24));

            if (calculated_crc != stored_crc) {
                tesc_err_no[ethid]++;
                log_print(log_fd, "NIC%d: CRC error, number %u.\n", ethid, tesc_err_no[ethid]);
            } else {  /* crc is good */
                udp_cnt_read = (uint32_t)((recv_buf[NET_MAX_NUM - 5]) | (recv_buf[NET_MAX_NUM - 6] << 8)    \
                    | (recv_buf[NET_MAX_NUM - 7] << 16) | (recv_buf[NET_MAX_NUM - 8] << 24));

                /* Calulate the number of lost packages, care it */
                if (udp_cnt_read >= udp_cnt_recv[ethid]) {
                    tesc_lost_no[ethid] += (udp_cnt_read - udp_cnt_recv[ethid]);
                    udp_cnt_recv[ethid] = udp_cnt_read;
                } else if (udp_cnt_read < udp_cnt_recv[ethid]) {
                    /* Maybe the package is late in sequence, here skip it */
                    //log_print(log_fd, "NIC%d: receive notice, maybe has received packages from other machine, or the package maybe late\n", ethid);
                }
            }
            udp_cnt_recv[ethid]++;

            j++;

            /* print log after given times */
            if (j >= LOG_INTERVAL_TIME) {
                log_print(log_fd, "NIC%d: recv udp count = %u, lost no = %u, err no = %u\n", \
                    ethid, udp_cnt_recv[ethid], tesc_lost_no[ethid], tesc_err_no[ethid]);
                j = 0;
             }
        } else if ((recv_num > 0) && (recv_num < NET_MAX_NUM)) {
            log_print(log_fd, "NIC%d: receive packet of %d bytes, lost %d bytes!\n", \
                    ethid, recv_num, NET_MAX_NUM - recv_num);
        } else {
            log_print(log_fd, "NIC%d: receive timeout [no.%d], no data is incoming.\n", ethid, timeout_rst_cnt[ethid]);
        }
    }
}

static int32_t udp_send(int sockfd, char *target_ip, uint16_t port, uint8_t *buff, int32_t length, int32_t ethid)
{
    struct sockaddr_in targetaddr;
    int32_t send_num = 0;

    if ((buff == NULL) || (target_ip == NULL)) {
        DBG_PRINT("udp_send error: NULL pointer!\n");
    }

    memset(&targetaddr, 0, sizeof(struct sockaddr_in));

    targetaddr.sin_family = AF_INET;
    targetaddr.sin_port = htons(port);
    targetaddr.sin_addr.s_addr = inet_addr(target_ip);

    if (is_udp_write_ready(sockfd) == 0) {
        send_num = sendto(sockfd, buff, length, 0, (struct sockaddr *)(&targetaddr), sizeof(struct sockaddr_in));
        if (send_num == -1) {
            log_print(log_fd, "sendto: NIC%d send to %s failed!\n", ethid, target_ip);

            return -1;
        }
    } else {
        log_print(log_fd, "udp_send failed: now NIC%d busy!\n", ethid);

        return -1;
    }

    return send_num;
}

static int32_t udp_recv(int sockfd, uint16_t portid, uint8_t *buff, int32_t length, int32_t ethid)
{
    struct sockaddr_in recv_from_addr;
    int32_t recv_num = 0;
    socklen_t addrlen;
    int tv;

    memset(&recv_from_addr, 0, sizeof(struct sockaddr_in));
    addrlen = sizeof(struct sockaddr_in);

    tv = is_udp_read_ready(sockfd);
    if (tv == 0) {
        recv_num = recvfrom(sockfd, buff, length, 0, (struct sockaddr *)&recv_from_addr, &addrlen);

        if (recv_num == -1) {
            log_print(log_fd, "udp_recv error: %d!\n", ethid);
        }
    } else if (tv == 1) {    /* select timeout */
        timeout_rst_cnt[ethid]++;
    }

    return recv_num;
}

static int is_udp_write_ready(int sockfd)
{
    fd_set wfds;
    int retval = 0, ret = -1;

    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);

    /* Wait up to 200000 microseconds
     * tv.tv_sec = 0;
     * tv.tv_usec = 200000; */

    retval = select(sockfd + 1, NULL, &wfds, NULL, NULL/* or no-blocking: &tv */);
    if (retval > 0) {
        if (FD_ISSET(sockfd, &wfds))
            ret = 0;
    } else {
        ret = -1;
    }

    return ret;
}

static int is_udp_read_ready(int sockfd)
{
    fd_set rfds;
    int retval = 0, ret = -1;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    /* Wait up to 1 second */
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    retval = select(sockfd + 1, &rfds, NULL, NULL, &tv /* no-blocking*/);
    if (retval > 0) {
        if (FD_ISSET(sockfd, &rfds)) {
            ret = 0;
        }
    } else if (retval == 0) { /* timeout */
        ret = 1;
    } else {
        ret = -1;
    }

    return ret;
}
