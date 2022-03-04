
const http = require('http')

var jsons =new Array();

// #1
jsons.push([
  {"cmd":{"exec": "cmd", "error": "on","value": "CHGFTPA AUTOSTART(*SAME)"}},
  {"cmd":{"exec": "cmd", "error": "ignore","value": "DLTLIB NOEXT"}},
  {"cmd":{"exec": "system", "value": "CHGFTPA AUTOSTART(*SAME)"}},
  {"cmd":{"exec": "system", "error": "ignore", "value": "DLTLIB NOEXT"}},
  {"cmd":{"exec": "rexx", "value": "RTVUSRPRF USRPRF(*CURRENT) RTNUSRPRF(?) GRPPRF(?) CCSID(?N)"}},
  {"cmd":{"exec": "cmd", "error": "on","value": "DLTLIB NOEXT"}}
]);
// #2
jsons.push([
  {"jobinfo":{"name":"","number":"","user":""}}
]);
// #3
jsons.push([
  {
    "pgm": {
      "name": "QWDRJOBD",
      "lib": "*LIBL",
      "parm": [
        {
          "io": "out",
          "var": "Receiver variable",
          "type": "ds",
          "value": [
            { "var": "Bytes returned", "type": "int(4)", "value": 0},
            { "var": "Bytes available", "type": "int(4)", "value": 0},
            { "var": "Padding", "type": "char(20)", "hex": "on", "value": "" },
            { "var": "User name", "type": "char(10)", "value": "" },
            { "var": "Padding", "type": "char(16)", "hex": "on", "value": "" },
            { "var": "Job queue name", "type": "char(10)", "value": "" },
            { "var": "Padding", "type": "char(64)", "hex": "on", "value": "" }
          ]
        },
        {
          "io": "in",
          "var": "Length of receiver variable",
          "type": "int(4)",
          "value": 128
        },
        {
          "io": "in",
          "var": "Format name",
          "type": "char(8)",
          "value": "JOBD0100"
        },
        {
          "io": "in",
          "var": "Qualified job description name",
          "type": "char(20)",
          "value": "QDFTJOBD  *LIBL     "
        },
        {
          "io": "both",
          "var": "Error code",
          "type": "ds",
          "value": [
            { "var": "Bytes_Provided", "type": "int(4)", "value": 16 },
            { "var": "Bytes_Available", "type": "int(4)", "value": 0 },
            { "var": "Exception_Id", "type": "char(7)", "value": "" },
            { "var": "Padding", "type": "char(1)", "hex": "on", "value": "" }
          ]
        }
      ],
      "return":{"value":0}
    }
  }
]);
// #4
jsons.push([
  {"sh":{"error": "on", "value": "ps -ef"}},
  {"qsh":{"error": "on", "value": "echo '123'"}}
]);
// #5
jsons.push([
  {
    "sql": {
       "error":"on",
       "operation":[
        {"execDirect": {"value":"SELECT * FROM QUSRSYS.QATOCHOST",
                        "fetch":{"block":"all"}}}      
       ]
    }
  },
  {
    "sql": {"free": ["all"]}
  }
]);


if ( process.argv.length !== 9 ) {
  console.log('Usage: \n');
  console.log('node ./ipc_requests.js <host> <port> <db2> <user> <pwd> <ctrl_flags> <loop_count>\n');
  process.exit(1);
}

var host      = process.argv[2]; //'URHOST'
var port      = process.argv[3]; //'URPORT'
var db2       = process.argv[4]; //'*LOCAL' 
var uid       = process.argv[5]; //'GUEST'                                  // escape if needed
var pwd       = process.argv[6]; //'passw0rd';                              // escape if needed
var ctl       = process.argv[7]; //'*ipc(/tmp/k1)*ipcwait(600)*ipclog'      // escape if needed
var LoopCount = process.argv[8];

var out_size  = 50000;


let i = 0;                
while ( i < LoopCount ) {
  ++i;

  /************** Change to your own settings *********************************/
  let options = {
    hostname: `${host}`,
    port: `${port}`,
    path: '/ISERVICE/HTTPCGI.PGM?',
    method: 'GET'
  };
  let input = JSON.stringify(jsons[i % jsons.length]);
  options.path += 'db2=' + db2 + '&uid=' + uid + '&pwd=' + pwd + 
                '&ctl=' + ctl + 
                '&in=' + escape(input) + 
                '&out=' + out_size;
  console.log('>>>> Requsest ' + i + ' : ' + input);

  const req = http.request(options, res => {
    console.log(`statusCode: ${res.statusCode}` + '\n');

    res.on('data', d => {
      process.stdout.write(d + '\n');
    });
  })

  req.on('error', error => {
    console.error(error + '\n');
  });

  req.end();

}

console.log('----- Sent all requests ---------\n');
