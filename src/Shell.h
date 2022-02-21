#ifndef SHELL_H
#define SHELL_H
#include "Runnable.h"
#include <string>

/******************************************************************
 * "sh" : 
 * {
 *       "error"  : "on|off",
 *       "value"  : "..."
 * }
 *****************************************************************/

class Shell : public Runnable
{
public:
  Shell(INode* node);
  ~Shell();

  void init(FlightRec& fr);
  void run(FlightRec& fr);

private:
  Shell(const Shell&);
  Shell& operator=(const Shell&);

  std::string         _value;
};

#endif