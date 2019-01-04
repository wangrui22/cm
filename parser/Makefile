all: l1

l1: parser.o main.o util.o
	g++ -std=c++11 -g -o l1 main.o parser.o -lpthread -lboost_system -lboost_filesystem -lboost_thread

main.o: main.cpp parser.o util.o
	g++ -std=c++11 -g -c main.cpp

parser.o: parser.cpp parser.h common.h util.o
	g++ -std=c++11 -g -c parser.cpp

util.o: util.cpp util.h
	g++ -std=c++11 -g -c util.cpp

.PHONY: clean

clean: 
	rm *.o l1