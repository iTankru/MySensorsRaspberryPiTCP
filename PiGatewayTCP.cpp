/*
 * PiGatewayTCP.cpp - MySensors Gateway for wireless node providing a TCP interface
 *
 * Copyright 2016 Andrey Ivanov <itank@itank.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <syslog.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <RF24.h>
#include <MyGateway.h>
#include <Version.h>

#define PORT    5003
#define MAXMSG  512
pthread_t thread_id;

/* variable indicating if the server is still running */
volatile static int running = 1;
MyGateway *gw = NULL;
int sock;
fd_set active_fd_set, read_fd_set;

int daemonizeFlag = 0;

void openSyslog() {
	setlogmask (LOG_UPTO(LOG_INFO));openlog(NULL, 0, LOG_USER);
}

void closeSyslog() {
	closelog();
}

int make_socket(uint16_t port) {
	int sock;
	struct sockaddr_in name;

	/* Create the socket. */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit (EXIT_FAILURE);
	}

	/* Give the socket a name. */
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		perror("setsockopt");
	if (bind(sock, (struct sockaddr *) &name, sizeof(name)) < 0) {
		perror("bind");
		exit (EXIT_FAILURE);
	}

	return sock;
}

int read_from_client(int filedes) {
	char buffer[MAXMSG];
	int nbytes;

	nbytes = read(filedes, buffer, MAXMSG);
	if (nbytes < 0) {
		/* Read error. */
		perror("read");
		exit (EXIT_FAILURE);
	} else if (nbytes == 0)
		/* End-of-file. */
		return -1;
	else {
		/* Data read. */
		strtok(buffer, "\r\n");

		log(LOG_INFO, "[TCPServer] receive: '%s'\n", buffer);
		buffer[nbytes] = '\0';
		gw->parseAndSend(buffer);

		return 0;
	}
}

void log(int priority, const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	if (daemonizeFlag == 1) {
		vsyslog(priority, format, argptr);
	} else {
		vprintf(format, argptr);
	}
	va_end(argptr);
}

/*
 * handler for SIGINT signal
 */
void handle_sigint(int sig) {
	log(LOG_INFO, "Received SIGINT\n");
	running = 0;
}

void handle_sigusr1(int sig) {
	log(LOG_INFO, "Received SIGUSR1\n");
	int curLogLevel = setlogmask(0);
	if (curLogLevel != LOG_UPTO(LOG_DEBUG))
		setlogmask (LOG_UPTO(LOG_DEBUG));else setlogmask(LOG_UPTO (LOG_INFO));
	}

void write_msg_to_pty(char *msg) {
	if (msg == NULL) {
		log(LOG_WARNING, "[callback] NULL msg received!\n");
		return;
	}
	for (int i = 0; i < FD_SETSIZE; ++i)
		if (FD_ISSET(i, &read_fd_set))
			if (i != sock)
				write(i, msg, strlen(msg));
	strtok(msg, "\r\n");

	log(LOG_INFO, "[TCPServer] send: '%s'\n", msg);

}
static void daemonize(void) {
	pid_t pid, sid;
	int fd;

	// already a daemon
	if (getppid() == 1)
		return;

	// Fork off the parent process  
	pid = fork();
	if (pid < 0)
		exit (EXIT_FAILURE);  // fork() failed 
	if (pid > 0)
		exit (EXIT_SUCCESS); // fork() successful, this is the parent process, kill it

	// From here on it is child only

	// Create a new SID for the child process
	sid = setsid();
	if (sid < 0)
		exit (EXIT_FAILURE);  // Not logging as nobody can see it.

	// Change the current working directory. 
	if ((chdir("/")) < 0)
		exit (EXIT_FAILURE);

	// Divert the standard file desciptors to /dev/null
	fd = open("/dev/null", O_RDWR, 0);
	if (fd != -1) {
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);

		if (fd > 2)
			close(fd);
	}

	// reset File Creation Mask 
	umask(027);
}
void *connection_nrf(void *running);
void *connection_nrf(void *running) {
	while (running)
		gw->processRadioMessage();
	return 0;
}
;

/*
 * Main gateway logic
 */

int main(int argc, char **argv) {
	int status = EXIT_SUCCESS;
	int c;

	while ((c = getopt(argc, argv, "d")) != -1) {
		switch (c) {
		case 'd':
			daemonizeFlag = 1;
			break;
		}
	}
	openSyslog();
	log(LOG_INFO, "Starting PiGatewayTCP...\n");
	log(LOG_INFO, "Protocol version - %s\n", LIBRARY_VERSION);
	log(LOG_INFO, "run 'PiGatewayTCP -d' for DEMONIZE...\n");

	/* register the signal handler */
	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);
	signal(SIGUSR1, handle_sigusr1);

	/* tcp */
	extern int make_socket(uint16_t port);

	int i;
	struct sockaddr_in clientname;
	size_t size;

	/* Create the socket and set it up to accept connections. */
	sock = make_socket(PORT);
	if (listen(sock, 1) < 0) {
		perror("listen");
		exit (EXIT_FAILURE);
	}
	log(LOG_INFO, "[TCPServer] TCPListen  0.0.0.0:%i\n", PORT);

	/* Initialize the set of active sockets. */
	FD_ZERO(&active_fd_set);
	FD_SET(sock, &active_fd_set);

	/* create MySensors Gateway object */
#ifdef __PI_BPLUS
	gw = new MyGateway(RPI_BPLUS_GPIO_J8_22, RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ, 1);
#else
	gw = new MyGateway(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0,
			BCM2835_SPI_SPEED_8MHZ, 1);
#endif	
	if (gw == NULL) {
		log(LOG_ERR, "Could not create MyGateway! (%d) %s\n", errno,
				strerror(errno));
		status = EXIT_FAILURE;
		goto cleanup;
	}

	if (daemonizeFlag)
		daemonize();
	/* we are ready, initialize the Gateway */
	gw->begin(RF24_PA_LEVEL_GW, RF24_CHANNEL, RF24_DATARATE, &write_msg_to_pty);

	if (pthread_create(&thread_id, NULL, connection_nrf, (void*) &running)
			< 0) {
		perror("could not create thread");
		return 1;
	}

	while (running) { /* Block until input arrives on one or more active sockets. */

		read_fd_set = active_fd_set;
		if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
			perror("select");
			exit (EXIT_FAILURE);
		}

		/* Service all the sockets with input pending. */
		for (i = 0; i < FD_SETSIZE; ++i)
			if (FD_ISSET(i, &read_fd_set)) {
				if (i == sock) {
					/* Connection request on original socket. */
					int new_connect;
					size = sizeof(clientname);
					new_connect = accept(sock, (struct sockaddr *) &clientname,
							(socklen_t*) &size);
					if (new_connect < 0) {
						perror("accept");
						exit (EXIT_FAILURE);
					}
					log(LOG_INFO,
							"[TCPServer] connect from host %s, port %hd \n",
							inet_ntoa(clientname.sin_addr),
							ntohs(clientname.sin_port));
					FD_SET(new_connect, &active_fd_set);
				} else {
					/* Data arriving on an already-connected socket. */
					if (read_from_client(i) < 0) {
						close(i);
						FD_CLR(i, &active_fd_set);
					}
				}
			}

	}

	/* Do the work until interrupted */

	cleanup: log(LOG_INFO, "Exiting...\n");
	if (gw)
		delete (gw);
	shutdown(sock, SHUT_RDWR);
	closeSyslog();
	return status;
}
