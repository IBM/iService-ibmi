#include <stdlib.h>
#include <stdio.h>
#include "FlightRec.h"
#include "Utility.h"
#include <stdarg.h>


/***********************************************************
 * Constructor
 ***********************************************************/
FlightRec::FlightRec() : _code(ERR_OK), _msgs(), _joblog(false)
{}

/***********************************************************
 * Constructor
 * 
 * Parm:
 *    code  - error code
 ***********************************************************/
FlightRec::FlightRec(int code) : _code(code), _msgs(), _joblog(false)
{}

/***********************************************************
 * Destructor
 ***********************************************************/
FlightRec::~FlightRec()
{
  clearMessages();

}

/***********************************************************
 * Send message to joblog
 * 
 * Parm:
 *    log      -  send to joblog as well.
 ***********************************************************/
void FlightRec::joblog(bool log)
{
  _joblog = log;
}

/***********************************************************
 * Send message to joblog
 * 
 * Parm:
 *    id      -  string of id
 *    msg     -  string of text
 ***********************************************************/
void FlightRec::logMessage(const std::string& id, const std::string& msg)
{
  std::string body(id);
  body += JC(SPACE);
  body += JC(DASH);
  body += JC(SPACE);
  body += msg;
  sendMsg2Joblog(body.c_str());
}

/***********************************************************
 * Get error code
 ***********************************************************/
const int FlightRec::code()
{
  return _code;
}

/***********************************************************
 * Set error code
 ***********************************************************/
void FlightRec::code(int code)
{
  _code = code;
}

/***********************************************************
 * Add a message into Flight recoder.
 * 
 * Parm:
 *    no      -  error no
 *    msg     -  message text
 ***********************************************************/
void FlightRec::addMessage(const std::string& msgId, const std::string& msgText)
{
  Pair* p = new Pair(msgId, msgText);
  if ( NULL == p )
  {
    abort();
  }
  _msgs.push_back(p);

  if ( _joblog )
  {
    logMessage(msgId, msgText);
  }
}

/***********************************************************
 * Add a message into Flight recoder.
 * 
 * Parm:
 *    value    -  error value
 *    msg      -  message text
 ***********************************************************/
void FlightRec::addMessage(const char* value, const char* msg)
{
  std::string id(INTERNAL_MSG_ID);
  std::string text(value);
  text += JC(SPACE);
  text += JC(DASH);
  text += JC(SPACE);
  text += msg;
  addMessage(id, text);
}

/***********************************************************
 * Add a message into Flight recoder.
 * 
 * Parm:
 *    no    -  error no
 *    msg   -  message text
 ***********************************************************/
void FlightRec::addMessage(int no, const char* msg)
{
  char n[32] = {'\0'};
  sprintf(n, "%d", no);
  std::string text(n);
  text  = n;
  text += JC(SPACE);
  text += JC(DASH);
  text += JC(SPACE);
  text += msg;
  std::string msgId(INTERNAL_MSG_ID);
  addMessage(msgId, text);
}

/***********************************************************
 * Add a message into Flight recoder with default error id.
 * 
 * Parm:
 *    msg   -  message text
 ***********************************************************/
void FlightRec::addMessage(const char* msg)
{
  std::string msgId(INTERNAL_MSG_ID);
  std::string msgText(msg);
  addMessage(msgId, msgText);
}

/***********************************************************
 * Append Pairs of <id, msg> into flight recorder
 * 
 * Parm:
 *    msgs - Pairs of <id, msg> held in flight recorder
 ***********************************************************/
void FlightRec::addMessages(const std::list<class Pair*>& msgs)
{
  std::list<class Pair*>::const_iterator it;
  for ( it = msgs.begin(); it != msgs.end(); ++it )
  {
    addMessage((*it)->name(), (*it)->value());
  }
}

/***********************************************************
 * Return Pairs of <id, msg> held in flight recorder
 * 
 * Return:
 *    Pairs of <id, msg> held in flight recorder
 ***********************************************************/
const std::list<class Pair*>& FlightRec::messages()
{
  return _msgs;
}

/***********************************************************
 * Set Pairs of <id, msg> into flight recorder
 * 
 * Parm:
 *    msgs - Pairs of <id, msg> held in flight recorder
 ***********************************************************/
void FlightRec::messages(const std::list<class Pair*>& msgs)
{
  clearMessages();
  std::list<class Pair*>::const_iterator it;
  for ( it = msgs.begin(); it != msgs.end(); ++it )
  {
    addMessage((*it)->name(), (*it)->value());
  }
}

/***********************************************************
 * Check if flight recorder is empty
 ***********************************************************/
bool FlightRec::hasMessages()
{
  return !_msgs.empty();
}

/***********************************************************
 * Clean up messages flight recorder
 ***********************************************************/
void FlightRec::clearMessages()
{
  std::list<class Pair*>::iterator it;
  for ( it = _msgs.begin(); it != _msgs.end(); ++it )
  {
    delete *it;
  }
  _msgs.clear();
}

/***********************************************************
 * Reset error to ERR_OK and clear all messages held
 ***********************************************************/
void FlightRec::reset()
{
  _code = ERR_OK;
  _joblog = false;
  clearMessages();
}

/***********************************************************
 * Ouput all messages in a single string with give ccsid
 * 
 * Parm:
 *      str   -  (output) output buffer
 *      ccsid -  ccsid to be converted for output
 ***********************************************************/
void FlightRec::toString(std::string& str, int ccsid)
{
  int i = 1;
  char suffix[32] = {'\0'}; 
  //------------- assemble msgs to a single string ---------------------
  std::list<class Pair*>::iterator it;
  for ( it = _msgs.begin(); it != _msgs.end(); ++it )
  {
    str += (*it)->name();
    if ( (*it)->name() == INTERNAL_MSG_ID)
    {
      sprintf(suffix, "%03d", i);
      ++i; 
      str += suffix;
    }
    else
    {
      // leave it as they are system messags.
    }

    str += JC(SPACE);
    str += JC(COLON);
    str += JC(SPACE);

    str += (*it)->value();

    str += JC(SEMICOLON);
    str += JC(SPACE);
    str += JC(LF);
  }

  //------------- convert to specified ccsid --------------------------
  if ( 0 != ccsid ) // input not job ccsid
  {
    size_t iLeft   = str.length();
    size_t oLen   = iLeft * 4;
    size_t oLeft  = oLen;
    char* pIn     = const_cast<char*>(str.c_str());
    char* pOut    = new char[oLen];
    if ( NULL == pOut )
    {
      abort();
    }
    char* pOut2 = pOut;

    int rc = convertCcsid(0, ccsid, &pIn, &iLeft, &pOut2, &oLeft);
    if ( 0 == rc )
    {
      str.clear();
      str.append(pOut, oLen - oLeft);
    }  

    delete[] pOut;  
  }  
}