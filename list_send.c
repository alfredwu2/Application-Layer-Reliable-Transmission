#include <setjmp.h>
#include <dirent.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "unprtt.h"

#define RTT_DEBUG

static struct rtt_info rttinfo;
static int rttinit = 0;
static struct msghdr msgsend, msgrecv;	/* assumed init to 0 */
static struct hdr {
	int32_t seq;	/* sequence # */
	uint32_t ts;	/* timestamp when sent */
	uint32_t retransmit;	/* is the packet a retransmission? */
	uint32_t room;	/* receiver window advertisement */
	uint32_t probe;
	uint32_t end;	/* informs receiver that all the data has been sent */ 
} sendhdr, recvhdr;

typedef struct datagram {	// entry in a sender window
	uint32_t seq;
	uint32_t valid;
	char data [512 - sizeof(struct hdr)];
} datagram;
static datagram send_buff[100];

static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;
static int last_acked = -1;
static int fast_retransmit_counter = 1;

static double cwnd = 1;	// congestion control window
static int slowstart_active = 1;
static int ssthresh = 128;

void printo(const char*format, ...)
{

	va_list args;
	va_start(args, format);

	Fputs("\033[032m", stdout);
	printf("Sender > ");
	vprintf(format, args);
	Fputs("\033[0m", stdout);

	va_end(args);
}

ssize_t list_send(int fd, const SA *destaddr, socklen_t destlen, int recv_win, int send_win, DIR *dir, FILE *file)
{
	ssize_t n;
	struct iovec iovsend[2], iovrecv[2];

	int data_len = 512 - sizeof(struct hdr);	// length of data buffer
	char outbuff[data_len];	// data buffer of outgoing packet
	char inbuff[data_len];	// data buffer of ingoing packet
	//struct msghdr send_window[win_size];	// sending window (temporarily set to 10)
	int send_window_head = 0;
	int send_window_tail = 0;

	if (rttinit == 0) {
		rtt_init(&rttinfo);	/* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}

	msgsend.msg_name = destaddr;
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

	struct dirent *dp;
	int eof = 0;	// flag indicating whether end-of-file has been reached

	Signal(SIGALRM, sig_alrm);


	char filelist[10000];
	int fl_len = 0;
	int fl_index = 0;

	if (dir != NULL) {
		while ((dp = readdir(dir)) != NULL) {
			if (dp->d_name[0] != '.') {
				int i;
				for (i = 0; i < strlen(dp->d_name); i++) {
					filelist[fl_len++] = dp->d_name[i];
				}	
				filelist[fl_len++] = '\n';
			}
		}
	}

	while (1) {

		// TODO fix bug with seed scenario 45
		// sending packets 11, 12, 13, even though ack for 10 not received
		// actually this doesn't appear to be a bug
		// wait, maybe it is - what if sender window > receiver window?
		// but wait, receiver window advertisement would still prevent overflowing rcver
		// ok making sender window 15 still results in constrained sending, come
		// back to this possible bug

		if (eof == 0 && sendhdr.seq <= last_acked + recv_win && sendhdr.seq <= last_acked + send_win && sendhdr.seq <= last_acked + cwnd) {

			if (dir != NULL) {
				/*
				if ((dp = readdir(dir)) != NULL) {
					if (dp->d_name[0] != '.') {
						sendhdr.ts = rtt_ts(&rttinfo);				
						strcpy(outbuff, dp->d_name);
						printo("Sending packet %d.\n", sendhdr.seq);
						Sendmsg(fd, &msgsend, 0);
						// store just-sent datagram in sender window buffer
						int index = sendhdr.seq - (last_acked + 1);
						send_buff[index].seq = sendhdr.seq;
						send_buff[index].valid = 1;
						strcpy(send_buff[index].data, outbuff);
						// increment sequence number
						sendhdr.seq++;
						// set alarm
						alarm(rtt_start(&rttinfo));
					}
				} else {
					eof = 1;	// end of file has been reached
					
					//printo("Final packet is %d.\n", sendhdr.seq - 1);

					if (last_acked == sendhdr.seq - 1) {
						// send a packet signalling end of data
						sendhdr.end = 1;
						Sendmsg(fd, &msgsend, 0);
						printo("Signalling to receiver end of data.\n");
						//break;
						exit(0);
					}
					

				}
				*/

				sendhdr.ts = rtt_ts(&rttinfo);				
				int i;
				for (i = 0; i < sizeof(outbuff); i++) {
					if (fl_index >= fl_len) {
						eof = 1;
						outbuff[i] = 0;
					} else {
						outbuff[i] = filelist[fl_index++];
					}
				}

				printo("Sending packet %d.\n", sendhdr.seq);
				Sendmsg(fd, &msgsend, 0);
				// store just-sent datagram in sender window buffer
				int index = sendhdr.seq - (last_acked + 1);
				send_buff[index].seq = sendhdr.seq;
				send_buff[index].valid = 1;
				strcpy(send_buff[index].data, outbuff);				
				// increment sequence number
				sendhdr.seq++;
				// set alarm
				alarm(rtt_start(&rttinfo));

			} else if (file != NULL) {


				sendhdr.ts = rtt_ts(&rttinfo);				
				int i;
				for (i = 0; i < sizeof(outbuff); i++) {
					char c;
					if ((c = fgetc(file)) != EOF) {
						outbuff[i] = c;
					} else {
						eof = 1;
						outbuff[i] = 0;
					}
				}
				printo("Sending packet %d.\n", sendhdr.seq);
				Sendmsg(fd, &msgsend, 0);
				// store just-sent datagram in sender window buffer
				int index = sendhdr.seq - (last_acked + 1);
				send_buff[index].seq = sendhdr.seq;
				send_buff[index].valid = 1;
				strcpy(send_buff[index].data, outbuff);				
				// increment sequence number
				sendhdr.seq++;
				// set alarm
				alarm(rtt_start(&rttinfo));

			}

		} else {
			if (sendhdr.seq > last_acked + send_win) {
				printo("Send buffer full.\n");
			}

			// Receive ACK from receiver
			Recvmsg(fd, &msgrecv, 0);


			recv_win = recvhdr.room;
			// don't update last_acked if the received packet was a window updater (not a real ACK)
			if (recvhdr.probe == 0) {
				// count duplicate ACKs for fast retransmit
				if (recvhdr.seq == last_acked) {
					fast_retransmit_counter++;
				} else {
					fast_retransmit_counter = 1;
				}

				// if ACK wasn't for retransmission, calculate new RTT
				if (recvhdr.retransmit == 0) {
					rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);
				}

				// reset timer if ACK received for new data
				if (recvhdr.seq > last_acked) {
					alarm(rtt_start(&rttinfo));
				}
				int difference = recvhdr.seq - last_acked; // for shifting
				last_acked = recvhdr.seq;
				printo("Received cumulative ACK up to and including packet %d.\n", recvhdr.seq);


				// if end-of-file and the last acked packet is equal to
				// the last data packet (one less than current sendhdr.seq)
				// then signal to receiver end of data
				if (eof && last_acked == sendhdr.seq - 1) {
					// send a packet signalling end of data
					sendhdr.end = 1;
					Sendmsg(fd, &msgsend, 0);
					printo("Signalling to receiver end of data.\n");
					//break;
					exit(0);
				}


				// if in slow start phase, increase cwnd by 1 per ACK
				if (slowstart_active) {
					printo("[Slow Start phase] cwnd increased from %.2f to %.2f.\n", cwnd, cwnd + 1);
					cwnd++;
					if (cwnd > ssthresh) {
						slowstart_active = 0;
						printo("ssthresh exceeded, Slow Start ending and Congestion Avoidance beginning.\n");
					}
				} else {
					printo("[Congestion Avoidance phase] cwnd increased from %f to %f (rounded to %d).\n", cwnd, cwnd + 1 / cwnd, (int) (cwnd + 1 / cwnd));
					cwnd += 1 / cwnd;
				}

				// shift sender window left until first unACKed packet is index 0
				int n;
				for (n = 0; n < difference; n++) {
					int i;
					for (i = 0; i < send_win - 1; i++) {
						send_buff[i].seq = send_buff[i+1].seq;
						send_buff[i].valid = send_buff[i+1].valid;
						strcpy(send_buff[i].data, send_buff[i+1].data);
					}
					send_buff[i].valid = 0; // empty last slot in window
				}

				// if third duplicate ACK, fast retransmit
				if (fast_retransmit_counter == 4) {
					printo("Third duplicate ACK received. Enabling Fast Retransmit.\n");
					ssthresh = cwnd / 2;
					if (ssthresh < 1) {
						ssthresh = 1;
					}
					cwnd = 1;
					printo("Setting ssthresh to cwnd / 2 = %d. Setting cwnd to 1. Restarting threshold slow start phase.\n", ssthresh); 
					//printo("Setting ssthresh to half of cwnd, and cwnd to new ssthresh plus 3");
					siglongjmp(jmpbuf, 1); // resend first unACKed packet
				}

			} else {
				//printo("Received window update packet after packet %d was read on receiver end.\n", recvhdr.seq);
			}

		}

		if (sigsetjmp(jmpbuf, 1) != 0) {

			slowstart_active = 1;

			// if receive window empty, send a window size probe
			if (recv_win == 0) {
				printo("Sending probe to discover current receive window size.\n");
				sendhdr.probe = 1;
				Sendmsg(fd,&msgsend, 0);
				sendhdr.probe = 0;
			}

			// retransmit first unACKed packet			
			int32_t temp = sendhdr.seq;	// save sequence number
			sendhdr.seq = last_acked + 1;	// temporarily set to first packet after last ACKed
			sendhdr.retransmit = 1;	// set retransmit flag for this packet
			strcpy(outbuff, send_buff[0].data);
			printo("Re-sending packet %d.\n", sendhdr.seq);
			Sendmsg(fd, &msgsend, 0);
			sendhdr.seq = temp;	// restore sequence number
			sendhdr.retransmit = 0;	// restore retransmit flag to false (0)
			alarm(rtt_start(&rttinfo));

		}
		
		//alarm(0);	// stop SIGALRM timer
		//alarm_set = 0;
	}

}

static void sig_alrm(int signo)
{
	printo("Retransmission timer expired.\n");

	ssthresh = cwnd / 2;
	if (ssthresh < 1) {
		ssthresh = 1;
	}
	cwnd = 1;

	printo("Setting ssthresh to cwnd / 2 = %d. Setting cwnd to 1. Restarting threshold slow start phase.\n", ssthresh); 
	
	

	siglongjmp(jmpbuf, 1);
}
