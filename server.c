/*
- Server-side implementation of the chat application.
- Overall Description of the module:
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
	/*
	Function Description:
	---------------------

	- Creates a UDP socket so that we can communicate with client, recieve datagrams from there and send messages to client.
	- socket() function is used for socket creation.
	
	Args:
		(i)   AF_INET: It is used to specify IPv4 addressing.
		(ii)  SOCK_DGRAM: Specifies that it is a UDP socket.
		(iii) 0: ....
	


	*/
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if(sockfd < 0)
	{
		fprintf(stderr, "%s\n", "Socket couldn't be created!");
		exit(-1);
	}

	return sockfd;
}



int preprocess_address(struct sockaddr_in* server_address, int server_port)
{	

	/*
	Function Description:
	---------------------

	- It sets the configurations of server address object.
		(i)   AF_INET: Specifies that server address has IPv4 addressing
		(ii)  INADDR_ANY: In general, for a server, you typically want to bind to all interfaces - not just "localhost".
		(iii) Server Port is also given as a sin_port parameter.

	- Also, this function binds the socket to the server address.

	
	*/

	// -----------------------------Socket Creation--------------------------------------------------------//
	int sockfd = create_socket();

	server_address->sin_family = AF_INET;
	server_address->sin_addr.s_addr = INADDR_ANY;
	server_address->sin_port = htons(server_port);

	
	// -----------------------------Socket Binding--------------------------------------------------------//

	int socket_bind_check = bind(sockfd, (const struct sockaddr *) server_address, sizeof(*server_address));

	if (socket_bind_check < 0)
	{

		fprintf(stderr, "%s\n", "Socket couldn't be bound!");
		exit(-1);
	}

	return sockfd;
}

void recieve_packet(int sockfd, char* buff, struct sockaddr_in* client_address, int* len)
{
	/*
	Function Description:
	---------------------
	
	- It receives the packets from client. 

	*/

	int n = 0;

	*len = sizeof(*client_address);
	n = recvfrom(sockfd, buff, MAXLINE, MSG_WAITALL, (struct sockaddr*) client_address, len);
	
	buff[n] = '\0';

	return;
}

void send_packet(int sockfd, struct sockaddr_in* client_address, char* buff, int* len)
{

	/*
	Function Description:
	---------------------

	-  It sends messages to the client

	*/
	
	*len = sizeof(*client_address);
	sendto(sockfd, (const char*) buff, strlen(buff), MSG_CONFIRM, (const struct sockaddr *) client_address, *len);

	return;
}

void synchronize_messages(int sockfd, char buff[MAXLINE], struct sockaddr_in* addr, int* len)
{
	
	/*

	Function Description:
	---------------------

	- This is the mainstream function between server and client. Normally, Client and server can communicate by following
	an order fashion: Server/Client must wait Client/Server to send a message to them when they want to send new message. 

	- With this function chat application synchronizes the message sending operation by utilizing polling. 

	
	Approach:
	---------
	
	- Two polls are created:
		(1) First  one is for, sender-side: socket
		(2) Second one is for, listener-side: standard input
	
	Args to poll():

		(i)   poll_fd: poll array containing 2 polls
		(ii)  number_of_polls: In this implementation it is 2.
		(iii) sleep: OS interrupts some process, it is the wait time in microseconds.
	
	*/

	int num_events = -1;
	char temp = '\0';
	struct pollfd poll_fd[2];

	poll_fd[0].fd = sockfd;
	poll_fd[0].events = POLLIN;

	poll_fd[1].fd = STDIN_FILENO;
	poll_fd[1].events = POLLIN;

	while(1)
	{

		num_events = poll(poll_fd, 2, 2000);

		if(num_events == 0)
			continue;


		int socket_check_point = poll_fd[0].revents & POLLIN;
		int stdin_check_point = poll_fd[1].revents & POLLIN;

		
		if(stdin_check_point)
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

		else if(socket_check_point)
		{
			recieve_packet(sockfd, buff, addr, len);
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

int main(int argc, char *argv[])
{
	int sockfd, SERVER_PORT;
	struct sockaddr_in SERVER_ADDRESS, CLIENT_ADDRESS;
	char *SERVER_PORT_STRING = argv[1];

	SERVER_PORT = atoi(SERVER_PORT_STRING);

	printf("bind_port: %d\n", SERVER_PORT);
	
	memset(&SERVER_ADDRESS, 0, sizeof(SERVER_ADDRESS));
    memset(&CLIENT_ADDRESS, 0, sizeof(CLIENT_ADDRESS));

	sockfd = preprocess_address(&SERVER_ADDRESS, SERVER_PORT);


	char in_buff[MAXLINE], out_buff[] = "Ben bir cengciyim.";
	int len = 0;

	
	recieve_packet(sockfd, in_buff, &CLIENT_ADDRESS, &len);
	printf("Server message received: %s\n", in_buff);

	send_packet(sockfd, out_buff, &CLIENT_ADDRESS, &len);
	printf("Server message sent: %s\n", out_buff);
	
	synchronize_messages(sockfd, &CLIENT_ADDRESS, in_buff, &len);

	close(sockfd);
	return 0;
}
