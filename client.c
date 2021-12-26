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
	int sockfd;
	/* I will create the socket based on the following configuration:
	   
	   1) IP address will be of type IPv4. To do this, we will specify the
	   first argument as AF_INET.

	   2) Application layer packets will be delivered via UDP datagrams. To specify
	   UDP as an underlying transport layer, we will give the second parameter as 
	   SOCK_DGRAM.

	   3) Default protocol will be used.
	*/ 

	// Create the socket file descriptor
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (sockfd < 0)
	{
		fprintf(stderr, "%s\n", "An error has occured while creating a socket!");
		exit(1);
	}

	return sockfd;
}

void establish_connection(int socket, struct sockaddr_in *client_address)
{
	/*
		Definition:
		- This function will bind the server IPv4 address to socket.
		
		Args: 
		- socket: The socket we have already created -> socketfd.
		- server_address: Pointer to server_address object.
		

		-> Both server address and client address will be bound to given socket. This means that,
		The socket will send the packet from server to client (server address takes place here) and
		receive message from client to server (client address takes place here).
	
	*/

	int bind_check = bind(socket, (const struct sockaddr *) client_address, sizeof(*client_address));

	// Check whether binding operations occurred with no error
	if (bind_check < 0)
	{
		fprintf(stderr, "%s\n", "Binding Error has occured.");
		exit(1);
	}

	printf("Establish Connection: Successful!\n");
	
	return;

}

void process_address(struct sockaddr_in *server_address, struct sockaddr_in *client_address, int server_port_number)
{
	/*

		Definition:

		- This function will initialize the IPv4 adresses of server and client as 0.
		- In addition to that, it will fill the information about server side.
			+ This informations includes:
			
			(i)   Family of the Server Adress: Specifying whether it is IPv4 or IPv6. In this implementation I will strictly use IPv4.
			(ii)  In most of the cases, we want to bind all interfaces but in this assignment we are already given the IP address of the server address.
			(iii) ...

	*/

	memset(server_address, 0, sizeof(server_address));
	memset(client_address, 0, sizeof(client_address));


	// Fill the informations for server address
	server_address->sin_family = AF_INET;
	server_address->sin_addr.s_addr = INADDR_ANY;
	server_address->sin_port = htons(server_port_number);	

	return;
}


void recieve_packet(int socket, struct sockaddr_in *server_address, char* buffer, char* message)
{	

	/*
	
		Definition:
		- This function is responsible for receiving messages sent by client and sending the message created by server side of the 
		chat application.
		
		Args:
		- socket: All of the message transmissions between client and server will take place over this socket.
		- client address: Message will be sent to this IP address.
		- buffer:  Buffer will contain the message sent by client.
		- message: The message that will be sent to client by server.


	*/
	int len, n;
	
	// In order to send and recieve message in dynamic manner, we take the actual code inside a infinite loop.
	while (1)
	{
		len = sizeof(*server_address);  
   
	    sendto(socket, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *) server_address, len);

	    printf("Message sent.\n");	

	    n = recvfrom(socket, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) server_address, &len);
	    buffer[n] = '\0';

	    printf("Server : %s\n", buffer);
	    
	  
	}
   
    return;
}



int main(int argc, char* argv[])
{
	int socketfd;
	char buffer[MAXLINE];
	struct sockaddr_in SERVER_ADDRESS, CLIENT_ADDRESS;
	char *IP_TO_BE_CONNECTED = argv[1];
	int SERVER_PORT, CLIENT_PORT;

	char* message = "senin yengen\n";

	if (argc != 4)
	{
		fprintf(stderr, "Expected 2 arguments, received [argc] argument(s).");
		exit(1);
	}

	SERVER_PORT = atoi(argv[2]);
	CLIENT_PORT = atoi(argv[3]);

	
	socketfd = create_socket();
	
	process_address(&SERVER_ADDRESS, &CLIENT_ADDRESS, SERVER_PORT);""

	establish_connection(socketfd, &CLIENT_ADDRESS);

	recieve_packet(socketfd, &SERVER_ADDRESS, buffer, message);

	close(socketfd);


}
