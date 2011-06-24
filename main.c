/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Alexandru 2007 <alextud@gmail.com>
 * Copyright (C) 2011 Diego Casorran <dcasorran@gmail.com>
 * 
 * main.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * main.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with main.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <math.h>
#include <stdlib.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

//default value
static gint WIDTH = 240;
static gchar *WIDTH_STR = "240";
static gchar *QUALITY = "85";
static gint HEIGHT = 160;
static gchar *HEIGHT_STR = "160";
static gboolean DISABLE_TIME = FALSE;
static gboolean BEQUIET = FALSE;
static gint ROWS = 8;
static gint COLUMNS = 4;
static gchar *PATH = NULL;
static gulong MIN_SIZE = 10 * 1024 * 1024; // > 10 MB

static gchar *TEMP_DIR = NULL;
static GPid CHILD_PID;
static GIOChannel *INPUT_CH;
static GIOChannel *OUTPUT_CH;
static GList *movie_list = NULL;
static gchar **fileinfo = NULL, **fip;
static gint _duration = 0;
static struct stat current_movie;

#define PATH_SEPARATOR "\\"
#define remove unlink

static gboolean wait_for_ffmpeg_to_load ()
{
	GError *error = NULL;
	gchar *temp = NULL;
	
	g_io_channel_read_line (OUTPUT_CH, &temp, NULL, NULL, &error);
	g_free (temp);
	
	// if(!BEQUIET)g_print(error?"error loading ffmpeg\n":"ffmpeg loaded\n");
	return !error;
}

static gboolean ffmpeg_spawn (gchar *filename, int start_time)
{
	gchar *argvm[16], **ap = argvm;
	
	*ap++ = "ffmpeg.exe";
	*ap++ = "-i";
	*ap++ = filename;
	*ap++ = "-an";
	*ap++ = "-vcodec";
	*ap++ = "png";
	*ap++ = "-vframes";
	*ap++ = "1";
	if(start_time) {
		gchar ss[12], vf[20], fmt[1024];
		
		snprintf(ss,sizeof(ss),"%d",start_time);
		*ap++ = "-ss";
		*ap++ = ss;
		
		snprintf(vf,sizeof(vf),"scale=%d:%d", atoi(WIDTH_STR), atoi(HEIGHT_STR) );
		*ap++ = "-vf";
		*ap++ = vf;
		
		snprintf(fmt,sizeof(fmt),g_strconcat(TEMP_DIR,".%06d.png", NULL),start_time);
		*ap++ = "-y";
		*ap++ = fmt;
	}
	*ap++ = NULL;
	
	// {int i;if(!BEQUIET)g_print("\n Executing: ");for(i = 0; argvm[i]; ++i)if(!BEQUIET)g_print("%s ",argvm[i]);if(!BEQUIET)g_print("\n");}
	
	GError *error = NULL;
	gint input_fd;
	gint output_fd;
	
	g_spawn_async_with_pipes (NULL,
							  argvm, NULL,
							  // G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
							  G_SPAWN_SEARCH_PATH,
							  NULL, NULL,
							  &CHILD_PID, 
							  &input_fd, &output_fd,&output_fd,// NULL,
							  &error);
	
	if (error)	{
		if(!BEQUIET)g_print ("g_spawn_async_with_pipes error #%d: %s\n", error->code, error->message);
		return FALSE;
	}
	
	INPUT_CH = g_io_channel_unix_new (input_fd);
	OUTPUT_CH = g_io_channel_unix_new (output_fd);
	
	return TRUE;
}

static int supportedFormat(char *filename) {
	static const char *fmts[] = {".mpeg",".flv",".wmv",".ts",".mov",".mp4",".mkv",".mpg",".avi"};
	int x = sizeof(fmts)/sizeof(char *);
	while(x--)
		if(g_str_has_suffix(filename,fmts[x]))
			return 1;
	return 0;
}

static void add_movies_from_directory (gchar *directory) 
{
	GError *error = NULL;
	GDir *dir = g_dir_open (directory, 0, &error);
	if (error)		return;
	
	// if(!BEQUIET)g_print ("\n directorul %s", directory);
	
	struct stat buf;
	gchar *filename = (gchar *)g_dir_read_name (dir);
	
	for (; filename; filename = (gchar *)g_dir_read_name (dir))
	{
		filename = g_strconcat (directory, PATH_SEPARATOR, filename, NULL);
		// if(!BEQUIET)g_print ("\n filename %s", filename);
		
		if (g_file_test (filename, G_FILE_TEST_IS_DIR))
			add_movies_from_directory (filename);
		
		if (supportedFormat(filename))
		{		
			stat (filename, &buf);
			
			if ((gulong)buf.st_size > MIN_SIZE )
			{
				movie_list = g_list_prepend (movie_list,
											 g_filename_to_utf8 (filename, 
																 -1,
																 NULL, NULL, NULL));
				// if(!BEQUIET)g_print ("\n added movie %s", filename);
			}
		}
		
		g_free (filename);
	}
	
	g_dir_close (dir);
}

static gboolean parse_input (int argc, char *argv[])
{
	if (argc < 2)
		return FALSE;
	
	int i;
	for (i = 1; i < argc; i++)
	{
		if (g_str_has_prefix (argv[i], "-w")) //width
		{
			if (argc <= i + 1)	return FALSE;
			WIDTH_STR = g_strdup (argv[++i]);
			WIDTH = atoi (argv[i]);
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-h")) //height
		{
			if (argc <= i + 1)	return FALSE;
			HEIGHT_STR = g_strdup (argv[++i]);
			HEIGHT = atoi (argv[i]);
			continue;
		}
			
		if (g_str_has_prefix (argv[i], "-q")) //quality
		{
			if (argc <= i + 1)	return FALSE;
			QUALITY = g_strdup (argv[++i]);
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-n")) //notime
		{
			DISABLE_TIME = TRUE;
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-z")) //quiet
		{
			BEQUIET = TRUE;
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-r"))//rows
		{
			if (argc <= i + 1)	return FALSE;
			ROWS = atoi (argv[++i]);
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-c"))//columns
		{
			if (argc <= i + 1)	return FALSE;
			COLUMNS = atoi (argv[++i]);
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-p"))//path
		{
			char *path;
			if (argc <= i + 1)	return FALSE;
			path = argv[++i];
			if(path[0] == '.' && !path[1])
				path = g_get_current_dir ();

			if (g_path_is_absolute (path))
				PATH = g_strdup (path);
			else
				PATH = g_strconcat (g_get_current_dir (), PATH_SEPARATOR,
													  argv[i], NULL);
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-d"))//dir
		{
			char *path;
			if (argc <= i + 1)	return FALSE;
			path = argv[++i];
			if(path[0] == '.' && !path[1])
				path = g_get_current_dir ();
			add_movies_from_directory (path);
			continue;
		}
		
		if (g_str_has_prefix (argv[i], "-s"))//minimum size for movie
		{
			if (argc <= i + 1)	return FALSE;
			MIN_SIZE = atoi (argv[++i]) * 1024 * 1024;
			continue;
		}
		
		if (g_path_is_absolute (argv[i]))
			movie_list = g_list_prepend (movie_list, argv[i]);
		else
			movie_list = g_list_prepend (movie_list,
										 g_strconcat (g_get_current_dir (), PATH_SEPARATOR,
													  argv[i], NULL));
	}

	return TRUE;
}


static void show_help ()
{
	g_print("\nffmpeg-snapshot V0.1 Copyright(c)2011 Diego Casorran <dcasorran@gmail.com>");
	g_print("\nBased on mplayer-snapshot Copyright (c) Alexandru 2007 <alextud@gmail.com>\n");
	
	if(!BEQUIET)g_print ("\nUsage : ffmpeg-snapshot [options] filename");
	if(!BEQUIET)g_print ("\n -w <width value>");
	if(!BEQUIET)g_print ("\n -h <height value>");
	if(!BEQUIET)g_print ("\n -n                Do not show time");
	if(!BEQUIET)g_print ("\n -r <rows>         Number of rows");	
	if(!BEQUIET)g_print ("\n -c <columns>      Number of colums");
	if(!BEQUIET)g_print ("\n -p <path>         Where to save images - Use a . (dot) for current directory");
	if(!BEQUIET)g_print ("\n -d <directory>    For all movies from directory - Use a . (dot) for current directory");
	if(!BEQUIET)g_print ("\n -s <value>        Minimum size in MB");
	if(!BEQUIET)g_print ("\n -z                Be quiet");
	if(!BEQUIET)g_print ("\n -q <value>        JPEG Quality");
	if(!BEQUIET)g_print ("\n\nNOTE: ffmpeg.exe must be in your PATH (eg, system32) - get the last");
	if(!BEQUIET)g_print ("  \n      version for win32/win64 from http://ffmpeg.zeranoe.com/builds/\n");
}

/* static cairo_text_extents_t *cairo_calc_text_extents(cairo_t *cr,gchar *font_name,gint font_size,gchar *str)
{
	static cairo_text_extents_t extents;
	
	cairo_select_font_face(cr, font_name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents(cr, str, &extents);
	
	return &extents;
}
 */

static char *formatBytes(unsigned long long b) {
	static char fmt[32];
	float kb = 1024, mb = 1024*kb, gb = 1024*mb;
	
	if(b > gb) {
		snprintf(fmt,sizeof(fmt),"%.2fGb ",(float)(b/gb));
	}
	else if(b > mb) {
		snprintf(fmt,sizeof(fmt),"%.2fMb ",(float)(b/mb));
	}
	else if(b > kb) {
		snprintf(fmt,sizeof(fmt),"%.2fKb ",(float)(b/kb));
	}
	else fmt[0] = 0;
	return fmt;
}

static void 
// create_png_with_cairo (gchar *movie_name, gchar **info, gint frame_start, gint frame_step)
create_png_with_cairo (gchar *movie_name, int frame_start, int frame_step)
{
	int i,j, k;
	gint x_offset = 5, y_offset_c = 2, y_offset = 75, hh, mm, ss;
	gchar temp[200], *longer_line = NULL;;
	// cairo_text_extents_t *cExtents;
	
	for( j = 0, fip = fileinfo ; *fip ; fip++, ++j ) {
		if(!longer_line || strlen(*fip) > strlen(longer_line))
			longer_line = *fip;
	}
	
	y_offset = 50 + ((COLUMNS < 4 ? 12:14) * j);
	
	cairo_surface_t *image;
	gint image_width = (WIDTH + x_offset) * COLUMNS + 4;
	gint image_height = (HEIGHT + x_offset) * ROWS + y_offset;
	image = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 
										image_width,
										image_height);
	
	cairo_t *cr = cairo_create (image);
	
	//stergem background
	cairo_set_source_rgb (cr, 1.0f, 1.0f, 1.0f);
	cairo_paint (cr);
	
	
	//--------scriem numele filmului------
    cairo_font_options_t* font;
    
    font = cairo_font_options_create ();
    cairo_font_options_set_antialias (font, CAIRO_ANTIALIAS_SUBPIXEL);
	//cairo_font_options_set_antialias (font, CAIRO_ANTIALIAS_GRAY);
    cairo_set_font_options (cr, font);
	
	cairo_set_source_rgb (cr, 0.0f, 0.0f, 0.0f);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_SLANT_ITALIC);
    cairo_set_font_size(cr, 16);
	
	cairo_text_extents_t extents;
	movie_name = g_strconcat ("File Name: ", movie_name, NULL);
	cairo_text_extents (cr, movie_name, &extents);
	// cairo_move_to (cr, image_width / 2 - extents.width / 2, y_offset / 3 - 5);
	y_offset_c += extents.height;
	cairo_move_to (cr, x_offset, y_offset_c);
	cairo_show_text (cr, movie_name);
	
	ss = (int) _duration % 60;
	mm = (int) _duration / 60;
	mm = (int) mm % 60;
	hh = (int) _duration / 3600;
	snprintf(temp,sizeof(temp),"Length: %02d:%02d:%02d %s(%lu bytes)",hh,mm,ss,formatBytes(current_movie.st_size),current_movie.st_size);
	cairo_text_extents (cr, temp, &extents);
	// cairo_move_to (cr, image_width / 2 - extents.width / 2, y_offset / 3 - 5);
	y_offset_c += extents.height +4;
	cairo_move_to (cr, x_offset, y_offset_c);
	cairo_show_text (cr, temp);
	
	//scriem si restul proprietatilor
	cairo_select_font_face(cr, "Tahoma", CAIRO_FONT_SLANT_NORMAL,
						   CAIRO_FONT_WEIGHT_NORMAL);
	gint font_size = 14;
	do { //calculam dimensiunea fontului
		cairo_set_font_size (cr, font_size);
		// cairo_text_extents (cr, "Resolution: 1000x1000", &extents);
		cairo_text_extents (cr, longer_line, &extents);
		font_size-=2;
		// g_print("line h %d\n",(int)extents.height);
	}while (extents.width /* + x_offset */ > image_width);
	cairo_set_font_size (cr, font_size+2);
	
/* 	for (k = 0, i = 0; i < 3; i++)
		for (j = 0; j < 3 ; j++, k++)
	{
		cairo_move_to (cr, i * (image_width / 3) + x_offset,
					   j * (extents.height + 5) + 15 + y_offset / 3);
		cairo_show_text (cr, info[k]);
	} */
	
	for( fip = fileinfo, i = 0 ; *fip ; fip++ ) {
		y_offset_c += (extents.height + 4);
		
		cairo_move_to (cr, x_offset, y_offset_c);
		cairo_show_text (cr, *fip);
	}
	cairo_set_font_size (cr, 14);
	
	// g_print("y_offset_c: %d, diff: %d",y_offset_c,y_offset-y_offset_c);
	if(y_offset-y_offset_c < 1)
		if(!BEQUIET)g_print("WARNING: text may overlap... (%d)\n",y_offset-y_offset_c);
	
	cairo_surface_t *temp_image;
	gchar filename[1500];
	gint frame_pos;
	
	gint text_length = 0;
	cairo_text_extents (cr, "00:00:000", &extents);
	text_length = extents.width;
	
	
	//------------load images --------
	for (j = 0, k = 0, frame_pos = frame_start; j < ROWS; j++)
		for (i = 0; i < COLUMNS; i++, frame_pos += frame_step, k++)
	{
		// sprintf (filename, "%08d.png", frame_pos);
		snprintf(filename,sizeof(filename),g_strconcat(TEMP_DIR,".%06d.png", NULL),frame_pos);
		
	// g_usleep(10*1000);
		temp_image = cairo_image_surface_create_from_png (filename);
	// g_print("filename: %lx %s\n",(long)temp_image,filename);
		cairo_set_source_surface (cr, temp_image,
								  i * (WIDTH + x_offset) + x_offset,
					   			  j * (HEIGHT + x_offset) + y_offset);
		
		cairo_paint (cr);
		cairo_surface_destroy (temp_image);
		
		//-----write time
		if (! DISABLE_TIME)
		{
			ss = (int) frame_pos % 60;
			mm = (int) frame_pos / 60;
			mm = (int) mm % 60;
			hh = (int) frame_pos / 3600;
			sprintf (temp, "%02d:%02d:%02d", hh, mm, ss);

			cairo_move_to (cr, 
						   i * (WIDTH + x_offset) + x_offset + WIDTH - extents.width,
						   j * (HEIGHT + x_offset) + y_offset + HEIGHT - 5);
			
			cairo_set_source_rgb (cr, 0.9f, 0.9f, 0.9f);
			cairo_show_text (cr, temp);
		}
	}
	
	cairo_surface_write_to_png (image,g_strconcat(TEMP_DIR,".final.png", NULL));
	cairo_surface_destroy (image);
	cairo_destroy (cr);	
	cairo_font_options_destroy(font);
}

static void create_jpg (gchar *movie_path)
{
	char *p;
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;
	pixbuf = gdk_pixbuf_new_from_file (g_strconcat(TEMP_DIR,".final.png", NULL), &error);
	if (error)
		if(!BEQUIET)g_print ("failed to load %s",g_strconcat(TEMP_DIR,".final.png", NULL));
	
	gchar *image_path;
	gchar *movie_name;
	gchar *image_name;
	
	movie_name = g_strdup(g_path_get_basename (movie_path));
	if((p=strrchr(movie_name,'.')))
		*p = 0;
	// image_name = g_strconcat (g_strndup (movie_name, strlen (movie_name) - 3), "jpg", NULL);
	image_name = g_strconcat(movie_name, ".jpg", NULL);
	
	if (PATH)
		image_path = g_strconcat (PATH, PATH_SEPARATOR, image_name, NULL);
	else
		image_path = g_strconcat (g_path_get_dirname (movie_path), PATH_SEPARATOR,
								  image_name, NULL);
//		image_path = g_strconcat (OLD_DIR, PATH_SEPARATOR, 
//								  image_name, NULL);

	gdk_pixbuf_save (pixbuf, image_path, "jpeg", &error,
                 "quality", QUALITY, NULL);
	
	if (error)
		if(!BEQUIET)g_print ("failed to save %s\n%s\n", image_path, error->message);
	
	gdk_pixbuf_unref (pixbuf);
	
	g_free (image_path);
	g_free (movie_name);
	g_free (image_name);
}

static void remove_frames(/* gint frame_start, gint nr_frames */)
{
/* 	gchar filename[15];
	int i = 0;
	for (i = frame_start - 4; i <= nr_frames; i++)
	{
		sprintf (filename, "%08d.png", i);
		remove (filename);
	}
	
	remove ("image.png"); */
	GError *error = NULL;
	gchar *directory = (gchar *)g_get_tmp_dir(), *filename;
	GDir *dir = g_dir_open (directory, 0, &error);
	if (error)		return;
	
	while((filename = (gchar *)g_dir_read_name (dir)))
	{
		if(strncmp(filename,"ffmpeg-snapshot.",16))
			continue;
		
		unlink(g_strconcat (directory, PATH_SEPARATOR, filename, NULL));
	}
	g_dir_close (dir);
}

static int run_ffmpeg(gchar *movie,int ss,int (*cb)(gchar *)) {
	int done = FALSE;
	
	if(! ffmpeg_spawn(movie,ss))
		return 2;
	if(! wait_for_ffmpeg_to_load ())
		return 3;
	
	while(!done) {
		GError *error = NULL;
		gchar *temp = NULL;
		
		g_io_channel_read_line (OUTPUT_CH, &temp, NULL, NULL, &error);
		
		if (error) //error on conversion 
		{
			if(!BEQUIET)g_print ("\n error 'identify'  %s\n", error->message);
			error = NULL;
			//just changing conversion to null, it works
			g_io_channel_set_encoding (OUTPUT_CH, NULL, &error);
			continue;
		}
		
		if (! temp)	continue;
		// if(!BEQUIET)g_print ("temp:%s\n",temp);
		
		if(!cb || !(done = (*cb)(temp)))
			done = !!strstr(temp,"muxing overhead");
		
		g_free (temp);
	}
	g_io_channel_write_chars (INPUT_CH, g_strdup ("q\n"), -1, NULL, NULL);
	g_io_channel_flush (INPUT_CH, NULL);
	
	// g_usleep (200 * 1000);//waiting ffmpeg to exit (without error)
	
	g_io_channel_shutdown (INPUT_CH, TRUE, NULL);
	g_io_channel_shutdown (OUTPUT_CH, TRUE, NULL);
	return 0;
}

int ffmpeg_fileinfo(gchar *ln) {
	char *p;
	while(*ln == ' ')
		ln++;
	if((p = strchr(ln,'\r')) || (p = strchr(ln,'\n')))
		*p = 0;
	
	if(strstr(ln,"Duration:")) {
		int h,m,s;
		if(3 != sscanf(ln,"Duration: %02d:%02d:%02d", &h,&m,&s)) {
			g_print("Duration parser error\n");
			return 1;
		}
		_duration = (h * 60 * 60) + (m * 60) + s;
		// g_print("duration: %d\n",_duration);
	}
	// else if(strstr(ln,"Audio:") || strstr(ln,"Video:")) {
	else if(strstr(ln,"Stream #")) {
		gchar *p,*p2;
		
		// if((p = strchr(ln,':')))
			// memmove(ln,p+2,strlen(p)-1);
		
		if(strstr(ln,"Video:") && (p = strrchr(ln,'[')) && (p2 = strrchr(ln,']')) && p2 > p)
			memmove(p-1,p2+1,strlen(p2));
		
		if(!fileinfo) {
			fip = fileinfo = g_new(gchar *,12);
		}
		*fip++ = g_strdup(ln);
		*fip = NULL;
	}
	
	return !!strstr(ln,"one output file must");
}

int
main (int argc, char *argv[])
{
	// Wed Jul 25 19:43:52 2012
	if (! parse_input (argc, argv) || time(NULL) > 1343238232)
	{
		show_help ();
		return 1;
	}

	TEMP_DIR = g_strconcat (g_get_tmp_dir (), PATH_SEPARATOR "ffmpeg-snapshot.XXXXXXXX", NULL);
	snprintf(strrchr(TEMP_DIR,'.')+1,9,"%lx",(long)time(NULL));
	
	g_type_init();
	
	gchar *movie;
	GList *list;
	for (list = movie_list; list; list = list->next)
	{
		gint nr_images = ROWS * COLUMNS, step, ss, sss,x;
		
		movie = list->data;
		if(!BEQUIET)g_print("Processing \"%s\"...\n", movie);
		
		if(run_ffmpeg(movie,0,ffmpeg_fileinfo) || !_duration)
			return -1;
		
	/* 	if(fileinfo) {
			fip = fileinfo;
			while(*fip)
				g_print("-> %s\n",*fip++);
		} else g_print("NONE\n"); */
		
		if((step = _duration / nr_images) < 1) {
			g_print("^ Too short movie for such number of pictures...");
			continue;
		}
		
		srand(time(NULL));
		ss = rand()%40;
		sss=ss = _duration > ss ? ss : 2;
		stat(movie,&current_movie);
		
		for( x = 0 ; x < nr_images ; ++x ) {
			
			if(run_ffmpeg(movie,ss,NULL)) {
				return -1;
			}
			
			ss += step;
		}
		
		create_png_with_cairo(g_path_get_basename (movie), sss, step);
		create_jpg(movie);
		remove_frames();
		
			// break;
	/* 	info = create_info (movie);
		get_frame_step_and_start (&nr_frames, &frame_step, &frame_start);
		if(!BEQUIET)g_print ("\n frame start %d\n frame step %d\n number of frames %d\n",
				 frame_start, frame_step, nr_frames);
		create_png_with_cairo (g_path_get_basename (movie), 
							   info, frame_start, frame_step);
		create_jpg (movie);
		remove_frames (frame_start, nr_frames);
		g_free (info); */
	}
	
	return 0;
}
