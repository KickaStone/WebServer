target: echo_server echo_client

echo_server: echo_server.o
	g++ echo_server.o -o echo_server

echo_client: echo_client.o
	g++ echo_client.o -o echo_client

clean:
	rm -f echo_server echo_client echo_server.o echo_client.o
