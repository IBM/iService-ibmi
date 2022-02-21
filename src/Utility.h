#ifndef UTILITY_H
#define UTILITY_H
#include <string>
#include "Common.h"

class INode;
class FlightRec;


/**************************************************
 * Special chars being converted to job ccsid.
 * This definition MUST be matched with jc_chars.
 *************************************************/
typedef enum Jc_Index_t
{
  SPACE,          //  ' '
  COMMA,          //  ','
  COLON,          //  ':'
  BACK_SLASH ,    //  '\\'
  SINGLE_QUOTE,   //  '\''
  DOUBLE_QUOTE,   //  '"'
  OPEN_CURLY ,    //  '{'
  CLOSE_CURLY,    //  '}'
  OPEN_SQUARE,    //  '['
  CLOSE_SQUARE,   //  ']'
  OPEN_PAREN ,    //  '('
  CLOSE_PAREN,    //  ')'
  LESS       ,    //  '<'
  GREAT      ,    //  '>'
  SLASH      ,    //  '/'
  PERCENT    ,    //  '%'
  LF         ,    //  '\n'
  LR         ,    //  '\r'
  DASH       ,    //  '-'
  STAR       ,    //  '*'
  EQUAL      ,    //  '='
  SEMICOLON  ,    //  ';'
  END
}jc_index;

/**************************************
 * Qus_EC_t alike with exception data.
 *************************************/
#pragma pack(1)
typedef struct Error_Code_t       
{                        
  int  Bytes_Provided;  
  int  Bytes_Available; 
  char Exception_Id[7]; 
  char Reserved;        
  char Exception_Data[512];
} Error_Code;
#pragma pack()

/**************************************
 * IO data format
 *************************************/
typedef enum IO_Format_t
{
  JSON,
  XML
} IO_Format;

INode*  parseNode(const char* in, int size, FlightRec& fr, int ccsid = 0);
void    runNode(INode* node, std::string& output, FlightRec& fr, int ccsid = 0);

char    JC(jc_index i);
int     convertCcsid(int fccsid, int tccsid, char** ppIn, size_t* pInLen, char** ppOut, size_t* pOutLen);
void    cvt2Upper(std::string& s);
void    trim(std::string& s);
void    trimTrailing(std::string& s, char c);
void    trimTrailing(char* s, int len, char c);
bool    hexChar2Bin(std::string& s);
bool    bin2HexChar(std::string& s);
void    writeIfs(char* file, std::string& data, int fromCcsid, FlightRec& fr);
void    readIfs(const char* file, std::string& data, int toCcsid, FlightRec& fr);
int     removeIfs(const char* file);
void    doCommand(std::string& cmd, FlightRec& fr);
void    doSystem(std::string& cmd, FlightRec& fr);
bool    getMessage(std::string& msg, const char* msgReplData, 
                const char* msgId, const char* msgFile = "QCPFMSG   *LIBL      ", bool helpOn = false);
bool    getMsgKey(int& msgKey);
void    getMessages(int startKey, int endKey, FlightRec& fr, bool lastMsgOnly = false);
void    sendMsg2Joblog(const char* msg, bool escape = false);


#endif