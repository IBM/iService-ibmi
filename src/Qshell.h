#ifndef QSHELL_H
#define QSHELL_H
#include "Runnable.h"
#include <string>

/******************************************************************
 * "qsh" : 
 * {
 *       "error"  : "on|off",
 *       "value"  : "..."
 * }
 *****************************************************************/

class Qshell : public Runnable
{
public:
  Qshell(INode* node);
  ~Qshell();

  void init(FlightRec& fr);
  void run(FlightRec& fr);

private:
  Qshell(const Qshell&);
  Qshell& operator=(const Qshell&);

  std::string         _value;
};

#endif