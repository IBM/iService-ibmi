#ifndef RUNNABLE_H
#define RUNNABLE_H
#include "FlightRec.h"

typedef enum Switch_Value_t
{
  OFF,         // Escape message only. Default.
  ON,          // Messages for the call.
  IGNORE       // No stop on error.
}Switch_Value;

class INode;
class Runnable
{
public:
  Runnable(INode* node);
  virtual ~Runnable();

  virtual void init(FlightRec& fr) = 0;
  virtual void run(FlightRec& fr) = 0;

protected:
  INode*              _node_p;
  Switch_Value        _error;

private:
  Runnable(const Runnable&);
  Runnable& operator=(const Runnable&);
};

#endif