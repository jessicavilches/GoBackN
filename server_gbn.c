/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define WIN 5
struct MyPacket		//Design your packet format (fields and possible values)
{
    char type;
    unsigned short seqno;
    unsigned int offset;
    int length;
    int eof;
    char payload[256];
};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char recv_buf[512];
    char sent_msg[] = "ACK";
	  struct MyPacket sent_pkt = {'C', 0, 0, 0, 0, ""};
    int recvlen, payloadlen;
	  int LAS = -1;

	struct MyPacket *pkt = malloc(sizeof(struct MyPacket));

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t addrlen = sizeof(cli_addr);
    int n;
    if (argc != 2) {
       fprintf(stderr,"usage %s port\n", argv[0]);
       exit(0);
    }

    /* creat upd socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
       error("ERROR opening socket");

	/* bind the socket */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");

/*
	listen(sockfd,5);
	clilen = sizeof(cli_addr);
	newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
*/
	printf("Waiting on port %d\n", portno);


	/* create file for writing*/
	FILE *fp;
	fp = fopen("README-recv", "ab");
	if (NULL == fp)
		error("ERROR opening file");

  int eof = 0;
	// receive data in chunks of 256 bytes
	while(!eof)
	{
    //Do Stuff
    bzero(recv_buf, 512);
   // n = recv(sockfd, recv_buf, 512, 0);
	n = recvfrom(sockfd, pkt, sizeof(*pkt), 0, (struct sockaddr *) &cli_addr, &clilen);    

        if(n==-1)
      error("Error reading from socket\n");

   // strncpy((char *)pkt, recv_buf, sizeof(struct MyPacket));

    if(n>0) {
      if(pkt ->seqno == ((LAS + 1)%(WIN+1)) && pkt->type =='P'){
        fprintf(fp, "%s", pkt->payload);
        LAS = pkt->seqno;
        printf("[recv data] %d(%d) ACCEPTED\n", pkt->offset, pkt->length);
        eof = pkt->eof;
      } else if( pkt ->seqno != ((LAS +1) %(WIN +1)) && pkt->type =='P'){
        printf("[recv data] %d(%d) IGNORED\n", pkt->offset, pkt->length);
      }
      if(pkt->type !='P')
	continue; 
      sent_pkt.seqno = pkt->seqno;
      n = sendto(sockfd, &sent_pkt, sizeof(struct MyPacket), 0,(struct sockaddr*) &cli_addr, clilen);
      if(n==-1)
        error("Error writing to socket\n");
    }
  }

  fclose(fp);
  close(sockfd);
  printf("[completed]\n");
  return 0;

}

