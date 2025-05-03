CXX = g++

CXXFLAGS = -g -std=c++11 

server: main.cpp ./timer/lst_timer.cpp ./http/http_conn.cpp ./log/log.cpp ./mysql/sql_connection_pool.cpp ./webserver.cpp ./config.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ -lpthread -lmysqlclient
clean:
	rm -f server