EXE=server_main
WORKPATH = ../../include/
BASEPATH = ../
CC=g++ 
FLAG= -levent_core -levent_pthreads -lhiredis -lpthread -ljson -lcurl -lcpp_redis -ltacopie
OBJ=$(WORKPATH)fileoperator.o $(WORKPATH)MD5.o $(BASEPATH)TSocket.o $(BASEPATH)DataNode.o $(BASEPATH)DataPacket.o $(BASEPATH)DataSender.o $(BASEPATH)DataSocket.o $(BASEPATH)DataService.o $(BASEPATH)DataClient.o $(BASEPATH)ParseIniFile.o $(BASEPATH)PrintLog.o $(BASEPATH)des.o $(BASEPATH)EncryptWrapper.o $(BASEPATH)HttpRequest.o $(BASEPATH)WebComm.o $(BASEPATH)ListenService.o $(BASEPATH)RedisClient.o $(BASEPATH)Subscriber.o TFCustomerClient.o TFCustomerService.o TFRedis.o xytfservice.o

all:$(EXE)

$(EXE):$(OBJ)
	$(CC)  -std=c++11 -g -rdynamic -o xytfservice $(WORKPATH)fileoperator.o $(WORKPATH)MD5.o $(BASEPATH)TSocket.o $(BASEPATH)DataNode.o $(BASEPATH)DataPacket.o $(BASEPATH)DataSender.o $(BASEPATH)DataSocket.o $(BASEPATH)DataService.o $(BASEPATH)ParseIniFile.o $(BASEPATH)PrintLog.o $(BASEPATH)des.o $(BASEPATH)EncryptWrapper.o $(BASEPATH)HttpRequest.o $(BASEPATH)WebComm.o $(BASEPATH)ListenService.o $(BASEPATH)RedisClient.o $(BASEPATH)Subscriber.o TFCustomerClient.o TFCustomerService.o TFRedis.o xytfservice.o   $(FLAG)

fileoperator.o:  $(WORKPATH)fileoperator.h
MD5.o:           $(WORKPATH)MD5.h
TSocket.o:       $(BASEPATH)TSocket.h
	$(CC) -std=c++11 -c $(BASEPATH)TSocket.cpp
DataNode.o:      $(BASEPATH)DataNode.h
	$(CC) -std=c++11 -c $(BASEPATH)DataNode.cpp
DataPacket.o:    $(BASEPATH)DataPacket.h
	$(CC) -std=c++11 -c $(BASEPATH)DataPacket.cpp
DataSender.o:    $(BASEPATH)DataSender.h
	$(CC) -std=c++11 -c $(BASEPATH)DataSender.cpp
DataSocket.o:    $(BASEPATH)DataSocket.h
	$(CC) -std=c++11 -c $(BASEPATH)DataSocket.cpp
DataService.o:   $(BASEPATH)DataService.h
	$(CC) -std=c++11 -c $(BASEPATH)DataService.cpp
DataClient.o:    $(BASEPATH)DataClient.h
	$(CC) -std=c++11 -c $(BASEPATH)DataClient.cpp
ParseIniFile.o:  $(BASEPATH)ParseIniFile.h
	$(CC) -std=c++11 -c $(BASEPATH)ParseIniFile.cpp
PrintLog.o:      $(BASEPATH)PrintLog.h
	$(CC) -std=c++11 -c $(BASEPATH)PrintLog.cpp
des.o:           $(BASEPATH)des.h
	$(CC) -std=c++11 -c $(BASEPATH)des.cpp
EncryptWrapper.o:$(BASEPATH)EncryptWrapper.h
	$(CC) -std=c++11 -c $(BASEPATH)EncryptWrapper.cpp
HttpRequest.o:   $(BASEPATH)HttpRequest.h
	$(CC) -std=c++11 -c $(BASEPATH)HttpRequest.cpp
WebComm.o:       $(BASEPATH)WebComm.h
	$(CC) -std=c++11 -c $(BASEPATH)WebComm.cpp
ListenService.o: $(BASEPATH)ListenService.h
	$(CC) -std=c++11 -c $(BASEPATH)ListenService.cpp
RedisClient.o:   $(BASEPATH)RedisClient.h
	$(CC) -std=c++11 -c $(BASEPATH)RedisClient.cpp
Subscriber.o:    $(BASEPATH)Subscriber.h
	$(CC) -std=c++11 -c $(BASEPATH)Subscriber.cpp
TFCustomerService.o:TFCustomerService.h
	$(CC) -std=c++11 -c TFCustomerService.cpp
TFCustomerClient.o:TFCustomerClient.h
	$(CC) -std=c++11 -c TFCustomerClient.cpp
TFRedis.o:TFRedis.h
	$(CC) -std=c++11 -c TFRedis.cpp
xytfservice.o:xytfservice.c
	$(CC) -std=c++11 -c xytfservice.c

