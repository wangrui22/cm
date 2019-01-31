CC = g++ -std=c++11

ver = release
ifeq ($(ver), debug)
CFLAGS += -Wall -g
else
CFLAGS += -Wall -O3
endif

all: l1

l1: main.o obfuscator.o lex.o util.o
	$(CC) $(CFLAGS) -o l1 main.o lex.o obfuscator.o util.o \
	-lmbedcrypto -lmbedtls -lmbedx509 \
	-lpthread -lboost_system -lboost_filesystem -lboost_thread
	

main.o: main.cpp lex.o obfuscator.o util.o
	$(CC) $(CFLAGS) -c main.cpp

lex.o: lex.cpp lex.h common.h util.o
	$(CC) $(CFLAGS) -c lex.cpp

obfuscator.o: obfuscator.cpp obfuscator.h common.h util.o lex.o
	$(CC) $(CFLAGS) -c obfuscator.cpp

util.o: util.cpp util.h
	$(CC) $(CFLAGS) -c util.cpp

.PHONY: clean

clean: 
	rm *.o l1