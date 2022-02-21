#ifndef PROGRAM_H
#define PROGRAM_H
#include "Runnable.h"
#include <string>
#include "Argument.h"

/*****************************************************************************************************************/
  //   "pgm": {
  //      "error": "on|off",
  //      "name": "pgm_name",
  //      "lib":  "pgm_library",
  //(opt) "func": "svrpgm_exported_symbol",
  //(opt) "parm": [{  "io": "in|out|both", "var": "name_1", "type": "char(10)", "value": "Hi"  },
  //               {  "io": "in|out|both", "var": "name_2", "type": "char(10)", "hex": "on", "value": "1A2B3C4D"  }, 
  //               {  "io": "in|out|both", "var": "name_3", "type": "ds", 
  //                     "value": [  { "var": "ds_var1", "type": "packed(10,2)", "value": 5.55 }, 
  //                                 { "var": "ds_var2", "type": "zoned(10,2)", "value": 66.6 } ] } 
  //              ]
  //(opt) "return": { "type": "char|int()|uint()|float()|double()|packed()|zoned()", "value": ...}
  //           }
  // NOTE:
  //   1) DS can not contain DS.
  //   2) DS is always passed by reference(pointer)
  //   2) "by" only for svrpgm prodedure call. pgm call are always by reference.
/*****************************************************************************************************************/

class Program : public Runnable
{
public:
  Program(INode* node);
  ~Program();

  void init(FlightRec& fr);
  void run(FlightRec& fr);

private:
  Program(const Program&);
  Program& operator=(const Program&);

  std::string         _name;
  std::string         _lib;
  std::string         _value;
  std::string         _func;
  Arguments           _args;
  Return              _return;
};

#endif