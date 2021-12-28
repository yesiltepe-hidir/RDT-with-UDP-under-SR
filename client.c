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

	char **extra_messages = (char **)calloc(20, sizeof(char*));
    
    for (int i = 0 ; i < 20; i++)
        extra_messages[i] = (char*)calloc(9, sizeof(char));
	
	int num_extra_messages = 0;
	int process = 0;

	while(1)
	{

		if (sent_chunks)
		{
			printf("sent_chunks\n");

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

		
		if (sent_chunks == 0 && process <= num_extra_messages)
		{
				

			if (process < num_extra_messages)
			{

				printf("num_extra_messages\n");
				message = extra_messages[process++];

				chunks = partition_message(message);
				
				// Calculate the total number of chunks
				NUMBER_OF_CHUNKS = ((strlen(message) - 1) / 8) + ((strlen(message) -1) % 8 != 0);
				sent_chunks = NUMBER_OF_CHUNKS;

				
				printf("Number of chunks: %d\n", NUMBER_OF_CHUNKS);
				printf("sent_chunks: %d\n", sent_chunks);
				
			}

			else if (process == num_extra_messages)
			{
				
				num_extra_messages = 0;
				process = 0;	
			}

			
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
				printf("NUMBER_OF_CHUNKS: %d\n", NUMBER_OF_CHUNKS);

				if (NUMBER_OF_CHUNKS == 0)
				{
					fgets(message, MAXLINE, stdin);
					// `chunks` is a string array containing the chunks in the message, partition_message divides the message into at most 8 bytes of chunks.
					chunks = partition_message(message);
					
					// Calculate the total number of chunks
					NUMBER_OF_CHUNKS = ((strlen(message) - 1) / 8) + ((strlen(message) -1) % 8 != 0);
					sent_chunks = NUMBER_OF_CHUNKS;
					initialize_window(&window);
					current_packet_no = 0;
					
					printf("window sequence_number: %d\n", window.sequence_number);
					printf("Number of chunks: %d\n", NUMBER_OF_CHUNKS);
				
				}
				
			}

			// If buffer is full, put the messages into a buffer
			else
			{

				fgets(message, MAXLINE, stdin);
				extra_messages[num_extra_messages++] = message;

			}

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
	
	reliable_data_transfer(sockfd, &servaddr ,in_buff, &len);

	close(sockfd);
	return 0;
}
