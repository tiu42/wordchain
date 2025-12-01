CC = gcc
CFLAGS = -Wall -g
LIBS = -lsqlite3
GTK_LIBS = `pkg-config --cflags --libs gtk+-3.0`

all: server client

server: server.o database.o message.o
	$(CC) $(CFLAGS) -o server server.o database.o message.o $(LIBS)

client: client.o database.o message.o
	$(CC) $(CFLAGS) -o client client.o database.o message.o $(LIBS) $(GTK_LIBS)

server.o: server.c database.h model/message.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c database.h model/message.h
	$(CC) $(CFLAGS) -c client.c $(GTK_LIBS)

database.o: database.c database.h
	$(CC) $(CFLAGS) -c database.c

message.o: model/message.c model/message.h
	$(CC) $(CFLAGS) -c model/message.c

clean:
	rm -f *.o server client

.PHONY: all clean
