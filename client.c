/*
- Client-side implementation of the chat applicaiton.
- Overal description of the module:
				.
				.
				.
				.
				.
				.
				.

*/



// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// Socket based libraries 
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAXLINE 1024 // one line contains at most 256 characters 
int create_socket()
{

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if(sockfd < 0)
		printf("A problem occured initializing socket.\n");

	return sockfd;
}


void bind_socket(int sockfd, struct sockaddr_in* addr)
{
	int flag = bind(sockfd, (const struct sockaddr *) addr, sizeof(*addr));

	if (flag < 0)
		printf("A problem occured at bind_socket.\n");

	return;
}

int init_comm_socket(int port, struct sockaddr_in* addr)
{

	if(!addr)
	{
		printf("parameter addr is NULL.\n");
		return -1;
	}

	int sockfd = create_socket();

	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;
	addr->sin_port = htons(port);

	bind_socket(sockfd, addr);

	return sockfd;
}

void recv_packet(int sockfd, char* buff, struct sockaddr_in* addr, int* len)
{
	
	if(!addr)
	{
		printf("parameter addr is NULL.\n");
		return;
	}

	int n = 0;

	*len = sizeof(*addr);
	n = recvfrom(sockfd, buff, MAXLINE, MSG_WAITALL, (struct sockaddr*) addr, len);
	
	buff[n] = '\0';

	return;
}

void send_packet(int sockfd, char* buff, struct sockaddr_in* addr, int* len)
{

	if(!addr)
	{
		printf("parameter addr is NULL.\n");
		return;
	}

	*len = sizeof(*addr);
	sendto(sockfd, (const char*) buff, strlen(buff), MSG_CONFIRM, 
		   (const struct sockaddr *) addr, *len);

	return;
}

void read_text(int sockfd, char buff[MAXLINE], struct sockaddr_in* addr, int* len)
{
	int num_events = -1;
	char temp = '\0';
	struct pollfd pfds[2];

	pfds[0].fd = sockfd;
	pfds[0].events = POLLIN;

	pfds[1].fd = STDIN_FILENO;
	pfds[1].events = POLLIN;

	while(1)
	{

		num_events = poll(pfds, 2, 2000);

		if(num_events == 0)
			continue;


		int sock_flag = pfds[0].revents & POLLIN;
		int stdio_flag = pfds[1].revents & POLLIN;

		
		if(stdio_flag)
		{	
			fgets(buff, MAXLINE, stdin);
			send_packet(sockfd, buff, addr, len);
			printf("Client message sent: %s\n", buff);
			

			temp = buff[4];
			buff[4] = '\0';
			if(!strcmp(buff, "BYE"))
				break;
			buff[4] = temp;
		}

		else if(sock_flag)
		{
			recv_packet(sockfd, buff, addr, len);
			printf("Client message received: %s\n", buff);
			
			temp = buff[4];
			buff[4] = '\0';
			if(!strcmp(buff, "BYE"))
				break;
			buff[4] = temp;
		}
	}

	return;
}

int main(int argc, char* argv[])
{
	int sockfd, port;
	struct sockaddr_in servaddr, cliaddr;
	char *ip_str = argv[1];
	int send_port = atoi(argv[2]), bind_port = atoi(argv[3]);

	printf("send_port: %d, bind_port: %d, ip_str: %s\n", send_port, bind_port, ip_str);

	memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    sockfd = init_comm_socket(bind_port, &cliaddr);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(send_port);

    char in_buff[MAXLINE], out_buff[MAXLINE] = "Ben bir eşşeğim.";
	int len = 0;

	
	send_packet(sockfd, out_buff, &servaddr, &len);
	printf("Client message sent: %s\n", out_buff);

	recv_packet(sockfd, in_buff, &servaddr, &len);
	printf("Client message received: %s\n", in_buff);
	
	read_text(sockfd, in_buff, &servaddr, &len);

	close(sockfd);
	return 0;
}
