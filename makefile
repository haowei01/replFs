
CFLAGS	= -g -Wall -DSUN
# CFLAGS	= -g -Wall -DDEC
#CC	= gcc
CC = g++
CCF	= $(CC) $(CFLAGS)

H	= .
C_DIR	= .

INCDIR	= -I$(H)
LIBDIRS = -L$(C_DIR)
LIBS    = -lclientReplFs

CLIENT_OBJECTS = network.o client.o

all:	appl  server

server:	server.o network.o $(C_DIR)/libclientReplFs.a
	$(CCF) -o replFsServer network.o server.o $(LIBDIRS) $(LIBS)

network.o:	network.cpp network.h
	$(CCF) -c $(INCDIR) network.cpp

server.o: server.cpp network.h server.h
	$(CCF) -c $(INCDIR) server.cpp

appl:	appl.o $(C_DIR)/libclientReplFs.a
	$(CCF) -o appl appl.o $(LIBDIRS) $(LIBS)

appl.o:	appl.c client.h appl.h
	$(CCF) -c $(INCDIR) appl.c

$(C_DIR)/libclientReplFs.a:	$(CLIENT_OBJECTS)
	ar cr libclientReplFs.a $(CLIENT_OBJECTS)
	ranlib libclientReplFs.a

client.o:	client.cpp client.h network.h
	$(CCF) -c $(INCDIR) client.cpp

clean:
	rm -f appl replFsServer *.o *.a *.gch

