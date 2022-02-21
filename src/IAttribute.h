#ifndef IATTRIBUTE_H
#define IATTRIBUTE_H

#include "FlightRec.h"

/************************************************************
 * Abstract attrbute definition sitting in a node.
 ***********************************************************/

class INode;

typedef enum ValueType_t
{
  UNKNOWN,
  NODE,
  NUMBER,
  STRING
}ValueType;

class IAttribute
{
public: 
  IAttribute();
  IAttribute(std::string name, std::string value);
  IAttribute(std::string name, long long number);
  IAttribute(std::string name, double dotNumber);
  IAttribute(std::string name, int number);
  IAttribute(std::string name, INode* node);
  virtual ~IAttribute();

  virtual const char* parse(const char* start, const char* end, FlightRec& fr) = 0;
  virtual void output(std::string& out) = 0;
  virtual void runNode(FlightRec& fr) = 0;

  ValueType type();
  void type(ValueType type);
  const std::string name();
  const std::string valueString();
  void setValue(std::string& s);
  void setValue(INode * node);
  INode* nodeValue();

protected:
  virtual void escape() = 0;
  std::string    _name;
  long long      _number;
  double         _dot_number;
  ValueType      _type;
  std::string    _value_string;   // string format
  INode*        _node;   
  
private:
  IAttribute(const IAttribute&);
  IAttribute& operator=(const IAttribute&);
   
};
#endif