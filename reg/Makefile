all: reg tmp

reg: reg.o
	g++ -std=c++11 -o reg reg.o

reg.o: reg.cpp
	g++ -std=c++11 -c reg.cpp

tmp.o: tmp.cpp
	g++ -std=c++11 -c tmp.cpp

.PHONY: clean

clean: 
	rm *.o reg tmp