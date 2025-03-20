
CXX = g++
CXXFLAGS = -Wall -std=c++11

CLIENT_SRC = client.cpp
SERVER_SRC = server.cpp
SERVER_WRITEV_SRC = server_writev.cpp

CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
SERVER_WRITEV_OBJ = $(SERVER_WRITEV_SRC:.cpp=.o)

CLIENT_EXEC = client
SERVER_EXEC = server
SERVER_WRITEV_EXEC = server_writev

all: $(CLIENT_EXEC) $(SERVER_EXEC) $(SERVER_WRITEV_EXEC)

$(CLIENT_EXEC): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SERVER_EXEC): $(SERVER_OBJ)		
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SERVER_WRITEV_EXEC): $(SERVER_WRITEV_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(CLIENT_EXEC) $(SERVER_EXEC)

.PHONY: all clean