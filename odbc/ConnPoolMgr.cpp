#include "ConnPoolMgr.h"

#include <unistd.h> // usleep

CDBConnPool::CDBConnPool() : m_nMinSize(5), m_nMaxSize(50)
{
	m_strActiveSql = "SELECT 1 FROM USERDATA WHERE 0=1";
}

CDBConnPool::~CDBConnPool()
{
	UnInitPool();
}

//��ʼ�����ӳ�,������nMinSize������,���������Ӷ�����ɹ�ʱ����true.
bool CDBConnPool::InitPool(const char* dbDNSName, 
                           const char* userName, 
                           const char* passWord, 
                           const char* szActiveSql, 
                           int nMinSize, 
                           int nMaxSize)
{
	bool bRet = true;
	if (NULL != szActiveSql && '\0' != szActiveSql[0])
	{
		m_strActiveSql = szActiveSql;
	}
    m_dbDNSName = dbDNSName;
    m_userName  = userName;
    m_passWord  = passWord;
    
	m_nMinSize = nMinSize;
	m_nMaxSize = nMaxSize;
	
	m_nMinSize < MIN_DBCONN_SIZE ? m_nMinSize = MIN_DBCONN_SIZE : 1;
	m_nMinSize > MAX_DBCONN_SIZE ? m_nMinSize = MAX_DBCONN_SIZE : 1;
	m_nMaxSize < MIN_DBCONN_SIZE ? m_nMaxSize = MIN_DBCONN_SIZE : 1;
	m_nMaxSize > MAX_DBCONN_SIZE ? m_nMaxSize = MAX_DBCONN_SIZE : 1;
	
	for (int i = 0; i < m_nMinSize; ++i)
	{
		DBCONN conn;
		if (ApplayNewConn(conn))
		{
			m_Idle.push_back(conn);
		}
		else
		{
			bRet = false;
			break;
		}
	}
	return bRet;
}

//����������
bool CDBConnPool::ApplayNewConn(DBCONN & conn)
{
	bool bRet = false;
	conn.conn = new ODBC::Connection();
	if (NULL != conn.conn)
	{
		conn.conn->init(m_dbDNSName, m_userName, m_passWord);
		if(conn.conn->connect())
		{
			time(&conn.lastActive);
			bRet = true;
		}
		else
		{
			delete [] conn.conn;
			conn.conn = NULL;
		}
	}
	return bRet;
}

//��ȡ���ӳ���Ϣ
void CDBConnPool::GetConnPoolInfo(int& nMinSize, int& nMaxSize, int& nIdle, int& nBusy)
{
    ODBC::MutexLockGuard guard(m_mutex);
	nMinSize = m_nMinSize;
	nMaxSize = m_nMaxSize;
	nIdle    = m_Idle.size();
	nBusy	 = m_InUse.size();
}

//�������ӳش�С
void CDBConnPool::ResetConnPoolSize(int nMinSize, int nMaxSize)
{
	m_nMinSize = nMinSize;
	m_nMaxSize = nMaxSize;
	m_nMinSize < MIN_DBCONN_SIZE ? m_nMinSize = MIN_DBCONN_SIZE : 1;
	m_nMinSize > MAX_DBCONN_SIZE ? m_nMinSize = MAX_DBCONN_SIZE : 1;
	m_nMaxSize < MIN_DBCONN_SIZE ? m_nMaxSize = MIN_DBCONN_SIZE : 1;
	m_nMaxSize > MAX_DBCONN_SIZE ? m_nMaxSize = MAX_DBCONN_SIZE : 1;
}

//�������ӳ�,���ͷ���������
void CDBConnPool::UnInitPool()
{
	ODBC::MutexLockGuard guard(m_mutex);
	std::list<DBCONN>::iterator it = m_Idle.begin();
	for ( ; it != m_Idle.end(); ++it)
	{
	    if (it->conn)
	    {
	        it->conn->close();
		    delete it->conn;
		    it->conn = NULL;
		}
	}
	
	m_Idle.clear();
	std::map<ODBC::Connection*, int>::iterator it2 = m_InUse.begin();
	for ( ; it2 != m_InUse.end(); ++it2)
	{
	    if (it2->first)
	    {
	        it2->first->close();
		    delete it2->first;
		}
	}
	m_InUse.clear();
}

//�ӳ����л�ȡһ������.��ָ�����ȴ�ʱ��(��λ:s),��ʱ���Զ�����NULL.���ȴ�ʱ����СֵΪ5s
ODBC::Connection* CDBConnPool::GetConnFromPool(int nMaxTimeWait)
{
	nMaxTimeWait < 5 ? nMaxTimeWait = 5 : 1;
	ODBC::Connection* pCon = NULL;
	//�����ж������Ƿ��п��õ�,������ֱ�ӷ���,��û��,�鿴�Ƿ񳬹��������ֵ,��û����������һ���µķ���,��������ȴ�
	ODBC::MutexLockGuard guard(m_mutex);
	if (!m_Idle.empty())
	{
		std::list<DBCONN>::iterator it = m_Idle.begin();
		for ( ; it != m_Idle.end(); )
		{
			//2013-06-28
			//ȡ����ʱ��ÿ�ζ�active,�������ݿ��������
			pCon = it->conn;
			time(&it->lastActive);
			m_InUse.insert(std::make_pair(it->conn, 0));
			m_Idle.erase(it++);
			break;
		}
	}
	//���ж���Ϊ��,��������û�г������ֵ,����������һ������
	else if (m_Idle.empty() && (int)m_InUse.size() < m_nMaxSize)
	{
		DBCONN conn;
		if (ApplayNewConn(conn))
		{
			pCon = conn.conn;
			m_InUse.insert(std::make_pair(pCon, 0));
		}
	}

	return pCon;
}

//��������
void CDBConnPool::RecycleConn(ODBC::Connection* pConn)
{
	if (NULL != pConn)
	{
		ODBC::MutexLockGuard guard(m_mutex);
		DBCONN conn;
		conn.conn = pConn;
		time(&conn.lastActive);
		//����ǰ��������Ƿ����,�������������,Ȼ�����ж��Ƿ�С����Сֵ,��С����������һ��
		if (m_InUse.find(pConn) != m_InUse.end())
		{
			m_Idle.push_back(conn);
			m_InUse.erase(pConn);
		}
	}
}

//���������Ƿ����
bool CDBConnPool::TestConn(ODBC::Connection* pConn)
{
	//bool bRet = false;
	//if (NULL != pConn)
	//{
	//	int nRet = pConn->ActivateDBConnection(m_strActiveSql.c_str());
	//	if (0 == nRet || (97 == nRet && pConn->OpenDatabase() && 0 == pConn->ActivateDBConnection(m_strActiveSql.c_str())))
	//	{
	//		bRet = true;
	//	}
	//}
	//return bRet;
	return true;
}

//���ʱ����е�����(����ǰ���������������ӳ����ޣ��������رտ�������)
void CDBConnPool::ActiveIdleConn()
{
	ODBC::MutexLockGuard guard(m_mutex);
	std::list<DBCONN>::iterator it = m_Idle.begin();
	for ( ; it != m_Idle.end(); )
	{
		//�п��еĲ��������������������ޣ��������ر�����
		if ((int)m_InUse.size() + (int)m_Idle.size() > m_nMaxSize)
		{
			it->conn->close();
			m_Idle.erase(it++);
		}
		else
		{
			//�����ӽ��м���
			if (time(NULL) - it->lastActive > MAX_DBCONN_ACTIVE_TM)
				    //&& 0 != it->pConn->ActivateDBConnection(m_strActiveSql.c_str()))
			{
				it->conn->close();
				m_Idle.erase(it++);
			}
			else
			{
				++it;
			}
		}
	}
}

//��Ҫ��������Ƿ���Ч
bool CDBConnPool::IsConnValid(ODBC::Connection* pConn)
{
	return TestConn(pConn);
}

///CDBConnPoolMgr
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDBConnPoolMgr& CDBConnPoolMgr::GetInstance()
{
    static CDBConnPoolMgr instance;
    return instance;
}

CDBConnPoolMgr::CDBConnPoolMgr() : m_bExit(false)
{
}

CDBConnPoolMgr::~CDBConnPoolMgr()
{
	//֪ͨ�߳��˳�
	m_bExit = true;
	//�ȴ��߳��˳�
	//while (m_bExit) CAdapter::Sleep(200);

	ODBC::MutexLockGuard guard(m_mutex);
	std::map<std::string, CDBConnPool* >::iterator it = m_dbpool.begin();
	for ( ; it != m_dbpool.end(); ++it)
	{
		it->second->UnInitPool();
		delete it->second;
		it->second = NULL;
	}
	m_dbpool.clear();
}

//�������ӳز�ָ�����ӵ�����,��ʹ��""��Ϊ��������,�򴴽�Ĭ�ϳ���,�����������ִ�Сд
bool CDBConnPoolMgr::CreateConnPool(const char* szPoolName, 
                                    const char* dbDNSName, 
                                    const char* userName, 
                                    const char* passWord, 
                                    const char* szActiveSql/*�������*/, 
                                    int nMinSize, 
                                    int nMaxSize)
{
	bool bRet = false;
	CDBConnPool* pPool = new CDBConnPool();
	if (NULL != pPool)
	{
		if (pPool->InitPool(dbDNSName, userName, passWord, szActiveSql, nMinSize, nMaxSize))
		{
			ODBC::MutexLockGuard guard(m_mutex);
			m_dbpool.insert(std::make_pair(szPoolName, pPool));
			bRet = true;
		}
		else
		{
			delete [] pPool;
			pPool = NULL;
		}
	}
	return bRet;
}

//��ָ���ĳ����л�ȡһ������.��ָ�����ȴ�ʱ��(��λ:s),��ʱ���Զ�����NULL.���ȴ�ʱ����СֵΪ5s
ODBC::Connection* CDBConnPoolMgr::GetConnFromPool(const char* szPoolName, int nMaxTimeWait)
{
	ODBC::Connection* pCon = NULL;
	CDBConnPool* pPool = NULL;
	{
		ODBC::MutexLockGuard guard(m_mutex);
		std::map<std::string, CDBConnPool*>::iterator it = m_dbpool.find(szPoolName);
		if (it != m_dbpool.end())
		{
			pPool = it->second;
		}
	}
	if (NULL != pPool)
	{
		pCon = pPool->GetConnFromPool(nMaxTimeWait);
	}
	//ÿ�ε���ʱֵ
	nMaxTimeWait < MIN_WAITCONN_TM ? nMaxTimeWait = MIN_WAITCONN_TM : 1;
	const int nDelay = 100;
	int nMaxLoopCnt = nMaxTimeWait * 1000/ nDelay;
	while (NULL == pCon && --nMaxLoopCnt >= 0)
	{
		{
			ODBC::MutexLockGuard guard(m_mutex);
			std::map<std::string, CDBConnPool*>::iterator it = m_dbpool.find(szPoolName);
			if (it != m_dbpool.end())
			{
				pPool = it->second;
			}
		}

		if (NULL != pPool)
		{
			pCon = pPool->GetConnFromPool(nMaxTimeWait);
			if (NULL != pCon) break;
		}

		usleep(nDelay);
	}

	return pCon;
}

//�����ӻ�����ָ��������
void CDBConnPoolMgr::RecycleConn(const char* szPoolName, ODBC::Connection* pConn)
{
	CDBConnPool * pPool = NULL;
	{
		ODBC::MutexLockGuard guard(m_mutex);
		std::map<std::string, CDBConnPool*>::iterator it = m_dbpool.find(szPoolName);
		if (it != m_dbpool.end())
		{
			pPool = it->second;
		}
	}

	if (NULL != pPool)
	{
		pPool->RecycleConn(pConn);
	}
}

//��Ҫ��������Ƿ���Ч
bool CDBConnPoolMgr::IsConnValid(const char* szPoolName, ODBC::Connection* pConn)
{
	bool bRet = false;
	CDBConnPool * pPool = NULL;
	{
		ODBC::MutexLockGuard guard(m_mutex);
		std::map<std::string, CDBConnPool*>::iterator it = m_dbpool.find(szPoolName);
		if (it != m_dbpool.end())
		{
			pPool = it->second;
		}
	}
	if (pPool)
	{
		bRet = pPool->TestConn(pConn);
		//�����ݿ����ӶϿ�,����ʱһ��ʱ��
		if (!bRet) 
		    usleep(5000);
	}
	
	return bRet;
}

//��ȡ���ӳ���Ϣ
void CDBConnPoolMgr::GetConnPoolInfo(const char* szPoolName, int& nMinSize, int& nMaxSize, int& nIdle, int& nBusy)
{
	CDBConnPool * pPool = NULL;
	{
		ODBC::MutexLockGuard guard(m_mutex);
		std::map<std::string, CDBConnPool*>::iterator it = m_dbpool.find(szPoolName);
		if (it != m_dbpool.end())
		{
			pPool = it->second;
		}
	}
	
	if (NULL != pPool)
	{
		pPool->GetConnPoolInfo(nMinSize, nMaxSize, nIdle, nBusy);
	}
}

//�������ӳش�С
void CDBConnPoolMgr::ResetConnPoolSize(const char* szPoolName, int nMinSize, int nMaxSize)
{
	CDBConnPool * pPool = NULL;
	{
		ODBC::MutexLockGuard guard(m_mutex);
		std::map<std::string, CDBConnPool*>::iterator it = m_dbpool.find(szPoolName);
		if (it != m_dbpool.end())
		{
			pPool = it->second;
		}
	}

	if (NULL != pPool)
	{
		pPool->ResetConnPoolSize(nMinSize, nMaxSize);
	}
}

void CDBConnPoolMgr::ActiveIdleConn()
{
	std::map<std::string, CDBConnPool*>::iterator it = m_dbpool.begin();
	for ( ; it != m_dbpool.end(); ++it)
	{
		if (NULL != it->second) it->second->ActiveIdleConn();
	}
}
