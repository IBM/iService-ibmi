#include "JobUtil.h"
#include <qusec.h>
#include <stdlib.h>
#include <string.h>

Qwc_JOBI0600_t* JobUtil::_info = NULL;

/******************************************************
 * Retrieve job information in format JOBI0600.
 *****************************************************/
void JobUtil::rtvJobInfo()
{
  if ( NULL == _info )
  {
    _info = new Qwc_JOBI0600_t;

    Qus_EC_t ec;
    ec.Bytes_Provided = sizeof(Qus_EC_t);
    QUSRJOBI( _info, 
              sizeof(Qwc_JOBI0600_t), 
              "JOBI0600",
              "*                         ", 
              "                ", 
              &ec);

    if ( ec.Bytes_Available > 0 ) 
    {
      abort();
    }
  }
}

/******************************************************
 * Get job user name
 *****************************************************/
const char*  JobUtil::UserName()
{
  static char* ret = NULL;
  if ( NULL == ret )
  {
    rtvJobInfo();
    ret = new char[11];
    memset(ret, '\0', 11);
    memcpy(ret, _info->User_Name, 10);
  }
  return ret;
}

/******************************************************
 * Get job number
 *****************************************************/
const char*  JobUtil::JobNumber()
{
  static char* ret = NULL;
  if ( NULL == ret )
  {
    rtvJobInfo();
    ret = new char[7];
    memset(ret, '\0', 7);
    memcpy(ret, _info->Job_Number, 6);
  }
  return ret;
}

/******************************************************
 * Get job name
 *****************************************************/
const char*  JobUtil::JobName()
{
  static char* ret = NULL;
  if ( NULL == ret )
  {
    rtvJobInfo();
    ret = new char[11];
    memset(ret, '\0', 11);
    memcpy(ret, _info->Job_Name, 10);
  }
  return ret;
}

/******************************************************
 * Get job name
 * Return:
 *  '\0' - if *NONE 
 *****************************************************/
const char*  JobUtil::JobGrpPrfName()
{
  static char* ret = NULL;
  if ( NULL == ret )
  {
    rtvJobInfo();
    ret = new char[11];
    memset(ret, '\0', 11);
    if ( 0 != memcmp(_info->Group_Profile_Name, "*NONE     ", 10) )
    {
      memcpy(ret, _info->Group_Profile_Name, 10);
    }
  }
  return ret;
}
