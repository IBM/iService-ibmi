/*********************************************************
 * Run shell cmd before calling this nodejs to accept 
 * self-signed certificate.
 * > export NODE_TLS_REJECT_UNAUTHORIZED='0'
 *********************************************************/

const { Console } = require('console');
const { Http2ServerResponse } = require('http2');
const https = require('https')

const LoopCount = 100;

console.log('You may need to run > export NODE_TLS_REJECT_UNAUTHORIZED=\'0\'');

var jsons =new Array();

jsons.push([
  {"cmd":{"exec": "cmd", "error": "on","value": "CHGFTPA AUTOSTART(*SAME)"}},
  {"cmd":{"exec": "cmd", "error": "ignore","value": "DLTLIB NOEXT"}},
  {"cmd":{"exec": "system", "value": "CHGFTPA AUTOSTART(*SAME)"}},
  {"cmd":{"exec": "system", "error": "ignore", "value": "DLTLIB NOEXT"}},
  {"cmd":{"exec": "rexx", "value": "RTVUSRPRF USRPRF(*CURRENT) RTNUSRPRF(?) GRPPRF(?) CCSID(?N)"}},
  {"cmd":{"exec": "cmd", "error": "on","value": "DLTLIB NOEXT"}}
]);

jsons.push([
  {"jobinfo":{"name":"","number":"","user":""}}
]);

jsons.push([
  {"testSrc": "test/TESTPGM.c"},
  {
    "pgm": {
      "name": "TESTPGM",
      "lib": "ISERVICE",
      "parm": [
        {
          "io": "both",
          "var": "string",
          "type": "char(10)",
          "value": "value1"
        },
        {
          "io": "both",
          "var": "int",
          "type": "int(4)",
          "value": 4
        },
        {
          "io": "both",
          "var": "usinged long long",
          "type": "uint(8)",
          "value": 8
        },
        {
          "io": "both",
          "var": "float",
          "type": "float(3)",
          "value": 333.222
        },
        {
          "io": "both",
          "var": "double",
          "type": "double(5)",
          "value": 4444.333444
        },
        {
          "io": "both",
          "var": "hexidecimal",
          "type": "char(12)",
          "hex": "on",
          "value": "010203040506"
        },
        {
          "io": "both",
          "var": "ds",
          "type": "ds",
          "value": [
            { "var": "ent1", "type": "int(2)", "value": 3 },
            { "var": "ent2", "type": "char(20)", "value": "ent2_value" }
          ]
        }
      ],
      "return":{"value":-10000}
    }
  }
]);

jsons.push([
  {"sh":{"error": "on", "value": "ps -ef"}},
  {"qsh":{"error": "on", "value": "echo '123'"}}
]);

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



var db2       = '*LOCAL';
var uid       = 'GUEST2';                                                   // escape if needed
var pwd       = 'passw0rd';                                                 // escape if needed
var ctl       = '*ipc(/tmp/k1)*waitclient(300)*waitdata(3)*ipclog';         // escape if needed
var out_size  = 50000;


let i = 0;                
while ( i < LoopCount ) {
  ++i;

  /************** Change to your own settings *********************************/
  let options = {
    hostname: 'URHOST',
    port: 443,
    path: '/ISERVICE/HTTPCGI.PGM?',
    method: 'GET'
  };
  let input = JSON.stringify(jsons[i % jsons.length]);
  options.path += 'db2=' + db2 + '&uid=' + uid + '&pwd=' + pwd + 
                '&ctl=' + ctl + 
                '&in=' + escape(input) + 
                '&out=' + out_size;
  console.log('>>>> Requsest ' + i + ' : ' + input);

  const req = https.request(options, res => {
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
