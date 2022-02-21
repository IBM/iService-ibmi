#include "Command.h"
#include "INode.h"
#include "JNode.h"
#include "IAttribute.h"
#include "JAttribute.h"
#include "Utility.h"
#include <string>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

/******************************************************
 * Constructor
 ******************************************************/
Command::Command(INode* node)
  : Runnable(node), _exec(COMMAND), _value()
{}

/******************************************************
 * Destructor
 ******************************************************/
Command::~Command()
{}

/******************************************************
 * Initialize command 
 * 
 * Parm:
 *    fr  - error information
 ******************************************************/
void Command::init(FlightRec& fr)
{ 
  std::string n;
  std::string v;

  //---------------------- "value" ------------------------------------
  n = "VALUE";
  IAttribute* p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    fr.code(ERR_CMD_INIT);
    fr.addMessage("EXPECTING ATTRIBUTE VALUE.");
    return;
  }
  else
  {
    _value = p->valueString();
    trim(_value);
  }

  //---------------------- "exec" ------------------------------------
  n = "EXEC";
  p = _node_p->getAttribute(n);
  if ( NULL == p )
  {
    _exec = COMMAND;
  }
  else
  {
    v = p->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "CMD" == v )
    {
      _exec = COMMAND;
    }
    else if ( "SYSTEM" == v )
    {
      _exec = SYSTEM;
    }
    else if ( "REXX" == v )
    {
      _exec = REXX;
    }
    else
    {
      fr.code(ERR_CMD_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE EXEC.");
      return;
    }
  }

  //---------------------- "error" ------------------------------------
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
    else if ( "IGNORE" == v )
    {
      _error = IGNORE;
    }
    else
    {
      fr.code(ERR_CMD_INIT);
      fr.addMessage("INVALID VALUE FOR ATTRIBUTE ERROR.");
      return;
    }
  }

}

/******************************************************
 * Run command 
 * 
 * Parm:
 *    fr  - error information
 ******************************************************/
void Command::run(FlightRec& fr)
{
  switch (_exec)
  {
  case COMMAND:
    runCommand(fr);
    break;
  case SYSTEM:
    runSystem(fr);
    break;
  case REXX:
    runRexx(fr);
    break;
  default:
    fr.code(ERR_CMD_RUN);
    fr.addMessage("INVALID COMMAND TYPE.");
    break;
  }
}

/*************************************************************
 * Run CL command.
 * 
 * Parm:
 *    fr  - error information
 ************************************************************/
void Command::runCommand(FlightRec& fr)
{
  int sKey = 0;
  int eKey = 0;
  if ( ON == _error )
  {
    if ( !getMsgKey(sKey) )
    {
      _error = OFF;
    }
  }
  fr.reset();
  doCommand(_value, fr);

  if ( ON == _error && ERR_OK != fr.code() )
  {
    if ( getMsgKey(eKey) )
    {
      getMessages(sKey, eKey, fr);
    }
  }
  else if ( IGNORE == _error && ERR_OK != fr.code() )
  {
    fr.code(ERR_OK);
  }
  else
  {}
}

/*************************************************************
 * Run command with system().
 * Parm:
 *    fr  - error information
 ************************************************************/
void Command::runSystem(FlightRec& fr)
{
  int sKey = 0;
  int eKey = 0;
  if ( ON == _error )
  {
    if ( !getMsgKey(sKey) )
    {
      _error = OFF;
    }
  }

  fr.reset();
  doSystem(_value, fr);

  if ( ON == _error && ERR_OK != fr.code() )
  {
    if ( getMsgKey(eKey) )
    {
      getMessages(sKey, eKey, fr);
    }
  }
  else if ( IGNORE == _error && ERR_OK != fr.code() )
  {
    fr.code(ERR_OK);
  }
  else
  {}
}

/***********************************************************************
 * Run REXX command. 
 * Parms:
 *    fr - error information
 **********************************************************************/
void Command::runRexx(FlightRec& fr)
{
  std::string cmd;

  static bool fileCreated = false;
  if ( !fileCreated )
  {
    FlightRec fbr;
    cmd = "QSYS/DLTF FILE(QTEMP/JSONREXX)";
    doCommand(cmd, fbr);

    cmd = "QSYS/CRTSRCPF FILE(QTEMP/JSONREXX) RCDLEN(92) CCSID(*JOB) MBR(HOW)";
    doCommand(cmd, fr);
    if ( ERR_OK != fr.code() )
    {
      return;
    }

    cmd = "QSYS/GRTOBJAUT OBJ(QTEMP/JSONREXX) OBJTYPE(*FILE) USER(*PUBLIC) AUT(*ALL)";
    if ( ERR_OK != fr.code() )
    {
      return;
    }

    #pragma convert(37)
    std::string data;
    data +=    "/* STRREXPRC SRCMBR(HOW)                       */\n";
    data +=    "/* SRCFILE(QTEMP/JSONREXX)                      */\n";
    data +=    "/* PARM('RTVJOBA USRLIBL(?)'                   */\n";
    data +=    "parse arg linein\n";
    data +=    "/* create output */\n";
    data +=    "'DLTF FILE(QTEMP/OUTREXX)'\n";
    data +=    "'CRTSRCPF FILE(QTEMP/OUTREXX) MBR(OUTREXX)'\n";
    data +=    "/* authorize PUBLIC (1.6.2) */\n";
    data +=    "'GRTOBJAUT OBJ(QTEMP/OUTREXX) OBJTYPE(*FILE) USER(*PUBLIC) AUT(*ALL)'\n";
    data +=    "'CLRPFM FILE(QTEMP/OUTREXX) MBR(OUTREXX)'\n";
    data +=    "'OVRDBF FILE(STDOUT) TOFILE(QTEMP/OUTREXX) MBR(OUTREXX)'\n";
    data +=    "/* substitution chars */\n";
    data +=    "V = \"\"\n";
    data +=    "V = keysub(linein)\n";
    data +=    "line = V.dat\n";
    data +=    "/* run the command */\n";
    data +=    "RC=\"CPF????\"\n";
    data +=    "line\n";
    data +=    "if RC <> 0\n";
    data +=    "then do\n";
    data +=    "  say \"123456789012{\"\"ERRORTAG\"\":\"\"\"||RC||\"\"\"}\"\n";
    data +=    "  exit\n";
    data +=    "end\n";
    data +=    "/* output to QTEMP/OUTREXX */\n";
    data +=    "comma = \",\"\n";
    data +=    "if V.cnt > 0\n";
    data +=    "then do\n";
    data +=    "  say \"            {\"\n";
    data +=    "  do i = 1 to V.cnt\n";
    data +=    "    if i = V.cnt\n";
    data +=    "    then do\n";
    data +=    "      comma = \"\"\n";
    data +=    "    end\n";  
    data +=    "    ret = keyparm(line,i,V.i)\n";
    data +=    "  end\n";
    data +=    "  say \"            }\"\n";
    data +=    "end\n";
    data +=    "else do\n";
    data +=    "  say \"12345678901234567890\"\n";
    data +=    "end\n";
    data +=    "exit\n";
    data +=    "keyparm:\n";
    data +=    "  parse arg line,idx,data\n";
    data +=    "  /* \"&V\" */\n";
    data +=    "  vname = \"(&V.\"||idx\n";
    data +=    "  name = \"nada\"\n";
    data +=    "  /* icmd parm1(&V.1) parm2(&V.2) */\n";
    data +=    "  /*                        x     */\n";
    data +=    "  pe = pos(vname,line)\n";
    data +=    "  if pe > 0\n";
    data +=    "  then do\n";
    data +=    "    /* icmd parm1(&V.1) parm2 */\n";
    data +=    "    /*                      x */\n";
    data +=    "    all = strip(left(line,pe-1))\n";
    data +=    "    /* icmd parm1(&V.1) parm2 */\n";
    data +=    "    /*                 x      */\n";
    data +=    "    pe = LastPos(\" \",all)\n";
    data +=    "    if pe > 0\n";
    data +=    "    then do\n";
    data +=    "      /* parm2 */\n";
    data +=    "      name = strip(substr(all,pe))\n";
    data +=    "    end\n";
    data +=    "  end\n";
    data +=    "  if name <> \"nada\"\n";
    data +=    "  then do\n";
    data +=    "    pe = 40\n";
    data +=    "    goop = \"123456789012\"\n";
    data +=    "    mydata = strip(data)\n";
    data +=    "    len = length(mydata)\n";
    data +=    "    do while (len > 0)\n";
    data +=    "      out = goop||\"\"\"\"||name||\"\"\":\"\"\"||strip(left(mydata,pe))||\"\"\"\"||comma\n";
    data +=    "      say out\n";
    data +=    "      mydata = substr(mydata,pe+1)\n";
    data +=    "      len = length(mydata)\n";
    data +=    "    end\n";
    data +=    "  end\n";
    data +=    "return 0\n";
    data +=    "keysub:\n";
    data +=    "  parse arg string\n";
    data +=    "  V.cnt = 0\n";
    data +=    "  V.dat = \"\"\n";
    data +=    "  old = \"?\"\n";
    data +=    "  out= \"\"\n";
    data +=    "  new = \"&V.\"\n";
    data +=    "  i = 0\n";
    data +=    "  DO WHILE POS(old,string) > 0\n";
    data +=    "    PARSE VAR string prepart (old) string\n";
    data +=    "    i = i + 1\n";
    data +=    "    V.cnt = i\n";
    data +=    "    aleft = left(string,1)\n";
    data +=    "    V.i = \" \"\n";
    data +=    "    if aleft <> \")\"\n";
    data +=    "    then do\n";
    data +=    "      V.i = 0\n";
    data +=    "      string = substr(string,2)\n";
    data +=    "    end\n";
    data +=    "    else do\n";
    data +=    "      do h = 1 to 4096\n";
    data +=    "        V.i = V.i||\" \"\n";
    data +=    "      end\n";
    data +=    "    end\n";
    data +=    "    out=out||prepart||new||i\n";
    data +=    "  END\n";
    data +=    "  V.dat = out||string\n";
    data +=    "return V\n";
    #pragma convert(0)

    writeIfs("/qsys.lib/QTEMP.lib/JSONREXX.file/how.mbr", data, 37, fr);
    if ( ERR_OK != fr.code() )
    {
      return;
    }

    fileCreated = true;
  }
  else 
  {
    // Template file in QTEMP already created
  }

  int sKey = 0;
  int eKey = 0;
  // always rcvmsg and then no need to deal more in rexx script
  if ( !getMsgKey(sKey) )
  {
    _error = OFF;
  }

  cmd  = "QSYS/STRREXPRC SRCMBR(HOW) SRCFILE(QTEMP/JSONREXX)";
  cmd += " PARM(";
  cmd += "'";
  cmd += _value;
  cmd += "')";

  fr.reset();
  doCommand(cmd, fr);
  if ( ERR_OK != fr.code() ) // no need to retrieve error message from OUTREXX
  {
  return;
  }

  std::string result;
  std::string dataRead;
  readIfs("/qsys.lib/QTEMP.lib/outrexx.file/outrexx.mbr", dataRead, 0, fr);
  if ( ERR_OK != fr.code() )
  {
    return;
  }

  // filter \r \n out
  std::string::iterator it;
  for ( it = dataRead.begin(); it != dataRead.end(); ++it )
  {
    if (  JC(LR) != *it && JC(LF) != *it  )
    {
      result += *it;
    }
  }

  //--------- prepare RESULT -----------------------
  fr.code(ERR_OK);
  FlightRec e;
  INode* rexxOut = parseNode(result.c_str(), result.length(), e);
  if ( ERR_OK != e.code() ) 
  {
    fr.code(ERR_REXX_RUN);
    fr.messages(e.messages());
  }
  else
  {
    IAttribute* errTag = rexxOut->getAttribute("ERRORTAG");
    if ( NULL != errTag ) // Rexx run failed 
    {
      fr.code(ERR_REXX_RUN);

      if ( ON == _error ) // retrieve all messages
      {
        if ( getMsgKey(eKey) )
        {
          getMessages(sKey, eKey, fr);
        }
      }
      else // only retrieve the last one
      {
        if ( getMsgKey(eKey) )
        {
          getMessages(sKey, eKey, fr, true);
        }        
      }
    }
    if ( ERR_OK == fr.code() )
    {
      INode* retNode = _node_p->getStatusNode();
      INode* resultNode = _node_p->createNode();
      retNode->addAttribute(STS_DATA, resultNode);
      resultNode->addAttributes(rexxOut->attributes());
    }
  }

  delete rexxOut;
}