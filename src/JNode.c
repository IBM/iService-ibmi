#include <assert.h>
#include "JNode.h"
#include "JAttribute.h"
#include "Common.h"
#include "Utility.h"
#include "Runnable.h"
#include <stdio.h>
#include "Control.h"
#include "sys/time.h"

/******************************************************
 * Constructor
 *****************************************************/
JNode::JNode()
  : INode()
{} 

/******************************************************
 * Constructor with type
 *****************************************************/
JNode::JNode(NodeType type)
  : INode(type)
{}

/******************************************************
 * Destructor
 *****************************************************/
JNode::~JNode()
{}

/******************************************************
 * Add attributes.
 * 
 * Parm:
 *    pairs - container with pairs of <attrName, attrValue>
 *****************************************************/
void JNode::addAttributes(const std::list<class Pair*>& pairs)
{
  int ind = 0;
  IAttribute* pAttr = NULL;
  for ( std::list<class Pair*>::const_iterator it = pairs.begin(); 
          it != pairs.end(); ++it )
  {
    std::string name = (*it)->name();
    // avoid same attribute name for INTERNAL
    char suffix[3] = {'\0'};
    if ( INTERNAL_MSG_ID == name )
    {
      ++ind;
      sprintf(suffix, "%03d", ind);
      name.append(suffix);
    }
    pAttr = new JAttribute(name, (*it)->value());
    if ( NULL == pAttr )
    {
      abort();
    }
    else
    {
      _attrs.push_back(pAttr);
    }
  }
}
/******************************************************
 * Add attributes.
 * 
 * Parm:
 *    attrs - container with ptrs of <attrName, attrValue>
 *****************************************************/
void JNode::addAttributes(const std::list<IAttribute*>& attrs)
{
  IAttribute* pAttr = NULL;
  for ( std::list<IAttribute*>::const_iterator it = attrs.begin(); 
          it != attrs.end(); ++it )
  {

    pAttr = new JAttribute( (*it)->name(), (*it)->valueString()); 
    if ( NULL == pAttr )
    {
      abort();
    }
    else
    {
      _attrs.push_back(pAttr);
    }
  }
}

/******************************************************
 * Create a JSON node.
 * 
 * Return:
 *    ptr to new created JSON node
 *****************************************************/
INode* JNode::createNode()
{
  INode* node = new JNode(NORMAL);
  if ( NULL == node )
  {
    abort();
  }
  return node;
}

/******************************************************
 * Add an attribute.
 * 
 * Parm:
 *    name - attr name
 *    node - ptr of node
 *    top  - add to the top. Default is false.
 *****************************************************/
IAttribute* JNode::addAttribute(const std::string& name, INode* node, bool top)
{
  assert( NULL != node);
  IAttribute* attr = new JAttribute(name, node);
  if ( NULL == attr )
  {
    abort();
  }
  if ( top )
  {
    _attrs.push_front(attr);
  }
  else
  {
    _attrs.push_back(attr);
  }
  
  return attr;
}

/******************************************************
 * Add an attribute.
 * 
 * Parm:
 *    name    - attr name
 *    value   - attr value
 *    top     - add to the top. Default is false.
 *****************************************************/
IAttribute* JNode::addAttribute(const std::string& name, const std::string& value, bool top)
{
  IAttribute* attr = new JAttribute(name, value);
  if ( NULL == attr )
  {
    abort();
  }
  if ( top )
  {
    _attrs.push_front(attr);
  }
  else
  {
    _attrs.push_back(attr);
  }
  return attr;
}

/******************************************************
 * Add an integer value attribute.
 * 
 * Parm:
 *    name    - attr name
 *    value   - attr integer value
 *    top     - add to the top. Default is false.
 *****************************************************/
IAttribute* JNode::addAttribute(const std::string& name, int value, bool top)
{
  IAttribute* attr = new JAttribute(name, value);
  
  if ( NULL == attr )
  {
    abort();
  }
  
  attr->type(NUMBER);

  if ( top )
  {
    _attrs.push_front(attr);
  }
  else
  {
    _attrs.push_back(attr);
  }

  return attr;
}

/************************************************************
 * Add an elementy into array node(CONTAINER).
 *  i.e. [ "aaa", "bbb", "ccc"]
 * Parms:
 *    e       - element to add
 *    quoted  - need to be quoted with "". default is true. 
 ***********************************************************/
void JNode::addArrayElement(const std::string& e, bool quoted)
{
  assert( CONTAINER == _type );
  if ( quoted )
  {
    std::string s(1, JC(DOUBLE_QUOTE));
    s += e;
    s += JC(DOUBLE_QUOTE);
    _raw_data_array.push_back(s);
  }
  else
  {
    _raw_data_array.push_back(e);
  }
}

/*******************************************************************
 * Get element array.
 * Parms:
 *    arr           - (output) elements array
 *    dropQuotes    - drop quotes if there for each element
 ******************************************************************/
void JNode::arrayElements(std::list<std::string>& arr, bool dropQuotes)
{
  if ( dropQuotes )
  {
    std::list<std::string>::iterator it = _raw_data_array.begin();
    for ( ; it != _raw_data_array.end(); ++it )
    {
      std::string& s = *it;
      if ( JC(DOUBLE_QUOTE) == s[0] && JC(DOUBLE_QUOTE) == s[s.length()-1] )
      {
        arr.push_back(s.substr(1, s.length()-2));
      }
      else
      {
        arr.push_back(s);
      }
    }
  }
  else
  {
    arr = _raw_data_array;
  }
}

/**********************************************************
 * Output with results in JSON format.
 **********************************************************/
void JNode::output(std::string& out)
{
  switch (_type)
  {
  case CONTAINER:
    out += JC(OPEN_SQUARE);
    if ( _nodes.size() > 0 ) 
    {
      std::list<INode*>::iterator it = _nodes.begin();
      (*it)->output(out);
      ++it;
      for ( ; it != _nodes.end(); ++it )
      {
        out += JC(COMMA);
        (*it)->output(out);
      }
    }
    else if ( _raw_data_array.size() > 0 )
    {
      std::list<std::string>::iterator it = _raw_data_array.begin();
      // out += JC(DOUBLE_QUOTE);
      out += *it;
      // out += JC(DOUBLE_QUOTE);
      ++it;
      for ( ; it != _raw_data_array.end(); ++it )
      {
        out += JC(COMMA);
        // out += JC(DOUBLE_QUOTE);
        out += *it;
        // out += JC(DOUBLE_QUOTE);
      }
    }
    else
    {}
    out += JC(CLOSE_SQUARE);
    break;
  case NORMAL:
    out += JC(OPEN_CURLY);
    if ( _attrs.size() > 0 ) 
    {
      std::list<IAttribute*>::iterator it = _attrs.begin();
      (*it)->output(out);
      ++it;
      for ( ; it != _attrs.end(); ++it )
      {
        out += JC(COMMA);
        (*it)->output(out);
      }
    }
    out += JC(CLOSE_CURLY);
    break;
  case CMD:
  case SH:
  case QSH:
  case PGM:
  case SQL:
  case JOBINFO:
    out += JC(OPEN_CURLY);
    if ( _attrs.size() > 0 ) 
    {
      std::list<IAttribute*>::iterator it = _attrs.begin();
      (*it)->output(out);
      ++it;
      for ( ; it != _attrs.end(); ++it )
      {
        out += JC(COMMA);
        (*it)->output(out);
      }
    }
   
    out += JC(CLOSE_CURLY);
    break;
  default:
    assert(true == false);
    break;
  }
}

/************************************************************
 * Parms:
 *    start - start of buffer to parse
 *    end   - end of buffer to stop
 *    fr   - (output) error indicator
 * Return:
 *    ptr  - where the working ptr stop at   
 ************************************************************/
const char* JNode::parse(const char* start, const char* end, FlightRec& fr)
{
  const char* pWhere = start; 
  bool done1 = false;
  bool done2 = false;

  struct timeval sTime, eTime;
  if ( Control::performance() )
  {
    gettimeofday(&sTime, NULL);
  }


  //--------- process for node -------------------------------------------------------------
  while ( pWhere <= end && !done1 )
  {
      if ( *pWhere == JC(SPACE) || *pWhere == JC(LF) ) // ignore whitespace
      {
        ++pWhere;
      }
      else if ( *pWhere == JC(OPEN_SQUARE)) // node container
      {
        type(CONTAINER);
        done2 = false;
        
        // peek next char to see data array or node array
        const char* pWork = pWhere + 1;
        for ( ; pWork <= end && ( JC(SPACE) == *pWork || JC(LF) == *pWork ); ++pWork ){}

        if ( *pWork == JC(DOUBLE_QUOTE) ) // parse data array [ "a", "b"]
        { 
          pWhere = pWork;
          const char* openQuote = pWhere;
          ++pWhere;
          // locate the close quote of current element
          while ( pWhere <= end && !done2 )
          {
            if ( JC(DOUBLE_QUOTE) == *pWhere && JC(BACK_SLASH) != *(pWhere-1) ) // found close "
            {
              std::string e(openQuote, pWhere-openQuote+1);
              addArrayElement(e, false);
              ++pWhere;
              for ( ; pWhere <= end &&( JC(SPACE) == *pWhere ||  JC(LF) == *pWhere ); 
                      ++pWhere ){}

              if ( JC(CLOSE_SQUARE) == *pWhere ) //// array is done
              {
                ++pWhere;
                done2 = true;
                done1 = true;
              }
              else if ( JC(COMMA) == *pWhere ) //// has next element
              {
                ++pWhere;
                for ( ; pWhere <= end && (JC(SPACE) == *pWhere || JC(LF) == *pWhere ); 
                        ++pWhere ){}
                // locate the open quote of the next element
                if ( JC(DOUBLE_QUOTE) != *pWhere ) 
                {
                  fr.code(ERR_PARSE_FAIL);
                  fr.addMessage("EXPECTING OPEN DOUBLE QUOTE  FOR STRING ARRAY.");
                  done2 = true;
                  done1 = true;
                }
                else 
                {
                  openQuote = pWhere;
                  ++pWhere;
                  continue;
                }
              }
              else
              {
                fr.code(ERR_PARSE_FAIL);
                fr.addMessage("EXPECTING OPEN ] or , FOR STRING ARRAY.");
                done2 = true;
                done1 = true;
              }
            }
            else
            {
              ++pWhere;
            }
          }
          if ( !done2 ) // hit the end but could not finish parsing array
          {
            done1 = true;
            fr.code(ERR_PARSE_FAIL);
            fr.addMessage("INCOMPLETE FORMAT FOR STRING ARRAY.");
          }
        }
        else if ( *pWork >= '0' && *pWork <='9' )  // parse data array [ 11, 22 ]
        {
          pWhere = pWork;
          const char* head = pWhere;
          ++pWhere;
          while ( pWhere <= end && !done2 )
          {
            if ( JC(COMMA) == *pWhere ) // has next element
            {
              std::string e(head, pWhere-head);
              addArrayElement(e, false);
              // continue to process next element
              head = pWhere + 1; 
              pWhere = head + 1;
              continue;
            }
            else if ( JC(CLOSE_SQUARE) == *pWhere ) // done with array
            {
              std::string e(head, pWhere-head);
              addArrayElement(e, false);

              ++pWhere;
              done1 = true;
              done2 = true;
            }
            else
            {
              ++pWhere;
            }
          }

          if ( !done2 ) // hit the end but could not finish parsing array
          {
            done1 = true;
            fr.code(ERR_PARSE_FAIL);
            fr.addMessage("INCOMPLETE FORMAT FOR NUMBER ARRAY."); 
          }
        }
        else if ( JC(CLOSE_SQUARE) == *pWork ) // empty []
        {
          ++pWork;
          pWhere = pWork;
          done1 = true;
        }
        else // process node array or empty
        {
          while ( pWhere <= end && !done2 )
          {
            INode* node = new JNode(); // create node with type indicator
            if ( NULL == node )
            {
              abort();
            }
            else
            {
              addNode(node);
            }

            pWhere = node->parse(++pWhere, end, fr);

            if ( ERR_OK != fr.code() )  
            {
              fr.addMessage("FAILED TO PARSE NODE."); 
              done2 = true;
              done1 = true;    
            }
            else
            {
              for ( ; pWhere <= end && (JC(SPACE) == *pWhere || JC(LF) == *pWhere); 
                    ++pWhere ){}
              if ( JC(COMMA) == *pWhere ) // comma found
              {
                ++pWhere;
                for ( ; pWhere <= end && (JC(SPACE) == *pWhere || JC(LF) == *pWhere); 
                      ++pWhere ){}
                if ( JC(OPEN_SQUARE) == *pWhere 
                  || JC(OPEN_CURLY) == *pWhere ) // [ {}, [], {}, ... ]
                {
                  --pWhere;
                }
                else // fail to locate next sibling
                {
                  fr.code(ERR_PARSE_FAIL);
                  fr.addMessage("EXPECTING { OR [ FOR NEXT SIBLING NODE.");
                  done2 = true;
                  done1 = true;
                }
              }
              else if ( JC(CLOSE_SQUARE) == *pWhere ) // current container is done
              {
                ++pWhere;
                done2 = true;
                done1 = true;
              }
              else // current container has no end JC(CLOSE_SQUARE)
              {
                fr.code(ERR_PARSE_FAIL);
                fr.addMessage("EXPECTING CLOSE ] FOR CONTAINER NODE.");
                done2 = true;
                done1 = true;
              }
            }
          }
        }
      }
      else if ( *pWhere == JC(OPEN_CURLY)) // node
      {
        done2 = false;
        // peek next char to see if its an empty {} or with attrs
        ++pWhere;
        for ( ; pWhere <= end && ( JC(SPACE) == *pWhere || JC(LF) == *pWhere ); ++pWhere ){}
        if ( JC(CLOSE_CURLY) == *pWhere )
        {
          ++pWhere;
          done2 = true;
          done1 = true;
        }
        else if ( JC(DOUBLE_QUOTE) == *pWhere )
        {
          // good to go for attrs
        }
        else
        {
          fr.code(ERR_PARSE_FAIL);
          fr.addMessage("EXPECTING DOUBLE QUOTE OR CLOSE CURLY.");
          done2 = true;
          done1 = true;
        }

        while ( !done2 && pWhere <= end )
        {
          IAttribute* attr = new JAttribute();
          INode::addAttribute(attr);
          if ( NULL == attr )
          {
            abort(); 
          }

          pWhere = attr->parse(pWhere, end, fr);

          if ( ERR_OK != fr.code() ) // failed to parse attribute
          {
            // fr.addMessage("FAILED TO PARSE ATTRIBUTE.");
            done2 = true;
            done1 = true;
          }
          else // get attribute successfully
          {
            for ( ; pWhere <= end && ( JC(SPACE) == *pWhere || JC(LF) == *pWhere ); 
                    ++pWhere ) {}
            if ( JC(COMMA) == *pWhere ) // comma found. i.e. { "a" : 1 , "b" : 2}
            {
              ++pWhere;
              for ( ; pWhere <= end && (JC(SPACE) == *pWhere || JC(LF) == *pWhere); 
                      ++pWhere ){}
              if ( JC(DOUBLE_QUOTE) == *pWhere ) // make sure next is "
              {
                // good to go next attr
              }
              else // fail to locate next sibling attributes
              {
                fr.code(ERR_PARSE_FAIL);
                fr.addMessage("EXPECTING OPEN \" FOR NEXT SIBLING ATTRIBUTE.");
                done2 = true;
                done1 = true;
              }
            }
            else if ( JC(CLOSE_CURLY) == *pWhere ) // current node is done
            {
              ++pWhere;
              done2 = true;
              done1 = true;
            }
            else // current node has no end JC(CLOSE_CURLY)
            {
              fr.code(ERR_PARSE_FAIL);
              fr.addMessage("EXPECTING CLOSE } FOR NODE.");
              done2 = true;
              done1 = true;
            }
          }
        }
      }
      else
      {
        // failed to find the starting { or [
        fr.code(ERR_PARSE_FAIL);
        fr.addMessage("EXPECTING OPEN { OR [ FOR NODE.");
        done1 = true;
      }
  }

  // Initialize runnable object for the node if necessary
  if ( ERR_OK == fr.code() )
  {
    initRunnable(fr);
  }

  if ( Control::performance() )
  {
    gettimeofday(&eTime, NULL);
    JAttribute* perf = new JAttribute(PERF_INIT, 
            static_cast<int>((eTime.tv_sec - sTime.tv_sec)*1000 + (eTime.tv_usec - sTime.tv_usec)/1000) );
    if ( NULL == perf ) 
    {
      abort();
    }
    INode::addAttribute(perf);
  }

  return pWhere;
}

/******************************************************
 * Run the node.
 * 
 * Parm:
 *    fr    - error information
 *****************************************************/
void JNode::run(FlightRec& fr)
{
  struct timeval sTime, eTime;
  if ( Control::performance() )
  {
    gettimeofday(&sTime, NULL);
  }

  //********* Invoke current node. *********************************
  // A runnable node cannot have attributes with runnable node value
  
  if ( NULL != _run_p ) // Current node is runnable
  {
    //---- Add attribute STATUS into current node before run -------
    assert( NULL == _sts_node );

    if ( ! _no_status )
    {
      _sts_node = new JNode(NORMAL);
      if ( NULL == _sts_node )
      {
        abort();
      }
      IAttribute* returnAttr = new JAttribute(STS_STATUS, _sts_node);
      if ( NULL == returnAttr )
      {
        abort();
      }
      else
      {
        INode::addAttribute(returnAttr);
      }
    }

    //------ Run current node -----------------------------------
    _run_p->run(fr);

    //-----------------------------------------------------------
    // Insert attribute STATUS into current runnable node.
    // "STATUS": {"CODE":0, 
    //            "MESSAGE": {"CPF0001" : "FlightRec found on *N command.",
    //                        "CPD0001" : "an alphabetic ...", 
    //                        ...},
    //            "DATA":{"RTNUSRPRF":"GUEST","GRPPRF":"*NONE""}}
    //-----------------------------------------------------------
    // Assemble return node value from fr info    
    if ( ! _no_status )
    {
      if ( fr.hasMessages() ) 
      {
        INode* nodeVal = new JNode(NORMAL);
        if ( NULL == nodeVal )
        {
          abort();
        }
        nodeVal->addAttributes(fr.messages());

        IAttribute* errMsgAttr = new JAttribute(STS_MESSAGE, nodeVal);
        if ( NULL == errMsgAttr )
        {
          abort();
        }

        _sts_node->addAttribute(errMsgAttr, true);
      }

      IAttribute* codeAttr = new JAttribute(STS_CODE, fr.code());
      if ( NULL == codeAttr )
      {
        abort();
      }
      _sts_node->addAttribute(codeAttr, true);
    }
  }
  else  // Pass intruction run to attributes with node value
  {
    std::list<IAttribute*>::iterator it;
    for ( it = _attrs.begin(); it != _attrs.end() && ERR_OK == fr.code(); ++it )
    {
      (*it)->runNode(fr);;
    }
  }

  //********** Invoke the childern ***********************
  std::list<INode*>::iterator it;
  for ( it = _nodes.begin(); it != _nodes.end() && ERR_OK == fr.code(); ++it )
  {
    (*it)->run(fr);
  }


  if ( Control::performance() )
  {
    gettimeofday(&eTime, NULL);
    JAttribute* perf = new JAttribute(PERF_RUN, 
          static_cast<int>((eTime.tv_sec - sTime.tv_sec)*1000 + (eTime.tv_usec - sTime.tv_usec)/1000) );
    if ( NULL == perf ) 
    {
      abort();
    }
    INode::addAttribute(perf);
  }

}