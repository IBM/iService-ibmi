# iService
iService is a set of procedures that help you to exchange data in JSON format with IBM i systems. 

## Introduction
### Advantages Over [XMLService](https://github.com/IBM/xmlservice)
* Light-weighted with essential features
* C/C++ implementation against RPG for more modern language developers
* Simplified parameters of XMLService similar interfaces 
* API style declarations of data types against RPG style

### Best practice
* Use HTTP REST APIs with HTTPS configuration for IBM i HTTP Apache Server to protect your privacy in public network. [See more](#http-request)
* Set appropriate authorities to objects built up with default [Makefile](Makefile) or customize with your own rules. [See more](#security-consideration)
* [IPC](#ipc-control-flags) mode should be only considered for high-frequency calls if jobs are with time-consuming intialization as IPC startup also takes additional time cost
* Configure user profiles for IPC mode per your own security and audit policies. [See more](#security-consideration)

### Support
This project is still under development. Plan your tests well before deploying on productions. 

## How to Use

### DB2 Stored Procedure

[Stored Procedures Registered](src/Storedp.sql.in)

```sql
call iService.iPlug1M(<control>, <input>, <output>)
```

### HTTP Request

```html
http(s)://<hostname>/iservice/httpcgi.pgm?db2=<db_name>&uid=<user>&pwd=<password>&ctl=<control>&in=<input>&out=<max_output_size>
```

*Configure IBM i HTTP(s) server*
```sh
--------------------------------
/www/apachedft/conf/httpd.conf:
--------------------------------
 ScriptAlias /iservice/ /QSYS.LIB/ISERVICE.LIB/
 <Directory /QSYS.LIB/ISERVICE.LIB/>
   AllowOverride None
   order allow,deny
   allow from all
   SetHandler cgi-script
   Options +ExecCGI
 </Directory>
```

### ILE External Interfaces

[Interface Description](src/IService.h)

```c
extern "C"  int runService(control, in, in_size, out, out_size_p);
extern "C"  int runService2(control, ccsid, in, in_size, out, out_size_p);
extern "C"  int runServer(ipc_key, ccsid);
```

### PASE Executable

[Parameter Description](src/pase/iService.c)

```text
/usr/bin/iService  'control_flags'  'control_ccsid' 'input'  'max_output_size'
-------------------------------------------------
  ctl         - CHAR(*) - Control flags.           
  ctl_ccsid   - CHAR(*) - CCSID of control flags.
  in          - CHAR(*) - Input buffer.                     
  out_size    - CHAR(*) - Size of output buffer.   
```
### ILE Executable

[Parameter Description](src/Main.c)


```text
iService/Main:
-------------------------------------------------
  ctl         - CHAR(*) - Control flags           
  ctl_ccsid   - INT(4)  - CCSID of control flags  
  in          - CHAR(*) - Input buffer            
  in_size     - INT(4)  - Size of input buffer    
  out         - CHAR(*) - Output buffer           
  out_size    - INT(4)  - Size of output buffer   
```

## Parameter Control
> Note:
> Parameter Control ends with null-termitator and contains up to 1024 bytes with ending null-terminator included.

### General Control Flags

| Control Flag              | Description                                                           |
| ------------------------- | --------------------------------------------------------------------- |
| *java                     | Start JAVA debug mode with PASE.                                      |
| *before(ccsid)            | CCSID of input data.                                                  |
| *after(ccsid)             | CCSID of output data.                                                 |
| *performance              | Inject performance data.                                              |


### IPC Control Flags

| Control Flag              | Description                                                           |
| ------------------------- | --------------------------------------------------------------------- |
| *ipc(/ifs_path/key)       | Run as IPC client. If IPC server not exist then submit server job. One IPC server job serves multiple client jobs with users in the same group. |
| *endipc                   | End IPC server. IPC server only.                                       |
| *waitserver(seconds)      | IPC client waits IPC server to start up. Value must greater than 0. Default is 5 seconds. IPC client only. | 
| *waitclient(seconds)      | IPC server timeout value of listening for clients. 0 means forever. Default is 300 seconds. IPC server only.                           |
| *waitdata(seconds)        | IPC server timeout value of exchanging data with clients. 0 means forever. Default is 5 seconds IPC server only.                           |
| *ipclog                   | IPC server/clients log messages and dump to spooled files.             |
| *sbmjob[(job:jobd:user)]   | Describe for IPC server job to submit. *sbmjob(ISERVICE:QSYS/QSRVJOB:*CURRENT) is used as default.                     |
| *ipcsvr(lib/pgm)          | IPC server startup program.                                                           |

> Note:
> * User profiles of IPC client/server jobs should have access pre-configured to the IPC key path. One way is to assign a group profile with accesses to IPC user profiles.
> * Parent path of IPC key directory should be existed before launching IPC mode. IPC key directory is automatically created by IPC server job. All accesses are granted to group profile of current user profile running IPC server job if set.
> * One IPC server job does the real work with system functions under the user profile running the IPC server job on behalf of multiple IPC client jobs.

## Parameter Input

### JSON

#### JOBINFO  
---

| Attribute   | Value        | Description                                                          |
|-------------|--------------|----------------------------------------------------------------------|
| name        | string       | Job name.                                                            |
| user        | string       | Job user.                                                            |
| number      | string       | Job number.                                                          |

> Retrieve information of job that executes request. 
> * In IPC mode, it is the server job.
> * In non-IPC mode, it is the current job.

[SHELL/QSHELL Example](test/T_JobInfo.json)

#### CMD  
---

| Attribute   | Value        | Description                                                          |
|-------------|--------------|----------------------------------------------------------------------|
| exec        | cmd          | CL call. Default.                                                    |
|             | system       | Called with system().                                                |
|             | rexx         | Called with REXX.                                                    |
| value       | Command      | String.                                                              |
| error       | off          | Only escape message reported. Default.                               |
|             | on           | Messages during the execution.                                       |
|             | ignore       | Escape message is reported but not stop for 'exec' and 'system'.     |

[CMD Example](test/T_Cmd.json)

#### SHELL/QSHELL  
---

| Attribute   | Value        | Description                                                          |
|-------------|--------------|----------------------------------------------------------------------|
| value       | Command      | Command string.                                                      |
| error       | off          | Only escape message reported. Default.                               |
|             | on           | Messages during the execution.                                       |

[SHELL/QSHELL Example](test/T_Shell.json)

#### PGM/SRVPGM
---

| Attribute   | Value        | Description                                                          |
|-------------|--------------|----------------------------------------------------------------------|
| name        | Name         | Program or service program.                                          |
| lib         | Library      | Library.                                                             |
| func        | Exported symbol | Service program only.                                             |
| error       | off          | Only escape message reported. Default.                               |
|             | on           | Messages during the execution.                                       |
| parm        | [{P1}...{Pn}] | Array of parameters P1...Pn                                         |
| return      | {R}          | Return value                                                         |
| pase        | off          | Don't call via PASE. Default value. Users don't need specify this and 
let iService decide automatically.        |
|             | on           | Force to call via PASE.                                              |


* Pramater Pn

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| io          | in            | Input type.                                                         |
|             | out           | Output type.                                                        |
|             | both          | Input and output type.                                              |
| hex         | on            | Input/output for char(n) in hexadecimal format for non-DS types.                      |
|             | off           | Default                                                             |
| by          | value         | Parameter value is passed by value. Service program only for non-DS types.          |
|             | ref           | Parameter value is passed by reference/pointer. Service program  only. Default.                                                                                      |
| type        | int(n)        | n bytes signed integer. n = 2, 4 or 8                               |
|             | uint(n)       | n bytes unsigned integer. n = 2, 4 or 8                             |
|             | char(n)       | n bytes characters                                                  |
|             | float(n)      | Fractional precision n                                              |
|             | double(n)     | Fractional precision n                                              |
|             | packed(n, m)  | Digits n and fractional m                                           |
|             | zoned(n, m)   | Digits n and fractional m                                           |
|             | ds            | Data structure                                                      |
| value       | string/number | For not 'ds' type, string or number value                           |
|             | [{M1}...{Mn}] | For 'ds' type, arrary of data strucure members                      |

* Data Structure Member Mn

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| type        | int(n)        | n bytes signed integer. n = 2, 4 or 8                               |
|             | uint(n)       | n bytes unsigned integer. n = 2, 4 or 8                             |
|             | char(n)       | n bytes characters                                                  |
|             | float(n)      | Fractional precision n                                              |
|             | double(n)     | Fractional precision n                                              |
|             | packed(n, m)  | Digits n and fractional m                                           |
|             | zoned(n, m)   | Digits n and fractional m                                           |
|             | ds            | Embedded data structure                                             |
| value       | string/number | String or number value                                              |
| hex         | on            | Input/output for char(n) in hexadecimal format for non-DS entries.  |
|             | off           | Default                                                             |
| by          | value         | Parameter value is passed by value as default.                      |
|             | ref           | Parameter value is passed by reference/pointer. Only for non-DS entries.         |

> Top level 'ds' used as parameters are always passed by reference.
> Embedded 'ds' and non-DS type entries in parameter are passed by value as default. 


* Return Value R


|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| type        | int(n)        | n bytes signed integer. n = 2, 4 or 8                               |
|             | uint(n)       | n bytes unsigned integer. n = 2, 4 or 8                             |
|             | char(n)       | n bytes characters                                                  |
|             | float(n)      | Fractional precision n                                              |
|             | double(n)     | Fractional precision n                                              |
|             | packed(n, m)  | Digits n and fractional m                                           |
|             | zoned(n, m)   | Digits n and fractional m                                           |
| value       | string/number | Value returned                                                      |



> Executing program via system interfaces of ILE or PASE is determined internally. 
> By default program is run via system interfaces of ILE.
> If number of program prameters is great than 32, program is run via system interfaces of PASE.

[PGM Executed with ILE Example](test/T_PgmILE.json) 

[PGM Executed with PASE Example](test/T_API_PASE.json) 

> Executing service program via system interfaces of ILE or PASE is determined internally. 
> By default service program is run via system interfaces of ILE.
> Service program is run via system interfaces of PASE if 
> * all the paramters are passed in by reference
> * number of prameters is great than 32
> * return value is required

[SRVPGM Executed with ILE Example](test/T_SrvpgmILE.json) 

[SRVPGM Executed with PASE Example](test/T_SrvpgmPase.json) 

#### SQL
---

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| servermode  | on            | Turn on server mode for SQL env if allowed. Default.                |
|             | off           | Turn off server mode for SQL env if allowed.                        |
| error       | off           | Only escape message reported. Default.                              |
|             | on            | Messages during the execution.                                      |
| connect     | [{CONN1}...{CONNn}] | Connection specification.                                     |
| operation   | [{OP1}...{OPn}] | Run SQL statements.                                               |
| free        | "all"         | Free all the connections with the SQL env. Default.                 |
|             | "none"        | Keep all open connections.                                          |
|             | ["conn_label", ...] | Array of connection lables in string format. This will turn off freeing all connections.                 |

* Connect Value CONNn

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| conn        | connection id | String.                                                             |
| db          | db name       | String.                                                             |
| uid         | user id       | String. Not used in SQL server mode.                                |
| pwd         | password      | Authentication charaters. Not used in SQL server mode.              |
| autocommit  | on            | Automatically commit for connection. Default.                       |
|             | off           | No auto-commit.                                                     |
| commit      | no_commit     | Reference to SQL_TXN_NO_COMMIT.                                     |
|             | read_uncommitted  | Reference to SQL_TXN_READ_UNCOMMITTED.                          |
|             | read_committed  | Reference to SQL_TXN_READ_COMMITTED.                              |
|             | repeatable_read | Reference to SQL_TXN_REPEATABLE_READ.                             |
|             | serializeble    | Reference to SQL_TXN_SERIALIZABLE.                                |

* Operation Value OPn

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| execDirect  |{EXD}          | Execute a statement.                                                |
| exec        |{EX}           | Execute a statement with parameters.                                |
| commit      |{CMT}          | Commit transaction.                                                 |

* Commit value CMT

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| conn        | connection id | Reference to connections defined in 'connect'.                      |
| action      | commit        | Reference to SQL_COMMIT.                                            |
|             | rollback      | Reference to SQL_ROLLBACK.                                          |
|             | commit_hold   | Referenc to SQL_COMMIT_HOLD.                                        |
|             | rollback_hold | Referenc to SQL_ROLLBACK_HOLD.                                      |

* ExecDirect Value EXD

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| conn        | connection id | Reference to connections defined in 'connect'. If not specified, database *LOCAL is used with current job user profile.                 |
| rowcount    | number        | Row count affected by the statement.                                |
| value       | statement     | Statement string.                                                   |
| fetch       | {F}           | Fetch for SELECT statement only.                                    |

* Exec Value EX

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| conn        | connection id | Reference to connections defined in 'connect'.                      |
| rowcount    | number        | Row count affected by the statement.                                |
| value       | statement     | Statement string.                                                   |
| parm        | [{P1}...{Pn}] | Parameters for SQL statement.                                       |
| fetch       | {F}           | Fetch for SELECT statement only.                                    |

* Fetch Value F

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| block       | all           | Fetch all the rows.                                                 |
|             | number        | Fetch a certain number of rows.                                     |
| rec         | number        | Starting position(>=1) of rows to fetch.                            |

* Parameter Value Pn

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| io          | in            | Input type.                                                         |
|             | out           | Output type.                                                        |
|             | both          | Input and output type.                                              |
| value       | number        | Specify number for type INTEGER, SMALLINT, BIGINT, FLOAT, DECFLOAT, NUMERIC, DECIMAL, REAL, DOUBLE. |
|             | chars         | Specify characters for type CHAR, VARCHAR, TIMESTAMP, DATE, TIME.   |
|             | hex chars     | Specify in hexadecimal format chars for type GRAPHIC, VARGRAPHIC, BIN, VARBINARY. |

[SQL Example](test/T_Sql.json) 

## Parameter Output

### JSON

#### Overall Status

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| STATUS      | 0             | Successful.                                                         |
|             | > 0           | Failed. [See definitions](./src/FlightRec.h) |

#### Individual Status

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| STATUS      | {S1, ..., Sn} | Contains STATUS attributes                                          |

* STATUS Attribute Sn

|  Attribute  | Value         | Description                                                         |
|-------------|---------------|---------------------------------------------------------------------|
| CODE        | 0             | Successful.                                                         |
|             | > 0           | Failed. [See definitions](./src/FlightRec.h)  |
| MESSAGE     | []            | Array of result messages.                                           |
| DATA        | {}, []        | Contains result data.                                               |



## Build

> If you do not have IBM i open source package installed, 
> 1. Start SSH server in your IBM i system by 'STRTCPSVR SERVER(*SSHD)'
> 2. In IBM i Access Client Solutions(ACS). From menu bar, select 'Tools' and then select item 'Open source package managment' to install on your IBM i system.
> 3. Use your SSH client to connect your IBM i system.
> 4. Now you are ready to start the build. 

### Requirements
Building requires GNU make and gcc. These can be installed with `yum`: `yum install make-gnu gcc`

### Building

```sh
PATH=/QOpenSys/pkgs/bin:$PATH

git clone git@github.com:IBM/iService-ibmi.git

cd iService

make

OR

make LIBRARY=yourlib DBGVIEW=*ALL

```
## Security Consideration

- The Makefile used to build up project leaves security configurations to the system administrator. Consider granting *USE to designated users/*PUBLIC and assigning the ownership to privileged user profile based on your own security policy.

- For using HTTP (REST) requests to exchange data with system, consider configuring IBM i HTTP server with secured socket layer to protect your sensitive information.

- The control for accessing the IPC server job is controlled by the authorities to the IPC key in IFS. If user profiles or their group profile running the client jobs have the access to the IPC key, then they can interact with the related IPC server job. *sbmjob is used to indicate under which user profile the server job will run when submitting IPC server jobs. Plan proper policy based on your own requirement in security and audit controls:

  * If expecting an IPC server job can only be accessed by a specific user, consider using *sbmjob(job:jobd:THIS_USER) to submit the IPC server job. Then IPC server job will run under THIS_USER and only serve client job requests from THIS_USER. THIS_USER better have no group profile assigned to share access with others.

  * If expecting the client jobs from a group of users to be served by the same IPC server job, consider using *sbmjob(job:jobd:THAT_USER). The group of users should be assigned with the same group profile. THAT_USER could be the group profile or one of the user profiles, under which you expect the submitted IPC server job is running.



## Tests
- CL 
```sh
> CALL ISERVICE/MAINTEST [/path/file.json|/path/*.json] [ctl_flags]
```
- nodejs
```sh
> node ./iService/test/nodejs/XXX.js
```

- python
```sh
> python3 ./test/python/XXX.py
```

- http requests
```sh
 ./iService/test/html/XXX.html
```

## License

[lgpl-2.1](LICENSE)

