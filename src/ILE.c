#include "FlightRec.h"
#include "ILE.h"
#include "Utility.h"
#include <string>
#include <assert.h>
#include <stdlib.h>
#include "errno.h"
#include "milib.h"
#include "Pase.h"

extern "OS" typedef int (PgmCall_t)(...);
static const int MAX_ARG_NUM = 30;

/*****************************************************************************
 * Resolve system pointer to pgm or svrpgm function.
 * Parms:
 *    pgm   - pgm/srvpgm name. 
 *    lib   - library
 *    func  - exported function name. NULL for pgm.
 *    pPgm  - (output) system ptr to pgm/srvpgm object
 *    pFunc - (output) ptr to exported function. NULL for pgm.
 *    fr    - error information
 ****************************************************************************/
void ILE::ileRslv(const char* pgm, const char* lib, const char* func,
                _SYSPTR* pgmPtr, void** funcPtr, FlightRec& fr)
{
  _OBJ_TYPE_T objType = WLI_PGM;
  unsigned long long actMark;
  *pgmPtr = NULL;
  Qus_EC_t ec;
  ec.Bytes_Provided = sizeof(ec);
  //---------- Retrieve sysptr of program -----------------------------------------
  if ( NULL != func )
  {
    objType = WLI_SRVPGM;
  }

  std::string pgmUC(pgm);
  std::string libUC(lib);
  trim(pgmUC);
  trim(libUC);
  cvt2Upper(pgmUC);
  cvt2Upper(libUC);
  #pragma exception_handler ( rslvsp_exc, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
  *pgmPtr = rslvsp(objType, const_cast<char*>(pgmUC.c_str()), 
                  const_cast<char*>(libUC.c_str()), _AUTH_NONE); 
  #pragma disable_handler
  rslvsp_exc:
  if ( NULL == *pgmPtr )
  {
    fr.code(ERR_PASE_RUN);
    fr.addMessage("FAILED TO RESOLVE PROGRAM.");
    return;
  }
  else 
  {
    if ( WLI_SRVPGM == objType )
    {
      //---------- Retrieve sysptr of function --------------------------------------
      actMark = QleActBndPgmLong(pgmPtr, NULL, NULL, NULL, &ec);
      if ( 0 != actMark )
      {
        int zero = 0;
        *funcPtr = QleGetExpLong( &actMark, 
                                  &zero, 
                                  &zero, 
                                  const_cast<char*>(func), 
                                  funcPtr, 
                                  &zero, 
                                  &ec);
        if ( NULL == *funcPtr )
        {
          fr.code(ERR_PASE_RUN);
          fr.addMessage("FAILED TO RESOLVE FUNCTION.");
        }
      }
      else
      {
        fr.code(ERR_PASE_RUN);
        fr.addMessage("FAILED TO ACTIVATE PROGRAM.");
      }
    }
  }
}

/****************************************************************************************
 * Align pointer with padding if needed.
 * NOTE: DO NOT use this function for char[], struct, union.
 * ---------     ------------
 *  Length 	      Alignment
 *  --------      -----------
 *  1 byte 	        any
 *  2 bytes 	      2 bytes
 *  3-4 bytes 	    4 bytes
 *  5-8 bytes 	    8 bytes
 *  9+ bytes 	      16 bytes
 * Parms:
 *    ptr       - current position
 *    tsize     - total size currently
 *    size2add  - size being added
 *    pad       - padding char to align
 * Return:
 *    ptr aligned for use
 ***************************************************************************************/
char* ILE::alignPadding(char* ptr, int tsize, int size2add, char pad)
{
  int toPad = 0;
  if ( size2add <= 1 ) // any 
  {
    return ptr;
  }
  else if ( 2 == size2add ) // 2 bytes
  {
    toPad = (tsize % 2 == 0) ? (0) : (2 - tsize % 2);
  }
  else if ( size2add >= 3 && size2add <= 4 ) // 4 bytes
  {
    toPad = (tsize % 4 == 0) ? (0) : (4 - tsize % 4);
  }
  else if ( size2add >= 5 && size2add <= 8 ) // 8 bytes
  {
    toPad = (tsize % 8 == 0) ? (0) : (8 - tsize % 8);
  }
  else if ( size2add >= 9 ) // 16 bytes
  {
    toPad = (tsize % 16 == 0) ? (0) : (16 - tsize % 16);
  }
  else
  {
    assert(false);
  }

  for ( int i = 0; i < toPad; ++i )
  {
    memset(ptr, pad, 1);
    ++ptr;
  }
  return ptr;
}

/************************************************************************************
 * Run program.
 * Parms:
 *    pgm     - program name
 *    lib     - program library
 *    argv    - ptr array of arguments
 *    argc    - number of arguments
 *    rtnValue- return value from pgm
 *    fr      - error information
 ***********************************************************************************/
void ILE::runPgm(const char* pgm, const char* lib, char** ppArgv, int argc, int* rtnValue, FlightRec& fr)
{
  PgmCall_t* PgmCall;
  _SYSPTR pgmPtr = NULL;
  int rc = 0; 
  char* argv[MAX_ARG_NUM] = { NULL };
  
  // resolve pgm ptr
  // PgmCall = (PgmCall_t*) rslvsp(WLI_PGM, const_cast<char*>(pgm), const_cast<char*>(lib), _AUTH_NONE);
  ileRslv(pgm, lib, NULL, &pgmPtr, NULL, fr);
  PgmCall = (PgmCall_t*) pgmPtr;
  if ( ERR_OK != fr.code() )
  {
    return;
  }
  else
  {}

  // init pgm argv
  char* arg = *ppArgv;
  for ( int i = 0; i < MAX_ARG_NUM && i < argc; ++i )
  {
    memcpy(&argv[i], arg, sizeof(char*));
    arg += sizeof(char*);
  }

  // call pgm
  #pragma exception_handler ( pgmcall_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
  switch (argc)
  {
  case 0:
    rc = (PgmCall)();
    break;
  case 1:
    rc = (PgmCall)(argv[0]);
    break; 
  case 2:
    rc = (PgmCall)(argv[0], argv[1]);
    break;
  case 3:
    rc = (PgmCall)(argv[0], argv[1], argv[2]);
    break;
  case 4:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3]);
    break;
  case 5:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4]);
    break; 
  case 6:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
    break;
  case 7:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6]);
    break;
  case 8:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7]);
    break;
  case 9:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8]);
    break;
  case 10:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9]);
    break;
  case 11:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10]);
    break;
  case 12:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11]);
    break;
  case 13:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12]);
    break;
  case 14:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13]);
    break;
  case 15:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14]);
    break;
  case 16:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15]);
    break;
  case 17:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16]);
    break;
  case 18:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17]);
    break;
  case 19:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18]);
    break;
  case 20:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19]);
    break;
  case 21:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20]);
    break;
  case 22:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21]);
    break;
  case 23:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22]);
    break;
  case 24:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23]);
    break;
  case 25:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24]);
    break;
  case 26:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25]);
    break;
  case 27:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26]);
    break;
  case 28:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26],
                    argv[27]);
    break;
  case 29:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26],
                    argv[27], argv[28]);
    break;
  case 30:
    rc = (PgmCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26],
                    argv[27], argv[28], argv[29]);
    break;
  default:
    fr.code(ERR_PGM_RUN);
    fr.addMessage(MAX_ARG_NUM, "EXCEEDED MAX ARUGMENTS OF PROGRAM CALL");
    break;
  }
  #pragma disable_handler

  *rtnValue = rc;
  return;

  pgmcall_ex:
  fr.code(ERR_PGM_RUN);
  fr.addMessage(errno, "ERROR NUMBER FROM PROGRAM CALL");
}

/*************************************************************************
 * Run service program.
 * Parms:
 *    pgm       - program name
 *    lib       - program library
 *    fun       - exported function name
 *    ppArgv    - '32-byte' aligned array
 *    argc      - number of arguments
 *    result    - ptr to buffer for return value
 *    fr        - error information
 *************************************************************************/
void ILE::runSrvgm(const char* pgm, const char* lib, const char* func,
            char** ppArgv, int argc, void* result, FlightRec& fr)
{
  typedef void (*Function_t)(...);
  Function_t FuncCall;
  _SYSPTR pgmPtr = NULL;
  char* argv[MAX_ARG_NUM] = { NULL };

  // resolve pgm ptr
  void* FuncPtr = NULL;
  ILE::ileRslv(pgm, lib, func, (_SYSPTR*)&FuncCall, &FuncPtr, fr);
  if ( ERR_OK != fr.code() )
  {
    return;
  }
  else
  {
    FuncCall = (Function_t) FuncPtr;
  }

  // init srvpgm argv
  char* arg = *ppArgv;
  for ( int i = 0; i < MAX_ARG_NUM && i < argc; ++i )
  {
    memcpy(&argv[i], arg, sizeof(char*));
    arg += sizeof(char*);
  }

  // call srvpgm
  #pragma exception_handler ( pgmcall_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
  switch (argc)
  {
  case 0:
    /*rc =*/ (*FuncCall)();
    break;
  case 1:
    /*rc =*/ (*FuncCall)(argv[0]);
    break; 
  case 2:
    /*rc =*/ (*FuncCall)(argv[0], argv[1]);
    break;
  case 3:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2]);
    break;
  case 4:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3]);
    break;
  case 5:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4]);
    break; 
  case 6:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
    break;
  case 7:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6]);
    break;
  case 8:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7]);
    break;
  case 9:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8]);
    break;
  case 10:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9]);
    break;
  case 11:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10]);
    break;
  case 12:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11]);
    break;
  case 13:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12]);
    break;
  case 14:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13]);
    break;
  case 15:
    /*rc =*/ (*FuncCall)(*argv[0], *argv[1], *argv[2], *argv[3], *argv[4], *argv[5],
                    *argv[6], *argv[7], *argv[8], *argv[9], *argv[10], *argv[11],
                    *argv[12], *argv[13], *argv[14]);
    break;
  case 16:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15]);
    break;
  case 17:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16]);
    break;
  case 18:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17]);
    break;
  case 19:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18]);
    break;
  case 20:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19]);
    break;
  case 21:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20]);
    break;
  case 22:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21]);
    break;
  case 23:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22]);
    break;
  case 24:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23]);
    break;
  case 25:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24]);
    break;
  case 26:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25]);
    break;
  case 27:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26]);
    break;
  case 28:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26],
                    argv[27]);
    break;
  case 29:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26],
                    argv[27], argv[28]);
    break;
  case 30:
    /*rc =*/ (*FuncCall)(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                    argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
                    argv[12], argv[13], argv[14], argv[15], argv[16],
                    argv[17], argv[18], argv[19], argv[20], argv[21],
                    argv[22], argv[23], argv[24], argv[25], argv[26],
                    argv[27], argv[28], argv[29]);
    break;
  default:
    fr.code(ERR_PGM_RUN);
    fr.addMessage(MAX_ARG_NUM, "EXCEEDED MAX ARUGMENTS OF SERVICE PROGRAM CALL");
    break;
  }
  #pragma disable_handler

  // memcpy(result, &rc.me[0], sizeof(Block_Byte_16)); 
  return;

  pgmcall_ex:
  fr.code(ERR_PGM_RUN);
  fr.addMessage(errno, "ERROR NUMBER FROM PROGRAM CALL");  
}

/****************************************************************************
 * Get the maximum number of arguments supported
 ****************************************************************************/
int ILE::maxNumOfArgs()
{
  return MAX_ARG_NUM;
}