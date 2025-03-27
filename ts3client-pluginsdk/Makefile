#
# Makefile to build TeamSpeak 3 Client Test Plugin
#

OUTPUT = "${HOME}/Scaricati/TeamSpeak3-Client-linux_amd64/plugins/test_plugin.so"
CFLAGS = -c -O2 -Wall -fPIC

all: config test_plugin

config:
	mkdir -p ~/.local/share/teamspeak-gsm
	echo channel=default >> ~/.local/share/teamspeak-gsm/env
	echo arduino_ip=1.2.3.4 >> ~/.local/share/teamspeak-gsm/env
	echo arduino_port=1234 >> ~/.local/share/teamspeak-gsm/env

test_plugin: server.o plugin.o
	gcc -o $(OUTPUT) -shared plugin.o server.o -lsoxr 
#-lcoap-3-notls 

plugin.o: ./src/plugin.c
	gcc -Iinclude src/plugin.c $(CFLAGS)

server.o: ./src/server.c
	gcc -Iinclude -I/usr/include/soxr src/server.c $(CFLAGS)
# -I/usr/include/coap3 -lcoap-3-notls 

clean:
	rm -rf *.o /home/simone/Scaricati/TeamSpeak3-Client-linux_amd64/plugins/test_plugin.so
