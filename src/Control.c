#include <stdlib.h>
#include <stdio.h>
#include "Utility.h"
#include "Control.h"
#include <string>

#include "Pase.h"


Control* Control::_control_p = NULL;
const static int MAX_LENGTH = 1024;
/***********************************************************
 * Constructor
 ***********************************************************/
Control::Control()
  : _ipcKey(), _flags(), _sbmjob_jobd("QSYS/QSRVJOB"), _sbmjob_job("ISERVICE"), _sbmjob_user("*CURRENT"),
    _java(false), _before(0), _after(0), _endipc(false), _wait_client(300), _wait_data(5), _wait_server(5),
    _ipc_log(false), _ipc_svr("ISERVICE/MAIN"), _performance(false)
{ }

/***********************************************************
 * Destructor
 ***********************************************************/
Control::~Control()
{
  if ( NULL != _control_p )
  {
    delete _control_p;
  }
}

/********************************************************
 * (Static) Initialize global control internal instance.
 *******************************************************/
Control*& Control::me()
{
  if ( NULL == _control_p )
  {
    _control_p = new Control();
    if ( NULL == _control_p )
    {
      abort();
    }
  }
  return _control_p;
}

/********************************************************
 * (Static) Locate flag value (...)
 * Parms
 *    p - ptr to string to be processed
 *    v - (output) value string with () dropped
 * 
 * Return:
 *   -1 - ')' not found
 *    0 - no value (...) described
 *    n - for p to increment for next parse
 *******************************************************/
int Control::chopFlagValue(const char* p, std::string& v)
{
  int incr = 0;
  const char* start = p;
  const char* end = NULL;
  if ( JC(OPEN_PAREN) == *start )
  {
    ++start;
    end = start;
    while( '\0' != *end )
    {
      if ( JC(CLOSE_PAREN) == *end )
      {
        v.append(start, end - start);
        incr = end - p + 1;
        break;
      }
      ++end;
    }
    
    if ( '\0' == *end )
    {
      incr = -1;
    }
  }
  
  return incr;
}

/********************************************************
 * (Static) Initialize global controls. 
 *  Parm:
 *      control   - control flag string with null-termination.
 *      fr        - (output) error inforamtion
 *  Return:
 *      true      - successful
 *      false     - failed
 *******************************************************/
bool Control::parse(const char* control, FlightRec& fr)
{
  bool ret = true;
  Control*& ctlp = Control::me();
  ctlp->_flags.clear();
  if ( NULL != control && strlen(control) > 0 )
  {
    std::string invalidFlags;
    //----------- get rid of all the whitespaces and cvt to upppercase --------
    for ( int i = 0; i < strlen(control) && i < MAX_LENGTH; ++i )
    { 
      if ( JC(SPACE) != control[i] )
      {
        ctlp->_flags += control[i];
      }
    }
    if ( ctlp->_flags.length() > 0 )
    {
      cvt2Upper(ctlp->_flags);
    }
    else
    {
      // no control flags specified. all go default.
      return ret;
    }

    //-------------------------------------------------------------------------
    //----------- process each flag -------------------------------------------
    //!!!!! sort by alphabetic and put lengest ahead !!!!!!!!!!!!!!!!!!!!!!!!!!
    //-------------------------------------------------------------------------
    std::string& flags = ctlp->_flags;
    for (int i = 0 ; i < flags.length(); )
    {
      char* token = &(flags[i]);
      int incr = 0;
      //================= *After(ccsid) ========================================//
      if      ( 0 == memcmp(token, "*AFTER", strlen("*AFTER")) )    
      //=======================================================================//
      {
        char* p = token + strlen("*AFTER");
        incr = strlen("*AFTER");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          char* e = NULL;
          long long int lli = strtoll(v.c_str(), &e, 10);
          if ( EINVAL != errno && ERANGE != errno )
          {
            ctlp->_after = (int)lli;
          }
          else
          {
            fr.code(ERR_CONTROL_INIT);
            fr.addMessage(v.c_str(), "INVALID VALUE SPECIFIED FOR CONTROL FLAG *AFTER.");
            ret = false;
            break;
          }
        }
        else
        {
          fr.code(ERR_CONTROL_INIT);
          fr.addMessage("EXPECTING VALUE SPECIFIED FOR CONTROL FLAG *AFTER.");
          ret = false;
          break;          
        }
      }
      //================= *Before(ccsid) ========================================//
      else if ( 0 == memcmp(token, "*BEFORE", strlen("*BEFORE")) ) 
      //========================================================================//
      {
        char* p = token + strlen("*BEFORE");
        incr = strlen("*BEFORE");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          char* e = NULL;
          long long int lli = strtoll(v.c_str(), &e, 10);
          if ( EINVAL != errno && ERANGE != errno )
          {
            ctlp->_before = (int)lli;
          }
          else
          {
            fr.code(ERR_CONTROL_INIT);
            fr.addMessage(v.c_str(), "INVALID VALUE SPECIFIED FOR CONTROL FLAG *BEFORE.");
            ret = false;
            break;
          }
        }
        else
        {
          fr.code(ERR_CONTROL_INIT);
          fr.addMessage("EXPECTING VALUE SPECIFIED FOR CONTROL FLAG *BEFORE.");
          ret = false;
          break;
        }
      }
      //================= *Endipc ============================================//
      else if ( 0 == memcmp(token, "*ENDIPC", strlen("*ENDIPC")) ) 
      //======================================================================// 
      {
        ctlp->_endipc = true;
        incr = strlen("*ENDIPC");
      }
      //================= *Ipcsvr(lib/pgm) =============================================//
      else if ( 0 == memcmp(token, "*IPCSVR", strlen("*IPCSVR")) ) 
      //=======================================================================//
      {
        char* p = token + strlen("*IPCSVR");
        incr = strlen("*IPCSVR");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          ctlp->_ipc_svr = v;
        }
      }
      //================= *Ipclog =============================================//
      else if ( 0 == memcmp(token, "*IPCLOG", strlen("*IPCLOG")) ) 
      //=======================================================================//
      {
        ctlp->_ipc_log = true;
        incr = strlen("*IPCLOG");
      }
      //================= *Ipc(/path/key) ======================================//
      else if ( 0 == memcmp(token, "*IPC", strlen("*IPC")) )       
      //=======================================================================//
      {
        char* p = token + strlen("*IPC");
        incr = strlen("*IPC");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          ctlp->_ipcKey = v;
          trim(ctlp->_ipcKey);
          if ( ctlp->_ipcKey.length() == 0 )
          {
            fr.code(ERR_CONTROL_INIT);
            fr.addMessage("INVALID VALUE SPECIFIED FOR CONTROL FLAG *IPC.");
            ret = false;
            break;  
          }
        }
      }
      //================= *Java ==============================================//
      else if ( 0 == memcmp(token, "*JAVA", strlen("*JAVA")) )      
      //======================================================================//
      {
        ctlp->_java = true;
        incr = strlen("*JAVA");
        Pase::jvmPase(true);
      }
      //================= *Performance ==============================================//
      else if ( 0 == memcmp(token, "*PERFORMANCE", strlen("*PERFORMANCE")) )      
      //======================================================================//
      {
        ctlp->_performance = true;
        incr = strlen("*PERFORMANCE");
      }
      //================= *Sbmjob(job:jobd:user) ===============================//
      else if ( 0 == memcmp(token, "*SBMJOB", strlen("*SBMJOB")) )  
      //=======================================================================//
      {
        char del[2] = { JC(COLON), '\0'};
        char* p = token + strlen("*SBMJOB");
        incr = strlen("*SBMJOB");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          char* s = const_cast<char*>(&v[0]);
          char* tok = strtok(s, del);
          int ind = 1;
          while ( NULL != tok )
          {
            switch (ind)
            {
            case 1:
              ctlp->_sbmjob_job = tok;
              break;
            case 2:
              ctlp->_sbmjob_jobd = tok;
              break;
            case 3:
              ctlp->_sbmjob_user = tok;
              break;            
            default:
              break;
            }
            
            tok = strtok(NULL, del);
            ++ind;
          }
        }
        else
        {
          // allow no value specified to use default
        }
      }
      //================= *Waitclient(0) ============================================//
      else if ( 0 == memcmp(token, "*WAITCLIENT", strlen("*WAITCLIENT")) ) 
      //=======================================================================//
      {
        char* p = token + strlen("*WAITCLIENT");
        incr = strlen("*WAITCLIENT");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          ctlp->_wait_client = atoi(v.c_str());
          if ( ctlp->_wait_client < 0 )
          {
            fr.code(ERR_CONTROL_INIT);
            fr.addMessage(v.c_str(), "INVALID VALUE SPECIFIED FOR CONTROL FLAG *WAITCLIENT.");
            ret = false;
            break;  
          }
        }
        else
        {
          fr.code(ERR_CONTROL_INIT);
          fr.addMessage("EXPECTING VALUE SPECIFIED FOR CONTROL FLAG *WAITCLIENT.");
          ret = false;
          break;     
        }
      }
      //================= *Waitdata(5) ============================================//
      else if ( 0 == memcmp(token, "*WAITDATA", strlen("*WAITDATA")) ) 
      //=======================================================================//
      {
        char* p = token + strlen("*WAITDATA");
        incr = strlen("*WAITDATA");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          ctlp->_wait_data = atoi(v.c_str());
          if ( ctlp->_wait_data < 0 )
          {
            fr.code(ERR_CONTROL_INIT);
            fr.addMessage(v.c_str(), "INVALID VALUE SPECIFIED FOR CONTROL FLAG *WAITDATA.");
            ret = false;
            break;  
          }
        }
        else
        {
          fr.code(ERR_CONTROL_INIT);
          fr.addMessage("EXPECTING VALUE SPECIFIED FOR CONTROL FLAG *WAITDATA.");
          ret = false;
          break;     
        }
      }
      //================= *Waitserver(5) ======================================//
      else if ( 0 == memcmp(token, "*WAITSERVER", strlen("*WAITSERVER")) ) 
      //=======================================================================//
      {
        char* p = token + strlen("*WAITSERVER");
        incr = strlen("*WAITSERVER");
        std::string v;
        int step = chopFlagValue(p, v);
        if ( step > 0 )
        {
          incr += step;
          ctlp->_wait_server = atoi(v.c_str());
          if ( ctlp->_wait_server <= 0 )
          {
            fr.code(ERR_CONTROL_INIT);
            fr.addMessage(v.c_str(), "INVALID VALUE SPECIFIED FOR CONTROL FLAG *WAITSERVER.");
            ret = false;
            break;  
          }
        }
        else
        {
          fr.code(ERR_CONTROL_INIT);
          fr.addMessage("EXPECTING VALUE SPECIFIED FOR CONTROL FLAG *WAITSERVER.");
          ret = false;
          break;     
        }
      }
      //================= invalid =============================================//
      else
      //=======================================================================//
      {
        ret = false;
        char* nextStar = strchr(token + 1, '*');
        if ( NULL == nextStar )
        {
          incr = strlen(token);
        }
        else
        {
          incr = nextStar - token;
        }
        invalidFlags.append(token, incr);
      }      

      i += incr;
    }

    if ( ! ret )
    {
      fr.code(ERR_CONTROL_INIT);
      fr.addMessage(invalidFlags.c_str(), "INVALID CONTROL FLAG.");
    }

  }

  return ret;
}

/***************************************************************
 * Indicate IPC being used 
 ***************************************************************/
bool Control::ipc()
{
  bool ret = me()->_ipcKey.length() == 0  ? false : true;
  return ret;
}

/***************************************************************
 * Indicate IPC being ended 
 ***************************************************************/
bool Control::endipc()
{
  return me()->_endipc;
}

/***************************************************************
 * Get timeout value for waiting server to start up
 ***************************************************************/
int Control::waitServer()
{
  return me()->_wait_server;
}

/***************************************************************
 * Get timeout value for waiting client
 ***************************************************************/
int Control::waitClient()
{
  return me()->_wait_client;
}

/***************************************************************
 * Get timeout value for waiting data
 ***************************************************************/
int Control::waitData()
{
  return me()->_wait_data;
}

/***************************************************************
 * Get ipc joblog setting
 ***************************************************************/
bool Control::ipcLog()
{
  return me()->_ipc_log;
}

/***************************************************************
 * Set ipc joblog behavior
 ***************************************************************/
void Control::ipcLog(bool log)
{
  me()->_ipc_log = log;
}


/***************************************************************
 * Preprocessed control flags in job ccsid.
 ***************************************************************/
std::string& Control::flags()
{
  return me()->_flags;
}

/***************************************************************
 * CCSID of input being converted to job ccsid before parsing. 
 ***************************************************************/
int Control::before()
{
  return me()->_before;
}

/***************************************************************
 * CCSID of input being converted to job ccsid before parsing. 
 ***************************************************************/
void Control::before(int ccsid)
{
  me()->_before = ccsid;
}

/***************************************************************
 * CCSID of output being converted from job ccsid after running. 
 ***************************************************************/
int Control::after()
{
  return me()->_after;
}

/***************************************************************
 * CCSID of output being converted from job ccsid after running. 
 ***************************************************************/
void Control::after(int ccsid)
{
  me()->_after = ccsid;
}

/***************************************************************
 * Indicate if need to start jvm pase. 
 ***************************************************************/
bool Control::java()
{
  return me()->_java;
}

/***************************************************************
 * Get the ipc key path
 ***************************************************************/
const char* Control::ipcKey()
{
  return me()->_ipcKey.c_str();
}

/***************************************************************
 * Get the jobd in format lib/jobd or *USRPRF for sbmjob
 ***************************************************************/
const char* Control::jobd2Sbm()
{
  return me()->_sbmjob_jobd.c_str();
}

/***************************************************************
 * Get the job name for sbmjob
 ***************************************************************/
const char* Control::job2Sbm()
{
  return me()->_sbmjob_job.c_str();
}

/***************************************************************
 * Get the job user for sbmjob
 ***************************************************************/
const char* Control::user2Sbm()
{
  return me()->_sbmjob_user.c_str();
}

/***************************************************************
 * Get ipc server main pgm
 ***************************************************************/
const char* Control::ipcServer()
{
  return me()->_ipc_svr.c_str();
}

/***************************************************************
 * Generate performance data
 ***************************************************************/
bool Control::performance()
{
  return me()->_performance;
}

