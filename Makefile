all: server client

server:	server.cpp server.h message.h
	g++ -o server server.cpp
client:	client.cpp client.h message.h
	g++ -o client client.cpp
clean:
	rm -f server client 
