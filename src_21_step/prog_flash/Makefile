CFLAGS = -Wall -O2

prog: prog.o
	gcc -o prog prog.o

.PHONY clean:
	-rm *.o prog
	
#-Wl,--gc-sections,--print-gc-sections
