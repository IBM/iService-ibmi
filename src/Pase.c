#include "Pase.h"
#include <errno.h>
#include <string.h>
#include <qusec.h>
#include <assert.h>
#include "Utility.h"
#include "ILE.h"

#define ROUND_QUAD(x) (((unsigned long long)(x) + 0xf) & ~0xf)

const int JVM_TYPE_NORMAL = 0;
const int JVM_TYPE_DEBUG  = 1;


bool    Pase::_jvm_pase   = false;
size_t  Pase::_ptr_bits   = 0;
int     Pase::_ccsid      = 819;
QP2_ptr64_t Pase::_id     = 0;

/*********************************************************
 * Start SQL Java with Pase - *SRVPGM QSYS/QSQLEJEXT
 * Parms:
 *    fr - error information
 * Return:
 *    true/false to indicate if JVM+Pase is started up
 * NOTE:
 *  getJVMtypeAndPort(&type, &port) will:
 *    - NOT start JVM if type = 0
 *    - start JVM with a random port if job ccsid != 65535
 *        and type = 1
 *********************************************************/
bool Pase::startJVM(FlightRec& fr)
{
  bool started = true;
  int type = JVM_TYPE_DEBUG; 
  int port = 30000;
  
  // ******* ALTERNATIVE: request to bind *SVRPGM QSYS/QSQLEJEXT *******
  // int rc = getJVMtypeAndPort(&type, &port); 
  // *******************************************************************

  static char jvmPgm[]      = "QSQLEJEXT";
  static char jvmLib[]      = "QSYS";
  static char jvmFunc[]     = "getJVMtypeAndPort";
  static _SYSPTR jvmPgmPtr  = NULL;
  static unsigned long long jvmActMark = 0;
  static void*  jvmFuncPtr = NULL;
  typedef int (*getJVMtypeAndPort_t)(int*, int*);
  int rc = 0;
  getJVMtypeAndPort_t getJVMtypeAndPort;
  ILE::ileRslv(jvmPgm, jvmLib, jvmFunc, &jvmPgmPtr, &jvmFuncPtr, fr);
  if ( ERR_OK != fr.code() )
  {
    started = false;
  }
  else
  {
    getJVMtypeAndPort = (getJVMtypeAndPort_t)jvmFuncPtr;
    rc = (*getJVMtypeAndPort)(&type, &port);  
    if ( 0 != rc )
    {
      if ( -332 == rc )
      {
        //---------------------------------------------------------------
        // If job ccsid is 65535, getJVMtypeAndPort() return -332.
        //---------------------------------------------------------------
        fr.code(ERR_PASE_START);
        fr.addMessage(rc, "JOB CCSID IS EXPECTED AS NOT 65535.");
        started = false;
      }
      else
      {
        fr.code(ERR_PASE_START);
        fr.addMessage(rc, "FAILED TO CALL getJVMtypeAndPort.");
        started = false;
      }
    }
    else
    { 
      // Determine Pase job ccsid that SQL started
      _ccsid = Qp2jobCCSID();
      if ( 0 == _ccsid ) 
      {
        fr.code(ERR_PASE_START);
        fr.addMessage("FAILED TO CALL Qp2jobCCSID.");
        started = false;
      }
    }
  }
  return started;
}

/**********************************************************
 * Start 64-bit Pase only
 * Parms:
 *    fr - error information
 * Return:
 *    true/false to indicate if JVM+Pase is started up
 **********************************************************/
bool Pase::startPase(FlightRec& fr)
{
  bool started = true;
  char *start64Path="/usr/lib/start64";
  char *arg_list[2];

  arg_list[0] = start64Path;
  arg_list[1] = NULL;
  int rc = Qp2RunPase(start64Path,
              NULL,
              NULL,
              0,
              _ccsid,
              (char**)&arg_list,
              NULL);

  if ( -1 == rc )
  {
    fr.code(ERR_PASE_START);
    fr.addMessage(rc, "FAILED TO CALL Qp2RunPase.");
    started = false;
  }
  return started;
}

/********************************************
 * Indicate to start java Pase.
 ********************************************/
void Pase::jvmPase(bool jvm)
{
  _jvm_pase = jvm;
}

/********************************************
 * Start Pase.
 ********************************************/
bool Pase::start(FlightRec& fr)
{
  _ptr_bits = Qp2ptrsize() * 8;
  if ( 0 == _ptr_bits )
  {
    end();

    bool started = false;
    if ( _jvm_pase )
    {
      started = startJVM(fr);
    }
    else
    {
      started = startPase(fr);
    }

    if ( !started )
    {
      return false;
    }

    // Check Pase is 32-bit or 64 bit
    _ptr_bits = Qp2ptrsize() * 8;

    if ( 0 == _ptr_bits ) 
    {
      fr.code(ERR_PASE_START);
      fr.addMessage("FAILED TO CALL Qp2ptrsize");
      return false;
    }
  }

  if ( 0 == _id )
  {
    // Open global name space
    _id = Qp2dlopen(NULL, QP2_RTLD_NOW, 0);
    if ( _id <= 0 )
    {
      fr.code(ERR_PASE_START);
      fr.addMessage("FAILED TO CALL Qp2dlopen.");

      int* err = Qp2errnop(); 
      if ( err != NULL && 0 != *err ) 
      {
        fr.addMessage(*err, "PASE ERROR NUMBER.");
      }
      return false;
    }
  }
  
  return true;
}

/*************************************************
 * End pase.
 ************************************************/
void Pase::end()
{
  if ( !_jvm_pase )
  {
    Qp2dlclose(_id);
    Qp2EndPase();
  }
}

/**************************************************
 * Run Shell command.
 * Parms:
 *      cmd     - command string
 *      output  - (output) output of cmd
 *      fr      - (output) error/result being returned
 *************************************************/
void Pase::runShell(const char* cmd, std::string& output, FlightRec& fr)
{
  if ( !start(fr) ) return;

  QP2_ptr64_t fp = popen(cmd, "r", fr);
  if ( NULL != fp )
  {
    const int BUFF_SIZE = 1024;
    char buff[BUFF_SIZE];
    QP2_ptr64_t paseBuff = NULL;
    char* ile2PaseBuff = (char*) Qp2malloc(BUFF_SIZE, &paseBuff);
    QP2_ptr64_t p = NULL;
    std::string paseOut;

    while ( NULL != (p = fgets(paseBuff, BUFF_SIZE, fp, fr)) ) 
    {
      paseOut.append(ile2PaseBuff, strlen(ile2PaseBuff));
    }

    if ( paseOut.length() > 0 )
    {
      char* pIn = const_cast<char*>(paseOut.c_str());
      size_t inLen = paseOut.length();
      size_t outLen = 3 * inLen;
      size_t outLeft = outLen;
      char* out = new char[outLen];
      if ( NULL == out) 
      {
        abort();
      }
      char* pOut = out;

      int rc = convertCcsid(_ccsid, 0, &pIn, &inLen, &pOut, &outLeft);
      if ( 0 != rc )
      {
        fr.code(ERR_PASE_RUN);
        fr.addMessage(rc, "FAILED TO CONVERT TO PASE CCSID.");
      }
      else 
      {
        output.append(out, (outLen-outLeft));
      }

      delete[] out;
    }

    Qp2free(ile2PaseBuff);

    pclose(fp, fr);
  }

}

/******************************************************************************
 * Calling an ILE program from Pase.
 *   int _PGMCALL(const ILEpointer  *target,
 *                void              **argv,
 *                unsigned          flags);
 * Parms:
 *    pgm       - pgm name
 *    lib       - pgm lib
 *    argv      - (input/output)argument pointer array on pase
 *    rtnValue  - return value from PGM
 *    fr        - error information
 *****************************************************************************/
void Pase::runPgm(const char* pgm, const char* lib, QP2_ptr64_t* argv, int* rtnValue, FlightRec& fr)
{
  if ( !start(fr) ) return;
  
  static void* pgmCallSym = NULL;
  char* ile2PaseStr = NULL;
  char* ile2PasePgmPtr  = NULL; 
  char* ileResultStr = NULL;
  char* ileResult = NULL;
  QP2_ptr64_t pasePgmPtr = NULL;
  QP2_ptr64_t paseResult = NULL;

  //----------------- load _PGMCALL() -------------------------------
  if ( NULL == pgmCallSym ) 
  {
    pgmCallSym = findSymbol("_PGMCALL", fr);
    if ( NULL == pgmCallSym )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO LOAD _PGMCALL.");
      return;
    }
  }

  //----------------- resolve sysptr for pgm ------------------------
  _SYSPTR pgmPtr = NULL;
  ILE::ileRslv(pgm, lib, NULL, &pgmPtr, NULL, fr);
  if ( ERR_OK != fr.code() )
  {
    return;
  }

  if ( 32 == _ptr_bits )
  {
    /////---------- argument signatures for _PGMCALL() --------------------
    static QP2_arg_type_t argSig32[] = {  QP2_ARG_PTR32, 
                                          QP2_ARG_PTR32,
                                          QP2_ARG_WORD,
                                          QP2_ARG_END };
    #pragma pack(4)
    struct 
    {
      QP2_ptr32_t target;
      QP2_ptr32_t argv;
      QP2_word_t flags;
    } argList32;
    #pragma pack()

    ////------------ prepare for parm "target" of _PGMCALL() ----------------------
    ile2PaseStr = allocAlignedPase(&ile2PasePgmPtr, &pasePgmPtr, sizeof(_SYSPTR), fr);
    if ( NULL == ile2PaseStr )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR PGM PTR.");
      return;
    }

    memcpy(ile2PasePgmPtr, (char*) &pgmPtr, sizeof(_SYSPTR));

    ////------------ prepare for parm "argv" of _PGMCALL() ----------------------
    argList32.target = (QP2_ptr32_t) pasePgmPtr;
    argList32.argv = (QP2_ptr32_t)(*argv); 
    // argList32.flags = 0x01 | 0x08; // PGMCALL_DIRECT_ARGS(1) | PGMCALL_NOMAXARGS(8) 
    argList32.flags = 8; 
    
    ////------------ prepare result buff ----------------------------------------
    short int resultLen = sizeof(QP2_word_t);
    ileResultStr = allocAlignedPase(&ileResult, &paseResult, resultLen, fr); // only accept INT
    if ( NULL == ileResultStr )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR PGM RESULT PTR.");
      return;
    }

    ////------------ call _PGMCALL() --------------------------------------------
    int sKey = 0;
    int eKey = 0;
    getMsgKey(sKey);
    #pragma exception_handler ( qp2callpase2_exc32, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
    int rc = Qp2CallPase2(pgmCallSym,
                          &argList32,
                          argSig32,
                          QP2_RESULT_WORD,
                          ileResult,
                          resultLen);
    #pragma disable_handler
    qp2callpase2_exc32:
    if ( 0 != rc ) // Qp2CallPase failed
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage(rc, "FAILED TO CALL Qp2CallPase2().");
      getMsgKey(eKey);
      getMessages(sKey, eKey, fr);
    }
    else // Qp2CallPase succeeded
    {
      int res = (int)(*ileResult); // return value from _PGMCALL()
      if ( 0 != res )
      {
        fr.code(ERR_PGM_RUN);
      }
      *rtnValue = res;
    }
  }
  else // 64-bit
  {
    /////---------- argument signatures for _PGMCALL() --------------------
    static QP2_arg_type_t argSig64[] = {  QP2_ARG_PTR64, 
                                          QP2_ARG_PTR64,
                                          QP2_ARG_DWORD,
                                          QP2_ARG_END };
    #pragma pack(8)
    struct 
    {
      QP2_ptr64_t target;
      QP2_ptr64_t argv;
      QP2_dword_t flags;
    } argList64;
    #pragma pack()

    ////------------ prepare for parm "target" of _PGMCALL() ----------------------
    ile2PaseStr = allocAlignedPase(&ile2PasePgmPtr, &pasePgmPtr, sizeof(_SYSPTR), fr);
    if ( NULL == ile2PaseStr )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR PGM PTR.");
      return;
    }

    memcpy(ile2PasePgmPtr, (char*) &pgmPtr, sizeof(_SYSPTR));

    ////------------ prepare for parm "argv" of _PGMCALL() ----------------------
    argList64.target = pasePgmPtr;
    argList64.argv = (QP2_ptr64_t)(*argv); 
    // argList64.flags = 0x01 | 0x08; // PGMCALL_DIRECT_ARGS(1) | PGMCALL_NOMAXARGS(8) 
    argList64.flags = 8; 
    
    ////------------ prepare result buff ----------------------------------------
    short int resultLen = sizeof(QP2_dword_t);
    ileResultStr = allocAlignedPase(&ileResult, &paseResult, resultLen, fr); // only accept INT
    if ( NULL == ileResultStr )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR PGM RESULT PTR.");
      return;
    }

    ////------------ call _PGMCALL() --------------------------------------------
    int sKey = 0;
    int eKey = 0;
    getMsgKey(sKey);
    #pragma exception_handler ( qp2callpase2_exc64, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
    int rc = Qp2CallPase2(pgmCallSym,
                          &argList64,
                          argSig64,
                          QP2_RESULT_DWORD,
                          ileResult,
                          resultLen);
    #pragma disable_handler
    qp2callpase2_exc64:
    if ( 0 != rc )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage(rc, "RETUN VALUE FROM Qp2CallPase2().");
      getMsgKey(eKey);
      getMessages(sKey, eKey, fr);
    }
    else
    {
      int res = (int)(*ileResult);
      if ( 0 != res )
      {
        fr.code(ERR_PGM_RUN);
      }  
      *rtnValue = res;
    }
  }

  //---------- free pase memory ---------------------------
  Qp2free(ile2PaseStr);
  Qp2free(ileResultStr);
}

/******************************************************************************
 * Calling an ILE service program from Pase.
 * int _ILECALL(const ILEpointer *target,
 *              ILEarglist_base  *ILEarglist,
 *              const arg_type_t *signature,
 *              result_type_t    result_type)
 * Parms:
 *    srvpgm    - service pgm name
 *    lib       - service pgm lib
 *    func      - exported function name
 *    argv      - (input/output)argument pase pointer array on ILE tagged pointers
 *    sigs      - signature pase pointer array
 *    rtnType   - type of return value
 *    fr        - error information
 *****************************************************************************/
void Pase::runSrvgm(const char* srvpgm, const char* lib, const char* func, 
                    QP2_ptr64_t* argv, QP2_ptr64_t* sigs, 
                    Result_Type_t rtnType, FlightRec& fr)
{
  if ( !start(fr) ) return;
  
  static void* ILECALLSym = NULL;
  char* ileILECALLStr = NULL;
  char* ileILECALLPtr  = NULL; 
  void* funcPtr = NULL;
  char* ileFuncStr = NULL;
  char* ileFuncPtr = NULL;
  QP2_ptr64_t paseILECALLPtr = NULL;
  QP2_ptr64_t paseFuncPtr = NULL;

  //----------------- load _ILECALL() -------------------------------
  if ( NULL == ILECALLSym ) 
  {
    ILECALLSym = findSymbol("_ILECALL", fr);
    if ( NULL == ILECALLSym )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO LOAD _ILECALL.");
      return;
    }
  }

  ////----------------- resolve sysptr for exported function ---------------
  _SYSPTR pgmPtr = NULL;
  ILE::ileRslv(srvpgm, lib, func, &pgmPtr, &funcPtr, fr);
  if ( ERR_OK != fr.code() )
  {
    return;
  }

  ileFuncStr = allocAlignedPase(&ileFuncPtr, &paseFuncPtr, sizeof(void*), fr);
  if ( NULL == ileFuncStr )
  {
    fr.code(ERR_PGM_RUN);
    fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR FUNCTION PTR.");
    return;
  }
  memcpy(ileFuncPtr, (char*) &funcPtr, sizeof(void*));

  if ( 32 == _ptr_bits )
  { 
    ////------------- signatures for _ILECALL() ----------------------------------
    static QP2_arg_type_t argSig32[] = {  QP2_ARG_PTR32, 
                                          QP2_ARG_PTR32,
                                          QP2_ARG_PTR32,
                                          QP2_ARG_WORD,
                                          QP2_ARG_END };
    #pragma pack(4)
    struct 
    {
      QP2_ptr32_t   target;
      QP2_ptr32_t   ILEarglist;
      QP2_ptr32_t   signature;
      QP2_word_t   result_type;
    } argList32;
    #pragma pack()

    ////------------ parm "target" for qp2callpase() ----------------------
    ileILECALLStr = allocAlignedPase(&ileILECALLPtr, &paseILECALLPtr, sizeof(void*), fr);
    if ( NULL == ileILECALLStr )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR SVRPGM FUNC PTR.");
      return;
    }
    memcpy(ileILECALLPtr, (char*) &funcPtr, sizeof(void*));

    ////------------ parms for _ILECALL() ---------------------------------
    #pragma pack(4)
    typedef struct ILECALL_Args_t
    {
      QP2_ptr32_t       target;       // ILEpointer*
      QP2_ptr32_t       ILEarglist;   // ILEarglist_base*
      QP2_ptr32_t       signature;    // ILECALL_Arg_t*
      Result_Type_t     result_type;
    } ILECALL_Args;
    #pragma pack()

    ////----------------- _ILECALL() args ----------------------------------------------------
    QP2_word_t result = 0;
    argList32.target = (QP2_ptr32_t) paseFuncPtr;
    argList32.ILEarglist = (QP2_ptr32_t)(*argv);
    argList32.signature = (QP2_ptr32_t)(*sigs);
    argList32.result_type = rtnType;

    ////------------ call _ILECALL() --------------------------------------------
    #pragma exception_handler ( qp2callpase2_exc64, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
    int rc = Qp2CallPase2(ILECALLSym,
                          &argList32,
                          argSig32,
                          QP2_RESULT_WORD,
                          &result,
                          sizeof(result));
    #pragma disable_handler
    qp2callpase2_exc64:
    if ( 0 != rc )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage(rc, "FAILED TO PASE CALL _ILECALL().");
    }
    else
    {
      if ( 0 != result )  
      {
        int* err = Qp2errnop(); 
        if ( err != NULL && 0 != *err ) 
        {
          fr.code(ERR_PGM_RUN);
          fr.addMessage(*err, "ERRNO FOR _ILECALL().");
        }
      }
    }
  }
  else // 64-bit
  { 
    ////------------- signatures for _ILECALL() ----------------------------------
    static QP2_arg_type_t argSig64[] = {  QP2_ARG_PTR64, 
                                          QP2_ARG_PTR64,
                                          QP2_ARG_PTR64,
                                          QP2_ARG_DWORD,
                                          QP2_ARG_END };
    #pragma pack(8)
    struct 
    {
      QP2_ptr64_t   target;
      QP2_ptr64_t   ILEarglist;
      QP2_ptr64_t   signature;
      QP2_dword_t   result_type;
    } argList64;
    #pragma pack()

    ////------------ parm "target" for qp2callpase() ----------------------
    ileILECALLStr = allocAlignedPase(&ileILECALLPtr, &paseILECALLPtr, sizeof(void*), fr);
    if ( NULL == ileILECALLStr )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR SVRPGM FUNC PTR.");
      return;
    }
    memcpy(ileILECALLPtr, (char*) &funcPtr, sizeof(void*));

    ////------------ parms for _ILECALL() ---------------------------------
    #pragma pack(8)
    typedef struct ILECALL_Args_t
    {
      QP2_ptr64_t       target;       // ILEpointer*
      QP2_ptr64_t       ILEarglist;   // ILEarglist_base*
      QP2_ptr64_t       signature;    // ILECALL_Arg_t*
      Result_Type_t     result_type;
    } ILECALL_Args;
    #pragma pack()

    ////----------------- _ILECALL() args ----------------------------------------------------
    QP2_dword_t result = 0;
    argList64.target = paseFuncPtr;
    argList64.ILEarglist = (QP2_ptr64_t)(*argv);
    argList64.signature = (QP2_ptr64_t)(*sigs);
    argList64.result_type = rtnType;

    ////------------ call _ILECALL() --------------------------------------------
    #pragma exception_handler ( qp2callpase2_exc, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
    int rc = Qp2CallPase2(ILECALLSym,
                          &argList64,
                          argSig64,
                          QP2_RESULT_DWORD,
                          &result,
                          sizeof(result));
    #pragma disable_handler
    qp2callpase2_exc:
    if ( 0 != rc )
    {
      fr.code(ERR_PGM_RUN);
      fr.addMessage(rc, "FAILED TO PASE CALL _ILECALL().");
    }
    else
    {
      if ( 0 != result )  
      {
        int* err = Qp2errnop(); 
        if ( err != NULL && 0 != *err ) 
        {
          fr.code(ERR_PGM_RUN);
          fr.addMessage(*err, "ERRNO FOR _ILECALL().");
        }
      }
    }
  }

  //---------- free pase memory ---------------------------
  Qp2free(ileILECALLStr);
  Qp2free(ileFuncStr);
}


/**************************************************
 * Find function symbol.
 * Parms:
 *    func - string name of function
 *    fr   - (output) error information
 * Return:
 *    function pointer
 *************************************************/
void* Pase::findSymbol(const char* func, FlightRec& fr)
{
  void* funcSym = NULL;

  funcSym = Qp2dlsym(_id, func, 0, NULL);
  if ( NULL == funcSym )
  {
    fr.code(ERR_PASE_RUN);
    fr.addMessage("FAILED TO CALL Qp2dlsym.");

    int* err = Qp2errnop();
    if ( err != NULL && 0 != *err ) 
    {
      fr.addMessage(*err, "PASE ERRNO");
    }
  }

  return funcSym;
}

/*************************************************************
 *  Qp2malloc and Convert input from job ccsid to pase ccsid.
 * Parms:
 *    in      - chars being converted
 *    len     - length to convert
 *    pasePtr - pase ptr to hold allocated memory
 * Return:
 *    NULL  - fail to convert
 *    ptr   - ile ptr to allocated Pase memory with converted 
 *            output
 *************************************************************/
char* Pase::allocAndCvt(const char* in, int len, QP2_ptr64_t* pasePtr, FlightRec& fr)
{
  char* out_ile2pase = NULL;
  int outLen = 3 * len; // Factor to multiply
  out_ile2pase = (char*) Qp2malloc(outLen, pasePtr);
  if ( NULL != out_ile2pase )
  {
    char* pIn  = const_cast<char*>(in);
    char* pOut = out_ile2pase;
    size_t iLen = len;
    size_t oLen = outLen;
    int rc = convertCcsid(0, _ccsid, &pIn, &iLen, &pOut, &oLen);
    if ( 0 != rc )
    {
      Qp2free(out_ile2pase);
      out_ile2pase = NULL;
      fr.code(ERR_PASE_RUN);
      fr.addMessage(rc, "FAILED TO CVT TO PASE CCSID");
    }
  }
  else
  {
    abort();
  }
  return out_ile2pase;
}

/********************************************************************
 * Allocate memory on Pase and round the pointer to 16-byte
 * alignment.
 * Parms:
 *    ileAlignedPtr     - ptr to ptr for ile use aligned pase memory
 *    paseAlignedPtr    - pase ptr after 16-byte aligned
 *    size              - bytes required
 *    align             - address to align. default is 16 bytes.
 *    fr                - error information
 * Return:
 *    NULL  - failed to allocate pase memory
 *    ptr   - ile usable ptr to start of pase memory
 *******************************************************************/
char*  Pase::allocAlignedPase(char** ileAlignedPtr, QP2_ptr64_t* paseAlignedPtr, 
          int size, FlightRec& fr, int align)
{
  assert( align >= 0 );
  if ( !start(fr) ) return NULL;

  char* retPtr = NULL;
  QP2_ptr64_t paseStr = NULL;
  *paseAlignedPtr = NULL;
  // allocate 15 bytes more for 16-byte alignment later
  retPtr = (char*) Qp2malloc(size + align, &paseStr);

  if ( NULL != retPtr )
  {
    *paseAlignedPtr = (QP2_ptr64_t) ROUND_QUAD(paseStr);
    *ileAlignedPtr = retPtr + (paseStr - *paseAlignedPtr);
  }

  return retPtr;
}

/****************************************************
 * Pase - FILE* popen(const char *, const char *)
 ****************************************************/
QP2_ptr64_t Pase::popen(const char* file, const char* mode, FlightRec& fr)
{
  assert(_id > 0);
  int rc = 0;
  char* file_ile2pase = NULL;
  char* mode_ile2pase = NULL;
  static void* func = NULL;
  QP2_ptr64_t fp = NULL;

  if ( NULL == func )
  {
    func = findSymbol("popen", fr);
    if ( NULL == func )
    {
      return fp;
    }
  }

  //------------- qp2callpase() --------------------------------
  if ( 32 == _ptr_bits )
  {
    static QP2_arg_type_t argSig32[] = {  QP2_ARG_PTR32, 
                                          QP2_ARG_PTR32, 
                                          QP2_ARG_END };
    #pragma pack(4)
    struct 
    {
      QP2_ptr32_t file;
      QP2_ptr32_t mode;
    } argList32;
    #pragma pack()

    QP2_ptr64_t filePtr64;
    file_ile2pase = allocAndCvt(file, strlen(file), &filePtr64, fr);
    if ( NULL == file_ile2pase )
    {
      return fp;
    }
    else
    {
      argList32.file = (QP2_ptr32_t) filePtr64;
    }

    QP2_ptr64_t modePtr64;
    mode_ile2pase = allocAndCvt(mode, strlen(mode), &modePtr64, fr);
    if ( NULL == mode_ile2pase )
    {
      Qp2free(file_ile2pase);
      return fp;
    }
    else
    {
      argList32.mode = (QP2_ptr32_t) modePtr64;
    }

    rc = Qp2CallPase2(func,
                      &argList32,
                      argSig32,
                      QP2_RESULT_PTR32,
                      &fp,
                      sizeof(fp));
    if ( 0 != rc )
    {
      fr.code(ERR_PASE_RUN);
      fr.addMessage(rc, "FAILED TO CALL popen.");

      int* err = Qp2errnop();  
      if ( err != NULL && 0 != *err ) 
      {
        fr.addMessage(*err, "PASE ERRNO");
      }
    }
    else
    {
      int* err = Qp2errnop(); 
      if ( err != NULL && 0 != *err ) 
      {
        fr.code(ERR_PASE_RUN);
        fr.addMessage(*err, "ERRNO FOR popen.");
      }
    }
  }
  else // 64-bit
  {
    static QP2_arg_type_t argSig64[] = {  QP2_ARG_PTR64, 
                                          QP2_ARG_PTR64, 
                                          QP2_ARG_END };
    #pragma pack(8)
    struct 
    {
      QP2_ptr64_t file;
      QP2_ptr64_t mode;
    } argList64;
    #pragma pack()

    file_ile2pase = allocAndCvt(file, strlen(file), &argList64.file, fr);
    if ( NULL == file_ile2pase )
    {
      return fp;
    }
    mode_ile2pase = allocAndCvt(mode, strlen(mode), &argList64.mode, fr);
    if ( NULL == mode_ile2pase )
    {
      Qp2free(file_ile2pase);
      return fp;
    }

    rc = Qp2CallPase2(func,
                      &argList64,
                      argSig64,
                      QP2_RESULT_PTR64,
                      &fp,
                      sizeof(fp));
    if ( 0 != rc )
    {
      fr.code(ERR_PASE_RUN);
      fr.addMessage(rc, "FAILED TO CALL popen.");

      int* err = Qp2errnop();  
      if ( err != NULL && 0 != *err ) 
      {
        fr.addMessage(*err, "PASE ERRNO");
      }
    }
    else
    {
      int* err = Qp2errnop(); 
      if ( err != NULL && 0 != *err ) 
      {
        fr.code(ERR_PASE_RUN);
        fr.addMessage(*err, "ERRNO FOR popen.");
      }
    }
  }

  //-------- free pase memory -----------------------
  int rcx = Qp2free(file_ile2pase);
  rcx = Qp2free(mode_ile2pase);

  return fp;
}

/****************************************************
 * Pase - int pclose(FILE*)
 ****************************************************/
int Pase::pclose(QP2_ptr64_t fp, FlightRec& fr)
{
  assert( _id > 0 );
  int ret = -1;
  static void* func = NULL;

  if ( NULL == func )
  {
    FlightRec fr;
    func = findSymbol("pclose", fr);
    if ( NULL == func )
    {
      return ret;
    }
  }

  //------------- qp2callpase() --------------------------------
  if ( 32 == _ptr_bits )
  {
    static QP2_arg_type_t argSig32[] = {  QP2_ARG_PTR32, 
                                          QP2_ARG_END };
    #pragma pack(4)
    struct 
    {
      QP2_ptr32_t fp;
    } argList32;
    #pragma pack()

    QP2_word_t result = 0;
    argList32.fp = fp;

    int rc = Qp2CallPase2(func,
                      &argList32,
                      argSig32,
                      QP2_RESULT_WORD,
                      &result,
                      sizeof(result));
    if ( 0 != rc )
    {
      ret = rc;
      fr.code(ERR_PASE_RUN);
      fr.addMessage(ret, "FAILED TO PASE CALL pclose.");
    }
    else
    {
      int* err = Qp2errnop();
      if ( NULL != err && 0 != *err ) 
      {
        ret = *err;
        fr.code(ERR_PASE_RUN);
        fr.addMessage(ret, "FAILED TO CALL pclose.");
      }
      else
      {
        ret = result;
      }
    }
  }
  else // 64 == _ptr_bits
  {
    static QP2_arg_type_t argSig64[] = {  QP2_ARG_PTR64, 
                                          QP2_ARG_END };
    #pragma pack(8)
    struct 
    {
      QP2_ptr64_t fp;
    } argList64;
    #pragma pack()

    QP2_dword_t result = 0;
    argList64.fp = fp;

    int rc = Qp2CallPase2(func,
                      &argList64,
                      argSig64,
                      QP2_RESULT_DWORD,
                      &result,
                      sizeof(result));
    if ( 0 != rc )
    {
      ret = rc;
      fr.code(ERR_PASE_RUN);
      fr.addMessage(ret, "FAILED TO PASE CALL pclose.");
    }
    else
    {
      int* err = Qp2errnop();
      if ( NULL != err && 0 != *err ) 
      {
        ret = *err;
        fr.code(ERR_PASE_RUN);
        fr.addMessage(ret, "FAILED TO CALL pclose.");
      }
      else
      {
        ret = result;
      }
    }
  }
  return ret;
}

/***************************************************************
 * Pase - char *fgets(char *s, int n, FILE *stream)
 * Parms:
 *    paseBuff  - caller pase buffer ptr to working
 *    size      - receiver size
 *    fr        - (output) error information
 * Return:
 *    NULL  - fail
 *    ptr   - same to buff
 ***************************************************************/
QP2_ptr64_t Pase::fgets(QP2_ptr64_t paseBuff, int size, QP2_ptr64_t fp, FlightRec& fr)
{
  assert(_id > 0);

  QP2_ptr64_t paseRet = NULL;
  int rc = 0;

  static void* func = NULL;

  if ( NULL == func )
  {
    func = findSymbol("fgets", fr);
    if ( NULL == func )
    {
      return paseRet;
    }
  }

  //------------- qp2callpase() --------------------------------
  if ( 32 == _ptr_bits )
  {
    static QP2_arg_type_t argSig32[] = {  QP2_ARG_PTR32,
                                          QP2_ARG_WORD, 
                                          QP2_ARG_PTR32, 
                                          QP2_ARG_END };
    #pragma pack(4)
    struct 
    {
      QP2_ptr32_t buff;
      QP2_word_t size;
      QP2_ptr32_t fp;
    } argList32;
    #pragma pack()

    argList32.buff = paseBuff;
    argList32.size = size;
    argList32.fp = fp;

    rc = Qp2CallPase2(func,
                      &argList32,
                      argSig32,
                      QP2_RESULT_PTR32,
                      &paseRet,
                      sizeof(paseRet));
    if ( 0 != rc )
    {
      fr.code(ERR_PASE_RUN);
      fr.addMessage(rc, "FAILED TO CALL fgets.");

      int* err = Qp2errnop();
      if ( err != NULL && 0 != *err ) 
      {
        fr.addMessage(*err, "PASE ERRNO");
      }
    }
    else
    {
      int* err = Qp2errnop();
      if ( err != NULL && 0 != *err ) 
      {
        paseRet = NULL;
        fr.code(ERR_PASE_RUN);
        fr.addMessage(*err, "FAILED TO CALL fgets.");
      }
      else
      { /* succeed */ }
    }
  }
  else // 64-bit
  {
    static QP2_arg_type_t argSig64[] = {  QP2_ARG_PTR64,
                                              QP2_ARG_DWORD, 
                                              QP2_ARG_PTR64, 
                                              QP2_ARG_END };
    #pragma pack(8)
    struct 
    {
      QP2_ptr64_t buff;
      QP2_dword_t size;
      QP2_ptr64_t fp;
    } argList64;
    #pragma pack()

    argList64.buff = paseBuff;
    argList64.size = size;
    argList64.fp = fp;

    rc = Qp2CallPase2(func,
                      &argList64,
                      argSig64,
                      QP2_RESULT_PTR64,
                      &paseRet,
                      sizeof(paseRet));
    if ( 0 != rc )
    {
      fr.code(ERR_PASE_RUN);
      fr.addMessage(rc, "FAILED TO CALL fgets.");

      int* err = Qp2errnop();
      if ( err != NULL && 0 != *err ) 
      {
        fr.addMessage(*err, "PASE ERRNO");
      }
    }
    else
    {
      int* err = Qp2errnop();
      if ( err != NULL && 0 != *err ) 
      {
        paseRet = NULL;
        fr.code(ERR_PASE_RUN);
        fr.addMessage(*err, "FAILED TO CALL fgets.");
      }
      else
      { /* succeed */ }
    }
  }

  return paseRet;
}
