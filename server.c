#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAXLINE 1024
#define WINDOW_SIZE 8
//--------------------------------Utility functions for sending and receiving messages------------------------------------------ // 

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

void synchronize_messages(int sockfd,  struct sockaddr_in* addr, char buff[MAXLINE], int* len)
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
		{
			printf("Poll timed out\n");
			continue;
		}


		int socket_check_point = poll_fd[0].revents & POLLIN;
		int stdin_check_point = poll_fd[1].revents & POLLIN;

		
		if(stdin_check_point)
		{	
			fgets(buff, MAXLINE, stdin);
			send_packet(sockfd, addr, buff, len);
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



// -------------------------------------------------Reliable Data Transfer--------------------------------------------------------//




struct UDP_Datagram	
{
	/*
	
	Struct Description:
	-------------------

	- This is the conceptual definition of UDP datagram. When a chunk of message is recieved, UDP protocol will add some additional information
	to the chunk of message. These aditional informations are given in members.

	
	Members:
	--------
	
	- payload:  8 byte message.
	- checksum:	
 	- sqNo:		Every UDP packet will have a sequence number. Sequence numbers follow a circular manner and 0 based: 0, 1, ..., WINDOW_SIZE-1, 0, 1, ...
	- is_ACKed: Specifying that whether this packet is ACKed by the reciever.
 	
	*/

	char payload[9]; // 8 byte message + '\0'
	int checksum;
	int sqNo;
	int is_ACKed;


};

struct Window
{	
	/*
	
	Struct Description:
	-------------------
	
	- Selective Repeat uses sliding window operation, we need a 
	window object.
	
	
	Members:
	--------
	
	- window_size: WINDOW_SIZE
	- sequence_number: It is the starting sequence number of the window. Since we will slide the window it needs to be kept.
	- buffer_available: number of spots available in the Window buffer.
	- packets: It is the buffer of the Window. It is declared as 
	- pass: It specifies how many full pass has occured so far. Let's give an example:
			
			# Suppose that window_size = 8.
			# Also suppose that maximum input line = 256 byte.
			# Then it will create a UPD_Datagram array of size 256 / 8 = 32. BUT this is not buffer size. This is just for physical design of 
		buffer. Instead of using a queue structure, I merged all of them. See below for visual explanation:

										________________Window Size_____________

										0       1       2       3              7        0        1            2
										-----------------------------------------------------------------------	
										| 		|		| 	    |     .....    |        |        |            |		-> Total 32 byte										  
										-----------------------------------------------------------------------	
										^                                      ^
								   sequence_start	                      sequence_end    

			# Assume that, for the first packet 0, ACK is recieved. Then, window will be slided and will have the form:

												________________Window Size_____________

										0       1       2       3              7        0        1            2
										-----------------------------------------------------------------------	
										| 	+	|		| 	    |     .....    |        |        |            |		-> Total 32 byte										  
										-----------------------------------------------------------------------	
												^                                       ^
								   			sequence_start	                        sequence_end  
							
	*/				
	
	int window_size; 
	int sequence_number; // starting sequence number
	int buffer_available;
	int pass;
	struct UDP_Datagram packets[256 / WINDOW_SIZE];
};


void initialize_window(struct Window *window)
{
	
	memset(window, 0, sizeof(*window));
	window->window_size = WINDOW_SIZE;
	window->sequence_number = 0;
	window->pass = 0;
	window->buffer_available = WINDOW_SIZE;


	return;
}


int calculate_checksum(struct UDP_Datagram *packet)
{
	
	int checksum = 0;

	checksum += packet->sqNo;

	for (int i = 0; i < 8; i++)
		checksum += packet->payload[i];

	return checksum;
}


struct UDP_Datagram* create_packet(char *partitioned_message, int sqNo)
{
	
	struct UDP_Datagram *packet; 
	packet = (struct UDP_Datagram*) malloc(sizeof(struct UDP_Datagram));

	memset(packet, 0, sizeof(struct UDP_Datagram));

	packet->sqNo = sqNo;
	packet->is_ACKed = 0;

	strcpy(packet->payload, partitioned_message);
	packet->payload[8] = '\0';
	packet->checksum = calculate_checksum(packet);

	return packet;
}


char **partition_message(char* message)
{
	/*
	Function Description:
	---------------------



	Returns:
	--------
		
	- A String array, containing the chunks corresponding the message

	*/
	char **partitioned_message = (char **)malloc(32 * sizeof(char*));
    
    for (int i = 0 ; i < 32; i++)
        partitioned_message[i] = (char*)calloc(9, sizeof(char));
    
    int len = strlen(message);
    
    int k = -1, t = 0;

    for (int i = 0; i < len; i++)
    {
    	if (i % 8 == 0)
    	{
    		k++;
    		t = 0;
    	}
		
		partitioned_message[k][t++] = message[i];    	
    }

    return partitioned_message; 
}



void reliable_data_transfer(int sockfd, struct sockaddr_in* client_address, char* message, int* len)
{

	/*
	
	Function Definition:
	--------------------

	- This function implements the reliable data transfer protocol using UDP Datagrams and Selective Repeat Protocol.
	- ....
	- .... 

	Steps:
		1) Given a message, read it in chunks, make these chunks a packet.


	*/


	int NUMBER_OF_CHUNKS = 0;
	char **chunks;

	// Create a window object
	struct Window window;
	initialize_window(&window);

	int current_packet_no = 0;

	// Polling Declarations
	int num_events;
	struct pollfd poll_fd[2];

	poll_fd[0].fd = sockfd;
	poll_fd[0].events = POLLIN;

	poll_fd[1].fd = STDIN_FILENO;
	poll_fd[1].events = POLLIN;

	int sent_chunks = 0;
	while(1)
	{

		if (sent_chunks)
		{

			sent_chunks--;	
			// --------------------Create the Packet--------------------------------------------//
				struct UDP_Datagram *sending_packet;
				
				printf("Payload: %s\n", chunks[window.pass * window.window_size + current_packet_no]);

				sending_packet = create_packet(chunks[window.pass * window.window_size + current_packet_no], current_packet_no);
				
				printf("Packet is created...\n");
				
				printf("Packet no: %d\n", current_packet_no);

				//----------------------Send the Packet--------------------------------------------//
				sendto(sockfd, (const struct UDP_Datagram*)sending_packet, sizeof(*sending_packet), MSG_CONFIRM, 
							   (const struct sockaddr *)client_address, 
							   sizeof(*client_address));
				
				printf("Packet has been send successfuly!\n");
				

				window.packets[window.pass * window.window_size + current_packet_no] = *sending_packet;
				window.buffer_available--;
				printf("Buffer Size: %d\n", window.buffer_available);
				current_packet_no = (current_packet_no + 1) % window.window_size;

				printf("---------------------------\n");
		}



		num_events = poll(poll_fd, 2, 2000);

		if (num_events == 0)
			continue;

		int socket_check_point = poll_fd[0].revents & POLLIN;
		int stdin_check_point =  poll_fd[1].revents & POLLIN;


		// -------------------------------------Send Operations ------------------------------------//
		if(stdin_check_point)
		{	
			// If buffer is not full, then create a packet and send it.
			if (window.buffer_available)
			{	

				printf("-SEND-\n");
				if (NUMBER_OF_CHUNKS == 0)
				{
					fgets(message, MAXLINE, stdin);
					// `chunks` is a string array containing the chunks in the message, partition_message divides the message into at most 8 bytes of chunks.
					chunks = partition_message(message);
					
					// Calculate the total number of chunks
					NUMBER_OF_CHUNKS = ((strlen(message) - 1) / 8) + ((strlen(message) -1) % 8 != 0);
					sent_chunks = NUMBER_OF_CHUNKS;
					printf("Number of chunks: %d\n", NUMBER_OF_CHUNKS);
				
				}
				
			}

			// TODO: If buffer is full, wait for ACKs


		}



		// --------------------------------Recieve Operations----------------------------------------//
		else if(socket_check_point)
		{
			int n, len;
			struct UDP_Datagram *receiving_packet;

			receiving_packet = (struct UDP_Datagram*) malloc(sizeof(struct UDP_Datagram));

			printf("-RECIEVE-\n");

			len = sizeof(*client_address);

			n = recvfrom(sockfd, (struct UDP_Datagram *)receiving_packet, MAXLINE, MSG_WAITALL, 
								 (struct sockaddr *)client_address, &len);
			
			printf("Packet has been recieved!\n");


			printf("payload: %s\n", receiving_packet->payload);

			// Check if data is garbled
			int received_sqNo, recieved_checksum, packet_checksum;

			received_sqNo = receiving_packet->sqNo;

			printf("received_sqNo: %d\n", received_sqNo);

			recieved_checksum = receiving_packet->checksum;
			packet_checksum = calculate_checksum(receiving_packet);

			printf("recieved_checksum: %d packet_checksum: %d\n", recieved_checksum, packet_checksum);
			
			// Compare the checksum with the sent checksum;
			if (recieved_checksum != packet_checksum)
			{
				fprintf(stderr, "%s\n", "Checksum Error: Packet hasn't been delivered correctly!\n");

				// --------------------Create the Packet--------------------------------------------//
				struct UDP_Datagram *resending_packet;
				*resending_packet = window.packets[window.pass * window.window_size + received_sqNo];				

				//----------------------Send the Packet--------------------------------------------//
				sendto(sockfd, (const struct UDP_Datagram*)resending_packet, sizeof(*resending_packet), MSG_CONFIRM, 
							   (const struct sockaddr *)client_address, 
							   sizeof(*client_address));
					
			}

			
			// -----------------------------------Send ACK--------------------------------------//
			if (recieved_checksum == packet_checksum && receiving_packet->is_ACKed == 0)
			{
				printf("ACK has been sent\n");

				receiving_packet->is_ACKed = 1;

				sendto(sockfd, (const struct UDP_Datagram*)receiving_packet, sizeof(*receiving_packet), MSG_CONFIRM, 
							   (const struct sockaddr *)client_address, 
							   sizeof(*client_address));
			}


			else if (receiving_packet->is_ACKed)
			{
				
				printf("Packet Delivered Correctly!\n");

				window.packets[window.pass * window.window_size + received_sqNo] = *receiving_packet;
				window.buffer_available++;
				NUMBER_OF_CHUNKS--;

				printf("Buffer Available: %d\n", window.buffer_available);

				// ------------------Sliding Window Operation ------------------------------------------//
				
				while (window.packets[window.pass * window.window_size + window.sequence_number].is_ACKed)
				{	
					
					window.sequence_number++;
					
					if (window.sequence_number == window.window_size)
					{
						window.pass++;
						window.sequence_number = 0;
						break;
					} 
				}

				printf("Sliding Window sequence_number: %d\n", window.sequence_number);
			}

			printf("---------------------------\n");
		
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

	send_packet(sockfd, &CLIENT_ADDRESS, out_buff, &len);
	printf("Server message sent: %s\n", out_buff);
	
	reliable_data_transfer(sockfd, &CLIENT_ADDRESS, in_buff, &len);

	close(sockfd);
	return 0;
}



// aaaaaaaabbbbbbbbaaaaaaaabbbbbbbbaaaaaaaabbbbbbbbaaaaaaaabbbbbbbbaaaaaaaabbbbbbbb
