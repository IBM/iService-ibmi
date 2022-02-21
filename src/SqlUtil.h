#ifndef SQL_UTIL_H
#define SQL_UTIL_H
#include "Runnable.h"
#include <string>
#include <list>
#include "FlightRec.h"
#include "INode.h"
#include "IAttribute.h"

#define __STDC_WANT_DEC_FP__
#include "sqlcli.h"

/******************************************************************
 *  "sql": {
 *   "servermode":"on",
 *   "error":"on",
 *   "connect":  [{"conn":"conn1", "db":"x", "user":"x", "pwd":"x", "autocommit":"on", "commit":"READ_COMMITTED"...}],
 *   "operation":[
 *    {"exec": {"conn":"conn1", "parm":[{"io":"in", "value":"frog"}, {"io":"in", "value":"9"}],
 *              "rowcount":0, "value":"UPDATE animal SET essay = ?, where id = ?"}},
 *    {"execDirect": {"conn":"conn1", "value":"SELECT * FROM QUSRSYS/QATOCHOST"}, 
 *              "rowcount":0, "fetch": {"block":"all", "rec":1}},
 *    {"commit": {"conn":"conn1", "action":"commit"}}
 *   ],
 *   "free": ["conn1", ... ] | "all" | "none"
 *  }
 *****************************************************************/

//++++++++++++++++++++++++++++ SqlConn +++++++++++++++++++++++++++++//
class SqlConn
{
public:
  SqlConn(const char* db, const char* user, const char* pwd);
  SqlConn(INode* node);
  ~SqlConn();

  std::string&        lable();
  void                lable(std::string& label);
  std::string&        db();
  void                db(std::string& db);
  std::string&        user();
  void                user(std::string& user); 
  std::string&        pwd();
  void                pwd(std::string& pwd); 
  SQLHDBC             hdbc();
  bool init(FlightRec& fr);
  bool connect(FlightRec& fr);
  bool autoCommit();
  bool commitTranct(SQLSMALLINT op, SQLHSTMT hstmt, FlightRec& fr);

private:
  SqlConn(const SqlConn&);
  SqlConn& operator=(const SqlConn&);

  INode*                    _node_p;
  SQLHDBC                   _hdbc;
  std::string               _lable;
  std::string               _db;
  std::string               _user;
  std::string               _pwd;

  SQLINTEGER               _auto_commit; // conn attr
  SQLINTEGER               _commit;      // conn attr
};

//++++++++++++++++++++++++++++ Parm +++++++++++++++++++++++++++++//
class Parm
{
public:
  Parm(SQLSMALLINT io, const std::string& value);
  Parm(INode* node);
  ~Parm();

  SQLSMALLINT&      io();
  SQLSMALLINT&      cDataType();
  SQLSMALLINT&      sqlDataType();
  SQLINTEGER&       parmSize();         
  SQLSMALLINT&      decimalDigits();
  SQLSMALLINT&      nullable();
  SQLPOINTER&       parmValuePtr();
  SQLINTEGER&       parmValueLength();

  bool init(FlightRec& fr);
  bool prepare4Bind(FlightRec& fr);
  void copyback();

private:
  Parm(const Parm&);
  Parm& operator=(const Parm&);

  INode*          _node_p;
  SQLSMALLINT     _io;
  std::string     _value;

  SQLSMALLINT     _c_data_type;
  SQLSMALLINT     _sql_data_type;
  SQLINTEGER      _parm_size;         
  SQLSMALLINT     _decimal_digits;
  SQLSMALLINT     _nullable;
  SQLPOINTER      _parm_value_p;
  SQLINTEGER      _parm_value_len; // MAX_BUFF_SIZE

};

typedef enum Op_Type_t
{
  COMMIT,
  EXEC,
  EXECD,
  INVOP
}Op_Type;

//++++++++++++++++++++++++++++ Operation +++++++++++++++++++++++++++++//
class Operation
{
public:
  Operation(SqlConn* conn, const std::string& stmt, Op_Type opType);
  Operation(INode* node);
  ~Operation();

  bool init(FlightRec& fr);
  bool run(FlightRec& fr);

  void addParm(Parm* parm);
  void copyback();

private:
  Operation(const Operation&);
  Operation& operator=(const Operation&);

  bool fetch(FlightRec& fr);

  std::string           _conn_lable;
  std::list<Parm*>      _parms;
  INode*                _node_p;

  INode*                _fetch_node_p;
  int                   _block;         // 0: ALL; n: >0
  int                   _rec;           // 0: N/A; n: >0

  Op_Type               _type;
  // std::string           _conn_lable;
  SqlConn*              _conn_p;
  std::string           _value;
  SQLHSTMT              _hstmt;
  SQLSMALLINT           _commit_op;

  //---------- fetch --------------------------
  SQLINTEGER            _row_count;
  IAttribute*           _attr_row_count;
  SQLSMALLINT           _result_col_num;

};

//++++++++++++++++++++++++++++ SqlUtil +++++++++++++++++++++++++++++//
class SqlUtil : public Runnable
{
public:
  SqlUtil(INode* node);
  ~SqlUtil();

  void              init(FlightRec& fr);
  void              run(FlightRec& fr);

  static void       sqlError(SQLHDBC hdbc, SQLHSTMT hstmt, FlightRec& fr);
  static void       clear();
  static SqlConn*   getConnection(const std::string& label);
  static bool       addConnection(SqlConn* conn);
  static SQLHENV    henv();
  static bool       isServerMode();
  static void       serverMode(bool mode);
  static bool       initEnv(FlightRec& fr);
  static SqlConn*   connect(const char* db, const char* user, const char* pwd, FlightRec& fr);

private:
  SqlUtil(const SqlUtil&);
  SqlUtil& operator=(const SqlUtil&);

  std::list<Operation*>                   _ops;
  std::list<std::string>                  _conns2free;
  bool                                    _free_all;    // attr "FREE" on "ALL"

  static std::list<SqlConn*>*             _conns_p;
  static SQLHENV                          _henv;
  static int                              _server_mode;       
};

#endif