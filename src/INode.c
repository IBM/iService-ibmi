#include <stdlib.h>
#include <assert.h>
#include "INode.h"
#include "Utility.h"
#include "IAttribute.h"
#include "Command.h"
#include "Shell.h"
#include "Qshell.h"
#include "SqlUtil.h"
#include "Program.h"
#include "JobInfo.h"

/******************************************************
 * Abstract class definition for node.
 *****************************************************/

/*****************************************************
 * (Static) method to check node type by string name.
 *****************************************************/
NodeType INode::getNodeType(std::string type)
{
  NodeType t;
  trim(type);
  cvt2Upper(type);
  if ( type == "CMD" )
    t = CMD;
  else if (  type == "SH" )
    t = SH;
  else if (  type == "QSH" )
    t = QSH;   
  else if (  type == "PGM" )
    t = PGM;   
  else if (  type == "SQL" )
    t = SQL;
  else if (  type == "JOBINFO") 
    t = JOBINFO;
  else
    t = NORMAL; 

  return t; 
}

/*********************************************************************
 * Constructor
 ********************************************************************/
INode::INode() 
  :  _attrs(), _nodes(), _type(NORMAL), _run_p(NULL), 
  _sts_node(NULL), _raw_data_array(), _no_status(false)
{}

/*********************************************************************
 * Constructor
 * Parm:
 *    type  - type of node
 ********************************************************************/
INode::INode(NodeType type) 
  :  _attrs(), _nodes(), _type(type), _run_p(NULL), _sts_node(NULL), _raw_data_array(),
     _no_status(false)
{}

/*********************************************************************
 * Destructor
 ********************************************************************/
INode::~INode()
{ 
  std::list<IAttribute*>::iterator it;
  for ( it = _attrs.begin(); it != _attrs.end(); ++it )
  {
    delete *it;
  }

  std::list<INode*>::iterator lit;
  for ( lit = _nodes.begin(); lit != _nodes.end(); ++lit )
  {
    delete *lit;
  }

  if ( NULL != _run_p )
  {
    delete _run_p;
  }

}

/*********************************************************************
 * Get type of the node
 ********************************************************************/
NodeType INode::type()
{
  return _type;
}

/*********************************************************************
 * Set type of the node
 ********************************************************************/
void INode::type(NodeType type)
{
  _type = type;
}

/*********************************************************************
 * Add a sub-node
 * Parms:
 *  node  - sub-node to be added
 *  top   - add to top. default is false
 ********************************************************************/
void INode::addNode(INode* node, bool top)
{
  if ( top )
  {
    _nodes.push_front(node);
  }
  else
  {
    _nodes.push_back(node);
  }
}

/*********************************************************************
 * Remove the last sub-node
 ********************************************************************/
void INode::popLastNode()
{
  _nodes.pop_back();
}

/*********************************************************************
 * Add an attribute into the node
 * 
 * Parm:
 *    attr    - ptr of attribute
 *    top     - add to the top. Default it false.
 ********************************************************************/
void INode::addAttribute(IAttribute* attr, bool top)
{
  if ( top )
  {
    _attrs.push_front(attr);
  }
  else
  {
    _attrs.push_back(attr);
  }
}

/*********************************************************************
 * Remove the last attribute
 ********************************************************************/
void INode::popLastAttribute()
{
  _attrs.pop_back();
}

/*********************************************************************
 * Get the attributes number
 ********************************************************************/
int INode::attrsNum()
{
  return _attrs.size();
}

/*********************************************************************
 * Get the number of sub-nodes
 ********************************************************************/
int INode::nodesNum()
{
  return _nodes.size();
}

/*********************************************************************
 * Get the container of sub-nodes
 ********************************************************************/
std::list<INode*>& INode::childNodes() 
{ 
  return _nodes; 
}

/*********************************************************************
 * Get the STATUS node
 ********************************************************************/
INode* INode::getStatusNode()
{
  return _sts_node;
}

/*********************************************************************
 * Get the container of attributes
 ********************************************************************/
const std::list<IAttribute*>& INode::attributes()
{
  return _attrs;
}

/***************************************************
 * Get attribute pointer by name.
 ***************************************************/
IAttribute* INode::getAttribute(std::string name)
{
  IAttribute* p = NULL;
  std::list<IAttribute*>::iterator it;
  int i = 0;
  std::string name1, name2 = name;
  cvt2Upper(name2);
  for ( it = _attrs.begin(); it != _attrs.end(); ++it, ++i )
  {
    name1 = (*it)->name();
    cvt2Upper(name1);
    if ( name1 == name2 )
    {
      p = *it;
      break;
    }
  }
  return p;
}

/***************************************************
 * Get the total number of raw array elements.
 ***************************************************/
int INode::arraySize()
{
  return _raw_data_array.size();
}

/***************************************************
 * Turn on/off for injecting STATUS 
 ***************************************************/
void INode::noStatus(bool flag)
{
  _no_status = flag;
}

/***************************************************
 * Based on internal node type, intialize runnable
 * object.
 ***************************************************/
void INode::initRunnable(FlightRec& fr)
{ 
  switch (_type)
  {
  case CMD:
    _run_p = new Command(this);
    break;
  case SH:
    _run_p = new Shell(this);
    break;
  case QSH:
    _run_p = new Qshell(this);
    break;
  case PGM:
    _run_p = new Program(this);
    break;
  case SQL:
    _run_p = new SqlUtil(this);
    break;
  case JOBINFO:
    _run_p = new JobInfo(this);
    break;
  default:
    // do nothing
    break;
  }

  if ( NULL != _run_p ) 
  {
    _run_p->init(fr);
  }
}

