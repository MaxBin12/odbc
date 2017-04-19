#ifndef __ODBC_PREPARED_STATEMENT_H__
#define __ODBC_PREPARED_STATEMENT_H__

#include <iostream>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>
#include <odbcwapper/ResultSet.h>

namespace ODBC
{

// �󶨲�����Ϣ�ṹ
struct BIND_PARAM_INFO
{
    SQLCHAR*     paramValue; // ���������ַ
    SQLLEN*      indArr;     // IND����ָ��
    SQLSMALLINT  sql_c_type; // C�������ͱ�ʶ
    SQLSMALLINT  sql_type;   // SQL�������ͱ�ʶ
    SQLLEN       columnSize; // ��Ӧ�ֶε�ʵ�ʳ���
    SQLLEN       columnMax;  // ��Ӧ�ֶε���󳤶�
    unsigned int memSize;    // ������ڴ��С

    BIND_PARAM_INFO()
    {
        memset(this, 0, sizeof(BIND_PARAM_INFO));
    }
};

class PreparedStatement
{
public:
    PreparedStatement(unsigned int paramSetSize = 1);
    PreparedStatement(SQLHSTMT hStmt, const std::string & strSql, unsigned int paramSetSize = 1);
    ~PreparedStatement();
    
    ResultSet* executeQuery();
    int executeUpdate();
    
    void setInt(int index, int value);
    // void setTinyInt(int index, unsigned char value);
    void setSmallInt(int index, short int value);
    void setString(int index, std::string & value);
    
private:
    PreparedStatement(const PreparedStatement &);
    PreparedStatement & operator = (const PreparedStatement &);
    
    void resizeParamTable(int maxSize);
    void setParamAttr(int index, SQLSMALLINT sqlType, int maxSize);
    void handleError(const char* FILE = __FILE__, int LINE = __LINE__);
    
private:
    SQLHSTMT hStmt_;
    std::string strSql_;
    unsigned int paramNum_;
    unsigned int paramSetSize_;
    std::vector<BIND_PARAM_INFO> bindParamInfoVec_;
}; 

} // namespace ODBC

#endif // __ODBC_PREPARED_STATEMENT_H__
