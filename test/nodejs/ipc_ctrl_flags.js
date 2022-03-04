
const http = require('http')


let reqJson = 
[
  {"jobinfo":{"name":"","number":"","user":""}}
];

let input = JSON.stringify(reqJson);


if ( process.argv.length !== 7 ) {
  console.log('Usage: \n');
  console.log('node ./ipc_ctrl_flags.js <host> <port> <db2> <user> <pwd>\n');
  process.exit(1);
}

var host      = process.argv[2]; //'URHOST'
var port      = process.argv[3]; //'URPORT'
var db2       = process.argv[4]; //'*LOCAL' 
var uid       = process.argv[5]; //'GUEST'                                  // escape if needed
var pwd       = process.argv[6]; //'passw0rd';                              // escape if needed
 
var out_size  = 50000;

  /************** Test all the control flags parsing *********************************/
  var ctl =
  '*java*before(37)*after(819)*performance' + 
  '*ipc(/tmp/k1)*ipcsvr(ISERVICE/MAIN)*endipc' +
  '*waitserver(5)*waitclient(12)*waitdata(2)' +
  '*ipclog*sbmjob(ISERV:*USRPRF:GUEST)';


  let content = 'db2=' + db2 + '&uid=' + uid + '&pwd=' + pwd + 
  '&ctl=' + ctl + 
  '&in=' + escape(input) + 
  '&out=' + out_size;
  console.log('>>>> Post requsest = ' + input);

  let options = {
    hostname: `${host}`,
    port: `${port}`,
    path: '/ISERVICE/HTTPCGI.PGM',
    method: 'POST',
    headers:{
      'Content-Type' : 'application/x-www-form-urlencoded' ,
      'Content-Length' :content.length
    } 
  };

  const req = http.request(options, res => {
    console.log(`statusCode: ${res.statusCode}` + '\n');

    var data= '' ;
    res.on( 'data' , function (chunk){
        data += chunk;
    });
    res.on( 'end' , function (){
      process.stdout.write(data + '\n');
    }); 
  })

  req.write(content);

  req.on('error', error => {
    console.error(error + '\n');
  });

  req.end();



console.log('----- Got response ---------\n');
