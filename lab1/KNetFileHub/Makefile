.DEFAULT_GOAL := all

all: server client

server-debug: server.c nfhs.c nfh.c util.c
	gcc -Wall -Werror -D DEBUGON -g server.c nfhs.c util.c nfh.c -o server_debug

server: server.c nfhs.c nfh.c util.c
	gcc -Wall -Werror server.c nfhs.c util.c nfh.c -o server

client-debug: client.c nfhc.c nfh.c util.c
	gcc -Wall -Werror -D DEBUGON -g client.c nfhc.c util.c nfh.c -o client_debug

client: client.c nfhc.c nfh.c util.c
	gcc -Wall -Werror client.c nfhc.c util.c nfh.c -o client

clean:
	rm -f server client server_debug client_debug
