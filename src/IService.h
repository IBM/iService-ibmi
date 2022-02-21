#ifndef ISERVICE_H
#define ISERVICE_H

/*************************************************************
 * Run service.
 * Parm:
 *    control     - control flags string with null-termination
 *    in          - input 
 *    in_size     - size of input
 *    out         - output buffer
 *    out_size_p  - (input)  size of output buffer;
 *                  (output) size of bytes returned in buffer.
 * Return:
 *    >=0 : Size of available bytes
 ************************************************************/
extern "C"  int runService(const char*  control,
                          const char*  in,
                          int          in_size,
                          char*        out,
                          int*         out_size_p);

/*************************************************************
 * Run service.
 * Parm:
 *    control     - control flags string with null-termination
 *    ccsid       - ccsid of input and output for parm control
 *    in          - input 
 *    in_size     - size of input
 *    out         - output buffer
 *    out_size_p  - (input)  size of output buffer;
 *                  (output) size of bytes returned in buffer.
 * Return:
 *    Size of available bytes
 ************************************************************/
extern "C"  int runService2(const char*  control,
                            int          ccsid,
                            const char*  in,
                            int          in_size,
                            char*        out,
                            int*         out_size_p);

/*************************************************************
 * Start IPC server.
 * Parm:
 *    ipc_key     - ipc key
 *    ccsid       - ccsid of ipc key

 * Return:
 *    0 - successful
 ************************************************************/
extern "C"  int runServer(const char* ipc_key, int ccsid);

#endif