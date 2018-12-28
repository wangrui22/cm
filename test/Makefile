all: reg

reg: reg.o
	g++ -std=c++11 -o reg reg.o

reg.o: reg.cpp
	g++ -std=c++11 -c reg.cpp

.PHONY: clean

clean: 
	rm *.o reg