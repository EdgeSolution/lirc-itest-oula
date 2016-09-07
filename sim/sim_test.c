/******************************************************************************
*
* FILENAME:
*     sim_test.c
*
* DESCRIPTION:
*     Define functions for SIM test.
*
* REVISION(MM/DD/YYYY):
*     07/29/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
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
#include "sim_test.h"

#define MAX_RETRY_COUNT 20
#define HEAD_1 0xff
#define HEAD_2 0xfd
#define HEAD_3 0xfc
#define HEAD_4 0xfb
#define HEAD_5 0xfa
#define TAIL 0xfe

#define BUFF_SIZE 265
#define FALSE -1

struct uart_package {
	uint8_t pack_head[5];//0xff 0xfd 0xfc 0xfb 0xfa
	uint8_t serial_number;
	uint8_t pack_data[250];//0x00->0xF9
	uint8_t pack_tail;
	uint32_t  pack_num;
	uint32_t crc_err;
}__attribute__ ((packed));

struct uart_attr {
	char *dev_name;
	int baudrate;
	char *test_type;
}__attribute__ ((packed));

int _err_count = 0;//global variable,count packet loss or error
uint32_t _recived_pack_count = 0;//global variable, count recived packet
uint32_t _send_pack_count = 1;//global variable, count send packet
uint32_t _target_send_pack_num = 0;//Record target amount of packets sent


//gcc -o test my_uart_test.c -lz
int open_uart(char *port);
void close_uart(int fd);

int read_pack_head_1_byte(int fd, uint8_t *buff);

void creat_uart_pack(struct uart_package *uart_pack, uint32_t pack_num, uint8_t serial_number);
int init_uart_port(int fd, int baud_rate);
int send_uart_packet(int fd, struct uart_package * packet_ptr, int len);
int recv_uart_packet(int fd, uint8_t *buff, int len);
void reassembly_packet(struct uart_package * recv_packet, uint8_t *buff);
int analysis_packet(struct uart_package *recv_packet, uint8_t *buff);

int first_recv_port(int fd, uint8_t serial_number);
int first_send_port(int fd, uint8_t serial_number);
void sim_print_status(void);
void sim_print_result(int fd);
void sim_test(void *args);



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
 *		open_uart
 * Description:
 *		Open serial port
 * PARAMETERS:
 *		port: The serial port name at linux
 * Return:
 *		Serial port File descriptor
 */

int open_uart(char *port)
{
	int fd;
	fd = open( port, O_RDWR|O_NOCTTY);
	if (fd < 0) {
		printf("Can't Open Serial Port at %s\n", port);
		return -1;
	} else {
		printf("Open Serial Port at %s Successful\n", port);
		return fd;
	}
}

/*
 * NAME:
 *		close_uart
 * Description:
 *		close serial port
 * PARAMETERS:
 *		fd: File descriptor
 * Return:
 *
 */
void close_uart(int fd)
{
	close(fd);
}

/*
 * NAME:
 *		creat_uart_pack
 * Description:
 *		creat uart package
 * PARAMETERS:
 *		uart_pack: creat uart packet and save into uart_pack
 		pack_num: count packet amount
 		serial_num: uart number
 * Return:
 *
 */
 void creat_uart_pack(struct uart_package *uart_pack, uint32_t pack_num, uint8_t serial_number)
 {
 		uint8_t i = 0;
 		uint32_t crc = 0xFFFFFFFF;

 		uart_pack->pack_head[0] = HEAD_1;
 		uart_pack->pack_head[1] = HEAD_2;
 		uart_pack->pack_head[2] = HEAD_3;
 		uart_pack->pack_head[3] = HEAD_4;
 		uart_pack->pack_head[4] = HEAD_5;

 		uart_pack->serial_number = serial_number;
 		for (i=0; i<=0xF9; i++) {
 			uart_pack->pack_data[i] = i;
 		}
 		uart_pack->pack_tail = TAIL;
 		uart_pack->pack_num = pack_num;

 		/*
		 * calculated CRC
 		 */
 		crc = crc32(0, uart_pack->pack_head, 5);
 		crc = crc32(crc, &uart_pack->serial_number, 1);
 		crc = crc32(crc, uart_pack->pack_data, 250);
 		crc = crc32(crc, &uart_pack->pack_tail, 1);
 		crc = crc32(crc, &uart_pack->pack_num, 4);
 		uart_pack->crc_err = crc;

 		return;
 }

/*
 * Name:
 *		 init_uart_port
 * Description:
 *		 init the serialport
 * PARAMETERS:
 *		 fd:
 *		 baud_rate: baudrate
 * Return:
 *		int(status)
 */
 int init_uart_port(int fd, int baud_rate)
 {
 	struct termios options;
	if  (tcgetattr(fd, &options)  !=  0) {
		perror("Get opertion error,Please check\n");
		return FALSE;
	}
	switch (baud_rate) {
		case 115200:
			cfsetispeed(&options, B115200);
			cfsetospeed(&options, B115200);
			break;
		case 19200:
			cfsetispeed(&options, B19200);
			cfsetospeed(&options, B19200);
			break;
		case 9600:
			cfsetispeed(&options, B9600);
			cfsetospeed(&options, B9600);
			break;
		case 4800:
			cfsetispeed(&options, B4800);
			cfsetospeed(&options, B4800);
			break;
		case 2400:
			cfsetispeed(&options, B2400);
			cfsetospeed(&options, B2400);
			break;
		case 1200:
			cfsetispeed(&options, B1200);
			cfsetospeed(&options, B1200);
			break;
		case 300:
			cfsetispeed(&options, B300);
			cfsetospeed(&options, B300);
			break;
		default:
			printf("Unkown baudrate\n");
			return FALSE;
	}

	// The c_cflag member contains two options that should always be enabled, CLOCAL and CREAD.
	// These will ensure that your program does not become the 'owner' of the port subject
	// to sporatic job control and hangup signals,
	// and also that the serial interface driver will read incoming data bytes.
	options.c_cflag |= (CLOCAL | CREAD);
	// No parity ( 8N1 )
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	// Disable Hardware Flow Control
    options.c_cflag &= ~CRTSCTS;

	//# Line control
	// using raw input
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	//# Input control
	// disable parity check
	options.c_iflag &= ~INPCK;
	// disable SW flow
	options.c_iflag &= ~(IXON | IXOFF | IXANY);
	// disable NL to CR
	options.c_iflag &= ~(INLCR | ICRNL);

	//# Output Control
	// raw output
	options.c_oflag &= ~OPOST;

	//# control characters
	options.c_cc[VMIN] = 0;

	/*active the newtio*/
	if ((tcsetattr(fd, TCSANOW, &options)) != 0) {
		printf("Serial port set error!!!\n");
		return FALSE;
	}
	printf("Serial port set done!!!\n");
	return 0;

 }


/*
 * Name:
 *		 send_uart_packet
 * Description:
 *		 send data
 * PARAMETERS:
 *		 fd:file point
 *		 packet_ptr:data point
 		 len:data length
 * Return:
 *		 send bytes
 */
int send_uart_packet(int fd, struct uart_package * packet_ptr, int len)
{
	uint8_t buff[BUFF_SIZE];
	int ret = 0;
	int i = 0;
	int bytes = 0;
	if (packet_ptr == NULL) {
		printf("Have no packet sent\n");
		return FALSE;
	}
	memcpy(buff + 0, packet_ptr->pack_head, sizeof(packet_ptr->pack_head));

	buff[5] = packet_ptr->serial_number;

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
 *		 read_pack_head_1_byte
 * Description:
 *		 read 1 data from uart
 * PARAMETERS:
 *		 fd:file point
 *		 buff:save data
 * Return:
 *		  read status
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
 *		 recv_uart_packet
 * Description:
 *		 receive data
 * PARAMETERS:
 *		 fd:file point
 *		 buff:save data
 *		 len:data length
 * Return:
 *		  received bytes
 */
int recv_uart_packet(int fd, uint8_t *buff, int len)
{
	int ret = 0;
	int bytes = 0;
	int retry_count = 0;

	ret = read_pack_head_1_byte(fd, buff + 0);
	while (1) {

		/* start if,check head_1 */
		if ((uint8_t)buff[0] == 0xff) {
			/*read head_2*/
			ret = read_pack_head_1_byte(fd, buff + 1);
			/* check read status*/
			if (ret != 1) {
				break;
			}
			/* start if,check head_2 */
			if ((uint8_t)buff[1] == 0xfd) {
				/* read head_3 */
				ret = read_pack_head_1_byte(fd, buff + 2);
				/* check read status*/
				if (ret != 1) {
					break;
				}
				/* start if,check head_3 */
				if ((uint8_t)buff[2] == 0xfc) {
					/* read head_3 */
					ret = read_pack_head_1_byte(fd, buff + 3);
					/* check read status*/
					if (ret != 1) {
						break;
					}
					/* start if,check head_4 */
					if ((uint8_t)buff[3] == 0xfb) {
						/* read head_5 */
						ret = read_pack_head_1_byte(fd, buff + 4);
						/* check read status*/
						if (ret != 1) {
							break;
						}
						/* start if,check head_5 */
						if ((uint8_t)buff[4] == 0xfa) {
							/*check head matching and received data */
							_recived_pack_count++;/* Packet Reception count +1*/
							bytes += 5;
							len -= 5;

							while (len > 0) {
								ret = read(fd, buff + bytes, len);
								if (ret < 0) {
									printf("Read error\n");
									break;
								} else if (ret == 0) {
									if (++retry_count > MAX_RETRY_COUNT) {
										printf("received timeout\n");
										break;
									}
									sleep(1);
									printf("Retry:%d\n", retry_count);
								} else {
									bytes += ret;
									len = len - ret;
								}
							} /* end while (len > 0) */
							break;
						} else {
							buff[0] == buff[4];
							continue;
						} /* end if check head_5 */
					} else {
						buff[0] == buff[3];
						continue;
					} /* end if check head_4 */
				} else {
					buff[0] == buff[2];
					continue;
				} /* end if check head_3 */
			} else {
				buff[0] == buff[1];
				continue;
			} /* end if check head_2 */
		} else {
			ret = read_pack_head_1_byte(fd, buff + 0);
			/* check read status*/
			if (ret != 1) {
				break;
			}
			continue;
		} /* end if check head_1 */
	} /* end while(1) */
	return bytes;
}


/*
 * Name:
 *		 recv_uart_packet
 * Description:
 *		 reassembly packet by buff
 * PARAMETERS:
 *		 recv_packet: packet
 *		 buff:data
 * Return:
 *
 */
void reassembly_packet(struct uart_package * recv_packet, uint8_t *buff)
{
	uint32_t crc;
	uint32_t crc_check;

	memcpy(recv_packet->pack_head, buff + 0, sizeof(recv_packet->pack_head));

	recv_packet->serial_number = buff[5];

	memcpy(recv_packet->pack_data, buff + 6, sizeof(recv_packet->pack_data));

	recv_packet->pack_tail = buff[256];

	memcpy(&recv_packet->pack_num, buff + 257, sizeof(recv_packet->pack_num));

	/*
	 *calculated received-data's CRC code
	 */
	crc = crc32(0, recv_packet->pack_head, 5);
	crc = crc32(crc, &recv_packet->serial_number, 1);
	crc = crc32(crc, recv_packet->pack_data, 250);
	crc = crc32(crc, &recv_packet->pack_tail, 1);
	crc = crc32(crc, &recv_packet->pack_num, 4);
	recv_packet->crc_err = crc;
}


/*
 * Name:
 *		 analysis_packet
 * Description:
 *		 check packet
 * PARAMETERS:
 *		 recv_packet: packet
 *		 buff:data
 * Return:
 *		 0: packet ok
 *      -1: packet error
 *
 */
int analysis_packet(struct uart_package *recv_packet, uint8_t *buff)
{
	uint8_t i = 0;
	uint32_t crc_check = 0xFFFFFFFF;

	/*
	 * check crc and printf which data is error
	 */
	memcpy(&crc_check, buff + 261, sizeof(crc_check)); /* Get received CRC code */
	if ((uint32_t)crc_check != (uint32_t)recv_packet->crc_err) {
		printf("Received packet error\n");

		/*
		 * check HEAD
		 */

		/*
		 * check data
		 */
		for (i=0; i<=0xF9; i++) {
			if ((uint8_t)recv_packet->pack_data[i] != i) {
				printf("Received the %d data error .The true data = %x, received data = %x\n", (uint8_t)i, (uint8_t)i, (uint8_t)recv_packet->pack_data[i]);
			}
		}

		/*
		 * check serial_number
		 */
		//to do....

		/*
		 * check packet number
		 */
		//to do....
		return -1;
	} else {
		_target_send_pack_num=recv_packet->pack_num;
	}
	return 0;

}


/*
 * Name:
 *		 first_send_port
 * Description:
 *		 set computer first send packet
 * PARAMETERS:
 *		 fd:ile point
 *		 serial_number: serial number
 * Return:
 *		 test status
 */
int first_send_port(int fd, uint8_t serial_number)
{
	int status = -1;
	uint8_t buff[BUFF_SIZE];

	int n;

	int flag = 0;

	struct uart_package * uart_pack;
	struct uart_package * recv_packet;

	uart_pack = (struct uart_package *)malloc(sizeof(struct uart_package));
	if (!uart_pack) {
		printf("Not Enough Memory\n");
		free(uart_pack);
		return -1;
	}

	recv_packet = (struct uart_package *)malloc(sizeof(struct uart_package));
	if (!recv_packet) {
		printf("Not Enough Memory\n");
		free(uart_pack);
		free(recv_packet);
		return -1;
	}

	_send_pack_count++;//creat send pack number
	creat_uart_pack(uart_pack, _send_pack_count, serial_number);
	printf("send %d\n", (uint32_t)_send_pack_count);
	n = send_uart_packet(fd, uart_pack, BUFF_SIZE);
	if (n != BUFF_SIZE) {
		printf("send data error\n");
		flag = -1;
	}

	n = recv_uart_packet(fd, buff, BUFF_SIZE);
	if (n != BUFF_SIZE) {
		printf("recv_uart_packet data error\n");
		flag = -1;
	}

	reassembly_packet(recv_packet, buff);
	status = analysis_packet(recv_packet, buff);
	if (status != 0) {
		_err_count++;
		flag = -1;

	}

	free(uart_pack);
	free(recv_packet);

	return flag;
}


/*
 * Name:
 *		 first_recv_port
 * Description:
 *		 set computer first received packet
 * PARAMETERS:
 *		 fd:ile point
 *		 serial_number: serial number
 * Return:
 *		 test status (-1 for error; 0 for pass;)
 */
int first_recv_port(int fd, uint8_t serial_number)
{
	int status;

	int flag = 0;

	uint8_t buff[BUFF_SIZE];

	int n;
	struct uart_package * uart_pack;
	struct uart_package * recv_packet;

	uart_pack = (struct uart_package *)malloc(sizeof(struct uart_package));
	if (!uart_pack) {
		printf("Not Enough Memory\n");
		free(uart_pack);
		return -1;
	}

	recv_packet = (struct uart_package *)malloc(sizeof(struct uart_package));
	if (!recv_packet) {
		printf("Not Enough Memory\n");
		free(recv_packet);
		free(uart_pack);
		return -1;
	}

	n = recv_uart_packet(fd, buff, 265);
	if (n != BUFF_SIZE) {
		printf("recv_uart_packet data error\n");
		flag = -1;
	}

	reassembly_packet(recv_packet, buff);

	status = analysis_packet(recv_packet, buff);
	if (status != 0) {
		_err_count++;
		flag = -1;
	}

	_send_pack_count++;//creat send pack number
	creat_uart_pack(uart_pack, _send_pack_count, serial_number);
	printf("send %d\n", (uint32_t)_send_pack_count);
	n = send_uart_packet(fd, uart_pack, 265);
	if (n != BUFF_SIZE) {
		printf("send data error\n");
		flag = -1;
	}


	free(uart_pack);
	free(recv_packet);

	return flag;
}


/*
 * Name:
 *		 sim_test
 * Description:
 *		 test uart
 * PARAMETERS:
 *		 args: uart attribute
 * Return:
 *		 NULL
 */
void sim_test(void *args)
{

	struct uart_attr *uart_param = (struct uart_attr *)args;

	int (*uart_function)(int fd, uint8_t serial_number);

	int fd;
	char *port;

	int status;

	uint8_t serial_number = (uint8_t)(uart_param->dev_name[9]);

	int log_fd = test_mod_sim.log_fd;

	_send_pack_count = 0;//init _send_pack_count;

	port = uart_param->dev_name;
	fd = open_uart(port);
	if (fd < 0) {
		printf("Open %s error\n",port);
	}

	if (init_uart_port(fd, uart_param->baudrate) < 0) {
		printf("init uart error\n");
		return;
	}

	if ((strcmp(uart_param->test_type, "B") == 0 )) {
		uart_function = first_recv_port;
	} else if ((strcmp(uart_param->test_type, "A") == 0 )) {
		uart_function = first_send_port;
	} else {
		printf("Set Uart param error\n");
		printf("Please check it\n");
		return;
	}

	log_print(log_fd, "Begin test!\n\n");

	while (g_running) {
		status = (*uart_function)(fd, serial_number);
		if (status != 0) {
			test_mod_sim.pass = -1;
		}
		sleep(1);
	}

	log_print(log_fd, "Test end\n\n");
	pthread_exit(NULL);
}

/*
 * Name:
 *		 sim_print_result
 * Description:
 *		 print test result
 * PARAMETERS:
 *		 fd: log file point
 * Return:
 *		 NULL
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
 *		 sim_print_status
 * Description:
 *		 print ,pactet loss rate,error rate ..
 * PARAMETERS:
 *		 NULL
 * Return:
 *		 NULL
 */
void sim_print_status(void)
{
	float rate;

	rate = (float)(_target_send_pack_num - _recived_pack_count)/_recived_pack_count;

	printf("Packet loss rate = %.2f\n", rate);
	printf("Error packet = %d\n",_err_count);
	printf("Send %d packet\n", (uint32_t)_send_pack_count);
	printf("target send packet = %d\n",(uint32_t)_target_send_pack_num);
}

void main(int argc, char *argv[])
{
	struct uart_attr *uart_param;
	int baudrate;

	uart_param = (struct uart_attr *)malloc(sizeof(struct uart_attr));
	if (!uart_param) {
		printf("Not Enough Memory\n");
		free(uart_param);
		return;
	}

	uart_param->test_type = argv[1];
	uart_param->dev_name = argv[2];
	uart_param->baudrate = atoi(argv[3]);

	sim_test(uart_param);

	sim_print_status();
}
