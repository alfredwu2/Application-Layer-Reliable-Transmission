#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <setjmp.h>
#include "unp.h"
#include "unpifi.h"

static int server_port;
static int send_win;
char path[300];

static struct sockaddr cliaddr;
//static struct sockaddr_in cliaddr;
//static int clilen;

static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;

static int rx_count = 0;

int main(int argc, char ** argv) {

	chdir("..");

	// read arguments from server.in
	FILE * file = fopen("server.in", "r");
	char argument[100];

	// read server port number
	fgets(argument, sizeof(argument), file);
	server_port = atoi(argument);

	// read maximum sender window size
	fgets(argument, sizeof(argument), file);
	send_win = atoi(argument);

	// read path to files on server
	fgets(path, sizeof(path), file);
	path[strlen(path) - 1] = '\0';	// remove trailing newline

	// mapping of socket fd -> ip address
	typedef struct entry {
		int sockfd;
		struct sockaddr_in *sa;
	} entry;
	entry mapping[100];
	int mapcount = 0;

	// other variables
	const int on = 1;
	int sockfd;
	struct ifi_info *ifi, *ifihead;
	struct sockaddr_in *sa;

	//struct sockaddr_in *cliaddr;

	int clilen = sizeof(cliaddr);

	int maxfdp1 = -1;
	fd_set rset;

	Signal(SIGALRM, sig_alrm);

	// bind every unicast address
	for (ifihead = ifi = Get_ifi_info(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
		
		// bind unicast address
		sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		sa = (struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htons(server_port);
		Bind(sockfd, (SA *) sa, sizeof(*sa));
		printf("*** Server binded %s ***\n", Sock_ntop((SA *) sa, sizeof(*sa))); 

		mapping[mapcount].sockfd = sockfd;
		mapping[mapcount].sa = sa;
		mapcount++;
	}



	// listen on all these descriptors
	FD_ZERO(&rset);
	int i;
	for (i = 0; i < mapcount; i++) {
		//printf("test: %d\n", mapping[i].sockfd);
		//printf("test: %s\n", Sock_ntop((SA *) mapping[i].sa, sizeof(mapping[i].sa)));
		FD_SET(mapping[i].sockfd, &rset);
		if (mapping[i].sockfd > maxfdp1) {
			maxfdp1 = mapping[i].sockfd;
		}
	}
	maxfdp1++; // should be 1 greater than largest descriptor number

	while (1) {
		outer:

		Select(maxfdp1, &rset, NULL, NULL, NULL);

		int j;
		for (j = 0; j < mapcount; j++) {
			if (FD_ISSET(mapping[j].sockfd, &rset)) {

				char command[100];
				Recvfrom(mapping[j].sockfd, command, sizeof(command), 0, (SA *) &cliaddr, &clilen); 

				// report where datagram came from
				//printf("*** Client at %s requested connection to %s ***\n", Sock_ntop((SA *) &cliaddr, clilen), Sock_ntop((SA *) mapping[j].sa, sizeof(mapping[j].sa))); 
				printf("*** Client at %s requested connection ***\n", Sock_ntop((SA *) &cliaddr, clilen)); 

				struct sockaddr_in connaddr;
				int connlen = sizeof(connaddr);

				int connfd = Socket(AF_INET, SOCK_DGRAM, 0);
				bzero(&connaddr, sizeof(connaddr));
				connaddr.sin_family = AF_INET;
				connaddr.sin_port = htons(0); // ask kernel to assign ephemeral port
				connaddr.sin_addr = mapping[j].sa->sin_addr;
				Bind(connfd, (SA *) &connaddr, sizeof(connaddr));

				// get the ephemeral port assigned to new connection socket
				Getsockname(connfd, (SA *) &connaddr, &connlen); 

				int recv_win;

				//printf("*** Server sending connection datagram to client. ***\n");

				while (1) {	
				
					if (sigsetjmp(jmpbuf, 1) != 0) {
						if (rx_count == 5) { // this is the 5th attempt
							//printf("*** Server giving up after five attempts. ***\n");
							rx_count = 0;
							goto outer;
						} else {
							//printf("*** Server did not receive ACK from client to connection datagram. ***\n");
							//printf("*** Server resending connection datagram to client. ***\n");
							// also send to connection socket
							Sendto(connfd, &connaddr, connlen, 0, (SA *) &cliaddr, clilen);	
						}
					}

					// send connection datagram to client
					Sendto(mapping[j].sockfd, &connaddr, connlen, 0, (SA *) &cliaddr, clilen);	

					alarm(5);

					// read ACK from client
					recv_win = 0;
					Recvfrom(mapping[j].sockfd, &recv_win, sizeof(recv_win), 0, NULL, NULL);

					//printf("*** Server received ACK from client to connection datagram. ***\n");

					alarm(0);
					break;
				

				}				

				pid_t pid;
				if ((pid = Fork()) == 0) { // child


					Close(mapping[j].sockfd);

					if (strcmp("list", command) == 0) {				

						printf("*** Client requested file list. ***\n"); // TODO temp

						DIR *dir;
						if ((dir = opendir(path)) == NULL) {
							printf("Invalid directory specified.\n");
							exit(1);
						}
						list_send(connfd, (SA *) &cliaddr, clilen, recv_win, send_win, dir, NULL);
					} else if (strncmp("download", command, 8) == 0) {

						chdir(path);
						
						char * filename = command + 9;

						printf("*** Client requested download of file %s ***\n", filename);

						FILE *file;
						if ((file = fopen(filename, "r")) == NULL) {
							exit(1);
						}		

						list_send(connfd, (SA *) &cliaddr, clilen, recv_win, send_win, NULL, file);

					}

				} else {
					// TODO parent
					close(connfd);
				}
			}
		}

	}


}

static void sig_alrm(int signo)
{
	rx_count++;
	siglongjmp(jmpbuf, 1);
}
