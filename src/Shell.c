#include "Shell.h"
#include "INode.h"
#include "Utility.h"
#include "IAttribute.h"
#include "Pase.h"

/******************************************************
 * Constructor with node
 *****************************************************/
Shell::Shell(INode* node)
  : Runnable(node), _value()
{}

/******************************************************
 * Destructor
 *****************************************************/
Shell::~Shell()
{}

/******************************************************
 * Initialize the pase shell
 * 
 * Parm:
 *    fr - error info
 *****************************************************/
void Shell::init(FlightRec& fr)
{
  std::string n;
  std::string v;

  n = "VALUE";
  IAttribute* p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    fr.code(ERR_SH_INIT);
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
      fr.code(ERR_SH_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE ERROR.");
      return;
    }
  }
}

/******************************************************
 * Run the pase shell
 * 
 * Parm:
 *    fr - error info
 *****************************************************/
void Shell::run(FlightRec& fr)
{
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
  Pase::runShell(_value.c_str(), out, fr);

  if ( ON == _error && ERR_OK != fr.code() )
  {
    if ( getMsgKey(eKey) )
    {
      getMessages(sKey, eKey, fr);
    }
  }
  else // put output into RESULT
  {
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
}