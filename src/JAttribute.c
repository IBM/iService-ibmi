#include "JAttribute.h"
#include "JNode.h"
#include "Utility.h"
#include <string>
#include <assert.h>

/*********************************************************************
 * Constructor
 ********************************************************************/
JAttribute::JAttribute()
  : IAttribute()
{}

/*********************************************************************
 * Constructor
 * 
 * Parm:
 *    name  -  attr name
 *    value -  string value
 ********************************************************************/
JAttribute::JAttribute(std::string name, std::string value)
  :  IAttribute(name, value)
{}

/*********************************************************************
 * Constructor
 * 
 * Parm:
 *    name      -  attr name
 *    dotNumber -  double value
 ********************************************************************/
JAttribute::JAttribute(std::string name, double dotNumber)
  :  IAttribute(name, dotNumber)
{}

/*********************************************************************
 * Constructor
 * 
 * Parm:
 *    name      -  attr name
 *    number    -  long long value
 ********************************************************************/
JAttribute::JAttribute(std::string name, long long number)
  :  IAttribute(name, number)
{}

/*********************************************************************
 * Constructor
 * 
 * Parm:
 *    name      -  attr name
 *    number    -  int value
 ********************************************************************/
JAttribute::JAttribute(std::string name, int number)
  :  IAttribute(name, number)
{}

/*********************************************************************
 * Constructor
 * 
 * Parm:
 *    name      -  attr name
 *    node      -  node value
 ********************************************************************/
JAttribute::JAttribute(std::string name, INode* node)
  : IAttribute(name, node)
{}

/*********************************************************************
 * Destructor
 ********************************************************************/
JAttribute::~JAttribute()
{}

/*********************************************************************
 * Escape double quote of attr value
 ********************************************************************/
void JAttribute::escape()
{
  std::string escaped;
  //------------ escape attribute name ----------------
  //// this doesn't seem necessary. ///////////////////

  //------------ escape attribute STRING value --------
  std::string::iterator it = _value_string.begin();
  for ( ; it != _value_string.end(); ++it )
  {
    /********************************************
     * Escape:
     *       "    ==>   \"
     *       \   ==>    \\
     *******************************************/
    if ( JC(DOUBLE_QUOTE) == *it )
    {
      escaped += JC(BACK_SLASH);
      escaped += JC(DOUBLE_QUOTE);
    }
    else if ( JC(BACK_SLASH) == *it )
    {
      escaped += JC(BACK_SLASH);
      escaped += JC(BACK_SLASH);
    }
    else
    {
      escaped += *it;
    }
  }

  if ( escaped.length() > 0 )
  {
    _value_string = escaped;
  }
}

/*************************************************
 * Output in string format
 *************************************************/
void JAttribute::output(std::string& out)
{
  escape();

  out += JC(DOUBLE_QUOTE);
  out += _name;
  out += JC(DOUBLE_QUOTE);

  out += JC(COLON);

  switch (_type)
  {
  case NODE:
    _node->output(out);
    break;
  case NUMBER:
    trim(_value_string);
    out += _value_string;
    break;
  case STRING:
    out += JC(DOUBLE_QUOTE);
    out += _value_string;
    out += JC(DOUBLE_QUOTE);
    break;  
  default:
    assert(true == false);
    break;
  }
}

/**************************************************************
 * Parms:
 *    start - start of buffer to parse
 *    end   - end of buffer to stop
 *    fr   - (output) error indicator
 * Return:
 *    ptr  - where the working ptr stop at   
 * Note:
 *    Only accept {...} for attribute CMD/PGM/SH/QSH/SQL.   
 **************************************************************/
const char* JAttribute::parse(const char* start, const char* end, FlightRec& fr)
{
  const char* pWhere = start;
  const char* ps = NULL; 
  bool done = false;
  bool found = false;
  
  //-------------- process attribute name -----------------------
  while ( !done && pWhere <= end )
  {
    if ( *pWhere == JC(SPACE) || *pWhere == JC(LF) ) 
    {
      ++pWhere;
    }
    else if ( *pWhere == JC(DOUBLE_QUOTE)) // found the starting "
    {
      ps = pWhere;
      ++pWhere;
      while ( pWhere <= end && !found ) 
      {
        if ( *pWhere == *ps && JC(BACK_SLASH) != *(pWhere-1) ) // " is found and not an escaped char
        {    
          found = true;
          break;
        }
        ++pWhere;
      }
      if ( found && pWhere-ps-1 > 0 )
      {
        _name.append(ps+1, pWhere-ps-1);
        ++pWhere;
        done = true;
      }
      else // fail to parse attribute name
      {
        fr.code(ERR_PARSE_FAIL);
        fr.addMessage("EXPECTING CLOSE \" FOR ATTRIBUTE NAME.");
        done = true;
      }
    }
    else
    {
      // failed to find the starting "
      fr.code(ERR_PARSE_FAIL);
      fr.addMessage("EXPECTING OPEN \" FOR ATTRIBUTE NAME.");
      done = true;
    }
  }
  
  if ( ERR_OK != fr.code() ) // fail to parse attribute name
  {
    return pWhere;
  }
  
  trim(_name);
  if ( _name.size() == 0 )
  {
    fr.code(ERR_PARSE_FAIL);
    fr.addMessage("ATTRIBUTE NAME SHOULD NOT BE EMPTY.");
  }

  //---------------- process  ':' -------------------------------------
  found = false;
  while ( !found && pWhere <= end )
  {
    if ( *pWhere == JC(SPACE) || *pWhere == JC(LF) )
    {
      ++pWhere;
    }
    else if ( *pWhere == JC(COLON))
    {
      found = true;
      ++pWhere;
    }
    else
    {
      // failed to locate ':'
      fr.code(ERR_PARSE_FAIL);
      fr.addMessage(_name, "THE ATTRIBUTE NOT FOLLOWED WITH A COLON.");
      break;
    }
  }

  if ( ERR_OK != fr.code() ) // fail to locate ':'
  {
    return pWhere;
  }

  //---------------- process attribute value --------------------------
  done = false;
  found = false;

  // Only accept { for attribute CMD/SH/QSH/PGM/SQL/JOBINFO 
  NodeType t = INode::getNodeType(_name);
  if ( t != CONTAINER && t != NORMAL ) 
  {
    for ( ; pWhere <= end && ( JC(SPACE) == *pWhere || JC(LF) == *pWhere ); ++pWhere ) {}
    if ( *pWhere != JC(OPEN_CURLY)  ) 
    {
      fr.code(ERR_PARSE_FAIL);
      fr.addMessage("EXPECTING { FOR ATTRIBUTE CMD/SH/QSH/PGM/SQL/JOBINFO.");
      return pWhere;
    }
  }

  while ( !done && pWhere <= end )
  {
    if ( *pWhere == JC(SPACE) || *pWhere == JC(LF) ) // ignore whitespace
    {
      ++pWhere;
    }
    else if ( *pWhere == JC(DOUBLE_QUOTE)) // value is NOT a node... string type...
    {
      ps = pWhere;
      ++pWhere;
      while ( pWhere <= end && !found ) // find the matched "
      {
        if ( *pWhere == *ps && JC(BACK_SLASH) != *(pWhere-1) ) // " found and not an escapled char
        {
          found = true;
          break;
        }
        ++pWhere;
      }
      if ( found && pWhere-ps > 0 )
      {
        _value_string.append(ps+1, pWhere-ps-1);
        type(STRING);
        ++pWhere;
        done = true;
      }
      else
      { // fail to parse attribute string value
        fr.code(ERR_PARSE_FAIL);
        fr.addMessage("EXPECTING \" FOR ATTRIBUTE STRING VALUE.");
        done = true;
      }
    }
    else if (  *pWhere == '1' ||
          *pWhere == '2' ||
          *pWhere == '3' ||
          *pWhere == '4' ||
          *pWhere == '5' ||
          *pWhere == '6' ||
          *pWhere == '7' ||
          *pWhere == '8' ||
          *pWhere == '9' ||
          *pWhere == '0' ||
          *pWhere == JC(DASH))// value is NOT a node... number type...
    {
      
      ps = pWhere;
      pWhere = ps;
      for (; pWhere <= end && !found; ++pWhere) 
      {
        if ( JC(COMMA) == *pWhere || JC(CLOSE_CURLY) == *pWhere )
        {
          found = true;
          break;
        }
      }
      if ( found && pWhere-ps > 0)
      {
        _value_string.append(ps, pWhere-ps);
        done = true;
      }
      else
      {
        // fail to parse attribute number value
        fr.code(ERR_PARSE_FAIL);
        fr.addMessage("EXPECTING , OR } FOR ATTRIBUTE NUMBER VALUE.");
        done = true;
      }
    }
    else if ( *pWhere == JC(OPEN_SQUARE) || *pWhere == JC(OPEN_CURLY) ) // value is a node
    { 
      _node = new JNode(t); // create node with type indicator
      if ( NULL == _node )
      {
        abort();
      }

      type(NODE); // set attribute type before parse node

      pWhere = _node->parse(pWhere, end, fr);
      if ( ERR_OK != fr.code() ) // fail to parse node
      {
        // leave the failed node there
      }
      else 
      {
        // node parsed successfully
      }

      done = true;
    }
    else
    {
      // failed to find the starting ' or " or [ or {
      fr.code(ERR_PARSE_FAIL);
      fr.addMessage("EXPECTING OPEN \" OR [ OR { FOR ATTRIBUTE VALUE.");
      done = true;
    }
  }

  return pWhere;
}

/*******************************************************
 * Pass instruction run to node value.
 *******************************************************/
void JAttribute::runNode(FlightRec& fr)
{
  if ( NODE == _type )
  {
    _node->run(fr);
  }
}