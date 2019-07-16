/******************************************************************************
 *
 * FILENAME:
 *     common.c
 *
 * DESCRIPTION:
 *     Define some useful functions
 *
 * REVISION(MM/DD/YYYY):
 *     08/16/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"
#include "term.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#ifdef DEBUG
#include <errno.h>
#endif

//Return code for system
#define DIAG_SYS_RC(x)    ((WTERMSIG(x) == 0)?(WEXITSTATUS(x)):-1)

#define IPSTR_LEN   16
#define UDP_PORT    9527

static int check_link_status(char *ifname);
static int set_if_up(char *ifname);

void kill_process(char *name)
{
    char kill[260];

    sprintf(kill, "pkill -2 -f %s", name);
    system(kill);
}


/******************************************************************************
 * NAME:
 *      ser_open
 *
 * DESCRIPTION:
 *      Open a serial port, and return the fd of it.
 *
 * PARAMETERS:
 *      dev -  The serial port to open
 *
 * RETURN:
 *      The fd of serial port
 ******************************************************************************/
int ser_open(char *dev)
{
    int fd;
    int baud = 115200;
    int databits = 8;
    int parity = 0;
    int stopbits = 1;

    fd = tc_init(dev);
    if (fd == -1) {
        return -1;
    }

    /* Set baudrate */
    tc_set_baudrate(fd, baud);

    /* Set databits, stopbits, parity ... */
    if (tc_set_port(fd, databits, stopbits, parity) == -1) {
        tc_deinit(fd);
        return -1;
    }

    return fd;
}


/******************************************************************************
 * NAME:
 *      send_sync_data
 *
 * DESCRIPTION:
 *      Send a DATA_SYNC char to the serial port.
 *
 * PARAMETERS:
 *      fd - The fd of serial port
 *
 * RETURN:
 *      1 - DATA_SYNC char has been sent
 *      0 - ERROR.
 ******************************************************************************/
static int send_sync_data(int fd, char snt_char)
{
    char buf[MAX_STR_LENGTH] = {snt_char, 0};
    memset(buf, snt_char, sizeof(buf) - 1);

    int size = write(fd, buf, sizeof(buf));
    if (size > 0) {
        return 1;
    } else {
        return 0;
    }
}


/******************************************************************************
 * NAME:
 *      recv_sync_data
 *
 * DESCRIPTION:
 *      Read data from serail port and search the DATA_SYNC char
 *
 * PARAMETERS:
 *      fd - The fd of serail port
 *
 * RETURN:
 *      1  - Reveived a right SYNC char
 *      -1 - Reveived a wrong SYNC char
 *      0  - Not received.
 ******************************************************************************/
static int recv_sync_data(int fd, char rcv_char, char snt_char)
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    int size;
    char buf[64];

    /* Watch fd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    /* Wait up to 3 seconds. */
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    retval = select(fd+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */
    if (retval > 0) {
        /* FD_ISSET(0, &rfds) will be true. */
        DBG_PRINT("Data is available now.\n");

        memset(buf, 0, sizeof(buf));
        size = read(fd, buf, sizeof(buf)-1);
        if (size > 0) {
            /* Search the sync char. */
            if (strchr(buf, rcv_char)) {
                return 1;
            } else if (strchr(buf, snt_char)) {
                return -1;
            }
        }
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      wait_other_side_ready
 *
 * DESCRIPTION:
 *      Wait the test program on the other side(machine) to be ready.
 *
 * PARAMETERS:
 *      fd - The fd of serail port
 *
 * RETURN:
 *      1 - Ready.
 *      0 - Not Ready(fail).
 ******************************************************************************/
int wait_other_side_ready(int fd)
{
    int rc = 0;
    char snt_char, rcv_char;

    g_syncing = 1;

    if (g_machine == 'A') {
        snt_char = DATA_SYNC_A;
        rcv_char = DATA_SYNC_B;
    } else /*if (g_machine == 'B')*/ {
        snt_char = DATA_SYNC_B;
        rcv_char = DATA_SYNC_A;
    }

    if (fd < 0 ) {
        fd = ser_open(CCM_SERIAL_PORT);
        if (fd < 0) {
            printf("Open the serial port of CCM fail!\n");
            return 0;
        }

        //NOTE: Pull-down RTS before sync, fix HSM test fail on CIM
        tc_set_rts_casco(fd, FALSE);
        sleep_ms(100);
    }

    while (g_running) {
        /* Send sync request */
        if (send_sync_data(fd, snt_char) == 0) {
            continue;
        }

        /* Wait for response in five seconds. */
        int ch = recv_sync_data(fd, rcv_char, snt_char);
        if (ch == 1) {
            send_sync_data(fd, snt_char);
            rc = 1;
            break;
        } else if (ch == -1) {
            printf("Invalid machine!\n");
            rc = 0;
            break;
        }
    }

    g_syncing = 0;

    //NOTE: RTS will be restored after close, remove the close.
    //close(fd);

    return rc;
}


/******************************************************************************
 * NAME:
 *      sleep_ms
 *
 * DESCRIPTION:
 *      Sleep some number of milli-seconds.
 *
 * PARAMETERS:
 *      ms - Time in milliseconds.
 *
 * RETURN:
 *      0 - OK, Other - interrupted or error
 ******************************************************************************/
int sleep_ms(unsigned int ms)
{
    struct timespec req;

    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    if (nanosleep(&req, NULL) < 0) {
        return -1;
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      is_exe_exist
 *
 * DESCRIPTION:
 *      Is the EXE file exist.
 *
 * PARAMETERS:
 *      exe - File name or full path of EXE file
 *
 * RETURN:
 *      1 - Exist
 *      0 - Non-exist
 ******************************************************************************/
int is_exe_exist(char *exe)
{
    char cmd[1024];
    int rc;

    if (exe == NULL) {
        return 0;
    }

    snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null", exe);
    rc = DIAG_SYS_RC(system(cmd));
    if (rc == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* Send sync data on exit */
static void *send_exit_data(void *args)
{
    char snt_char;
    int i;

    if (g_machine == 'A') {
        snt_char = EXIT_SYNC_A;
    } else {
        snt_char = EXIT_SYNC_B;
    }

    int fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        pthread_exit(NULL);
    }

    //Send exit data 3 times
    for(i=0; i < 3; i++) {
        write(fd, &snt_char, 1);
        sleep_ms(500);
    }

    close(fd);

    pthread_exit(NULL);
}

/* Receive sync data on exit */
static void *receive_exit_data(void *args)
{
    char buf[64];

    /* Open serial port */
    int fd = ser_open(CCM_SERIAL_PORT);
    if (fd < 0) {
        pthread_exit(NULL);
    }

    while (g_running) {
        if (g_syncing) {
            sleep_ms(100);
            continue;
        }

        //If has msm test and hsm switch test finished, then break
        if (g_test_msm && !g_hsm_switching) {
            break;
        }

        memset(buf, 0, sizeof(buf));
        int size = read(fd, buf, sizeof(buf)-1);
        if (size > 0) {
            if (strchr(buf, EXIT_SYNC_A) || strchr(buf, EXIT_SYNC_B)) {
                g_running = 0;
                break;
            }
        }

        sleep_ms(200);
    }

    close(fd);

    pthread_exit(NULL);
}

/******************************************************************************
 * NAME:
 *      send_exit_sync
 *
 * DESCRIPTION:
 *      Create thread to send exit sync data
 *
 ******************************************************************************/
void send_exit_sync(void)
{
    pthread_t pid;

    pthread_create(&pid, NULL, send_exit_data, NULL);
}

/******************************************************************************
 * NAME:
 *      receive_exit_sync
 *
 * DESCRIPTION:
 *      Create thread to receive exit sync data
 ******************************************************************************/
void receive_exit_sync(void)
{
    pthread_t pid;

    pthread_create(&pid, NULL, receive_exit_data, NULL);
}

/******************************************************************************
 * NAME:
 *      send_packet
 *
 * DESCRIPTION:
 *      Send serial data, used by hsm and sim module
 *
 * PARAMETERS:
 *      fd  - The fd of serial port
 *      buf - memory buffer
 *      len - buffer length
 *
 * RETURN:
 *      0   - success
 *      -1  - on error
 ******************************************************************************/
int send_packet(int fd, char *buf, uint8_t len)
{
    int rc = 0;
    int bytes_left = len;
    int bytes_sent = 0;

    if (!buf) {
        return -1;
    }

    while (bytes_left > 0) {
        int size = write(fd, &buf[bytes_sent], bytes_left);
        if (size >= 0) {
            bytes_left -= size;
            bytes_sent += size;
        } else {
            rc = -1;
            break;
        }
    }

    return rc;
}

int set_ipaddr(char *ifname, char *ipaddr, char *netmask)
{
    int sockfd = -1;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    if ((ifname == NULL) || (ipaddr == NULL)) {
        DBG_PRINT("illegal do config ip!\n");

        return -1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        DBG_PRINT("socket failed for setting ip! err: %s\n", strerror(errno));

        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, ifname);
    sin = (struct sockaddr_in *)&ifr.ifr_addr;

    sin->sin_family = AF_INET;

    /* config ip address */
    sin->sin_addr.s_addr = inet_addr(ipaddr);
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
        close(sockfd);
        DBG_PRINT("ioctl failed for set ip address! err: %s\n", strerror(errno));

        return -1;
    }

    /* config netmask */
    sin->sin_addr.s_addr = inet_addr(netmask);
    if  (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
        close(sockfd);
        DBG_PRINT("ioctl failed for set netmask! err: %s\n", strerror(errno));

        return -1;
    }

    close(sockfd);

    return 0;
}

int socket_init(int *sockfd, char *ipaddr, uint16_t portid)
{
    struct sockaddr_in hostaddr;
    int reuse;

    *sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP /*0*/);
    if (*sockfd == -1) {
        return -1;
    }

    reuse = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        return -1;
    }

    memset(&hostaddr, 0, sizeof(struct sockaddr_in));

    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = htons(portid);
    hostaddr.sin_addr.s_addr = inet_addr(ipaddr);

    if (bind(*sockfd, (struct sockaddr *)(&hostaddr), sizeof(struct sockaddr)) == -1) {
        return -1;
    }

    return 0;
}

static int send_sync_data_eth(int sockfd, char snt_char, char* target_ip)
{
    char buf[2] = {snt_char, 0};

    //Set target addr for sendto
    struct sockaddr_in taddr;
    memset(&taddr, 0, sizeof(taddr));
    taddr.sin_family = AF_INET;
    taddr.sin_port = htons(UDP_PORT);
    taddr.sin_addr.s_addr = inet_addr(target_ip);

    int size = sendto(sockfd, buf, sizeof(buf), 0,
            (struct sockaddr*)&taddr, sizeof(taddr));
    if (size > 0){
        return TRUE;
    } else {
        return FALSE;
    }
}

static int recv_sync_data_eth(int sockfd, char rcv_char, char snt_char)
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    char buf[64];

    /* Watch fd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    /* Wait up to 3 seconds. */
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    struct sockaddr_in raddr;
    socklen_t len;
    memset(&raddr, 0, sizeof(raddr));
    len = sizeof(struct sockaddr_in);

    retval = select(sockfd+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */
    if (retval > 0) {
        /* FD_ISSET(0, &rfds) will be true. */
        DBG_PRINT("Data is available now.\n");

        memset(buf, 0 ,sizeof(buf));
        int size = recvfrom(sockfd, buf, sizeof(buf), 0,
                (struct sockaddr*)&raddr, &len);
        if (size > 0) {
            if (strchr(buf, rcv_char)) {
                return 1;
            } else if (strchr(buf, snt_char)) {
                return -1;
            }
        }
    }

    return 0;
}

/******************************************************************************
 * NAME:
 *      wait_other_side_ready_eth_eth
 *
 * DESCRIPTION:
 *      Wait the test program on the other side(machine) to be ready.
 *
 * PARAMETERS:
 *      None
 *
 * RETURN:
 *      1 - Ready.
 *      0 - Not Ready(fail).
 ******************************************************************************/
int wait_other_side_ready_eth(void)
{
    int sockfd;

    char local_ip[IPSTR_LEN];
    char target_ip[IPSTR_LEN];
    char rcv_char;
    char snt_char;
    char nic[5];

    if (g_machine == 'A') {
        sprintf(local_ip, "192.100.1.2");
        sprintf(target_ip, "192.100.1.3");
        snt_char = DATA_SYNC_A;
        rcv_char = DATA_SYNC_B;
    } else {
        sprintf(local_ip, "192.100.1.3");
        sprintf(target_ip, "192.100.1.2");
        snt_char = DATA_SYNC_B;
        rcv_char = DATA_SYNC_A;
    }

    //Check if nic was enabled in test
    if (g_nim_test_eth[0]) {
        sprintf(nic, "eth0");
    } else if (g_nim_test_eth[1]) {
        sprintf(nic, "eth1");
    } else {
        return TRUE;
    }

    if (0 != set_ipaddr("eth0", local_ip, "255.255.255.0")) {
        printf("Set IP address fail\n");
        return FALSE;
    }

    if (0 != socket_init(&sockfd, local_ip, UDP_PORT)) {
        printf("Set IP address fail\n");
        return FALSE;
    }

    int rc = FALSE;
    while (g_running) {
        /* Send sync request */
        if (send_sync_data_eth(sockfd, snt_char, target_ip) == 0) {
            continue;
        }

        int ch = recv_sync_data_eth(sockfd, rcv_char, snt_char);
        if (ch == 1) {
            send_sync_data_eth(sockfd, snt_char, target_ip);
            rc = TRUE;
            break;
        } else if (ch == -1) {
            printf("Invalid machine!\n");
            rc = FALSE;
            break;
        }
    }

    close(sockfd);
    sleep(3);

    return rc;
}

static int check_link_status(char *ifname)
{
    char buf[MAX_STR_LENGTH];
    char cmd[MAX_STR_LENGTH];
    FILE *read_fp;
    int chars_read;
    int ret = -1;

    snprintf(cmd, sizeof(cmd), "ifconfig %s 2>/dev/null | grep RUNNING",
            ifname);

    memset(buf, 0, MAX_STR_LENGTH);

    read_fp = popen(cmd, "r");
    if (read_fp != NULL) {
        chars_read = fread(buf, sizeof(chars_read), MAX_STR_LENGTH-1, read_fp);
        if (chars_read > 0) {
            ret = 1;
        } else {
            ret = 0;
        }
        pclose(read_fp);
    }

    return ret;
}

/******************************************************************************
 * NAME:
 *      set_if_up(char *ifname)
 *
 * DESCRIPTION:
 *      Bring up interface with given name
 *
 * PARAMETERS:
 *      ifname -ethernet interface name
 *
 * RETURN:
 *      0 - success
 *      other - fail
 ******************************************************************************/
static int set_if_up(char *ifname)
{
    int rc = 0;
    char cmd[MAX_STR_LENGTH];

    if (ifname == NULL)
        return 1;

    sprintf(cmd, "ifconfig %s up > /dev/null 2>&1", ifname);

    rc = DIAG_SYS_RC(system(cmd));

    return rc;
}

/******************************************************************************
 * NAME:
 *      set_if_up_all(uint8_t num)
 *
 * DESCRIPTION:
 *      Bring up interface with given name
 *
 * PARAMETERS:
 *      num -max port
 *
 * RETURN:
 *      NONE
 ******************************************************************************/
void set_if_up_all(void)
{
    char ifname[MAX_STR_LENGTH];
    uint8_t i;

    for (i = 0; i < MAX_NIC_COUNT; i++) {
        sprintf(ifname, "eth%d", i);

        //Briing up interface
        set_if_up(ifname);
    }
}

/******************************************************************************
 * NAME:
 *      wait_link_status_all(uint8_t num)
 *
 * DESCRIPTION:
 *      wait for link up
 *
 * PARAMETERS:
 *      num -max port
 *
 * RETURN:
 *      NONE
 ******************************************************************************/
#define TIME_STEP 10
#define TIME_OUT  3000
void wait_link_status_all(const uint8_t num)
{
    char ifname[MAX_STR_LENGTH];
    uint8_t i;
    uint32_t counter;

    for (i = 0; i < num; i++) {
        sprintf(ifname, "eth%d", i);
        counter = 0;

        //Wait for link up
        while (1) {
            if (check_link_status(ifname))
                break;

            sleep_ms(TIME_STEP);

            counter += TIME_STEP;
            //wait up to 3 seconds
            if (counter > TIME_OUT)
                break;
        }
    }
}

void tc_set_rts_casco(int fd, char enabled)
{
    unsigned char flags = TIOCM_RTS;

    if (enabled == TRUE) {
        ioctl(fd, TIOCMBIC, &flags);
    } else {
        ioctl(fd, TIOCMBIS, &flags);
    }
}
