target: stress_test

stress_test: main.o
	g++ -o stress_test main.o

main.o: main.cpp
	g++ -g -Wall -o main.o -c main.cpp

clean:
	rm -f stress_test main.o