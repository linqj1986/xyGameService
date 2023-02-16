#include <stdlib.h>
#include <hiredis/hiredis.h>
#include <string.h>
#include <vector>
#include <queue>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>
#include "SingletonObj.h"
using namespace std;


class CRedisClient
{
public:
    CRedisClient();
    virtual ~CRedisClient();
    void Init(string ip, int port, string psw,string db, int timeout = 2000);
    bool ExecuteCmd(string &response,const char *fmt, vector<string>& vec);
    bool ExecuteArrayCmd(vector<string>& vecKeys, vector<string>& vecValues,const char *fmt, vector<string>& vec);

    redisReply* ExecuteCmd(const char *fmt, vector<string>& vec);

private:
    int m_timeout;
    int m_serverPort;
    string m_setverIp;
    string mstrPsw;
    string mstrDb;
    sem_t semlock;
    std::queue<redisContext *> m_clients;

    time_t m_beginInvalidTime;
    static const int m_maxReconnectInterval = 3;

    redisContext* CreateContext();
    void ReleaseContext(redisContext *ctx, bool active);
    bool CheckStatus(redisContext *ctx);
};
