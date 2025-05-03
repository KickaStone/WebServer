target: main

main: main.o
	g++ main.o -o main

main.o: server.cpp
	g++ -c server.cpp -o main.o

clean:
	rm -f main.o main

