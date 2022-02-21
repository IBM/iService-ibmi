#include "Ipc.h"
#include "stdlib.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include "Utility.h"
#include "Control.h"
#include "JobUtil.h"

const int SHDM_SIZE     = 16000000; // ~ 16 megabytes
const int SEMKEY_ID     = 1;
const int SHDMKEY_ID    = 1;

//// Request format in shrdm //////////////
#pragma pack(1)
typedef struct Request_Header_t
{
  int     ctl_size;
  int     in_size;
  char    data;
  /* ctl data  */
  /* input data  */
}Request_Header;
#pragma pack()

//// Response format in shrdm //////////////
#pragma pack(1)
typedef struct Response_Header_t
{
  int     size;
  char    data;
  /* output data  */
}Response_Header;
#pragma pack()

//// signals ///////////////////////////////
struct sigaction sact;
struct sigaction oact;

/******************************************************
 * Constructor in private.
 ******************************************************/
IPC::IPC() 
  : _server(false), _ipcKey(), _shdm_p(NULL), _semid(-1),
    _shdmid(-1), _semkey((key_t) -1), _shmkey((key_t) -1),
    _server_submitted(false)
{ }

/******************************************************
 * Destructor for static cleanup.
 ******************************************************/
IPC::~IPC()
{
  FlightRec fr;
  IPC::destroy(fr);
}

/*************************************************
 * Wait and block other clients
 * 
 * Return:
 *    0 - successful
 *    1 - restartable
 *   -1 - failed
 *************************************************/
int IPC::lockClientQ(FlightRec& fr)
{
  int ret = 0;
  int rc = 0; 
  IPC* &ipcPtr = me();

  ipcPtr->_ops[0].sem_num = QCLNT; 
  ipcPtr->_ops[0].sem_op =  0;
  ipcPtr->_ops[0].sem_flg = 0;

  ipcPtr->_ops[1].sem_num = QCLNT; 
  ipcPtr->_ops[1].sem_op =  1;
  if ( IPC::isServer() )
  {
    ipcPtr->_ops[1].sem_flg = 0; 
  }
  else
  {
    /////////////////////////////////////////////////////////////////////
    // If IPC client job is ended immediately after IPC server gets    //
    // ready to listen for next client, current client job could not   //
    // release client q.                                               //
    /////////////////////////////////////////////////////////////////////
    ipcPtr->_ops[1].sem_flg = SEM_UNDO;
  }

  rc = semop( ipcPtr->_semid, ipcPtr->_ops, 2 ); 
  if (rc == -1)
  {
    if ( EIDRM == errno )
    {
      // EIDRM(3509) - The semaphore, shared memory, or message queue identifier is removed from the system.                                                          
      ret = 1;
    }
    else
    {
      fr.code(ERR_IPC_CLNT_REQUEST);
      fr.addMessage(errno, strerror(errno));
      ret = -1;
    }
  }
  else
  {}

  return ret;
}


/******************************************************
 * Create internal instance.
 ******************************************************/
IPC*& IPC::me()
{
  static IPC* _me = NULL;
  if ( NULL == _me )
  {
    static IPC ipc;
    _me = &ipc;
  }

  return _me;
}

/******************************************************
 * Set up IPC server. 
 * 
 * Parms:
 *  ipc_key   -   /path/file  with null-termination
 *  ccsid     -   ccsid of ipc_key
 *  fr        -   error information
 * Return:
 *  true      -   successful
 *  false     -   failed
 ******************************************************/
bool IPC::setupIpcServer(const char* ipc_key, int ccsid, FlightRec& fr)
{
  IPC* &ipcPtr = me();
  int  rc = 0;
  //------------- mark as server ----------------------------------------
  ipcPtr->_server = true;

  //------------- Users in the same group have the access ----------------
  ipcPtr->_ipcKey = ipc_key;
  trim(ipcPtr->_ipcKey);

  ////---------- Create ipc dir ----------------------------------------
  if ( 0 != mkdir(ipcPtr->_ipcKey.c_str(), S_IRWXU | S_IRWXG ) )
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO CREATE DIR FOR IPC.");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    fr.addMessage(errno, strerror(errno));

    ////////////////////////////////////////////////////////////////////
    // Clear out ipc key path, so destroy() won't remove ipc key path //
    // created by the successfully launched ipc server.               //
    ////////////////////////////////////////////////////////////////////
    ipcPtr->_ipcKey.clear();

    return false;
  }
  
  ////--------- Set *PUBLIC to *EXCLUDE -------------------------------------
  std::string cmd;
  cmd.clear();
  cmd += "CHGAUT OBJ('";
  cmd += ipcPtr->_ipcKey;
  cmd += "') USER(*PUBLIC) DTAAUT(*EXCLUDE) OBJAUT(*NONE) SUBTREE(*ALL) SYMLNK(*YES)";
  doCommand(cmd, fr);
  if ( ERR_OK != fr.code() )
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO REVOKE *PUBLIC AUTHORTIES.");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    return false;    
  }
  ////--------- Grant accesses to group profile -----------------------------
  if ( strlen(JobUtil::JobGrpPrfName()) > 0 )
  {
    cmd.clear();
    cmd += "CHGAUT OBJ('";
    cmd += ipcPtr->_ipcKey;
    cmd += "') USER(";
    cmd += JobUtil::JobGrpPrfName();
    cmd += ") DTAAUT(*RWX) OBJAUT(*NONE) SUBTREE(*ALL) SYMLNK(*YES)";
    doCommand(cmd, fr);
    if ( ERR_OK != fr.code() )
    {
      fr.code(ERR_IPC_SVR_SETUP);
      fr.addMessage("FAILED TO GRANT GROUP PROFILE AUTHORTIES.");
      fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
      return false;    
    }
  }

  ipcPtr->_semkey = ftok(ipcPtr->_ipcKey.c_str(),SEMKEY_ID);
  if ( ipcPtr->_semkey == (key_t)-1 )
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO ftok() ON SEMPHORE.");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  ipcPtr->_shmkey = ftok(ipcPtr->_ipcKey.c_str(),SHDMKEY_ID);
  if ( ipcPtr->_shmkey == (key_t)-1 )
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO ftok() ON SHARED MEMORY.");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  ipcPtr->_semid = semget( ipcPtr->_semkey, SEM_NUM, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
  if ( ipcPtr->_semid == -1 )
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO semget().");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    fr.addMessage(errno, strerror(errno));
    return false;
  }


  short  sarray[SEM_NUM] = {0};
  // 2nd parm value "1" is no use as SETALL is used.
  rc = semctl( ipcPtr->_semid, 1, SETALL, sarray);
  if( rc == -1 )
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO semctl().");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  ipcPtr->_shdmid = shmget(ipcPtr->_shmkey, SHDM_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
  if (ipcPtr->_shdmid == -1)
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO shmget().");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  ipcPtr->_shdm_p = shmat(ipcPtr->_shdmid, NULL, 0);
  if ( ipcPtr->_shdm_p == NULL )
  {
    fr.code(ERR_IPC_SVR_SETUP);
    fr.addMessage("FAILED TO shmat().");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  return true;
}

/******************************************************
 * Connect IPC server. If failed, submit IPC server job
 * and retry connecting in a given time.
 *  
 * Parms:
 *  fr        -   error information
 ******************************************************/
bool IPC::clientConnect(FlightRec& fr)
{
  IPC* &ipcPtr = me();
  const int  TIME2WAIT = Control::waitServer() * 1000 * 1000; // 5s 
  const int  INTERVAL  = 10* 1000; // 10ms

  ipcPtr->_ipcKey = Control::ipcKey();

  int  elapsed = 0;
  while ( elapsed < TIME2WAIT && ERR_OK == fr.code() )
  {
    ipcPtr->_semkey = ftok( ipcPtr->_ipcKey.c_str(), SEMKEY_ID );
    if ( ipcPtr->_semkey == (key_t)-1 )
    {
      if ( ENOENT == errno )
      {
        //--------- start server since ipc path not created yet -----------//
        if ( ! startIpcServerOnce(fr) )
        {
          fr.code(ERR_IPC_CLNT_CONNECT);
          fr.addMessage("FAILED TO START IPC SERVER.");
          fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
          break;       
        }
        else
        { 
          /********* wait server up **************/
          elapsed += INTERVAL;
          usleep(INTERVAL);
          fr.code(ERR_OK);
        }
      }
      else
      {
        fr.code(ERR_IPC_CLNT_CONNECT);
        fr.addMessage("FAILED TO ftok() ON SEMPHORE.");
        fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
        fr.addMessage(errno, strerror(errno));
      }

      continue;
    }


    if ( (ipcPtr->_shmkey = ftok(ipcPtr->_ipcKey.c_str(),SHDMKEY_ID)) == (key_t)-1 )
    {
      fr.code(ERR_IPC_CLNT_CONNECT);
      fr.addMessage("FAILED TO ftok() ON SHARED MEMORY.");
      fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
      fr.addMessage(errno, strerror(errno));

      continue;
    }

    if ( (ipcPtr->_semid = semget( ipcPtr->_semkey, SEM_NUM, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP )) == -1 )
    {
      if ( ENOENT == errno )
      { 
        /********* wait server up **************/
        elapsed += INTERVAL;
        usleep(INTERVAL);
        fr.code(ERR_OK);
      }
      else
      {
        fr.code(ERR_IPC_CLNT_CONNECT);
        fr.addMessage("FAILED TO semget().");
        fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
        fr.addMessage(errno, strerror(errno));
      }

      continue;
    }
    
    if ( (ipcPtr->_shdmid = shmget(ipcPtr->_shmkey, SHDM_SIZE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP )) == -1 )
    {
      if ( ENOENT == errno )
      { 
        /********* wait server up **************/
        elapsed += INTERVAL;
        usleep(INTERVAL);
        fr.code(ERR_OK);
      }
      else
      {
        fr.code(ERR_IPC_CLNT_CONNECT);
        fr.addMessage("FAILED TO shmget().");
        fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
        fr.addMessage(errno, strerror(errno));
      }

      continue;
    }

    if ( (ipcPtr->_shdm_p = shmat(ipcPtr->_shdmid, NULL, 0)) == NULL )
    {
      fr.code(ERR_IPC_CLNT_CONNECT);
      fr.addMessage("FAILED TO shmat().");
      fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
      fr.addMessage(errno, strerror(errno));

      continue;
    }
    else
    { }

    break; // when got here, everything is good.
  }

  if ( elapsed >= TIME2WAIT )
  {
    fr.code(ERR_IPC_CLNT_CONNECT);
    fr.addMessage("STARTING IPC SERVER TIMED OUT.");
    fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
  }

  return fr.code() == ERR_OK ;
}

/******************************************************
 * Clean up IPC client to release Q to other clients
 *  
 * Parms:
 *  fr        -   error information
 ******************************************************/
bool IPC::clientClose(FlightRec& fr)
{
  IPC* &ipcPtr = me();
  int rc = 0;
  bool ret = true;

  //--------- detach shared memory ------------------
  if ( NULL != ipcPtr->_shdm_p )
  {
    rc = shmdt(ipcPtr->_shdm_p);
    if ( rc == -1 )
    {
      fr.code(ERR_IPC_CLNT_CLOSE);
      fr.addMessage("FAILED TO DETACH SHARED MOMERY.");
      fr.addMessage(errno, strerror(errno));
      ret = false;
    }
    else
    {
      ipcPtr->_shdm_p = NULL;
    }
  }
  ///////////////////////////////////////////////////////////////////
  // client Q is automatically unlocked by SEM_UNDO when job ends. //
  ///////////////////////////////////////////////////////////////////

  return true;
}

/******************************************************
 * Set status of whether server job is submitted.
 * 
 * Parms:
 *    flag - boolean switch
 ******************************************************/
void IPC::serverSubmitted(bool flag)
{
  IPC* &ipcPtr = me();
  ipcPtr->_server_submitted = flag;
}

/******************************************************
 * Copy client request data into shared memory and wait
 * server to process.
 *  
 * Parms:
 *  ctl_size  -   size of control flag string
 *  in_size   -   size of input
 *  control   -   control flag string
 *  in        -   input
 *  fr        -   error information
 ******************************************************/
bool IPC::sendRequest(int ctl_size, int in_size, const char* control, const char* in, FlightRec& fr)
{
  IPC* &ipcPtr = me();
  int rc = 0;

  //=======================================================
  //------ Copy data to shared memory ---------------------
  //=======================================================
#pragma exception_handler ( sndreq_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
  Request_Header* req = (Request_Header*)ipcPtr->_shdm_p;
  int ctlSize = ctl_size + 1;
  req->ctl_size = ctlSize;
  req->in_size  = in_size;
  char* ctlData = &(req->data);
  char* inData  = ctlData + ctlSize;
  memcpy(ctlData, control, ctl_size);
  memset(ctlData + ctl_size, '\0', 1); // Appended null-terminator
  memcpy(inData, in, in_size);

  //---- Wake up server to process request  --------------------- 
  ipcPtr->_ops[0].sem_num = SVR_WAIT; 
  ipcPtr->_ops[0].sem_op =  1;
  ipcPtr->_ops[0].sem_flg = 0;

  rc = semop( ipcPtr->_semid, ipcPtr->_ops, 1 );
  if (rc == -1)
  {
    fr.code(ERR_IPC_CLNT_REQUEST);
    fr.addMessage("FAILED TO NOTIFY SERVER TO CLIENT DONE WITH REQUEST.");
    fr.addMessage(errno, strerror(errno));
    return false;
  }
  else
  {
    ////////////////////////////////////////////////////////////////////
    // Server waits request to copy in at this point, so it deosn't   //
    // matter client is interrupted by a signal without notifying     //
    // server. In the client cleanup, client Q is unlocked and another//
    // client still has a clean start as well as server.              //
    ////////////////////////////////////////////////////////////////////
  }

  //---- Client waits server to process request  --------------------- 
  ipcPtr->_ops[0].sem_num = CLNT_WAIT; 
  ipcPtr->_ops[0].sem_op =  -1;
  ipcPtr->_ops[0].sem_flg = 0;
  
  rc = semop( ipcPtr->_semid, ipcPtr->_ops, 1 );
  if (rc == -1)
  {
    fr.code(ERR_IPC_CLNT_REQUEST);
    fr.addMessage("FAILED TO WAIT SERVER TO PROCESS REQUEST.");
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  return true;

#pragma disable_handler
  sndreq_ex:
    // IPC Server times out when client copies request data into shrdm 
    fr.code(ERR_IPC_CLNT_REQUEST);
    fr.addMessage("FAILED TO SEND REQUEST DATA.");
    return false;
}

/******************************************************
 * Receive output from server in shared memory and 
 * notify server done with receiving.
 *  
 * Parms:
 *  out       -   (output) buffer to hold data from server
 *  fr        -   error information
 ******************************************************/
bool IPC::receiveResponse(std::string& out, FlightRec&fr)
{
  IPC* &ipcPtr = me();
  int rc = 0;

  //=========================================================
  //------- receive from shrdm ------------------------------
  //=========================================================
#pragma exception_handler ( sndres_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
  Response_Header* res = (Response_Header*)ipcPtr->_shdm_p;
  char* outData = &(res->data);
  out.append(outData, res->size);

  //---- wake up server as client done with response -------- 
  ipcPtr->_ops[0].sem_num = SVR_WAIT; 
  ipcPtr->_ops[0].sem_op =  1;
  ipcPtr->_ops[0].sem_flg = 0;

  rc = semop( ipcPtr->_semid, ipcPtr->_ops, 1 );
  if ( rc == -1 )
  {
    fr.code(ERR_IPC_CLNT_RESPONSE);
    fr.addMessage("FAILED TO NOTIFY SERVER TO CLIENT DONE WITH RESPONSE.");
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  return true;

#pragma disable_handler
  sndres_ex:
    // IPC Server times out when client copies response data from shrdm 
    fr.code(ERR_IPC_CLNT_RESPONSE);
    fr.addMessage("FAILED TO COPY RESPONSE DATA.");
    return false;

}

/******************************************************
 * Indicate if server is being ended.
 ******************************************************/
bool IPC::beingEnded()
{
  IPC* &ipcPtr = me();
  bool ret = false;
  ret = Control::endipc();

  return ret;
}

/******************************************************
 * Server waits for client to send in request data
 *  
 * Parms:
 *  fr        -   error information
 ******************************************************/
bool IPC::serverListen(FlightRec& fr)
{
  IPC* &ipcPtr = me();
  int rc = 0;

  //---- server waits client to send request  ---------------
  ipcPtr->_ops[0].sem_num = SVR_WAIT;
  ipcPtr->_ops[0].sem_op  = -1;
  ipcPtr->_ops[0].sem_flg = 0;

  rc = semop( ipcPtr->_semid, ipcPtr->_ops, 1);
  if (rc == -1)
  {
    fr.code(ERR_IPC_SVR_LISTEN);
    fr.addMessage("SERVER FAILED TO LISTEN.");
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  return true;
}

/******************************************************
 * Server receives request data from client in shared
 * memory.
 *  
 * Parms:
 *  control   -   reference to ptr of control flag string
 *  in        -   reference to ptr of input
 *  in_size   -   reference to ptr of input size
 *  fr        -   error information
 ******************************************************/
bool IPC::receiveRequest(char* &control, char* &in, int &in_size, FlightRec& fr)
{
  IPC* &ipcPtr = me();
  Request_Header* req = (Request_Header *)ipcPtr->_shdm_p;
  if ( NULL != req )
  {
    in_size = req->in_size;
    control = &(req->data);
    in  = &(req->data) + req->ctl_size;
  }

  return true;
}

/******************************************************
 * Server writes output into shared memory
 *  
 * Parms:
 *  out        -   (output) buffer to hold output
 ******************************************************/
void IPC::sendResponse(std::string& out)
{
  IPC* &ipcPtr = me();
  Response_Header* res = (Response_Header *)ipcPtr->_shdm_p;
  if ( NULL != res )
  {
    char* outData = &(res->data);
    res->size = out.length();
    memcpy(outData, out.c_str(), res->size);
  }
}

/******************************************************
 * Server notifies client to read data out in shared
 * memory and waits client to finish reading.
 *  
 * Parms:
 *  fr        -   error information
 ******************************************************/
bool IPC::serverWaitClient2Receive(FlightRec& fr)
{
  IPC* &ipcPtr = me();
  int rc = 0;

  //---- Wake up client to receive response ---------------
  ipcPtr->_ops[0].sem_num = CLNT_WAIT;
  ipcPtr->_ops[0].sem_op  = 1;
  ipcPtr->_ops[0].sem_flg = 0;

  rc = semop( ipcPtr->_semid, ipcPtr->_ops, 1 );
  if (rc == -1)
  {
    fr.code(ERR_IPC_SVR_WAIT_CLNT);
    fr.addMessage("SERVER FAILED TO NOTIFY CLIENT TO RECEIVE.");
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  //---- Server waits client to receive response ---------------
  ipcPtr->_ops[0].sem_num = SVR_WAIT;
  ipcPtr->_ops[0].sem_op  = -1;
  ipcPtr->_ops[0].sem_flg = 0;

  rc = semop( ipcPtr->_semid, ipcPtr->_ops, 1 );
  if ( rc == -1 && errno != EAGAIN )
  {
    fr.code(ERR_IPC_SVR_WAIT_CLNT);
    fr.addMessage("SERVER FAILED TO WAIT CLIENT TO RECEIVE.");
    fr.addMessage(errno, strerror(errno));
    return false;
  }

  return true;
}

/******************************************************
 * Destroy IPC server.
 *  
 * Parms:
 *  fr        -   error information
 ******************************************************/
void IPC::destroy(FlightRec& fr)
{
  IPC* &ipcPtr = me();
  int rc = 0; 

  //----- remove IPC key path --------------------------------
  if ( ipcPtr->_server ) 
  {
    if ( ipcPtr->_ipcKey.length() > 0 )
    {
      rc = rmdir(ipcPtr->_ipcKey.c_str());
      if ( rc != 0 )
      {
        fr.code(ERR_IPC_DESTROY);
        fr.addMessage("SERVER FAILED TO REMOVE IPC KEY.");
        fr.addMessage("PATH", ipcPtr->_ipcKey.c_str());
        int err = errno;
        char* errInfo = strerror(errno);
        fr.addMessage(err, errInfo); 
      }
      else
      {
        ipcPtr->_ipcKey.clear();
      }
    }

    //---- destroy shared memory -----------------------------
    if ( NULL != ipcPtr->_shdm_p ) 
    {
      rc = shmdt(ipcPtr->_shdm_p);
      if ( rc == -1 )
      {
        fr.code(ERR_IPC_DESTROY);
        fr.addMessage("SERVER FAILED TO DETACH SHARED MEMORY.");
        fr.addMessage(errno, strerror(errno));
      }
      else
      {
        ipcPtr->_shdm_p = NULL;
      }
    }

    if ( -1 != ipcPtr->_shdmid )
    {
      struct shmid_ds _shdmid_struct;
      rc = shmctl(ipcPtr->_shdmid, IPC_RMID, &_shdmid_struct);
      if ( rc == -1 )
      {
        fr.code(ERR_IPC_DESTROY);
        fr.addMessage("SERVER FAILED TO REMOVE SHARED MEMORY.");
        fr.addMessage(errno, strerror(errno));
      }
      else
      {
        ipcPtr->_shdmid = -1;
      }
    }

    //----- destroy semphore ---------------------------------
    if ( -1 != ipcPtr->_semid )
    {
      rc = semctl( ipcPtr->_semid, 1, IPC_RMID );
      if ( rc == -1 )
      {
        fr.code(ERR_IPC_DESTROY);
        fr.addMessage("SERVER FAILED TO REMOVE SEMPHORE.");
        fr.addMessage(errno, strerror(errno));
      }
      else
      {
        ipcPtr->_semid = -1;
      }
    }
  }
  else
  {
    IPC::clientClose(fr);
  }

  //--- unregister signals ----------------
  IPC::setSignals(false, fr);
}

/******************************************************
 * (Non-class) Handles server timeout and destroy server
 *  
 * Parms:
 *  sig        -   signal
 ******************************************************/
static void sigHandler( int sig ) 
{
  struct itimerval value;
  int which = ITIMER_REAL;
  sigset_t new_set, old_set;

  //--- mask signals ------------------
  sigemptyset(&new_set);
  if ( IPC::isServer() ) 
  {
    sigaddset(&new_set, SIGALRM);
  }
  sigaddset(&new_set, SIGTERM);
  sigprocmask(SIG_BLOCK, &new_set, &old_set);
  
  //--- disable time ------------------- 
  getitimer( which, &value );
  value.it_value.tv_sec = 0;
  value.it_value.tv_usec = 0;
  setitimer( which, &value, NULL );

  //--- destroy IPC--------------
  FlightRec fr;
  if ( IPC::isServer() )
  {
    fr.addMessage("IPC SERVER TIMED OUT.");
  }
  else
  {}
  IPC::destroy(fr);

  //--- restore mask ----------------------
  sigprocmask( SIG_SETMASK, &old_set, NULL );

  exit(0);
}

/******************************************************
 * Set up signals.
 * For client, set SIGTERM
 * For server, set SIGTERM & SIGALRM
 *  
 * Parms:
 *  enable    -   enable/disable signals
 *  fr        -   error information
 ******************************************************/
bool IPC::setSignals(bool enable, FlightRec& fr)
{
  static bool registered = false;
  bool ret = true;
  int rc = 0;

  if ( enable )
  {
    if ( ! registered )
    {
      sigemptyset(&sact.sa_mask);
      sact.sa_flags = 0;
      sact.sa_handler = sigHandler;
      
      rc = sigaction(SIGTERM, &sact, &oact);
      
      if ( 0 == rc && IPC::isServer() )
      {
        rc = sigaction(SIGALRM, &sact, NULL);
      }

      if ( 0 != rc )
      {
        fr.code(ERR_IPC_SET_SIGNALS);
        fr.addMessage(errno, strerror(errno));
        ret = false;
      }
      else
      {
        registered = true;
      }
    }
  }
  else
  {
    if ( registered )
    {
      rc = sigaction( SIGTERM, &oact, NULL );
      rc = sigaction( SIGALRM, &oact, NULL );
      registered = false;
    }    
  }

  return ret; 
}

/*****************************************************
 * Check current job is IPC server.
 *****************************************************/
bool IPC::isServer()
{
  IPC* &ipcPtr = me();
  return ipcPtr->_server;
}

/******************************************************
 * Set server listening timeout to wait client 
 * connecting in 
 *  
 * Parms:
 *  enable    -   enable/disable timer
 *  seconds   -   seconds to wait if enable = true
 *  fr        -   error information
 ******************************************************/
bool IPC::setTimeout(bool enable, int seconds, FlightRec& fr)
{
  bool ret = true;
  int rc = 0;
  struct itimerval value;
  int which = ITIMER_REAL;

  //-------- set a one-time-use timer ------------
  getitimer( which, &value );
  value.it_value.tv_sec = enable ? seconds : 0;           
  value.it_value.tv_usec = 0;   
  value.it_interval.tv_sec = 0; 
  value.it_interval.tv_usec = 0;       

  rc = setitimer( which, &value, NULL );
  if ( 0 != rc )
  {
    fr.code(ERR_IPC_TIME_OUT);
    fr.addMessage(errno, strerror(errno));
    ret = false;
  }
  
  return ret;
}

/******************************************************
 * Client submits IPC server job to start IPC server.
 *  It only works one time for current job.
 * Parms:
 *  fr        -   error information
 ******************************************************/
bool IPC::startIpcServerOnce(FlightRec& fr)
{
  IPC* &ipcPtr = me();
  if ( ! ipcPtr->_server_submitted )
  {
    // QSYS/SBMJOB CMD(QSYS/CALL PGM(ISERVICE/MAIN) PARM('IPCKEY' 'CCSID')) JOB(JOBNAME) JOBD(JOBLIB/JOBD) USER(JOBUSER) 
    std::string cmd;
    cmd += "QSYS/SBMJOB CMD(QSYS/CALL PGM(";
    cmd += Control::ipcServer();
    cmd += ") PARM('";
    cmd += Control::ipcKey();
    cmd += "' '0')) JOB(";
    cmd += Control::job2Sbm();
    cmd += ") JOBD(";
    cmd += Control::jobd2Sbm();
    cmd += ") USER(";
    cmd += Control::user2Sbm();
    cmd += ") ";

    FlightRec lfr;
    int sKey = -1;
    int eKey = -1;
    getMsgKey(sKey);
    doCommand(cmd, lfr);
    getMsgKey(eKey);

    if ( ERR_OK != lfr.code() )
    {
      fr.code(lfr.code());
      fr.addMessage("CMD", cmd.c_str());
      getMessages(sKey, eKey, lfr);
      fr.addMessages(lfr.messages());
    }

    ipcPtr->_server_submitted = true;
  }

  return ERR_OK == fr.code();
}