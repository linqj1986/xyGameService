#include "CustomerService.h"
#include <time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "PrintLog.h"

CCustomerService *g_pCustomerService=NULL;

void GlobalReSubscribeAll(void* p)
{
	pthread_t hThread;
	pthread_create(&hThread,NULL,CCustomerService::ReSubscribeAllThread, (void*)g_pCustomerService);
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

void* CCustomerService::PopSubMsgThread( void *param )
{
	pthread_detach(pthread_self());
	CCustomerService* pParam = (CCustomerService*)param;

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

void* CCustomerService::PopUnsubMsgThread(void *param)
{
	pthread_detach(pthread_self());
	CCustomerService* pParam = (CCustomerService*)param;

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
				/*if(pParam->m_oSubscriber.IsConnected())
				{
					pParam->m_oSubscriber.unsubscribe(strChannel.c_str());
				}*/		
				pParam->m_oSubscriber.UnSubscribe(strChannel);
			}
			sem_post(&(pParam->sem_lock));
		}
		
		usleep(10*1000*1000);
	}
}

void CCustomerService::DoSubscriberMsg(const char *channel_name,const char *message)
{
	sem_wait(&sem_lock);
	Json::FastWriter jWriter;
	Json::Value jRoot;
	Json::Reader jReader(Json::Features::strictMode());
	if(jReader.parse((const char*)message,jRoot))
	{
		// 游戏过程通知
		if((!jRoot["LastUserId"].isNull()) &&(!jRoot["CurUserId"].isNull()))
		{
			char szCurUserId[20]={0};
			sprintf(szCurUserId,"%d",jRoot["CurUserId"].asInt());

    			bool bFound=false;
			// 获取最新数据
			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				list<CCustomerClient*>::iterator itListFind = itrM->second.clientList.begin();
				while(itListFind != itrM->second.clientList.end())
				{
					if( strcmp(szCurUserId, (*itListFind)->GetUserId().c_str())==0 )
					{
						bFound = true;//是本服务器用户
						break;
					}
					itListFind++;
				}
				if(!bFound)
				{
					bFound = IsInThisServer(channel_name,GetServerId(),szCurUserId);
					if(bFound)
					{
						CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"!!!ReCheck In This Server,User=%s!!!",
                                                                                     szCurUserId);
					}
				}
				if(bFound)
				{
					CRedisPublisher::GetInstance()->UpdateUserStatus( channel_name,
											  GetServerId(),szCurUserId,USERROLE,-1);
					itrM->second.bThisServer = true;
					itrM->second.strCurUserId = szCurUserId;
					
					TagRedisGame tag;
					CRedisPublisher::GetInstance()->GetGameTag((string)(itrM->first),tag);
					if(tag.nGameId!=0)
					{
						itrM->second.nRemainTime  = tag.nRemainTime;
						itrM->second.nGameId = tag.nGameId;
					}
				}
				else
				{
					itrM->second.bThisServer = false;
				}

				// 遍历客户端通知用户变更
				list<CCustomerClient*>::iterator itL = itrM->second.clientList.begin();
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
				sprintf(szNextChannelName,"1_%d",nNewGameId);
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
					tagGame.bThisServer = false;
					tagGame.nGameId  = nNewGameId;
					tagGame.nMaxTime = 30*1000;
					tagGame.nMinTime = 20*1000;
					tagGame.strLastChannelName = channel_name;
					
					m_mapChannel.insert(CHANNELMAP::value_type(strNextChannelName.c_str(),tagGame));
					SubscribeC(strNextChannelName.c_str());

					CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"UserIds : New Channel=%s",
						                             strNextChannelName.c_str());	
				}
				
				// 旧局玩家追加入新局中
				list<CCustomerClient*> listLastTemp;
				CHANNELIT itrM = m_mapChannel.find(channel_name);
				if( itrM!=m_mapChannel.end() )
				{
					int nMaxTime = itrM->second.nMaxTime;
					int nMinTime = itrM->second.nMinTime;

					// 提取旧局玩家
					listLastTemp = itrM->second.clientList;
					
					// 把旧局玩家加入新局
					CHANNELIT itrNew = m_mapChannel.find(strNextChannelName.c_str());
					if( itrNew!=m_mapChannel.end() )
					{
						list<CCustomerClient*>::iterator itListFind = listLastTemp.begin();
						while(itListFind != listLastTemp.end())
						{
							list<CCustomerClient*>::iterator itL = find( itrNew->second.clientList.begin(),
								     itrNew->second.clientList.end(),*itListFind);
							if(itL == itrNew->second.clientList.end())
							{
								(*itListFind)->SetChannelName(strNextChannelName.c_str());
								(*itListFind)->SetGameId(nNewGameId);
								itrNew->second.nMaxTime = nMaxTime;
								itrNew->second.nMinTime = nMinTime;
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
				map<CCustomerClient*,CCustomerClient*> mapLastTemp;
				map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itrLast = m_mapViewer.find(channel_name);
				if( itrLast!=m_mapViewer.end() )
				{  
					// 提取旧局观众
					mapLastTemp = itrLast->second;
					
					// 把旧局观众加入新局
					map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itrMapViewer = m_mapViewer.find(strNextChannelName.c_str());
					if( itrMapViewer==m_mapViewer.end() )
					{
						// by linqj
						map<CCustomerClient*,CCustomerClient*>::iterator itrView = mapLastTemp.begin();
						while( itrView!=mapLastTemp.end() )
						{
							itrView->second->SetChannelName(strNextChannelName.c_str());
							itrView->second->SetGameId(nNewGameId);
							itrView++;
						}

						map<CCustomerClient*,CCustomerClient*> mapTemp;
						mapTemp=mapLastTemp;
						m_mapViewer.insert(VIEWERMAP::value_type(strNextChannelName.c_str(),mapTemp));
					}
					else
					{
						map<CCustomerClient*,CCustomerClient*>::iterator itrView = mapLastTemp.begin();
						while( itrView!=mapLastTemp.end() )
						{
							// 新链表中没有，则加入
							map<CCustomerClient*,CCustomerClient*>::iterator itrClient = itrMapViewer->second.find(itrView->second);
							if( itrClient==itrMapViewer->second.end() )
							{  
								itrView->second->SetChannelName(strNextChannelName.c_str());
								itrView->second->SetGameId(nNewGameId);
								itrMapViewer->second.insert( map<CCustomerClient*,
                                                                                             CCustomerClient*>::value_type(itrView->second,itrView->second));
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
							list<CCustomerClient*>::iterator itListFind = itrM->second.clientList.begin();
							while(itListFind != itrM->second.clientList.end())
							{
								if( nUserId== atoi((*itListFind)->GetUserId().c_str()) )
								{
									CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"---------SetReconn2OtherServer user=%d",
			             			     						     nUserId);
									(*itListFind)->SetReconn2OtherServer(true);
									CRedisPublisher::GetInstance()->RemoveUserId( (*itListFind)->GetChannelName().c_str(),
											     GetServerId(),
		                                                                             (*itListFind)->GetUserId().c_str());
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
				list<CCustomerClient*>::iterator itL = itrM->second.clientList.begin();
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
		// 爆雷通知
		if(!jRoot["OverUserId"].isNull())
		{
			CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"-----[Boom]%s,OverUserId=%d,nextid=%d",channel_name,
                                                             jRoot["OverUserId"].asInt(), jRoot["NextUserId"].asInt());

			// 补充更新用户信息为已炸
			char szOverUserId[20]={0};
			sprintf(szOverUserId,"%d",jRoot["OverUserId"].asInt());
			CRedisPublisher::GetInstance()->UpdateUserStatus( channel_name,GetServerId(),
									  szOverUserId,0,-1);

			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				//判断下一个用户是否本服务器
				if(jRoot["NextUserId"].asInt()!=0)
				{
					bool bThisServer = false;
					char szNextUserId[20]={0};
					sprintf(szNextUserId,"%d",jRoot["NextUserId"].asInt());
					list<CCustomerClient*>::iterator itListFind = itrM->second.clientList.begin();
					while(itListFind != itrM->second.clientList.end())
					{
						if( strcmp(szNextUserId, (*itListFind)->GetUserId().c_str())==0 )
						{
							//是本服务器用户.
							bThisServer = true;
							break;
						}
						itListFind++;
					}
					if(!bThisServer)
					{
						bThisServer = IsInThisServer(channel_name,GetServerId(),szNextUserId);
						if(bThisServer)
							CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"!!!ReCheck2 In This Server,User=%s!!!",
                                                                                     	     szNextUserId);
					}
					if(bThisServer)
					{
						itrM->second.bThisServer = true;
						itrM->second.strCurUserId = szNextUserId;
						CRedisPublisher::GetInstance()->UpdateUserStatus( channel_name,
												  GetServerId(),szNextUserId,USERROLE,-1);
						// 重置炸弹时间
						int nRan = 0;
						int nCountSec = itrM->second.nMaxTime-itrM->second.nMinTime;
						if(nCountSec>0)
						{
							srand((int)time(0));
							nRan = rand()%(nCountSec);
						}
				
						itrM->second.nRemainTime  = itrM->second.nMaxTime-nRan; 
					}
				}
				// 通知爆雷
				list<CCustomerClient*>::iterator itL = itrM->second.clientList.begin();
				while( itL != itrM->second.clientList.end() )
				{
					(*itL)->NotifyGameOver(message);
					itL++;
				}
				BroadcastViewer( COMMANDTYPE_S2C_GAMEOVER,message,channel_name);

				// 游戏结束
				if(jRoot["NextUserId"].asInt()==0)
				{
					// 结束
					struct timespec ts_now;
					clock_gettime(CLOCK_REALTIME,&ts_now);
					itrM->second.nGameOverTime = ts_now.tv_sec;
				}
			}
		}
		
	}
	sem_post(&sem_lock);
}

void* CCustomerService::CheckBoomThread( void *param )
{
	pthread_detach(pthread_self());
	CCustomerService* pParam = (CCustomerService*)param;
	pParam->CheckBoom();
}

void CCustomerService::CheckBoom()
{
	while(1)
	{
		usleep(CKBOOM_SLEEP_US);
		sem_wait(&sem_lock);
		CHANNELIT itr = m_mapChannel.begin();
		while( itr!=m_mapChannel.end() )
		{
			if(itr->second.bThisServer)
			{	
				itr->second.nRemainTime-=(int)(CKBOOM_SLEEP_US/1000);
				itr->second.nMaxStayTime-=(int)(CKBOOM_SLEEP_US/1000);
				if(  (itr->second.nRemainTime<=0)
                                   /*||(itr->second.nMaxStayTime<=0)*/
                                  )
				{
					//更新用户信息为已炸
					CRedisPublisher::GetInstance()->UpdateUserStatus(itr->first,GetServerId(),
									itr->second.strCurUserId.c_str(),0,-1);
					//随机获取下一个用户持雷
					string strNextUser="";
					string strIdleUser;
					int nIdleSize = CRedisPublisher::GetInstance()->GetIdleUser( itr->first,
                                                                                                     itr->second.strCurUserId.c_str(),
                                                                                                     strIdleUser);
					if(nIdleSize>1)
					{
						strNextUser = strIdleUser;
					}

					// 删除数据
					int nRan = 0;
					int nCountSec = itr->second.nMaxTime-itr->second.nMinTime;
					if(nCountSec>0)
					{
						srand((int)time(0));
						nRan = rand()%(nCountSec);
					}
					
					itr->second.nRemainTime  = itr->second.nMaxTime-nRan; 
					itr->second.nMaxStayTime = MAX_STAY_TIME;
					itr->second.bThisServer=false;
					
					bool bRet = CRedisPublisher::GetInstance()->UpdateGameInfo(
							itr->first,
							strNextUser.c_str(),
							itr->second.strCurUserId,
							itr->second.nRemainTime,-1);

					string strDataInfo="";
					vector<int> vec;
					vec.empty();
					if(nIdleSize<=1)
					{// 游戏结束清理
						// 先插入第一名
						vec.push_back(atoi(strIdleUser.c_str()));
						//获取排名
						CRedisPublisher::GetInstance()->GetOrderList(itr->first,vec);
						
						int nRet = m_oWebComm.PostBomb( itr->second.nGameId,
										atoi(itr->second.strCurUserId.c_str()),1,true,vec,strDataInfo);

						CRedisPublisher::GetInstance()->DeleteGameInfo(itr->first);
						CRedisPublisher::GetInstance()->DeleteUserInfo(itr->first);
						CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"=======GameOver_%s,OverUserId=%s======",
                                                                                     itr->first.c_str(),itr->second.strCurUserId.c_str());
					}
					else
					{
						int nRet = m_oWebComm.PostBomb( itr->second.nGameId,
										atoi(itr->second.strCurUserId.c_str()),1,false,vec,strDataInfo);
					}
					PublishBoom(itr->first.c_str(),itr->second.strCurUserId.c_str(),strNextUser.c_str(),strDataInfo);
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

CCustomerService::CCustomerService(void)
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

	pthread_t hCheckBoomThread;
	pthread_create(&hCheckBoomThread,NULL,CheckBoomThread, (void*)this);

	pthread_t hPopSubMsgThread;
	pthread_create(&hPopSubMsgThread,NULL,CCustomerService::PopSubMsgThread, (void*)g_pCustomerService);

	pthread_t hPopUnSubMsgThread;
	pthread_create(&hPopUnSubMsgThread,NULL,CCustomerService::PopUnsubMsgThread, (void*)g_pCustomerService);

	
}

CCustomerService::~CCustomerService(void)
{
	Stop();
	sem_destroy(&sem_lock);
	sem_destroy(&semAllSockLock);
	sem_destroy(&semViewerLock);
}

int CCustomerService::OnNewSocket( int sock, const char* pszIp )
{
	if ( sock )
	{
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"new sock=%d\n",sock);
		CCustomerClient* pCustomerClient = new CCustomerClient((void*)this,pszIp);		
		sem_wait(&semAllSockLock);
		m_vecCustomerClient.push_back( pCustomerClient );
		pCustomerClient->Init(sock);
		sem_post(&semAllSockLock);
	}
	return 1;
}

unsigned int CCustomerService::Run( unsigned int uPort, int bAutoAddPort )
{
	return CDataService::Run(uPort, bAutoAddPort);
}

void CCustomerService::Stop()
{
	CDataService::Stop();
}

void* CCustomerService::CheckAliveThread( void *param )
{
	pthread_detach(pthread_self());
	CCustomerService* pParam = (CCustomerService*)param;

	// 首次web连接测试
	sleep(1);
	pParam->m_oWebComm.PostUserExitRoom(0,0,false,2);

	while(1)
	{
		sleep(50);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"CheckAlive..........");
		pParam->CheckAlive();
	}
}

bool CCustomerService::IsClientReConn(CCustomerClient*pClient)
{
	bool bRet = false;
	sem_wait(&semAllSockLock);
	std::vector<CCustomerClient*>::iterator itr = m_vecCustomerClient.begin();
	while( itr != m_vecCustomerClient.end() )
	{
		CCustomerClient* pCustomerClient = (CCustomerClient*)*itr;
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

void CCustomerService::CheckAlive()
{
	sem_wait(&semAllSockLock);
	std::vector<CCustomerClient*>::iterator itr = m_vecCustomerClient.begin();
	while( itr != m_vecCustomerClient.end() )
	{
		CCustomerClient* pCustomerClient = (CCustomerClient*)*itr;
		if (  (!pCustomerClient->IsAlive())
		    /*||(!pCustomerClient->IsValid())*/
		   )
		{
			// szUserId有可能为空，因为连上socket了，但是还未握手就掉线
			CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CheckAlive : Close %s,this=%u\n",pCustomerClient->GetUserId().c_str()
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
				list<CCustomerClient*>::iterator itL = find( itrM->second.clientList.begin(),
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
					CRedisPublisher::GetInstance()->GetGameTag(pCustomerClient->GetChannelName(),tag);
					if(GAMESTATUSWAIT==tag.nGameStatus)
					{//游戏未开始

						// 本服务器已无观众和用户
						//m_oSubscriber.unsubscribe(pCustomerClient->GetChannelName());
						sem_wait(&g_pCustomerService->semUnSubQueLock);
						g_pCustomerService->m_queUnSubMsg.push(pCustomerClient->GetChannelName());
						sem_post(&g_pCustomerService->semUnSubQueLock);

						m_mapChannel.erase(itrM);

						// 判断redis中所有用户是否都离线了
						bool bAllOff = CRedisPublisher::GetInstance()->IsAllOffLine(pCustomerClient->GetChannelName());
						if(bAllOff)
						{
							CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CheckALive : PostGameTimeOut %s\n",
		                                                                             pCustomerClient->GetUserId().c_str());
							m_oWebComm.PostGameTimeOut( atoi(pCustomerClient->GetGameId().c_str()),
								    		    atoi(pCustomerClient->GetUserId().c_str()));
						
							CRedisPublisher::GetInstance()->DeleteUserInfo(pCustomerClient->GetChannelName());
							CRedisPublisher::GetInstance()->DeleteGameInfo(pCustomerClient->GetChannelName());
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
				list<CCustomerClient*>::iterator itL = find( itrALLM->second.clientList.begin(),
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

void CCustomerService::SubscribeC(const char* szChannelName)
{
	/*if(!m_oSubscriber.IsConnected())
	{
		CRedisSubscriber::NotifyMessageFn fn = bind( recieve_message, 
							     std::tr1::placeholders::_1,
							     std::tr1::placeholders::_2, 
							     std::tr1::placeholders::_3);
		
		CRedisSubscriber::NotifyReSubscribeFn fnReSubscribe = bind(GlobalReSubscribeAll,std::tr1::placeholders::_1);
		m_oSubscriber.init(fn,fnReSubscribe);

		bool ret = m_oSubscriber.connect();
		if (!ret)
		{
			CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"SubscribeC Connect failed.");
			//sem_post(&sem_lock);
			return ;
		}

		//等待redis成功连接
		struct timespec ts_now;
		clock_gettime(CLOCK_REALTIME,&ts_now);

	    	ts_now.tv_sec+=2;
	    	int nTimeout=sem_timedwait(&(m_oSubscriber.moSemConn),&ts_now);
	}
	if(m_oSubscriber.IsConnected())
	{
		m_oSubscriber.subscribe(szChannelName);
	}*/
	m_oSubscriber.Init(recieve_message);
	m_oSubscriber.Subscribe(szChannelName);
}

void* CCustomerService::ReSubscribeAllThread( void *param )
{
	usleep(1000*1000);
	pthread_detach(pthread_self());
	/*CCustomerService* pParam = (CCustomerService*)param;
	pParam->ReSubscribeAll();*/
}

void CCustomerService::ReSubscribeAll()
{
	sem_wait(&sem_lock);
        CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"!!!!!!    CCustomerService::DoReSubscribeAll    !!!!!!");
	CHANNELIT itr = m_mapChannel.begin();
	while( itr!=m_mapChannel.end() )
	{
		SubscribeC(itr->first.c_str());
		itr++;
	}
	sem_post(&sem_lock);
}

void CCustomerService::AddChannel(const char* szChannelName)
{
	sem_wait(&sem_lock);
	CHANNELIT itr = m_mapChannel.find(szChannelName);
	if( itr==m_mapChannel.end() )
	{/* 频道不存在则订阅频道 */
		TagGame tagGame;
		tagGame.nGameOverTime = 0x7ffffffd;
		tagGame.bThisServer = false;
		m_mapChannel.insert(CHANNELMAP::value_type(szChannelName,tagGame));
		
		SubscribeC(szChannelName);
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"CCustomerService::Subscribe %s\n",szChannelName);	
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"CCustomerService::Already Subscribe %s\n",szChannelName);
	}
	sem_post(&sem_lock);
}

int CCustomerService::ClientCreateRoom(CCustomerClient*pClient)
{
	int nRet = ERROR0NORMAL;
	// 锁redis，并操作GameInfo get key
	// 锁redis，并获取所有用户并判断是否存在过
	//int nGameStatus = CRedisPublisher::GetInstance()->GetGameStatus(pClient->GetChannelName());
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
		nUpLoadId = atoi(pClient->GetGameId().c_str());
		#endif
		nRealGameId = m_oWebComm.PostCheck( nUpLoadId,
						    atoi(pClient->GetUserId().c_str()),nJoinUserCount,nJoinType,nWaitOpenGameTimeOut,strDataInfo,
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
					CRedisPublisher::GetInstance()->CreateGameInfo( pClient->GetChannelName(),pClient->GetUserId(),
											pClient->GetGameId(),nWaitOpenGameTimeOut,
											pClient->GetMaxRemainTime(),pClient->GetMaxStayTime());
				}
				// 锁redis，并获取所有用户
				if(pClient->GetLastChannelName().length()>0)
				{// 再来一局
					PublishUserIds( atoi(pClient->GetUserId().c_str()),
							pClient->GetChannelName().c_str(),
                                                        pClient->GetLastChannelName().c_str(),
							atoi(pClient->GetGameId().c_str()),
                                                        strDataInfo);
				}
				else
				{// 普通创建房间
					PublishUserIds( atoi(pClient->GetUserId().c_str()),pClient->GetChannelName().c_str(),
                                                        NULL,
							atoi(pClient->GetGameId().c_str()),
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

int CCustomerService::ClientHandShake(CCustomerClient*pClient)
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
		bool bRet = CRedisPublisher::GetInstance()->GetUserIds(pClient->GetChannelName().c_str(),strValue);
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
		//int nGameStatus = CRedisPublisher::GetInstance()->GetGameStatus(pClient->GetChannelName());
		if(  //(nGameStatus==GAMESTATUSWAIT)
                     //||(IsValidConnect(pClient->GetChannelName(),pClient->GetUserId())) 
		     1	
                  )
		{ 
			int nJoinUserCount=0;
			int nJoinType = 1;
			int nWaitOpenGameTimeOut=300;
			string strDataInfo="";
			int nRealGameId = m_oWebComm.PostCheck( atoi(pClient->GetGameId().c_str()),
								atoi(pClient->GetUserId().c_str()),nJoinUserCount,nJoinType,
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
				CRedisPublisher::GetInstance()->GetGameCreateTime(pClient->GetChannelName(),tag);
				if(GAMESTATUSBEGIN==tag.nGameStatus)
				{
					// TODO 
				}*/

				// 人满了
				string strIdleUser="";
				int nIdleSize = CRedisPublisher::GetInstance()->GetIdleUser(pClient->GetChannelName(),"",strIdleUser);
				if(nIdleSize==pClient->GetOpenGameUserCount())
				{
					CRedisPublisher::GetInstance()->UpdateGameCreateTime(pClient->GetChannelName() );
				}

				// 锁redis，并获取所有用户
				PublishUserIds(atoi(pClient->GetUserId().c_str()),pClient->GetChannelName().c_str(),NULL,0,strDataInfo);
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

int CCustomerService::ClientGameStart(CCustomerClient*pClient)
{
	int nRet = 0;
	/*TagRedisUser tag;
	CRedisPublisher::GetInstance()->GetUserInfo( pClient->GetChannelName().c_str(),GetServerId(),
						     pClient->GetUserId().c_str(),tag);
	if(tag.nType==OWNER)*/
	TagRedisGame tag;
	CRedisPublisher::GetInstance()->GetGameTag(pClient->GetChannelName().c_str(),tag);
	if(GAMESTATUSWAIT==tag.nGameStatus)
	{
		// 锁redis，并获取所有用户
		string strIdleUser;
		int nIdleSize = CRedisPublisher::GetInstance()->GetIdleUser(pClient->GetChannelName(),"",strIdleUser);
		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"ClientGameStart : Player NUM=%d\n",nIdleSize);
		if(nIdleSize==pClient->GetOpenGameUserCount())
		{
			int nPostRet = m_oWebComm.PostGameStart(atoi(pClient->GetGameId().c_str()),
								atoi(pClient->GetUserId().c_str()),
						 		atoi(pClient->GetLiveUserId().c_str()),
								atoi(pClient->GetLiveId().c_str()) );
			if(nPostRet>0)
			{
				// 锁redis，并操作GameInfo get/set key
				int nRan = 0;
				int nCountSec = pClient->GetMaxRemainTime()-pClient->GetMinRemainTime();
				if(nCountSec>0)
				{
					srand((int)time(0));
					nRan = rand()%(nCountSec);
				}
				
				int nRemainTime  = pClient->GetMaxRemainTime()-nRan; 
				CRedisPublisher::GetInstance()->UpdateGameInfo( pClient->GetChannelName(), 
										strIdleUser.c_str(),"", 
										nRemainTime,GAMESTATUSBEGIN);

				PublishRoleChg( pClient->GetChannelName().c_str(),
						"",
						strIdleUser.c_str());	
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
			CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"ProcessStart : Failed nIdleSize=%d, UserId=%s",
							     nIdleSize,pClient->GetUserId().c_str());
		}
	}
	else
	{
		nRet = ERROR0NORMAL;
	}
	
	return nRet;
}

int CCustomerService::ClientRoleChg(CCustomerClient*pClient,string strDestUserId)
{
	int nRet = 0;
	sem_wait(&sem_lock);
	// 锁redis，并读取Userinfo Status
	// 锁redis，并读取Userinfo Status
	// 锁redis，并读取Gameinfo CurUser
	if( (!IsBoom(pClient->GetChannelName(),GetServerId(),pClient->GetUserId()) )
	&&  (!IsBoom(pClient->GetChannelName(),"",strDestUserId.c_str()) )
	&&  (IsTheRole(pClient->GetChannelName(),pClient->GetUserId()))
	)
	{
		// 锁redis，并更新Userinfo Status
		bool bRet = CRedisPublisher::GetInstance()->UpdateUserStatus(pClient->GetChannelName(),
				GetServerId(),pClient->GetUserId(),USERIDLE,-1);
		if(bRet)
		{
			// 锁redis，并更新Gameinfo CurUser
			int nLeft = GetTimeLeftAndStopBoomCheck(pClient->GetChannelName().c_str());
			CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"game=%s,TIMELEFT=%d",pClient->GetChannelName().c_str(),nLeft);
			bool bRet = CRedisPublisher::GetInstance()->UpdateGameInfo(
					pClient->GetChannelName(),
					strDestUserId.c_str(),"",
					nLeft,-1);		
			if(bRet)
			{
				PublishRoleChg( pClient->GetChannelName().c_str(),pClient->GetUserId().c_str(),
						strDestUserId.c_str());
				nRet = SUCCESS;
			}
		}	
	}
	else
	{
		nRet = ERROR0RoleChg0Check;
		CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"ProcessGameCommand Failed: UserId=%s,DestUserId=%s",
						     pClient->GetUserId().c_str(),strDestUserId.c_str());
	}
	sem_post(&sem_lock);
	return nRet;
}

int CCustomerService::ClientExitRoom(CCustomerClient*pClient)
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
		CRedisPublisher::GetInstance()->GetUserInfo( pClient->GetChannelName().c_str(),GetServerId(),
							     pClient->GetUserId().c_str(),tag);
		if(tag.nType!=USERTYPECOUNT)
		{// 玩家
			// 锁redis，并读取Gameinfo Status
			int nStatus=CRedisPublisher::GetInstance()->GetGameStatus(pClient->GetChannelName().c_str());
			if(nStatus==GAMESTATUSBEGIN)
			{// 游戏已开始,更新为不在房间
				// 锁redis，并更新Userinfo Status
				bool bRet = CRedisPublisher::GetInstance()->UpdateUserStatus(pClient->GetChannelName().c_str(),
						GetServerId(),pClient->GetUserId().c_str(),-1,0,-1);

				nRet = m_oWebComm.PostUserExitRoom( atoi(pClient->GetGameId().c_str()),
		                                                     atoi(pClient->GetUserId().c_str()),true,EXIT_ACTION);
			}
			else
			{// 直接删除
				if(tag.nType==OWNER)
				{// 更换fz
					CRedisPublisher::GetInstance()->ChangeOwner( pClient->GetChannelName().c_str(),
										     pClient->GetUserId().c_str());
				}
				// 锁redis，并更新Userinfo Status
				CRedisPublisher::GetInstance()->RemoveUserId( pClient->GetChannelName().c_str(),
											     GetServerId(),
		                                                                             pClient->GetUserId().c_str());
				string strIdleUser;
				int nUserNum = CRedisPublisher::GetInstance()->GetIdleUser(pClient->GetChannelName().c_str(),"",strIdleUser);
				if(nUserNum==0 && (pClient->GetLastChannelName().length()==0))
					nRet=m_oWebComm.PostGameTimeOut( atoi(pClient->GetGameId().c_str()),
								    atoi(pClient->GetUserId().c_str()));
				else
					nRet=m_oWebComm.PostUserExitRoom( atoi(pClient->GetGameId().c_str()),
		                                                     atoi(pClient->GetUserId().c_str()),false,EXIT_ACTION);
				// 锁redis，并获取所有用户
				PublishUserIds(atoi(pClient->GetUserId().c_str()),pClient->GetChannelName().c_str(),NULL,0,"",nStatus);
			}
		}
		else
		{
			nRet=0-ERROR0Exit0Check;
		}
		
	}

	return nRet;
}

bool CCustomerService::ClientOffLine(CCustomerClient*pClient,bool bNotify)
{
	int nRet = 0;
	
	if(pClient->GetUserType()!=VIEWER)
	{
		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"ClientOffLine : UserId=%s",pClient->GetUserId().c_str());

		// 获取redis玩家表确认是否玩家
		TagRedisUser tag;
		CRedisPublisher::GetInstance()->GetUserInfo( pClient->GetChannelName().c_str(),GetServerId(),
							     pClient->GetUserId().c_str(),tag);
		if(tag.nType!=USERTYPECOUNT)
		{// 玩家
			// 锁redis，并读取Gameinfo Status
			int nStatus=CRedisPublisher::GetInstance()->GetGameStatus(pClient->GetChannelName().c_str());
			if(nStatus==GAMESTATUSBEGIN)
			{// 游戏已开始,更新为离线
				// 锁redis，并更新Userinfo Status
				bool bRet = CRedisPublisher::GetInstance()->UpdateUserStatus( pClient->GetChannelName().c_str(),
											      GetServerId(),pClient->GetUserId().c_str(),-1,-1,0);
				if(bNotify)
					m_oWebComm.PostUserExitRoom( atoi(pClient->GetGameId().c_str()),
		                                                     atoi(pClient->GetUserId().c_str()),true,EXIT_SOCKET);		
			}
			else
			{// 直接删除
				if(tag.nType==OWNER)
				{// 更换fz
					CRedisPublisher::GetInstance()->ChangeOwner( pClient->GetChannelName().c_str(),
										     pClient->GetUserId().c_str());
				}
				// 锁redis，并更新Userinfo Status
				CRedisPublisher::GetInstance()->RemoveUserId( pClient->GetChannelName().c_str(),
											     GetServerId(),
		                                                                             pClient->GetUserId().c_str());
				if(bNotify)
				{
					string strIdleUser;
					int nUserNum = CRedisPublisher::GetInstance()->GetIdleUser(pClient->GetChannelName().c_str(),"",strIdleUser);
					if(nUserNum==0&& (pClient->GetLastChannelName().length()==0))
						m_oWebComm.PostGameTimeOut( atoi(pClient->GetGameId().c_str()),
									    atoi(pClient->GetUserId().c_str()));
					else if(nUserNum>0)
						m_oWebComm.PostUserExitRoom( atoi(pClient->GetGameId().c_str()),
				                                             atoi(pClient->GetUserId().c_str()),false,EXIT_SOCKET);
					// 锁redis，并获取所有用户
					PublishUserIds(atoi(pClient->GetUserId().c_str()),pClient->GetChannelName().c_str(),NULL,0,"",nStatus);
				}
				
			}
		
		
			nRet = SUCCESS;
		}
	}

	return nRet;
}

void CCustomerService::AddUser(CCustomerClient*pClient)
{
	sem_wait(&sem_lock);
	CHANNELIT itr = m_mapChannel.find(pClient->GetChannelName());
	if( itr==m_mapChannel.end() )
	{/* 频道不存在则订阅频道 */
		TagGame tagGame;
		tagGame.nGameOverTime = 0x7ffffffd;
		tagGame.bThisServer = false;
		tagGame.nGameId  = atoi(pClient->GetGameId().c_str());
		tagGame.nMaxTime = pClient->GetMaxRemainTime();
		tagGame.nMinTime = pClient->GetMinRemainTime();
		tagGame.nRemainTime = pClient->GetMaxRemainTime();
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
				list<CCustomerClient*>::iterator itListFind = itrM->second.clientList.begin();
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
			map<CCustomerClient*,CCustomerClient*> mapLastTemp;
			map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itrLast = m_mapViewer.find(pClient->GetLastChannelName());
			if( itrLast!=m_mapViewer.end() )
			{  
				// 提取观众
				mapLastTemp = itrLast->second;
				map<CCustomerClient*,CCustomerClient*>::iterator itrView = mapLastTemp.begin();
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
			map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itrNow = m_mapViewer.find(pClient->GetChannelName());
			if( itrNow==m_mapViewer.end() )
			{
				map<CCustomerClient*,CCustomerClient*> mapTemp;
				mapTemp=mapLastTemp;
				m_mapViewer.insert(VIEWERMAP::value_type(pClient->GetChannelName(),mapTemp));
			}
			sem_post(&semViewerLock);
		}
		///////////////////////////////////////////////////////////////////////////////////

		m_mapChannel.insert(CHANNELMAP::value_type(pClient->GetChannelName(),tagGame));
		SubscribeC(pClient->GetChannelName().c_str());

		CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CCustomerService : New Channel=%s",
                                                     pClient->GetChannelName().c_str());	
	}
	else
	{
		list<CCustomerClient*>::iterator itL = find( itr->second.clientList.begin(),
							     itr->second.clientList.end(),pClient);

		itr->second.nGameId  = atoi(pClient->GetGameId().c_str());
		itr->second.nMaxTime = pClient->GetMaxRemainTime();
		itr->second.nMinTime = pClient->GetMinRemainTime();
		itr->second.nRemainTime = pClient->GetMaxRemainTime();
		itr->second.strLastChannelName = pClient->GetLastChannelName();
		if(itL == itr->second.clientList.end())
		{
			itr->second.clientList.push_back(pClient);
		}
	}
	sem_post(&sem_lock);
	
	// 数据库
	CRedisPublisher::GetInstance()->AddUserId( pClient->GetChannelName(),GetServerId(),
						   pClient->GetUserId(),pClient->GetUserType(),pClient->GetUserPic(),pClient->GetUserName());
}

int CCustomerService::GetTimeLeftAndStopBoomCheck(const char *channel_name)
{
	int nTimeLeft = 0;
	
	CHANNELIT itr = m_mapChannel.find(channel_name);
	if( itr!=m_mapChannel.end() )
	{
		int nTimeLeftRedis = itr->second.nMaxTime;

		// 换雷，暂停检测
		itr->second.bThisServer = false;
		nTimeLeft = itr->second.nRemainTime;
	}
	
	return nTimeLeft;
}

string CCustomerService::GetServerId()
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

bool CCustomerService::IsBoom(string strTable,string strKey, string strUserId)
{
	bool bRet =false;
	int nStatus = CRedisPublisher::GetInstance()->GetUserStatus(strTable,strKey,strUserId);
	if(nStatus==USERBOOM)
	{
		bRet=true;
		CPrintLog::GetInstance()->LogFormat(LOG_WARNING,"IsBoom : %s Already Boom",strUserId.c_str());
	}	
	return bRet;
}

bool CCustomerService::IsInThisServer(string strTable,string strKey, string strUserId)
{

	bool bRet =false;
	int nStatus = CRedisPublisher::GetInstance()->GetUserStatus(strTable,strKey,strUserId);
	if(nStatus!=USERSTATUSCOUNT)
	{
		bRet=true;
	}	

	return bRet;
}

bool CCustomerService::IsTheRole(string strTable,string strUserId)
{
	bool bFound =false;

	TagRedisGame tag;
	CRedisPublisher::GetInstance()->GetGameTag(strTable,tag);
	if(tag.nGameId!=0)
	{
		if( atoi(strUserId.c_str()) == tag.nCurUserId )
			bFound = true;
	}

	if(!bFound)
	{
		CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"%s Not The Role: CurUserId=%d",
						     strUserId.c_str(),tag.nCurUserId);
	}
	return bFound;
}

bool CCustomerService::IsValidConnect(string strTable,string strUserId)
{
	sem_wait(&sem_lock);
	
	bool bValid =false;
	string strCurUserId = "";
	string strValue="";
	bool bRet = CRedisPublisher::GetInstance()->GetUserIds(strTable,strValue);		
	if(bRet)
	{
		Json::Reader jReader(Json::Features::strictMode());
		Json::Value jGetRoot;
		Json::Value jUserIdsArray;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			jUserIdsArray = jGetRoot["UserIds"];
			for(int j=0;j<jUserIdsArray.size();j++)
			{
				if((strUserId == jUserIdsArray[j]["UserId"].asString())	)
				{// reconn
					CPrintLog::GetInstance()->LogFormat( LOG_WARNING,"IsValidConnect : ReConnId=%s,Game=%s",
									     strUserId.c_str(),strTable.c_str());
					bValid=true;
				}
			}
		}
		else if(strValue.length()==0)
		{// 列表为空
			CPrintLog::GetInstance()->LogFormat(LOG_NORMAL,"GetUserIds = 0\n");
			bValid=true;
		}
			
	}
	
	sem_post(&sem_lock);
	return bValid;
}

void CCustomerService::AddViewer(CCustomerClient*pClient,const char* szChannelName)
{
	sem_wait(&sem_lock);
	CHANNELIT itrM = m_mapChannel.find(szChannelName);
	if( itrM==m_mapChannel.end() )
	{/* 频道不存在则订阅频道 */
		TagGame tagGame;
		tagGame.nGameOverTime = 0x7ffffffd;
		tagGame.nGameId  = atoi(pClient->GetGameId().c_str());
		tagGame.bThisServer = false;
		m_mapChannel.insert(CHANNELMAP::value_type(szChannelName,tagGame));
		
		SubscribeC(szChannelName);
		//printf("CCustomerService::Subscribe %s\n",szChannelName);	
	}
	else
	{
		//printf("CCustomerService::Already Subscribe %s\n",szChannelName);
	}
	sem_post(&sem_lock);

	sem_wait(&semViewerLock);
	map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itr = m_mapViewer.find(szChannelName);
	if( itr==m_mapViewer.end() )
	{
		map<CCustomerClient*,CCustomerClient*> mapTemp;
		mapTemp.insert(map<CCustomerClient*,CCustomerClient*>::value_type(pClient,pClient));
		m_mapViewer.insert(VIEWERMAP::value_type(szChannelName,mapTemp));
	}
	else
		itr->second.insert(map<CCustomerClient*,CCustomerClient*>::value_type(pClient,pClient));
	sem_post(&semViewerLock);
}

int CCustomerService::DelViewer(CCustomerClient*pClient,const char* szChannelName)
{
	int nLeft=0;
	sem_wait(&semViewerLock);
	map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itr = m_mapViewer.find(szChannelName);
	if( itr!=m_mapViewer.end() )
	{
		map<CCustomerClient*,CCustomerClient*>::iterator itrView = itr->second.find(pClient);
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

int CCustomerService::DelViewerInAll(CCustomerClient*pClient,const char* szChannelName)
{
	sem_wait(&semViewerLock);
	map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itr = m_mapViewer.begin();
	while( itr!=m_mapViewer.end() )
	{
		map<CCustomerClient*,CCustomerClient*>::iterator itrView = itr->second.find(pClient);
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

void CCustomerService::BroadcastViewer(enumCustomerClientCommand cmdType,string strData,const char* szChannelName)
{
	sem_wait(&semViewerLock);
	map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itr = m_mapViewer.find(szChannelName);
	if( itr!=m_mapViewer.end() )
	{
		char szTestMsg[5000]={0};
		map<CCustomerClient*,CCustomerClient*>::iterator itrView = itr->second.begin();
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

void CCustomerService::PublishUserIds(int nUserId,const char *szChannelName,const char *szLastChannelName,int nGameId,string strDataInfo,int nGameStatus)
{
	bool bRet = false;
	string strValue;
	bRet = CRedisPublisher::GetInstance()->GetUserIds(szChannelName,strValue);
	if(bRet)
	{
		Json::Value jGetRoot;
		Json::Reader jReader(Json::Features::strictMode());
		Json::FastWriter jWriter;
		if(jReader.parse((const char*)strValue.c_str(),jGetRoot))
		{
			// 加入时间 
			TagRedisGame tag;
			CRedisPublisher::GetInstance()->GetGameTag(szChannelName,tag);
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
				CRedisPublisher::GetInstance()->Publish(szChannelName, strValue.c_str());
				CRedisPublisher::GetInstance()->Publish(szLastChannelName, strValue.c_str());
			
			}
			else
			{ // 普通 
				if(nGameId!=0)
				{// 如果是创建房间，加上GameId字段
				
					jGetRoot["GameId"] = nGameId;
				}
				strValue = jWriter.write(jGetRoot);
				CRedisPublisher::GetInstance()->Publish(szChannelName, strValue.c_str());
			}
		}
	}
	else
	{
		CPrintLog::GetInstance()->LogFormat(LOG_ERROR,"Error:PublishUserIds GetUserIds fail\n");
	}
}

void CCustomerService::PublishRoleChg(const char *szChannelName, const char *szLastUserId, const char *szCurUserId)
{
	Json::FastWriter jWriter;
	Json::Value jsonRoot;
	jsonRoot["LastUserId"] = atoi(szLastUserId);
	jsonRoot["CurUserId"] = atoi(szCurUserId);
	string strData = jWriter.write(jsonRoot);
	CRedisPublisher::GetInstance()->Publish(szChannelName, strData.c_str());	
}

void CCustomerService::PublishBoom(const char *szChannelName, const char *szOverUserId, const char *szNextUserId,string strDataInfo)
{
	Json::FastWriter jWriter;
	Json::Value jsonRoot,jData;
	Json::Reader jReader(Json::Features::strictMode());

	jsonRoot["OverUserId"] = atoi(szOverUserId);
	jsonRoot["NextUserId"] = atoi(szNextUserId);
	
	if(jReader.parse((const char*)strDataInfo.c_str(),jData))
	{
		jsonRoot["Data"]=jData;
	}

	string strData = jWriter.write(jsonRoot);
	CRedisPublisher::GetInstance()->Publish(szChannelName, strData.c_str());
}
