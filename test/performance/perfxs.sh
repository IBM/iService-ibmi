#!/QOpenSys/pkgs/bin/bash
#set -x

if (( $# != 7 ))
then
  echo -e "perfxs.sh <host> <user> <pwd> <db2> <loop> <output{0,1}> <ipc{0,1}>\n";
  echo -e "* Ensure XMLService configured in http server.\n"
  exit;
fi

HOST=$1;
uid=$2;
pwd=$3;
db2=$4;
LOOP=$5;
OUTPUT=$6;
IPC=$7;




ipc='/tmp/k2';

if (( ${IPC} == 1 ))
then
  ctl='*sbmjob*idle(10/kill)';
else
  ctl='';
fi

ctl='*here';
in="<?xml version='1.0'?><cmd exec='system'>CHGFTPA</cmd><cmd exec='rexx'>RTVUSRPRF USRPRF(GUEST)  CCSID(?N) RTNUSRPRF(?) GRPPRF(?)</cmd><cmd exec='cmd'>CHGFTPA</cmd><sh>pwd</sh><qsh>echo '123'</qsh><sql><prepare>select INTERNET from qusrsys.qatochost where IPINTGER=?</prepare><execute><parm io='in'>1077952576</parm></execute><fetch></fetch></sql><pgm name='TESTPGM' lib='ISERVICE'><parm><data type='10A'>fake</data></parm></pgm>";
out=3000;

in=$(echo $in | sed 's/ /%20/g');
in=$(echo $in | sed 's/{/%7B/g');
in=$(echo $in | sed 's/}/%7D/g');
in=$(echo $in | sed 's/\[/%5B/g');
in=$(echo $in | sed 's/\]/%5D/g');

url="http://"${HOST}"/cgi-bin/xmlcgi.pgm?db2=${db2}&uid=${uid}&pwd=${pwd}&ipc=${ipc}&ctl=%22${ctl}%22&xmlin=%22${in}%22&xmlout=${out}";

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