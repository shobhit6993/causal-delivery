all: process

process: process.cpp process.h
	g++ -o process -pthread process.cpp

clean:
	rm -f *.o *.txt process0 process1 process2 process3 process4