#include <stdlib.h>   
#include <stdio.h>                                                                                                                                 
int main(int argc, char* argv[])                                                                       
{               
  if ( argc > 1 ) 
  {                                                                                                                                                                          
    char* arg1 = (char*) argv[1]; 
    arg1[0] = 'F';
  }
  if ( argc == 8 )
  {                                                   
    int*  arg2 = (int*) argv[2]  ;                                   
    *arg2 = 2222;                                                      
    unsigned long long int* arg3 = (unsigned long long int*) argv[3];
    *arg3 = 3333;                                                      
    float* arg4 = (float*) argv[4];                                  
    *arg4 = 444444.444444;                                              
    double* arg5 = (double*) argv[5];                                
    *arg5 = 555555.555555;                                          
    char* arg6 = (char*) argv[6];                                                                 
    arg6[0] = 0xFF;                                                                                     
    *((short int*)argv[7]) += 3; // bin(2)                                                                     
    char str[] = "ESC\"NO\\\" "; // char(20).. only change a part                                
    memcpy((char*)(argv[7]+2), str, strlen(str));
  }                           
  return 0; 
}                                                           