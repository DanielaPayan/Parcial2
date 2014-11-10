#CC=clang++ -std=c++11
CC=g++ -std=c++11
ZMQ=/home/utp/cs/zmq
#ZMQ=/usr/local
ZMQ_LIBS=$(ZMQ)/lib
ZMQ_HDRS=$(ZMQ)/include


all: client server repserver broker

client: client.cc
	$(CC) -I$(ZMQ_HDRS) -c client.cc
	$(CC) -L$(ZMQ_LIBS) -o client client.o -lzmq -lczmq -lmpg123 -lao

server: server.cc
	$(CC) -I$(ZMQ_HDRS) -c server.cc
	$(CC) -L$(ZMQ_LIBS) -o server server.o -lzmq -lczmq 

repserver: repServer.cc
	$(CC) -I$(ZMQ_HDRS) -c repServer.cc
	$(CC) -L$(ZMQ_LIBS) -o repServer repServer.o -lzmq -lczmq

broker: broker.cc
	$(CC) -I$(ZMQ_HDRS) -c broker.cc
	$(CC) -L$(ZMQ_LIBS) -o broker broker.o -lzmq -lczmq

clean:
	rm -rf client client.o server server.o broker broker.o repServer repServer.o *~


#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/utp/cs/zmq/lib
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/utp/zmq/lib
# LD_LIBRARY_PATH=/usr/local/lib
# export LD_LIBRARY_PATH
