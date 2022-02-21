#include <stdlib.h>                                                                             
#include <stdio.h>

#ifdef __cplusplus                                                                              
extern "C"{                                                                                   
#endif                                                                                          
int TestMe(int arg1, int* arg2, short int arg3, short int* arg4,
           long long arg5, long long* arg6, unsigned int arg7,  
           unsigned int* arg8, unsigned short int arg9,         
           unsigned short int* arg10, float arg11, float* arg12,
           double arg13, double* arg14, char* arg15)                                                                         
{                                                                                               
  arg1 += 1000;                                                                                    
  *arg2 += 1000;                                                                                  
  arg3 += 1000;                                                                                    
  *arg4 += 1000;                                                                                   
  arg5 += 1000;                                                                                    
  *arg6 += 1000;                                                                                   
  arg7 += 1000;                                                    
  *arg8 += 1000;                                                                            
  arg9 += 1000;                                                                             
  *arg10 += 1000;                                                                           
  arg11 += 1000;                                                                           
  *arg12 += 1000;                                                                          
  arg13 += 1000;                                                                           
  *arg14 += 1000;                                                                          
  *arg15 = 'F';                                                                          
  return 99;                                                                            
}
#ifdef __cplusplus                                                                              
}                                                                                  
#endif 

#ifdef __cplusplus                                                                              
extern "C"{                                                                                   
#endif 
int TestMeAllByRef(int *arg1, short int* arg2, long long* arg3,      
    float* arg4, double* arg5, unsigned int* arg6,                   
    unsigned short int* arg7)                                        
{                                                                    
  *arg1 += 1000;                                                        
  *arg2 += 1000;                                                        
  *arg3 += 1000;                                                        
  *arg4 += 1000;                                                        
  *arg5 += 1000;                                                        
  *arg6 += 1000;                                                        
  *arg7 += 1000;                                                        
  return 88;                                                         
}
#ifdef __cplusplus                                                                              
}                                                                                  
#endif                                                                     