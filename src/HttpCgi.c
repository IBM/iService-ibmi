#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <assert.h>
#include <algorithm>
#include "FlightRec.h"
#include "ILE.h"
#include "Utility.h"
#include "SqlUtil.h"
#include "Config.h"

/**********************************************************
 * Definitions for stored procedure call.
 *********************************************************/
const char PARMS[]      = "(?,?,?)";
const char IPLUG4K[]     = "IPLUG4K";
const char IPLUG32K[]    = "IPLUG32K";
const char IPLUG65K[]    = "IPLUG65K";
const char IPLUG512K[]   = "IPLUG512K";
const char IPLUG1M[]     = "IPLUG1M";
const char IPLUG5M[]     = "IPLUG5M";
const char IPLUG10M[]    = "IPLUG10M";
const char IPLUG15M[]    = "IPLUG15M";


/***********************************************************
 * State engine for CGI calls.
 **********************************************************/
const int CGI_STS_ERROR_FR      = -1;
const int CGI_STS_DONE          = 0;
const int CGI_STS_RTV_URL       = 1;
const int CGI_STS_CHOP_PARMS    = 2;
const int CGI_STS_LOGON         = 3;
const int CGI_STS_RUN           = 4;
const int CGI_STS_RESPONSE      = 5;


/******************************************************************
 * User profile is QTMHHTP1 when invoked by IBM i Apache.
 * --------------------------------
 * /www/apachedft/conf/httpd.conf:
 * --------------------------------
 *  ScriptAlias /iservice/ /QSYS.LIB/ISERVICE.LIB/
 *  <Directory /QSYS.LIB/ISERVICE.LIB/>
 *    AllowOverride None
 *    order allow,deny
 *    allow from all
 *    SetHandler cgi-script
 *    Options +ExecCGI
 *  </Directory>
 * 
 * Restart http defaut server:
 * > QSYS/STRTCPSVR SERVER(*HTTP) RESTART(*HTTP) HTTPSVR(APACHEDFT)
 * 
 * Example:
 * http://host/iservice/httpcgi.pgm?db2=LOCALHOST&uid=guest&pwd=password&ctl=*ipc(/tmp/k1)*waitclient(120)*waitdata(2)&in={"a":"v"}&out=500
 * 
 * CURL: ( Need to escape { } ==> \{ \} )
 *   curl  'http://host/iservice/httpcgi.pgm?db2=LOCALHOST&uid=guest&pwd=password&ctl=*ipc(/tmp/k1)&in=\{"a":"v"\}&out=500'
 * 
 * HTTPS: 
 *  Config at http://<host>:2001/HTTPAdmin:
 *   HTTP Servers >> APACHEDFT >> HTTP Tasks and Wizards 
 *    >> Configure SSL
 * 
 *  Test: (for test/html/*.html):
 *      <VirtualHost *:443>
 *        ...
 *        Header add Access-Control-Allow-Origin "*"
 *      </VirtualHost> 
 * 
 * -------------- 
 * HTTP Workflow:
 * --------------
 * APACHEDFT(QTMHHTP1)
 *  |
 *  +--> httpcig.pgm(*ENTMOD)  |
 *        +--> SQLConnect db2 
 *        |     | 
 *        |     +---------------> QSQSRVR(GUEST) starts 
 *        |                        |                               *ipc
 *        +--> Call storedp -------+--> storedp.svrpgm(*CALLER) <---------> IPC server
 *        |                           
 *        +--> SQLDisconnect db2
 *              |
 *              +---------------> QSQSRVR(GUEST) ends
 *****************************************************************/

extern "C" int ebcdic_unescape_url(char* buff);
int unescapeUrl(std::string& url, bool mixed)
{
  int rc = true;
  char* pUrl = const_cast<char*>(url.c_str());
  if ( mixed )
  {
    // FUTURE: not support mixed ccsid yet
    rc = -999;
  }
  else
  {
    
    rc = ebcdic_unescape_url(pUrl);
  }

  return rc;
}

#pragma convert(37)
/****** all in ccsid 37 from ibm i http ********************/
const char TAG_DB2[]  = "&db2=";
const char TAG_UID[]  = "&uid=";
const char TAG_PWD[]  = "&pwd=";
const char TAG_CTL[]  = "&ctl=";
const char TAG_IN[]   = "&in=";
const char TAG_OUT[]  = "&out=";

const char CTL_IN_OUT_CCSID[] = "*BEFORE(37)*AFTER(1208)";
#pragma convert(0)

/**********************************************************
 * Get parms from HTTP request
 * 
 * Parm:
 *    url   -   (output) URL string
 *    db2   -   (output) database name
 *    uid   -   (output) user id
 *    pwd   -   (output) password
 *    ctl   -   (output) control flags
 *    in    -   (output) input
 *    out   -   (output) size of output
 *    fr    -   (output) error information
 * 
 * Return:
 *    true/false - suceed to parse parms or not
 *********************************************************/
bool chopParms(std::string& url,
               std::string& db2,    
               std::string& uid,
               std::string& pwd,
               std::string& ctl,
               std::string& in,
               int*         out,  
               FlightRec&   fr )
{
  //------------- add & ahead of url ------------------
  std::string url2(1, 0x50); 
  url2.append(url);
  
  //------------- Locate all parms --------------------
  const int PARM_NUM = 6;
  int positions[PARM_NUM + 1] = { url2.length() };

  int db2Pos = url2.find(TAG_DB2);
  positions[0] = db2Pos;
  if ( std::string::npos == db2Pos )
  {
    fr.code(ERR_CGI_CHOP_PARMS);
    fr.addMessage("MISSING URL PARAMETER 'db2'.");
    return false;
  } 

  int uidPos = url2.find(TAG_UID);
  positions[1] = uidPos;
  if ( std::string::npos == uidPos )
  {
    fr.code(ERR_CGI_CHOP_PARMS);
    fr.addMessage("MISSING URL PARAMETER 'uid'.");
    return false;
  }  

  int pwdPos = url2.find(TAG_PWD);
  positions[2] = pwdPos;
  if ( std::string::npos == pwdPos )
  {
    fr.code(ERR_CGI_CHOP_PARMS);
    fr.addMessage("MISSING URL PARAMETER 'pwd'.");
    return false;
  }

  int ctlPos = url2.find(TAG_CTL);
  positions[3] = ctlPos;
  if ( std::string::npos == ctlPos )
  {
    fr.code(ERR_CGI_CHOP_PARMS);
    fr.addMessage("MISSING URL PARAMETER 'ctl'.");
    return false;
  }  

  int inPos  = url2.find(TAG_IN); 
  positions[4] = inPos;
  if ( std::string::npos == inPos )
  {
    fr.code(ERR_CGI_CHOP_PARMS);
    fr.addMessage("MISSING URL PARAMETER 'in'.");
    return false;
  }  

  int outPos = url2.find(TAG_OUT); 
  positions[5] = outPos;
  if ( std::string::npos == outPos )
  {
    fr.code(ERR_CGI_CHOP_PARMS);
    fr.addMessage("MISSING URL PARAMETER 'out'.");
    return false;
  }

  //-------- sort parm postions --------------------
  std::sort(positions, positions + PARM_NUM);

  //-------- process each parm value ---------------
  for ( int i = 0; i < PARM_NUM; ++i )
  {
    if      ( positions[i] == db2Pos )
    {
      int start  = db2Pos + strlen(TAG_DB2);
      int length = positions[i+1] - start;
      std::string s = url2.substr(start, length);
      db2.append(s);
      trim(db2);
      // cvt2Upper(db2);
    }
    else if ( positions[i] == uidPos )
    {
      int start  = uidPos + strlen(TAG_UID);
      int length = positions[i+1] - start;
      std::string s = url2.substr(start, length);
      uid.append(s);
      trim(uid);
    }
    else if ( positions[i] == pwdPos )
    {
      int start  = pwdPos + strlen(TAG_PWD);
      int length = positions[i+1] - start;
      std::string s = url2.substr(start, length);
      pwd.append(s);
    }
    else if ( positions[i] == ctlPos )
    {
      ctl = CTL_IN_OUT_CCSID;

      int start  = ctlPos + strlen(TAG_CTL);
      int length = positions[i+1] - start;
      std::string s = url2.substr(start, length);
      ctl.append(s);
      trim(ctl);
      cvt2Upper(ctl);
    }
    else if ( positions[i] == inPos )
    {
      int start  = inPos + strlen(TAG_IN);
      int length = positions[i+1] - start;
      std::string s = url2.substr(start, length);
      in.append(s);
      // trim(in);
      // cvt2Upper(in);
    }
    else if ( positions[i] == outPos )
    {
      int start  = outPos + strlen(TAG_OUT);
      int length = positions[i+1] - start;
      std::string s = url2.substr(start, length);

      *out = atoi(s.c_str());
    }
    else
    {
      assert(false);
    }
  }

  return true;
}


/********************************************************
 * HTTP CGI call entrance. No arguments required and 
 * get HTTP request from job level envvar.
 *******************************************************/
int main( int argc, char* argv[] )
{
  FlightRec fr;

  /** i http apache encode url in ccsid 37 **/
  std::string url;
  
  IO_Format ioFormat = JSON;

  std::string out;
  SqlConn*    conn = NULL; 

  /** db2 connect parms *********************/
  std::string db2;
  std::string uid;
  std::string pwd;
  std::string ctl;
  std::string in;
  int         out_size;

#pragma exception_handler ( main_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
  int status = CGI_STS_RTV_URL;
  while ( true )
  {
    /*********************************************************/
    if      ( CGI_STS_RTV_URL == status )
    /*********************************************************/
    {
      status = CGI_STS_CHOP_PARMS;

      char* method = getenv("REQUEST_METHOD");
      if ( NULL == method )
      {
        fr.code(ERR_CGI_RTV_URL);
        fr.addMessage("REQUEST METHOD NOT SET IN.");        
      }
      else if ( 0 == strcmp(method, "POST") || 
                0 == strcmp(method, "PUT")  || 
                0 == strcmp(method, "DELETE") )
      {
        int size = atoi(getenv("CONTENT_LENGTH"));
        if ( size <= 0 )
        {
          fr.code(ERR_CGI_RTV_URL);
          fr.addMessage(size, "THE SIZE OF CONTENT_LENGTH NOT VALID.");
        }
        else
        {
          const int BUFF_SIZE = 1024;
          int readN = 0;
          char buff[BUFF_SIZE] = {'\0'};
          while ( true )
          {
            readN = read(0, buff, BUFF_SIZE);
            if ( readN < 0 )
            {
              fr.code(ERR_CGI_RTV_URL);
              fr.addMessage(errno, "ERROR RECIEVED IN READING CONTENT.");  
              break;            
            }

            url.append(buff, readN);

            if ( readN < BUFF_SIZE )
            { // done with reading
              break;
            }
          }  
        }
      }
      else if ( 0 == strcmp(method, "GET") )
      {
        char* qryString = getenv("QUERY_STRING");
        if ( NULL == qryString )
        {
          fr.code(ERR_CGI_RTV_URL);
          fr.addMessage("NO QUERY STRING SET IN.");
        }
        else
        {
          url.append(qryString);
        }
      }
      else
      {
        fr.code(ERR_CGI_RTV_URL);
        fr.addMessage(method, "HTTP REQUEST METHOD NOT SUPPORTED.");
      }

      if ( ERR_OK != fr.code() )
      {
        status = CGI_STS_ERROR_FR;
      }

    }
    /*********************************************************/
    else if ( CGI_STS_CHOP_PARMS == status )
    /*********************************************************/
    {
      status = CGI_STS_LOGON;

      bool isMixed = false;
      char* cgiMode = getenv("CGI_MODE");
      if ( NULL != cgiMode && NULL != strstr(cgiMode, "MIXED") )
      { // mixed
        isMixed = true;
      }

      int rc = unescapeUrl(url, isMixed);

      if ( rc < 0 )
      {
        fr.code(ERR_CGI_CHOP_PARMS);
        fr.addMessage(rc, "ERROR OCCURRED IN UNESCAPE URL.");
      }
      else
      {
        chopParms(url, db2, uid, pwd, ctl, in, &out_size, fr);
      }

      if ( ERR_OK != fr.code() )
      {
        status = CGI_STS_ERROR_FR;
      }

    }
    /*********************************************************/
    else if ( CGI_STS_LOGON == status )
    /*********************************************************/
    {
      status = CGI_STS_RUN;

      SqlUtil::serverMode(true);
      conn = SqlUtil::connect(db2.c_str(), uid.c_str(), pwd.c_str(), fr);
      if ( NULL == conn )
      {
        status = CGI_STS_ERROR_FR;
      }
    }
    /*********************************************************/
    else if ( CGI_STS_RUN == status )
    /*********************************************************/
    {
      status = CGI_STS_RESPONSE;

      std::string stmt("CALL ");
      stmt += iServLib;
      stmt += ".";

      if      ( out_size > 10485760 )
      {
        stmt += IPLUG15M;
      }
      else if ( out_size > 5242880 )
      {
        stmt += IPLUG10M;
      }
      else if ( out_size > 1048576 )
      {
        stmt += IPLUG5M;
      }
      else if ( out_size > 524288 )
      {
        stmt += IPLUG1M;
      }
      else if ( out_size > 65536 )
      {
        stmt += IPLUG512K;
      }
      else if ( out_size > 32768 )
      {
        stmt += IPLUG65K;
      }
      else if ( out_size > 4096 )
      {
        stmt += IPLUG32K;
      }
      else
      {
        stmt += IPLUG4K;
      }

      stmt += PARMS;

      Operation* op = new Operation(conn, stmt, EXEC);
      if ( NULL == op )
      {
        abort();
      }

      Parm* parm1 = new Parm(SQL_PARAM_INPUT, ctl);
      if ( NULL == parm1 )
      {
        abort();
      }
      op->addParm(parm1);

      Parm* parm2 = new Parm(SQL_PARAM_INPUT, in);
      if ( NULL == parm2 )
      {
        abort();
      }
      op->addParm(parm2);

      Parm* parm3 = new Parm(SQL_PARAM_OUTPUT, in);
      if ( NULL == parm3 )
      {
        abort();
      }
      op->addParm(parm3);

      op->run(fr);
      
      if ( ERR_OK != fr.code() )
      {
        status = CGI_STS_ERROR_FR;
      }
      else
      {
        int size = out_size > parm3->parmValueLength() ? parm3->parmValueLength() : out_size;
        out.append(static_cast<char*>(parm3->parmValuePtr()), size);
        trim(out);
      }

      delete op;
      op = NULL;

    }
    /*********************************************************/
    else if ( CGI_STS_RESPONSE == status )
    /*********************************************************/
    {
      status = CGI_STS_DONE;

      //------------------- write header --------------------------------------
      char lineBreak[] = { 0x15, 0x15, '\0' }; // why not ASCII { 0x0D, 0x0A, '\0' };
      std::string head("Content-type: application/json; charset=utf-8");
      if ( XML == ioFormat )
      {
        head = "Content-type: text/xml; charset=utf-8";
      }
      head.append(lineBreak);

      int writeN = write(1, head.c_str(), head.length());
      if ( writeN < 0 )
      {}

      //------------------- write json -----------------------------------------
      writeN = write(1, out.c_str(), out.length());
      if ( writeN < 0 )
      {}
    }
    /*********************************************************/
    else if ( CGI_STS_ERROR_FR == status )
    /*********************************************************/
    {      
      fr.toString(out, 1208);
      status = CGI_STS_RESPONSE;
    }
    /*********************************************************/
    else if ( CGI_STS_DONE == status )
    /*********************************************************/
    {
      // Release QSOSRVR job
      SqlUtil::clear();
      
      break;
    }
    /*********************************************************/
    else        // unknow //
    /*********************************************************/
    {
      assert(false);
    }
  }

  return 0;

#pragma disable_handler
  main_ex:
    SqlUtil::clear();

  return 99;
}