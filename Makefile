# type either "make linux", "make win32", or "make os2" to compile

help:
	@echo "You can use this Makefile in the following ways:"
	@echo "make linux ............ Make Linux binaries"
	@echo "gmake bsd .............. Make BSD binaries"
	@echo "make win32 ............ Make Win32 binaries"
	@echo "make osx .............. Make OS/X binaries"
	@echo "make os2 .............. Make OS/2 binaries"
	@echo "make amiga ............ Make AmigaOS binaries"
	@echo "make crossmingw32 ..... Make Windows 64-bit binaries (cross-compiled from Linux)"
	@echo "make crossmingw32-32 .. Make Windows 32-bit binaries (cross-compiled from Linux)"
	@echo "make crossmingw32-all . Make Windows 32+64-bit crashmail binaries (cross-compiled from Linux)"
	@echo "make cleanlinux ....... Remove object files under Linux"
	@echo "gmake cleanbsd ......... Remove object files under BSD"
	@echo "make cleanwin32 ....... Remove object files under Win32"
	@echo "make cleanosx ......... Remove object files under OS/X"
	@echo "make cleanos2 ......... Remove object files under OS/2"
	@echo "make cleanamiga ....... Remove object files under AmigaOS"
	@echo "make cleancrossmingw32 Remove object files for cross-compilation"
	@echo "make tests ............ Run Tests (requires /bin/sh)"

linux :
	mkdir -p bin
	make CFLAGS="-std=gnu11" -C src -f Makefile linux

bsd :
	mkdir -p bin
	gmake CFLAGS="-std=gnu11" -C src -f Makefile bsd

win32 :
	make -C src -f Makefile win32

osx :
	make -C src -f Makefile osx

os2 :
	make -C src -f Makefile os2

amiga :
	mkdir -p bin
	make -C src -f Makefile amiga

crossmingw32 :
	mkdir -p bin
	make -C src -f Makefile.crossmingw32 ARCH=win64

crossmingw32-32 :
	mkdir -p bin
	make -C src -f Makefile.crossmingw32 ARCH=win32

crossmingw32-all :
	mkdir -p bin
	make -C src -f Makefile.crossmingw32 ARCH=win64 files
	make -C src -f Makefile.crossmingw32 clean
	make -C src -f Makefile.crossmingw32 ARCH=win32 files

cleanlinux :
	rm -rf bin
	make -C src -f Makefile cleanlinux

cleanbsd :
	rm -rf bin
	gmake -C src -f Makefile cleanbsd

cleanwin32 :
	make -C src -f Makefile cleanwin32

cleanosx :
	make -C src -f Makefile cleanosx

cleanos2 :
	make -C src -f Makefile cleanos2

cleanamiga :
	rm -rf bin
	make -C src -f Makefile cleanamiga

cleancrossmingw32 :
	rm -rf bin
	make -C src -f Makefile.crossmingw32 clean

.PHONY: tests
tests: linux
	make -C tests

