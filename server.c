#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "unp.h"

int main(int argc, char ** argv) {

	/*
	// TODO
	// Implement printing out the contents of current working directory

	DIR *dir;
	struct dirent *dp;
	
	if ((dir = opendir(".")) == NULL) {
		printf("Unable to open current directory.");
		exit(1);
	}

	while ((dp = readdir(dir)) != NULL) {
		if (dp->d_name[0] != '.') {
			printf("%s\n", dp->d_name);
		}
	}	
	*/

	int sockfd;
	struct sockaddr_in servaddr, cliaddr;

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT + 1);

	Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

	
	// temp
	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = htons(SERV_PORT + 2); // port should be 0, temp 5000
	Inet_pton(AF_INET, "127.0.0.1", &cliaddr.sin_addr);	
	

	//
	dg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr));

}

void dg_echo(int sockfd, SA *pcliaddr, socklen_t clilen) {

	/*
	DIR *dir;
	if ((dir = opendir(".")) == NULL) {
		printf("Unable to open current directory.");
		exit(1);
	}
	*/

	//list_send(sockfd, pcliaddr, clilen, dir);
	list_recv(sockfd, pcliaddr, clilen, 45, 0.2, 5);

	/*
	int n;
	socklen_t len;
	char mesg[MAXLINE];
	char packet[512];

	for ( ; ; ) {
		len = clilen;
		n = Recvfrom(sockfd, mesg, MAXLINE, 0, pcliaddr, &len);
		mesg[n] = 0;	

		if (strcmp(mesg, "list\n") == 0) {
			DIR *dir;
			struct dirent *dp;

			if ((dir = opendir(".")) == NULL) {
				printf("Unable to open current directory.");
				exit(1);
			}

			while ((dp = readdir(dir)) != NULL) {
				if (dp->d_name[0] != '.') {
					strcpy(packet, dp->d_name);
					Sendto(sockfd, packet, 512, 0, pcliaddr, len);
				}
			}
	
		}

		//Sendto(sockfd, mesg, n, 0, pcliaddr, len);

	}
	*/

}
