/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/glxgears/glxgears.c,v 1.3tsi Exp $ */

/*
 * This is a port of the infamous "gears" demo to straight GLX (i.e. no GLUT)
 * Port by Brian Paul  23 March 2001
 *
 * Exact timing added by Behdad Esfahbod to achieve a fixed speed regardless
 * of frame rate.  November 2003
 *
 * Printer support added by Roland Mainz <roland.mainz@nrubsig.org>. April 2004
 *
 * Command line options:
 *    -singletooth     Draw only one tooth for each gear (for debugging etc.).
 *    -display         Set X11 display for output.
 *    -print           Use printer instead of video card for output.
 *    -printer         Printername name of printer to use.
 *    -printfile printername  Output file for print job.
 *    -numpages count  Number of pages to print.
 *    -info            Print additional GLX information.
 *    -h               Print this help page.
 *    -v               Verbose output.
 *
 */


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef BUILD_PRINTSUPPORT
#include <X11/XprintUtil/xprintutil.h>
#endif /* BUILD_PRINTSUPPORT */
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <sys/time.h>
#include <sched.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif /* !M_PI */

/* Turn a NULL pointer string into an empty string */
#define NULLSTR(x) (((x)!=NULL)?(x):(""))
#define Log(x) { if(verbose) printf x; }
#define Msg(x) { printf x; }

/* Globla vars */ 
static const char *ProgramName;        /* program name (from argv[0]) */
static Bool        verbose = False;    /* verbose output what the program is doing */
 
static int         xp_event_base,      /* XpExtension even base */
                   xp_error_base;      /* XpExtension error base */
static double      degperpage = 5.0;   /* Rotate gears <deg> degree per page */

#define TIMING 1

#ifdef TIMING

/* XXX this probably isn't very portable */

/* return current time (in seconds) */
static long
current_time(void)
{
   struct timeval tv;
   struct timezone tz;
   (void) gettimeofday(&tv, &tz);
   return (long) tv.tv_sec * 1000000 + (long) tv.tv_usec;
}

#else /* TIMING */

/* dummy */
static long
current_time(void)
{
   return 0L;
}

#endif /* TIMING */

static
void usage(void)
{
   fprintf (stderr, "usage:  %s [options]\n", ProgramName);
   fprintf (stderr, "-display\tSet X11 display for output.\n");
#ifdef BUILD_PRINTSUPPORT
   fprintf (stderr, "-print\t\tUse printer instead of video card for output.\n");
   fprintf (stderr, "-printer printername\tname of printer to use.\n");
   fprintf (stderr, "-printfile printername\tOutput file for print job.\n");
   fprintf (stderr, "-numpages count\tNumber of pages to print.\n");
   fprintf (stderr, "-degperpage deg\tRotate gears <deg> degree per page.\n");
#endif /* BUILD_PRINTSUPPORT */
   fprintf (stderr, "-info\t\tPrint additional GLX information.\n");
   fprintf (stderr, "-singletooth\tDraw only one tooth for each gear (for debugging etc.).\n");
   fprintf (stderr, "-h\t\tPrint this help page.\n");
   fprintf (stderr, "-v\t\tVerbose output.\n");
   fprintf (stderr, "\n");
   exit(EXIT_FAILURE);
}

#ifdef BUILD_PRINTSUPPORT
static
void PrintSpoolerCommandResults( Display *pdpy, XPContext pcontext )
{
    char *scr;

    scr = XpGetOneAttribute(pdpy, pcontext, XPJobAttr, "xp-spooler-command-results");
    if( scr )
    {
      if( strlen(scr) > 0 )
      {
        const char *msg = XpuCompoundTextToXmb(pdpy, scr);
        if( msg )
        {
          Msg(("Spooler command returned '%s'.\n", msg));
          XpuFreeXmbString(msg);
        }
        else
        {
          Msg(("Spooler command returned '%s' (unconverted).\n", scr));
        }
      }

      XFree((void *)scr);
    }
}
#endif /* BUILD_PRINTSUPPORT */

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;
static GLint speed = 60;
static int singletooth = 0, paused = 0;
static GLboolean printInfo = GL_FALSE;

/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 * 
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
   GLint i;
   GLfloat r0, r1, r2, maxr2, minr2;
   GLfloat angle, da;
   GLfloat u, v, len;

   r0 = inner_radius;
   r1 = outer_radius - tooth_depth / 2.0;
   maxr2 = r2 = outer_radius + tooth_depth / 2.0;
   minr2 = singletooth ? r1 : r2;

   da = 2.0 * M_PI / teeth / 4.0;

   glShadeModel(GL_FLAT);

   glNormal3f(0.0, 0.0, 1.0);

   /* draw front face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      if (i < teeth) {
         glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
         glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                    width * 0.5);
      }
   }
   glEnd();

   /* draw front sides of teeth */
   glBegin(GL_QUADS);
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 width * 0.5);
      r2 = minr2;
   }
   r2 = maxr2;
   glEnd();

   glNormal3f(0.0, 0.0, -1.0);

   /* draw back face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      if (i < teeth) {
         glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                    -width * 0.5);
         glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      }
   }
   glEnd();

   /* draw back sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 -width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 -width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      r2 = minr2;
   }
   r2 = maxr2;
   glEnd();

   /* draw outward faces of teeth */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      u = r2 * cos(angle + da) - r1 * cos(angle);
      v = r2 * sin(angle + da) - r1 * sin(angle);
      len = sqrt(u * u + v * v);
      u /= len;
      v /= len;
      glNormal3f(v, -u, 0.0);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glNormal3f(cos(angle + 1.5 * da), sin(angle + 1.5 * da), 0.0);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 -width * 0.5);
      u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
      v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
      glNormal3f(v, -u, 0.0);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 -width * 0.5);
      glNormal3f(cos(angle + 3.5 * da), sin(angle + 3.5 * da), 0.0);
      r2 = minr2;
   }
   r2 = maxr2;

   glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
   glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

   glEnd();

   glShadeModel(GL_SMOOTH);

   /* draw inside radius cylinder */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glNormal3f(-cos(angle), -sin(angle), 0.0);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
   }
   glEnd();
}


static void
draw(void)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glPushMatrix();
   glRotatef(view_rotx, 1.0, 0.0, 0.0);
   glRotatef(view_roty, 0.0, 1.0, 0.0);
   glRotatef(view_rotz, 0.0, 0.0, 1.0);

   glPushMatrix();
   glTranslatef(-3.0, -2.0, 0.0);
   glRotatef(angle, 0.0, 0.0, 1.0);
   glCallList(gear1);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(3.1, -2.0, 0.0);
   glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
   glCallList(gear2);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(-3.1, 4.2, 0.0);
   glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
   glCallList(gear3);
   glPopMatrix();

   glPopMatrix();
}


/* new window size or exposure */
static void
reshape(int width, int height)
{
   GLfloat h = (GLfloat) height / (GLfloat) width;

   glViewport(0, 0, (GLint) width, (GLint) height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   /* fit width and height */
   if (h >= 1.0)
     glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
   else
     glFrustum(-1.0/h, 1.0/h, -1.0, 1.0, 5.0, 60.0);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.0, 0.0, -40.0);
}


static void
init(void)
{
   static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
   static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
   static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
   static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

   glLightfv(GL_LIGHT0, GL_POSITION, pos);
   glEnable(GL_CULL_FACE);
   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glEnable(GL_DEPTH_TEST);

   /* make the gears */
   gear1 = glGenLists(1);
   glNewList(gear1, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
   gear(1.0, 4.0, 1.0, 20, 0.7);
   glEndList();

   gear2 = glGenLists(1);
   glNewList(gear2, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
   gear(0.5, 2.0, 2.0, 10, 0.7);
   glEndList();

   gear3 = glGenLists(1);
   glNewList(gear3, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
   gear(1.3, 2.0, 0.5, 10, 0.7);
   glEndList();

   glEnable(GL_NORMALIZE);
}


/*
 * Create an RGB, double-buffered window.
 * Return the window and context handles.
 */
static void
make_window( Display *dpy, Screen *scr,
             const char *name,
             int x, int y, int width, int height,
             Window *winRet, GLXContext *ctxRet)
{
   int attrib[] = { GLX_RGBA,
                   GLX_RED_SIZE, 1,
                   GLX_GREEN_SIZE, 1,
                   GLX_BLUE_SIZE, 1,
                   GLX_DOUBLEBUFFER,
                   GLX_DEPTH_SIZE, 1,
                   None };
   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;
   GLXContext ctx;
   XVisualInfo *visinfo;
   GLint max[2] = { 0, 0 };

   scrnum = XScreenNumberOfScreen(scr);
   root   = XRootWindow(dpy, scrnum);

   visinfo = glXChooseVisual( dpy, scrnum, attrib );
   if (!visinfo) {
      fprintf(stderr, "%s: Error: couldn't get an RGB, Double-buffered visual.\n", ProgramName);
      exit(EXIT_FAILURE);
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;
   attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( dpy, root, x, y, width, height,
                       0, visinfo->depth, InputOutput,
                       visinfo->visual, mask, &attr );

   /* set hints and properties */
   {
      XSizeHints sizehints;
      sizehints.x = x;
      sizehints.y = y;
      sizehints.width  = width;
      sizehints.height = height;
      sizehints.flags = USSize | USPosition;
      XSetNormalHints(dpy, win, &sizehints);
      XSetStandardProperties(dpy, win, name, name,
                              None, (char **)NULL, 0, &sizehints);
   }

   ctx = glXCreateContext( dpy, visinfo, NULL, True );
   if (!ctx) {
      fprintf(stderr, "%s: Error: glXCreateContext failed.\n", ProgramName);
      exit(EXIT_FAILURE);
   }

   XFree(visinfo);

   XMapWindow(dpy, win);
   glXMakeCurrent(dpy, win, ctx);

   /* Check for maximum size supported by the GL rasterizer */   
   glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max);
   if (printInfo)
      printf("GL_MAX_VIEWPORT_DIMS=%d/%d\n", (int)max[0], (int)max[1]);
   if (width > max[0] || height > max[1]) {
      fprintf(stderr, "%s: Error: Requested window size (%d/%d) larger than "
              "maximum supported by GL engine (%d/%d).\n",
              ProgramName, width, height, (int)max[0], (int)max[1]);
      exit(EXIT_FAILURE);
   }

   *winRet = win;
   *ctxRet = ctx;
}

static void
event_loop(Display *dpy, Window win, int numPages )
{
   while (1) {
      /* Process interactive events only when we are not printing */
      if (numPages == 0) {
         while (XPending(dpy) > 0) {
            XEvent event;
            XNextEvent(dpy, &event);
            switch (event.type) {
            case Expose:
               Log(("Event: Expose\n"));
               /* we'll redraw below */
               break;
            case ConfigureNotify:
               Log(("Event: ConfigureNotify\n"));
               reshape(event.xconfigure.width, event.xconfigure.height);
               break;
            case KeyPress:
               {
                  char buffer[10];
                  int code;
                  code = XLookupKeysym(&event.xkey, 0);
                  if (code == XK_Left) {
                     view_roty += 5.0;
                     Log(("view_roty=%g\n", (double)view_roty));
                  }
                  else if (code == XK_Right) {
                     view_roty -= 5.0;
                     Log(("view_roty=%g\n", (double)view_roty));
                  }
                  else if (code == XK_Up) {
                     view_rotx += 5.0;
                     Log(("view_roty=%g\n", (double)view_rotx));
                  }
                  else if (code == XK_Down) {
                     view_rotx -= 5.0;
                     Log(("view_roty=%g\n", (double)view_rotx));
                  }
                  else {
                     (void) XLookupString(&event.xkey, buffer, sizeof(buffer),
                                          NULL, NULL);
                     switch (buffer[0]) {
                        case 27: /* escape */
                        case 'q':
                        case 'Q':
                           Log(("Quitting...\n"));
                           return;
                        case 32: /* space */
                           paused = 1 - paused;
                           Log(("Pause status: %d\n", paused));
                           break;
                        case 'a':
                           if (speed < 360) {
                              speed += 5;
                              Log(("Speed increased to %d\n", (int)speed));
                           }
                           break;
                        case 'z':
                           if (speed > -360) {
                              speed -= 5;
                              Log(("Speed decreased to %d\n", (int)speed));
                           }
                           break;
                        default:
#ifdef XKB
                           XkbStdBell(XtDisplay(dpy, win, 0, XkbBI_MinorError);
#else
                           XBell(dpy, 0);
#endif /* XKB */
                           break;

                     }
                  }
               }
            }
         }
      }

      {
         long t = current_time();
         {
            /* next frame */
            static long t0 = 0;
            long useconds;

            if (!t0)
               t0 = t;

            useconds = t - t0;
            if (!useconds) /* assume 100FPS if we don't have timer */
               useconds = 10000;

            if (numPages > 0) {
                angle = angle + degperpage; /* |degperpage| degrees per page */
            }
            else
            {
                if (!paused)
                    angle = angle + ((double)speed * useconds) / 1000000.0;
            }
            
            /* keep angle small so we don't lose precision! */
            if (angle > 360.0)
               angle = angle - 360.0;

#ifdef BUILD_PRINTSUPPORT
            if (numPages > 0) {
               Log(("Start page.\n"));
               XpStartPage(dpy, win);
               XpuWaitForPrintNotify(dpy, xp_event_base, XPStartPageNotify);      
            }
#endif /* BUILD_PRINTSUPPORT */

            draw();
            glXSwapBuffers(dpy, win);

#ifdef BUILD_PRINTSUPPORT
            if (numPages > 0) {
               Log(("End page.\n"));
               XpEndPage(dpy);
               XpuWaitForPrintNotify(dpy, xp_event_base, XPEndPageNotify);

               /* Last page ? */          
               if( --numPages == 0 )
                  return;
            }
#endif /* BUILD_PRINTSUPPORT */

            t0 = t;
         }

#ifdef TIMING
         {
            /* calc framerate */
            static int frames = 0;
            static long t0 = 0;

            if (!t0)
               t0 = t;

            frames++;

            if (t - t0 >= 5000000L) {
               GLfloat seconds = (t - t0) / 1000000.0;
               GLfloat fps = frames / seconds;

               printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,
                      fps);
               t0 = t;
               frames = 0;
            }
         }

         /* Need to give cpu away in order to get precise timing next cycle,
          * otherwise, gettimeofday would return almost the same value. */
         sched_yield();
#endif /* TIMING */
      }
   }
}


int
main(int argc, char *argv[])
{
   Bool           use_threadsafe_api = False;
   Display       *dpy;
   Window         win;
   Screen        *screen;
   GLXContext     ctx;
   char          *dpyName            = NULL;
   int            i;
   XRectangle     winrect;

#ifdef BUILD_PRINTSUPPORT
   long           dpi_x              = 0L,
                  dpi_y              = 0L;
   XPContext      pcontext           = None; /* Xprint context */
   void          *printtofile_handle = NULL; /* "context" when printing to file */
   Bool           doPrint            = FALSE; /* Print to printer ? */
   const char    *printername        = NULL;  /* printer to query */
   const char    *toFile             = NULL;  /* output file (instead of printer) */
   int            numPages           = 5;     /* Numer of pages to print */
   XPPrinterList  plist              = NULL;  /* list of printers */
   int            plist_count;                /* number of entries in |plist|-array */
   unsigned short dummy;
#endif /* BUILD_PRINTSUPPORT */

   ProgramName = argv[0];
    
   for (i = 1; i < argc; i++) {
      const char *arg = argv[i];
      int         len = strlen(arg);

      if (strcmp(argv[i], "-display") == 0) {
         if (++i >= argc)
            usage();
         dpyName = argv[i];
      }
      else if (strcmp(argv[i], "-info") == 0) {
         printInfo = GL_TRUE;
      }
#ifdef BUILD_PRINTSUPPORT
      else if (strcmp(argv[i], "-print") == 0) {
         doPrint = True;
      }
      else if (!strncmp("-printer", arg, len)) {
         if (++i >= argc)
            usage();
         printername = argv[i];
         doPrint = True;
      }
      else if (!strncmp("-printfile", arg, len)) {
         if (++i >= argc)
            usage();
         toFile = argv[i];
         doPrint = True;
      }
      else if (!strncmp("-numpages", arg, len)) {
         if (++i >= argc)
            usage();
         errno = 0; /* reset errno to catch |atoi()|-errors */
         numPages = atoi(argv[i]);
         if ((numPages <= 0) || (errno != 0))
            usage();
         doPrint = True;
      }
      else if (!strncmp("-degperpage", arg, len)) {
         if (++i >= argc)
            usage();
         errno = 0; /* reset errno to catch |atof()|-errors */
         degperpage = atof(argv[i]);
         if (errno != 0)
            usage();
         doPrint = True;
      }
#endif /* BUILD_PRINTSUPPORT */
      else if (!strncmp("-v", arg, len)) {
         verbose   = True;
         printInfo = GL_TRUE;
      }
      else if (!strncmp("-singletooth", arg, len)) {
         singletooth = True;
      }
      else if( !strncmp("-debug_use_threadsafe_api", arg, len) )
      {
         use_threadsafe_api = True;
      }
      else if (!strcmp(argv[i], "-h")) {
         usage();
      }
      else
      {
        fprintf(stderr, "%s: Unsupported option '%s'.\n", ProgramName, argv[i]);
        usage();
      }
   }

   /* Init X threading API on demand (for debugging) */
   if( use_threadsafe_api )
   {
      if( !XInitThreads() )
      {
         fprintf(stderr, "%s: XInitThreads() failure.\n", ProgramName);
         exit(EXIT_FAILURE);
      }
   }
   
#ifdef BUILD_PRINTSUPPORT
   /* Display and printing at the same time not implemented */
   if (doPrint && dpyName) {
      usage();
   }

   if (doPrint) {
      plist = XpuGetPrinterList(printername, &plist_count);

      if (!plist) {
         fprintf(stderr, "%s:  no printers found for printer spec \"%s\".\n",
                 ProgramName, NULLSTR(printername));
         return EXIT_FAILURE;
      }

      printername = plist[0].name;

      Log(("Using printer '%s'\n", printername));

      if (XpuGetPrinter(printername, &dpy, &pcontext) != 1) {
         fprintf(stderr, "%s: Cannot open printer '%s'\n", ProgramName, printername);
         return EXIT_FAILURE;
      }

      if (XpQueryExtension(dpy, &xp_event_base, &xp_error_base) == False) {
         fprintf(stderr, "%s: XpQueryExtension() failed.\n", ProgramName);
         XpuClosePrinterDisplay(dpy, pcontext);
         return EXIT_FAILURE;
      }

      /* Listen to XP(Start|End)(Job|Doc|Page)Notify events).
       * This is mantatory as Xp(Start|End)(Job|Doc|Page) functions are _not_ 
       * syncronous !!
       * Not waiting for such events may cause that subsequent data may be 
       * destroyed/corrupted!!
       */
      XpSelectInput(dpy, pcontext, XPPrintMask);

      /* Set job title */
      XpuSetJobTitle(dpy, pcontext, "glxgears for Xprint");

      /* Set print context
       * Note that this modifies the available fonts, including builtin printer prints.
       * All XListFonts()/XLoadFont() stuff should be done _after_ setting the print 
       * context to obtain the proper fonts.
       */ 
      XpSetContext(dpy, pcontext);

      /* Get default printer reolution */   
      if (XpuGetResolution(dpy, pcontext, &dpi_x, &dpi_y) != 1) {
         fprintf(stderr, "%s: No default resolution for printer '%s'.\n",
         ProgramName, printername);
         XpuClosePrinterDisplay(dpy, pcontext);
         return EXIT_FAILURE;
      }

      if (toFile) {
         Log(("starting job (to file '%s').\n", toFile));
         printtofile_handle = XpuStartJobToFile(dpy, pcontext, toFile);
         if( !printtofile_handle ) {
            fprintf(stderr, "%s: Error: %s while trying to print to file.\n", 
                    ProgramName, strerror(errno));
            XpuClosePrinterDisplay(dpy, pcontext);
            return EXIT_FAILURE;
         }

         XpuWaitForPrintNotify(dpy, xp_event_base, XPStartJobNotify);
      }
      else
      {
         Log(("starting job.\n"));
         XpuStartJobToSpooler(dpy);    
         XpuWaitForPrintNotify(dpy, xp_event_base, XPStartJobNotify);
      }

      screen = XpGetScreenOfContext(dpy, pcontext);

      /* Obtain some info about page geometry */
      XpGetPageDimensions(dpy, pcontext, &dummy, &dummy, &winrect);

      /* Center output window on page */
      winrect.width  /= 2;
      winrect.height /= 2;
      winrect.x += winrect.width  / 2;
      winrect.y += winrect.height / 2;
   }
   else
#endif /* BUILD_PRINTSUPPORT */
   {
      dpy = XOpenDisplay(dpyName);
      if (!dpy) {
         fprintf(stderr, "%s: Error: couldn't open display '%s'\n", ProgramName, dpyName);
         return EXIT_FAILURE;
      }

      screen = XDefaultScreenOfDisplay(dpy);

      winrect.x      = 0;
      winrect.y      = 0;
      winrect.width  = 300;
      winrect.height = 300;
   }
   
   Log(("Window x=%d, y=%d, width=%d, height=%d\n",
       (int)winrect.x, (int)winrect.y, (int)winrect.width, (int)winrect.height));

   make_window(dpy, screen, "glxgears", winrect.x, winrect.y, winrect.width, winrect.height, &win, &ctx);
   reshape(winrect.width, winrect.height);

   if (printInfo) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
      printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
   }

   init();

#ifdef BUILD_PRINTSUPPORT
   event_loop(dpy, win, doPrint?numPages:0);
#else /* !BUILD_PRINTSUPPORT */
   event_loop(dpy, win, 0);
#endif /* !BUILD_PRINTSUPPORT */

   glXDestroyContext(dpy, ctx);

#ifdef BUILD_PRINTSUPPORT
   if (doPrint) {
      /* End the print job - the final results are sent by the X print
       * server to the spooler sub system.
       */
      XpEndJob(dpy);
      XpuWaitForPrintNotify(dpy, xp_event_base, XPEndJobNotify);    
      Log(("end job.\n"));

      if (toFile) {
         if (XpuWaitForPrintFileChild(printtofile_handle) != XPGetDocFinished) {
            fprintf(stderr, "%s: Error while printing to file.\n", ProgramName);
            XpuClosePrinterDisplay(dpy, pcontext);
            return EXIT_FAILURE;
         }
      }

      PrintSpoolerCommandResults(dpy, pcontext);    

      XDestroyWindow(dpy, win);
      XpuClosePrinterDisplay(dpy, pcontext);

      XpuFreePrinterList(plist);
   }
   else
#endif /* BUILD_PRINTSUPPORT */
   {
      XDestroyWindow(dpy, win);
      XCloseDisplay(dpy);
   }
   
   return EXIT_SUCCESS;
}

