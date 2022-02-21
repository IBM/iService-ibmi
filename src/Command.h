#ifndef COMMAND_H
#define COMMAND_H
#include "Runnable.h"
#include "FlightRec.h"


/******************************************************************
 * "cmd" : 
 * {
 *       "exec"   : "cmd|system|rexx",
 *       "error"  : "on|off",
 *       "value"  : "..."
 * }
 *****************************************************************/
typedef enum Exec_Type_t
{
  COMMAND,        // ILE comamnd. Default.
  SYSTEM,         // system()
  REXX            // STRREXX script
}Exec_Type;


class INode;

class Command : public Runnable
{
public:
  Command(INode* node);
  ~Command();

  void init(FlightRec& fr);
  void run(FlightRec& fr);

private:
  Command(const Command&);
  Command& operator=(const Command&);
  void runCommand(FlightRec& fr);
  void runSystem(FlightRec& fr);
  void runRexx(FlightRec& fr);

  /***** CMD attributes mapping ***************/
  Exec_Type       _exec;        // "exec"
  std::string     _value;       // "value"            
};

#endif