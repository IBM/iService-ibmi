#include "JobInfo.h"
#include "INode.h"
#include "Utility.h"
#include "JobUtil.h"
#include <stdlib.h>
#include <string>
#include "Ipc.h"

/******************************************************
 * Constructor
 *****************************************************/
JobInfo::JobInfo(INode* node)
  : Runnable(node), 
    _job_name(NULL), _job_number(NULL), _job_user(NULL)
{}

/******************************************************
 * Destructor
 *****************************************************/
JobInfo::~JobInfo()
{}

/******************************************************
 * Initialize JobInfo
 * 
 * Parm:
 *    fr  -   error info
 *****************************************************/
void JobInfo::init(FlightRec& fr)
{
  std::string n;
  std::string v;

  n = "NAME";
  _job_name = _node_p->getAttribute(n);

  n = "USER";
  _job_user = _node_p->getAttribute(n);

  n = "NUMBER";
  _job_number = _node_p->getAttribute(n);

  // no individual STATUS needed in output
  _node_p->noStatus(true);

}

/******************************************************
 * Run the JobInfo
 * 
 * Parm:
 *    fr  -   error info
 *****************************************************/
void JobInfo::run(FlightRec& fr)
{
  std::string v;

  if ( NULL != _job_name )
  {
    v = JobUtil::JobName();
    trim(v);
    _job_name->setValue(v);
  }

  if ( NULL != _job_user )
  {
    v = JobUtil::UserName();
    trim(v);
    _job_user->setValue(v);
  }

  if ( NULL != _job_number )
  {
    v = JobUtil::JobNumber();
    trim(v);
    _job_number->setValue(v);
  }
}