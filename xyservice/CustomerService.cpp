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
	// redis���Ļص�
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
		// ��Ϸ����֪ͨ
		if((!jRoot["LastUserId"].isNull()) &&(!jRoot["CurUserId"].isNull()))
		{
			char szCurUserId[20]={0};
			sprintf(szCurUserId,"%d",jRoot["CurUserId"].asInt());

    			bool bFound=false;
			// ��ȡ��������
			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				list<CCustomerClient*>::iterator itListFind = itrM->second.clientList.begin();
				while(itListFind != itrM->second.clientList.end())
				{
					if( strcmp(szCurUserId, (*itListFind)->GetUserId().c_str())==0 )
					{
						bFound = true;//�Ǳ��������û�
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

				// �����ͻ���֪ͨ�û����
				list<CCustomerClient*>::iterator itL = itrM->second.clientList.begin();
				while( itL != itrM->second.clientList.end() )
				{
					(*itL)->NotifyGameRoleChange(message);
					itL++;
				}
				BroadcastViewer( COMMANDTYPE_S2C_GAMECOMMANDRET,message,channel_name);
			}
		}
		// �����߹㲥֪ͨ
		if(!jRoot["UserIds"].isNull())
		{
			bool bNormal = true;
			string strChannelName=channel_name;
			string strNextChannelName=channel_name;
			int nNewGameId = 0;
			// TODO����һ�ֵĹ㲥
			if((!jRoot["NextGameId"].isNull()) && (jRoot["NextGameId"].isInt()))
			{
				// �ж�����ϷƵ���Ƿ���ڣ���������Ϊ����������֪ͨ�����ģ������¾ָ���
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
				{/* ��Ƶ������������Ƶ�� */
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
				
				// �ɾ����׷�����¾���
				list<CCustomerClient*> listLastTemp;
				CHANNELIT itrM = m_mapChannel.find(channel_name);
				if( itrM!=m_mapChannel.end() )
				{
					int nMaxTime = itrM->second.nMaxTime;
					int nMinTime = itrM->second.nMinTime;

					// ��ȡ�ɾ����
					listLastTemp = itrM->second.clientList;
					
					// �Ѿɾ���Ҽ����¾�
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

					// ����ɵ���ң���Ϸid
					itrM->second.clientList.clear();
					m_mapChannel.erase(itrM);

					//m_oSubscriber.unsubscribe(channel_name);
					sem_wait(&g_pCustomerService->semUnSubQueLock);
					g_pCustomerService->m_queUnSubMsg.push(channel_name);
					sem_post(&g_pCustomerService->semUnSubQueLock);
				}

				// �ɾֹ���׷�����¾���
				sem_wait(&semViewerLock);
				map<CCustomerClient*,CCustomerClient*> mapLastTemp;
				map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itrLast = m_mapViewer.find(channel_name);
				if( itrLast!=m_mapViewer.end() )
				{  
					// ��ȡ�ɾֹ���
					mapLastTemp = itrLast->second;
					
					// �Ѿɾֹ��ڼ����¾�
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
							// ��������û�У������
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
					
					// ����ɵ�channel����
					itrLast->second.clear();
					m_mapViewer.erase(itrLast);
				}
				sem_post(&semViewerLock);
				CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"UserIds : one more game=%s.",
		                     				     channel_name);
			}

			// �㲥���û�
			CHANNELIT itrM = m_mapChannel.find(strNextChannelName.c_str());
			if( itrM!=m_mapChannel.end() )
			{
				// TODO ����userids,���������������������û�ɾ��
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
		// ����֪ͨ
		if(!jRoot["OverUserId"].isNull())
		{
			CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"-----[Boom]%s,OverUserId=%d,nextid=%d",channel_name,
                                                             jRoot["OverUserId"].asInt(), jRoot["NextUserId"].asInt());

			// ��������û���ϢΪ��ը
			char szOverUserId[20]={0};
			sprintf(szOverUserId,"%d",jRoot["OverUserId"].asInt());
			CRedisPublisher::GetInstance()->UpdateUserStatus( channel_name,GetServerId(),
									  szOverUserId,0,-1);

			CHANNELIT itrM = m_mapChannel.find(channel_name);
			if( itrM!=m_mapChannel.end() )
			{
				//�ж���һ���û��Ƿ񱾷�����
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
							//�Ǳ��������û�.
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
						// ����ը��ʱ��
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
				// ֪ͨ����
				list<CCustomerClient*>::iterator itL = itrM->second.clientList.begin();
				while( itL != itrM->second.clientList.end() )
				{
					(*itL)->NotifyGameOver(message);
					itL++;
				}
				BroadcastViewer( COMMANDTYPE_S2C_GAMEOVER,message,channel_name);

				// ��Ϸ����
				if(jRoot["NextUserId"].asInt()==0)
				{
					// ����
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
					//�����û���ϢΪ��ը
					CRedisPublisher::GetInstance()->UpdateUserStatus(itr->first,GetServerId(),
									itr->second.strCurUserId.c_str(),0,-1);
					//�����ȡ��һ���û�����
					string strNextUser="";
					string strIdleUser;
					int nIdleSize = CRedisPublisher::GetInstance()->GetIdleUser( itr->first,
                                                                                                     itr->second.strCurUserId.c_str(),
                                                                                                     strIdleUser);
					if(nIdleSize>1)
					{
						strNextUser = strIdleUser;
					}

					// ɾ������
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
					{// ��Ϸ��������
						// �Ȳ����һ��
						vec.push_back(atoi(strIdleUser.c_str()));
						//��ȡ����
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
			// ����ѿ�ʼ��Ϸ�Ƿ����5���� �ͷ���Դ
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
	sem_init(&sem_lock, 0, 1); // ���е�
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

	// �״�web���Ӳ���
	sleep(1);
	pParam->m_oWebComm.PostUserExitRoom(0,0,false,2);

	while(1)
	{
		sleep(10);
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
			// szUserId�п���Ϊ�գ���Ϊ����socket�ˣ����ǻ�δ���־͵���
			CPrintLog::GetInstance()->LogFormat( LOG_NORMAL,"CheckAlive : Close %s,this=%u\n",pCustomerClient->GetUserId().c_str()
                                                             ,pCustomerClient);

			// ����ǻ�δ���ֵģ�ֱ��ɾ��
			if(pCustomerClient->GetChannelName().length()==0)
			{
				pCustomerClient->Close();
				delete pCustomerClient;
				pCustomerClient=NULL;

				itr=m_vecCustomerClient.erase(itr);
				continue;
			}

			// ����ǹ��ڣ����б��Ƴ�
			bool bHaveViewer = true;
			int nLeft = DelViewer(pCustomerClient,pCustomerClient->GetChannelName().c_str());
			if(nLeft==0)
				bHaveViewer = false;

			//test by linqj
			DelViewerInAll(pCustomerClient,pCustomerClient->GetChannelName().c_str());

			// �������ң����б��Ƴ�
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

				// TODO ����û����ˣ�����sockû�յ�close����Ҫ��clientoffline����--------------------------------------
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
					{//��Ϸδ��ʼ

						// �����������޹��ں��û�
						//m_oSubscriber.unsubscribe(pCustomerClient->GetChannelName());
						sem_wait(&g_pCustomerService->semUnSubQueLock);
						g_pCustomerService->m_queUnSubMsg.push(pCustomerClient->GetChannelName());
						sem_post(&g_pCustomerService->semUnSubQueLock);

						m_mapChannel.erase(itrM);

						// �ж�redis�������û��Ƿ�������
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
					{//��Ϸ�ѿ�ʼ
						// ��ʱ������
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
	//test gdb debug error
	printf("test gdb debug error\n");
	int* error = NULL;
	int x = *error;
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

		//�ȴ�redis�ɹ�����
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
	{/* Ƶ������������Ƶ�� */
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
	// ��redis��������GameInfo get key
	// ��redis������ȡ�����û����ж��Ƿ���ڹ�
	//int nGameStatus = CRedisPublisher::GetInstance()->GetGameStatus(pClient->GetChannelName());
	if(  //(nGameStatus==GAMESTATUSCOUNT) 
             //||(IsValidConnect(pClient->GetChannelName(),pClient->GetUserId())) 
             1
          )
	{ 
		// �����������֤��gameid=0
		int nJoinUserCount=0;
		int nJoinType = 1;
		int nWaitOpenGameTimeOut=300;
		string strDataInfo="";
		int nRealGameId = 0;
                int nUpLoadId = 0;
                #if TESTNOWEB //������web����ģʽ
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
			
				// ��m_mapChannel���������û�����
				AddUser(pClient);

				if(nJoinUserCount==1)
				{
					// ��redis��������GameInfo set key
					CRedisPublisher::GetInstance()->CreateGameInfo( pClient->GetChannelName(),pClient->GetUserId(),
											pClient->GetGameId(),nWaitOpenGameTimeOut,
											pClient->GetMaxRemainTime(),pClient->GetMaxStayTime());
				}
				// ��redis������ȡ�����û�
				if(pClient->GetLastChannelName().length()>0)
				{// ����һ��
					PublishUserIds( atoi(pClient->GetUserId().c_str()),
							pClient->GetChannelName().c_str(),
                                                        pClient->GetLastChannelName().c_str(),
							atoi(pClient->GetGameId().c_str()),
                                                        strDataInfo);
				}
				else
				{// ��ͨ��������
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
	{// ��������	
		// ��m_mapChannel����insert new channel
		//AddChannel(pClient->GetChannelName().c_str());

		// ��m_mapViewer����������������
		AddViewer(pClient,pClient->GetChannelName().c_str());

		// ��redis������ȡ�����û�
		string strValue;
		bool bRet = CRedisPublisher::GetInstance()->GetUserIds(pClient->GetChannelName().c_str(),strValue);
		if(bRet)
		{
			pClient->NotifyUsers(strValue);
			nRet = SUCCESS;
		}
	}
	else if((pClient->GetUserType()==OWNER) || (pClient->GetUserType()==PLAYER))
	{// �������
		// ��redis��������GameInfo get key
		// ��redis������ȡ�����û����ж��Ƿ���ڹ�
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
				// ��������
				if(nJoinUserCount==1)
					pClient->SetUserType(OWNER);
				else
					pClient->SetUserType(PLAYER);
				// ��m_mapChannel����insert new channel
				//AddChannel(pClient->GetChannelName().c_str());

				// ��m_mapChannel���������û�����
				AddUser(pClient);
				
				/*TagRedisGame tag;
				CRedisPublisher::GetInstance()->GetGameCreateTime(pClient->GetChannelName(),tag);
				if(GAMESTATUSBEGIN==tag.nGameStatus)
				{
					// TODO 
				}*/

				// ������
				string strIdleUser="";
				int nIdleSize = CRedisPublisher::GetInstance()->GetIdleUser(pClient->GetChannelName(),"",strIdleUser);
				if(nIdleSize==pClient->GetOpenGameUserCount())
				{
					CRedisPublisher::GetInstance()->UpdateGameCreateTime(pClient->GetChannelName() );
				}

				// ��redis������ȡ�����û�
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
		// ��redis������ȡ�����û�
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
				// ��redis��������GameInfo get/set key
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
	// ��redis������ȡUserinfo Status
	// ��redis������ȡUserinfo Status
	// ��redis������ȡGameinfo CurUser
	if( (!IsBoom(pClient->GetChannelName(),GetServerId(),pClient->GetUserId()) )
	&&  (!IsBoom(pClient->GetChannelName(),"",strDestUserId.c_str()) )
	&&  (IsTheRole(pClient->GetChannelName(),pClient->GetUserId()))
	)
	{
		// ��redis��������Userinfo Status
		bool bRet = CRedisPublisher::GetInstance()->UpdateUserStatus(pClient->GetChannelName(),
				GetServerId(),pClient->GetUserId(),USERIDLE,-1);
		if(bRet)
		{
			// ��redis��������Gameinfo CurUser
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
	{// ����
		// ��m_mapViewer����������������
		DelViewer(pClient,pClient->GetChannelName().c_str());
		nRet = SUCCESS;
	}
	else
	{
		// ��ȡredis��ұ�ȷ���Ƿ����
		TagRedisUser tag;
		CRedisPublisher::GetInstance()->GetUserInfo( pClient->GetChannelName().c_str(),GetServerId(),
							     pClient->GetUserId().c_str(),tag);
		if(tag.nType!=USERTYPECOUNT)
		{// ���
			// ��redis������ȡGameinfo Status
			int nStatus=CRedisPublisher::GetInstance()->GetGameStatus(pClient->GetChannelName().c_str());
			if(nStatus==GAMESTATUSBEGIN)
			{// ��Ϸ�ѿ�ʼ,����Ϊ���ڷ���
				// ��redis��������Userinfo Status
				bool bRet = CRedisPublisher::GetInstance()->UpdateUserStatus(pClient->GetChannelName().c_str(),
						GetServerId(),pClient->GetUserId().c_str(),-1,0,-1);

				nRet = m_oWebComm.PostUserExitRoom( atoi(pClient->GetGameId().c_str()),
		                                                     atoi(pClient->GetUserId().c_str()),true,EXIT_ACTION);
			}
			else
			{// ֱ��ɾ��
				if(tag.nType==OWNER)
				{// ����fz
					CRedisPublisher::GetInstance()->ChangeOwner( pClient->GetChannelName().c_str(),
										     pClient->GetUserId().c_str());
				}
				// ��redis��������Userinfo Status
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
				// ��redis������ȡ�����û�
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

		// ��ȡredis��ұ�ȷ���Ƿ����
		TagRedisUser tag;
		CRedisPublisher::GetInstance()->GetUserInfo( pClient->GetChannelName().c_str(),GetServerId(),
							     pClient->GetUserId().c_str(),tag);
		if(tag.nType!=USERTYPECOUNT)
		{// ���
			// ��redis������ȡGameinfo Status
			int nStatus=CRedisPublisher::GetInstance()->GetGameStatus(pClient->GetChannelName().c_str());
			if(nStatus==GAMESTATUSBEGIN)
			{// ��Ϸ�ѿ�ʼ,����Ϊ����
				// ��redis��������Userinfo Status
				bool bRet = CRedisPublisher::GetInstance()->UpdateUserStatus( pClient->GetChannelName().c_str(),
											      GetServerId(),pClient->GetUserId().c_str(),-1,-1,0);
				if(bNotify)
					m_oWebComm.PostUserExitRoom( atoi(pClient->GetGameId().c_str()),
		                                                     atoi(pClient->GetUserId().c_str()),true,EXIT_SOCKET);		
			}
			else
			{// ֱ��ɾ��
				if(tag.nType==OWNER)
				{// ����fz
					CRedisPublisher::GetInstance()->ChangeOwner( pClient->GetChannelName().c_str(),
										     pClient->GetUserId().c_str());
				}
				// ��redis��������Userinfo Status
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
					// ��redis������ȡ�����û�
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
	{/* Ƶ������������Ƶ�� */
		TagGame tagGame;
		tagGame.nGameOverTime = 0x7ffffffd;
		tagGame.bThisServer = false;
		tagGame.nGameId  = atoi(pClient->GetGameId().c_str());
		tagGame.nMaxTime = pClient->GetMaxRemainTime();
		tagGame.nMinTime = pClient->GetMinRemainTime();
		tagGame.nRemainTime = pClient->GetMaxRemainTime();
		tagGame.strLastChannelName = pClient->GetLastChannelName();
		tagGame.clientList.push_back(pClient);

		// ����һ��///////////////////////////////////
		if(pClient->GetLastChannelName().length()>0)
		{
			// ��Ҳ���
			CHANNELIT itrM = m_mapChannel.find(pClient->GetLastChannelName());
			if( itrM!=m_mapChannel.end() )
			{
				// ��ȡ���
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
				// ����ɵ���ң���Ϸid
				itrM->second.clientList.clear();
				m_mapChannel.erase(itrM);
				//m_oSubscriber.unsubscribe(pClient->GetLastChannelName());
				sem_wait(&g_pCustomerService->semUnSubQueLock);
				g_pCustomerService->m_queUnSubMsg.push(pClient->GetLastChannelName());
				sem_post(&g_pCustomerService->semUnSubQueLock);
			}
			// ���ڲ���
			sem_wait(&semViewerLock);
			map<CCustomerClient*,CCustomerClient*> mapLastTemp;
			map< string,map<CCustomerClient*,CCustomerClient*> >::iterator itrLast = m_mapViewer.find(pClient->GetLastChannelName());
			if( itrLast!=m_mapViewer.end() )
			{  
				// ��ȡ����
				mapLastTemp = itrLast->second;
				map<CCustomerClient*,CCustomerClient*>::iterator itrView = mapLastTemp.begin();
				while( itrView!=mapLastTemp.end() )
				{
					itrView->second->SetChannelName(pClient->GetChannelName());
					itrView->second->SetGameId(tagGame.nGameId);
					itrView++;
				}
				// ����ɵ�channel����
				itrLast->second.clear();
				m_mapViewer.erase(itrLast);
			}
			// �ѹ��ڼӽ���
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
	
	// ���ݿ�
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

		// ���ף���ͣ���
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
		{// �б�Ϊ��
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
	{/* Ƶ������������Ƶ�� */
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
			// ����ʱ�� 
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

			// ����Data�ֶ�
			Json::Value jData;
			Json::Reader jDataReader(Json::Features::strictMode());
			if(jDataReader.parse((const char*)strDataInfo.c_str(),jData))
			{
				jGetRoot["Data"]=jData;
			}

			// ����һ�� ����NextGameId�ֶ�
			if(szLastChannelName!=NULL)
			{ 
				jGetRoot["SrcUserId"] = nUserId;
				jGetRoot["NextGameId"] = nGameId;
				strValue = jWriter.write(jGetRoot);
				CRedisPublisher::GetInstance()->Publish(szChannelName, strValue.c_str());
				CRedisPublisher::GetInstance()->Publish(szLastChannelName, strValue.c_str());
			
			}
			else
			{ // ��ͨ 
				if(nGameId!=0)
				{// ����Ǵ������䣬����GameId�ֶ�
				
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
