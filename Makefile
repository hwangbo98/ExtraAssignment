# Makefile for fileserver and fileclient
CC = gcc
CFLAGS = -Wall -pthread
TARGET1 = fileserver
TARGET2 = fileclient
OBJS1 = fileserver.o
OBJS2 = fileclient.o

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJS1)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJS1)

$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $(TARGET2) $(OBJS2)

fileserver.o: fileserver.c
	$(CC) $(CFLAGS) -c -o fileserver.o fileserver.c

fileclient.o: fileclient.c
	$(CC) $(CFLAGS) -c -o fileclient.o fileclient.c

clean:
	rm -f $(OBJS1) $(OBJS2) $(TARGET1) $(TARGET2)