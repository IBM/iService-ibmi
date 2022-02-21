**free
ctl-opt nomain;

dcl-ds myDS qualified;
  field1  int(10);
  dcl-ds myDS2;
    field2 char(10);
  end-ds;
end-ds;

dcl-proc TESTME export pgminfo(*yes);
  dcl-pi *N int(10);
    arg1      char(10);
    arg1b     char(10) value;
    arg2      int(5);
    arg2b     int(5) value;
    arg3      uns(5);
    arg3b     uns(5) value;    
    arg4      int(10);
    arg4b     int(10) value;
    arg5      uns(10);
    arg5b     uns(10) value;
    arg6      int(20);
    arg6b     int(20) value;
    arg7      uns(20);
    arg7b     uns(20) value;
    arg8      packed(10:4);
    arg8b     packed(10:4) value;
    arg9      zoned(10:4);
    arg9b     zoned(10:4) value;
    arg10     likeds(myDS);
  end-pi;

  %subst(arg1:1:1) = 'F';
  arg2 += 100;
  arg3 += 100;
  arg4 += 100;
  arg5 += 100;
  arg6 += 100;
  arg7 += 100;
  arg8 += 100;
  arg9 += 100;
  arg10.field1 += 100;
  %subst(arg10.myDS2.field2:1:1) = 'F';

  dsply (arg1b);
  dsply (%char(arg2b));
  dsply (%char(arg3b));
  dsply (%char(arg4b));
  dsply (%char(arg5b));
  dsply (%char(arg6b));
  dsply (%char(arg7b));
  dsply (%char(arg8b));
  dsply (%char(arg9b));
  dsply (%char(arg10.field1));
  dsply (arg10.myDS2.field2);

  %subst(arg1:1:1) = 'F';

  return 99;
end-proc;