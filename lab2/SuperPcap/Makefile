.DEFAULT_GOAL := all

all: spcap

spcap-debug: sp.c util.c util.h packet.c packet.h
	gcc -Wall -Werror -D DEBUGON -g util.c util.h packet.c packet.h sp.c -lpcap -o spcap_debug
	sudo setcap cap_net_raw+eip ./spcap_debug

spcap: sp.c util.c util.h packet.c packet.h
	gcc -Wall -Werror util.c util.h sp.c packet.c packet.h -lpcap -o spcap
	sudo setcap cap_net_raw+eip ./spcap

clean:
	rm -f spcap spcap_debug
