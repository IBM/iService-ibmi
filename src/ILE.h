#ifndef ILE_H
#define ILE_H
#include <miptrnam.h> 
#include <except.h>
#include <qleawi.h> 
#include "FlightRec.h"

typedef struct Block_Byte_16_t 
{
  char me[16];
}Block_Byte_16;

class ILE
{
public:
  static void   ileRslv(const char* pgm, const char* lib, const char* func, 
                        _SYSPTR* pPgm, void** pFunc, FlightRec& fr);
  static char*  alignPadding(char* curPtr, int tsize, int size2add, char pad = 0x40);
  static void   runPgm(const char* pgm, const char* lib, char** ppArgv, int argc, int* rtnValue, FlightRec& fr);
  static void   runSrvgm(const char* pgm, const char* lib, const char* func,
              char** ppArgv, int argc, void* result, FlightRec& fr);
  static int    maxNumOfArgs();
private:
  ILE();
  ILE(const ILE&);
  ILE& operator=(const ILE&);

};

#endif