#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "declarations.h"
#include <mavlink.h>
#include <time.h>

/* GLOBALS */
FILE						*log_fp;
struct termios	oldset, newset;
int							serial_fd;
int							sock_fd;

/* Signal handler */
static void signal_handler(int sign) {
	if (sign == SIGINT) {
		fprintf(stdout, "SIGINT\n");
		/* Reset settings and terminate gracefully */
		tcsetattr(serial_fd, TCSANOW, &oldset);
		close(serial_fd);
		shutdown(sock_fd, 2);
		close(sock_fd);
		fclose(log_fp);
		exit(0);
	}
}

/* Proxy routine */
void startProxy(int,int, struct sockaddr_in*);

int main(int argc, char* argv[]) {
	/* Variables */
	char filename[STR_LEN] = { '\0' };
	char temp[STR_LEN] = { '\0' };
	struct sockaddr_in dest;
	int opt = 1;
	time_t time_var = time(NULL);
	struct tm time_struct = *localtime(&time_var);

	/* Open serial port with read | write */
	if ((serial_fd = open(SERIAL_INPUT, O_RDWR)) < 0) {
		fprintf(stderr, "Unable to open %s\n", SERIAL_INPUT);
		exit(1);
	} else {
		fprintf(stderr, "Opened %s, descriptor: %d\n", SERIAL_INPUT, serial_fd);
	}

	/* Save current port settings */
	tcgetattr(serial_fd, &oldset);
	bzero(&newset, sizeof(newset));

	/*
	 * CS8			: 8bit no parity 1stopbit
	 * CREAD		: enable receiver
	 * c_lflag	: 0 for non-canonical input
	 * VTIME		: inter-character timer
	 * VMIN			: blocking read until min char */
	newset.c_cflag = BAUDRATE | CS8 | CREAD;
	newset.c_lflag = 0;
	newset.c_cc[VTIME] = 0;		// no timer
	newset.c_cc[VMIN] = 1;	// at least 1 byte 

	/* Flush modem and set with new settings */
	tcflush(serial_fd, TCIFLUSH);
	tcsetattr(serial_fd, TCSANOW, &newset);

	/* Open socket for sending / receiving dgrams
	 * IP protocol family: AF_INET
	 * UDP protocol: SOCK_DGRAM */
	if((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		fprintf(stderr, "Unable to create socket\n");
		exit(1);
	}

	/* Set address and port */
	memset((char*)&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(PORT);
	if (inet_aton(TRGT_IP, &dest.sin_addr) == 0) {
		fprintf(stderr, "inet_aton failed\n");
		exit(1);
	}

	/* Set socket options for port reuse */
	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
		fprintf(stderr, "Error in SO_REUSEPORT\n");
		exit(1);
	}

	/* Set socket options for non-blocking */
	fcntl(sock_fd, F_SETFL, O_NONBLOCK);

	/* Register SIGINT signal handler */
	fprintf(stdout, "Registering signal SIGINT\n");
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		fprintf(stderr, "Cannot register SIGINT\n");
		exit(1);
	}

	/* Open file for logging */
	sprintf(temp, "log_%d_%d_%d_%d_%d_%d.txt", 
			time_struct.tm_year+1900, time_struct.tm_mon+1, time_struct.tm_mday,
			time_struct.tm_hour, time_struct.tm_min, time_struct.tm_sec);
	strcpy(filename, LOG_DIR);
	strcat(filename, temp);
	log_fp = fopen(filename, "w+");
	printf("%s\n", filename);
	if (log_fp != NULL) {
		fprintf(stdout, "Opened %s\n", filename);
	} else {
		fprintf(stderr, "Unable to write to file..\n");
		exit(1);
	}

	/* Start procedure */
	startProxy(sock_fd, serial_fd, &dest);

	return 0;
}

void startProxy(int sock_fd, int serial_fd, struct sockaddr_in* remote) {
	/* Variables */
	mavlink_message_t mavmsg;
	mavlink_status_t status;
	uint8_t buf[BUFFER_LEN] = { 0 }, recvbuf[BUFFER_LEN] = { 0 };
	uint8_t temp;
	uint32_t s_size;
	int i = 0, len, bytes_read, bytes_sent;
	char buf_t[3] = { '\0' };
	char frame_buf[BUFFER_LEN] = { '\0' };

	fprintf(stdout, "Starting proxy..\n");
	s_size = sizeof(*remote);

	/* Main loop */
	while (1) {
		i = 0;
		bytes_read = 0;
		memset(buf_t, '\0', 3);
		memset(frame_buf, '\0', BUFFER_LEN);
		memset((char*)&mavmsg, 0, sizeof(mavmsg));
		memset((char*)&status, 0, sizeof(status));

		/* Read and Parse message while saving it into buffer */
		while((len = read(serial_fd, &temp, 1)) > 0) {
			bytes_read += len;
			buf[i++] = temp;
			snprintf(buf_t, 3, "%02X", temp);
			strcat(frame_buf, buf_t);
			if (mavlink_parse_char(MAVLINK_COMM_0, temp, &mavmsg, &status)) {
				/* Valid packet */
				fprintf(stdout, "%s: SYS: %d, COMP: %d, LEN: %d, MSGID: %d\n",
						TAG_MAVTOGCS, mavmsg.sysid, mavmsg.compid, mavmsg.len, mavmsg.msgid);
				/* Write to log file */
				fprintf(log_fp, "%s: SYS: %d, COMP: %d, LEN: %d, MSGID: %d\n",
						TAG_MAVTOGCS, mavmsg.sysid, mavmsg.compid, mavmsg.len, mavmsg.msgid);
				fprintf(log_fp, "FRAME: %s\n", frame_buf);
				break;
			}
		}

		/* If parse is successful, send to remote */
		if ((bytes_sent = sendto(sock_fd, buf, bytes_read, 
						0, (struct sockaddr*)remote, s_size)) < 0) {
			fprintf(stderr, "Unable to send..\n");
			exit(1);
		}

		/* Reset string buffers */
		memset(buf_t, '\0', 3);
		memset(frame_buf, '\0', BUFFER_LEN);

		/* Get reply from remote(non-blocking) */
		if ((bytes_read = recvfrom(sock_fd, recvbuf, BUFFER_LEN, 0, 
						(struct sockaddr*)remote, &s_size)) < 0) {
			fprintf(stderr, "No reply from UDP\n");
			fprintf(log_fp, "No reply from GCS\n");
		} else {
			fprintf(stdout, "Reading..%d\n",bytes_read );
			/* Parse packet */
			for (i=0; i<bytes_read; i++) {
				snprintf(buf_t, 3, "%02X", recvbuf[i]);
				strcat(frame_buf, buf_t);
				if (mavlink_parse_char(MAVLINK_COMM_0, recvbuf[i], &mavmsg, &status)) {
					/* Print to stdout */
					fprintf(stdout, "%s: SYS: %d, COMP: %d, LEN: %d, MSGID: %d\n",
							TAG_GCSTOMAV, mavmsg.sysid, mavmsg.compid, mavmsg.len, mavmsg.msgid);
					/* Write reply(if there is one) to serial */
					if ((bytes_sent = write(serial_fd, recvbuf, bytes_read)) < 0) {
						fprintf(stderr, "Unable to write to serial!!\n");
					}
					/* Write to log file */
					fprintf(log_fp, "%s: SYS: %d, COMP: %d, LEN: %d, MSGID: %d\n",
							TAG_GCSTOMAV, mavmsg.sysid, mavmsg.compid, mavmsg.len, mavmsg.msgid);
					fprintf(log_fp, "FRAME: %s\n", frame_buf);
				}
			}
		}
	}
}
