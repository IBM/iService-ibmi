#ifndef ARGUMENT_H
#define ARGUMENT_H
#include <string>
#include <list>
#include "FlightRec.h"
#include "Pase.h"
#include "Common.h"

/********************************************************************
 * Common Data types:
 *    -   char(n)
 *    -   int(n)            where n = 2|4|8
 *    -   unit(n)           where n = 2|4|8
 *    -   float(n)          where n = precision
 *    -   double(n)         where n = precision
 *    -   packed(n, m)      where n = digits, m = precision
 *    -   zoned(n, m)       where n = digits, m = precision
 *    -   ds                Embbedded DS allowed
 * RPG Data Types:
 *    -   char(length) *
 *    -   varchar(length, 2|4)
 *    -   int(digits) *
 *    -   uns(digits) *
 *    -   float(bytes) *
 *    -   packed(digits, decimal-positions)
 *    -   zoned(digits, decimal-positions)
 *    -   bindec(digits, decimal-positions)
 *******************************************************************/

typedef enum Arg_Type_t
{
  DS,
  CHAR,
  INT,
  UINT,
  FLOAT,
  DOUBLE,
  PACKED,
  ZONED,
  INVALID
}Arg_Type; 

class INode;
class IAttribute;

class Argument
{
public:
  Argument(bool isPase);
  ~Argument();

  bool          init(INode* node, FlightRec& fr);
  void          pase(bool flag);
  bool          type(std::string type, FlightRec& fr);
  Arg_Type      type();
  void          type(Arg_Type type);
  bool          byValue();
  void          byValue(bool byVal);
  void          hex(bool on);
  int           size();
  void          size(int size);
  void          output(IO_Type io);
  IO_Type       output();
  void          attribute(IAttribute* attr);
  void          addDsEntry(Argument* subArg);
  std::list<Argument*>&  dsEntries();
  bool          copyIn(FlightRec& fr, bool srvpgm);
  void          copyBack(FlightRec& fr);
  QP2_ptr64_t&  valuePasePtr();
  char*&        valueIlePtr();
  ILECALL_Arg_t signature();

private:
  Argument(const Argument&);
  Argument& operator=(const Argument&);

  bool                      _by_value;      // mapping to attr "by". Only for srvpgm for now.
  bool                      _is_pase;
  IO_Type                   _io;
  Arg_Type                  _type;
  bool                      _is_hex;
  int                       _bytes;
  int                       _total_digits;    // Total width
  int                       _frac_digits;    // Fractional part digits for output
  IAttribute*               _attr_p;        // "VALUE" attribute

  char*                     _value_p;       // ile-used pase memory aligned
  char*                     _value_org_p;   // ile-used pase memory start
  QP2_ptr64_t               _value_pase_p;  // pase-used memory aligned

  ILECALL_Arg_t             _sig;           // ile-use ptr to pase aligned OR ile memory

  std::list<Argument*>      _ds_entries;    // for "DS"
};

class Arguments
{
public:
  Arguments();
  ~Arguments();

  void            init(INode* parm, FlightRec& fr);
  int             number();
  void            pase(bool isPase);
  bool            isPase();
  bool            allByReference();
  void            create(FlightRec& fr, bool srvpgm = false);
  char**          ileArgv();
  QP2_ptr64_t*    paseArgv();
  QP2_ptr64_t*    signatures();
  void            copyBack(FlightRec& fr);
  char*           ilePtr();

private:
  Arguments(const Arguments&);
  Arguments& operator=(const Arguments&);

  bool                      _init_done;
  bool                      _all_by_ref;
  bool                      _is_pase;
  std::list<Argument*>      _args;

  char*                     _argv_p;          // ile-use ptr to pase aligned OR ile memory
  char*                     _argv_org_p;      // ile-use ptr to pase original allocated 
  QP2_ptr64_t               _argv_pase_p;     // pase ptr aligned

  char*                     _sigs_p;          // ile-use ptr to pase aligned OR ile memory
  char*                     _sigs_org_p;      // ile-use ptr to pase origianl allocated
  QP2_ptr64_t               _sigs_pase_p;     // pase ptr aligned

  INode*                    _node_p;
};

class Return
{
public:
  Return();
  ~Return();

  bool          init(INode* node, FlightRec& fr);
  Result_Type_t type();
  void          attribute(IAttribute* attr);
  void          copyBack(char* result, FlightRec& fr);
  void          copyBack(int rtnValue);
  bool          isVoid();

private:
  Return(const Return&);
  Return& operator=(const Return&);
       
  Result_Type_t             _type;
  int                       _bytes;
  int                       _total_digits;    // Total width
  int                       _frac_digits;     // Fractional part digits for output
  IAttribute*               _attr_p;          // "VALUE" attribute
};
#endif