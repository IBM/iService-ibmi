#ifndef JNODE_H
#define JNODE_H
#include <string>
#include "INode.h"

class JNode : public INode
{
public:
  JNode();
  JNode(NodeType type);
  ~JNode();

  INode* createNode();  
  IAttribute* addAttribute(const std::string& name, INode* node, bool top = false);
  IAttribute* addAttribute(const std::string& name, const std::string& value, bool top = false);
  IAttribute* addAttribute(const std::string& name, int value, bool top = false);  
  void addAttributes(const std::list<class Pair*>& pairs);
  void addAttributes(const std::list<IAttribute*>& attrs);
  void addArrayElement(const std::string& e, bool quoted);
  void arrayElements(std::list<std::string>& arr, bool dropQuotes = true);
  
  const char* parse(const char* start, const char* end, FlightRec& fr);
  void output(std::string& out);
  void run(FlightRec& fr);
  
private:
  JNode(const JNode&);
  JNode& operator=(const JNode&);

};
  
#endif