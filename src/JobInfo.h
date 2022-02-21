#ifndef _JOBINFO_H
#define _JOBINFO_H

#include "Runnable.h"
#include "IAttribute.h"

/**********************************************************************
 * "jobinfo": {
 *     "name":"ISERV",
 *     "number": "001122",
 *     "user": "me"
 * }
 *********************************************************************/

class JobInfo: public Runnable
{
public:
  JobInfo(INode* node);
  ~JobInfo();

  void init(FlightRec& fr);
  void run(FlightRec& fr);

private:
  JobInfo(const JobInfo&);
  JobInfo& operator=(const JobInfo&);

  IAttribute*       _job_name;
  IAttribute*       _job_number;
  IAttribute*       _job_user;
};

#endif