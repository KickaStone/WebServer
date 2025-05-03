target: server oneshot

server: server.cpp
	g++ -o server server.cpp

oneshot: epoll-oneshot.cpp
	g++ -o epoll-oneshot epoll-oneshot.cpp
	
clean:
	rm -f server epoll-oneshot