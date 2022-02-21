#ifndef JOB_UTIL_H
#define JOB_UTIL_H

#include <qusrjobi.h>
// typedef _Packed struct Qwc_JOBI0600 {
//     int  Bytes_Return;              
//     int  Bytes_Avail;               
//     char Job_Name[10];              
//     char User_Name[10];             
//     char Job_Number[6];             
//     char Int_Job_ID[16];            
//     char Job_Status[10];            
//     char Job_Type[1];               
//     char Job_Subtype[1];            
//     char Job_Switch[8];             
//     char End_Status[1];             
//     char Subsys_Name[10];        
//     char Subsys_Lib[10];         
//     char Curr_Usrprf_Name[10];   
//     char Dbcs_Enabled[1];        
//     char Exit_Key[1];            
//     char Cancel_Key[1];          
//     int  Product_Return_Code;    
//     int  User_Return_Code;       
//     int  Program_Return_Code;    
//     char Special_Environment[10];
//     char Special_Environment[10];
//     char Device_Name[10];       
//     char Group_Profile_Name[10];
//     Qwc_Grp_List_t Grp[15];     
//     char Job_User_ID[10];       
//     char Job_User_ID_Setting[1];
//     char Client_IP_Address[15]; 
//     char Reserved[2];           
//     int  Time_Zone_Info_Offset; 
//     int  Time_Zone_Info_Length; 
//     char Day_of_Week[4];        
//     /*Qwc_TZone_List_t TimeZoneInfo;*//* Varying length             */
// } Qwc_JOBI0600_t;                                                    

class JobUtil
{
public:
  static const char* UserName();
  static const char* JobNumber();
  static const char* JobName();
  static const char* JobGrpPrfName();
private:
  JobUtil();
  JobUtil(const JobUtil&);
  JobUtil& operator=(const JobUtil&);

  static void rtvJobInfo();

  static Qwc_JOBI0600_t*    _info;
};

#endif