#/**
#*
#* @File : Makefile
#*
#*
#* @Synopsis : Makefile pour le Projet Systemes
#*
#**/
include ../include/INCLUDE_H
#
COMPILER = g++ -c -I$(INCLUDE) -Wall -O3 -Werror $*.cxx; 

proj.run : proj.o ProcDebug.o MiniDbg.o
	g++ -s -o proj.run proj.o ProcDebug.o MiniDbg.o -L../lib -lSys

proj.o : proj.cxx $(CEXC_H) $(NSSYSTEME_H)
	$(COMPILER)

ProcDebug.o : ProcDebug.cxx ../include/ProcDebug.h $(CEXC_H) $(NSSYSTEME_H)
	$(COMPILER)

MiniDbg.o : MiniDbg.cxx ../include/MiniDbg.h $(CEXC_H) $(NSSYSTEME_H)
	$(COMPILER)

#
# Nettoyage du repertoire courant : executables et fichiers .o
#
clean : 
	clear; rm -rf *.o *.run *~ ../include/*~
#




