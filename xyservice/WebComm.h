#include <string>
#include <vector>
using namespace std;

#define URLEXITROOM      "/vc/UserOut"
#define URLBEGINGAME     "/vc/BeginGame"
#define URLGAMETIMEOUT   "/vc/GameTimeOut"
#define URLBOMB   	 "/vc/Bomb"
#define URLCHECK   	 "/vc/VerificationGameData"

#define POSTCREATE  1
#define POSTJOIN    2

#define TESTNOWEB 0

typedef enum ENUM_ERRORCODE
{
	ERROR0NORMAL 	= 0,
	SUCCESS 	= 1,
	ERROR0UNKNOWN0USER       = 9000,
	ERROR0Exit0Check    	 = 9001,
	ERROR0GameStart0IDLE	 = 9002,
	ERROR0GameStart0OWER	 = 9003,
	ERROR0RoleChg0Check	 = 9004,
	//ERROR0PostGameStart	 = 9005,
	//ERROR0PostCheck        = 9006,
	ERROR0Role0Viewer	 = 9007,
	ERROR0POST0ERROR	 = 9008,
	ERROR0Count	         = 9900
}TIPCODE;

class CWebComm
{
public:
    	CWebComm();
    	~CWebComm();

	int PostUserExitRoom(int nGameId, int nUserId, bool bGameBegin, int nUserOutType);
	int PostGameStart(int nGameId, int nUserId, int nLiveUserId, int nLiveId);
	int PostGameTimeOut(int nGameId, int nUserId, int nType=1);
	int PostBomb(int nGameId, int nUserId, int nType, bool bOver,vector<int> &vecIds,string& strDataInfo);
	int PostCheck(int nGameId, int nUserId,int& nJoinUserCount,int& nJoinType,int& nWaitOpenGameTimeOut,string& strDataInfo, int nType=POSTJOIN);
private:
	string mstrHostName;
	string GetRequestData(string strData);
	string GetResponseData(string strData);
	string GetUUID(string strData);
};
