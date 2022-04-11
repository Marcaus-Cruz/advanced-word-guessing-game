/* server.c - code for server. Do not rename this file */

#include <stdio.h>
#include "trie.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define QLEN 6

/* helper that takes in number of bytes from a recv and the clients socket descriptors
	if a client has disconnected, we end the game and close both sockets */
void checkDC(int n, int sd2, int sd3){
	if(n == 0){
		fprintf(stderr, "Lost connection, ended game\n");	
		close(sd2);
		close(sd3);
		exit(-1);
	}
}

int main( int argc, char **argv) {

	struct protoent* ptrp; 	/* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	struct sockaddr_in cad; /* structure to hold client's address */
	int sd, sd2, sd3; 		/* socket descriptors  -- sd2 = player 1 and sd3 = player 2 */
	int port; 		/* protocol port number */
	int alen; 		/* length of address */
	int optval = 1; 	/* boolean value when we set socket option */
	char buf[1000]; 	/* buffer for string the server sends */
	struct Trie* head;	/* Dictionary root node */
	char line[1000];	/* Buffer used for client input */
	char *token;	/* string used for creating dictionary */	
	int boardSize;	/* board size set by server */
	int sec;		/* number of seconds per round set by server */
	int pid;		/* process ID */
	int game = 1;	/* condition for playing the game */
	int activePlayer;	/* indicates active player for server */
	uint8_t round;		/* round number to be sent to clients */
	uint8_t score1;		/* score of player 1 */
	uint8_t score2;		/* score of player 2 */
	char letter;		/* used to create random letters on board */
	uint8_t one = 1;	/* sent to player 1 to assign it's number */
	uint8_t two = 2;	/* sent to player 2 to assign it's number */
	uint8_t zero = 0;	/* sent across socket for incorrect guesses */
	int guessLen;		/* length of client input  */
	trieNode* guesses;	/* trie to keep track of client guesses per round */
	char active = 'Y';		/* sent to players to indicate who's to guess and who's to wait */
	char inactive = 'N';	/* sent to players to indicate who's to guess and who's to wait */
	int m = 0;			/* index for double char array of guesses by users for a round (used to free guesses trie) */
	int check;			/* used for validating guesses by user */
	int n;				/* number of bytes from recv, used for timeouts */

	srand(time(0));		/* To create random board */
	struct timeval timeout;		/* for timeout */
	struct timeval noTimeout;	/* for timeout */

	/* input check */
	if (argc != 5) {
		fprintf(stderr, "Error: Wrong number of arguments\n");
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "./server  server_port  board_size  sec_per_round  dict_file \n");
		exit(EXIT_FAILURE);
	}
	FILE* file = fopen(argv[4], "r");

	boardSize = atoi(argv[2]);		/* get board size */
	char board[boardSize];			/* initialize board */
	char allGuesses[1000][boardSize];	/* data structure for client guesses */
	char checkBoard[boardSize];			/* data structure to check against client guesses for validity */

	sec = atoi(argv[3]);			/* get number of seconds for timeout */

	timeout.tv_sec = sec;		
	timeout.tv_usec = 0;
	noTimeout.tv_sec = 0;
	noTimeout.tv_usec = 0;

	/* create the dictionary from txt file */
	head = trieCreate();

	if (file != NULL) {
		while (fgets(line, sizeof(line), file) != NULL) {
			token = strtok(line, "\n");
			trieInsert(head, token);
		}
		fclose(file);
	}
	else {
		fprintf(stderr, "Error in opening file\n");
		exit(EXIT_FAILURE);
	}

	memset((char*)&sad, 0, sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */
	sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

	port = atoi(argv[1]);	/* get port # */

	/* tests for illegal port value */
	if (port > 0) {
		sad.sin_port = htons((u_short)port);
	}
	else { /* print error message and exit */
		fprintf(stderr, "Error: Bad port number %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if (((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(sd, (struct sockaddr*) & sad, sizeof(sad)) < 0) {
		fprintf(stderr, "Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(sd, QLEN) < 0) {
		fprintf(stderr, "Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

	/* Main server loop - accept and handle requests */
	while (1) {
		alen = sizeof(cad);
		/* setup for player 1 */
		if ((sd2 = accept(sd, (struct sockaddr*) & cad, (socklen_t*)&alen)) < 0) {
			fprintf(stderr, "Error: First accept failed\n");
			exit(EXIT_FAILURE);
		}

		//send player number
		send(sd2, &one, sizeof(uint8_t), 0);
		n = recv(sd2, buf, sizeof(uint8_t), 0);
		checkDC(n, sd2, sd3);

		//send board size
		send(sd2, &boardSize, sizeof(uint8_t),0);
		n = recv(sd2, buf, sizeof(uint8_t), 0);
		checkDC(n, sd2, sd3);

		//send number of seconds per round
		send(sd2, &sec, sizeof(uint8_t), 0);
		n = recv(sd2, buf, sizeof(uint8_t),0);
		checkDC(n, sd2, sd3);

		/* setup for player 2 */
		if((sd3 = accept(sd, (struct sockaddr*) &cad, (socklen_t*)&alen)) < 0){
			fprintf(stderr, "Error: Second accept failed\n");
			exit(EXIT_FAILURE);
		}


		//send player number
		send(sd3, &two, sizeof(uint8_t), 0);
		n = recv(sd3, buf, sizeof(uint8_t), 0);
		checkDC(n, sd2, sd3);


		//send board size
		send(sd3, &boardSize, sizeof(uint8_t),0);
		n = recv(sd3, buf, sizeof(uint8_t), 0);
		checkDC(n, sd2, sd3);

		//send number of seconds per round
		send(sd3, &sec, sizeof(uint8_t), 0);
		n = recv(sd3, buf, sizeof(uint8_t), 0);
		checkDC(n, sd2, sd3);

		// Fork, parent loops to receive additonal clients, child runs the game
		pid = fork();

		//if parent, close socket and loop
		if (pid > 0) {
			close(sd2);
			close (sd3);
		}
		//if fork errors
		else if (pid < 0) {
			fprintf(stderr, "Error: fork failed\n");
			close(sd2);
			close(sd3);
			exit(EXIT_FAILURE);
		}
		//else we are a child, run the game
		else {

			/* initialize game values */
			round = 0;
			activePlayer = 1;
			score1 = 0;
			score2 = 0;

			/* while game condition is met, play */
			while (game == 1) {

				round++;

				//free guesses tree per round
				if(round > 1){
					for(int i = 0; i < m; i++){
						deletion(&guesses, allGuesses[i]);
					}
				}
				m = 0;

				//send Player1 scores
				send(sd2, &score1, sizeof(uint8_t), 0);
				n = recv(sd2, buf, sizeof(uint8_t), 0);
				checkDC(n, sd2, sd3);

				send(sd2, &score2, sizeof(uint8_t), 0);
				n = recv(sd2, buf, sizeof(uint8_t), 0);
				checkDC(n, sd2, sd3);

				//send player2 scores
				send(sd3, &score1, sizeof(uint8_t), 0);
				n = recv(sd3, buf, sizeof(uint8_t), 0);
				checkDC(n, sd2, sd3);

				send(sd3, &score2, sizeof(uint8_t), 0);
				n = recv(sd3, buf, sizeof(uint8_t), 0);
				checkDC(n, sd2, sd3);

				/* if either player has won */
				if(score1 == 3 || score2 == 3){
					game = 0;

				} else{

					//send round #
					send(sd2, &round, sizeof(uint8_t), 0);
					n = recv(sd2, buf, sizeof(uint8_t), 0);
					checkDC(n, sd2, sd3);

					send(sd3, &round, sizeof(uint8_t), 0);
					n = recv(sd3, buf, sizeof(uint8_t), 0);
					checkDC(n, sd2, sd3);

					//generate board
					int x = 0;
					while(x < boardSize){
						letter = 'a' + (random()%26);
						board[x] = letter;
						x++;
					}
					x = 0;
					for(int i = 0; i < boardSize; i++){
						if(board[i] == 'a' || board[i] == 'e' || board[i] == 'i' || board[i] == 'o' || board[i] == 'u'){
							x = 1;
						}
					}
					if(x == 0){
						while(1){
							letter = 'a' + (random()%26);
							if(letter == 'a' || letter == 'e' || letter == 'i' || letter == 'o' || letter == 'u'){
								board[boardSize-1] = letter;
								break;
							}
						}
					}
					board[boardSize] = '\0';

					//send board
					send(sd2, &board, boardSize, 0);
					n = recv(sd2, buf, sizeof(uint8_t), 0);
					checkDC(n, sd2, sd3);

					send(sd3, &board, strlen(board),0);
					n = recv(sd3, buf, sizeof(uint8_t), 0);
					checkDC(n, sd2, sd3);

					if(round % 2 != 0){
						activePlayer = 1;
					} else{
						activePlayer = 2;
					}

					guesses = trieCreate();


					//turn loop
					while(1){
						if(activePlayer == 1){

							//send Y to player 1
							send(sd2, &active, sizeof(char),0);
							n = recv(sd2, buf, sizeof(uint8_t), 0);
							checkDC(n, sd2, sd3);

							//send N to player 2
							send(sd3, &inactive, sizeof(char), 0);
							n = recv(sd3, buf, sizeof(uint8_t), 0);
							checkDC(n, sd2, sd3);

							//wait for user input
							/* set socket option for timeout only when waiting for user input */
							setsockopt(sd2, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
							n = recv(sd2, buf, sizeof(buf), 0);
							/* reset socket option for other recvs */
							setsockopt(sd2, SOL_SOCKET, SO_RCVTIMEO, &noTimeout, sizeof(noTimeout));
							guessLen = n;

							/* get player 1 guess */
							for(int i = 0; i < guessLen; i++){
								line[i] = buf[i];
							}
							if (n == -1) {
								printf("Player 1 TIMEOUT\n");
							}
							else if (n > 0){
								line[guessLen] = '\0';
							}

							/* validity checking */
							check = 0;
							if(guessLen <= boardSize && n > -1){
								strcpy(checkBoard, board);
								for(int i = 0; i < guessLen; i++){
									check = 0;
									for(int j = 0; j < boardSize; j++){
										if(line[i] == checkBoard[j]){
											checkBoard[j] = '\0';
											check = 1;
											break;
										}
									}
									if(check == 0){
										break;
									}
								}
							}

							//if valid guess
							if(check == 1 && trieSearch(head, line) && !trieSearch(guesses, line)){
								
								for(int i = 0; i < guessLen; i++){
									allGuesses[m][i] = line[i];
								}
								allGuesses[m][guessLen] = '\0';
								m++;

								/* send players a "correct" value */
								send(sd2, &one, sizeof(uint8_t), 0);
								n = recv(sd2, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								send(sd3, &one, sizeof(uint8_t), 0);
								n = recv(sd3, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								/* send player 2 player 1's guess */
								send(sd3, &line, strlen(line), 0);
								n = recv(sd3, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								/* add guess to tree */
								trieInsert(guesses, line);


							} 
							/* incorrect/invalid guess */
							else{

								/* send zero to both players */
								send(sd2, &zero, sizeof(uint8_t),0);
								n = recv(sd2, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								send(sd3, &zero, sizeof(char), 0);
								n = recv(sd3, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								/* send player 2 incorrect guess */
								if (n > 0){
									send(sd3, &line, strlen(line), 0);
									n = recv(sd3, buf, sizeof(uint8_t), 0);
									checkDC(n, sd2, sd3);
								}
								else {
									send(sd3, &" ", strlen(" "), 0);
									n = recv(sd3, buf, sizeof(uint8_t), 0);
									checkDC(n, sd2, sd3);
								}

								score2++;
								activePlayer = 2;
								break;
							}

							activePlayer = 2;

						} 
						/* if player 2 is active */
						/* same logic as above */
						else{
							
							send(sd2, &inactive, sizeof(char), 0);
							n = recv(sd2, buf, sizeof(uint8_t), 0);
							checkDC(n, sd2, sd3);

							send(sd3, &active, sizeof(char), 0);
							n = recv(sd3, buf, sizeof(uint8_t), 0);
							checkDC(n, sd2, sd3);

							//get input
							setsockopt(sd3, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
							n = recv(sd3, buf, sizeof(buf), 0);
							setsockopt(sd3, SOL_SOCKET, SO_RCVTIMEO, &noTimeout, sizeof(noTimeout));
							guessLen = n;

							for(int i = 0; i < guessLen; i++){
								line[i] = buf[i];
							}
							if (n == -1)
								printf(" Player 2 TIMEOUT\n");
							if (n > 0){
								line[guessLen] = '\0';
							}

							check = 0;
							if(guessLen <= boardSize && n > -1){
								strcpy(checkBoard, board);
								for(int i = 0; i < guessLen; i++){
									check = 0;
									for(int j = 0; j < boardSize; j++){
										if(line[i] == checkBoard[j]){
											checkBoard[j] = '\0';
											check = 1;
											break;
										}
									}
									if(check == 0){
										break;
									}
								}

							}

							//check for validity
							if(check == 1 && trieSearch(head, line) && !trieSearch(guesses, line)){
								for(int i = 0; i < guessLen; i++){
									allGuesses[m][i] = line[i];
								}
								allGuesses[m][guessLen] = '\0';
								m++;

								send(sd3, &one, sizeof(uint8_t), 0);
								n = recv(sd3,buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								send(sd2, &one, sizeof(uint8_t), 0);
								n = recv(sd2, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								send(sd2, &line, strlen(line), 0);
								n = recv(sd2, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								trieInsert(guesses, line);
							} else{
								send(sd3, &zero, sizeof(uint8_t), 0);
								n = recv(sd3, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								send(sd2, &zero, sizeof(uint8_t), 0);
								n = recv(sd2, buf, sizeof(uint8_t), 0);
								checkDC(n, sd2, sd3);

								if (n > 0){
									send(sd2, &line, strlen(line), 0);
									n = recv(sd2, buf, sizeof(uint8_t), 0);
									checkDC(n, sd2, sd3);
								}
								else {
									send(sd2, &" ", strlen(" "), 0);
									n = recv(sd2, buf, sizeof(uint8_t), 0);
									checkDC(n, sd2, sd3);
								}

								score1++;
								activePlayer = 1;
								break;
							}


							activePlayer = 1;

						}

					}

				}
			}
			//end game
			close(sd2);
			close(sd3);
			exit(0);
		}

	}
}


