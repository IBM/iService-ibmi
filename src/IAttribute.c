#include <stdlib.h>
#include <stdio.h>
#include "IAttribute.h"
#include "INode.h"
#include "Utility.h"
#include <assert.h>

/***************************************************************
 * Contructor
 **************************************************************/
IAttribute::IAttribute()
  : _name(), _value_string(), _type(STRING), _node(NULL), _number(0), _dot_number(0.0)
{}

/***************************************************************
 * Contructor for LONG LONG type
 **************************************************************/
IAttribute::IAttribute(std::string name, long long number)
  : _name(name), _value_string(), _type(NUMBER), _node(NULL), _number(0), _dot_number(0.0)
{
  char buff[32];
  sprintf(buff, "%lld", number);
  _value_string = buff;
}

/***************************************************************
 * Contructor for INT type
 **************************************************************/
IAttribute::IAttribute(std::string name, int number)
  : _name(name), _value_string(), _type(NUMBER), _node(NULL), _number(0), _dot_number(0.0)
{
  char buff[32];
  sprintf(buff, "%d", number);
  _value_string = buff;
}

/***************************************************************
 * Contructor for DOUBLE type
 **************************************************************/
IAttribute::IAttribute(std::string name, double dotNumber)
  : _name(name), _value_string(), _type(NUMBER), _node(NULL), _number(0), _dot_number(0.0)
{
  char buff[64];
  sprintf(buff, "%f", dotNumber);
  _value_string = buff;
}

/***************************************************************
 * Contructor for STRING type
 **************************************************************/
IAttribute::IAttribute(std::string name, std::string value)
  :  _name(name), _value_string(value), _type(STRING), _node(NULL), _number(0), _dot_number(0.0)
{}

/***************************************************************
 * Contructor for NODE VALUE type
 **************************************************************/
IAttribute::IAttribute(std::string name, INode* node)
  : _name(name), _value_string(), _type(NODE), _node(node)
{}

/***************************************************************
 * Destructor
 **************************************************************/
IAttribute::~IAttribute()
{
  if ( NULL != _node )
  {
    delete _node;
  }
}

/***************************************************************
 * Get the type of attr
 **************************************************************/
ValueType IAttribute::type()
{
  return _type;
}

/***************************************************************
 * Set the type of attr
 **************************************************************/
void IAttribute::type(ValueType type)
{
  _type = type;
}

/***************************************************************
 * Get the name of attr
 **************************************************************/
const std::string IAttribute::name()
{
  return _name;
}

/***************************************************************
 * Get the value in chars of attr
 **************************************************************/
const std::string IAttribute::valueString()
{
  return _value_string;
}

/***************************************************************
 * Set the value for the attr in chars
 **************************************************************/
void IAttribute::setValue(std::string& s)
{
  _value_string = s;
}

/***************************************************************
 * Set the noce value for the attr 
 **************************************************************/
void IAttribute::setValue(INode * node)
{
  assert( NODE == _type );
  delete _node;
  _node = node;
}

/***************************************************************
 * Get the noce value for the attr 
 **************************************************************/
INode* IAttribute::nodeValue() 
{ 
  return _node; 
}