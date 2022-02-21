#include "Program.h"
#include "INode.h"
#include "Utility.h"
#include "IAttribute.h"
#include "Pase.h"
#include "ILE.h"

/******************************************************
 * Constructor with node
 *****************************************************/
Program::Program(INode* node)
  : Runnable(node), _value(), _name(), _lib(), _func(),
    _args(), _return()
{}

/******************************************************
 * Destructor
 *****************************************************/
Program::~Program()
{}

/******************************************************
 * Initialize program
 * 
 * Parm:
 *    fr   - error information
 *****************************************************/
void Program::init(FlightRec& fr)
{
  std::string n;
  std::string v;
  IAttribute* p = NULL;
  bool forcePase = false;
  n = "PASE";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "ON" == v )
    {
      forcePase = true;
    }
    else if ( "OFF" == v )
    {
      forcePase = false;
    }
    else
    {
      fr.code(ERR_PGM_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE PASE.");
      return;
    }
  }

  n = "ERROR";
  p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    _error = OFF;
  }
  else
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "OFF" == v )
    {
      _error = OFF;
    }
    else if ( "ON" == v )
    {
      _error = ON;
    }
    else
    {
      fr.code(ERR_PGM_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE ERROR.");
      return;
    }
  }

  n = "NAME";
  p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    fr.code(ERR_PGM_INIT);
    fr.addMessage("EXPECTING ATTRIBUTE NAME.");
    return;
  }
  else
  {
    _name = p->valueString();
    trim(_name);
  }

  n = "LIB";
  p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    fr.code(ERR_PGM_INIT);
    fr.addMessage("EXPECTING ATTRIBUTE LIB.");
    return;
  }
  else
  {
    _lib = p->valueString();
    trim(_lib);
  }

  n = "FUNC";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    _func = p->valueString();
    trim(_func);
  }

  n = "PARM";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    INode* node = p->nodeValue();
    if ( NULL == node )
    {
      fr.code(ERR_PGM_INIT);
      fr.addMessage("EXPECTING ATTRIBUTE PARM.");
      return;
    }
    else
    { 
      _args.pase(forcePase);
      _args.init(node, fr);
    }
  }

  n = "RETURN";
  p = _node_p->getAttribute(n);
  if ( NULL != p )
  {
    INode* node = p->nodeValue();
    if ( NULL == node )
    {
      fr.code(ERR_PGM_INIT);
      fr.addMessage("EXPECTING ATTRIBUTE RETURN.");
      return;
    }
    else
    {
      _return.init(node, fr);
      if ( ERR_OK != fr.code() )
      {
        return;
      }
    }
  }

  if ( ! forcePase ) 
  {
    /*************************************************************
     * Determine ILE or PASE calls being used.
     * Reasons:
     *  1) ILE pgm/procedure call has limited # of arguments and
     *    only support passing arguments by pointers
     *  2) Pase pgm/procedure call are flexibile but expensive
     ************************************************************/
    if ( _args.number() > ILE::maxNumOfArgs() ||     
        !_args.allByReference() ||                  
        _func.length() > 0 && ! _return.isVoid() )
    {
      forcePase = true;
    }
  }
  _args.pase(forcePase); 
  
}

/******************************************************
 * Run the program.
 * 
 * Parm:
 *    fr - error information
 *****************************************************/
void Program::run(FlightRec& fr)
{
  QP2_ptr64_t*  paseArgv  = NULL; // ptr to array of pase ptr. 16-byte aligned.
  void* ile2PaseArgv      = NULL; // ile usable ptr to paseArgv
  void* ile2PaseStr       = NULL; // start of allocated pase memory
  char* result            = NULL; // result memory for ile result
  int   rtnValue          = 0;    // return value for pgm

  int sKey = 0;
  int eKey = 0;
  if ( ON == _error )
  {
    if ( !getMsgKey(sKey) )
    {
      _error = OFF;
    }
  }

  //----------------- call Pase ----------------------------
  if ( ERR_OK == fr.code() )
  {
    if ( 0 == _func.length() ) // _PGMCALL() for *pgm
    {
      _args.create(fr);
      if ( _args.isPase() ) // Pase
      {
        Pase::runPgm(_name.c_str(), _lib.c_str(), _args.paseArgv(), &rtnValue, fr);
      }
      else // ILE
      {
        ILE::runPgm(_name.c_str(), _lib.c_str(), _args.ileArgv(), _args.number(), &rtnValue, fr);
      }

    }
    else // _ILECALL() for *svrpgm
    {
       _args.create(fr, true);
      if ( _args.isPase() ) // Pase
      {
        Pase::runSrvgm(_name.c_str(), _lib.c_str(), _func.c_str(), 
              _args.paseArgv(), _args.signatures(), _return.type(), fr);
      }
      else // ILE
      {
        // re-use pase _ILECALL return structure
        result = new char[sizeof(ILEarglist_base)]; 
        ILE::runSrvgm(_name.c_str(), _lib.c_str(), _func.c_str(), 
              _args.ileArgv(), _args.number(), 
              (void*)&(((ILEarglist_base*)result)->result), fr);
      }
    }
    

    if ( ON == _error && ERR_OK != fr.code() )
    {
      if ( getMsgKey(eKey) )
      {
        getMessages(sKey, eKey, fr);
      }
    }
    else
    {
      if ( ERR_OK == fr.code() )
      {
        //---------------- get result back from pgm call -----------------
        _args.copyBack(fr);
        if ( ERR_OK == fr.code() && _func.length() > 0 ) // srvpgm call
        {
          if ( _args.isPase() )
          {
            _return.copyBack(_args.ilePtr(), fr);
          }
          else
          {
            _return.copyBack(result, fr);
          }
        }
        else // pgm call
        {
          _return.copyBack(rtnValue);
        }
      }
    }
    if ( NULL != result )
    {
      delete result;
    }
  }
}