#ifndef __CONNPOOL_MGR_HH___
#define __CONNPOOL_MGR_HH___

#include <odbcwapper/Connection.h>
#include <odbcwapper/Mutex.h>
#include <map>
#include <list>

#define MAX_DBCONNPOOL_SIZE		5					//���ɴ��������ӳ���
#define MIN_DBCONN_SIZE			1					//ÿ�����ӳ�����������С������
#define	DEFAUT_DBCONN_SIZE		5					//ÿ�����ӳ�Ĭ�ϴ�����������
#define MAX_DBCONN_SIZE			255					//ÿ�����ӳ������������������
#define MAX_DBCONN_ACTIVE_TM	3*60				//���Ӽ���ʱ��
#define DEFAULT_POOLNAME		"DBCONNECTPOOL"		//Ĭ�����ӳ�����
#define MIN_WAITCONN_TM			5					//�ȴ���ȡ���ӵĳ�ʱֵ��λ��
#define POOLNAME_ACC			"ACC"				//acc���ӳ���
#define POOLNAME_SVR			"SVR"				//svr���ӳ���
#define POOLNAME_RPT			"RPT"				//RPT_WAIT_B�����ӳ���
#define POOLNAME_MTLVL			"MTLVL"				//MT_LEVEL_QUEUE�����ӳ���
#define POOLNAME_MTVFY			"MTVFY"				//MT_VERIFY_WAIT�����ӳ���

struct DBCONN
{
    ODBC::Connection* conn;
    time_t lastActive;      // �ϴ�ʹ�õ�ʱ��

    DBCONN()
    {
        conn = NULL;
		time(&lastActive);
    }
	
    DBCONN & operator = (const DBCONN & other)
    {
        if (this == &other)
        {
            return *this;
        }
        memset(this, 0, sizeof(DBCONN));
        memcpy(this, &other, sizeof(DBCONN));
        return *this;
    }
};

class CDBConnPoolMgr;

//���ݿ����ӳ���
class CDBConnPool
{
	friend class CDBConnPoolMgr;

public:
    CDBConnPool();
    virtual ~CDBConnPool();
private:  
	//��ʼ�����ӳ�,������nMinSize������,���������Ӷ�����ɹ�ʱ����true.
	bool InitPool(const char* dbDNSName, 
                  const char* userName, 
                  const char* passWord,
                  const char* szActiveSql = ""/*�������*/, 
                  int nMinSize = MIN_DBCONN_SIZE, 
                  int nMaxSize = MAX_DBCONN_SIZE);
                  
	//�ӳ����л�ȡһ������.��ָ�����ȴ�ʱ��(��λ:s),��ʱ���Զ�����NULL.���ȴ�ʱ����СֵΪ5s
	ODBC::Connection* GetConnFromPool(int nMaxTimeWait = MIN_WAITCONN_TM);
	//��������
	void RecycleConn(ODBC::Connection* pConn);
	//��Ҫ��������Ƿ���Ч
	bool IsConnValid(ODBC::Connection* pConn);
	//��ȡ���ӳ���Ϣ
	void GetConnPoolInfo(int& nMinSize, int& nMaxSize, int& nIdle, int& nBusy);
	//�������ӳش�С
	void ResetConnPoolSize(int nMinSize, int nMaxSize);
private:
	//���������Ƿ����
	bool TestConn(ODBC::Connection* pConn);
	//�������ӳ�,���ͷ���������
	void UnInitPool();
	//����������
	bool ApplayNewConn(DBCONN& conn);
	//���ʱ����е�����
	void ActiveIdleConn();
private:
	int m_nMinSize;
	int m_nMaxSize;
	std::string m_dbDNSName;
	std::string m_userName;
	std::string m_passWord;
	std::string m_strActiveSql;
	std::map<ODBC::Connection*, int> m_InUse;
	std::list<DBCONN> m_Idle;
    ODBC::MutexLock m_mutex;
};

//���ݿ����ӳع�����
class CDBConnPoolMgr
{
public:
    static CDBConnPoolMgr& GetInstance();//����ʵ����ȡ�ӿ�
    virtual ~CDBConnPoolMgr();
public: 
	//�������ӳز�ָ�����ӵ�����,�����������ִ�Сд
	bool CreateConnPool(const char* szPoolName, 
	                    const char* dbDNSName, 
                        const char* userName, 
                        const char* passWord,
                        const char* szActiveSql = ""/*�������*/, 
                        int nMinSize = 5, 
                        int nMaxSize = 50);
    
	//��ָ���ĳ����л�ȡһ������.��ָ�����ȴ�ʱ��(��λ:s),��ʱ���Զ�����NULL.���ȴ�ʱ����СֵΪ5s
	ODBC::Connection* GetConnFromPool(const char* szPoolName, int nMaxTimeWait=5);
	//�����ӻ�����ָ��������
	void RecycleConn(const char* szPoolName, ODBC::Connection* pConn);
	//��Ҫ��������Ƿ���Ч
	bool IsConnValid(const char* szPoolName, ODBC::Connection* pConn);
	//�������ӳ��еĿ�������
	void ActiveIdleConn();
	//��ȡ���ӳ���Ϣ
	void GetConnPoolInfo(const char* szPoolName, int& nMinSize, int& nMaxSize, int& nIdle, int& nBusy);
	//�������ӳش�С
	void ResetConnPoolSize(const char* szPoolName, int nMinSize, int nMaxSize);

private:
    CDBConnPoolMgr();
private:
	// static THREAD_RETURN __STDCALL ThreadCheckConnPool(LPVOID pParam);
	std::map<std::string, CDBConnPool*> m_dbpool;
	ODBC::MutexLock m_mutex;
	bool m_bExit;
};

class CDBConnectionPtr
{
public:
	CDBConnectionPtr(const std::string & DBConnPoolName, ODBC::Connection* conn) 
	        : DBConnPoolName_(DBConnPoolName), conn_(conn)
	{};
	virtual ~CDBConnectionPtr()
	{
		if (conn_ != NULL);
		{	
			CDBConnPoolMgr::GetInstance().RecycleConn(DBConnPoolName_.c_str(), conn_);
			conn_ = NULL;
		}
	}

protected:
    std::string DBConnPoolName_;
	ODBC::Connection* conn_;
};

#endif // __CONNPOOL_MGR_HH___
