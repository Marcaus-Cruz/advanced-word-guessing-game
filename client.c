/* client.c - code for client. Do not rename this file */

#include <stdio.h>
#include "trie.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

/* helper that takes in number of bytes from a recv and the socket
	if a client has disconnected, we end the game and close both sockets */
void checkDC(int n, int sd){
        if(n == 0){
                fprintf(stderr, "Lost connection, ended game\n");
                close(sd);
                exit(0);
        }
}

int main( int argc, char **argv) {

	struct hostent* ptrh; 	/* pointer to a host table entry */
	struct protoent* ptrp; 	/* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; 		/* socket descriptor */
	int port; 		/* protocol port number */
	char* host; 		/* pointer to host name */
	char buf[1000]; 	/* buffer for data from the server */
	uint8_t ack = 1;	/* acknowledgement value */
	uint8_t playerNum;	/* this client's player number */
	uint8_t boardSize;	/* size of board from server */
	uint8_t sec;		/* seconds per round from server */
	uint8_t score1;		/* Player 1 score */
	uint8_t score2;		/* Player 2 score */
	uint8_t round;		/* round number */
	char active;		/* indicates if this player is active or not */
	int guessLen;		/* length of guess */
	uint8_t check;		/* value for correct/incorrect guess */
	time_t startTime;	/* to handle timeout on client side */
	time_t endTime;
	int n;				/* to check for disconections from across server */

	memset((char*)&sad, 0, sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	//input checking
	if (argc != 3) {
		fprintf(stderr, "Error: Wrong number of arguments\n");
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) /* test for legal value */
		sad.sin_port = htons((u_short)port);
	else {
		fprintf(stderr, "Error: bad port number %s\n", argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if (ptrh == NULL) {
		fprintf(stderr, "Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if (((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr*) & sad, sizeof(sad)) < 0) {
		fprintf(stderr, "connect failed\n");
		exit(EXIT_FAILURE);
	}

	//get player number
	n = recv(sd, buf, sizeof(uint8_t), 0);
	checkDC(n, sd);
	send(sd, &ack, sizeof(uint8_t), 0);

	playerNum = (uint8_t) buf[0];
	printf("You are player %d! \n", playerNum);

	//get boardSize
	n = recv(sd, buf, sizeof(uint8_t), 0);
	checkDC(n, sd);
	send(sd, &ack, sizeof(uint8_t), 0);

	boardSize = (uint8_t) buf[0];
	char board[boardSize];		/* to print board */
	char guess[boardSize];		/* buffer for user input */

	printf("boardSize = %d \n", boardSize);

	//get number of seconds per round
	n = recv(sd, buf, sizeof(uint8_t), 0);
	checkDC(n, sd);
	send(sd, &ack, sizeof(uint8_t), 0);

	sec = (uint8_t) buf[0];

	printf("Number of seconds per round: %d \n", sec);
	printf("Waiting for opponent to connect...\n");

	while(1){

		//get player1 score
		n = recv(sd, buf, sizeof(uint8_t), 0);
		checkDC(n, sd);
		send(sd, &ack, sizeof(uint8_t), 0);
		score1 = (uint8_t) buf[0];

		//get player2 score
		n = recv(sd, buf, sizeof(uint8_t), 0);
		checkDC(n, sd);
		send(sd, &ack, sizeof(uint8_t), 0);
		score2 = (uint8_t) buf[0];

		/* win conditions */
		if(score1 == 3 && playerNum == 1){
			printf("You have won!! \n");
			break;
		} else if(score2 == 3 && playerNum == 1){
			printf("You took an L \n");
			break;
		} else if(score1 == 3 && playerNum == 2){
			printf("You took an L \n");
			break;
		} else if(score2 == 3 && playerNum == 2){
			printf("You have won!! \n");
			break;
		}


		//get round Number
		n = recv(sd, buf, sizeof(uint8_t), 0);
		checkDC(n, sd);
		send(sd, &ack, sizeof(uint8_t), 0);
		round = (uint8_t) buf[0];

		// Print info on the current round
		printf("Round: %d \n", round);
		printf("Player 1 Score: %d	Player 2 Score: %d\n", score1, score2);

		n = recv(sd, buf, sizeof(buf), 0);
		checkDC(n, sd);

		send(sd, &ack, sizeof(uint8_t), 0);

		for(int i = 0; i < boardSize; i++){
			board[i] = buf[i];
		}
		board[boardSize] = '\0';

		/* print board */
		printf("Board: ");
		for(int i = 0; i < boardSize; i++){
			printf("%c ", board[i]);
		}
		printf("\n");

		while(1){
			/* get Y or N from server */
			n = recv(sd, buf, sizeof(char), 0);
			checkDC(n, sd);
			send(sd, &ack, sizeof(uint8_t), 0);
			active =(char)  buf[0];

			if(active == 'Y'){

				fprintf(stderr, "Enter your guess (only use letters on board): ");

				startTime = time(0);
				fgets(guess, 1000, stdin);
				endTime = time(0);

				/* Timeout checking */
				if (endTime < startTime + sec)
					send(sd, &guess, strlen(guess) - 1, 0);
				else
					printf("You ran out of time!");
				
				/* receive if guess was correct or not */
				n = recv(sd, buf, sizeof(uint8_t), 0);
				checkDC(n, sd);
				send(sd, &ack, sizeof(uint8_t), 0);

				if ((uint8_t)buf[0] == 1){
					printf("Correct!\n");
				}
				else {
					printf("Incorrect!\n");
					break;
				}


			} 
			/* This client is not active */
			else{

				printf("Opponent is guessing... \n");

				n = recv(sd, buf, sizeof(uint8_t), 0);
				checkDC(n, sd);
				send(sd, &ack, sizeof(uint8_t), 0);

				check = (uint8_t)buf[0];	/* correct guess or not */

				/* receive opponent's guess */
				guessLen = recv(sd, buf, sizeof(buf), 0);
				send(sd, &ack, sizeof(uint8_t), 0);

				for(int i = 0; i < guessLen; i++){
					guess[i] = buf[i];
				}
				guess[guessLen] = '\0';

				if(playerNum == 1){
					if (check)
						printf("Player 2 has guessed: %s, and it was correct!\n", guess);
					else{
						printf("Player 2 has guessed: %s, and they took an L!\n", guess);
						break;
					}
				} else {
					if (check)
						printf("Player 1 has guessed: %s, and it was correct!\n", guess);
					else {
						printf("Player 1 has guessed: %s, and they took an L!\n", guess);
						break;
					}
				}
			}
		}

	}

	close(sd);

	exit(EXIT_SUCCESS);

}
