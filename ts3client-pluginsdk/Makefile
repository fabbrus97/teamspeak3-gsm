#
# Makefile to build TeamSpeak 3 Client Test Plugin
#

OUTPUT = "${HOME}/Scaricati/TeamSpeak3-Client-linux_amd64/plugins/test_plugin.so"
# CFLAGS = -c -O2 -Wall -fPIC
CFLAGS = -c -g -O0 -fPIC -Wall # -w
CONFIG_PATH = "~/.config/teamspeak-gsm"
all: config test_plugin

config:
	mkdir -p $(CONFIG_PATH)
	if [ ! -f $(CONFIG_PATH) ]; then cp settins.ini $(CONFIG_PATH)/ ; fi

test_plugin: server.o plugin.o at_commands.o settings.o
	gcc -o $(OUTPUT) -shared plugin.o server.o at_commands.o settings.o -lsoxr -liniparser
#-lcoap-3-notls 

plugin.o: ./src/plugin.c
	gcc -Iinclude src/plugin.c $(CFLAGS)

server.o: ./src/server.c
	gcc -Iinclude -I/usr/include/soxr src/server.c $(CFLAGS)
# -I/usr/include/coap3 -lcoap-3-notls 

settings.o: ./src/settings.c
	gcc -Iinclude -I/usr/include/iniparser src/settings.c $(CFLAGS)

at_commands.o: ./src/utils/at_commands.c
	gcc -Isrc/utils src/utils/at_commands.c -liconv $(CFLAGS)

clean:
	rm -rf *.o /home/simone/Scaricati/TeamSpeak3-Client-linux_amd64/plugins/test_plugin.so
