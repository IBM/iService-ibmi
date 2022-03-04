#include <string>
#include <stdlib.h>
#include <string.h>
#include "IService.h"
#include "Utility.h"
#include "FlightRec.h"
#include "INode.h"
#include "Control.h"
#include "Pase.h"
#include "Ipc.h"
#include <assert.h>

/************************************************************
 * State engine defintions:
 ***********************************************************/
const int STS_ERROR_FR              = -1;
const int STS_DONE                  = 0;
const int STS_CONTROL_INIT          = 1;
const int STS_CONTROL_SETUP         = 2;
const int STS_NODE_INIT             = 3;
const int STS_NODE_RUN              = 4;
const int STS_NODE_COPYIN           = 5;
const int STS_NODE_COPYOUT          = 6;
const int STS_IPC_CLNT_CONNECT      = 7;
const int STS_IPC_CLNT_COPYIN       = 8;
const int STS_IPC_CLNT_COPYOUT      = 9;
const int STS_IPC_CLNT_CLOSE        = 10;
const int STS_IPC_SVR_SETUP         = 11;
const int STS_IPC_SVR_LISTEN        = 12;
const int STS_IPC_SVR_COPYIN        = 13;
const int STS_IPC_SVR_COPYOUT       = 14;
const int STS_IPC_SVR_WAIT_CLNT_RCV = 15;


int runService(const char*  control,
               const char* in,
               int         in_size,
               char*       out,
               int*        out_size_p)
{
  int status = STS_CONTROL_INIT;
  bool skipOutput = false;
  bool done = false;
  int outTotalSize = 0;
  int outputSize = 0;
  FlightRec fr;
  INode* iNode = NULL;
  std::string outputStr;

  //---------------------- Validate parms ---------------------------
  if ( NULL == in || in_size <= 0 )
  {
    *out_size_p = 0;
    outTotalSize = 0;
    done = true;
  }
  else if ( NULL == out || *out_size_p <= 0 )
  {
    skipOutput = true;
  }
  else
  {
    memset(out, '\0', *out_size_p);
  }

  while ( !done )
  {
    /**************************************************************/
    if      ( STS_CONTROL_INIT == status)
    /**************************************************************/
    {
      status = STS_CONTROL_SETUP;

      if ( ! Control::parse(control, fr) )
      { 
        status = STS_ERROR_FR;
      }
      else if ( Control::ipc() )
      {
        status = STS_IPC_CLNT_CONNECT;
      }
      else
      {
        sendMsg2Joblog("CLIENT PARSED CONTROL PARM.");
      }
    }
    /**************************************************************/
    else if ( STS_IPC_CLNT_CONNECT == status )
    /**************************************************************/
    {
      status = STS_IPC_CLNT_COPYIN;
      if ( ! IPC::clientConnect(fr) )
      {
        status = STS_ERROR_FR;
      }
      else if ( ! IPC::setSignals(true, fr) ) 
      {
        sendMsg2Joblog("FAILED TO SETUP SIGNALS."); 
        status = STS_DONE;
      }
      else
      {
        sendMsg2Joblog("IPC CLIENT CONNECTED SERVER.");
      }
    }
    /**************************************************************/
    else if ( STS_IPC_CLNT_COPYIN == status )
    /**************************************************************/
    {
      status = STS_IPC_CLNT_COPYOUT;

      int rc = IPC::lockClientQ(fr);
      if ( 0 == rc ) // successful
      {
        if ( ! IPC::sendRequest(Control::flags().length(), in_size, 
                                Control::flags().c_str(), in,
                                fr ) )
        {
          status = STS_ERROR_FR;
        }       

        sendMsg2Joblog("IPC CLIENT SENT REQUEST TO SERVER.");
      }
      else if ( -1 == rc ) // failed
      {
        status = STS_ERROR_FR;
      }
      else if ( 1 == rc ) // restartable
      {
        IPC::serverSubmitted(false);
        status = STS_IPC_CLNT_CONNECT;
      }
      else
      {}
    }
    /**************************************************************/
    else if ( STS_IPC_CLNT_COPYOUT == status )
    /**************************************************************/
    {
      status = STS_IPC_CLNT_CLOSE;
      outputStr.clear();
      if ( ! IPC::receiveResponse(outputStr, fr) )
      {
        status = STS_ERROR_FR;
      }
      else 
      {
        sendMsg2Joblog("IPC CLIENT RECEIVED RESPONSED FROM SERVER.");
      }
    }
    /**************************************************************/
    else if ( STS_IPC_CLNT_CLOSE == status )
    /**************************************************************/
    {
      status = STS_NODE_COPYOUT;
      if ( ! IPC::clientClose(fr) )
      {
        status = STS_ERROR_FR;
      }
      else 
      {
        sendMsg2Joblog("IPC CLIENT CLOSED.");
      }
    }
    /**************************************************************/
    else if ( STS_CONTROL_SETUP == status )
    /**************************************************************/
    {
      status = STS_NODE_COPYIN;

      if ( Control::java() )
      {
        Pase::start(fr);
        if ( ERR_OK != fr.code() )
        {
          status = STS_ERROR_FR;
        }
        else 
        {
          sendMsg2Joblog("CLIENT STARTED PASE.");
        }
      }
    }
    /**************************************************************/
    else if ( STS_NODE_COPYIN == status )
    /**************************************************************/
    {
      status = STS_NODE_INIT;
    }
    /**************************************************************/
    else if ( STS_NODE_INIT == status )
    /**************************************************************/
    {
      status = STS_NODE_RUN;

      iNode = parseNode(in, in_size, fr, Control::before()); 
      if ( NULL == iNode ) 
      { 
        status = STS_ERROR_FR;
      }
      else
      { 
        sendMsg2Joblog("CLIENT PARSED INPUT.");
      }
    }
    /**************************************************************/
    else if ( STS_NODE_RUN == status )
    /**************************************************************/
    {
      status = STS_NODE_COPYOUT;
      outputStr.clear();
      runNode(iNode, outputStr, fr, Control::after());

      sendMsg2Joblog("IPC CLIENT EXECUTED INPUT INSTRUCTIONS.");
    }
    /**************************************************************/
    else if ( STS_NODE_COPYOUT == status )
    /**************************************************************/
    {
      status = STS_DONE;
      if ( ! skipOutput )
      {
        if ( outputStr.length() < *out_size_p )
        {
          outputSize = outputStr.length();
          *out_size_p = outputSize;
          outTotalSize = outputSize;
        }
        else
        {
          outputSize = *out_size_p;
          outTotalSize = outputStr.length();
        }
        memcpy(out, outputStr.c_str(), outputSize);
      }
      else
      {
        outTotalSize = outputStr.length();
      }

      sendMsg2Joblog("CLIENT SET OUTPUT BACK TO CALLER.");

    }
    /**************************************************************/
    else if ( STS_ERROR_FR == status )
    /**************************************************************/
    {
      status = STS_DONE;
      
      if ( ERR_OK != fr.code() )
      {
        std::string msgs;
        fr.toString(msgs, Control::after());
        if ( msgs.length() < *out_size_p )
        {
          outputSize = msgs.length();
          *out_size_p = outputSize;
          outTotalSize = outputSize;
        }
        else
        {
          outputSize = *out_size_p;
          outTotalSize = msgs.length();
        }
        memcpy(out, msgs.c_str(), outputSize); 
      }

      sendMsg2Joblog("CLIENT SET ERROR BACK TO CALLER.");
    }
    /**************************************************************/
    else if ( STS_DONE == status )
    /**************************************************************/
    { 
      IPC::clientClose(fr);  
      done = true;

      sendMsg2Joblog("CLIENT CLOSED.");
    }
    /**************************************************************/
    else // Unknown status
    /**************************************************************/
    {
      assert(false);
    }
  }

  delete iNode;
  iNode = NULL;

  return outTotalSize;
}


int runService2(const char* control,
                int         ccsid,
                const char* in,
                int         in_size,
                char*       out,
                int*        out_size_p)
{
  int availableBytes = 0;
  char* ctl = NULL;
  if ( 0 != ccsid )
  {
    size_t iLeft   = strlen(control);
    size_t oLen   = iLeft * 4;
    size_t oLeft  = oLen;
    char* pIn     = const_cast<char*>(control);
    ctl           = new char[oLen];
    if ( NULL == ctl )
    {
      abort();
    }
    char* pOut2 = ctl;

    int rc = convertCcsid(ccsid, 0, &pIn, &iLeft, &pOut2, &oLeft);
    if ( 0 != rc )
    {
      *out = '\0';
      *out_size_p = 0;
      availableBytes = 0;
    }

    availableBytes = runService(ctl, in, in_size, out, out_size_p);
    delete[] ctl;
  }
  else
  {
    availableBytes = runService(control, in, in_size, out, out_size_p);
  }

  return availableBytes;
}

int runServer(const char* ipc_key, int ccsid)
{
  int status    = STS_DONE;
  bool done     = false;
  INode* iNode  = NULL;
  char* control = NULL;
  char* in      = NULL;
  int in_size   = 0;
  std::string outputStr;
  FlightRec fr;

  //---------------------- Validate parms ---------------------------
  if ( NULL == ipc_key || strlen(ipc_key) == 0 )
  {
    return 1;
  }

  //---------------------- run server -------------------------------
  sendMsg2Joblog("IPC SERVER IS STARTING.");
  fr.joblog(true); // Send msg to joblog for starting phase.
  status = STS_IPC_SVR_SETUP;
  while ( !done )
  {
    /**************************************************************/
    if      ( STS_IPC_SVR_SETUP == status)
    /**************************************************************/
    {
      status = STS_IPC_SVR_LISTEN;
      
      if ( ! IPC::setupIpcServer(ipc_key, ccsid, fr) )
      {
        sendMsg2Joblog("FAILED TO SETUP IPC SERVER."); 
        status = STS_DONE;
      }
      else if ( ! IPC::setSignals(true, fr) ) 
      {
        sendMsg2Joblog("FAILED TO SETUP SIGNALS."); 
        status = STS_DONE;
      }
      else
      {
        sendMsg2Joblog("IPC SERVER IS SET UP.");
      }
    }
    /**************************************************************/
    else if ( STS_IPC_SVR_LISTEN == status)
    /**************************************************************/
    {
      status = STS_IPC_SVR_COPYIN;

      // Refresh fr for next request coming in.
      fr.code(ERR_OK);
      fr.clearMessages();
      // Free allocated memory for previous request
      delete iNode;
      iNode = NULL;

      if ( IPC::beingEnded() )
      {
        status = STS_DONE;
        //------- disable timer and exit ignoring timer errors ------
        if ( ! IPC::setTimeout(false, 0, fr) )
        {
          sendMsg2Joblog("FAILED TO DISABLE TIMEOUT FOR IPC SERVER BEFORE BEING ENDED.");
          status = STS_DONE;
        }
      }
      else 
      { 
        //-------- enable timer before listening ------------------
        if ( ! IPC::setTimeout(true, Control::waitClient(),fr) )
        {
          fr.addMessage("FAILED TO ENABLE TIMEOUT BEFORE LISTENING.");
          status = STS_DONE;
        }
        else 
        {
          if ( ! IPC::serverListen(fr) )
          {
            sendMsg2Joblog("FAILED TO LISTEN FOR CLIENTS.");
            status = STS_DONE;
          }
          else
          {
            sendMsg2Joblog("IPC SERVER IS LISTENING.");
          }
          //------- disable timer before processing or going error ------
          if ( ! IPC::setTimeout(false, 0, fr) )
          {
            sendMsg2Joblog("FAILED TO DISABLE TIMEOUT AFTER LISTENED.");
            status = STS_DONE;
          }          
        }
      }
    }
    /**************************************************************/
    else if ( STS_IPC_SVR_COPYIN == status)
    /**************************************************************/
    {
      status = STS_CONTROL_INIT;

      if ( ! IPC::receiveRequest(control, in, in_size, fr) )
      {
        sendMsg2Joblog("FAILED TO RECEIVE REQUEST.");
        status = STS_DONE;
      }
      else
      { 
        sendMsg2Joblog("IPC SERVER RECEIVED CLIENT REQUEST.");
      }
    }
    /**************************************************************/
    else if ( STS_CONTROL_INIT == status)
    /**************************************************************/
    {
      status = STS_CONTROL_SETUP;

      if ( ! Control::parse(control, fr) )
      { 
        sendMsg2Joblog("FAILED TO PARSE CONTROL FLAGS.");
        status = STS_ERROR_FR;
      }
      else
      {
        sendMsg2Joblog("IPC SERVER PARSED CONTROL PARM.");
      }
    }
    /**************************************************************/
    else if ( STS_CONTROL_SETUP == status )
    /**************************************************************/
    {
      status = STS_NODE_COPYIN;

      if ( Control::java() )
      {
        Pase::start(fr);
        if ( ERR_OK != fr.code() )
        {
          sendMsg2Joblog("FAILED TO START JAVA.");
          status = STS_DONE;
        }
        else 
        {
          sendMsg2Joblog("IPC SERVER STARTED PASE.");
        }
      }

      fr.joblog(Control::ipcLog()); // set joblog as *ipclog required
    }
    /**************************************************************/
    else if ( STS_NODE_COPYIN == status )
    /**************************************************************/
    {
      status = STS_NODE_INIT;
      // process input in shared memory directly
    }
    /**************************************************************/
    else if ( STS_NODE_INIT == status )
    /**************************************************************/
    {
      status = STS_NODE_RUN;

      iNode = parseNode(in, in_size, fr, Control::before()); 
      if ( NULL == iNode ) 
      { 
        sendMsg2Joblog("FAILED TO INITIALIZE NODE.");
        status = STS_ERROR_FR;
      }
      else
      { 
        sendMsg2Joblog("IPC SERVER PARSED INPUT.");
      }
    }
    /**************************************************************/
    else if ( STS_NODE_RUN == status )
    /**************************************************************/
    {
      status = STS_IPC_SVR_COPYOUT;
      outputStr.clear();
      runNode(iNode, outputStr, fr, Control::after());

      sendMsg2Joblog("IPC SERVER EXECUTED PARSED INSTRUCTIONS.");
    }
    /**************************************************************/
    else if ( STS_IPC_SVR_COPYOUT == status )
    /**************************************************************/
    {
      status = STS_IPC_SVR_WAIT_CLNT_RCV;
      IPC::sendResponse(outputStr);

      sendMsg2Joblog("IPC SERVER SENT RESPONSE BACK.");
    }
    /**************************************************************/
    else if ( STS_IPC_SVR_WAIT_CLNT_RCV == status )
    /**************************************************************/
    {
      status = STS_IPC_SVR_LISTEN;

      //-------- enable timer before waiting client to copy response ------------------
      if ( ! IPC::setTimeout(true, Control::waitData(), fr) )
      {
        fr.addMessage("FAILED TO ENABLE TIMEOUT BEFORE WAITING CLIENT TO RECEIVE RESPONSE.");
        status = STS_DONE;
      }
      else
      {
        if ( ! IPC::serverWaitClient2Receive(fr) )
        {
          sendMsg2Joblog("FAILED TO WAIT CLIENT TO RECEIVE REPONSE.");
          status = STS_DONE;
        }
        else 
        {
          sendMsg2Joblog("IPC SERVER IS AWAITING FOR CLIENTS.");
        }

        //-------- disable timer after waiting client to copy response ------------------
        if ( ! IPC::setTimeout(false, 0, fr) )
        {
          fr.addMessage("FAILED TO DISABLE TIMEOUT AFTER WAITING CLIENT TO RECEIVE RESPONSE.");
          status = STS_DONE;
        }
      }

    }
    /**************************************************************/
    else if ( STS_ERROR_FR == status )
    /**************************************************************/
    {
      status = STS_IPC_SVR_COPYOUT;
      
      if ( ERR_OK == fr.code() )
      {
        status = STS_DONE;
      }
      else 
      {
        outputStr.clear();
        fr.toString(outputStr, Control::after());
      }

      sendMsg2Joblog("IPC SERVER SENT ERROR BACK TO CLIENT.");
    }
    /**************************************************************/
    else if ( STS_DONE == status )
    /**************************************************************/
    {    
      IPC::destroy(fr);  
      done = true;

      delete iNode;
      iNode = NULL;

      sendMsg2Joblog("IPC SERVER ENDED.");
    }
    /**************************************************************/
    else // Unknown status
    /**************************************************************/
    {
      assert(false);
    }
  }

  return 0;
}