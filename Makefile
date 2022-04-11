CC=gcc -Wall

all: client server

client: client.c
	$(CC) -o client client.c

server: server.c trie.c
	$(CC) -o server server.c trie.c

clean:
	rm -f client server
