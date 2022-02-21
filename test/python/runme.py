#!/QOpenSys/pkgs/bin/python3
# -*- coding:utf-8 -*-

import subprocess as subp
import json

def quote(s):
  return '\'' + s + '\''

parm_ctrl         = '*before(819)*after(819)'
parm_ctrl_ccsid   = '819'
parm_in           = json.dumps([{"cmd":{"exec": "cmd", "error": "on","value": "CHGFTPA AUTOSTART(*SAME)"}}])
parm_max_out_size = '32000'

iserv = "/usr/bin/iService " + \
          quote(parm_ctrl) + " " + \
          quote(parm_ctrl_ccsid) + " " + \
          quote(parm_in) + " " + \
          quote(parm_max_out_size)

print('** Running command: %s' %(iserv))

rc, out = subp.getstatusoutput(iserv)
if rc != 0:
  print('(E) rc = %d' %(rc))
else:
  try:
    jout = json.loads(out)
    print('STATUS = ', jout[0]['STATUS'])
  except Exception as err:
    print('(E) exception = ', str(err))

print(out)

