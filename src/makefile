CC = gcc
CFLAGS = -std=gnu11 -Wall
CPPFLAGS = -I/opt/homebrew/opt/openssl@3/include $(shell pkg-config --cflags libwebsockets) $(shell pkg-config --cflags json-c) $(shell pkg-config --cflags uuid)
LDFLAGS = $(shell pkg-config --libs libwebsockets) $(shell pkg-config --libs json-c) $(shell pkg-config --libs uuid)

objects = session_manager.o main.o msg_frag_manager.o message_queue.o
signal-server: $(objects)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(objects) -o bin/signal-server $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(objects) bin/signal-server
