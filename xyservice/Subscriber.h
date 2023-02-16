#include <condition_variable>
#include <iostream>
#include <mutex>
#include <signal.h>
#include <stdio.h>
#include <cstdio>
#include <string>
#include <semaphore.h>
using namespace std;

#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>

typedef std::function<void(const std::string&, const std::string&)> subscribe_callback_t;


class CSubscriber
{
public:
    CSubscriber();
    virtual ~CSubscriber();

    int Init(const subscribe_callback_t& fNotify);
    int Subscribe(string strChannel);
    int UnSubscribe(string strChannel);

private:
    cpp_redis::subscriber msub;

    string mstrIP;
    int mnPort;
    string mstrPsw;

    sem_t moSemLock;

    int Connect();
};
