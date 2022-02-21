#ifndef PASE_H
#define PASE_H
#include <string>
#include "FlightRec.h"
#include <qp2user.h>
#include <pointer.h>
#include <miptrnam.h> 
#include <except.h>
#include <qleawi.h> 

///////////////////////////////////////////////////////////////////////////
// _ILECALL() : parameter and return structure.
//-------------------------------
// Argument Length  Alignment  
//-------------------------------
// 1 byte 	        any
// 2 bytes 	        2 bytes
// 3-4 bytes 	      4 bytes
// 5-8 bytes 	      8 bytes
// 9 or more bytes 	16 bytes
//---------- Alias names for basic types ---------------------------------
typedef /*_SYSPTR*/char*             ILEpointer;
typedef signed char		      int8;
typedef signed short		    int16;
typedef	signed int		      int32;
typedef	signed long long	  int64;
typedef unsigned char		    uint8;
typedef unsigned short		  uint16;
typedef	unsigned int		    uint32;
typedef	unsigned long long	uint64;
typedef float		            float32;
typedef double		          float64;
typedef long double	        float128;
//----------- Argument types ----------------------------------------------
typedef int16	              ILECALL_Arg_t;
#define ARG_END		0	/* end-of-list sentinel */
#define ARG_INT8	(-1)
#define ARG_UINT8	(-2)
#define ARG_INT16	(-3)
#define ARG_UINT16	(-4)
#define ARG_INT32	(-5)
#define ARG_UINT32	(-6)
#define ARG_INT64	(-7)
#define ARG_UINT64	(-8)
#define ARG_FLOAT32	(-9)
#define ARG_FLOAT64	(-10)	/* 8-byte binary or decimal float */
#define ARG_MEMPTR	(-11)	/* Caller provides PASE memory address */
#define ARG_SPCPTR	(-12)	/* Caller provides tagged pointer */
#define ARG_OPENPTR	(-13)	/* Caller provides tagged pointer */
#define ARG_MEMTS64	(-14)	/* Caller provides PASE memory address */
#define ARG_TS64PTR	(-15)	/* Caller provides teraspace pointer */
#define ARG_SPCPTRI	(-16)	/* Caller provides address of tagged ptr */
#define ARG_OPENPTRI	(-17)	/* Caller provides address of tagged ptr */
#define ARG_FLOAT128	(-18)	/* 16-byte binary or decimal float */
//----------- /QOpenSys/QIBM/ProdData/OS400/PASE/include/as400_types.h ----
#pragma pack(1)
typedef struct ILEarglist_base_t 
{
	ILEpointer		descriptor; /* Operational descriptor */
  union {
	ILEpointer	r_aggregate; /* Aggregate result pointer */
	struct {
	    char	filler[7];
	    int8	r_int8;
	} s_int8;
	struct {
	    char	filler[7];
	    uint8	r_uint8;
	} s_uint8;
	struct {
	    char	filler[6];
	    int16	r_int16;
	} s_int16;
	struct {
	    char	filler[6];
	    uint16	r_uint16;
	} s_uint16;
	struct {
	    char	filler[4];
	    int32	r_int32;
	} s_int32;
	struct {
	    char	filler[4];
	    uint32	r_uint32;
	} s_uint32;
	int64		r_int64;
	uint64		r_uint64;
	float64		r_float64;
	float128	r_float128; 
  } result;			/* Function result */
} ILEarglist_base;
#pragma pack()
//----------------- Result types ------------------------------------------
typedef int16	Result_Type_t;
/* positive result_type_t values are length of aggregate result */
/* (zoned decimal and packed longer than 31 digits returned as aggregate result) */
#define RESULT_VOID	0
#define RESULT_INT8	(-1)
#define RESULT_UINT8	(-2)
#define RESULT_INT16	(-3)
#define RESULT_UINT16	(-4)
#define RESULT_INT32	(-5)
#define RESULT_UINT32	(-6)
#define RESULT_INT64	(-7)
#define RESULT_UINT64	(-8)
#define RESULT_FLOAT64	(-10)	/* 8-byte binary or decimal float */
#define RESULT_FLOAT128	(-18)	/* 16-byte binary or decimal float */
#define RESULT_PACKED15	(-19)	/* 15-digit packed decimal */
#define RESULT_PACKED31	(-20)	/* 31-digit packed decimal */
///////////////////////////////////////////////////////////////////////////

class Pase
{
public:
  static void   runShell(const char* cmd, std::string& output, FlightRec& fr);
  static void   runPgm(const char* pgm, const char* lib, QP2_ptr64_t* argv, int* rtnValue, FlightRec& fr); 
  static void   runSrvgm(const char* srvpgm, const char* lib, const char* func, 
                        QP2_ptr64_t* argv, QP2_ptr64_t* sigs, 
												Result_Type_t rtnType, FlightRec& fr); 
	static bool   start(FlightRec& fr);
  static void   end();
  static char*  allocAlignedPase(char** ileAlignedPtr, QP2_ptr64_t* paseAlignedPtr, 
                                 int size, FlightRec& fr, int align = 16);
  static int    ccsid() { return _ccsid; }
	static void   jvmPase(bool jvm);
private:
  Pase();
  ~Pase();
  Pase(const Pase&);
  Pase& operator=(const Pase&);

  static bool   startJVM(FlightRec& fr);
  static bool   startPase(FlightRec& fr);

  static void*  findSymbol(const char* func, FlightRec& fr);
  static char*  allocAndCvt(const char* in, int len, QP2_ptr64_t* pasePtr, FlightRec& fr);

  static QP2_ptr64_t  popen(const char* file, const char* mode, FlightRec& fr);
  static int          pclose(QP2_ptr64_t fp, FlightRec& fr);
  static QP2_ptr64_t  fgets(QP2_ptr64_t paseBuff, int size, QP2_ptr64_t fp, FlightRec& fr);

  static bool           _jvm_pase;
  static size_t         _ptr_bits;
  static int            _ccsid;
  static QP2_ptr64_t    _id;
};

#endif