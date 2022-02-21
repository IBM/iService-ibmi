#ifndef IPC_H
#define IPC_H
////////////////////////////////////////////////////////////////////////////
// ---------------------------------------------
// Multiple users to access the same IPC server:
// ---------------------------------------------
//   Configure parent path of IPC key direcory with multiple users 
//     OR
//   Configure a group profile for multiple users
//
// IPC key path setup:
//   1. Create by IPC startup automatically. It should not be existed.
//   2. Set authority to the current user if no group profile set in user 
//      profile. Otherwise grant group profile access as well.
//   3. Remove IPC key path automatically when IPC server ends.
//    
// -------------------------------------------
// Cleanup shrdm manually if accidents happen:
// -------------------------------------------
//  $ ipcs -mp
//  $ ipcrm -m 851
//  $ ipcrm -M 0X0101368F
//
// --------------
// IPC Workflow:
// --------------
//  |******* CLIENT *********|******************SERVER *************************|
//   storedp.svrpgm                                     
//   |
//   +--> sbm svr job  -------+-> main.pgm
//   |                            |
//   |                  +-----+   |
//   |                  |     |   +--> storedp.pgm
//   |                  V     |        | 
//   +--> Connecting server---+        |
//   |                                 +--> server setup
//   +--> connected to server          |
//   |                                 |
//   +--> lock client Q (QCLNT: 0->+1) |
//   |                                 +--> listen(SVR_WAIT:-1)        <---------+
//   |                                 |                                         |
//   +--> send request(write shrdm)    |                                         |
//   |                                 |                                         |
//   +--> notify svr to rcv req        +--> receive request(read shrdm)          |
//   |    (SVR_WAIT:+1)                |                                         |
//   |                                 +--> process request                      |
//   +--> wait response                |                                         |
//   |    (CLNT_WAIT:-1)               +--> send response(write shrdm)           |
//   |                                 |                                         |
//   |                                 +--> notify client to receive response    |
//   |                                 |                                         |                
//   +--> receive response(read shrdm) |                                         |
//   |                                 +--> wait client to finish receiving      |
//   |                                 |    (SVR_WAIT:-1)                        |                 
//   +--> notify server rcv done       |                                         |
//   |    ((SVR_WAIT:+1))              |                                         |
//   |                                 |                                         |
//   |                                 +-----------------------------------------+
//   +--> unlock client Q(SEM_UNDO) 
//        when job ends
//
////////////////////////////////////////////////////////////////////////////

#include "FlightRec.h"
#include <string>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#define SEM_NUM   3
typedef enum Sem_t
{
  QCLNT,        // Queue clients to talk with server. 0: wait
  SVR_WAIT,     // Server waits. <0 : wait
  CLNT_WAIT     // Client waits. <0 : wait
}Sem;

class IPC
{
public:
  //-------------- Common functions ------------------------------------------
  static bool setSignals(bool enable, FlightRec& fr);
  static void destroy(FlightRec& fr);
  static int  lockClientQ(FlightRec& fr);
  
  //-------------- Client functions ------------------------------------------
  static bool clientConnect(FlightRec& fr);
  static bool sendRequest(int ctl_size, int in_size, const char* control, const char* in, FlightRec& fr);
  static bool receiveResponse(std::string& out, FlightRec&fr);
  static bool clientClose(FlightRec& fr);
  static void serverSubmitted(bool flag);

  //-------------- Server functions ------------------------------------------
  static bool isServer();
  static bool setupIpcServer(const char* ipc_key, int ccsid, FlightRec& fr);
  static bool setTimeout(bool enable, int seconds, FlightRec& fr);
  static bool beingEnded();
  static bool serverListen(FlightRec& fr);
  static bool receiveRequest(char* &control, char* &in, int &in_size, FlightRec& fr);
  static void sendResponse(std::string& out);
  static bool serverWaitClient2Receive(FlightRec& fr);
  
private:
  IPC();
  ~IPC();
  IPC(const IPC&);
  IPC& operator=(const IPC&);

  static IPC* &me();
  static bool startIpcServerOnce(FlightRec& fr);

  bool           _server_submitted;
  bool           _server;
  std::string    _ipcKey;
  void*          _shdm_p;
  int            _semid;
  int            _shdmid;
  key_t          _semkey;
  key_t          _shmkey;
  struct sembuf  _ops[SEM_NUM];
};

#endif