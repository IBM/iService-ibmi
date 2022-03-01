#include "SqlUtil.h"
#include <assert.h>
#include "IAttribute.h"
#include "Utility.h"
#include <stdlib.h>
#include <cmath>
#include <xxcvt.h>
#include <except.h>

//================================================================//
//=======  SqlConn ===============================================//
//================================================================//

/******************************************************
 * Constructor
 * 
 * Parm:
 *  db      - database 
 *  user    - user id
 *  pwd     - password
 *****************************************************/
SqlConn::SqlConn(const char* db, const char* user, const char* pwd)
  : _hdbc(SQL_NULL_HDBC), _db(db), _user(user), _pwd(pwd),
    _auto_commit(SQL_TRUE), _node_p(NULL), _commit(SQL_TXN_READ_UNCOMMITTED)
{
  _lable = _db + _user + _pwd;
}

/******************************************************
 * Constructor with ptr of node.
 *****************************************************/
SqlConn::SqlConn(INode* node)
  : _hdbc(SQL_NULL_HDBC), _lable(), _db("*LOCAL"), _user(), _pwd(),
    _auto_commit(SQL_TRUE), _node_p(node), _commit(SQL_TXN_READ_UNCOMMITTED)
{}

/******************************************************
 * Destructor
 *****************************************************/
SqlConn::~SqlConn()
{
  SQLRETURN rc = SQL_SUCCESS;
  if ( SQL_NULL_HDBC != _hdbc)
  {
    rc = SQLDisconnect(_hdbc);
    rc = SQLFreeConnect(_hdbc);
    _hdbc = SQL_NULL_HDBC;
  }
}

/******************************************************
 * Get connection label
 *****************************************************/
std::string& SqlConn::lable()
{
  return _lable;
}

/******************************************************
 * Set connection label
 *****************************************************/
void SqlConn::lable(std::string& label)
{
  _lable = label;
}

/******************************************************
 * Get the database name
 *****************************************************/
std::string& SqlConn::db()
{
  return _db;
}

/******************************************************
 * Set the database name
 *****************************************************/
void SqlConn::db(std::string& db)
{
  _db = db;
}

/******************************************************
 * Get the user name
 *****************************************************/
std::string& SqlConn::user()
{
  return _user;
}

/******************************************************
 * Set the user name
 *****************************************************/
void SqlConn::user(std::string& user)
{
  _user = user;
}

/******************************************************
 * Get the passowrd
 *****************************************************/
std::string& SqlConn::pwd()
{
  return _pwd;
}

/******************************************************
 * Set the passowrd
 *****************************************************/
void SqlConn::pwd(std::string& pwd)
{
  _pwd = pwd;
}

/******************************************************
 * Get the connection handle
 *****************************************************/
SQLHDBC SqlConn::hdbc()
{
  return _hdbc;
}

/******************************************************
 * Init the connection
 * 
 * Parm:
 *    fr    -  error info
 *****************************************************/
bool SqlConn::init(FlightRec& fr)
{
  bool ret = false;
  std::string n;
  std::string v;
  IAttribute* p = NULL;
  //////-------------- lable ----------------------------------------
  n = "CONN";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    v = p->valueString();
    // cvt2Upper(v);
    // trim(v);
    _lable = v;
  }
  else
  {
    fr.code(ERR_SQL_INIT);
    fr.addMessage("MISSING ATTRIBUTE CONN FOR CONNECT.");   
    return ret;       
  }
  //////-------------- db  -----------------------------------
  n = "DB";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    _db = v; 
  }
  else
  {
    fr.code(ERR_SQL_INIT);
    fr.addMessage("MISSING ATTRIBUTE DB FOR CONNECT.");
    return ret; 
  }

  if ( SqlUtil::isServerMode() ) // USER & PWD are only valid in server mode
  {
    //////-------------- user (opt) --------------------------
    n = "USER";
    p = _node_p->getAttribute(n);
    if ( NULL != p )
    {
      v = p->valueString();
      trim(v);
      _user = v;
    }
    //////-------------- pwd (opt) --------------------------
    n = "PWD";
    p = _node_p->getAttribute(n);
    if ( NULL != p )
    {
      _pwd = p->valueString();
    }
  }
  //////-------------- autocommit (opt) ----------------------
  n = "AUTOCOMMIT";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "OFF" == v )
    {
      _auto_commit = SQL_FALSE;
    }
  }

    //////-------------- commit (opt) ----------------------
  n = "COMMIT";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "NO_COMMIT" == v )
    {
      _commit = SQL_TXN_NO_COMMIT ;
    }
    else if ( "READ_UNCOMMITTED" == v )
    {
      _commit = SQL_TXN_READ_UNCOMMITTED;
    }
    else if ( "READ_COMMITTED" == v )
    {
      _commit = SQL_TXN_READ_COMMITTED;
    }
    else if ( "REPEATABLE_READ" == v )
    {
      _commit = SQL_TXN_REPEATABLE_READ;
    }
    else if ( "SERIALIZABLE" == v )
    {
      _commit = SQL_TXN_SERIALIZABLE;
    }
    else
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage(v, "INVALID VALUE FOR ATTRIBUTE COMMIT.");
      return ret;
    }
  }

  /****************** Validate "USER" & "PWD" if server mode ********************/
  if ( SqlUtil::isServerMode() )
  {
    if ( _user.length() == 0 || _pwd.length() == 0 )
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("ATTRIBUTE USER AND PWD MUST BE SPECIFIED FOR CONNECT IN SERVER MODE.");
      return ret;
    }
  }

  ret = true;
  return ret;
}

/******************************************************
 * Connect database
 * 
 * Parm:
 *    fr    -  error info
 *****************************************************/
bool SqlConn::connect(FlightRec& fr)
{
  bool ret = true;
  if ( SQL_NULL_HDBC != _hdbc )
  {
    // connection is active
    return ret;
  }

  bool serverMode = SqlUtil::isServerMode();
  
  if ( SQL_SUCCESS != SQLAllocConnect(SqlUtil::henv(), &_hdbc) )
  {
    fr.code(ERR_SQL_RUN);
    fr.addMessage("FAIL TO CALL SQLAllocConnect.");
    SqlUtil::sqlError(_hdbc, SQL_NULL_HSTMT, fr);
    ret = false;
  }
  else
  {

    //------------------- set connection attributes ---------------
    if ( SQL_SUCCESS != SQLSetConnectAttr(_hdbc, SQL_ATTR_AUTOCOMMIT, &_auto_commit, SQL_NTS) )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAIL TO CALL SQLSetConnectAttr for AUTOCOMMIT.");
      SqlUtil::sqlError(_hdbc, SQL_NULL_HSTMT, fr);
      ret = false;
      return ret;
    }
    if ( SQL_SUCCESS != SQLSetConnectAttr(_hdbc, SQL_ATTR_COMMIT, &_commit, SQL_NTS) )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAIL TO CALL SQLSetConnectAttr for COMMIT.");
      SqlUtil::sqlError(_hdbc, SQL_NULL_HSTMT, fr);
      ret = false;
      return ret;
    }

    //------------------------ connect --------------------------------
    // In server mode, we should also allow current job user to log on
    // DB2 without usr/pwd. aka, OPERATION CONN is allowed as not
    // specified.                                           
    //-----------------------------------------------------------------
    if ( serverMode && _user.length() != 0 && _pwd.length() != 0 ) 
    {
      if ( SQL_SUCCESS != SQLConnect( _hdbc, 
                                      const_cast<char*>(_db.c_str()), SQL_NTS, 
                                      const_cast<char*>(_user.c_str()), SQL_NTS, 
                                      const_cast<char*>(_pwd.c_str()), SQL_NTS) )
      {
        fr.code(ERR_SQL_RUN);
        fr.addMessage("FAIL TO CALL SQLConnect TO CONNECT DB IN SERVER MODE.");
        SqlUtil::sqlError(_hdbc, SQL_NULL_HSTMT, fr);
        ret = false;
      }
    }
    else
    {
      if ( SQL_SUCCESS != SQLConnect(_hdbc, 
                                     const_cast<char*>(_db.c_str()), SQL_NTS, 
                                     NULL, SQL_NTS, 
                                     NULL, SQL_NTS) )
      {
        fr.code(ERR_SQL_RUN);
        fr.addMessage("FAIL TO CALL SQLConnect TO CONNECT DB.");
        SqlUtil::sqlError(_hdbc, SQL_NULL_HSTMT, fr);
        ret = false;
      }
    }
  }

  return ret;
}

/******************************************************
 * Get setting of autocommit
 *****************************************************/
bool SqlConn::autoCommit()
{
  return SQL_TRUE == _auto_commit;
}

/******************************************************
 * Commit transaction.
 * 
 * Parm:
 *    op    -  action
 *    hstmt -  stmt handle
 *    fr    -  error info
 *****************************************************/
bool SqlConn::commitTranct(SQLSMALLINT op, SQLHSTMT hstmt, FlightRec& fr)
{
  bool ret = true;
  if ( SQL_SUCCESS != SQLTransact(SqlUtil::henv(), _hdbc, op) )
  {
    fr.code(ERR_SQL_RUN);
    fr.addMessage("FAIL TO CALL SQLTransact");
    SqlUtil::sqlError(_hdbc, hstmt, fr);
    ret = false;
  }
  return ret;
}

//================================================================//
//=======  Parm ==================================================//
//================================================================//
const int MAX_BUFF_SIZE = 65535;

/******************************************************
 * Constructor
 * 
 * Parm:
 *    io    -  type of io
 *    value -  parm value
 *****************************************************/
Parm::Parm(SQLSMALLINT io, const std::string& value)
  : _node_p(NULL), _io(io), _value(value), 
    _c_data_type(SQL_CHAR), _sql_data_type(SQL_CHAR),
    _parm_size(0), _decimal_digits(0), _nullable(SQL_FALSE),
    _parm_value_p(NULL), _parm_value_len(MAX_BUFF_SIZE)
{}

/******************************************************
 * Constructor with node ptr
 * 
 * Parm:
 *    node    -  ptr of node
 *****************************************************/
Parm::Parm(INode* node)
  : _node_p(node), _io(SQL_PARAM_INPUT), _value(), 
    _c_data_type(SQL_CHAR), _sql_data_type(SQL_CHAR),
    _parm_size(0), _decimal_digits(0), _nullable(SQL_FALSE),
    _parm_value_p(NULL), _parm_value_len(MAX_BUFF_SIZE)
{}

/******************************************************
 * Destructor
 *****************************************************/
Parm::~Parm()
{
  if ( NULL != _parm_value_p )
  {
    delete[] _parm_value_p;
  }
}

/******************************************************
 * Get io type of parm
 *****************************************************/
SQLSMALLINT& Parm::io()
{
  return _io;
}

/******************************************************
 * Get C-style data type of parm
 *****************************************************/
SQLSMALLINT& Parm::cDataType()
{
  return _c_data_type;
}

/******************************************************
 * Get sql data type of parm
 *****************************************************/
SQLSMALLINT& Parm::sqlDataType()
{
  return _sql_data_type;
}

/******************************************************
 * Get size of parm
 *****************************************************/
SQLINTEGER& Parm::parmSize()
{
  return _parm_size;
}       

/******************************************************
 * Get decimal digits number of parm
 *****************************************************/
SQLSMALLINT& Parm::decimalDigits()
{
  return _decimal_digits;
}

/******************************************************
 * Get setting of nullable
 *****************************************************/
SQLSMALLINT& Parm::nullable()
{
  return _nullable;
}

/******************************************************
 * Get ptr to parm value
 *****************************************************/
SQLPOINTER& Parm::parmValuePtr()
{
  return _parm_value_p;
}

/******************************************************
 * Get length of the parm value
 *****************************************************/
SQLINTEGER& Parm::parmValueLength()
{
  return _parm_value_len;
}

/******************************************************
 * Init parm
 * 
 * Parm:
 *    fr  -  error info
 *****************************************************/
bool Parm::init(FlightRec& fr)
{
  bool ret = false;
  std::string n;
  std::string v;
  IAttribute* p = NULL;

  n = "IO";
  p = _node_p->getAttribute(n);
  if ( NULL != p  )
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "IN" == v )
    {
      _io = SQL_PARAM_INPUT;
    }
    else if ( "OUT" == v )
    {
      _io = SQL_PARAM_OUTPUT;
    }
    else if ( "BOTH" == v )
    {
      _io = SQL_PARAM_INPUT_OUTPUT;
    }
    else
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage(v, "INVALID VALUE FOR ATTRIBUTE IO IN PARM ELEMENT.");
      return ret;      
    }
  }
  else
  {
    fr.code(ERR_SQL_INIT);
    fr.addMessage("MISSING ATTRIBUTE IO FOR PARM ELEMENT.");
    return ret;
  }

  n = "VALUE";
  p = _node_p->getAttribute(n);
  if ( NULL != p  )
  {
    v = p->valueString();
    _value = v;
  }
  else
  {
    fr.code(ERR_SQL_INIT);
    fr.addMessage("MISSING ATTRIBUTE VALUE FOR PARM ELEMENT.");
    return ret;
  }

  ret = true;
  return ret;
}

/******************************************************************
 * Prepare for bind parms
 * 
 * Parm:
 *    fr  -  error info
 * 
 * Note:
 *   Should be called between SQLDescribeParam and SQLBindParameter 
 ******************************************************************/
bool Parm::prepare4Bind(FlightRec& fr)
{
  bool ret = true;

  // for most of the types, use SQL_C_CHAR to indicate the input type for SQLBindParameter().
  _parm_value_len = _value.length() > _parm_size ? _value.length() : _parm_size;
  _parm_value_p = new char[_parm_value_len];
  memset(_parm_value_p, 0x00, _parm_value_len);

  switch (_sql_data_type)
  {
  case SQL_CHAR:
  case SQL_VARCHAR:
  case SQL_GRAPHIC:
  case SQL_VARGRAPHIC:
    _c_data_type = SQL_C_CHAR;
    _parm_value_len = _value.length();
    memcpy(_parm_value_p, _value.c_str(), _value.length());
    break;
  case SQL_WCHAR:
  case SQL_WVARCHAR:
    // user should use hex chars to describe the value
    _c_data_type = SQL_C_CHAR;
    _parm_value_len = _value.length()/2; 
    ret = hexChar2Bin(_value);
    if ( !ret )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAILED TO CONVERT HEXADECIMAL CHARS TO BINARY.");
    }
    else
    {
      memcpy(_parm_value_p, _value.c_str(), _value.length());
    }
    break;  
  case SQL_SMALLINT:
    _c_data_type = SQL_C_SHORT;
    _parm_value_len = _value.length();
    memcpy(_parm_value_p, _value.c_str(), _value.length());
    break;
  case SQL_INTEGER:
    _c_data_type = SQL_C_SLONG;
    _parm_value_len = _value.length();
    memcpy(_parm_value_p, _value.c_str(), _value.length());
    break;
  case SQL_BIGINT:
    _c_data_type = SQL_C_BIGINT;
    _parm_value_len = _value.length();
    memcpy(_parm_value_p, _value.c_str(), _value.length());
    break; 
  case SQL_FLOAT:
  case SQL_REAL:
  case SQL_DOUBLE:
  case SQL_DECFLOAT:
    _c_data_type = SQL_C_DEFAULT;
    _parm_value_len = _value.length();
    memcpy(_parm_value_p, _value.c_str(), _value.length());
    break; 
  case SQL_DATETIME:
  case SQL_DATE:
  case SQL_TIME:
  case SQL_TIMESTAMP:
    _c_data_type = SQL_C_CHAR;
    _parm_value_len = _value.length();
    memcpy(_parm_value_p, _value.c_str(), _value.length());
    break;
  case SQL_NUMERIC:
  case SQL_DECIMAL:
    _c_data_type = SQL_C_CHAR;
    _parm_value_len = _value.length();
    memcpy(_parm_value_p, _value.c_str(), _value.length());
    break;
  case SQL_BINARY:
  case SQL_VARBINARY: 
    _c_data_type = SQL_C_BINARY;
    _parm_value_len = _value.length()/2;
    ret = hexChar2Bin(_value);
    if ( !ret )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAILED TO CONVERT HEXADECIMAL CHARS TO BINARY.");
    }
    else
    {
      memcpy(_parm_value_p, _value.c_str(), _value.length());
    }
    break;
  case SQL_CLOB:
  case SQL_DBCLOB:
    _c_data_type = SQL_C_CHAR;
    if ( SQL_PARAM_INPUT == _io )
    {
      _parm_value_len = _value.length();
      memcpy(_parm_value_p, _value.c_str(), _value.length());
    }
    else
    {
      memset(_parm_value_p, JC(SPACE), _parm_value_len);
    }
    break;
  default:
    assert(false);
    break;
  }
  return ret;
}

/******************************************************
 * Copy data from sql return to node
 * 
 * Parm:
 *    fr  -  error info
 *****************************************************/
void Parm::copyback()
{
  if ( SQL_CLOB == _sql_data_type || SQL_DBCLOB == _sql_data_type )
  {
    if ( SQL_PARAM_INPUT_OUTPUT == _io || SQL_PARAM_OUTPUT == _io )
    {
      _value.clear();
      _value.append(static_cast<char*>(_parm_value_p), _parm_value_len);
    }
  }
  else
  { 
    // do nothing for other types. 
  }
}

//================================================================//
//=======  Operation =============================================//
//================================================================//

/*************************************************************
 * Constructor
 * 
 * Parm:
 *    conn    -  ptr of connection
 *    stmt    -  stmt string
 *    opType  -  type of attr OPERATION
 *************************************************************/
Operation::Operation(SqlConn* conn, const std::string& stmt, Op_Type opType)
  : _node_p(NULL), _type(opType), _value(stmt), _parms(),_conn_lable(),
    _hstmt(SQL_NULL_HSTMT), _conn_p(conn), _commit_op(SQL_COMMIT),
    _row_count(0), _attr_row_count(NULL), _result_col_num(0),
    _fetch_node_p(NULL), _block(0), _rec(1)
{}

/*************************************************************
 * Constructor
 * 
 * Parm:
 *    node    -  ptr of node
 *************************************************************/
Operation::Operation(INode* node)
  : _node_p(node), _type(INVOP), _value(), _parms(), _conn_lable(),
    _hstmt(SQL_NULL_HSTMT), _conn_p(NULL), _commit_op(SQL_COMMIT),
    _row_count(0), _attr_row_count(NULL), _result_col_num(0),
    _fetch_node_p(NULL), _block(0), _rec(1)
{}

/*************************************************************
 * Destructor
 *************************************************************/
Operation::~Operation()
{
  SQLRETURN rc = SQL_SUCCESS;
  // free parms
  std::list<Parm*>::iterator it = _parms.begin();
  for ( ; it != _parms.end(); ++it )
  {
    delete *it;
  }
  
  // free stmt
  if ( SQL_NULL_HSTMT == _hstmt )
  {
    rc = SQLFreeStmt(_hstmt, SQL_DROP);
  }
}

/*************************************************************
 * Add a parm
 * 
 * Parm:
 *    parm    -  ptr of parm
 *************************************************************/
void Operation::addParm(Parm* parm)
{
  _parms.push_back(parm);
}

/*************************************************************
 * Copy operation result back to node
 *************************************************************/
void Operation::copyback()
{
  std::list<Parm*>::iterator it = _parms.begin();
  for ( ; it != _parms.end(); ++it )
  {
    Parm* parm = *it;
    parm->copyback();
  }
}

/*************************************************************
 * Init operation
 * 
 * Parm:
 *    fr    -  error info
 *************************************************************/
bool Operation::init(FlightRec& fr)
{
  bool ret = false;
  std::string n;
  std::string v;
  IAttribute* p = NULL;
  INode* node = NULL; // node value for attribute EXEC, EXECD, COMMIT...

  //------------------- exec | execDirect ----------------------------------
  n = "EXEC";
  p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    n = "EXECDIRECT";
    p = _node_p->getAttribute(n);
    if ( NULL != p )
    {
      _type = EXECD;
    }
  }
  else
  {
    _type = EXEC;
  }

  if ( NULL != p && NULL != (node = p->nodeValue()) )
  {
    ////------------- conn -------------------------------------
    n = "CONN";
    p = node->getAttribute(n);
    if ( NULL != p )
    {
      v = p->valueString();
      trim(v);
      _conn_lable = v;
      
      _conn_p = SqlUtil::getConnection(_conn_lable);
      if ( NULL == _conn_p )
      {
        fr.code(ERR_SQL_INIT);
        fr.addMessage(_conn_lable, "CONN LABEL NOT FOUND FOR EXEC.");
        return ret;        
      }
    }
    else
    {
      // if ( SqlUtil::isServerMode() )
      // {
      //   // If server mode, could not use default *LOCAL with current job user profile.
      //   if ( _conn_lable.length() == 0 )
      //   {
      //     fr.code(ERR_SQL_INIT);
      //     fr.addMessage("ATTRIBUTE CONN MUST BE SPECIFIED FOR OPERATION IN SERVER MODE.");
      //     return ret;           
      //   }
      // }
      // else
      // {
        // If non-server mode, use *LOCAL for on CONN specified
        _conn_lable = "*LOCAL";

        //------------------------------------------------------------------
        //-------- if no "CONN" sepcified, aka "*LOCAL", create sql --------
        //--------  connection to local db2 with current user. -------------
        //------------------------------------------------------------------
        SqlConn* conn = SqlUtil::getConnection(_conn_lable);
        if ( NULL == conn )
        {
          _conn_p = new SqlConn(_conn_lable.c_str(), "", ""); // db=*LOCAL for non-server mode
          if ( NULL == _conn_p )
          {
            abort();
          }
          else
          {
            SqlUtil::addConnection(_conn_p);
          }
        }
        else
        {
          _conn_p = conn;
        }
      }
    // }

    ////------------- rowcount -----------------------------
    n = "ROWCOUNT"; // affected row count by stmt
    p = node->getAttribute(n);
    if ( NULL != p )
    {
      _attr_row_count = p;
    }  

    ////------------- value --------------------------------
    n = "VALUE";
    p = node->getAttribute(n);
    if ( NULL != p )
    {
      v = p->valueString();
      trim(v);
      _value = v;
    }
    else
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("MISSING ATTRIBUTE VALUE FOR EXEC OR EXECDIRECT.");
      return ret;
    }

    ////------------- parm --------------------------------
    if ( EXEC == _type )
    {
      INode* parmNode = NULL;
      n = "PARM";
      p = node->getAttribute(n);
      if ( NULL != p && NULL != (parmNode = p->nodeValue()) )
      {
        //////---------- process each parm -------------------
        std::list<INode*>& parms = parmNode->childNodes();
        std::list<INode*>::iterator it = parms.begin();
        for ( ; it != parms.end(); ++it )
        {
          Parm* parm = new Parm(*it);
          if ( NULL == parm )
          {
            fr.code(ERR_SQL_INIT);
            fr.addMessage("FAILED TO CREATE PARM INSTANCE.");
            return ret;
          }
          else
          {
            _parms.push_back(parm);
            if ( ! parm->init(fr) )
            {
              return ret;
            }
          }
        }
      }
      else
      {
        fr.code(ERR_SQL_INIT);
        fr.addMessage("MISSING OR INVALID ATTRIBUTE PARM FOR EXEC.");
        return ret;
      }
    }

    ////------------- fetch (opt) -------------------------------
    n = "FETCH";
    p = node->getAttribute(n);
    if ( NULL != p && NULL != (_fetch_node_p = p->nodeValue()))
    {
      //////------------- block (opt) ---------------------------
      n = "BLOCK";
      p = _fetch_node_p->getAttribute(n);
      if ( NULL != p )
      {
        v = p->valueString();
        trim(v);
        cvt2Upper(v);
        if ( "ALL" == v )
        {
          _block = 0;
        }
        else
        {
          const char* s = v.c_str();
          char* e = NULL;
          errno = 0; // Easy check to avoid checking combinations of return value and errno.
          long long int lli = strtoll(s, &e, 10);
          if ( 0 == errno )
          {
            _block = (int)lli;
            if ( _block < 0 )
            {
              fr.code(ERR_SQL_INIT);
              fr.addMessage(_rec, "VALUE FOR ATTRIBUTE BLOCK SHOULD NOT BE NEGATIVE.");
              return ret;
            }
          }
          else
          {
            fr.code(ERR_SQL_INIT);
            fr.addMessage(errno, strerror(errno));
            fr.addMessage(v, "INVALID VALUE FOR BLOCK IN FETCH.");
            return ret;
          }
        }
      }
      else
      {
        _block = 0; // "ALL" by default
      }
      //////------------- rec (opt) ---------------------------
      n = "REC";
      p = _fetch_node_p->getAttribute(n);
      if ( NULL != p )
      {
        v = p->valueString();
        trim(v);
        const char* s = v.c_str();
        char* e = NULL;
        errno = 0; // Easy check to avoid checking combinations of return value and errno.
        long long int lli = strtoll(s, &e, 10);
        if ( 0 == errno )
        {
          _rec = (int)lli;
          if ( _rec <= 0 )
          {
            fr.code(ERR_SQL_INIT);
            fr.addMessage(_rec, "VALUE FOR ATTRIBUTE REC SHOULD NOT BE NEGATIVE.");
            return ret;
          }
        }
        else
        {
          fr.code(ERR_SQL_INIT);
          fr.addMessage(errno, strerror(errno));
          fr.addMessage(v, "INVALID VALUE FOR REC IN FETCH.");
          return ret;
        }
      }
      else
      {
        _rec = 1; // Not specified by default
      }
    }

    ret = true;
    return ret;
  }

  //------------------- commit --------------------------------
  n = "COMMIT";
  p = _node_p->getAttribute(n);
  if ( NULL != p && NULL != (node = p->nodeValue()) )
  {
    _type = COMMIT;
    ////------------- conn -------------------------------------
    n = "CONN";
    p = node->getAttribute(n);
    if ( NULL != p )
    {
      v = p->valueString();
      trim(v);
      _conn_lable = v;
      
      _conn_p = SqlUtil::getConnection(_conn_lable);
      if ( NULL == _conn_p )
      {
        fr.code(ERR_SQL_INIT);
        fr.addMessage(_conn_lable, "CONN LABEL NOT FOUND FOR COMMIT.");
        return ret;        
      }
      else
      { // conflicts with autocommit
        if ( _conn_p->autoCommit() )
        {
          fr.code(ERR_SQL_INIT);
          fr.addMessage(_conn_lable, "CONN ALREADY SET TO AUTOCOMMIT. DO NOT SPECIFIFY COMMIT.");
          return ret;
        }
      }
    }
    else
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("MISSING ATTRIBUTE CONN FOR COMMIT.");
      return ret;
    }

    ////------------- action --------------------------------
    n = "ACTION";
    p = node->getAttribute(n);
    if ( NULL != p )
    {
      v = p->valueString();
      cvt2Upper(v);
      trim(v);
      _value = v;
      if ( "COMMIT" == v ) 
      {
        _commit_op = SQL_COMMIT;
      }
      else if ( "ROLLBACK" == v ) 
      {
        _commit_op = SQL_ROLLBACK;
      }
      else if ( "COMMIT_HOLD" == v ) 
      {
        _commit_op = SQL_COMMIT_HOLD;
      }
      else if ( "ROLLBACK_HOLD" == v ) 
      {
        _commit_op = SQL_ROLLBACK_HOLD;
      }
      else
      {
        fr.code(ERR_SQL_INIT);
        fr.addMessage("INVALID VALUE FOR ATTRIBUTE ACTION IN EXECDIRECT.");
        return ret;
      }
    }
    else
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("MISSING ATTRIBUTE ACTION FOR EXECDIRECT.");
      return ret;
    }

    ret = true;
    return ret;
  }

  if ( !ret )
  {
    fr.code(ERR_SQL_INIT);
    fr.addMessage("NO VALID OPERATION TYPE DESCRIBED.");    
  }

  return ret;
}

/*************************************************************
 * Run the operation
 * 
 * Parm:
 *    fr    -  error info
 *************************************************************/
bool Operation::run(FlightRec& fr)
{
  bool ret = false;
  SQLRETURN rc = SQL_SUCCESS;

  _conn_p->connect(fr);
  if ( ERR_OK != fr.code() )
  {
    return ret;
  }

  if ( EXEC == _type || EXECD == _type ) 
  {
    rc = SQLAllocStmt(_conn_p->hdbc(), &_hstmt);
    if ( SQL_SUCCESS != rc )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAIL TO CALL SQLAllocStmt.");
      SqlUtil::sqlError(_conn_p->hdbc(), SQL_NULL_HSTMT, fr);
      return ret;
    }
    else
    {
      // set stmt attrs
      SQLINTEGER scroll = SQL_TRUE;
      rc = SQLSetStmtAttr(_hstmt, SQL_ATTR_CURSOR_SCROLLABLE, &scroll, SQL_NTS);
      if ( SQL_SUCCESS != rc )
      {
        fr.code(ERR_SQL_RUN);
        fr.addMessage("FAIL TO CALL SQLSetStmtAttr TO SET SCROLLABLE.");
        SqlUtil::sqlError(_conn_p->hdbc(), SQL_NULL_HSTMT, fr);
        return ret;
      }
    }

    if ( EXEC == _type ) 
    {
      rc = SQLPrepare(_hstmt, const_cast<char*>(_value.c_str()), SQL_NTS);
      if ( SQL_SUCCESS != rc )
      {
        fr.code(ERR_SQL_RUN);
        fr.addMessage("FAIL TO CALL SQLPrepare.");
        SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
        return ret;
      } 

      SQLSMALLINT parmCount = 0;
      rc = SQLNumParams(_hstmt, &parmCount);
      if ( SQL_ERROR == rc )
      {
        fr.code(ERR_SQL_RUN);
        fr.addMessage("FAIL TO CALL SQLNumParams.");
        SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
        return ret;
      }
      else
      {
        if ( parmCount != _parms.size() )
        {
          fr.code(ERR_SQL_RUN);
          fr.addMessage("PARAMETER COUNT MISMATCHED.");
          return ret;
        }
      }

      std::list<Parm*>::iterator it = _parms.begin();
      for (int i = 1; it != _parms.end(); ++it, ++i )
      {
        Parm* parm = *it;
        rc = SQLDescribeParam(_hstmt,                   // i - stmt handle
                              i,                        // i = parm number 
                              &(parm->sqlDataType()),   // o - sql data type
                              &(parm->parmSize()),      // o - parm (column) size
                              &(parm->decimalDigits()), // o - dec digits
                              &(parm->nullable()));     // o - allow nullable
        if ( SQL_ERROR == rc )
        {
          fr.code(ERR_SQL_RUN);
          fr.addMessage(i+1, "FAIL TO CALL SQLDescribeParam FOR THE NTH PARM.");
          SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
          return ret;
        }
        else
        {
          if ( ! parm->prepare4Bind(fr) )
          {
            fr.code(ERR_SQL_RUN);
            fr.addMessage(i+1, "THE NTH PARM IS INVALID FOR EXEC.");
            return ret;
          }
        }
      }

      it = _parms.begin();
      for (int i = 1; it != _parms.end(); ++it, ++i )
      {
        Parm* parm = *it;
        rc = SQLBindParameter(_hstmt,                       // i    - stmt handle
                              i,                            // i    - parm index
                              parm->io(),                   // i    - io type
                              parm->cDataType(),            // i    - C data type
                              parm->sqlDataType(),          // i    - sql data type
                              parm->parmSize(),             // i    - column size(parm size)
                              parm->decimalDigits(),        // i    - decimal digits
                              parm->parmValuePtr(),         // i/o  - io buffer
                              0,                            // i    - buf length(not used)
                              &(parm->parmValueLength()));  // i/o  - io buffer length
        if ( SQL_ERROR == rc )
        {
          fr.code(ERR_SQL_RUN);
          fr.addMessage(i+1, "FAIL TO CALL SQLBindParameter FOR THE NTH PARM.");
          SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
          return ret;
        }
      }

      rc = SQLExecute(_hstmt);
      if ( rc != SQL_SUCCESS && 
           rc != SQL_SUCCESS_WITH_INFO && 
           rc != SQL_NO_DATA_FOUND )
      {
        fr.code(ERR_SQL_RUN);
        fr.addMessage("FAIL TO CALL SQLExecute.");
        SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
        return ret;
      }

    }
    else // EXECD
    {
      rc = SQLExecDirect(_hstmt, const_cast<char*>(_value.c_str()), SQL_NTS);
      if (rc != SQL_SUCCESS && 
          rc != SQL_SUCCESS_WITH_INFO && 
          rc != SQL_NO_DATA_FOUND )
      {
        fr.code(ERR_SQL_RUN);
        fr.addMessage("FAIL TO CALL SQLExecDirect.");
        SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
        return ret;
      }
    }
    
    rc = SQLRowCount(_hstmt, &_row_count);
    if ( rc != SQL_SUCCESS )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAIL TO CALL SQLRowCount.");
      SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
      return ret;
    }

    rc = SQLNumResultCols(_hstmt, &_result_col_num);
    if ( rc != SQL_SUCCESS )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAIL TO CALL SQLNumResultCols.");
      SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);
      return ret;
    }

    // may fetch data for SELECT
    if ( ! fetch(fr) )
    {
      return ret;
    }

    // update "rowcount". If SELECT, it is -1 so update with how many fetch() got.
    if ( NULL != _attr_row_count ) 
    {    
      char num[32] = {'\0'};
      sprintf(num, "%d", _row_count);
      std::string count(num);
      _attr_row_count->setValue(count);
    }

    // free stmt 
    rc = SQLFreeStmt(_hstmt, SQL_DROP);
    _hstmt = SQL_NULL_HSTMT;
  }
  else if ( COMMIT == _type ) 
  {
    if ( _conn_p->commitTranct(_commit_op, _hstmt, fr) )
    {
      return ret;
    }
  }
  else
  {
    assert(false);
  }

  ret = true;
  return ret;
}

/*************************************************************
 * Fetch the result data for SELECT
 * 
 * Parm:
 *    fr    -  error info
 *************************************************************/
bool Operation::fetch(FlightRec& fr)
{
  const int MAX_COL_NAME_LEN = 64;

  bool            ret = true;
  SQLCHAR**       colname = NULL;
  SQLSMALLINT*    coltype = NULL;
  SQLSMALLINT*    appDataType = NULL;
  SQLSMALLINT     colnamelen = 0;
  SQLSMALLINT     nullable = 0;
  SQLINTEGER*     collen = NULL;
  SQLSMALLINT     scale = 0;
  SQLINTEGER*     outlen = NULL;
  SQLCHAR**       data = NULL;
  SQLRETURN       rc = 0;
  SQLINTEGER      i = 0;
  INode*          resNode = NULL;
  IAttribute*     resAttr = NULL;

  if ( NULL == _fetch_node_p ) // fetch not required by user
  {
    return ret;
  }

  colname = new SQLCHAR*[_result_col_num];
  for ( i = 0; i < _result_col_num; ++i )
  {
    colname[i] = new SQLCHAR[MAX_COL_NAME_LEN+1];
    memset(colname[i], '\0', MAX_COL_NAME_LEN+1);
  }
  coltype     = new SQLSMALLINT[_result_col_num];
  appDataType = new SQLSMALLINT[_result_col_num];
  collen      = new SQLINTEGER[_result_col_num];
  outlen      = new SQLINTEGER[_result_col_num];
  data        = new SQLCHAR*[_result_col_num];

  for (i = 0; i < _result_col_num; i++)
  {
    if ( SQL_ERROR == SQLDescribeCol(_hstmt,      // i - stmt handle
                            i+1,                  // i - col number
                            colname[i],           // o - col name buff
                            MAX_COL_NAME_LEN,     // i - col name buff size
                            &colnamelen,          // o - bytes available for col name
                            &coltype[i],          // o - sql data type
                            &collen[i],           // o - precision. Number for double-char.
                            &scale,               // o - only for SQL_DECIMAL, SQL_NUMERIC, SQL_TIMESTAMP
                            &nullable))           // o - SQL_NO_NULLS/SQL_NULLABLE
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage(i+1, "FAIL TO CALL SQLDescribeCol FOR NTH COLUMN.");
      SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);  
      ret = false;
      break;   
    }
    else
    {
      // By default expect sql cli return chars for most of the types
      appDataType[i] = SQL_C_CHAR;

      // just give a hint if this happens
      if ( colnamelen > MAX_COL_NAME_LEN )
      {
        fr.addMessage(i+1, "The NTH COLUMN NAME IS TRUNCATED.");
      }
    }

    // set data receiver size and app data type
    int length = collen[i] + 1;
    switch(coltype[i])
    {
      case SQL_WCHAR:
      case SQL_WVARCHAR:
        length = length*2;
        break;
      case SQL_NUMERIC:
      case SQL_SMALLINT:
      case SQL_INTEGER:
      case SQL_BIGINT:
        length = 20 + 1;
        break;
      case SQL_FLOAT:
      case SQL_REAL:
      case SQL_DOUBLE:
      case SQL_DECFLOAT:
      case SQL_DECIMAL:
        length = 128 + 1;
        break;
      case SQL_DATETIME:
      case SQL_DATE:
      case SQL_TIME:
      case SQL_TIMESTAMP:
        length = 30 + 1;
        break;
      case SQL_BINARY:
      case SQL_VARBINARY:
        appDataType[i] = SQL_C_BINARY;
        break;
      case SQL_CLOB:
      case SQL_DBCLOB:
        appDataType[i] = SQL_C_BLOB;
        break;
      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_GRAPHIC:
      case SQL_VARGRAPHIC:
      default:
        break;
    };

    // allocate memory to bind column                     
    data[i] = new SQLCHAR[length];
    if ( NULL == data[i] )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage(i+1, "FAIL TO ALLOCATE MEMORY FOR NTH COLUMN.");
      SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);  
      ret = false;
      break;
    }
    else
    {
      memset(data[i], '\0', length);
    }

    // bind columns to program vars, converting all types to CHAR
    if ( SQL_ERROR == SQLBindCol( _hstmt,           // i - stmt handle
                                  i+1,              // i - column number
                                  appDataType[i],   // i - application data type 
                                  data[i],          // o - store the column data
                    /*collen[i]*/ length,           // i - size of buffer available to store data
                                  &outlen[i]))      // o - size of data available to return
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage(i+1, "FAIL TO CALL SQLBindCol FOR NTH COLUMN.");
      SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);  
      ret = false;
      break;      
    }
  }

  /*******************************************************************
   * "fetch":
   * { "...":"...", ...
   *   "DATA":[{row}, {row}, ...] }
   ******************************************************************/
  resNode = _fetch_node_p->createNode(); 
  resNode->type(CONTAINER);
  _fetch_node_p->addAttribute(STS_DATA, resNode);

  bool firstLoop = true;
  int rowNum = 1;
  while ( ret )
  {
    if ( firstLoop )
    {
      rc = SQLFetchScroll(_hstmt, SQL_FETCH_ABSOLUTE, _rec);
      firstLoop = false;
    }
    else
    {
      rc = SQLFetchScroll(_hstmt, SQL_FETCH_NEXT, 0);
    }

    if ( SQL_NO_DATA_FOUND == rc )
    {
      break;
    }
    else if ( SQL_ERROR == rc )
    {
      fr.code(ERR_SQL_RUN);
      fr.addMessage("FAIL TO CALL SQLFetch.");
      SqlUtil::sqlError(_conn_p->hdbc(), _hstmt, fr);  
      ret = false;
      break;   
    }

    INode* row = resNode->createNode();  
    resNode->addNode(row);

    for (i = 0; ret && i < _result_col_num; i++)
    {
      if ( outlen[i] > collen[i] )
      {
        // fr.addMessage(rowNum, "> THE NTH ROW HAS TRUNCATED CLUMN DATA.");
        // fr.addMessage(i+1,    ">> THE NTH COLUMN DATA IS TRUNCATED.");
      }
      
      if (outlen[i] == SQL_NULL_DATA)
      {
        std::string attrName(colname[i]);
        std::string attrValue(""); //  for NULL
        row->addAttribute(attrName, attrValue);
      }
      else
      {
        std::string attrName(colname[i]);
        std::string attrValue;
        
        // Convert output format
        ValueType attrType = STRING;
        switch (appDataType[i])
        {
          case SQL_BINARY:    
          case SQL_VARBINARY:
            // outlen[i] != SQL_NTS
            attrValue.append(data[i]);
            ret = bin2HexChar(attrValue);
            break; 
          case SQL_CLOB: 
          case SQL_DBCLOB: 
            // outlen[i] != SQL_NTS
            attrValue.append(data[i], (outlen[i]>collen[i]) ? collen[i]:outlen[i]);
            ret = bin2HexChar(attrValue);
            break;
          case SQL_C_CHAR: /* SQL_NTS */
            /****************************************************
             * See what prepare4Bind() set for SqlBindCol() 
             ***************************************************/
            if (      SQL_GRAPHIC == coltype[i]     ||    
                      SQL_VARGRAPHIC == coltype[i]    )
            { // outlen[i]==SQL_NTS but use hex here
              attrValue.append(data[i], collen[i]);
              ret = bin2HexChar(attrValue);
            }
            else if ( SQL_WCHAR == coltype[i] )
            { // outlen[i]==SQL_NTS but use hex here
              attrValue.append(data[i], collen[i]);
              trimTrailing(attrValue, JC(SPACE));
              ret = bin2HexChar(attrValue);
            }
            else if ( SQL_WVARCHAR == coltype[i] )
            { // outlen[i]==SQL_NTS but use hex here
              attrValue.append(data[i], collen[i]);
              trimTrailing(attrValue, 0x00);
              ret = bin2HexChar(attrValue);
            }
            else if ( SQL_NUMERIC == coltype[i]   ||
                      SQL_SMALLINT == coltype[i]  ||
                      SQL_INTEGER == coltype[i]   ||
                      SQL_BIGINT == coltype[i]    ||
                      SQL_FLOAT == coltype[i]     ||
                      SQL_REAL == coltype[i]      ||
                      SQL_DOUBLE == coltype[i]    ||
                      SQL_DECFLOAT == coltype[i]  ||
                      SQL_DECIMAL == coltype[i] )
            {
              attrValue.append(data[i]);
              attrType = NUMBER;
            }
            else if ( SQL_VARCHAR == coltype[i] )
            {
              attrValue.append(data[i]);
            }
            else // SQL_CHAR
            {
              attrValue.append(data[i]);
              trimTrailing(attrValue, JC(SPACE));
            }
            break;
          default:
            assert(false);
            break;
        };
        if ( !ret )
        {
          fr.code(ERR_SQL_RUN);
          fr.addMessage(rowNum, "> THE NTH ROW HAS CONVERSION ERROR.");
          fr.addMessage(i+1,    ">> THE NTH COLUMN DATA FAILED TO CONVERT HEXADECIMAL CHARS."); 
        }
        else
        {
          IAttribute* attr = row->addAttribute(attrName, attrValue);
          attr->type(attrType);
        }
      }
    }

    ////------------ check if get enough records from result set -----------------
    if ( 0 == _block ) // retrieve all till the end 
    {
      // let next SqlFetchScroll() to detect in the next loop
    }
    else if ( _block > 0 && rowNum == _block)
    { // got enough records from result set
      break;
    }
    else
    {
      // need to get next record from result set
    }

    ++rowNum;
  }

  //----------- set number fetched for SELECT -----------------------
  if ( _row_count < 0 )
  {
    _row_count = rowNum;
  }

  //------------ clean up memory ------------------------------------
  for (i = 0; i < _result_col_num; i++)
  {
    delete[] colname[i];
    delete[] data[i];
  }
  delete[] colname;
  delete[] data;
  delete[] coltype;
  delete[] collen;
  delete[] outlen;

  return ret;
}


//================================================================//
//=======  SqlUtil ===============================================//
//================================================================//
std::list<SqlConn*>* SqlUtil::_conns_p = NULL;  
SQLHENV SqlUtil::_henv = SQL_NULL_HENV;
int SqlUtil::_server_mode = SQL_TRUE;

/***********************************************************
 * (Static method) Get connection by connection lablel.
 ***********************************************************/
SqlConn* SqlUtil::getConnection(const std::string& label)
{
  SqlConn* conn = NULL;

  if ( NULL != _conns_p && label.size() > 0 )
  {
    std::list<SqlConn*>::iterator it = _conns_p->begin();
    for ( ; it != _conns_p->end(); ++it )
    {
      ////////////////////////////////////////////////////////
      // Two types of conn label:                           //
      //     1. User defined in attr "conn"                 //
      //     2. Internal gen with "db"+"user"+"pwd"         //
      ////////////////////////////////////////////////////////
      std::string genLabel((*it)->db());
      genLabel += (*it)->user();
      genLabel += (*it)->pwd();

      if ( (*it)->lable() == label || genLabel == label  )
      {
        conn = *it;
        break;
      }
    }
  }

  return conn;
}

/***********************************************************
 * (Static method) Add connection into connection pool. 
 * Parm:
 *    conn  - connection to be added
 * Return:
 *    true  - added successfully
 *    fail  - either label or db name is duplicate
 ***********************************************************/
bool SqlUtil::addConnection(SqlConn* conn)
{
  bool ret = true;

  if ( NULL != SqlUtil::getConnection(conn->lable()) )
  {
    ret = false;
  }
  else
  {
    _conns_p->push_back(conn);
  }

  return ret;
}

/***********************************************************
 * (Static method) Clear up connections and henv 
 ***********************************************************/
void SqlUtil::clear()
{
  SQLRETURN rc = SQL_SUCCESS;
  if ( NULL != _conns_p )
  {
    // free connections
    std::list<SqlConn*>::iterator it = _conns_p->begin();
    for ( ; it != _conns_p->end(); ++it )
    {
      SqlConn* conn = *it;
      delete conn;
    }
    _conns_p->clear();

    // free env
    if ( SQL_NULL_HENV != _henv ) 
    {
      rc = SQLFreeEnv(_henv);
    }

    delete _conns_p;
    _conns_p = NULL;
    _henv = SQL_NULL_HENV;
    _server_mode = SQL_FALSE;
  }
}

/***********************************************************
 * (Static method) Get henv. 
 ***********************************************************/
SQLHENV SqlUtil::henv()
{
  return _henv;
}

/***********************************************************
 * (Static method) Get server mode setting. 
 ***********************************************************/
bool SqlUtil::isServerMode()
{
  return _server_mode == SQL_TRUE;
}

/***********************************************************
 * (Static method) Set server mode. 
 ***********************************************************/
void SqlUtil::serverMode(bool mode)
{
  if ( mode )
  {
    _server_mode = SQL_TRUE;
  }
  else
  {
    _server_mode = SQL_FALSE;
  }
}

/***********************************************************
 * (Static method) Initialize sql env if not yet.
 ***********************************************************/
bool SqlUtil::initEnv(FlightRec& fr)
{
  bool ret = true;

// #pragma exception_handler ( sqlutil_run_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
  if ( SQL_NULL_HENV == _henv )
  {
    int sKey = 0;
    int eKey = 0;
    getMsgKey(sKey);

    if ( SQL_ERROR == SQLAllocEnv(&_henv) )
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("FAILED TO CALL SQLAllocEnv.");
      if ( SQL_NULL_HENV != _henv )
      {
        SqlUtil::sqlError(SQL_NULL_HDBC, SQL_NULL_HSTMT, fr);
      }
      
      ret = false;
    }

    if ( SQL_SUCCESS != SQLSetEnvAttr(SqlUtil::henv(), SQL_ATTR_SERVER_MODE, 
                                      &_server_mode, SQL_NTS) )
    {
      ///////////////////////////////////////////////////////////////////////////
      // For http request, QSQSRVR job runs here and it doesn't allow chaning  //
      // server mode. But still need "servermode":"on" if in this job multiple //
      // connections are attempted.                                            //
      /////////////////////////////////////////////////////////////////////////// 
    }
  }

  if ( NULL == _conns_p )
  {
    _conns_p = new std::list<SqlConn*>();
    if ( NULL == _conns_p ) 
    {
      abort();
    }
  }

  return ret;

  // #pragma disable_handler
  // sqlutil_run_ex:
  //   fr.code(ERR_SQL_INIT);
  //   getMsgKey(eKey);
  //   getMessages(sKey, eKey, fr);
  //   SqlUtil::clear();
  //   ret = false;
  //   return ret;
}

/***********************************************************
 * (Static method) Execute a stmt. 
 ***********************************************************/
SqlConn* SqlUtil::connect(const char* db, const char* user, const char* pwd, FlightRec& fr)
{
  SqlConn* conn = NULL;

  std::string connLabel;
  connLabel += db;
  connLabel += user;
  connLabel += pwd;

  if ( ! SqlUtil::initEnv(fr) )
  {
    return conn;
  }

  conn = SqlUtil::getConnection(connLabel);
  if ( NULL == conn )
  {
    conn = new SqlConn(db, user, pwd);
    if ( NULL == conn )
    {
      abort();
    }
    else
    {
      if ( ! conn->connect(fr) )
      {
        delete conn;
        conn = NULL;
      }
      else
      {
        SqlUtil::addConnection(conn);
      }      
    }
  }
 
  return conn;  
}

/***********************************************************
 * (Static method) Get sql error. 
 ***********************************************************/
void SqlUtil::sqlError(SQLHDBC hdbc, SQLHSTMT hstmt, FlightRec& fr)
{
  SQLCHAR        buffer[SQL_MAX_MESSAGE_LENGTH + 1];
  SQLCHAR        sqlstate[SQL_SQLSTATE_SIZE + 1];
  SQLINTEGER     sqlcode;
  SQLSMALLINT    length;

  while ( SQL_SUCCESS == SQLError(_henv, 
                                  hdbc, 
                                  hstmt,
                                  sqlstate,
                                  &sqlcode,
                                  buffer,
                                  SQL_MAX_MESSAGE_LENGTH + 1,
                                  &length) )
  {
    fr.addMessage(sqlcode, "NATIVE SQL ERROR CODE."); // native error code 
    fr.addMessage(sqlstate, buffer);            // state + message
  };
}

/*************************************************************
 * Constructor 
 * 
 * Parm:
 *    node    -  ptr of node
 *************************************************************/
SqlUtil::SqlUtil(INode* node)
  : Runnable(node), _ops(), _conns2free(), _free_all(true)
{}

/*************************************************************
 * Destructor
 *************************************************************/
SqlUtil::~SqlUtil()
{
  // free operations
  std::list<Operation*>::iterator it = _ops.begin(); 
  for ( ; it != _ops.end(); ++it )
  {
    delete *it;
  }

  // free designated or ALL connections
  if ( NULL != _conns_p )
  {
    if ( _free_all )
    {
      SqlUtil::clear();
    }
    else
    {
      std::list<std::string>::iterator it2 = _conns2free.begin();
      std::list<SqlConn*>::iterator it3 = _conns_p->begin();
      for ( ; it2 != _conns2free.end(); ++it2 )
      {
        it3 = _conns_p->begin();
        for ( ; it3 != _conns_p->end(); ++it3 )
        {
          if ( (*it2) == (*it3)->lable()  )
          {
            delete *it3;
            _conns_p->erase(it3);
            break;
          }
        }
      }
      _conns2free.clear();
    }
  }
}

/*************************************************************
 * Init SqlUtil
 * 
 * Parm:
 *    fr    -  error info
 *************************************************************/
void SqlUtil::init(FlightRec& fr)
{

  //---------- inz connection cache ----------------------------------
  if ( NULL == _conns_p )
  {
    _conns_p = new std::list<SqlConn*>();
    if ( NULL == _conns_p ) 
    {
      abort();
    }
  }

  std::string n;
  std::string v;
  IAttribute* p = NULL;

  //--------------- inz "error" -------------------------------
  n = "ERROR";
  p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    _error = OFF;
  }
  else
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "OFF" == v )
    {
      _error = OFF;
    }
    else if ( "ON" == v )
    {
      _error = ON;
    }
    else
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE ERROR.");
      return;
    }
  }

  //--------------- inz "servermode" -------------------------------
  n = "SERVERMODE";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "OFF" == v )
    {
      _server_mode = SQL_FALSE;
    }
    else if ( "ON" == v )
    {
      _server_mode = SQL_TRUE;
    }
    else
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE SERVERMODE.");
      return;
    }
  }

  //--------------- inz "connect" (opt)-------------------------------
  n = "CONNECT";
  p = _node_p->getAttribute(n);

  if ( NULL != p )
  {
    INode* node = p->nodeValue();
    if ( NULL == node || node->nodesNum() < 1 )
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("EXPECTING ARRAY ELEMENTS FOR ATTRIBUTE CONNECT.");
      return;
    }
    else
    {
      std::list<INode*> nodes = node->childNodes();
      std::list<INode*>::iterator it = nodes.begin(); 
      int ind = 0;
      for ( ; it != nodes.end(); ++it, ind++ )
      {
        INode* connNode = *it;
        SqlConn* conn = new SqlConn(connNode);
        if ( NULL == conn )
        {
          abort();
        }

        if ( conn->init(fr) )
        {
          if ( ! SqlUtil::addConnection(conn) )
          {
            delete conn;
            return; 
          }
          else
          {}
        }
        else
        {
          delete conn;
          return;
        }
      }

    }
  }

  //--------------- inz "operation" -----------------------------------
  n = "OPERATION";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    INode* opsNode = p->nodeValue();
    if (  NULL == opsNode || opsNode->nodesNum() < 1)
    {
      fr.code(ERR_SQL_INIT);
      fr.addMessage("EXPECTING ARRAY ELEMENTS FOR ATTRIBUTE OPERATION.");
      return;
    }
    else
    {
      std::list<INode*> opNodes = opsNode->childNodes();
      std::list<INode*>::iterator it = opNodes.begin(); 
      for (int ind = 0; it != opNodes.end(); ++it, ++ind)
      {
        Operation* op = new Operation(*it);
        _ops.push_back(op);
        if ( ! op->init(fr) )
        {
          fr.code(ERR_SQL_INIT);
          fr.addMessage(ind+1, "FAILED TO PARSE THE NTH OPERATION.");
          return;
        }
      }
    }
  }

  //--------------- inz "free" -----------------------------------
  n = "FREE";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    INode* freeNode = p->nodeValue();
    if ( NULL == freeNode )  // string special value
    {
      v = p->valueString();
      cvt2Upper(v);
      trim(v);
      if ( "ALL" == v )
      {
        _free_all = true;
      }     
      else if ("NONE" == v ) 
      {
        _free_all = false;
      }
      else
      {
        fr.code(ERR_SQL_INIT);
        fr.addMessage(v, "INVLID VALUE SPECIFIED FOR ATTRIBUTE FREE.");
        return;
      }
    }
    else // array of indivisual conn labels
    {
      std::list<std::string> lables;
      freeNode->arrayElements(lables);
      std::list<std::string>::iterator it = lables.begin(); 
      for ( ; it != lables.end(); ++it )
      {
        std::string conn = *it;
        trim(conn);
        cvt2Upper(conn);
        _conns2free.push_back(conn);
        _free_all = false;
      }
    }
  }
}

/*************************************************************
 * Run the SqlUtil
 * 
 * Parm:
 *    fr    -  error info
 *************************************************************/
void SqlUtil::run(FlightRec& fr)
{ 
  bool success = true;
  int sKey = 0;
  int eKey = 0;
  if ( ON == _error )
  {
    if ( !getMsgKey(sKey) )
    {
      _error = OFF;
    }
  }

  //---------------------- create sql env -----------------------
  if ( ! SqlUtil::initEnv(fr) )
  {
    return;
  }

  //--------------- connect dbs -----------------------------------
  std::list<SqlConn*>::iterator it = _conns_p->begin();
  for ( ; success && it != _conns_p->end(); ++it )
  {
    SqlConn* conn = *it;
    if ( ! conn->connect(fr) )
    {
      success = false;
    }
  }

  //--------------- Process "operation" ----------------------------------
  std::list<Operation*>::iterator it2 = _ops.begin();
  for ( ; success && it2 != _ops.end(); ++it2 )
  {
    Operation* op = *it2;
    if ( ! op->run(fr) )
    {
      success = false;
    }
    else
    {
      op->copyback();
    }
  }

  if ( ON == _error && ERR_OK != fr.code() )
  {
    if ( getMsgKey(eKey) )
    {
      getMessages(sKey, eKey, fr);
    }
  }
  return;
}



