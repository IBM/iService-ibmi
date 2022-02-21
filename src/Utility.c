#include "Utility.h"
#include "Common.h"
#include "INode.h"
#include "JNode.h"
#include <qtqiconv.h>
#include <qlgcase.h>
#include <qusec.h>
#include <assert.h>
#include <qcapcmd.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <qmhrcvpm.h>
#include <qmhrmvpm.h>
#include <qmhsndpm.h>
#include <qmhrtvm.h>
#include <sys/types.h>

/*****************************************************************
 * Special chars that need to convert to job ccsid in runtime.
 ****************************************************************/
#pragma convert(37)
static char jc_chars[] = " ,:\\\'\"{}[]()<>/%\n\r-*=;";
#pragma convert(0)


#pragma pack(1)
typedef struct RTVM0100_Receiver_t                            
{ 
  Qmh_Rtvm_RTVM0100_t Header;                                                                                               
  char Message[512];
  char Message_Help[1024];
} RTVM0100_Receiver; 

typedef struct RCVM0200_Receiver_t
{
  Qmh_Rcvpm_RCVM0200_t Header;
  char Message_Rcvr[1024];
} RCVM0200_Receiver;

#pragma pack()       

/******************************************************************
 * Based on the 1st non-whitespace char, call relative parser to 
 * parse input buffer.
 * Parms:
 *    in    - input buffer to parse in job ccsid
 *    size  - size of the input buffer
 *    fr    - (output) error information
 *    ccsid - ccsid of input. default is job ccsid.
 * Return:
 *    INode* -  pointer to parsed node root if successful
 *              pointer to the error node if failed
 *              NULL if unknown format
 * Note: Caller needs to destroy INode* pointer.
 ******************************************************************/
INode* parseNode(const char* input, int input_size, FlightRec& fr, int ccsid )
{
  INode* inode      = NULL;
  const char* where = NULL;

  char* in  = const_cast<char*>(input);
  int size  = input_size;

  //---------------------- Convert input to job CCSID ---------------------
  size_t iLen = 0;
  size_t oLen = 0;
  char* pIn   = NULL;
  char* pOut  = NULL;
  if ( 0 != ccsid ) // input not job ccsid
  {
    iLen = size;
    oLen = size * 4;
    pIn   = in;
    pOut  = new char[oLen];
    if ( NULL == pOut )
    {
      abort();
    }

    char* pIn2 = pIn;
    char* pOut2 = pOut;
    size_t iLen2 = iLen;
    size_t oLen2 = oLen;
    int rc = convertCcsid(ccsid, 0, &pIn2, &iLen2, &pOut2, &oLen2);
    if ( rc != 0 )
    {
      fr.code(rc);
      fr.addMessage(ccsid, "FAILED TO CONVERT FROM THE GIVEN CCSID.");
      delete[] pOut;
      pOut == NULL;

      return inode;
    }  
    else
    {
      in = pOut;
      oLen = oLen - oLen2;
      size = oLen;
    }    
  }

  //---------------------- Parse input ------------------------------
  where = in;
  for ( ; where < in+size && (JC(SPACE) == *where || JC(LF) == *where); ++where) {}
  if ( where < in+size )
  {
    char s = *where;
    where = in + size -1; 
    for ( ; where > in && (JC(SPACE) == *where || JC(LF) == *where) ; --where) {}
    if ( s == JC(LESS))
    { 
      // INode* inode = new XNode();
      // if ( NULL == inode)
      // {
      //   abort();
      // }

      if ( JC(GREAT) == *where ) 
      {
        // FUTURE: XNode for xml
      }
      else
      {
        fr.code(ERR_INPUT_INVALID);
        fr.addMessage("EXPECING LAST VALID CHAR IS >.");
        inode->addAttributes(fr.messages());
      }
    }
    else if ( s == JC(OPEN_SQUARE) || s == JC(OPEN_CURLY) )
    {
      inode = new JNode();
      if ( NULL == inode ) 
      {
        abort();
      }

      if ( JC(OPEN_SQUARE) == s && JC(CLOSE_SQUARE) == *where || 
           JC(OPEN_CURLY) == s && JC(CLOSE_CURLY) == *where ) 
      {
        const char* stop = inode->parse(in, in+size-1, fr);
        if ( ERR_OK != fr.code() )
        {
          fr.addMessage(static_cast<int>(stop-in) + 1, "POSITION PARSING STOPPED AT."); 
          delete inode;
          // compose error node for return
          inode = new JNode();
          if ( NULL == inode )
          {
            abort();
          }
          inode->addAttributes(fr.messages());
        }
      }
      else // compose error node for return
      {
        fr.code(ERR_INPUT_INVALID);
        fr.addMessage("EXPECTING LAST VALID CHAR IS ] OR }.");
        inode->addAttributes(fr.messages());
      }
    }
    else
    {
      fr.code(ERR_INPUT_INVALID);
      fr.addMessage("UNKONW FORMAT");
    }
  }
  else
  {
    fr.code(ERR_INPUT_INVALID);
    fr.addMessage("INCOMPLETE");
  }

  if ( NULL != pOut )
  {
    delete[] pOut;
  }

  return inode;
}

/******************************************************************
 * Starting from the root node, traverse the tree and invoke each
 * runnable node. 
 * Parms:
 *    node    - pointer to the root node
 *    output  - (output) output receiver
 *    fr      - (output) error information
 *    ccsid   - ccsid of output. default is job ccsid.
 *****************************************************************/
void runNode(INode* node, std::string& output, FlightRec& fr, int ccsid)
{
  assert( NULL != node );

  //------------ run node ---------------------------------------------
  if ( ERR_OK == fr.code() )
  {
    node->run(fr);
  }

  //------------ Add gloabl status to root node -----------------------
  if ( CONTAINER == node->type() )
  {
    INode* stsNode = node->createNode();
    if ( NULL == stsNode )
    {
      abort();
    }
    node->addNode(stsNode, true);
    stsNode->addAttribute(STS_STATUS, fr.code(), true);
  }
  else if ( NORMAL == node->type() )
  {
    node->addAttribute(STS_STATUS, fr.code(), true);
  }
  else
  {}
  
  //------------ Get node ouput ---------------------------------------
  node->output(output);

  //------------- convert to specified ccsid --------------------------
  if ( 0 != ccsid ) // input not job ccsid
  {
    size_t iLeft   = output.length();
    size_t oLen   = iLeft * 4;
    size_t oLeft  = oLen;
    char* pIn     = const_cast<char*>(output.c_str());
    char* pOut    = new char[oLen];
    if ( NULL == pOut )
    {
      abort();
    }
    char* pOut2 = pOut;

    int rc = convertCcsid(0, ccsid, &pIn, &iLeft, &pOut2, &oLeft);
    if ( 0 == rc )
    {
      output.clear();
      output.append(pOut, oLen - oLeft);
    }  

    delete[] pOut;  
  } 

}

/*****************************************************************
 * Get special char in job ccsid.
 * Parms:
 *    c - CCSID 37 signle byte char.
 * Return:
 *    Job CCSID char.
 *****************************************************************/
char JC(jc_index i)
{
  assert( i>=0 && i < END);
  
  char ret = 0xFF;
  static char* jc = NULL;
  if ( NULL == jc )
  {
    static char jcs[END] = {'\0'};
    jc = &jcs[0];
    char* out = jc;
    char* in = jc_chars;
    size_t inLen = (size_t) END;
    size_t outLen = (size_t) END;
    int rc = convertCcsid(37, 0, &in, &inLen, &out, &outLen);
    assert(0 == rc);
  }
  return jc[i];
}

/*********************************************************
 * Wrapper of iConv().
 * Return:
 *    errno - if iConv() returns -1
 *    rc    - if iConv() returns NOT -1
 *    0     - if iConv() successful
 *********************************************************/
int convertCcsid(int fccsid, int tccsid, char** ppIn, size_t* pInLen, 
                    char** ppOut, size_t* pOutLen)
{
  int ret = 0;
	QtqCode_T FromDesc, ToDesc;
	iconv_t XlateDesc;
	FromDesc.CCSID = fccsid;
	ToDesc.CCSID = tccsid;
	FromDesc.cnv_alternative = 0;
	ToDesc.cnv_alternative = 0;
	FromDesc.subs_alternative = 0;
	ToDesc.subs_alternative = 0;
	FromDesc.shift_alternative = 1;
	ToDesc.shift_alternative = 1;
	FromDesc.length_option = 0;
	ToDesc.length_option = 0;
	FromDesc.mx_error_option = 0;
	ToDesc.mx_error_option = 0;
	memset(FromDesc.reserved, 0, sizeof(FromDesc.reserved));
	memset(ToDesc.reserved, 0, sizeof(ToDesc.reserved));

	XlateDesc = QtqIconvOpen(&ToDesc, &FromDesc);
	if ( -1 != XlateDesc.return_value )
  {
    int rc = iconv(XlateDesc, ppIn, pInLen, ppOut, pOutLen);
    if ( -1 == rc ) 
    {
      ret = errno;
    }
    else
    {
      ret = rc;
    }
    iconv_close(XlateDesc);
  }
  else
  {
    ret = -1;
  }

  return ret;
}


/*******************************************************
 * Convert to upper case with QlgConvertCase().
 *******************************************************/
void cvt2Upper(std::string& s)
{
  if ( s.length() == 0 )
  {
    return;
  }
  Qus_EC_t *pErrBuf;
  long len = s.length();
  const int BUF_SIZE = 256;
  char errBuf[BUF_SIZE];
  Qlg_CCSID_ReqCtlBlk_T parms;
  parms.Type_of_Request = 1;
  parms.CCSID_of_Input_Data = 0;
  parms.Case_Request = 0;
  memset(parms.Reserved, 0, sizeof(parms.Reserved));
  pErrBuf = (Qus_EC_t*) &errBuf[0];
  pErrBuf->Bytes_Provided = BUF_SIZE;
  pErrBuf->Bytes_Available = 0;
  char* in  = new char[len+1];
  char* out = new char[len+1];
  if ( NULL == in || NULL == out )
  {
    abort();
  }
  else
  {
    memset(in, '\0', len+1);
    memset(out, '\0', len+1);
    memcpy(in, s.c_str(), len);
  }

  QlgConvertCase((char*) &parms, in, out, &len,
                   (char*) pErrBuf);
  if ( pErrBuf-> Bytes_Available > 0 )
  {
    abort();
  }
  else 
  {
    s.clear();
    s.append(out, len);
  }

  delete[] in;
  delete[] out;
}

/************************************************
 * Get rid of leading and trailing whitespaces.
 ************************************************/
void trim(std::string& s)
{
  // trim leading whitespace
  std::string::iterator it = s.begin();
  for ( ; it != s.end(); ++it )
  {
    if ( *it != JC(SPACE))
    {
      break;
    }
  }
  s.erase(s.begin(), it);

  // trim trailing whitespace
  std::size_t pos = s.length()-1;
  for ( ; pos >= 0 ; --pos ) 
  {
    if ( JC(SPACE) == s[pos] )
    {
      s.erase(pos, 1);
    }
    else
    {
      break;
    }
  }
}

/************************************************
 * Get rid of trailing char.
 ************************************************/
void trimTrailing(std::string& s, char c)
{
  // trim trailing whitespace
  std::size_t pos = s.length()-1;
  for ( ; pos >= 0 ; --pos ) 
  {
    if ( c == s[pos] )
    {
      s.erase(pos, 1);
    }
    else
    {
      break;
    }
  }
}

/************************************************
 * Replace the trailing char c with '\0'
 ************************************************/
void trimTrailing(char* s, int len, char c)
{
  assert( NULL != s && len > 0 );
  for ( int pos = len-1; pos >= 0 ; --pos ) 
  {
    if ( c == s[pos] )
    {
      s[pos] = '\0';
    }
    else
    {
      break;
    }
  }
}


/**********************************************************
 * Convert hex chars to binary.
 *********************************************************/
bool hexChar2Bin(std::string& s)
{
  bool ret = true;
  size_t iLen = s.length();
  char* pIn = const_cast<char*>(s.c_str());

  if ( 0 == iLen || iLen % 2 != 0 )
  {
    ret = false;
  }
  else
  {
    std::string out;
    int ind = 0; 
    while ( ind < iLen/2 )
    {
      char s[3] = {'\0'};
      memcpy(s, pIn, 2);
      char* e = NULL;
      char c = (char) strtol(s, &e, 16);
      if ( EINVAL != errno && ERANGE != errno )
      {
        out += c;
      }
      else
      {
        ret = false;
        break;
      }
      ++ind;
      pIn += 2;
    }

    // copy result back
    s = out;
  }
  
  return ret;
}

/**********************************************************
 * Convert binary steam to hex chars.
 *********************************************************/
bool bin2HexChar(std::string& s)
{
  bool ret = false;

  std::string out;
  for ( int ind = 0; ind < s.length(); ++ind)
  {
    ret = true;
    char* pc = &s[ind];
    char buf[3] = {'\0'};
    sprintf(buf, "%02X", *pc);
    out.append(buf, 2);
  }

  if ( ret )
  {
    s = out;
  }

  return ret;
}

/******************************************************************
 * Write data with IFS APIs.
 * Parms:
 *    file        - file name
 *    data        - data to write
 *    fromCcsid   - ccsid of data being converted to file ccsid
 *    fr         - (output) error information
 *****************************************************************/
void writeIfs(char* file, std::string& data, int fromCcsid, FlightRec& fr)
{
  int fd = open(file, O_WRONLY|O_CREAT|O_TRUNC|O_TEXTDATA|O_CCSID,
                S_IRUSR | S_IWUSR | S_IXUSR, fromCcsid);
  if ( fd < 0 )
  {
    fr.code(ERR_WRITE_IFS);
    fr.addMessage( errno, strerror(errno) );
    return;
  }
  int writeN = write(fd, const_cast<char*>(data.c_str()), data.length());
  if ( writeN < 0 )
  {
    fr.code(ERR_WRITE_IFS);
    fr.addMessage( errno, strerror(errno) );
    close(fd);
    return;
  }

  close(fd);
}

/**********************************************************************
 * Read data with IFS APIs.
 * Parms:
 *    file    - file name
 *    data    - (output) data read from file
 *    toCcsid - ccsid of data being converted against file ccsid
 *    fr     - (output) error information
 **********************************************************************/
void readIfs(const char* file, std::string& data, int toCcsid, FlightRec& fr)
{
  int fd = open(file, O_RDONLY|O_TEXTDATA|O_CCSID, S_IRUSR | S_IWUSR | S_IXUSR, toCcsid);  
  if ( fd < 0 )
  {
    fr.code(ERR_READ_IFS);
    fr.addMessage(errno, strerror(errno));
    return;
  }

  const int BUF_SIZE = 256;
  char buff[BUF_SIZE] = {0};
  bool done = false;
  while ( !done )
  {
    int readN = read(fd, buff, BUF_SIZE);
    if ( -1 == readN ) // read error
    {
      fr.code(ERR_READ_IFS);
      fr.addMessage(errno, strerror(errno));
      close(fd);
      return;
    }
    else if ( readN < BUF_SIZE ) // read eof
    {
      done = true;
    }
    else // read ok
    {}
    
    data.append(buff, readN);
  }

  close(fd);
}

/**************************************************
 * Remove an IFS file.
 * ***********************************************/
int removeIfs(const char* file)
{
  return unlink(file);
}

/*************************************************************
 * Run the command with QCAPCMD
 * 
 * Parm:
 *    cmd   -  command string
 *    fr    -  error info
 *************************************************************/
void doCommand(std::string& cmd, FlightRec& fr)
{
  Error_Code ec;
  memset((char*)&ec, '\0', sizeof(ec));
  ec.Bytes_Provided = sizeof(ec);
  ec.Bytes_Available = 0;

  Qca_PCMD_CPOP0100_t cpop0100;
  memset(&cpop0100, 0x00, sizeof(Qca_PCMD_CPOP0100_t));
  cpop0100.Command_Process_Type  = 0;
  cpop0100.DBCS_Data_Handling    = '1';
  cpop0100.Prompter_Action       = '0';
  cpop0100.Command_String_Syntax = '0';
  
  QCAPCMD(const_cast<char*>(cmd.c_str()), cmd.length(),
          &cpop0100, sizeof(Qca_PCMD_CPOP0100_t), 
          "CPOP0100", NULL, 0, NULL, &ec);

  if ( ec.Bytes_Available > 0 ) 
  {
    fr.code(ERR_CMD_RUN);
    std::string text;
    getMessage(text, ec.Exception_Data, ec.Exception_Id);
    std::string title(ec.Exception_Id, 7);
    fr.addMessage(title, text);
  }
  else
  {
    fr.code(ERR_OK);
  }
}

/*************************************************************
 * Run the command with system()
 * 
 * Parm:
 *    fr    -  error info
 *************************************************************/
void doSystem(std::string& cmd, FlightRec& fr)
{
  int rc = system(const_cast<char*>(cmd.c_str()));
  if ( rc == 0 ) // succeed
  {
    fr.code(ERR_OK);
  }
  else if ( rc < 0 ) // null cmd
  {
    fr.code(ERR_SYSTEM_RUN);
    fr.addMessage("NULL COMMAND PASSED IN.");
  }
  else // fail
  {
    fr.code(ERR_SYSTEM_RUN);
    fr.addMessage(_EXCP_MSGID);
  }
}

/***********************************************************************
 * Get message with replacement data injected.
 * Parms:
 *    msg         - (output) complete message returned
 *    msgReplData - replacement data being injected into message
 *                  endded with null termitator
 *    msgId       - message ID (7 chars)
 *    msgFile     - message file and library (20 chars ended with null terminator)
 * Return:
 *    true    - successful
 *    false   - failed
 **********************************************************************/
bool getMessage(std::string& msg, const char* msgReplData, 
                const char* msgId, const char* msgFile, bool helpOn)
{
  bool ret = true;
  Qus_EC_t e;
  RTVM0100_Receiver rcv;
  memset((char*)&rcv, '\0', sizeof(RTVM0100_Receiver));
  e.Bytes_Provided = sizeof(RTVM0100_Receiver);

  QMHRTVM((char*)&rcv, sizeof(RTVM0100_Receiver), "RTVM0100", const_cast<char*>(msgId),
          const_cast<char*>(msgFile), const_cast<char*>(msgReplData),
          strlen(msgReplData), "*YES      ", "*NO       ",
          &e);
  assert( e.Bytes_Available == 0 );
  if ( 0 == e.Bytes_Available )
  {
    ret = true;
    msg.append(rcv.Message, rcv.Header.Length_Message_Returned);
    if ( helpOn )
    {
      msg.append(JC(SPACE), 1);
      msg.append(rcv.Message_Help, rcv.Header.Length_Help_Returned);
    }
  }
  else
  {
    ret = false;
  }
  
  return ret;
}

/**************************************************************
 * Aquire a message key to locate current position in the 
 * message queue.
 * Parms:
 *    msgKey  - reference of output
 * Return:
 *    true  - successful
 *    false - failed 
 *************************************************************/
bool getMsgKey(int& msgKey)
{
  bool ret = true;
  Qus_EC_t  ec;
  ec.Bytes_Provided = sizeof(Qus_EC_t);

  //------ Send test key to message queue -----------------------------
  QMHSNDPM( "       ",              /* Message ID                  */
            "                    ", /* Message file name          */
            "iService",             /* Message text                */
            8,                      /* Length of message text      */
            "*INFO     ",           /* Message type                */
            "*          ",          /* Call stack entry            */
            0,                      /* Call stack counter          */
            (char *)(&msgKey),      /* Message key - must be cast  */
                                    /* to char *                   */
            &ec);                   /* FlightRec code                  */
  if ( ec.Bytes_Available > 0 )
  {
    ret = false;
  }
  else
  {
    QMHRMVPM( "*          ",          /* Call stack entry                     */
              0,                      /* Call stack counter                   */
              (char *)(&msgKey),      /* Message key - must be cast           */
                                      /* to char *                            */
              "*BYKEY    ",           /* Messages to remove option            */
              &ec);                   /* FlightRec code structure        */
  }
  
  return ret;
}

/***********************************************************************
 * Get messages in a range.
 * Parms:
 *    startKey    - starting key
 *    endKey      - ending key
 *    fr          - (output) error information
 *    lastMsgOnly - only rcv the last message
 **********************************************************************/
void getMessages(int startKey, int endKey, FlightRec& fr, bool lastMsgOnly)
{
  Qus_EC_t ec;
  RCVM0200_Receiver rcvr;

  ec.Bytes_Provided = sizeof(Qus_EC_t);
  for ( int msgKey = endKey - 1; msgKey >= startKey; --msgKey )
  {
    memset((char*)&rcvr, '\0', sizeof(RCVM0200_Receiver));

    QMHRCVPM( &rcvr,              /* Message information         */
              sizeof(rcvr),       /* Length of msg information   */
              "RCVM0200",         /* Format name                 */
              "*          ",      /* Call stack entry            */
              0,                  /* Call stack counter          */
              "*ANY      ",       /* Message type                */
              (char *)(&msgKey),  /* Message key cast to char *  */
              0,                  /* Wait time                   */
              "*SAME     ",       /* Message action              */
              &ec);               /* FlightRec code structure        */
    if ( (ec.Bytes_Available == 0) && (rcvr.Header.Bytes_Available > 0) 
          && rcvr.Header.Length_Message_Returned > 0 )
    {
      std::string msgId;
      std::string msgText;
      msgId.append(rcvr.Header.Message_Id, 7);
      msgText.append(rcvr.Message_Rcvr + rcvr.Header.Length_Data_Returned, 
                      rcvr.Header.Length_Message_Returned);
      fr.addMessage(msgId, msgText);

      if ( lastMsgOnly ) 
      {
        break; // Only get the last message
      }
    }
    else 
    {
      // Ignore and try next
    }
  }
} 

/***********************************************************************
 * Send messsage to joblog.
 * Parms:
 *    msg    - message text
 *    escape - send escape message. false by default.
 **********************************************************************/
void sendMsg2Joblog(const char* msg, bool escape)
{
  Qus_EC_t  ec;
  ec.Bytes_Provided = sizeof(Qus_EC_t);
  int msgKey = 0;
  char msgType[10] = {JC(SPACE)};
  if ( ! escape )
  {
    memcpy(msgType, "*INFO     ", 10);
  }
  else
  {
    memcpy(msgType, "*ESCAPE   ", 10);
  }
  //------ Send test key to message queue -----------------------------
  QMHSNDPM( "CPF9898",              /* Message ID                  */
            "QCPFMSG   QSYS      ", /* Message file name           */
            const_cast<char*>(msg), /* Message text                */
            strlen(msg),            /* Length of message text      */
            msgType,                /* Message type                */
            "*          ",          /* Call stack entry            */
            0,                      /* Call stack counter          */
            (char *)(&msgKey),      /* Message key - must be cast  */
                                    /* to char *                   */
            &ec);                   /* FlightRec code              */
  if ( ec.Bytes_Available > 0 )
  {
    // ignore
  }
}