#ifndef JATTRIBUTE_H
#define JATTRIBUTE_H
#include "IAttribute.h"

class JAttribute : public IAttribute
{
public: 
  JAttribute();
  JAttribute(std::string name, std::string value);
  JAttribute(std::string name, double dotNumber);
  JAttribute(std::string name, long long number);
  JAttribute(std::string name, int number);
  JAttribute(std::string name, INode* node);
  ~JAttribute();

  const char* parse(const char* start, const char* end, FlightRec& fr);
  void output(std::string& out);
  void runNode(FlightRec& fr);

private:
  JAttribute(const JAttribute&);
  JAttribute& operator=(const JAttribute&);

  void escape();
};

#endif