#include "TFCustomerService.h"
#include <time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../PrintLog.h"

CTFCustomerService *g_pCustomerService=NULL;

void GlobalReSubscribeAll(void* p)
{
	pthread_t hThread;
	pthread_create(&hThread,NULL,CTFCustomerService::ReSubscribeAllThread, (void*)g_pCustomerService);
}

void recieve_message(const std::string&channel_name, const std::string&message)
{
	// redis订阅回调
	if(( g_pCustomerService!=NULL )&&(message.length()!=0))
	{
		//g_pCustomerService->DoSubscriberMsg(channel_name,message);
		TagSubMsg tag;
		tag.strChannel = channel_name;
		tag.strMsg = message;
		sem_wait(&g_pCustomerService->semQueLock);
		g_pCustomerService->m_queSubMsg.push(tag);
		sem_post(&g_pCustomerService->semQueLock);
	}
}

void* CTFCustomerService::PopSubMsgThread( void *param )
{
	pthread_detach(pthread_self());
	CTFCustomerService* pParam = (CTFCustomerService*)param;

	while(1)
	{
		TagSubMsg tag;
		tag.strChannel="";
	
		if(pParam->m_queSubMsg.size()>0)
		{
			sem_wait(&pParam->semQueLock);
			tag = pParam->m_queSubMsg.front();
			pParam->m_queSubMsg.pop();
			sem_post(&pParam->semQueLock);
			//printf("Deal %s\n",tag.strChannel.c_str());
			pParam->DoSubscriberMsg(tag.strChannel.c_str(),tag.strMsg.c_str());
		}
		else
		{
			usleep(100*1000);
		}
	}	
}

void* CTFCustomerService::PopUnsubMsgThread(void *param)
{
	pthread_detach(pthread_self());
	CTFCustomerService* pParam = (CTFCustomerService*)param;

	while(1)
	{
		string strChannel="";
		
		if(pParam->m_queUnSubMsg.size()>0)
		{
			sem_wait(&(pParam->semUnSubQueLock));
			strChannel = pParam->m_queUnSubMsg.front();
			pParam->m_queUnSubMsg.pop();
			sem_post(&(pParam->semUnSubQueLock));

			sem_wait(&(pParam->sem_lock));
			CHANNELIT itrM = pParam->m_mapChannel.find(strChannel.c_str());
			if( itrM==pParam->m_mapChannel.end() )
			{
				CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"----: unsubscribe=%s\n",strChannel.c_str());		
				pParam->m_oSubscriber.UnSubscribe(strChannel);
			}
			sem_post(&(pParam->sem_lock));
		}
		
		usleep(10*1000*1000);
	}
}
bool CTFCustomerService::PraseJsonToChgTag(string strJson,TagChg* pTag)
{
	bool bRet = false;
	Json::FastWriter jWriter;
	Json::Value jData;
	Json::Reader jReader(Json::Features::strictMode());

	if(jReader.parse((const char*)strJson.c_str(),jData))
	{
		bRet = true;
		pTag->nType = jData["Type"].asInt();
		pTag->nSrcTowerId = jData["SrcTowerId"].asInt();
		pTag->nDstTowerId = jData["DstTowerId"].asInt();
		pTag->nBotNum = jData["BotNum"].asInt();
		pTag->nUserId = jData["UserId"].asInt();
		pTag->nSecs = jData["Secs"].asInt();
		pTag->nBeginTime = jData["BeginTime"].asInt();
		pTag->nPassSecs = jData["PassSecs"].asInt();
	}
	return bRet;
}

string CTFCustomerService::PraseChgTagToJson(TagChg tag)
{
	Json::FastWriter jWriter;
	Json::Value jData;
	
	jData["Type"] = tag.nType;
	jData["SrcTowerId"] = tag.nSrcTowerId;
	jData["DstTowerId"] = tag.nDstTowerId;
	jData["BotNum"] = tag.nBotNum;
	jData["UserId"] = tag.nUserId;
	jData["Secs"] = tag.nSecs;
	jData["nBeginTime"] = tag.nSecs;
	jData["nPassSecs"] = tag.nSecs;
	string strData = jWriter.write(jData);
	return strData;
}

string CTFCustomerService::GetChgList(string strChannelName)
{
	Json::FastWriter jWriter;
	Json::Value jData,jArray;
	string strList = "";
	CHANNELIT itrM = m_mapChannel.find(strChannelName);
	if( itrM!=m_mapChannel.end() )
	{
		list<string>::iterator itListFind = itrM->second.chgList.begin();
		while(itListFind != itrM->second.chgList.end())
		{
			jArray.append(*itListFind);
			itListFind++;
		}
		jData["ChgList"] = jArray;
		strList = jWriter.write(jData);	
	}
	return strList;
}

void CTFCustomerService::DoSubscriberMsg(const char *channel_name,const char *message)
{
	sem_wait(&sem_lock);
	Json::FastWriter jWriter;
	Json::Value jRoot;
	Json::Reader jReader(Json::Features::strictMode());
	if(jReader.parse((const char*)message,jRoot))
	{
		// 游戏过程通知
		if((!jRoot["SrcTowerId"].isNull()) &&(!jRoot["DstTowerId"].isNull()))
		{
			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				itrM->second.chgList.push_back(message);
			}
		}
		if(!jRoot["ChgList"].isNull())
		{
			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				// 遍历客户端通知用户变更
				list<CTFCustomerClient*>::iterator itL = itrM->second.clientList.begin();
				while( itL != itrM->second.clientList.end() )
				{
					(*itL)->NotifyGameRoleChange(message);
					itL++;
				}
				BroadcastViewer( COMMANDTYPE_S2C_GAMECOMMANDRET,message,channel_name);
			}
		}
		// 参与者广播通知
		if(!jRoot["UserIds"].isNull())
		{
			DealUserIdsMsg(channel_name,message);
		}
		// 结束通知
		if(!jRoot["WinUserId"].isNull())
		{
			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				list<CTFCustomerClient*>::iterator itL = itrM->second.clientList.begin();
				while( itL != itrM->second.clientList.end() )
				{
					(*itL)->NotifyGameOver(message);
					itL++;
				}
				BroadcastViewer( COMMANDTYPE_S2C_GAMEOVER,message,channel_name);

				struct timespec ts_now;
				clock_gettime(CLOCK_REALTIME,&ts_now);
				itrM->second.nGameOverTime = ts_now.tv_sec;
			}
		}

		// 
		if(!jRoot["TowerIds"].isNull())
		{
			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				list<CTFCustomerClient*>::iterator itL = itrM->second.clientList.begin();
				while( itL != itrM->second.clientList.end() )
				{
					(*itL)->NotifyTowers(message);
					itL++;
				}
				BroadcastViewer( COMMANDTYPE_S2C_TOWERS,message,channel_name);
			}
		}
		
	}
	sem_post(&sem_lock);
}

void CTFCustomerService::DealUserIdsMsg(const char *channel_name,const char *message)
{
	Json::Value jRoot;
	Json::Reader jReader(Json::Features::strictMode());
	bool bNormal = true;
	string strChannelName=channel_name;
	string strNextChannelName=channel_name;
	int nNewGameId = 0;
	// TODO再来一局的广播
	if((!jRoot["NextGameId"].isNull()) && (jRoot["NextGameId"].isInt()))
	{
		// 判断新游戏频道是否存在，不存在则为其他服务器通知过来的，进行新局复制
		nNewGameId = jRoot["NextGameId"].asInt();
		char szNextChannelName[100]={0};
		sprintf(szNextChannelName,"2_%d",nNewGameId);
		strNextChannelName = szNextChannelName;
	}
	if(strChannelName!=strNextChannelName)
	{
		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"UserIds : one more game=%s",
				                     channel_name);

		CHANNELIT itr = m_mapChannel.find(strNextChannelName.c_str());
		if( itr==m_mapChannel.end() )
		{/* 新频道不存在则订阅频道 */
			TagGame tagGame;
			tagGame.nGameOverTime = 0x7ffffffd;
			tagGame.nGameStatus = 0;
			tagGame.nGameId  = nNewGameId;
			tagGame.strLastChannelName = channel_name;
			
			m_mapChannel.insert(CHANNELMAP::value_type(strNextChannelName.c_str(),tagGame));
			SubscribeC(strNextChannelName.c_str());

			CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"UserIds : New Channel=%s",
				                             strNextChannelName.c_str());	
		}
		
		// 旧局玩家追加入新局中
		list<CTFCustomerClient*> listLastTemp;
		CHANNELIT itrM = m_mapChannel.find(channel_name);
		if( itrM!=m_mapChannel.end() )
		{
			// 提取旧局玩家
			listLastTemp = itrM->second.clientList;
			
			// 把旧局玩家加入新局
			CHANNELIT itrNew = m_mapChannel.find(strNextChannelName.c_str());
			if( itrNew!=m_mapChannel.end() )
			{
				list<CTFCustomerClient*>::iterator itListFind = listLastTemp.begin();
				while(itListFind != listLastTemp.end())
				{
					list<CTFCustomerClient*>::iterator itL = find( itrNew->second.clientList.begin(),
						     itrNew->second.clientList.end(),*itListFind);
					if(itL == itrNew->second.clientList.end())
					{
						(*itListFind)->SetChannelName(strNextChannelName.c_str());
						(*itListFind)->SetGameId(nNewGameId);
						itrNew->second.clientList.push_back(*itListFind);
					}
					itListFind++;
				}
			}

			// 清除旧的玩家，游戏id
			itrM->second.clientList.clear();
			m_mapChannel.erase(itrM);

			//m_oSubscriber.unsubscribe(channel_name);
			sem_wait(&g_pCustomerService->semUnSubQueLock);
			g_pCustomerService->m_queUnSubMsg.push(channel_name);
			sem_post(&g_pCustomerService->semUnSubQueLock);
		}

		// 旧局观众追加入新局中
		sem_wait(&semViewerLock);
		map<CTFCustomerClient*,CTFCustomerClient*> mapLastTemp;
		map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itrLast = m_mapViewer.find(channel_name);
		if( itrLast!=m_mapViewer.end() )
		{  
			// 提取旧局观众
			mapLastTemp = itrLast->second;
			
			// 把旧局观众加入新局
			map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itrMapViewer = m_mapViewer.find(strNextChannelName.c_str());
			if( itrMapViewer==m_mapViewer.end() )
			{
				// by linqj
				map<CTFCustomerClient*,CTFCustomerClient*>::iterator itrView = mapLastTemp.begin();
				while( itrView!=mapLastTemp.end() )
				{
					itrView->second->SetChannelName(strNextChannelName.c_str());
					itrView->second->SetGameId(nNewGameId);
					itrView++;
				}

				map<CTFCustomerClient*,CTFCustomerClient*> mapTemp;
				mapTemp=mapLastTemp;
				m_mapViewer.insert(VIEWERMAP::value_type(strNextChannelName.c_str(),mapTemp));
			}
			else
			{
				map<CTFCustomerClient*,CTFCustomerClient*>::iterator itrView = mapLastTemp.begin();
				while( itrView!=mapLastTemp.end() )
				{
					// 新链表中没有，则加入
					map<CTFCustomerClient*,CTFCustomerClient*>::iterator itrClient = itrMapViewer->second.find(itrView->second);
					if( itrClient==itrMapViewer->second.end() )
					{  
						itrView->second->SetChannelName(strNextChannelName.c_str());
						itrView->second->SetGameId(nNewGameId);
						itrMapViewer->second.insert( map<CTFCustomerClient*,
                                                                             CTFCustomerClient*>::value_type(itrView->second,itrView->second));
					}

					itrView++;
				}
			}
			
			// 清除旧的channel观众
			itrLast->second.clear();
			m_mapViewer.erase(itrLast);
		}
		sem_post(&semViewerLock);
		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"UserIds : one more game=%s.",
                     				     channel_name);
	}

	// 广播给用户
	CHANNELIT itrM = m_mapChannel.find(strNextChannelName.c_str());
	if( itrM!=m_mapChannel.end() )
	{
		// TODO 解析userids,将重连到其他服务器的用户删除
		Json::Reader jReader(Json::Features::strictMode());
		Json::FastWriter jWriter;
		Json::Value jUserIdsArray,jGetRoot;
		if(jReader.parse((const char*)message,jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				int nUserId = jUserIdsArray[j]["UserId"].asInt();
				if(jUserIdsArray[j]["Key"].asString()!=GetServerId())
				{
					list<CTFCustomerClient*>::iterator itListFind = itrM->second.clientList.begin();
					while(itListFind != itrM->second.clientList.end())
					{
						if( nUserId== (*itListFind)->GetUserId() )
						{
							CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"---------SetReconn2OtherServer user=%d",
	             			     						     nUserId);
							(*itListFind)->SetReconn2OtherServer(true);
							CTFRedis::GetInstance()->RemoveUserId( (*itListFind)->GetChannelName().c_str(),
									     GetServerId(),
                                                                             (*itListFind)->GetUserId());
							itrM->second.clientList.erase(itListFind);
							break;
						}
						itListFind++;
					}
				}

			}
		}
		/////////////////////////////
		char szTestMsg[5000]={0};
		list<CTFCustomerClient*>::iterator itL = itrM->second.clientList.begin();
		while( itL != itrM->second.clientList.end() )
		{
			char szTestItem[20]={0};
			sprintf(szTestItem,"%u,",(*itL));
			strcat(szTestMsg,szTestItem);

			(*itL)->NotifyUsers(message);
			itL++;
		}
		CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"UserIds =%s",
	             			     	     szTestMsg);
		BroadcastViewer( COMMANDTYPE_S2C_USERS,message,strNextChannelName.c_str());
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"----WARNING: [UserIds] Cant Find Channel=%s\n",strNextChannelName.c_str());
	}
	CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"UserIds : end=%s",
	             			     channel_name);
}

void* CTFCustomerService::CheckBirthThread( void *param )
{
	pthread_detach(pthread_self());
	CTFCustomerService* pParam = (CTFCustomerService*)param;
	pParam->CheckBirth();
}

void CTFCustomerService::CheckBirth()
{
	while(1)
	{
		usleep(CKBOOM_SLEEP_US);
		sem_wait(&sem_lock);
		CHANNELIT itr = m_mapChannel.begin();
		while( itr!=m_mapChannel.end() )
		{
			if((itr->second.nGameStatus == GAMESTATUSBEGIN)&&(itr->second.nOwnerId !=0))
			{//已开始的
				struct timespec ts_now;
				clock_gettime(CLOCK_REALTIME,&ts_now);
				
				bool bChange = false;
				string strValue="";
				bool bRet = CTFRedis::GetInstance()->Lock(itr->first);
				if(bRet)
				{
					Json::Reader jReader(Json::Features::strictMode());
					Json::FastWriter jWriter;
					Json::Value jGetRoot,jTowerIdArray;
					CTFRedis::GetInstance()->GetTowerIds(itr->first,strValue);
					if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
					{
						jTowerIdArray = jGetRoot["TowerIds"];
						// 判断出发/到达时间
						int nChgListSize = itr->second.chgList.size();
						list<string>::iterator itListFind = itr->second.chgList.begin();
						while(itListFind != itr->second.chgList.end())
						{
							TagChg tag;
							PraseJsonToChgTag(*itListFind,&tag);
							int nPassSecs = ts_now.tv_sec-tag.nBeginTime;
							if(nPassSecs>=tag.nSecs)
							{// 到达
								bChange = true;
								if(jTowerIdArray[tag.nDstTowerId]["UserId"]==tag.nUserId)
								{//同一方
									jTowerIdArray[tag.nDstTowerId]["BotNum"] = jTowerIdArray[tag.nDstTowerId]["BotNum"].asInt()+tag.nBotNum;
								}
								else
								{
									if(jTowerIdArray[tag.nDstTowerId]["BotNum"]>tag.nBotNum)
									{//占领方不变
										jTowerIdArray[tag.nDstTowerId]["BotNum"] = jTowerIdArray[tag.nDstTowerId]["BotNum"].asInt()-tag.nBotNum;
									}
									else if(jTowerIdArray[tag.nDstTowerId]["BotNum"]==tag.nBotNum)
									{//无占领方
										jTowerIdArray[tag.nDstTowerId]["BotNum"] = 0;
										jTowerIdArray[tag.nDstTowerId]["UserId"] = 0;
										jTowerIdArray[tag.nDstTowerId]["Time"] = 0;
									}
									else
									{//占领方变更
										jTowerIdArray[tag.nDstTowerId]["BotNum"] = tag.nBotNum-jTowerIdArray[tag.nDstTowerId]["BotNum"].asInt();
										jTowerIdArray[tag.nDstTowerId]["UserId"] = tag.nUserId;
										jTowerIdArray[tag.nDstTowerId]["Time"] = (int)ts_now.tv_sec;
									}
								}
								itListFind = itr->second.chgList.erase(itListFind);
								continue;
							}
							else
							{
								if(tag.nPassSecs==0)
								{
									// 出发
									jTowerIdArray[tag.nSrcTowerId]["BotNum"] = jTowerIdArray[tag.nSrcTowerId]["BotNum"].asInt()-tag.nBotNum;
									if(jTowerIdArray[tag.nSrcTowerId]["BotNum"].asInt()==0)
									{// 空城
										bChange = true;
										jTowerIdArray[tag.nSrcTowerId]["BotNum"] = 0;
										jTowerIdArray[tag.nSrcTowerId]["UserId"] = 0;
										jTowerIdArray[tag.nSrcTowerId]["Time"] = 0;
									}
								}
								tag.nPassSecs = nPassSecs;
								(*itListFind) = PraseChgTagToJson(tag);
							}
							itListFind++;
						}
						if(nChgListSize>0)
						{// 广播派兵动态
							string strList = GetChgList(itr->first);
							CTFRedis::GetInstance()->Publish(itr->first.c_str(), strList);
						}
						// 判断是否结束
						int nWinerId = 0;
						for(int i=0;i<jTowerIdArray.size();i++)
						{
							int nCurUserId = jTowerIdArray[i]["UserId"].asInt();
							if(nWinerId==0)
							{
								nWinerId = nCurUserId;
								continue;
							}
				
							if((nWinerId!=nCurUserId)&&(0!=nCurUserId))
							{
								nWinerId = 0;
								break;
							}
						}
						if(nWinerId!=0)//需要考虑平局的情况
						{
							itr->second.nGameStatus = GAMESTATUSOVER;
							CTFRedis::GetInstance()->DeleteGameInfo(itr->first);
							CTFRedis::GetInstance()->DeleteUserInfo(itr->first);
							
							PublishGameOver(itr->first.c_str(),nWinerId,"");
						}

						// 判断自增
						for(int i=0;i<jTowerIdArray.size();i++)
						{
							int nTime = jTowerIdArray[i]["Time"].asInt();
							if((ts_now.tv_sec-nTime >=1)&&(jTowerIdArray[i]["UserId"]!=0))
							{
								bChange=true;
								jTowerIdArray[i]["BotNum"] = jTowerIdArray[i]["BotNum"].asInt()+1;
								jTowerIdArray[i]["Time"] = (int)ts_now.tv_sec;
							}
						}
						if(bChange)
						{
							jGetRoot["TowerIds"] = jTowerIdArray;
							strValue = jWriter.write(jGetRoot);	
							CTFRedis::GetInstance()->SetTowerIds(itr->first,strValue);
						}
					}	
					bRet = CTFRedis::GetInstance()->Unlock(itr->first);
				}
				if(bChange)
				{// 广播塔兵数更新
					PublishTowerIds( itr->first.c_str(),strValue);
				}	
			}
			// 检测已开始游戏是否过期5分钟 释放资源
			struct timespec ts_now;
			clock_gettime(CLOCK_REALTIME,&ts_now);
			int nTick = ts_now.tv_sec;
			if( (nTick-itr->second.nGameOverTime) > 300)
			{
				CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"=======CLEAR_%s======",
                                                                     itr->first.c_str());

				//m_oSubscriber.unsubscribe(itr->first.c_str());
				sem_wait(&g_pCustomerService->semUnSubQueLock);
				g_pCustomerService->m_queUnSubMsg.push(itr->first.c_str());
				sem_post(&g_pCustomerService->semUnSubQueLock);

				m_mapChannel.erase(itr++);
				continue;
			}
			itr++;
		}
		sem_post(&sem_lock);
	}
}

CTFCustomerService::CTFCustomerService(void)
{
	g_pCustomerService = this;
	memset(m_szMac,0,MAX_CHANNELNAME_LEN);
	sem_init(&sem_lock, 0, 1); // 空闲的
	sem_init(&semAllSockLock, 0, 1);
	sem_init(&semViewerLock, 0, 1);
	sem_init(&semQueLock, 0, 1);
	sem_init(&semUnSubQueLock, 0, 1);

 	pthread_t hCheckAliveThread;
	pthread_create(&hCheckAliveThread,NULL,CheckAliveThread, (void*)this);

	pthread_t hCheckBirthThread;
	pthread_create(&hCheckBirthThread,NULL,CheckBirthThread, (void*)this);

	pthread_t hPopSubMsgThread;
	pthread_create(&hPopSubMsgThread,NULL,CTFCustomerService::PopSubMsgThread, (void*)g_pCustomerService);

	pthread_t hPopUnSubMsgThread;
	pthread_create(&hPopUnSubMsgThread,NULL,CTFCustomerService::PopUnsubMsgThread, (void*)g_pCustomerService);

	
}

CTFCustomerService::~CTFCustomerService(void)
{
	Stop();
	sem_destroy(&sem_lock);
	sem_destroy(&semAllSockLock);
	sem_destroy(&semViewerLock);
}

int CTFCustomerService::OnNewSocket( int sock, const char* pszIp )
{
	if ( sock )
	{
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"new sock=%d\n",sock);
		CTFCustomerClient* pCustomerClient = new CTFCustomerClient((void*)this,pszIp);		
		sem_wait(&semAllSockLock);
		m_vecCustomerClient.push_back( pCustomerClient );
		pCustomerClient->Init(sock);
		sem_post(&semAllSockLock);
	}
	return 1;
}

unsigned int CTFCustomerService::Run( unsigned int uPort, int bAutoAddPort )
{
	return CDataService::Run(uPort, bAutoAddPort);
}

void CTFCustomerService::Stop()
{
	CDataService::Stop();
}

void* CTFCustomerService::CheckAliveThread( void *param )
{
	pthread_detach(pthread_self());
	CTFCustomerService* pParam = (CTFCustomerService*)param;

	// 首次web连接测试
	sleep(1);
	//pParam->m_oWebComm.PostUserExitRoom(0,0,false,2);

	while(1)
	{
		sleep(50);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"CheckAlive..........");
		pParam->CheckAlive();
	}
}

bool CTFCustomerService::IsClientReConn(CTFCustomerClient*pClient)
{
	bool bRet = false;
	sem_wait(&semAllSockLock);
	std::vector<CTFCustomerClient*>::iterator itr = m_vecCustomerClient.begin();
	while( itr != m_vecCustomerClient.end() )
	{
		CTFCustomerClient* pCustomerClient = (CTFCustomerClient*)*itr;
		if (    (pCustomerClient!=pClient)
	             && (pCustomerClient->GetUserId()==pClient->GetUserId())
		     && (pCustomerClient->GetChannelName()==pClient->GetChannelName())
		     && (!pCustomerClient->IsOffLine())
                   )
		{
			bRet = true;
			break;
		}
		itr++;
	}
	sem_post(&semAllSockLock);
	return bRet;
}

void CTFCustomerService::CheckAlive()
{
	sem_wait(&semAllSockLock);
	std::vector<CTFCustomerClient*>::iterator itr = m_vecCustomerClient.begin();
	while( itr != m_vecCustomerClient.end() )
	{
		CTFCustomerClient* pCustomerClient = (CTFCustomerClient*)*itr;
		if (  (!pCustomerClient->IsAlive())
		    /*||(!pCustomerClient->IsValid())*/
		   )
		{
			// szUserId有可能为空，因为连上socket了，但是还未握手就掉线
			CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CheckAlive : Close %d,this=%u\n",pCustomerClient->GetUserId()
                                                             ,pCustomerClient);

			// 如果是还未握手的，直接删除
			if(pCustomerClient->GetChannelName().length()==0)
			{
				pCustomerClient->Close();
				delete pCustomerClient;
				pCustomerClient=NULL;

				itr=m_vecCustomerClient.erase(itr);
				continue;
			}

			// 如果是观众，从列表移除
			bool bHaveViewer = true;
			int nLeft = DelViewer(pCustomerClient,pCustomerClient->GetChannelName().c_str());
			if(nLeft==0)
				bHaveViewer = false;

			//test by linqj
			DelViewerInAll(pCustomerClient,pCustomerClient->GetChannelName().c_str());

			// 如果是玩家，从列表移除
			sem_wait(&sem_lock);
			CHANNELIT itrM = m_mapChannel.find(pCustomerClient->GetChannelName());
			if( itrM!=m_mapChannel.end() )
			{
				list<CTFCustomerClient*>::iterator itL = find( itrM->second.clientList.begin(),
									     itrM->second.clientList.end(),pCustomerClient);
				if(itL != itrM->second.clientList.end())
					itrM->second.clientList.erase(itL);
				CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CheckAlive : game=%s,left=%d\n",
								     pCustomerClient->GetChannelName().c_str(),
                                                                     itrM->second.clientList.size());

				// TODO 如果用户闪退，可能sock没收到close这里要补clientoffline流程--------------------------------------
				if( !pCustomerClient->IsOutRoom()  && !pCustomerClient->IsOffLine()
                                    &&!pCustomerClient->IsReconn2OtherServer()
                                  )
				{
					ClientOffLine(pCustomerClient);
				}
				if((itrM->second.clientList.size()==0)&&!bHaveViewer)
				{
					TagRedisGame tag;
					CTFRedis::GetInstance()->GetGameTag(pCustomerClient->GetChannelName(),tag);
					if(GAMESTATUSWAIT==tag.nGameStatus)
					{//游戏未开始

						// 本服务器已无观众和用户
						//m_oSubscriber.unsubscribe(pCustomerClient->GetChannelName());
						sem_wait(&g_pCustomerService->semUnSubQueLock);
						g_pCustomerService->m_queUnSubMsg.push(pCustomerClient->GetChannelName());
						sem_post(&g_pCustomerService->semUnSubQueLock);

						m_mapChannel.erase(itrM);

						// 判断redis中所有用户是否都离线了
						bool bAllOff = CTFRedis::GetInstance()->IsAllOffLine(pCustomerClient->GetChannelName());
						if(bAllOff)
						{
							CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CheckALive : PostGameTimeOut %d\n",
		                                                                             pCustomerClient->GetUserId());
							//m_oWebComm.PostGameTimeOut( atoi(pCustomerClient->GetGameId().c_str()),
								    		    //pCustomerClient->GetUserId());
						
							CTFRedis::GetInstance()->DeleteUserInfo(pCustomerClient->GetChannelName());
							CTFRedis::GetInstance()->DeleteGameInfo(pCustomerClient->GetChannelName());
						}
						
					}
					else
					{//游戏已开始
						// 超时再清理
					}
					
				}
			}

			// test
			CHANNELIT itrALLM = m_mapChannel.begin();
			while( itrALLM!=m_mapChannel.end() )
			{
				list<CTFCustomerClient*>::iterator itL = find( itrALLM->second.clientList.begin(),
									     itrALLM->second.clientList.end(),pCustomerClient);
				if(itL != itrALLM->second.clientList.end())
				{
					itrALLM->second.clientList.erase(itL);
					CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"!!!DelUserInAll = %s\n",itrALLM->first.c_str());
				}
				itrALLM++;
			}

			pCustomerClient->Close();
			delete pCustomerClient;
			pCustomerClient=NULL;
			sem_post(&sem_lock);

			itr=m_vecCustomerClient.erase(itr);
			continue;
		}
		itr++;
	}
	sem_post(&semAllSockLock);
}

void CTFCustomerService::SubscribeC(const char* szChannelName)
{
	m_oSubscriber.Init(recieve_message);
	m_oSubscriber.Subscribe(szChannelName);
}

void* CTFCustomerService::ReSubscribeAllThread( void *param )
{
	usleep(1000*1000);
	pthread_detach(pthread_self());
	/*CTFCustomerService* pParam = (CTFCustomerService*)param;
	pParam->ReSubscribeAll();*/
}

void CTFCustomerService::ReSubscribeAll()
{
	sem_wait(&sem_lock);
        CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"!!!!!!    CTFCustomerService::DoReSubscribeAll    !!!!!!");
	CHANNELIT itr = m_mapChannel.begin();
	while( itr!=m_mapChannel.end() )
	{
		SubscribeC(itr->first.c_str());
		itr++;
	}
	sem_post(&sem_lock);
}

int CTFCustomerService::ClientCreateRoom(CTFCustomerClient*pClient)
{
	int nRet = ERROR0NORMAL;
	// 锁redis，并操作GameInfo get key
	// 锁redis，并获取所有用户并判断是否存在过
	//int nGameStatus = CTFRedis::GetInstance()->GetGameStatus(pClient->GetChannelName());
	if(  //(nGameStatus==GAMESTATUSCOUNT) 
             //||(IsValidConnect(pClient->GetChannelName(),pClient->GetUserId())) 
             1
          )
	{ 
		// 创建房间的验证，gameid=0
		int nJoinUserCount=0;
		int nJoinType = 1;
		int nWaitOpenGameTimeOut=300;
		string strDataInfo="";
		int nRealGameId = 0;
                int nUpLoadId = 0;
                #if TESTNOWEB //测试无web请求模式
		nJoinUserCount = 1;
		nUpLoadId = pClient->GetGameId();
		#endif
		nRealGameId = m_oWebComm.PostCheck( nUpLoadId,
						    pClient->GetUserId(),nJoinUserCount,nJoinType,nWaitOpenGameTimeOut,strDataInfo,
						    POSTCREATE);
		if(nRealGameId>0)
		{
			if(nRealGameId>0)
			{
				pClient->SetGameId(nRealGameId);
				pClient->SetChannelName(nRealGameId);
				if(nJoinUserCount==1)
					pClient->SetUserType(OWNER);
				else
					pClient->SetUserType(PLAYER);
			
				// 锁m_mapChannel，并操作用户链表
				AddUser(pClient);

				if(nJoinUserCount==1)
				{
					// 锁redis，并操作GameInfo set key
					CTFRedis::GetInstance()->CreateGameInfo( pClient->GetChannelName(),
										 pClient->GetGameId(),nWaitOpenGameTimeOut);
				}
				// 锁redis，并获取所有用户
				if(pClient->GetLastChannelName().length()>0)
				{// 再来一局
					PublishUserIds( pClient->GetUserId(),
							pClient->GetChannelName().c_str(),
                                                        pClient->GetLastChannelName().c_str(),
							pClient->GetGameId(),
                                                        strDataInfo);
				}
				else
				{// 普通创建房间
					PublishUserIds( pClient->GetUserId(),pClient->GetChannelName().c_str(),
                                                        NULL,
							pClient->GetGameId(),
							strDataInfo );
				}
				nRet = SUCCESS;
			}
		}
		else
		{
			nRet = 0-nRealGameId;
		}
	}
	else
	{
		//CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"ClientCreateRoom : Failed User=%s,GameStatus=%d",
		//				     pClient->GetUserId().c_str(),nGameStatus);
	}

	return nRet;
}

int CTFCustomerService::ClientHandShake(CTFCustomerClient*pClient)
{
	int nRet = 0;

	if(pClient->GetUserType()==VIEWER)
	{// 观众握手	
		// 锁m_mapChannel，并insert new channel
		//AddChannel(pClient->GetChannelName().c_str());

		// 锁m_mapViewer，并操作观众链表
		AddViewer(pClient,pClient->GetChannelName().c_str());

		// 锁redis，并获取所有用户
		string strValue;
		bool bRet = CTFRedis::GetInstance()->GetUserIds(pClient->GetChannelName().c_str(),strValue);
		if(bRet)
		{
			pClient->NotifyUsers(strValue);
			nRet = SUCCESS;
		}
	}
	else if((pClient->GetUserType()==OWNER) || (pClient->GetUserType()==PLAYER))
	{// 玩家握手
		// 锁redis，并操作GameInfo get key
		// 锁redis，并获取所有用户并判断是否存在过
		//int nGameStatus = CTFRedis::GetInstance()->GetGameStatus(pClient->GetChannelName());
		if(  //(nGameStatus==GAMESTATUSWAIT)
                     //||(IsValidConnect(pClient->GetChannelName(),pClient->GetUserId())) 
		     1	
                  )
		{ 
			int nJoinUserCount=0;
			int nJoinType = 1;
			int nWaitOpenGameTimeOut=300;
			string strDataInfo="";
			int nRealGameId = m_oWebComm.PostCheck( pClient->GetGameId(),
								pClient->GetUserId(),nJoinUserCount,nJoinType,
								nWaitOpenGameTimeOut,strDataInfo,POSTJOIN);
			if(nRealGameId>0)
			{
				if(nRealGameId>0)
				{
					pClient->SetGameId(nRealGameId);
					pClient->SetChannelName(nRealGameId);
				}
				// 房主重连
				if(nJoinUserCount==1)
					pClient->SetUserType(OWNER);
				else
					pClient->SetUserType(PLAYER);
				// 锁m_mapChannel，并insert new channel
				//AddChannel(pClient->GetChannelName().c_str());

				// 锁m_mapChannel，并操作用户链表
				AddUser(pClient);
				
				/*TagRedisGame tag;
				CTFRedis::GetInstance()->GetGameCreateTime(pClient->GetChannelName(),tag);
				if(GAMESTATUSBEGIN==tag.nGameStatus)
				{
					// TODO 
				}*/

				// 人满了
				string strIdleUser="";
				int nIdleSize = CTFRedis::GetInstance()->GetIdleUser(pClient->GetChannelName(),"",strIdleUser);
				if(nIdleSize==pClient->GetOpenGameUserCount())
				{
					CTFRedis::GetInstance()->UpdateGameCreateTime(pClient->GetChannelName() );
				}

				// 锁redis，并获取所有用户
				PublishUserIds(pClient->GetUserId(),pClient->GetChannelName().c_str(),NULL,0,strDataInfo);
				nRet = SUCCESS;
			}
			else
			{
				nRet = 0-nRealGameId;
			}			
		}
	}

	return nRet;
}

int CTFCustomerService::ClientGameStart(CTFCustomerClient*pClient)
{
	int nRet = 0;

	TagRedisGame tag;
	CTFRedis::GetInstance()->GetGameTag(pClient->GetChannelName().c_str(),tag);
	if(GAMESTATUSWAIT==tag.nGameStatus)
	{
		// 锁redis，并获取所有用户
		bool bRet = false;
		string strValue;
		bRet = CTFRedis::GetInstance()->GetUserIds(pClient->GetChannelName(),strValue);
		if(bRet)
		{
			Json::Value jGetRoot;
			Json::Reader jReader(Json::Features::strictMode());
			Json::FastWriter jWriter;
			if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
			{
				int UserArray[2];
				int nIdleSize = jGetRoot["UserIds"].size();
				Json::Value jValueUserArray = jGetRoot["UserIds"];
				for(int i=0;i<nIdleSize;i++)
				{
					if(jValueUserArray[i]["UserType"].asInt()==0)
						UserArray[0]=jValueUserArray[i]["UserId"].asInt();
					else
						UserArray[1]=jValueUserArray[i]["UserId"].asInt();
				}
				CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"ClientGameStart : Player NUM=%d\n",nIdleSize);
				if(nIdleSize==pClient->GetOpenGameUserCount())
				{
					int nPostRet = m_oWebComm.PostGameStart(pClient->GetGameId(),
										pClient->GetUserId(),
								 		pClient->GetLiveUserId(),
										pClient->GetLiveId() );
					if(nPostRet>0)
					{
						// 锁redis，并操作GameInfo get/set key 
						sem_wait(&sem_lock);
						CHANNELIT itr = m_mapChannel.find(pClient->GetChannelName());
						if( itr!=m_mapChannel.end() )
						{
							itr->second.nGameStatus = GAMESTATUSBEGIN;
							itr->second.nOwnerId = UserArray[0];
						}
						sem_post(&sem_lock);

						CTFRedis::GetInstance()->UpdateGameInfo( pClient->GetChannelName(),GAMESTATUSBEGIN);
				
						Json::FastWriter jWriter;
						Json::Value jRoot,jGetRoot;
						struct timespec ts_now;
						clock_gettime(CLOCK_REALTIME,&ts_now);
						for(int i=0;i<pClient->GetTowerCount();i++)
						{
							if(i==0)
							{
								jRoot[i]["BotNum"] = 10;
								jRoot[i]["UserId"]=UserArray[0];
								jRoot[i]["Time"]=(int)ts_now.tv_sec;
							}
							else if(i==pClient->GetTowerCount()-1)
							{
								jRoot[i]["BotNum"] = 10;
								jRoot[i]["UserId"]=UserArray[1];
								jRoot[i]["Time"]=(int)ts_now.tv_sec;
							}
							else
							{
								jRoot[i]["BotNum"] = 0;
								jRoot[i]["UserId"]=0;
							}
						}
						jGetRoot["TowerIds"]=jRoot;
						string strValue = jWriter.write(jGetRoot);		
						CTFRedis::GetInstance()->SetTowerIds( pClient->GetChannelName(),strValue);

						PublishTowerIds( pClient->GetChannelName().c_str());	
						nRet = SUCCESS;
					}
					else
					{
						nRet = 0-nPostRet;
					}
				}	
				else
				{
					nRet = ERROR0GameStart0IDLE;
					CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"ProcessStart : Failed nIdleSize=%d, UserId=%d",
									     nIdleSize,pClient->GetUserId());
				}
			}
		}
	}
	else
	{
		nRet = ERROR0NORMAL;
	}
	
	return nRet;
}

int CTFCustomerService::ClientBotMoving(CTFCustomerClient*pClient,TagChg tag)
{
	int nRet = 0;
	/*struct timespec ts_base;
	clock_gettime(CLOCK_REALTIME,&ts_base);
	int nBaseTime = ts_base.tv_sec;
	while(1)
	{
		struct timespec ts_now;
		clock_gettime(CLOCK_REALTIME,&ts_now);
		if(ts_now.tv_sec-nBaseTime>2)
		{
			CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"ClientBotMoving : TimeOut Game=%s",
							    pClient->GetChannelName().c_str());
			break;
		}
		// 加锁
		bool bRet = CTFRedis::GetInstance()->Lock(pClient->GetChannelName());
		if(!bRet)
		{
			usleep(10*1000);
			continue;
		}
		else
		{
			DoBotMoving(pClient,tag);
			// 解锁
			bRet = CTFRedis::GetInstance()->Unlock(pClient->GetChannelName());
			nRet = SUCCESS;
			break;
		}
		
	}*/
	DoBotMoving(pClient,tag);
	nRet = SUCCESS;
	return nRet;
}

int CTFCustomerService::DoBotMoving(CTFCustomerClient*pClient,TagChg tag)
{
	sem_wait(&sem_lock);
	/*string strValue;
	Json::Reader jReader(Json::Features::strictMode());
	Json::FastWriter jWriter;
	Json::Value jGetRoot,jTowerIdArray;
	CTFRedis::GetInstance()->GetTowerIds(pClient->GetChannelName(),strValue);
	if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
	{
		jTowerIdArray=jGetRoot["TowerIds"];
		if(tag.nType==1)
		{//派出
			jTowerIdArray[tag.nSrcTowerId]["BotNum"] = jTowerIdArray[tag.nSrcTowerId]["BotNum"].asInt()-tag.nBotNum;
		}
		
		jGetRoot["TowerIds"]=jTowerIdArray;
		string strSet = jWriter.write(jGetRoot);		
		CTFRedis::GetInstance()->SetTowerIds(pClient->GetChannelName(),strSet);	

		PublishBotMoving( pClient->GetChannelName().c_str(),tag);
		PublishTowerIds( pClient->GetChannelName().c_str(),strSet);
	}*/
	PublishBotMoving( pClient->GetChannelName().c_str(),tag);
	sem_post(&sem_lock);
}

int CTFCustomerService::ClientExitRoom(CTFCustomerClient*pClient)
{
	int nRet = 0;
	
	if(pClient->GetUserType()==VIEWER)
	{// 观众
		// 锁m_mapViewer，并操作观众链表
		DelViewer(pClient,pClient->GetChannelName().c_str());
		nRet = SUCCESS;
	}
	else
	{
		// 获取redis玩家表确认是否玩家
		TagRedisUser tag;
		CTFRedis::GetInstance()->GetUserInfo( pClient->GetChannelName().c_str(),GetServerId(),
							     pClient->GetUserId(),tag);
		if(tag.nType!=USERTYPECOUNT)
		{// 玩家
			// 锁redis，并读取Gameinfo Status
			int nStatus=CTFRedis::GetInstance()->GetGameStatus(pClient->GetChannelName().c_str());
			if(nStatus==GAMESTATUSBEGIN)
			{// 游戏已开始,更新为不在房间
				// 锁redis，并更新Userinfo Status
				bool bRet = CTFRedis::GetInstance()->UpdateUserStatus(pClient->GetChannelName().c_str(),
						GetServerId(),pClient->GetUserId(),-1,0,-1);

				nRet = m_oWebComm.PostUserExitRoom( pClient->GetGameId(),
		                                                    pClient->GetUserId(),true,EXIT_ACTION);
			}
			else
			{// 直接删除
				if(tag.nType==OWNER)
				{// 更换fz
					CTFRedis::GetInstance()->ChangeOwner( pClient->GetChannelName().c_str(),
									      pClient->GetUserId());
				}
				// 锁redis，并更新Userinfo Status
				CTFRedis::GetInstance()->RemoveUserId( pClient->GetChannelName().c_str(),
								       GetServerId(),
		                                                       pClient->GetUserId());
				string strIdleUser;
				int nUserNum = CTFRedis::GetInstance()->GetIdleUser(pClient->GetChannelName().c_str(),"",strIdleUser);
				if(nUserNum==0 && (pClient->GetLastChannelName().length()==0))
					nRet=m_oWebComm.PostGameTimeOut( pClient->GetGameId(),
								         pClient->GetUserId());
				else
					nRet=m_oWebComm.PostUserExitRoom( pClient->GetGameId(),
		                                                          pClient->GetUserId(),false,EXIT_ACTION);
				// 锁redis，并获取所有用户
				PublishUserIds(pClient->GetUserId(),pClient->GetChannelName().c_str(),NULL,0,"",nStatus);
			}
		}
		else
		{
			nRet=0-ERROR0Exit0Check;
		}
		
	}

	return nRet;
}

bool CTFCustomerService::ClientOffLine(CTFCustomerClient*pClient,bool bNotify)
{
	int nRet = 0;
	
	if(pClient->GetUserType()!=VIEWER)
	{
		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"ClientOffLine : UserId=%d",pClient->GetUserId());

		// 获取redis玩家表确认是否玩家
		TagRedisUser tag;
		CTFRedis::GetInstance()->GetUserInfo( pClient->GetChannelName().c_str(),GetServerId(),
							     pClient->GetUserId(),tag);
		if(tag.nType!=USERTYPECOUNT)
		{// 玩家
			// 锁redis，并读取Gameinfo Status
			int nStatus=CTFRedis::GetInstance()->GetGameStatus(pClient->GetChannelName().c_str());
			if(nStatus==GAMESTATUSBEGIN)
			{// 游戏已开始,更新为离线
				// 锁redis，并更新Userinfo Status
				bool bRet = CTFRedis::GetInstance()->UpdateUserStatus( pClient->GetChannelName().c_str(),
											      GetServerId(),pClient->GetUserId(),-1,-1,0);
				if(bNotify)
					m_oWebComm.PostUserExitRoom( pClient->GetGameId(),
		                                                     pClient->GetUserId(),true,EXIT_SOCKET);		
			}
			else
			{// 直接删除
				if(tag.nType==OWNER)
				{// 更换fz
					CTFRedis::GetInstance()->ChangeOwner( pClient->GetChannelName().c_str(),
										     pClient->GetUserId());
				}
				// 锁redis，并更新Userinfo Status
				CTFRedis::GetInstance()->RemoveUserId( pClient->GetChannelName().c_str(),
											     GetServerId(),
		                                                                             pClient->GetUserId());
				if(bNotify)
				{
					string strIdleUser;
					int nUserNum = CTFRedis::GetInstance()->GetIdleUser(pClient->GetChannelName().c_str(),"",strIdleUser);
					if(nUserNum==0&& (pClient->GetLastChannelName().length()==0))
						m_oWebComm.PostGameTimeOut( pClient->GetGameId(),
									    pClient->GetUserId());
					else if(nUserNum>0)
						m_oWebComm.PostUserExitRoom( pClient->GetGameId(),
				                                             pClient->GetUserId(),false,EXIT_SOCKET);
					// 锁redis，并获取所有用户
					PublishUserIds(pClient->GetUserId(),pClient->GetChannelName().c_str(),NULL,0,"",nStatus);
				}
				
			}
		
		
			nRet = SUCCESS;
		}
	}

	return nRet;
}

void CTFCustomerService::AddUser(CTFCustomerClient*pClient)
{
	sem_wait(&sem_lock);
	CHANNELIT itr = m_mapChannel.find(pClient->GetChannelName());
	if( itr==m_mapChannel.end() )
	{/* 频道不存在则订阅频道 */
		TagGame tagGame;
		tagGame.nGameOverTime = 0x7ffffffd;
		tagGame.nGameStatus = 0;
		tagGame.nGameId  = pClient->GetGameId();
		tagGame.strLastChannelName = pClient->GetLastChannelName();
		tagGame.clientList.push_back(pClient);

		// 再来一局///////////////////////////////////
		if(pClient->GetLastChannelName().length()>0)
		{
			// 玩家操作
			CHANNELIT itrM = m_mapChannel.find(pClient->GetLastChannelName());
			if( itrM!=m_mapChannel.end() )
			{
				// 提取玩家
				list<CTFCustomerClient*>::iterator itListFind = itrM->second.clientList.begin();
				while(itListFind != itrM->second.clientList.end())
				{
					if(pClient!= (*itListFind))
					{
						(*itListFind)->SetChannelName(pClient->GetChannelName());
						(*itListFind)->SetGameId(tagGame.nGameId);
						tagGame.clientList.push_back(*itListFind);
					}
					itListFind++;
				}
				// 清除旧的玩家，游戏id
				itrM->second.clientList.clear();
				m_mapChannel.erase(itrM);
				//m_oSubscriber.unsubscribe(pClient->GetLastChannelName());
				sem_wait(&g_pCustomerService->semUnSubQueLock);
				g_pCustomerService->m_queUnSubMsg.push(pClient->GetLastChannelName());
				sem_post(&g_pCustomerService->semUnSubQueLock);
			}
			// 观众操作
			sem_wait(&semViewerLock);
			map<CTFCustomerClient*,CTFCustomerClient*> mapLastTemp;
			map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itrLast = m_mapViewer.find(pClient->GetLastChannelName());
			if( itrLast!=m_mapViewer.end() )
			{  
				// 提取观众
				mapLastTemp = itrLast->second;
				map<CTFCustomerClient*,CTFCustomerClient*>::iterator itrView = mapLastTemp.begin();
				while( itrView!=mapLastTemp.end() )
				{
					itrView->second->SetChannelName(pClient->GetChannelName());
					itrView->second->SetGameId(tagGame.nGameId);
					itrView++;
				}
				// 清除旧的channel观众
				itrLast->second.clear();
				m_mapViewer.erase(itrLast);
			}
			// 把观众加进来
			map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itrNow = m_mapViewer.find(pClient->GetChannelName());
			if( itrNow==m_mapViewer.end() )
			{
				map<CTFCustomerClient*,CTFCustomerClient*> mapTemp;
				mapTemp=mapLastTemp;
				m_mapViewer.insert(VIEWERMAP::value_type(pClient->GetChannelName(),mapTemp));
			}
			sem_post(&semViewerLock);
		}
		///////////////////////////////////////////////////////////////////////////////////

		m_mapChannel.insert(CHANNELMAP::value_type(pClient->GetChannelName(),tagGame));
		SubscribeC(pClient->GetChannelName().c_str());

		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CTFCustomerService : New Channel=%s",
                                                     pClient->GetChannelName().c_str());	
	}
	else
	{
		list<CTFCustomerClient*>::iterator itL = find( itr->second.clientList.begin(),
							     itr->second.clientList.end(),pClient);

		itr->second.nGameId  = pClient->GetGameId();
		itr->second.strLastChannelName = pClient->GetLastChannelName();
		if(itL == itr->second.clientList.end())
		{
			itr->second.clientList.push_back(pClient);
		}
	}
	sem_post(&sem_lock);
	
	// 数据库
	CTFRedis::GetInstance()->AddUserId( pClient->GetChannelName(),GetServerId(),
						   pClient->GetUserId(),pClient->GetUserType(),pClient->GetUserPic(),pClient->GetUserName());
}

string CTFCustomerService::GetServerId()
{
	if(strlen(m_szMac)<=0)
	{
		int                 sockfd;
		struct ifreq        ifr;

		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd >0 ) 
		{
			strcpy(ifr.ifr_name, "eth0");
			if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) >= 0) 
			{
				sprintf(m_szMac, "%02x%02x%02x%02x%02x%02x",
				    (unsigned char)ifr.ifr_hwaddr.sa_data[0],
				    (unsigned char)ifr.ifr_hwaddr.sa_data[1],
				    (unsigned char)ifr.ifr_hwaddr.sa_data[2],
				    (unsigned char)ifr.ifr_hwaddr.sa_data[3],
				    (unsigned char)ifr.ifr_hwaddr.sa_data[4],
				    (unsigned char)ifr.ifr_hwaddr.sa_data[5]
				    );
			}
			close(sockfd);
		}

		if(strlen(m_szMac)==0)
		{
			sprintf(m_szMac,"p%d",getpid());
		}
	}
	
	
	char szServerId[MAX_CHANNELNAME_LEN]={0};
	sprintf(szServerId,"%s_%d",m_szMac,m_uPort);
	return szServerId;
}

bool CTFCustomerService::IsInThisServer(string strTable,string strKey, string strUserId)
{

	bool bRet =false;
	int nStatus = CTFRedis::GetInstance()->GetUserStatus(strTable,strKey,strUserId);
	if(nStatus!=USERSTATUSCOUNT)
	{
		bRet=true;
	}	

	return bRet;
}

void CTFCustomerService::AddViewer(CTFCustomerClient*pClient,const char* szChannelName)
{
	sem_wait(&sem_lock);
	CHANNELIT itrM = m_mapChannel.find(szChannelName);
	if( itrM==m_mapChannel.end() )
	{/* 频道不存在则订阅频道 */
		TagGame tagGame;
		tagGame.nGameOverTime = 0x7ffffffd;
		tagGame.nGameId  = pClient->GetGameId();
		tagGame.nGameStatus = 0;
		m_mapChannel.insert(CHANNELMAP::value_type(szChannelName,tagGame));
		
		SubscribeC(szChannelName);
		//printf("CTFCustomerService::Subscribe %s\n",szChannelName);	
	}
	else
	{
		//printf("CTFCustomerService::Already Subscribe %s\n",szChannelName);
	}
	sem_post(&sem_lock);

	sem_wait(&semViewerLock);
	map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itr = m_mapViewer.find(szChannelName);
	if( itr==m_mapViewer.end() )
	{
		map<CTFCustomerClient*,CTFCustomerClient*> mapTemp;
		mapTemp.insert(map<CTFCustomerClient*,CTFCustomerClient*>::value_type(pClient,pClient));
		m_mapViewer.insert(VIEWERMAP::value_type(szChannelName,mapTemp));
	}
	else
		itr->second.insert(map<CTFCustomerClient*,CTFCustomerClient*>::value_type(pClient,pClient));
	sem_post(&semViewerLock);
}

int CTFCustomerService::DelViewer(CTFCustomerClient*pClient,const char* szChannelName)
{
	int nLeft=0;
	sem_wait(&semViewerLock);
	map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itr = m_mapViewer.find(szChannelName);
	if( itr!=m_mapViewer.end() )
	{
		map<CTFCustomerClient*,CTFCustomerClient*>::iterator itrView = itr->second.find(pClient);
		if( itrView!=itr->second.end() )
		{
			itr->second.erase(itrView);
		}
		nLeft = itr->second.size();
		if(nLeft==0)
		{
			m_mapViewer.erase(itr);
		}
	}
	sem_post(&semViewerLock);
	return nLeft;
}

int CTFCustomerService::DelViewerInAll(CTFCustomerClient*pClient,const char* szChannelName)
{
	sem_wait(&semViewerLock);
	map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itr = m_mapViewer.begin();
	while( itr!=m_mapViewer.end() )
	{
		map<CTFCustomerClient*,CTFCustomerClient*>::iterator itrView = itr->second.find(pClient);
		if( itrView!=itr->second.end() )
		{
			if(itr->first!=szChannelName)
			{
				CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"!!!DelViewerInAll new = %s,old = %s\n",itr->first.c_str(),szChannelName);
			}
			itr->second.erase(itrView);
		}
		itr++;
	}
	sem_post(&semViewerLock);
}

void CTFCustomerService::BroadcastViewer(enumCustomerClientCommand cmdType,string strData,const char* szChannelName)
{
	sem_wait(&semViewerLock);
	map< string,map<CTFCustomerClient*,CTFCustomerClient*> >::iterator itr = m_mapViewer.find(szChannelName);
	if( itr!=m_mapViewer.end() )
	{
		char szTestMsg[5000]={0};
		map<CTFCustomerClient*,CTFCustomerClient*>::iterator itrView = itr->second.begin();
		while( itrView!=itr->second.end() )
		{
			char szTestItem[20]={0};
			sprintf(szTestItem,"%u,",itrView->second);
			strcat(szTestMsg,szTestItem);
			itrView->second->NotifyClient(cmdType,strData);
			itrView++;
		}
		CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"!!! viewerids = %s,szChannelName = %s\n",szTestMsg,szChannelName);
	}
	sem_post(&semViewerLock);
}

void CTFCustomerService::PublishUserIds(int nUserId,const char *szChannelName,const char *szLastChannelName,int nGameId,string strDataInfo,int nGameStatus)
{
	bool bRet = false;
	string strValue;
	bRet = CTFRedis::GetInstance()->GetUserIds(szChannelName,strValue);
	if(bRet)
	{
		Json::Value jGetRoot;
		Json::Reader jReader(Json::Features::strictMode());
		Json::FastWriter jWriter;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			// 加入时间 
			TagRedisGame tag;
			CTFRedis::GetInstance()->GetGameTag(szChannelName,tag);
			if(GAMESTATUSWAIT==tag.nGameStatus)
			{
				struct timespec ts_now;
				clock_gettime(CLOCK_REALTIME,&ts_now);
				int nWaitForStart = tag.nWaitOpenGameTimeOut-((int)ts_now.tv_sec-tag.nCreateTime);
				if(nWaitForStart>=0)
				{
					jGetRoot["WaitForStart"] = nWaitForStart;
				}
			}

			// 加入Data字段
			Json::Value jData;
			Json::Reader jDataReader(Json::Features::strictMode());
			if(jDataReader.parse((const char*)strDataInfo.c_str(),jData))
			{
				jGetRoot["Data"]=jData;
			}

			// 再来一局 加上NextGameId字段
			if(szLastChannelName!=NULL)
			{ 
				jGetRoot["SrcUserId"] = nUserId;
				jGetRoot["NextGameId"] = nGameId;
				strValue = jWriter.write(jGetRoot);
				CTFRedis::GetInstance()->Publish(szChannelName, strValue.c_str());
				CTFRedis::GetInstance()->Publish(szLastChannelName, strValue.c_str());
			
			}
			else
			{ // 普通 
				if(nGameId!=0)
				{// 如果是创建房间，加上GameId字段
				
					jGetRoot["GameId"] = nGameId;
				}
				strValue = jWriter.write(jGetRoot);
				CTFRedis::GetInstance()->Publish(szChannelName, strValue.c_str());
			}
		}
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Error:PublishUserIds GetUserIds fail\n");
	}
}

void CTFCustomerService::PublishTowerIds(const char *szChannelName,string strIds)
{
	bool bRet = false;
	if(strIds.length()==0)
	{
		string strValue;
		bRet = CTFRedis::GetInstance()->GetTowerIds(szChannelName,strValue);
		if(bRet)
		{
			CTFRedis::GetInstance()->Publish(szChannelName, strValue.c_str());
		}
	}
	else
	{
		CTFRedis::GetInstance()->Publish(szChannelName, strIds.c_str());
	}
		
}

void CTFCustomerService::PublishBotMoving(const char *szChannelName,TagChg tag)
{
	string strData = PraseChgTagToJson(tag);

	CTFRedis::GetInstance()->Publish(szChannelName, strData.c_str());
}

void CTFCustomerService::PublishGameOver(const char *szChannelName, int nWinUserId,string strDataInfo)
{
	Json::FastWriter jWriter;
	Json::Value jsonRoot,jData;
	Json::Reader jReader(Json::Features::strictMode());

	jsonRoot["WinUserId"] = nWinUserId;
	
	if(jReader.parse((const char*)strDataInfo.c_str(),jData))
	{
		jsonRoot["Data"]=jData;
	}

	string strData = jWriter.write(jsonRoot);
	CTFRedis::GetInstance()->Publish(szChannelName, strData.c_str());
}
