CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -g

TARGET = server
OBJS = main.cpp http_conn.cpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -pthread

clean:
	rm -f $(TARGET)

.PHONY: all clean
