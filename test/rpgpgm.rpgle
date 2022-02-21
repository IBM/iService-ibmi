**free
ctl-opt dftactgrp(*no);
ctl-opt main(RPGPGM);

dcl-pr cpybytes extproc('_CPYBYTES');
  dcl-parm target pointer value;
  dcl-parm source pointer value;
  dcl-parm length uns(10) value;
end-pr;

dcl-ds myDS qualified;
  field1  int(10);
  dcl-ds myDS2;
    ptr2   pointer;
    field2 char(10);
  end-ds;
end-ds;

dcl-s buff char(11) inz('abcdefghij');

dcl-proc RPGPGM;
  dcl-pi *N;
    arg1      char(10);
    arg2      int(5);
    arg3      uns(5);    
    arg4      int(10);
    arg5      uns(10);
    arg6      int(20);
    arg7      uns(20);
    arg8      packed(10:4);
    arg9      zoned(10:4);
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
    arg10.field1 += 100; // bin(4)
    cpybytes(arg10.myDS2.ptr2:%addr(buff):10); // char(10)
    %subst(arg10.myDS2.field2:1:10) = 'ABCDEFG   '; // char(10)

end-proc;