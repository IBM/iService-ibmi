#include "Qshell.h"
#include "INode.h"
#include "Utility.h"
#include "IAttribute.h"
#include "Pase.h"
#include "JobUtil.h"
#include <stdlib.h>

/******************************************************
 * Constructor
 *****************************************************/
Qshell::Qshell(INode* node)
  : Runnable(node), _value()
{}

/******************************************************
 * Destructor
 *****************************************************/
Qshell::~Qshell()
{}

/******************************************************
 * Initialize qshell
 * 
 * Parm:
 *    fr  -   error info
 *****************************************************/
void Qshell::init(FlightRec& fr)
{
  std::string n;
  std::string v;

  n = "VALUE";
  IAttribute* p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    fr.code(ERR_QSH_INIT);
    fr.addMessage("EXPECTING ATTRIBUTE VALUE.");
    return;
  }
  else
  {
    _value = p->valueString();
    trim(_value);
  }

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
      fr.code(ERR_QSH_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE ERROR.");
      return;
    }
  }
}

/******************************************************
 * Run the qshell
 * 
 * Parm:
 *    fr  -   error info
 *****************************************************/
void Qshell::run(FlightRec& fr)
{
  static bool isQshRdy = false;

  int sKey = 0;
  int eKey = 0;
  if ( ON == _error )
  {
    if ( !getMsgKey(sKey) )
    {
      _error = OFF;
    }
  }

  std::string out;
  
  //----------------- Create temporary file to hold qsh output -------------------------
  std::string outFile("/TMP/ISERVICE");
  outFile += JobUtil::JobName();
  trim(outFile);
  outFile += JobUtil::JobNumber();
  trim(outFile);
  outFile += JobUtil::UserName();
  trim(outFile);
  
  if ( !isQshRdy )
  {
    std::string addEnvVar;
    addEnvVar = "ADDENVVAR ENVVAR(QIBM_QSH_CMD_OUTPUT) VALUE('FILE=" + outFile + 
                    "') CCSID(*JOB) LEVEL(*JOB) REPLACE(*YES)";
    doCommand(addEnvVar, fr);
    if ( ERR_OK != fr.code() )
    {
      return;
    }
    addEnvVar = "ADDENVVAR ENVVAR(QIBM_QSH_CMD_ESCAPE_MSG) VALUE(Y) CCSID(*JOB) LEVEL(*JOB) REPLACE(*YES)";
    doCommand(addEnvVar, fr);
    if ( ERR_OK != fr.code() )
    {
      return;
    }

    isQshRdy = true;
  }

  //----------------- Invoke qsh command -----------------------------------------------
  std::string cmd;
  cmd += "STRQSH CMD('";
  
  // escape single quote
  std::string escaped;
  for ( std::string::iterator it = _value.begin();
        it < _value.end(); ++it )
  {
    if ( *it == JC(SINGLE_QUOTE) )
    {
      escaped += *it;
      escaped += *it;
    }
    else
    {
      escaped += *it;
    }
  }
  
  cmd += escaped; 
  cmd += "')";
  doCommand(cmd, fr);

  if ( ON == _error && ERR_OK != fr.code() )
  {
    if ( getMsgKey(eKey) )
    {
      getMessages(sKey, eKey, fr);
    }
  }
  else // put output into RESULT
  {
    //----------------- Process qsh output from temporary file ---------------------------
    readIfs(outFile.c_str(), out, 0, fr);
    if ( ERR_OK != fr.code() )
    {
      return;
    }

    if ( out.length() > 0 )
    {
      //-----------------------------------------------
      //  "DATA" : [ "aaa", "bbb", "ccc", ...]
      //-----------------------------------------------
      INode* retNode = _node_p->getStatusNode();
      INode* resultNode = _node_p->createNode();
      resultNode->type(CONTAINER);
      retNode->addAttribute(STS_DATA, resultNode);

      // split output into lines
      size_t start = 0;
      std::size_t found = 0;
      while ( std::string::npos != (found = out.find(JC(LF), start)) 
              && start < out.length() )
      {
        resultNode->addArrayElement(out.substr(start, found-start));
        start = found + 1;
      }
      if ( start < out.length()-1 )
      {
        resultNode->addArrayElement(out.substr(start));
      }
    }
  }

  removeIfs(outFile.c_str());
}