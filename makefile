all: process0 process1 process2

process0: process.cpp process.h
	g++ -o process0 -pthread process.cpp

process1: process.cpp process.h
	g++ -o process1 -pthread process.cpp

process2: process.cpp process.h
	g++ -o process2 -pthread process.cpp

# client: client.cpp process.h
# 	g++ -o client -pthread client.cpp

# test: test.cpp process.h
# 	g++ -o test -pthread test.cpp

# 4: 4.cpp process.h
# 	g++ -o 4 -pthread 4.cpp