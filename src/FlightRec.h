#ifndef FLIGHT_RECORDER_H
#define FLIGHT_RECORDER_H
#include <string>
#include <list>

#define STS_STATUS                      "STATUS"
#define STS_MESSAGE                     "MESSAGE"
#define STS_CODE                        "CODE"
#define STS_DATA                        "DATA"
#define PERF_INIT                       "INIT_ELAPSED"
#define PERF_RUN                        "RUN_ELAPSED"

#define INTERNAL_MSG_ID                 "ERR"

#define ERR_OK                          0
#define ERR_PARSE_FAIL                  1
#define ERR_PARSE_BLANK                 2

#define ERR_INPUT_INVALID               51

#define ERR_CMD_INIT                    101
#define ERR_SH_INIT                     105
#define ERR_QSH_INIT                    106
#define ERR_PGM_INIT                    107
#define ERR_ARGUMENT_INIT               108
#define ERR_RETURN_INIT                 109
#define ERR_SQL_INIT                    110
#define ERR_CONTROL_INIT                111
#define ERR_JOBINFO_INIT                112

#define ERR_CMD_RUN                     151
#define ERR_SYSTEM_RUN                  152
#define ERR_REXX_RUN                    153
#define ERR_QSH_RUN                     154
#define ERR_PGM_RUN                     155
#define ERR_SQL_RUN                     156

#define ERR_WRITE_IFS                   201
#define ERR_READ_IFS                    202

#define ERR_PASE_START                  251
#define ERR_PASE_RUN                    252

#define ERR_CGI_RTV_URL                 301
#define ERR_CGI_CHOP_PARMS              302

#define ERR_IPC_SVR_SETUP               351
#define ERR_IPC_SVR_LISTEN              352
#define ERR_IPC_SVR_WAIT_CLNT           353
#define ERR_IPC_DESTROY                 354
#define ERR_IPC_CLNT_CONNECT            355
#define ERR_IPC_CLNT_CLOSE              356
#define ERR_IPC_CLNT_REQUEST            357
#define ERR_IPC_CLNT_RESPONSE           358
#define ERR_IPC_TIME_OUT                359
#define ERR_IPC_SET_SIGNALS             360

class INode;

class Pair
{
public:
  Pair(const std::string& name, const std::string value)
    : _name(name), _value(value)
  {}

  ~Pair() 
  {}

  const std::string& name() 
  {
    return _name;
  }

  const std::string& value() 
  {
    return _value;
  }

private:
  Pair(const Pair&);
  Pair& operator=(const Pair&);

  std::string _name;
  std::string _value;
};

class FlightRec
{
public:
  FlightRec();
  FlightRec(int code);
  ~FlightRec();

  const int code();
  void code(int code);
  void joblog(bool log);
  void addMessage(const char* msg);
  void addMessage(int no, const char* msg);
  void addMessage(const char* value, const char* msg);
  void addMessage(const std::string& msgId, const std::string& msgText);
  void addMessages(const std::list<class Pair*>& msgs);
  void messages(const std::list<class Pair*>& msgs);
  const std::list<class Pair*>& messages();
  bool hasMessages();
  void clearMessages();
  void reset();
  void toString(std::string& str, int ccsid = 0);

private:
  FlightRec(const FlightRec&);
  FlightRec& operator=(const FlightRec&);
  void logMessage(const std::string& id, const std::string& msg);

  int                     _code;
  std::list<class Pair*>  _msgs; 
  bool                    _joblog; 

};

#endif