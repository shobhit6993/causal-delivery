all: process0 process1 process2 process3 process4

process0: process.cpp process.h
	g++ -o process0 -pthread process.cpp

process1: process.cpp process.h
	g++ -o process1 -pthread process.cpp

process2: process.cpp process.h
	g++ -o process2 -pthread process.cpp

process3: process.cpp process.h
	g++ -o process3 -pthread process.cpp

process4: process.cpp process.h
	g++ -o process4 -pthread process.cpp

clean:
	rm -f *.o *.txt process0 process1 process2 process3 process4