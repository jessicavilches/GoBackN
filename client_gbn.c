#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#define WIN 5
#define TIMEOUT 1
#define DELAY 210000

struct MyPacket         //Design your packet format (fields and possible values)
{
    char type;
    unsigned short seqno;
    unsigned int offset;
    int length;
    int eof;
    char payload[256];
};

struct PacketInfo {
	struct MyPacket p;
	int time_sent;
};

void error(const char *msg)
{
    perror(msg);
    exit(0);
}


int main(int argc, char *argv[])
{
    clock_t start_sending;
    clock_t done_sending;
    double total_time;
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    socklen_t addrlen = sizeof(serv_addr);
    char buffer[256];


    char message[256];

    struct PacketInfo pkt_buf[WIN] = {};
    struct MyPacket pkt = {};
    struct timeval timeout;
    struct MyPacket *pkt_rcvd = malloc(sizeof(struct MyPacket));

    clock_t start, end;

    if (argc != 4) {
       fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
       exit(0);
    }


    /* Create a socket */
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    /* fill in the server's address and port*/
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    bzero(&timeout, sizeof(timeout));
    timeout.tv_usec = DELAY;
    int errorid = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (errorid < 0){
        printf("errorid: %d\n", errorid);
        error("ERROR timeout set\n");
   }
 	/* create file for reading*/

    FILE *fp = fopen(argv[3], "rb");
    if (fp == NULL)
    {
        error("ERROR open file error");
    }
    int idx_next_rcv = 0;
    int head = 0;
    int buff_full = 0;
    int seqno = 0;
    char payload[256];
    unsigned int offset = 0;
    int done = 0;

//Used anytime in need a temp variable 
    struct PacketInfo i_pkt_info;
    struct MyPacket i_pkt;
    start_sending = clock();
    int i = 0;
	// send data in chunks of 256 bytes
    while(!done){
		//Do Stuff

      while(!buff_full && !feof(fp)) {

        bzero(buffer,256);

        int read = fread(buffer, 1, 256, fp);
        if(read < 0)
          error("Read In Error\n");

        pkt.offset = offset;
        pkt.length = read;
        offset += read;
        strncpy(payload, buffer, 256);

        pkt.type = 'P';
        pkt.seqno = (seqno++)% (WIN+1); //REMEMEBER THIS!!!!
        pkt.eof =feof(fp);
        
        strncpy(pkt.payload, payload, 256);

  	n = sendto(sockfd, &pkt, sizeof(struct MyPacket), 0, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
	start = clock();
        if(n==-1)
          error("Error writing to socket\n");
	//Create packet info
	struct PacketInfo temp = {};
        memcpy(&temp.p, &pkt, sizeof(struct MyPacket));
        temp.time_sent = start;
	//Save it to buffer and update buffer variables
  	pkt_buf[head++] = temp;
	head %= WIN;
	if(head  == idx_next_rcv)
	  buff_full = 1;

        printf("[send data] %d(%d)\n", pkt.offset, pkt.length);
      }
      i_pkt_info = pkt_buf[idx_next_rcv];
      if(((double) clock() - i_pkt_info.time_sent) / CLOCKS_PER_SEC > TIMEOUT) {
	
	//If timeout on first packet resend all in buffer
	int i;
	for(i = 0 ; i < WIN ; i++){
	  if(i == head && i != 0)
	    continue;
	  i_pkt_info = pkt_buf[(idx_next_rcv + i) %WIN];
	  i_pkt  = i_pkt_info.p;
          n = sendto(sockfd, &i_pkt, sizeof(struct MyPacket), 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
          if(n==-1)
            error("Error writing to socket\n");
	  i_pkt_info.time_sent = clock();
          printf("[resend data] %d(%d)\n", i_pkt.offset, i_pkt.length);
	}
      }

      bzero(buffer, 256);
      
      if(buff_full || feof(fp)){
	int serv_len = sizeof(serv_addr);
	double time_left = ((double)clock() - pkt_buf[idx_next_rcv].time_sent)/ CLOCKS_PER_SEC;
	timeout.tv_sec = (int)time_left;
	time_left -= timeout.tv_sec;
	timeout.tv_usec = (int)(time_left * 1000000); 
	int errorid = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (errorid < 0){
          printf("errorid: %d\n", errorid);
          error("ERROR timeout set\n");
        }
        n = recvfrom(sockfd, pkt_rcvd, sizeof(struct MyPacket), 0, (struct sockaddr*)&serv_addr,&serv_len);
	if(n>0) {
	  i_pkt_info = pkt_buf[idx_next_rcv];
	  i_pkt = i_pkt_info.p;
          if(pkt_rcvd->seqno == i_pkt.seqno && pkt_rcvd->type=='C'){
            printf("[recv ack] %d\n", pkt_rcvd->seqno);
            //Update packet buffer variables
	    idx_next_rcv++;
	    idx_next_rcv %= WIN;
	    buff_full = 0;
            if(feof(fp) && idx_next_rcv == head) done = 1;
  	  }
        }
      }
    }
    done_sending = clock();
    total_time = (double)(done_sending - start_sending)/ CLOCKS_PER_SEC; 
    printf("[completed]\n");
    printf("It took %f seconds to send %d bytes\n", total_time, offset);
    if(total_time == 0){
      printf("A bigger file is needed to compute throughput\n");	
    }else{ 
      printf("Throughput = %f bytes per second\n",(offset/total_time));
    }
    close(sockfd);
    return 0;
}
