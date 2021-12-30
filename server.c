#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#define MAXLINE 256
#define WINDOW_SIZE 8
#define TIME_OUT 100000

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
	- checksum:	Checksum value
 	- sqNo:		Every UDP packet will have a sequence number. Sequence numbers follow a circular manner and 0 based: 0, 1, ..., 2 * WINDOW_SIZE-1, 0, ...
	- is_ACKed: Specifying that whether this packet is ACKed by the reciever.
 	- timeout_time: Every UDP packet has its own sending time. This will be used for detecting whether there exists any timeout for given UDP packet.
 	- remained: It is used for detecting if message end has been recieved.
	*/

	char payload[9]; // 8 byte message + '\0'
	int checksum;
	int sqNo;
	int is_ACKed;
	struct timeval timeout_time;
	int remained;

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
	
	struct UDP_Datagram packets[256 / WINDOW_SIZE];
	struct UDP_Datagram ack_cache[256 / WINDOW_SIZE]; 
	int window_size; 
	int sequence_number; // starting sequence number
	int buffer_available;
	int pass;
	int cache_index;
};


void initialize_window(struct Window *window)
{
	
	memset(window, 0, sizeof(*window));
	window->window_size = WINDOW_SIZE;
	window->sequence_number = 0;
	window->pass = 0;
	window->buffer_available = WINDOW_SIZE;
	window->cache_index = 0;


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
	gettimeofday(&(packet->timeout_time), NULL);

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
	char **partitioned_message = (char **)calloc(32 ,sizeof(char*));
    
    for (int i = 0 ; i < 32; i++)
        partitioned_message[i] = (char*)calloc(8, sizeof(char));
    
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

	struct UDP_Datagram ack_cache[256 / WINDOW_SIZE]; 
	int cache_index = 0;
	memset(ack_cache, 0, sizeof(ack_cache));

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
        extra_messages[i] = (char*)calloc(256, sizeof(char));

	int num_extra_messages = 0;
	int process = 0;


	int start, end;
	int total_send_packets = 0;

	while(1)
	{


		/*
			
			# Definition of `if (sent_chunks)` block:
			----------------------------------------

			- If there are still packets (chunks) that need to be sent, they have the priority, send them first before taking a new input if exists.
		
		    
		    # Parameters:
		    -------------
		    - sent_chunks: It is a count parameter showing the remained number of chunks. Let's say total 4 packets
		 need to be sent. If 3 of them have already been sent, sent_chunks will be equal to 4 - 3 = 1.

		 	- sending_packet: Since packets needs to be sent to client, we need to encapsulate the message under UDP packet. The way, it is done
		 is calling `create_packet` utility function and passing its return packet as `sending_packet`.

		 	- current_packet_no: It is a circular sequence number for packets that has been created. 
		 		-> In this implementation, window size is 8. Then, packet numbers follow 0, 1, 2, 3, 4, 5, 6, 7, ..., 14, 15, 0, 1, 2 ... ordering.
			
			- num_extra_messages = It is the count that shows how many messages has been recieved while delivering the current packet.
				-> For example, Suppose we deliver the packages for message "I'm sleeping 4 hours for previous 3 days JUST BECAUSE OF NETWORK HOMEWORK!"
			and meanwhile the user candy_girl2003 types "What a nice day!", this lovely message needs to be appended to queue on the purpose of 
			delivering it after the current message delivered.


			
			# Side-Notes:
			-------------

			- Calculation of current chunk value: window object has 2 important members. These are `pass` and `window_size`. The calculation I used is
		as the following formula:

							current packet payload = chunks[current payload index] where
							current payload index = [window.pass * 2 * window.window_size + current_packet_no]

			Suppose that packets have been sent and we have following configuration:
			(-) denotes that ACK hasn't been recieved at the moment we took below snapshot.
			(+) denotes that ACK has    been recieved.


				14       15           0             1           2             3            4          5          6            7           8
				-----------------------------------------------------------------------------------------------------------------------------------
				|   +    |      +     |      +      |      -     |     -      |      -     |     -    |    -     |      -     |    -      |   ....
				------------------------------------------------------------------------------------------------------------------------------------
											        ^																				      ^
										      window sequence                                                                        current packet
											     number                                                                                 number
				
				# If we didn't apply the circular indexing, we would have window sequence number 17. But now it is 8 and we need to send
			the 24th chunk. Let's look at the formula again to see it gives the correct answer:
							
							window pass = 1 since window slided only 1 pass of the length 16 (2 * window size).
							current payload index = (1 * 2 * 8 + 8 = 24 (Correct) 

		*/
			if (sent_chunks == 0 && process <= num_extra_messages)
		{

			initialize_window(&window);
			total_send_packets = 0;
			
			if (process < num_extra_messages)
			{
				message = extra_messages[process++];

				chunks = partition_message(message);
				
				// Calculate the total number of chunks
				NUMBER_OF_CHUNKS = ((strlen(message)) / 8) + ((strlen(message)) % 8 != 0);
				sent_chunks = NUMBER_OF_CHUNKS;
				
				
			}
			
			else if (sent_chunks == 0 && process == num_extra_messages)
			{
				//printf("sent_chunksss: %d\n", sent_chunks);
				num_extra_messages = 0;
				process = 0;	
				NUMBER_OF_CHUNKS = 0;
			}

			
		}


		if (sent_chunks)
		{

			sent_chunks--;

			// -------------------------------------------Create the Packet--------------------------------------------//
				
				struct UDP_Datagram *sending_packet;
				
				sending_packet = create_packet(chunks[window.pass * 2 * window.window_size + current_packet_no], current_packet_no);
				sending_packet->remained = sent_chunks;
				
				


				//--------------------------------------Send the Packet--------------------------------------------//
				
				sendto(sockfd, (const struct UDP_Datagram*)sending_packet, sizeof(*sending_packet), MSG_CONFIRM, 
							   (const struct sockaddr *)client_address, 
							   sizeof(*client_address));

				if (strcmp(sending_packet->payload, "BYE\n") == 0)
				{
						break;
				}

				total_send_packets++;
				
				
				window.packets[window.pass * 2 *  window.window_size + current_packet_no] = *sending_packet;
				window.buffer_available--;
				current_packet_no = (current_packet_no + 1) % (2 * window.window_size);

		}

		

		

		num_events = poll(poll_fd, 2, 2000);

		int socket_check_point = poll_fd[0].revents & POLLIN;
		int stdin_check_point =  poll_fd[1].revents & POLLIN;


		// ---------------------------------------------Send Operations ---------------------------------------------------------//
		
		/*

			Description of `Send Operations` block:
			---------------------------------------
			
			- This block refers to the operations corresponding to sending messages from server to client.
			- When a user types a message via standard input 2 main cases can occur:

				(1)   If sender has a spot in its window buffer and there is no other packages sending from server to client at that moment, 
			take the input initialize the process and divide the message into data chunks each of which is at most 8 bytes long.  
					
					-> Why checking buffer? Because whenever an ACK message is taken from the client window buffer is relieved. If server sent
			8 packets (where 8 is the window size) and haven't taken any ACK from these messages we couldn't send the new message. 

				(2) In this case, all the new messages are appended into a queue named extra_messages[]. 


		*/
		if(stdin_check_point)
		{	
			// If buffer is not full, then create a packet and send it.
			if (window.buffer_available)
			{
	
				if (NUMBER_OF_CHUNKS == 0)
				{
					fgets(message, MAXLINE, stdin);
					// `chunks` is a string array containing the chunks in the message, partition_message divides the message into at most 8 bytes of chunks.
					chunks = partition_message(message);
					
					// Calculate the total number of chunks
					NUMBER_OF_CHUNKS = ((strlen(message)) / 8) + ((strlen(message)) % 8 != 0);
					printf("NUMBER_OF_CHUNKS: %d\n", NUMBER_OF_CHUNKS);
					sent_chunks = NUMBER_OF_CHUNKS;
					initialize_window(&window);
					current_packet_no = 0;
					
					struct UDP_Datagram *sending_packet;
					sending_packet = create_packet(chunks[window.pass * 2 * window.window_size + current_packet_no], current_packet_no);


					sending_packet->remained = sent_chunks - 1;
					window.packets[window.pass * 2 * window.window_size + current_packet_no] = *sending_packet;
				
				
				}
				
			}

			// If buffer is full, put the messages into a buffer
			else
			{

				fgets(message, MAXLINE, stdin);
				extra_messages[num_extra_messages++] = message;

			}

		}


		// ------------------------------------------Recieve Operations-----------------------------------------------------------//

		/*

			Description of `Receive Operations` block:
			------------------------------------------

			- This part of the code corresponds to all of the recieving operations of the chat application.
			- There are some control points, Let me describe them:
				(i)   First, checksun is controlled, if there is any corruption in the recieved data, message is asked to be resended.
				
				(ii)  Secondly, if message is recieved correctly then server send a ACK message to client.

				(iii) Furthermore, if the recieved UDP packet has ACK 1 in its is_ACKed field then Window sliding operation takes place.

			
			Side Notes:
			-----------
			- How window is slided? Let's take a look at it more closely. Suppose at the snapshot we have following configuration:

					13      14        15        0       1           2           3           4           5              6 
			--------------------------------------------------------------------------------------------------------------------
				+	|	+	|	 +    |   	+	|	+	|	  -		|	 +		|	 +		|	  -		|		-	   |
			--------------------------------------------------------------------------------------------------------------------
														^
												 window sequence
			     									 number		
		
			# When an ACK: 1 has been recieved, since window sequence number is pointing to point 1, it will slide whenever it encounters with
		a packet with has no ACK or recieved packets are finished, it stops sliding. After ACK 1 is recieved and sliding the window, 
		the configuration will be like:

					13      14        15        0       1           2           3           4           5              6 
			--------------------------------------------------------------------------------------------------------------------
				+	|	+	|	 +    |   	+	|	+	|	  +		|	 +		|	 +		|	  -		|		-	   |
			--------------------------------------------------------------------------------------------------------------------
																							^
												 									  window sequence
			     									 									  number		
		


		*/
		else if(socket_check_point)
		{
			int n, len;
			struct UDP_Datagram *receiving_packet;

			receiving_packet = (struct UDP_Datagram*) malloc(sizeof(struct UDP_Datagram));

			//printf("-RECIEVE-\n");

			len = sizeof(*client_address);

			n = recvfrom(sockfd, (struct UDP_Datagram *)receiving_packet, MAXLINE, MSG_WAITALL, 
								 (struct sockaddr *)client_address, &len);
			

			
			if (strcmp(receiving_packet->payload, "BYE\n") == 0)
			{
				break;
			}

			// Check if data is garbled
			int received_sqNo, recieved_checksum, packet_checksum;

			received_sqNo = receiving_packet->sqNo;

			recieved_checksum = receiving_packet->checksum;
			packet_checksum = calculate_checksum(receiving_packet);
			
			
			// Compare the checksum with the sent checksum;
			if (recieved_checksum != packet_checksum)
			{
				//fprintf(stderr, "%s\n", "Checksum Error: Packet hasn't been delivered correctly!\n");
				//printf("Waiting for Time out...\n");
			}

			
			// -----------------------------------Send ACK--------------------------------------//
			else if (recieved_checksum == packet_checksum && receiving_packet->is_ACKed == 0)
			{
				//printf("Sending ACK\n");
				
				if (window.sequence_number > receiving_packet->sqNo)
				{
					//printf("ACK has already been sent!, Resending again...\n");
					sendto(sockfd, (const struct UDP_Datagram*)receiving_packet, sizeof(*receiving_packet), MSG_CONFIRM, 
								   (const struct sockaddr *)client_address, 
								   sizeof(*client_address));

					
				}

				else
				{
					receiving_packet->is_ACKed = 1;
					ack_cache[received_sqNo] = *receiving_packet;


					//printf("\nPayload: %s\n", receiving_packet->payload);
					//printf("\nRemained: %d\n", receiving_packet->remained);
					while (ack_cache[cache_index].is_ACKed)
					{
						printf("%s", ack_cache[cache_index].payload);
						cache_index++;

					}




					sendto(sockfd, (const struct UDP_Datagram*)receiving_packet, sizeof(*receiving_packet), MSG_CONFIRM, 
								   (const struct sockaddr *)client_address, 
								   sizeof(*client_address));

				if (ack_cache[0].remained + 1== cache_index)
					{
						//printf("\nHereee\n");
						while (ack_cache[cache_index].is_ACKed)
						{
							printf("%s", ack_cache[cache_index].payload);
							cache_index++;
							
							if (cache_index == 2 * window.window_size)
							{
								//printf("Here\n");
								memset(ack_cache, 0, sizeof(ack_cache));
								cache_index = 0;
							}

						}
						
						cache_index = 0;
						memset(ack_cache, 0, sizeof(ack_cache));
						ack_cache[0].remained = 0;
						initialize_window(&window);
					}

			
					
					
					
				}
				
					
				
				
			}


			else if (recieved_checksum == packet_checksum && receiving_packet->is_ACKed)
			{
				
				if (ack_cache[window.pass* 2  * window.window_size + received_sqNo].is_ACKed)
				{
					//printf("Don't worry, I received it.\n");

				}

				else
				{
					total_send_packets--;
					window.packets[window.pass* 2 * window.window_size + received_sqNo] = *receiving_packet;
			
					//NUMBER_OF_CHUNKS--;


					// ------------------Sliding Window Operation ------------------------------------------//
					
					while (window.packets[window.pass* 2 * window.window_size + window.sequence_number].is_ACKed)
					{	
						window.sequence_number++;
						window.buffer_available++;
					
						if (window.sequence_number == 2 * window.window_size)
						{
							window.pass++;
							window.sequence_number = 0;
						} 
					}	
				}
				

			}

		

		}



		// -----------------------------------------------------Timeout-------------------------------------------------------//

	/*
		# Definition of `Timeout` block:
		--------------------------------
		- A UDP datagram has timeout_time field.
		
		-> UDP DATAGRAM  <-  
		___________________
		|		           | 
		|------------------|     # If a sent packet hasn't been ACKed yet, `timeout_time` value is used to detect this. If this is the case,
		|	timeout_time   |  send the packet again.
		|------------------|	 # ACK may be recieved after we resend the packet, in this case take the ACK, if window sequence is 
		|				   |  pointing to this place, slide the window. When same ACK came twice, do anything.
		|__________________|
		
	
	*/
		struct timeval current_time;

		gettimeofday(&current_time, NULL);

		long current_time_microsecond = current_time.tv_sec * 1000000 + current_time.tv_usec;
		long time_passed = current_time_microsecond - (window.packets[0].timeout_time.tv_sec * 1000000 + window.packets[0].timeout_time.tv_usec); 

		
		int temp_sent_chunks = total_send_packets;
		
		if (temp_sent_chunks && ack_cache[cache_index].is_ACKed == 0)
		{
			
			start = window.window_size * 2 * window.pass + window.sequence_number;
			end = start + total_send_packets;
			
			while (start < end)
			{	
				
				struct timeval current_time;

				gettimeofday(&current_time, NULL);

				long current_time_microsecond = current_time.tv_sec * 1000000 + current_time.tv_usec;
				long time_passed = current_time_microsecond - (window.packets[start].timeout_time.tv_sec * 1000000 + window.packets[start].timeout_time.tv_usec); 


				if (time_passed > TIME_OUT)
				{
					//printf("Timeout!.. Resending the packet no: %d\n", start - window.window_size * 2 * window.pass);
					struct UDP_Datagram *sending_packet;
					
					sending_packet = create_packet(window.packets[start].payload, window.packets[start].sqNo);
					sending_packet->remained = window.packets[start].remained;
					//printf("payload: %s\n", window.packets[start].payload);
					*sending_packet = window.packets[start];  

					sendto(sockfd, (const struct UDP_Datagram*)sending_packet, sizeof(*sending_packet), MSG_CONFIRM, 
									   (const struct sockaddr *)client_address, 
									   sizeof(*client_address));
					start++;


				}


				else
					break;


			}

		}
		if (ack_cache[0].remained + 1 == cache_index)
					{
						//printf("\nHereee\n");
						while (ack_cache[cache_index].is_ACKed)
						{
							printf("%s", ack_cache[cache_index].payload);
							cache_index++;
							
							if (cache_index == 2 * window.window_size)
							{
								//printf("Here\n");
								memset(ack_cache, 0, sizeof(ack_cache));
								cache_index = 0;
							}

						}
						
						cache_index = 0;
						memset(ack_cache, 0, sizeof(ack_cache));
						ack_cache[0].remained = 0;
						initialize_window(&window);
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

	printf("BIND: %d\n", SERVER_PORT);
	
	memset(&SERVER_ADDRESS, 0, sizeof(SERVER_ADDRESS));
    memset(&CLIENT_ADDRESS, 0, sizeof(CLIENT_ADDRESS));

	sockfd = preprocess_address(&SERVER_ADDRESS, SERVER_PORT);

	
	char buffer[2 * MAXLINE];
	int len = 0;

	reliable_data_transfer(sockfd, &CLIENT_ADDRESS, buffer, &len);

	close(sockfd);
	return 0;
}



// aaaaaaaabbbbbbbbccccccccddddddddaaaaaaaabbbbbbbbccccccccddddddddaaaaaaaabbbbbbbb
