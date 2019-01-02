all: l1

l1: parser.o main.o
	g++ -std=c++11 -g -o l1 main.o parser.o

main.o: main.cpp parser.o
	g++ -std=c++11 -g -c main.cpp

parser.o: parser.cpp
	g++ -std=c++11 -g -c parser.cpp

.PHONY: clean

clean: 
	rm *.o l1