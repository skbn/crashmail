
                                 CrashMail II

                             The Next Generation!

                      ...a stranger in a strange land...


Added AmigaOS 3.2 support and crashedit an editor to jamlib using jamlib.a from crashmail.
Crashedit based on golded+ but with basic functionalities: area, messages, reader and editor, plus an ansi art viewer and file request.

Added tag "aso" to crashmail config file to work in aso mode.

```
**For CrashEdit documentation, see [crashedit/README.md](crashedit/README.md)**
**For CrashEdit documentation in Spanish, see [crashedit/README_ESP.md](crashedit/README_ESP.md)**
```

This project was inspired by Golded+ [https://github.com/golded-plus/golded-plus]. While it served as a reference during development, the implementation in this repository was written independently and does not contain code copied from the original project.

============
Introduction
============
Welcome to CrashMail II! CrashMail II is basically a more portable version
of CrashMail, a tosser for Amiga computers. Users of the old Amiga
version will probably find some things familiar while some features are
gone such as the ARexx port (for obvious reasons!) and the GUI
configuration editor. The only feature that CrashMail II has and the old
CrashMail hasn't is support for JAM messagebases.

Homepage:   http://ftnapps.sourceforge.net/crashmail.html
Code:       http://sourceforge.net/p/ftnapps/crashmail/code/
Downloads:  http://sourceforge.net/projects/ftnapps/files/crashmail/


=========
Copyright
=========

Copyright (C) 1998-2004, Johan Billing <billing@df.lth.se>
Copyright (C) 1999-2010, Peter Krefting <peter@softwolves.pp.se>
Copyright (C) 2009-2016, Robert James Clay <jame@rocasa.us>
Copyright (C) 2016-2016, Lars Kellogg-Stedman <lars@oddbit.com>
Copyright (C)      2016, Niklas Lindholm <niklas@nilin.se>

JAMLIB is copyright (c) 1999 Björn Stenberg. JAMLIB is released under the
GNU Lesser General Public License, See src/jamlib/jamlib.doc for more
information.

tests/roundup is copyright (c) 2010 Blake Mizerany - MIT License

Except where explicitly stated otherwise, all other parts of CrashMail are
copyright 1998-2004 Johan Billing. Permission to use, copy and distribute
CrashMail is granted provided that this copyright notice is included. Permission
to modify CrashMail is granted. Distributing modified versions of CrashMail is
allowed provided that the documentation clearly states that it is a modified
version. Parts of CrashMail may be freely used in other projects as long as
the documentation mentions the original copyright holder.


================
Acknowledgements
================
Many thanks to Björn Stenberg for creating the excellent subroutine library
JAMLIB which CrashMail uses for handling JAM messagebases.

Thanks for Peter Karlsson for porting CrashMail II to OS/2 and the man pages.

Thanks to Lars Kellogg-Stedman for the testing scripts as well as the other
improvements he has made.


=============
Documentation
=============
The documentation is very brief and CrashMail probably isn't the ideal
choice for Fidonet beginners. All documentation of the available keywords
in the configuration file can be found in the doc/example.prefs file, and
other information can be found in the doc/ReadMe.txt file..


=========
Platforms
=========
This version of CrashMail can be compiled for Win32, Linux and OS/2; see the
INSTALL file for details. If you are interested in running CrashMail on another
platform, please contact me if you are willing to do the work necessary to adapt
CrashMail to your platform. The amount of work required mostly depends on whether
your C-compiler supports some common POSIX-functions which CrashMail uses.

=========
Screenshots
=========

```
For AmigaOS the program use ttengine or freetype with libpng and zlib

https://aminet.net/package/util/libs/ttengine-68k

libpng: https://www.libpng.org/
zlib: https://zlib.net/
FreeType: https://freetype.org/

To compile:

In crashmail directory:
make cleanamiga
make amiga

In crashedit directory:

To ttengine.library: make -f Makefile.amiga

To static freetype with libpng and zlib:
Extract the files freetype-2.14.3.tar.xz, libpng-1.6.58.tar.xz and, zlib.tar.gz
into CrashEdit and rename them to freetype, zlib, and libpng.

To prepair headers:
make -f Makefile.amiga.te unprep
make -f Makefile.amiga.te prep
make -f Makefile.amiga.te clean all

For freetype, the antialiasing is still buggy; I need to fix it.

The executable is large, but you don't need any libraries. It's optimized for RTG and also works with OCS, ECS, or AGA.
```

![AmigaOS 3.2](img/amiga.png)

![FreeBSD](img/freebsd.png)

![Linux](img/linux.png)

![Ansi Art support](img/ansi%20art.png)

![TTF support](img/ttf%20amigaos.png)

![ansi art ttf](img/ttf%20utf8%20ansi.png)
