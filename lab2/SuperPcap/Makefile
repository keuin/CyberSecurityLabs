.DEFAULT_GOAL := all

all: spcap

spcap-debug: sp.c util.c util.h packet.c packet.h layer3.c layer3.h layer4.c layer4.h
	gcc -Wall -Werror -D DEBUGON -g util.c util.h packet.c packet.h layer3.c layer4.c sp.c -lpcap -o spcap_debug
	sudo setcap cap_net_raw+eip ./spcap_debug

spcap: sp.c util.c util.h packet.c packet.h layer3.c layer3.h layer4.c layer4.h
	gcc -Wall -Werror util.c util.h packet.c packet.h layer3.c layer4.c sp.c -lpcap -o spcap
	sudo setcap cap_net_raw+eip ./spcap

util-debug: util.c util.h
	gcc -Wall -Werror -g util.c -o ./util_debug

clean:
	rm -f spcap spcap_debug
