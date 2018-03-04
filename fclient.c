#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "unp.h"
#include "unpifi.h"

char server_ip[300];
static int server_port;
static int seed;
static double probability;
static int recv_win;
static int read_delay;

static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;

static int rx_count = 0;

int main(int argc, char ** argv) {

	chdir("..");

	// read arguments from client.in
	FILE * file = fopen("client.in", "r");
	char argument[100];

	// read server ip address
	fgets(server_ip, sizeof(server_ip), file);
	server_ip[strlen(server_ip) - 1] = '\0'; // remove trailing newline

	// read server port number
	fgets(argument, sizeof(argument), file);
	server_port = atoi(argument);

	// read seed for srand()
	fgets(argument, sizeof(argument), file);
	seed = atoi(argument);

	// read probability of datagram loss
	fgets(argument, sizeof(argument), file);
	probability = atof(argument);

	// read maximum receiver window size
	fgets(argument, sizeof(argument), file);	
	recv_win = atoi(argument);

	// read mean ms read-delay
	fgets(argument, sizeof(argument), file);
	read_delay = atoi(argument);

	/*
	// temp
	printf("%s\n", server_ip);
	printf("%d\n", server_port);
	printf("%d\n", seed);
	printf("%f\n", probability);
	printf("%d\n", recv_win);
	printf("%d\n", read_delay);
	*/

	srand(seed);

	//
	const int on = 1;
	int sockfd;
	struct ifi_info *ifi;
	struct sockaddr_in *sa, servaddr;
	int servlen;
	struct sockaddr *addr;

	int salen;
	
	socklen_t *addrlen;

	char command[100];

	Signal(SIGALRM, sig_alrm);

	// bind client to a unicast address linked to network
	ifi = Get_ifi_info(AF_INET, 1)->ifi_next;

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
	sa = (struct sockaddr_in *) ifi->ifi_addr;
	sa->sin_family = AF_INET;
	sa->sin_port = htons(0); // kernel will assign ephemeral port
	Bind(sockfd, (SA *) sa, sizeof(*sa));	
	//printf("bound %s\n", Sock_ntop((SA *) sa, sizeof(*sa))); // temp	

	// Print out IP address / port client is bound to
	Getsockname(sockfd, (SA *) sa, &salen);
	printf("*** Client socket bound to %s ***\n", Sock_ntop((SA *) sa, sizeof(*sa)));


	//use getpeername() to print server's info // TODO get this working

	int first_connection = 1;

	while (1) {


		// TODO connect UDP socket to server
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(server_port);
		Inet_pton(AF_INET, server_ip, &servaddr.sin_addr);
		Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));

		if (first_connection) {
			// Print out IP address / port of server that client is connected to
			Getpeername(sockfd, (SA *) &servaddr, &servlen);
			printf("*** Client socket connected to %s ***\n", Sock_ntop((SA *) &servaddr, sizeof(servaddr)));
			first_connection = 0;
		}

		// prompt user for command
		printf("> ");
		Fgets(command, sizeof(command), stdin);
		command[strlen(command) - 1] = '\0'; // remove trailing newline

		if (strcmp("quit", command) == 0) {
			printf("*** Client exiting. ***\n");
			exit(0);
		} else if (strcmp("list", command) != 0 && strncmp("download", command, 8) != 0) {
			printf("Please enter a valid command.\n");
			continue;
		}

		char redirect[100];
		int isRedirect = 0;

		int i;
		for (i = 9; i < sizeof(command); i++) {
			if (command[i] == ' ') {
				command[i] = '\0';
				strcpy(redirect, command + i + 3);
				isRedirect = 1;
				break;
			}
		}


		struct sockaddr_in connaddr;

		//printf("*** Client sending request to server. ***\n");

		int timeout = 5;
		while (1) {
	
			if (sigsetjmp(jmpbuf, 1) != 0) {
				if (rx_count == 5) { // this is the 5th attempt
					printf("*** Client giving up after five attempts. ***\n");
					exit(0);
				} else {
					printf("*** Client resending request to server... ***\n");
				}
				timeout *= 2;
			}


			alarm(timeout);

			if (rand() / (double)RAND_MAX < probability) {
				// packet dropped
				//printf("*** Client received no response from server to request datagram. ***\n");
				while(1) {}
			} else {

				// Send request datagram to server
				Write(sockfd, command, sizeof(command));

				// Receive connection datagram from server
				Read(sockfd, &connaddr, sizeof(connaddr));

				//printf("*** Client received connection datagram from server. ***\n");

				servaddr.sin_port = connaddr.sin_port;

				alarm(0);
				break;
			}

		}

		// ACK the connection datagram to server, including receive window advertisement
		Write(sockfd, &recv_win, sizeof(recv_win));

		// connect to new connection ip/port
		Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));

		if (strcmp("list", command) == 0) { // user chost list

			// receive file list from server
			list_recv(sockfd, (SA *) &connaddr, sizeof(connaddr), seed, probability, recv_win, read_delay);  


		} else if (strncmp("download", command, 8) == 0) {

			// receive file list from server
			if (isRedirect) {
				list_recv(sockfd, (SA *) &connaddr, sizeof(connaddr), seed, probability, recv_win, read_delay, redirect);  
			} else {
				list_recv(sockfd, (SA *) &connaddr, sizeof(connaddr), seed, probability, recv_win, read_delay, NULL);  
			}

		}

	}

	//sendto(sockfd, buf, sizeof(buf), 0, (SA *) &servaddr, sizeof(servaddr));



}

static void sig_alrm(int signo)
{
	rx_count++;	
	siglongjmp(jmpbuf, 1);
}
