#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "JNode.h"
#include "INode.h"
#include "FlightRec.h"
#include "Common.h"
#include "Utility.h"
#include "IService.h"
#include <vector>
#include <string>
#include <sys/types.h>
#include <dirent.h>

/*********************************************************
 * Test program.
 * Usage:
 *  CALL ISERVICE/MainTest 
 *      [output_dir]
 *      [/path/file.json|*.json] 
 *      [ctl_flags]
 *********************************************************/

int main(int argc, char* argv[])
{
  const int  BUF_LEN = 1024;
  char OUT_DIR[BUF_LEN] = {'\0'};
  const int  OUTPUT_LEN = 16000000;

  std::vector<std::string> jsonFiles;  

  struct stat StatInfo;
  DIR *dir;
  struct dirent *entry;
  
  char* flags = NULL;
  FlightRec fr;
  int rc = 0;


  if ( argc != 3 && argc != 4 )
  {
    printf("CALL ISERVICE/MAINTEST [output_dir] [/path/file.json|/path/*.json] [ctl_flags] \n");
    return 1;
  }

  //----------------------------------------------------------------------------------//
  //--------------- Parse arguments --------------------------------------------------//
  //----------------------------------------------------------------------------------//
  /****** Argument 1: output directory ********************/
  strcpy(OUT_DIR, argv[1]);
  strcat(OUT_DIR, "/");
  rc = mkdir(OUT_DIR, S_IRWXU|S_IRGRP|S_IXGRP);
  int errNum = errno;
  if ( 0 != rc && EEXIST != errNum ) {
    printf("mkdir() failed with errno = %d : %s \n", errno, strerror(errno));
    return 2;
  }

  /****** Argument 2: input json file ********************/
  std::string fileName(argv[2]);
  int pos = fileName.find("/*.json");

  if ( pos != std::string::npos ) 
  { // wildcard found... running all jsons

    fileName[pos] = '\0';
    char* dirPath = &fileName[0];

    if ( NULL == (dir = opendir(dirPath)) )
    {
      printf("opendir() failed with errno = %d : %s \n", errno, strerror(errno));
      return 3;
    }
    else 
    {
      std::string jsonFile;
      while ( (entry = readdir(dir)) != NULL ) 
      {
        if ( entry->d_name[0] != '.' && 0 != memcmp(entry->d_name, "..", 2) && 
          strlen(entry->d_name) > 5 && 
          (0 == memcmp(&(entry->d_name[strlen(entry->d_name)-5]), ".json", 5) ||
           0 == memcmp(&(entry->d_name[strlen(entry->d_name)-5]), ".JSON", 5)    )){
            jsonFile = dirPath;
            jsonFile += "/";
            jsonFile += entry->d_name;

            jsonFiles.push_back(jsonFile);
          }
      }
    }

    closedir(dir);

  }
  else 
  { // running a single json
    jsonFiles.push_back(fileName);
  }

  /******** Argument 3: control flags *********************/
  if ( 4 == argc ) 
  {
    flags = argv[3];

  }

  //-------------------------------------------------------------------------------//
  //--------------- Processing  ---------------------------------------------------//
  //-------------------------------------------------------------------------------//

  for ( int i = 0; i < jsonFiles.size(); ++i ) 
  {
    std::string input;

    const char* pFileName = jsonFiles[i].c_str();

    printf("++++++++ Processing %s +++++++++++\n", pFileName);

    rc = stat(pFileName, &StatInfo);
    if ( 0 != rc )
    {
      printf("Failed to stat(%s). Errno = %d : %s \n", pFileName, errno, strerror(errno));
      return 1;
    }

    int fd = open(pFileName, O_RDONLY);  
    if ( fd < 0 )
    {
      printf("open(%s) failed with errno = %d : %s\n", pFileName, errno, strerror(errno));

      rc = -3;
    }
    else
    {
      /*************** Read and convert file data *****************************/
      char in[BUF_LEN] = {'\0'};
      char out[BUF_LEN*2] = {'\0'};
      int readN = 0;   
      bool readDone = false;

      do
      {
        readN = read(fd, in, BUF_LEN);

        // Process buff read from file
        if ( readN > 0 ) 
        {
          // Convert to job ccsid
          size_t iLen = readN;
          size_t oLen = BUF_LEN*2;
          char*  pIn  = &in[0];
          char*  pOut = &out[0];

          int success = convertCcsid((int)StatInfo.st_ccsid, 0, &pIn, &iLen, &pOut, &oLen);
          if ( 0 != success ) 
          {
            printf("Failed to convertCcsid(%s) from ccsid %d. Errno = %d. %d chars left. \n", 
                      pFileName, (int)StatInfo.st_ccsid, errno, iLen);
            
            rc = -1;
            break;
          }
          else 
          {
            // Get rid of LR/LF
            char* p = &out[0];
            int count = 0;
            while ( count < BUF_LEN*2-oLen )
            {
              if ( JC(LF) != *p && JC(LR) != *p  )
              {
                input += *p;
              }
              ++p;
              ++count;
            }
          }
        }

        // Handle state of read
        if ( readN > 0 ) 
        {
          rc = 0; 
        }
        else if ( readN < BUF_LEN ) 
        { // eof
          rc = 0;
          readDone = true;
        }
        else 
        {
          rc = -1;
        }
        
      } 
      while ( -1 == readN && errno == EINTR || !readDone );
      
      if ( 0 != rc ) // read error
      {
        printf("Failed to read(%s). Errno = %d. \n", fileName.c_str(), errno);
        
        rc = -1;
      }

      close(fd);
    }

    /***************** Call iService ****************************************/
    if ( 0 == rc ) 
    {
      int outputSize = OUTPUT_LEN;
      char* output = new char[outputSize];
      if ( NULL == output ) {
        printf("Failed to allocate memory for output.\n");
        return -4;
      }
      
      int totalBytes = runService(flags,
                                  input.c_str(),
                                  input.length(),
                                  output,
                                  &outputSize);


      // Print out the output
      printf("**** Total bytes of output    = %d\n", totalBytes);
      printf("**** Returned bytes of output = %d\n", outputSize);


      for ( int i = 0 ; i < outputSize; ++i )
      {
        printf("%c", output[i]);
      }

      printf("\n\n");

      if ( ERR_OK == fr.code() )
      {
        printf("**** Result = SUCCESSFUL\n\n");
      }
      else
      {
        printf("**** Result = FAILED\n\n");
      }

      // Write the output into files
      std::string outFile;
      outFile = OUT_DIR;
      const char* jsonName = NULL;
      int separator = jsonFiles[i].rfind('/');
      if ( std::string::npos == separator ) 
      {
        outFile += jsonFiles[i];
      }
      else 
      {
        outFile.append(jsonFiles[i], separator+1, jsonFiles[i].length()-separator);
      }

      int outFd = open(outFile.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_TEXTDATA|O_CCSID,
                S_IRUSR | S_IWUSR | S_IXUSR, 819);

      if ( outFd >= 0 ) 
      {
        write(outFd, output, outputSize);
      }
      else 
      {
        printf("open() failed with errno = %d, for output file = %s \n", errno, outFile.c_str());
      }

      close(outFd);


      delete[] output;

    }
    else 
    {}
  }


  return 0;
}
