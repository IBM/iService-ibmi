BINDIR=/usr/bin
LIBRARY=iService
DBGVIEW=*ALL

LIBRARYUC=$(shell echo $(LIBRARY) | tr a-z A-Z)

COMMON=\
	FlightRec.module\
	Utility.module\
	Pase.module\
	ILE.module\
	INode.module\
	JNode.module\
	IAttribute.module\
	JAttribute.module\
	Runnable.module\
	Command.module\
	Shell.module\
	Qshell.module\
	Program.module\
	Argument.module\
	SqlUtil.module\
	JobUtil.module\
	Control.module\
	Ipc.module\
	JobInfo.module\
	IService.module

STOREDP_MODULES=Storedp.module $(COMMON)

all: $(LIBRARY).lib production test
production: $(LIBRARY).lib build-procedure build-cgi pase.pgm build-main
test: $(LIBRARY).lib MainTest.pgm TESTPGM.pgm TESTSRVPGM.srvpgm RPGPGM.pgm RPGSRVPGM.srvpgm
build-main: $(LIBRARY).lib Storedp.srvpgm Main.pgm
build-procedure: $(LIBRARY).lib Storedp.srvpgm Storedp.sqlinst
build-cgi: $(LIBRARY).lib HttpCgi.pgm

%.lib:
	(system -q 'CHKOBJ $* *LIB' || system -q 'CRTLIB $*') && touch $@
	sed 's/@ISERVICE@/$(LIBRARY)/g' src/Config.h.in > src/Config.h

%.srcpf: $(LIBRARY).lib
	(system -q 'CHKOBJ $(LIBRARY)/$* *FILE' || system -q 'CRTSRCPF $(LIBRARY)/$*') && touch $@

%.sqlinst: src/%.sql.in
	@sed 's/@LIBRARY@/$(LIBRARYUC)/g' $< > $(@:%.sqlinst=%).sql
	system -q "RUNSQLSTM SRCSTMF('$(@:%.sqlinst=%).sql')" && touch $@

%.sqluninst: src/%.sql.in
	@sed 's/@LIBRARY@/$(LIBRARYUC)/g' $< > $(@:%.sqlinst=%).sql
	-system -q "RUNSQLSTM SRCSTMF('$(@:%.sqlinst=%).sql')" 
 
%.module: src/%.sqlcpp
	system "CRTSQLCPPI OBJ($(LIBRARY)/$*) SRCSTMF('$<') DBGVIEW($(DBGBIEW)) REPLACE(*YES) CVTCCSID(*JOB) COMPILEOPT('INCDIR(''src/'') TGTCCSID(*JOB)')" > $*.log 2> $*.msg && rm $*.log $*.msg || touch $*.failed
	test ! -e $*.msg || cat $*.msg
	test ! -e $*.log || cat $*.log
	if [ -e $*.failed ]; then rm $*.failed; exit 1; fi
	touch $@

%.module: src/%.c
	system "CRTCPPMOD MODULE($(LIBRARY)/$*) SRCSTMF('$<') DBGVIEW($(DBGVIEW)) STGMDL(*SNGLVL) TERASPACE(*YES) REPLACE(*YES) INCDIR('./src') TGTCCSID(*JOB)" > $*.log 2> $*.msg && rm $*.log $*.msg || touch $*.failed
	test ! -e $*.msg || cat $*.msg
	test ! -e $*.log || cat $*.log
	if [ -e $*.failed ]; then rm $*.failed; exit 1; fi
	touch $@

Storedp.srvpgm: qsrvsrc.srcpf $(STOREDP_MODULES)
	system -q "CPYFRMSTMF FROMSTMF('src/$(@:%.srvpgm=%).bnd') TOMBR('/qsys.lib/$(LIBRARY).lib/qsrvsrc.file/$(@:%.srvpgm=%).mbr') MBROPT(*REPLACE)" && touch $@
	system -q "CRTSRVPGM ACTGRP(*CALLER) STGMDL(*SNGLVL) SRVPGM($(LIBRARY)/$(@:%.srvpgm=%)) MODULE($(STOREDP_MODULES:%.module=$(LIBRARY)/%)) EXPORT(*SRCFILE) SRCFILE($(LIBRARY)/QSRVSRC) " && touch $@
	if [ -e pase.pgm ]; then rm pase.pgm; fi

HttpCgi.pgm: HttpCgi.module $(COMMON)
	system -q "CRTPGM ACTGRP(*CALLER) PGM($(LIBRARY)/$(@:%.pgm=%)) MODULE($(^:%.module=$(LIBRARY)/%)) BNDSRVPGM(QHTTPSVR/QZSRCORE) " && touch $@

MainTest.module: test/MainTest.c
	system -q "CRTCPPMOD MODULE($(LIBRARY)/MainTest) SRCSTMF('$<') DBGVIEW($(DBGVIEW)) INCDIR('./src/') REPLACE(*YES) TGTCCSID(*JOB)" && touch MainTest.module

MainTest.pgm: MainTest.module $(COMMON)
	system -q "CRTPGM ACTGRP(*NEW) PGM($(LIBRARY)/$(@:%.pgm=%)) MODULE($(^:%.module=$(LIBRARY)/%))" && touch $@

TESTPGM.pgm: test/TESTPGM.c
	system -q "CRTBNDCPP PGM($(LIBRARY)/TESTPGM) SRCSTMF('$<') DBGVIEW($(DBGVIEW)) REPLACE(*YES) TGTCCSID(*JOB)" && touch $@

RPGPGM.pgm: test/RPGPGM.rpgle
	system -q "CRTBNDRPG PGM($(LIBRARY)/RPGPGM) SRCSTMF('$<') DBGVIEW($(DBGVIEW)) REPLACE(*YES) TGTCCSID(*JOB)" && touch $@

RPGSRVPGM.srvpgm: test/RPGSRVPGM.rpgle
	system -q "CRTRPGMOD MODULE($(LIBRARY)/RPGSRVPGM) SRCSTMF('$<') DBGVIEW($(DBGVIEW)) REPLACE(*YES) TGTCCSID(*JOB)" && touch RPGSRVPGM.module
	system -q "CRTSRVPGM SRVPGM($(LIBRARY)/RPGSRVPGM) MODULE($(LIBRARY)/RPGSRVPGM) EXPORT(*ALL) REPLACE(*YES)" && touch $@                                                       

TESTSRVPGM.srvpgm: test/TESTSRVPGM.c
	system -q "CRTCPPMOD MODULE($(LIBRARY)/TESTSRVPGM) SRCSTMF('$<') DBGVIEW($(DBGVIEW)) REPLACE(*YES) TGTCCSID(*JOB)" && touch TESTSRVPGM.module
	system -q "CRTSRVPGM SRVPGM($(LIBRARY)/TESTSRVPGM) MODULE($(LIBRARY)/TESTSRVPGM) EXPORT(*ALL) REPLACE(*YES)" && touch $@                                                                   

Main.pgm: Main.module
	system -q "CRTPGM ACTGRP(*CALLER) PGM($(LIBRARY)/$(@:%.pgm=%)) MODULE($(^:%.module=$(LIBRARY)/%)) BNDSRVPGM($(LIBRARY)/Storedp)" && touch $@

pase.pgm: src/pase/iService.c
	gcc -g $^ -o $(BINDIR)/$(LIBRARY) && touch $@
	chmod 777 $(BINDIR)/$(LIBRARY) && touch $@

clean: DropStoredp.sqluninst
	rm -f *.pgm *.srvpgm *.module *.lib *.sqlinst *.srcpf *.sql $(BINDIR)/$(LIBRARY) src/Config.h
	system -q 'CLRLIB $(LIBRARY)' || :
	system -q 'DLTLIB $(LIBRARY)' || :



