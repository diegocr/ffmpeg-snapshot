 ____  ____  __  __  ____  ____  ___       ___  _  _    __    ____  ___  _   _  _____  ____ 
( ___)( ___)(  \/  )(  _ \( ___)/ __) ___ / __)( \( )  /__\  (  _ \/ __)( )_( )(  _  )(_  _)
 )__)  )__)  )    (  )___/ )__)( (_-.(___)\__ \ )  (  /(__)\  )___/\__ \ ) _ (  )(_)(   )(  
(__)  (__)  (_/\/\_)(__)  (____)\___/     (___/(_)\_)(__)(__)(__)  (___/(_) (_)(_____) (__) 


FFmpeg-Snapshot can be used to create thumbnails for Movies.

It is based on mplayer-snapshot (MPS from now on) which was created
back in 2009 by Alexandru (alextud) and available from:

http://code.google.com/p/mplayer-snapshot/


The main changes between them are:

- By default thumbnails are scaled to 16:9's 240x160 (MPS used 250x250)

- The minimum movie filesize by default is 10MB (100MB in MPS)

- You can change the JPEG Quality of the resulting image.

- It now accepts the formats .flv, .ts, .mp4, .mkv (and probably should me lots
  more, as supported by ffmpeg, but these are the common ones to myself..)

- You no longer need to provide full paths on the command-line arguments -d or -p
  using a dot will automatically translate it to the current directory.

- The way the movie file name/duration/etc are show on the resulting image have
  been completely changed, being the video/audio info coming as-is from ffmpeg

- Frames are extracted sequentially from the start to the end of the movie.. being
  the first frame calculated randomly

- Some other minimal internal changes...



FFmpeg-Snapshot requires the following libraries:

freetype6.dll
intl.dll
libcairo-2.dll
libexpat-1.dll
libfontconfig-1.dll
libgdk_pixbuf-2.0-0.dll
libgio-2.0-0.dll
libglib-2.0-0.dll
libgmodule-2.0-0.dll
libgobject-2.0-0.dll
libgthread-2.0-0.dll
libiconv-2.dll
libpng14-14.dll
zlib1.dll

You can get the very latest versions of them from:

http://www.gtk.org/download/win32.php


Also, you'll need ffmpeg.exe to be in your PATH (eg, C:\Windows\system32)

For the latest stable build of FFmpeg for Windows (32, or 64 bits) take a
look at http://ffmpeg.zeranoe.com/builds/


If you want to know how ffmpeg-snapshot was born, check:

http://ffmpeg.zeranoe.com/forum/viewtopic.php?f=12&t=66

Use that forum if you want to report/request/suggest something.

FFmpeg-Snapshot has been compiled using MinGW GCC 4.5.0 on Seven 32-bits
