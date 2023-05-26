CC = gcc
CFLAGS = -std=c17
CPPFLAGS = -I/opt/homebrew/opt/openssl@3/include $(shell pkg-config --cflags libwebsockets) $(shell pkg-config --cflags json-c)
LDFLAGS = -L/opt/homebrew/opt/openssl@3/lib $(shell pkg-config --libs libwebsockets) $(shell pkg-config --libs json-c)

objects = session_manager.o main.o msg_frag_manager.o
signal-server: $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) $(objects) -o signal-server

%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -c $< -o $@

clean:
	rm -f $(objects) signal-server