EXE=server_main
WORKPATH = ../include/
CC=g++ 
FLAG= -levent_core -levent_pthreads -lhiredis -lpthread -ljson -lcurl -lcpp_redis -ltacopie
OBJ=TSocket.o DataNode.o DataPacket.o DataSender.o DataSocket.o DataService.o CustomerClient.o CustomerService.o xyservice.o DataClient.o redis_publisher.o ParseIniFile.o $(WORKPATH)fileoperator.o $(WORKPATH)MD5.o PrintLog.o des.o EncryptWrapper.o HttpRequest.o WebComm.o ListenService.o RedisClient.o Subscriber.o

all:$(EXE)

$(EXE):$(OBJ)
	$(CC)  -std=c++11 -g -rdynamic -o xybombservice TSocket.o DataNode.o DataPacket.o DataSender.o DataSocket.o DataService.o CustomerClient.o CustomerService.o redis_publisher.o ParseIniFile.o  $(WORKPATH)fileoperator.o $(WORKPATH)MD5.o PrintLog.o des.o EncryptWrapper.o HttpRequest.o WebComm.o ListenService.o RedisClient.o xyservice.o Subscriber.o  $(FLAG)

Subscriber.o:Subscriber.h
	$(CC) -std=c++11 -c Subscriber.cpp

CustomerService.o:CustomerService.h
	$(CC) -std=c++11 -c CustomerService.cpp
TSocket.o:TSocket.h
	$(CC) -std=c++11 -c TSocket.cpp
DataNode.o:DataNode.h
	$(CC) -std=c++11 -c DataNode.cpp
DataPacket.o:DataPacket.h
	$(CC) -std=c++11 -c DataPacket.cpp
DataSender.o:DataSender.h
	$(CC) -std=c++11 -c DataSender.cpp
DataSocket.o:DataSocket.h
	$(CC) -std=c++11 -c DataSocket.cpp
DataService.o:DataService.h
	$(CC) -std=c++11 -c DataService.cpp
CustomerClient.o:CustomerClient.h
	$(CC) -std=c++11 -c CustomerClient.cpp
redis_publisher.o:redis_publisher.h
	$(CC) -std=c++11 -c redis_publisher.cpp
DataClient.o:DataClient.h
	$(CC) -std=c++11 -c DataClient.cpp
ParseIniFile.o:ParseIniFile.h
	$(CC) -std=c++11 -c ParseIniFile.cpp
fileoperator.o:$(WORKPATH)fileoperator.h
MD5.o:$(WORKPATH)MD5.h
PrintLog.o:PrintLog.h
	$(CC) -std=c++11 -c PrintLog.cpp
des.o:des.h
	$(CC) -std=c++11 -c des.cpp
EncryptWrapper.o:EncryptWrapper.h
	$(CC) -std=c++11 -c EncryptWrapper.cpp
HttpRequest.o:HttpRequest.h
	$(CC) -std=c++11 -c HttpRequest.cpp
WebComm.o:WebComm.h
	$(CC) -std=c++11 -c WebComm.cpp
ListenService.o:ListenService.h
	$(CC) -std=c++11 -c ListenService.cpp
RedisClient.o:RedisClient.h
	$(CC) -std=c++11 -c RedisClient.cpp

xyservice.o:xyservice.c
	$(CC) -std=c++11 -c xyservice.c

