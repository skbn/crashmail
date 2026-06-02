INCDIR = ../

CC = m68k-amigaos-gcc -noixemul -mcpu=68020 -Wall -I $(INCDIR) -DPLATFORM_AMIGA
AR = m68k-amigaos-ar -ru
RM = rm -f

OBJS = osfile.o osdir.o osmisc.o osmem.o ospattern.o os.o

oslib.a : $(OBJS) 
	$(AR)	oslib.a $(OBJS)

# os

osfile.o : osfile.c
	$(CC) -c osfile.c -o osfile.o
 
osmisc.o : osmisc.c
	$(CC) -c osmisc.c -o osmisc.o

osdir.o : osdir.c
	$(CC) -c osdir.c -o osdir.o

osmem.o : osmem.c
	$(CC) -c osmem.c -o osmem.o

ospattern.o : ospattern.c
	$(CC) -c ospattern.c -o ospattern.o

os.o : os.c
	$(CC) -c os.c -o os.o

clean :
	$(RM) *.o *.a
	
