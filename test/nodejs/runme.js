const progress = require('child_process');

function quote(str)
{
  return "\'" + str + "\'";
}

let parm_ctrl         = '*before(819)*after(819)';
let parm_ctrl_ccsid   = 819;
let parm_in           = [{"cmd":{"exec": "cmd", "error": "on","value": "CHGFTPA AUTOSTART(*SAME)"}}];
let parm_max_out_size = 32000;

let run = 'iService ' 
            + quote(parm_ctrl) + ' '
            + quote(parm_ctrl_ccsid) + ' '
            + quote(JSON.stringify(parm_in)) + ' '
            + quote(parm_max_out_size);
            

console.log("Running command = \n" + run);

progress.exec(run, (err, stdout, stderr) => {
  if (err) {
    console.error("Failed to run cmd with error = \n" + err);
    return;
  }

  if (stderr) {
    console.error("Cmd failed with stderr = \n" + err);
    return;
  }

  let result = {};
  try{
    result = JSON.parse(stdout);
    console.log("Output is = \n" + JSON.stringify(result));
  } catch(e)
  {
    console.log("Invalid JSON format = \n" + stdout);
    console.log("Failed with parse output as JSON with error = \n" + e);
  }
});