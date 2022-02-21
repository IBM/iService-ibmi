#ifndef CONTROL_H
#define CONTROL_H
#include "FlightRec.h"

/***************************************************************************************
 * Control flags:
 *    *ipc(/ifs_path/key)           IPC key path
 *    *endipc                       End IPC server
 *    *waitserver(seconds)          Timeout for client to connect IPC server.
 *    *waitclient(seconds)          Timeout for waiting client
 *    *waitdata(seconds)            Timeout for waiting data
 *    *ipclog                       Force IPC server dump joblog
 *    *ipcsvr(lib/pgm)              IPC server startup program. iServie/Main by default
 *    *sbmjob[(job:jobd:user)]      *USRPRF:QSYS/QSRVJOB:ISERVICE by default
 *    *java                         Start java debug with pase.
 *    *before(ccsid)                CCSID of input
 *    *after(ccsid)                 CCSID of output
 *    *ipcsvr(lib/pgm)              IPC server startup program
 *    *performance                  Inject performance attributes
 *    ...
 ***************************************************************************************/

class Control
{
public:
  static bool           parse(const char* control, FlightRec& fr);
  static bool           ipc();
  static bool           endipc();
  static int            waitServer();
  static int            waitClient();
  static int            waitData();
  static bool           ipcLog();
  static void           ipcLog(bool log);
  static std::string&   flags();
  static int            before();
  static int            after();
  static void           before(int ccsid);
  static void           after(int ccsid);
  static bool           java();
  static const char*    ipcKey();
  static const char*    jobd2Sbm();
  static const char*    job2Sbm();
  static const char*    user2Sbm();
  static const char*    ipcServer();
  static bool           performance();
private:
  /****** class level members ********************************/
  static Control*& me();
  static Control*            _control_p;

  /****** instance level members *****************************/
  Control();
  Control(const Control&);
  Control& operator=(const Control&);
  ~Control();
  static int chopFlagValue(const char* p, std::string& v);

  std::string                _flags;              // Flag string in uppercased and whitespace trimmed
  std::string                _ipcKey;             // *ipc(/path/key)
  bool                       _endipc;             // *endipc
  int                        _wait_server;        // *waitserver(5)
  int                        _wait_client;        // *waitclient(600)
  int                        _wait_data;          // *waitdata(600)
  std::string                _sbmjob_jobd;        // *sbmjob(job:jobd:user)
  std::string                _sbmjob_job;         // *sbmjob(job:jobd:user)
  std::string                _sbmjob_user;        // *sbmjob(job:jobd:user)
  bool                       _java;               // *java - start java dbg with pase
  int                        _before;             // *before(819) - ccsid of input
  int                        _after;              // *after(819)  - ccsid of output
  bool                       _ipc_log;            // *ipclog     - ipc server fr to joblog
  std::string                _ipc_svr;            // *ipcsvr(lib/pgm)
  bool                       _performance;        // *performance
};

#endif