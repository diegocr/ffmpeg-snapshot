@REM	
@REM	Build script to compile ffmpeg-snapshot on WIN32 using MinGW
@REM	Copyright(c)2011 Diego Casorran <dcasorran@gmail.com>
@REM	
@REM	http://ffmpeg.zeranoe.com/forum/viewtopic.php?f=12&t=66
@REM	http://code.google.com/p/mplayer-snapshot/
@REM	http://www.gtk.org/download/win32.php
@REM	
@REM	Tested with gcc 4.5.0 and MinGW version 3.1.8(?)
@echo off

SET PROGRAM=ffmpeg-snapshot.exe
SET PCOPY=COPY /Y %PROGRAM% c:\windows\system32

SET DEVROOT=C:\MinGW
SET DEVBIN=%DEVROOT%\bin
SET DEVINC=%DEVROOT%\include
SET DEVLIB=%DEVROOT%\lib

SET COMPILER=%DEVBIN%\gcc.exe
SET STRIP=%DEVBIN%\strip.exe

SET GTK_INC=-I%DEVINC%\gtk-2.0 -mms-bitfields -I%DEVINC%\gtk-2.0 -I%DEVINC%\gtk-2.0\include
SET GTK_LIB=-L%DEVLIB% -lgtk-win32-2.0 -lgdk-win32-2.0 -lgobject-2.0 -lgthread-2.0
SET GLIB_INC=-I%DEVINC%\glib-2.0 -I%DEVLIB%\glib-2.0\include -mms-bitfields
SET GLIB_LIB=-L%DEVLIB% -lglib-2.0 -lintl
SET CAIRO_INC=-I%DEVINC%\cairo
SET CAIRO_LIB=-L%DEVLIB% -lcairo -lz
SET GDKPIX_INC=-I%DEVINC%\gdk-pixbuf-2.0 
SET GDKPIX_LIB=-L%DEVLIB% -lgdk_pixbuf-2.0

SET INC=%GTK_INC% %GLIB_INC% %CAIRO_INC% %GDKPIX_INC%
SET XFLAGS=-O3 -march=i686
SET CFLAGS=-c %XFLAGS% %INC% -W -Wall
SET LFLAGS=%XFLAGS% %GTK_LIB% %GLIB_LIB% %CAIRO_LIB% %GDKPIX_LIB% %GTK_LIB% -liberty

del *.o
del %PROGRAM%

"%COMPILER%" %CFLAGS% main.c

@REM Note that the -l libraries MUST come at the very end or linking will fail
"%COMPILER%" main.o -o %PROGRAM% %LFLAGS% -Wl,-L"%DEVLIB%",-lkernel32,-luser32,-lwinmm,-lws2_32
"%STRIP%" -s %PROGRAM%
%PCOPY%

echo Operation Completed
