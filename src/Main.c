#include "IService.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*****************************************************************
 * 1) Main entrace to call iService srvpgm.
 * 2) Main entrace to start IPC server.
 ****************************************************************/
int main( int argc, char* argv[] )
{
  if ( 7 != argc && 5 != argc && 3 != argc )
  {
    printf("*******************************************************\n");
    printf("* Run service:                                        *\n");
    printf("*     ctl         - CHAR(*) - Control flags           *\n");
    printf("*     ctl_ccsid   - INT(4)  - CCSID of control flags  *\n");
    printf("*     in          - CHAR(*) - Input buffer            *\n");
    printf("*     in_size     - INT(4)  - Size of input buffer    *\n");
    printf("*     out         - CHAR(*) - Output buffer           *\n");
    printf("*     out_size    - INT(4)  - Size of output buffer   *\n");
    printf("*                                                     *\n");
    printf("* Run IPC server:                                     *\n");
    printf("*     ipc_key   - CHAR(*) - IPC key path              *\n");
    printf("*     ccsid     - CHAR(*) - CCSID of IPC key          *\n");
    printf("*******************************************************\n");
    return 0;
  }

  if ( 7 == argc )
  {
    const char*   control     = (char*) argv[1];
    int           ctrl_ccsid  = *((int*) argv[2]);
    const char*   in          = (char*) argv[3];
    int           in_size     = *((int*) argv[4]);
    char*         out         = (char*) argv[5];
    int*          out_size_p  = (int*) argv[6]; 

    int availableBytes = runService2( control,
                                      ctrl_ccsid,
                                      in,
                                      in_size,
                                      out,
                                      out_size_p);
    
    if ( availableBytes != *out_size_p )
    {
      return 1;
    }
  }
  else if ( 3 == argc )
  {
    const char*   ipc_key = (char*) argv[1];
    int           ccsid   = atoi((char*) argv[2]);

    int rc = runServer(ipc_key, ccsid);

    return rc;
  }
  else
  {}


  return 0;
}