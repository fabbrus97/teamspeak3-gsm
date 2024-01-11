#
# Makefile to build TeamSpeak 3 Client Test Plugin
#

OUTPUT = "/home/simone/Scaricati/TeamSpeak3-Client-linux_amd64/plugins/test_plugin.so"
CFLAGS = -c -O2 -Wall -fPIC

all: test_plugin

test_plugin: server.o plugin.o
	gcc -o $(OUTPUT) -shared plugin.o server.o -lcoap-3-notls -lsoxr

plugin.o: ./src/plugin.c
	gcc -Iinclude src/plugin.c $(CFLAGS)

server.o: ./src/server.c
	gcc -Iinclude -I/usr/include/coap3 -I/usr/include/soxr src/server.c -lcoap-3-notls $(CFLAGS)

clean:
	rm -rf *.o /home/simone/Scaricati/TeamSpeak3-Client-linux_amd64/plugins/test_plugin.so
