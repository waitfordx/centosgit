src=$(wildcard *.c)
obj=$(patsubst %.c, %.o, $(src))

all : server client

server: chat_server.o
	gcc chat_server.o -std=gnu99 -Wall -g -o server

client: chat_client.o
	gcc chat_client.o -std=gnu99 -Wall -g -o client

%.o:%.c
	gcc -c $< -Wall -std=gnu99

.phony: clean all

clean:
	-rm -rf server client $(obj)
	-rm -f /tmp/fifo
