#ifndef INODE_H
#define INODE_H

#include <list>
#include <string>
#include "FlightRec.h"
#include <map>

class Runnable;
class IAttribute;

typedef enum NodeType_t
{
  CONTAINER,
  NORMAL,
  CMD,
  SH,
  QSH,
  PGM,
  SQL,
  JOBINFO
}NodeType;

class INode 
{ 
public:
  INode();
  INode(NodeType type); 
  virtual ~INode();

  NodeType type();
  void type(NodeType type);
  void addNode(INode* node, bool top = false);
  virtual INode* createNode() = 0;
  void popLastNode();
  void addAttribute(IAttribute* attr, bool top = false);
  virtual IAttribute* addAttribute(const std::string& name, INode* node, bool top = false) = 0;
  virtual IAttribute* addAttribute(const std::string& name, const std::string& value, bool top = false) = 0;
  virtual IAttribute* addAttribute(const std::string& name, int value, bool top = false) = 0;
  void popLastAttribute();
  int attrsNum();
  int nodesNum();
  std::list<INode*>& childNodes();
  IAttribute* getAttribute(std::string name);
  INode* getStatusNode();
  virtual void addAttributes(const std::list<class Pair*>& pairs) = 0;
  virtual void addAttributes(const std::list<IAttribute*>& attrs) = 0;
  const std::list<IAttribute*>& attributes();
  virtual const char* parse(const char* start, const char* end, FlightRec& fr) = 0; 
  virtual void  output(std::string& out) = 0;
  virtual void  run(FlightRec& fr) = 0;
  virtual void  addArrayElement(const std::string& e, bool quoted = true) = 0;
  virtual void  arrayElements(std::list<std::string>& arr, bool dropQuotes = true) = 0;
  int arraySize();
  void noStatus(bool flag);

  static NodeType getNodeType(std::string type);

protected:
  void initRunnable(FlightRec& fr);

  NodeType                  _type;
  std::list<IAttribute*>    _attrs;           // attributes
  std::list<INode*>         _nodes;           // child nodes
  Runnable*                 _run_p;           // pointer to callable object
  INode*                    _sts_node;        // "STATUS" - A fast reference and no need to delete.
  std::list<std::string>    _raw_data_array;  // Array elements "a", "b", "c" or  1, 2 ,3. 
  bool                      _no_status;       // indicates if inject "STATUS" 

private:
  INode(const INode&);
  INode& operator=(const INode&);

};
#endif