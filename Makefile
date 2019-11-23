CXX = g++
CXXFLAGS = -std=c++11

all:server client

server: server_main.o
	$(CXX) $(CXXFLAGS) version.o message.o server.o server_main.o -o server

server_main.o: server_main.cpp server.o 
	$(CXX) $(CXXFLAGS) -c server_main.cpp

server.o: server.cpp message.o version.o
	$(CXX) $(CXXFLAGS) -c server.cpp

message.o: message.cpp version.o
	$(CXX) $(CXXFLAGS) -c message.cpp

version.o: version.cpp
	$(CXX) $(CXXFLAGS) -c version.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f core *.o
	rm server client
