CC = gcc
CPP = g++
LD = ld
RM = -rm -rf
AR = ar
INCLUDES = -I/usr/include
LD_LIBRARY_PATH = LD_LIBRARY_PATH:.

CFLAGS =  $(INCLUDES) -Wall -O2 -ggdb
LFLAGS = -lspread -lpthread

targets = lockserver client1 client2 client3 

all: $(targets)

clean:
	$(RM) *.o $(targets)

lockserver: lockserver.c
	$(CC) $(CFLAGS) $(LDFLAGS) -D CLIENT_NAME=\"tokensrv\" -o $@ proj4.c lockserver.c $(LFLAGS)

client1: client.c 
	$(CC) $(CFLAGS) $(LDFLAGS) -D CLIENT_NAME=\"client1\" -o $@ proj4.c client.c $(LFLAGS)

client2: client.c
	$(CC) $(CFLAGS) $(LDFLAGS) -D CLIENT_NAME=\"client2\" -o $@ proj4.c client.c $(LFLAGS)

client3: client.c
	$(CC) $(CFLAGS) $(LDFLAGS) -D CLIENT_NAME=\"client3\" -o $@ proj4.c client.c $(LFLAGS)

                
.DEFAULT : all
.PHONY : clean all
