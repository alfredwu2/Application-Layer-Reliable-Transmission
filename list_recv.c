#include <setjmp.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include "unprtt.h"
#include "unpthread.h"

static struct msghdr msgsend, msgrecv;	/* assumed init to 0 */
static struct hdr {
	int32_t seq;	/* sequence # */
	uint32_t ts;	/* timestamp when sent */
	uint32_t retransmit;	/* is the packet a retransmission? */
	uint32_t room;	/* receiver window advertisement */
	uint32_t probe;
	uint32_t end;	/* informs reciver that all the data has been sent */
} sendhdr, recvhdr;

static int data_len = 512 - sizeof(struct hdr);

typedef struct datagram {	// entry in a receiver window
	uint32_t seq;
	uint32_t valid;
	char data [512 - sizeof(struct hdr)];
} datagram;
static datagram recv_buff[100];

static int last_read;

static int recv_win_val;
static int *recv_win_ptr = &recv_win_val;

static int filedes;

static int static_read_delay;

static char * redirect_file;

// mutex
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

void * doit(void *arg);	// prototype

// custom printf that prepends "Receiver > " and colorizes output
// Uses mutex to prevent reading thread from causing race condition
void printo(const char *format, ...)
{
	Pthread_mutex_lock(&print_mutex);

	va_list args;
	va_start(args, format);

	Fputs("\033[034m", stdout);
	printf("Receiver > ");
	vprintf(format, args);
	Fputs("\033[0m", stdout);

	va_end(args);

	Pthread_mutex_unlock(&print_mutex);
}

ssize_t list_recv(int fd, const SA *destaddr, socklen_t destlen, int seed, double probability, int recv_win, int read_delay, char * redirect)
{

	Pthread_mutex_unlock(&counter_mutex);
	Pthread_mutex_unlock(&print_mutex);

	// initialize variables
	bzero(&msgsend, sizeof(msgsend));
	bzero(&msgrecv, sizeof(msgrecv));
	bzero(&sendhdr, sizeof(sendhdr));
	bzero(&recvhdr, sizeof(recvhdr));
	bzero(&recv_buff, sizeof(recv_buff));
	last_read = -1;
	redirect_file = redirect;

	// initialize seed
	srand(seed);

	filedes = fd;

	pthread_t tid;

	static_read_delay = read_delay;

	ssize_t n;
	struct iovec iovsend[2], iovrecv[2];

	char outbuff[data_len];
	char inbuff[data_len];
	//datagram recv_window[win_size];	// receive window

	//msgsend.msg_name = destaddr;
	//msgsend.msg_namelen = destlen;
	msgsend.msg_name = NULL;
	msgsend.msg_namelen = destlen;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = outbuff;
	iovsend[1].iov_len = data_len;

	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	iovrecv[1].iov_base = inbuff;
	iovrecv[1].iov_len = data_len;

	*recv_win_ptr = recv_win;

	Pthread_create(&tid, NULL, doit, (void *) recv_win_ptr);


	while (1) {

		// Receive next packet
		Recvmsg(fd, &msgrecv, 0);

		if (recvhdr.probe == 1) {

			// transmit window update packet to sender if room in receive buffer opens
			int j;
			int count = 0;
			for (j = recv_win - 1; j >= 0; j--) {
				if (recv_buff[j].valid == 1) {
					break;
				} else {
					count++;
				}
			}
			
			sendhdr.seq = last_read;
			sendhdr.room = count;
			sendhdr.probe = 1;
			Sendmsg(filedes, &msgsend, 0);
			sendhdr.probe = 0;
		}

		// Did sender signal end of data?
		if (recvhdr.end == 1) {
			printo("Sender signalled end of data.\n");
			break;
		}

		// Incoming packet loss simulation
		if ( (rand() / (double)RAND_MAX) < probability) {
			printo("Packet %d dropped in transit from sender.\n", recvhdr.seq);
			continue;
		} else {
			printo("Packet %d received from sender.\n", recvhdr.seq);
		}

		

		Pthread_mutex_lock(&counter_mutex);

		int index = recvhdr.seq - (last_read + 1);
		// if the buffer is not full, otherwise drop packet
		if (index < recv_win) {


	
			if (index < 0) {
				printo("Discarding packet %d - already read.\n", recvhdr.seq);
			} else {
				printo("Storing packet %d in receive buffer.\n", recvhdr.seq);
				recv_buff[index].seq = recvhdr.seq;
				recv_buff[index].valid = 1;
				strcpy(recv_buff[index].data, inbuff);

				if (index == recv_win - 1) {
					printo("Receive buffer now full.\n");
				}
			}




			// cumulative ack and receiver window advertisement

			// calculate cumulative ack
			int i;
			int cumulative_ack = -1;
			for (i = 0; i < recv_win; i++) {
				if (recv_buff[i].valid == 0) {
					break;
				} else {
					cumulative_ack = recv_buff[i].seq;
				}
			}


			if (cumulative_ack > last_read) {
				sendhdr.seq = cumulative_ack;
			} else {
				sendhdr.seq = last_read;
			}




			int j;
			int room = 0;
			for (j = recv_win - 1; j >= 0; j--) {
				if (recv_buff[j].valid == 1) {
					break;
				} else {
					room++;
				}			
			}

			sendhdr.room = room;


			// echo timestamp and retransmit flag of received packet
			sendhdr.ts = recvhdr.ts;
			sendhdr.retransmit = recvhdr.retransmit;



			if (sendhdr.seq >= 0) {
				if ( (rand() / (double)RAND_MAX) < probability) {
					printo("ACK to sender for packet %d lost in transmission.\n", sendhdr.seq);	
				} else {
					printo("ACKing up to packet %d. Window advertisement is %d.\n", sendhdr.seq, sendhdr.room);
					
					sendmsg(fd, &msgsend, 0);
				}
			}


		} else {
			printo("Receive buffer full. Dropping packet %d.\n", recvhdr.seq);
		}


		Pthread_mutex_unlock(&counter_mutex);

	}

	// Wait for reading thread to terminate.
	Pthread_join(tid, NULL);

}

void * doit(void *arg)
{


	int recv_win = *(int *) arg;

	FILE *output;
	if (redirect_file != NULL) {
		output = fopen(redirect_file, "w");
	} else {
		output = stdout;
	}

	//Pthread_detach(pthread_self());
	
	while (1) {

		// sleep for exponential distribution of time

		double rand_range = (double) rand() / RAND_MAX;
		int sleeptime = -1 * static_read_delay * log(rand_range);		
		usleep(sleeptime);

		//Pthread_mutex_lock(&counter_mutex);

		// TODO


		Pthread_mutex_lock(&counter_mutex);

		while (1) {

			if (recv_buff[0].valid == 1) {



				printo("Reading and displaying packet %d from buffer.\n", recv_buff[0].seq);
				if (recv_buff[recv_win - 1].valid == 1) {
					printo("Space in receive buffer opened up.\n");
				}

				last_read = recv_buff[0].seq;

				Pthread_mutex_lock(&print_mutex);

				Fputs(recv_buff[0].data, output);
				if (redirect_file == NULL) {
					Fputs("\n", output);	
				}
			
				Pthread_mutex_unlock(&print_mutex);


	
				// shift everything in window to the left
				int i;
				for (i = 0; i < recv_win - 1; i++) {
					recv_buff[i].seq = recv_buff[i+1].seq;
					recv_buff[i].valid = recv_buff[i+1].valid;
					strcpy(recv_buff[i].data, recv_buff[i+1].data);
				}
				recv_buff[i].valid = 0;	// empty last slot in window


			} else {
				break;
			}
		}

		if (recvhdr.end == 1) {
			break;
		}

		Pthread_mutex_unlock(&counter_mutex);
	}

	if (redirect_file != NULL) {
		fclose(output);
	}
	
	return (NULL);
}
