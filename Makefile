TARGET = ciadpi
CC ?= gcc
CFLAGS += -std=c99 -g -O2 -D_XOPEN_SOURCE=500 
SOURCES = packets.c main.c conev.c proxy.c desync.c mpool.c extend.c
WIN_SOURCES = win_service.c

all:
	$(CC) $(CFLAGS) $(SOURCES) -I . -o $(TARGET)

windows:
	$(CC) $(CFLAGS) $(SOURCES) $(WIN_SOURCES) -I . -lws2_32 -lmswsock -o $(TARGET).exe

clean:
	rm -f $(TARGET) *.o
