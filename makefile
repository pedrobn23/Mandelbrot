.SUFFIXES:
.PHONY:    da, d, clean

CXXFLAGS := $(shell libpng-config --cflags)
LDFLAGS := $(shell libpng-config --ldflags)
compilador := mpicxx -std=c++11
flagsc     := -g -Wall

da: dibujo-asterisco_exe
	mpirun -np 4 ./$<

d: dibujo_exe
	mpirun -np 4 ./$<

dibujo_exe: dibujo.o
	$(compilador) $(flagsc) -o $@ $< $(LDFLAGS)

dibujo.o: dibujo.cpp 
	$(compilador) -c $(flagsc) $(CXXFLAGS) -o $@ $< 

%_exe: %.cpp
	$(compilador) $(flagsc) -o $@ $<

clean:
	rm -rf *_exe *.dSYM *~ *.o \#*
