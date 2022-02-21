#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <stdio.h>
#include "as400_types.h"
#include "as400_protos.h"

#define ROUND_QUAD(x) (((size_t)(x) + 0xf) & ~0xf)

typedef struct 
{
  ILEarglist_base base;
  ILEpointer      control;      /* control        */
  int32           ccsid;        /* control ccsid  */
  char            filler1[8];
  ILEpointer      in;           /* input          */
  int32           in_size;      /* input size     */
  char            filler2[8];
  ILEpointer      out;          /* output         */
  ILEpointer      out_size_p;   /* output size    */
} ILEarglistSt;

int main(int argc, char* argv[])
{
  char* control0[1024 + 15] = {'\0'};
  char* control      = NULL;
  char* in0          = NULL;
  char* in           = NULL;
  int   in_size      = 0;
  char* out0         = NULL;
  char* out          = NULL;
  char* out_size_p0  = NULL;
  char* out_size_p   = NULL;
  int   out_size     = 0;
  int   ccsid        = 0;

  int   rc              = 0;
  int   availableBytes  = 0;
  
  char ILEarglist_buf[sizeof(ILEarglistSt) + 15] = {0};
  ILEarglistSt* ILEarglist  = (ILEarglistSt*)ROUND_QUAD(ILEarglist_buf);
  
  char ILEtarget_buf[sizeof(ILEpointer) + 15] = {0};
  ILEpointer* ILEtarget = NULL;
  unsigned long long actmark = 0;

  if ( argc != 5 )
  {
    printf("Usage:                                                           \n");
    printf(" iService   <control>           - CHAR(*) - Control flag string  \n");
    printf("            <control_ccsid>     - CHAR(*) - CCSID of control     \n");
    printf("            <input>             - CHAR(*) - Input string         \n");
    printf("            <output_max_size>   - CHAR(*) - Max output size      \n");
    return 99;
  }
  else
  {
    control     = (char*)ROUND_QUAD( control0 );
    strcpy(control, (char*)argv[1]);
    
    ccsid       = atoi((char*)argv[2]);

    in_size     = strlen((char*)argv[3]);
    in0         = (char*)malloc( in_size + 15 );
    if ( NULL == in0 )
    {
      abort();
    }
    in          = (char*)ROUND_QUAD( in0 );
    memset(in, '\0', in_size);
    strcpy(in, (char*)argv[3] );

    out_size_p0 = (char*)malloc( sizeof(int) + 15 );
    if ( NULL == out_size_p0 )
    {
      abort();
    }
    out_size_p  = (char*)ROUND_QUAD( out_size_p0 );
    out_size    = atoi((char*)argv[4]);
    memcpy(out_size_p, (char*)&out_size, sizeof(int));

    out0        = (char*)malloc( out_size + 15 );
    if ( NULL == out0 )
    {
      abort();
    }
    out         = (char*)ROUND_QUAD( out0 );
    memset(out, '\0', out_size);

  }

  actmark = _ILELOADX("ISERVICE/STOREDP", ILELOAD_LIBOBJ);
  if ( actmark == -1 )
  {
    printf("_ILELOADX() Failed with errno = %d", actmark);
    return 1;
  }

  ILEtarget = (ILEpointer*)ROUND_QUAD(ILEtarget_buf);
  rc = _ILESYMX(ILEtarget, actmark, "runService2");
  if ( rc == -1 )
  {
    printf("_ILESYMX() Failed with errno = %d", actmark);
    return 2;
  }

  short int signature[] =
  {
      ARG_MEMPTR,   /* char* control    */
      ARG_INT32,    /* int   ccsid      */
      ARG_MEMPTR,   /* char* in         */
      ARG_INT32,    /* int   in_size    */
      ARG_MEMPTR,   /* char* out        */
      ARG_MEMPTR,   /* int*  out_size_p */
      ARG_END
  };


  ILEarglist->control.s.addr     = (address64_t)control;
  ILEarglist->ccsid              = ccsid;
  ILEarglist->in.s.addr          = (address64_t)in;
  ILEarglist->in_size            = in_size;
  ILEarglist->out.s.addr         = (address64_t)out;
  ILEarglist->out_size_p.s.addr  = (address64_t)out_size_p;


  result_type_t result_type = RESULT_INT32;

  _ILECALL(ILEtarget, &ILEarglist->base, signature, result_type);

  availableBytes = ILEarglist->base.result.s_int32.r_int32;

  out_size = *((int*)out_size_p);

  if ( availableBytes != out_size )
  {
    printf("(E) Available bytes = %d. Returned bytes = %d.", availableBytes, out_size);
  }
  else
  {
    for ( int i = 0; i < out_size; ++i )
    {
      printf("%c", out[i]);
    }
  }

  free(in0);
  free(out0);
  free(out_size_p0);

  return 0;
}

