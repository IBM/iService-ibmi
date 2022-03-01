#!/QOpenSys/pkgs/bin/bash
#set -x



if (( $# != 7 ))
then
  echo -e "perf.sh <host> <user> <pwd> <db2> <loop> <output{0,1}> <ipc{0,1}>\n";
  echo -e "* Ensure iService configured in http server.\n"
  exit;
fi

HOST=$1;
uid=$2;
pwd=$3;
db2=$4;
LOOP=$5;
OUTPUT=$6;
IPC=$7;




if (( ${IPC} == 1 ))
then
  ctl='*ipc(/tmp/k1)*waitclient(10)'
else
  ctl='';
fi

in='[{"cmd":{"exec": "system", "value": "CHGFTPA"}},{"cmd":{"exec": "rexx", "value": "RTVUSRPRF USRPRF(GUEST)  CCSID(?N) RTNUSRPRF(?) GRPPRF(?)"}},{"cmd":{"exec": "cmd", "error": "ignore","value": "DLTLIB NOEXT"}},{"sh":{"error": "on", "value": "pwd"}},{"qsh":{"error": "on", "value": "echo 123"}},{"sql":{"servermode":"off", "operation":[{"exec":{"parm":[{"io":"in", "value":"LOOPBACK"}],"value":"SELECT INTERNET FROM QUSRSYS.QATOCHOST WHERE HOSTNME1=?","fetch":{"block":"all"}}}]}},{"pgm": {"error": "ON", "name": "TESTPGM", "lib": "ISERVICE","parm": [{"io": "both", "type": "char(4)", "by" : "ref", "value": "fake"}]}}]';
out=3000

in=$(echo $in | sed 's/ /%20/g');
in=$(echo $in | sed 's/{/%7B/g');
in=$(echo $in | sed 's/}/%7D/g');
in=$(echo $in | sed 's/\[/%5B/g');
in=$(echo $in | sed 's/\]/%5D/g');

url="http://"${HOST}"/iService/httpcgi.pgm?db2=${db2}&uid=${uid}&pwd=${pwd}&ctl=${ctl}&in=${in}&out=${out}";

stime=$(date +"%s.%N");
i=0;
while (( ${i} < ${LOOP} ))
do
  (( i = ${i} + 1 ));
  echo -n ".";
  if (( ${OUTPUT} == 0 ))
  then
    curl -s ${url} >/dev/null;
  else
    curl -s ${url};
  fi

  rc=$?;
  if (( ${rc} != 0 ))
  then
    echo -e "(E)curl failed with rc = "${rc}
    break;
  fi

done
etime=$(date +"%s.%N")
echo -e "\n"

timediff=$(echo "${etime}-${stime}" | bc)
echo -e "** Elaped(s) = ${timediff}\n"
