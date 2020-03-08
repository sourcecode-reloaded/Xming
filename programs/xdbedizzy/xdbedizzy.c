/* Xorg: xdbedizzy.c /main/3 2004/10/18 14:21:35 gisburn $ */
/******************************************************************************
 * 
 * Copyright (c) 1994, 1995  Silicon Graphics Inc.
 * Copyright (c) 2004  Roland Mainz <roland.mainz@nrubsig.org>
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of Silicon Graphics not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific prior written permission.
 * Silicon Graphics makes no representation about the suitability
 * of this software for any purpose. It is provided "as is"
 * without any express or implied warranty.
 *
 * SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 * GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 * THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 *****************************************************************************/

/*
 * xdbedizzy - demo of DBE creating a double buffered spinning scene
 *
 * Original dizzy program written by Mark Kilgard.
 *
 * Adapted to use DBE for double buffering by Allen Leinwand, 2/24/1995 .
 * Print support added by Roland Mainz, 10/18/2004
 *
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdbe.h>
#ifdef USE_XPRINT
#include <X11/XprintUtil/xprintutil.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <X11/Xpoll.h>

/* Turn a NULL pointer string into an empty string */
#define NULLSTR(x) (((x)!=NULL)?(x):(""))
#define Log(x) { if(verbose) printf x; }
#define Msg(x) { printf x; }

/* Global variables */
static char             *ProgramName   = NULL;
static Display          *dpy           = NULL;
static Screen           *screen        = NULL;
static int               screennum     = -1;
#ifdef USE_XPRINT
static XPContext         pcontext      = None; /* Xprint context */
#endif
static XRectangle        winrect       = { 0 };
static unsigned long     c_black, c_pink, c_green, c_orange, c_blue;
static Window            win           = None;
static XID               buf           = None;
static XdbeSwapInfo      swapInfo      = { 0 };
static GC                gc_black, gc_pink, gc_green, gc_orange, gc_blue;
static float             rotation      = 0.0;
static float             delta         = 0.05;
static float             speed         = 20.0;
static Bool              paused        = False;
static Bool              manual_paused = False;
#ifdef USE_XPRINT
static int               xp_event_base,         /* XpExtension even base   */
                         xp_error_base;         /* XpExtension error base  */
static long              dpi_x         = 0L,    /* Current page resolution */
                         dpi_y         = 0L;    
static int               numPages      = 5,     /* Numer of pages to print */
                         currNumPages  = 0;     /* Current page number */
static Bool              doPrint       = False; /* Print to printer ? */
#endif

/* Default values for unspecified command line arguments */
static char             *display_name  = NULL;
static int               visclass      = PseudoColor;
static int               depth         = 0;
static Bool              listVis       = False;
static int               spokes        = 12;
static Bool              do_db         = True;
static Bool              verbose       = False;
static Bool              synchronous   = False;
static VisualID          visid         = 0;

static const char *help_message[] = {
"  where options include:",
"    -display host:dpy       X server connection to use.",
#ifdef USE_XPRINT
"    -print                  Use printer instead of video card for output.",
"    -printer printername    Name of printer to use.",
"    -printfile printername  Output file for print job.",
"    -numpages count         Number of pages to print.",
#endif
"    -delta dlt              Rotate <dlt> per frame (video) or page (printer).",
"    -class classname        Class of visual to use.",
"    -depth n                Depth of visual to use.",
"    -visid [nn,0xnn]        Visual ID to use (ignore -class, -depth).",
"    -list                   List double buffer capable visuals.",
"    -nodb                   Single buffer (ignore -class, -depth, -visid).",
"    -help                   Print this message.",
"    -speed val              Floating-point value to set the speed.",
"    -sync                   Use synchronous X connection.",
"    -spokes n               Specify number of spokes to draw.",
"    -verbose                Produce chatty messages while running.",
NULL};

static
void usage(void)
{
    const char **cpp;

    fprintf (stderr, "\nusage:  %s [-options ...]\n", ProgramName);
    for (cpp = help_message; *cpp; cpp++) {
        fprintf (stderr, "%s\n", *cpp);
    }
    fprintf (stderr, "\n");
    exit(EXIT_FAILURE);
}


static
unsigned long getColor(Colormap cmap, const char *color_name)
{
    XColor color;
    XColor exact;
    int    status;

    status = XAllocNamedColor(dpy, cmap, color_name, &color, &exact);
    if (status == 0) {
        fprintf(stderr, "%s: Couldn't get color: %s\n", ProgramName, color_name);
        exit(EXIT_FAILURE);
    }
    return (color.pixel);
}



#define RATIO1 0.4
#define RATIO2 0.7
#define RATIO3 0.95

#ifndef M_PI
#define M_PI 3.1415927
#endif

#define S_ANGLE(s) (M_PI*2./(s))

static
void redraw(void)
{
    int     i;
    int     x, y;
    XPoint  pnt[4];

    Log(("redraw.\n"));

    /* the double-buffer extension will clear the buffer itself */
    if (!do_db) {
        XClearWindow(dpy, win);
    }
    x = winrect.width / 2;
    x += (int) (sin(rotation * 2) * 20);
    y = winrect.height / 2;
    y += (int) (cos(rotation * 2) * 20);
    for (i = 5; i < 26; i += 3) {
        XDrawArc(dpy, buf, gc_orange, x - i * 10,      y - i * 10,      i * 20,      i * 20,      0, 360 * 64);
        XDrawArc(dpy, buf, gc_green,  x - i * 10 - 5,  y - i * 10 - 5,  i * 20 + 10, i * 20 + 10, 0, 360 * 64);
        XDrawArc(dpy, buf, gc_blue,   x - i * 10 - 10, y - i * 10 - 10, i * 20 + 20, i * 20 + 20, 0, 360 * 64);
    }

    x = winrect.width  / 2;
    y = winrect.height / 2;
    pnt[0].x = x;
    pnt[0].y = y;
    for (i = 0; i < spokes; i++) {
        pnt[1].x = (int) (cos(i * S_ANGLE(spokes) + rotation)       * (RATIO1 * x)) + x;
        pnt[1].y = (int) (sin(i * S_ANGLE(spokes) + rotation)       * (RATIO1 * y)) + y;
        pnt[2].x = (int) (cos(i * S_ANGLE(spokes) + rotation - 0.1) * (RATIO2 * x)) + x;
        pnt[2].y = (int) (sin(i * S_ANGLE(spokes) + rotation - 0.1) * (RATIO2 * y)) + y;
        pnt[3].x = (int) (cos(i * S_ANGLE(spokes) + rotation - 0.2) * (RATIO3 * x)) + x;
        pnt[3].y = (int) (sin(i * S_ANGLE(spokes) + rotation - 0.2) * (RATIO3 * y)) + y;
        XDrawLines(dpy, buf, gc_pink, pnt, 4, CoordModeOrigin);
    }

    if (do_db) {
        XdbeSwapBuffers(dpy, &swapInfo, 1);
    }
}


static
Visual *
choose_DB_visual( /* Input */         Display *dpy, Bool listVis, int visclass,
                  /* Input, Output */ int *pDepth)
{
    Drawable               screen_list[1];
    int                    num_screens;
    XdbeScreenVisualInfo  *DBEvisInfo;
    int                    i, nitems;
    int                    chosenDepth = 0;
    Visual                *chosenVisual = NULL;
    XVisualInfo            vinfo_template, *XvisInfo;

    screen_list[0] = XRootWindowOfScreen(screen);
    num_screens = 1;
    DBEvisInfo = XdbeGetVisualInfo(dpy, screen_list, &num_screens);
    if (DBEvisInfo == NULL) {
        fprintf(stderr, "XdbeGetVisualInfo returned NULL\n");
        return (NULL);
    }

    if (listVis) {
        printf("\nThe double buffer capable visuals are:\n");
        printf("      visual ID    depth    class\n");
    }
    for (i = 0; i < DBEvisInfo->count; i++) {

        vinfo_template.visualid = DBEvisInfo->visinfo[i].visual;
        XvisInfo = XGetVisualInfo(dpy, VisualIDMask,
                                   &vinfo_template, &nitems);
        if (XvisInfo == NULL) {
            fprintf(stderr,
                    "%s: XGetVisualInfo returned NULL for visual %d\n",
                    ProgramName, (int)vinfo_template.visualid);
            return (NULL);
        }
        if (listVis) {
            char visualClassName[64];

            switch( XvisInfo->class ) {
                case TrueColor:   strcpy(visualClassName, "TrueColor");    break;
                case DirectColor: strcpy(visualClassName, "DirectColor");  break;
                case PseudoColor: strcpy(visualClassName, "PseudoColor");  break;
                case StaticColor: strcpy(visualClassName, "StaticColor");  break;
                case GrayScale:   strcpy(visualClassName, "GrayScale");    break;
                case StaticGray:  strcpy(visualClassName, "StaticGray");   break;
                default:
                    sprintf(visualClassName, "unknown_visual_class_%x", (int)XvisInfo->class);
                    break;
            }
 
            printf("        %#4x      %4d    %s\n",
                   (int)DBEvisInfo->visinfo[i].visual,
                   (int)DBEvisInfo->visinfo[i].depth,
                   visualClassName);
        }

        if (visid) {
            if (XvisInfo->visualid == visid) {
                chosenVisual = XvisInfo->visual;
                chosenDepth = XvisInfo->depth;
            }
        }
        else if (XvisInfo->class == visclass) {
            if (*pDepth == 0) {
                /* Choose first deepest visual of matching class. */
                if (DBEvisInfo->visinfo[i].depth > chosenDepth) {
                    chosenVisual = XvisInfo->visual;
                    chosenDepth  = XvisInfo->depth;
                }
            }
            else {
                /* Choose last visual of matching depth and class. */
                if (DBEvisInfo->visinfo[i].depth == *pDepth) {
                    chosenVisual = XvisInfo->visual;
                    chosenDepth  = XvisInfo->depth;
                }
            }
        }
    }

    if (chosenVisual) {
        if (listVis) {
            printf("\n");
        }
        *pDepth = chosenDepth;
        return (chosenVisual);
    }
    else {
        return (NULL);
    }
}

static
void main_loop(void)
{
    fd_set         select_mask;
    int            fd;
    struct timeval timeout;
    int            new_event;
    int            pending;
    Bool           done = False;

    fd = XConnectionNumber(dpy);
    FD_ZERO(&select_mask);
    FD_SET(fd, &select_mask);

    while (!done) {
        XEvent event;

        /* When we print we only render on Expose events and bump
         * |rotation| when the page number changes */                 
        if (!paused && !manual_paused
#ifdef USE_XPRINT
	    && !doPrint
#endif
	    ) {
            pending = XEventsQueued(dpy, QueuedAfterFlush);
            if (pending == 0) {
                do {
                    FD_ZERO(&select_mask);
                    FD_SET(fd, &select_mask);
                    timeout.tv_sec  = 0;
                    timeout.tv_usec = 2000000./speed;
                    new_event = select(fd + 1, &select_mask, NULL, NULL, &timeout);

                    /* This isn't good - we should check the time stamps
                     * between two frames to get a stable frame rate */
                    if (new_event == 0) {
                      rotation = rotation + delta;
                      redraw();
                      XFlush(dpy);
                    }
                } while (new_event == 0);
            }
        }

        XNextEvent(dpy, &event);

#ifdef USE_XPRINT
        /* XpExtension event ? */
        if( doPrint && 
            (event.type == xp_event_base+XPPrintNotify) )
        {
            XPPrintEvent *pev = (XPPrintEvent *)&event;

            switch( pev->detail ) {
                case XPStartJobNotify:
                    Log(("XPStartJobNotify: Starting first page...\n"));
                    XpStartPage(dpy, win);
                    break;
                case XPEndJobNotify:
                    Log(("XPEndJobNotify: Job done..."));
                    /* Job done... */
                    done = True;
                    break;
                case XPStartDocNotify:
                    Log(("XPStartJobNotify: Nop\n"));
                    break;
                case XPEndDocNotify:
                    Log(("XPEndDocNotify: Nop\n"));
                    break;
                case XPStartPageNotify:
                    /* XpStartPage() will automatically trigger an Expose event */

                    Log(("XPStartPageNotify: Page end reached.\n"));
                    XpEndPage(dpy);
                    break;
                case XPEndPageNotify:
                    /* next page or exit */
                    currNumPages++;
                    
                    rotation = rotation + delta;

                    if( currNumPages < numPages ) {
                      Log(("Starting next page (%d)...\n", currNumPages));
                      XpStartPage(dpy, win);
                    }
                    else
                    {
                      Log(("XPEndPageNotify: Finishing job...\n"));
                      XpEndJob(dpy);
                    }
                    break;
                default:
                    Log(("--> other XPPrintEvent event, detail=%x\n", (int)pev->detail));
                    break;
            }
        }
        else
#endif
        {
            switch (event.type) {
                case MapNotify:
                    Log(("MapNotify: resuming...\n"));
                    paused = False;
                    break;
                case UnmapNotify:
                    Log(("UnmapNotify: pausing...\n"));
                    paused = True;
                    break;
                case VisibilityNotify:
                    switch (event.xvisibility.state) {
                    case VisibilityUnobscured:
                        Log(("VisibilityUnobscured: resuming...\n"));
                        paused = False;
                        break;
                    case VisibilityPartiallyObscured:
                        Log(("VisibilityPartiallyObscured: resuming...\n"));
                        paused = False;
                        break;
                    case VisibilityFullyObscured:
                        Log(("VisibilityFullyObscured: pausing...\n"));
                        paused = True;
                        break;
                    }
                    break;
                case Expose:
                    Log(("Expose: rendering.\n"));

                    /* Swallow any extra Expose events (only needed for video
                     * display, the Xprint server is non-interactive and
                     * therefore cannot create extra Expose events caused
                     * by user input) */
#ifdef USE_XPRINT
                    if (!doPrint)
#endif
                      while (XCheckTypedEvent(dpy, Expose, &event))
                        ;

                    redraw();
                    break;
                case ButtonPress:
                    switch (event.xbutton.button) {
                        case 1:
                            Msg(("ButtonPress: faster: %g\n", delta));
                            delta += 0.005;
                            break;
                        case 2:
                            Msg(("ButtonPress: slower: %g\n", delta));
                            delta += -0.005;
                            break;
                        case 3:
                            if (manual_paused) {
                                Msg(("ButtonPress: manual resume.\n"));
                                manual_paused = False;
                            } else {
                                Msg(("ButtonPress: manual pause.\n"));
                                manual_paused = True;
                            }
                    }
                    break;
                case KeyPress:
                    Msg(("KeyPress: done.\n"));
                    done = True;
                    break;
                case ConfigureNotify:
                    Log(("ConfigureNotify: resizing.\n"));
                    winrect.width  = event.xconfigure.width;
                    winrect.height = event.xconfigure.height;
                    break;
            }
        }
    }
}


int main(int argc, char *argv[])
{
    int                  i;
    XSetWindowAttributes attrs;
    Visual              *visual;
    Colormap             cmap;
    XGCValues            gcvals;
    void                *printtofile_handle = NULL; /* "context" when printing to file */
    const char          *printername        = NULL;  /* printer to query */
    const char          *toFile             = NULL;  /* output file (instead of printer) */
#ifdef USE_XPRINT
    XPPrinterList        plist              = NULL;  /* list of printers */
    int                  plist_count;                /* number of entries in |plist|-array */
#endif
    unsigned short       dummy;
    Bool                 use_threadsafe_api = True;

    ProgramName = argv[0];

    for (i = 1; i < argc; i++) {
        char *arg;

        arg = argv[i];
        if (!strcmp(arg, "-display")) {
            if (++i >= argc) {
                fprintf(stderr, "%s: Missing argument to -display\n", ProgramName);
                exit(EXIT_FAILURE);
            }
            display_name = argv[i];
        }
#ifdef USE_XPRINT
        else if (!strcmp(arg, "-print")) {
            doPrint = True;
        }
        else if (!strcmp(arg, "-printer")) {
            if (++i >= argc)
               usage();
            printername = argv[i];
            doPrint = True;
        }
        else if (!strcmp(arg, "-printfile")) {
            if (++i >= argc)
                usage();
            toFile = argv[i];
            doPrint = True;
        }
        else if (!strcmp(arg, "-numpages")) {
            if (++i >= argc)
                usage();
            errno = 0; /* reset errno to catch |atoi()|-errors */
            numPages = atoi(argv[i]);
            if ((numPages <= 0) || (errno != 0))
                usage();
            doPrint = True;
        }
#endif
        else if (!strcmp(arg, "-delta")) {
            if (++i >= argc)
                usage();
            errno = 0; /* reset errno to catch |atof()|-errors */
            delta = atof(argv[i]);
            if (errno != 0)
                usage();
        }
        else if (!strcmp(arg, "-class")) {
            arg = argv[++i];
            if (arg == NULL) {
                fprintf(stderr, "%s: Missing argument to -class\n", ProgramName);
                exit(EXIT_FAILURE);
            }
            if ((!strcmp(arg, "TrueColor")) || (!strcmp(arg, "True")))
                visclass = TrueColor;
            else if (!strcmp(arg, "DirectColor"))
                visclass = DirectColor;
            else if ((!strcmp(arg, "PseudoColor")) || (!strcmp(arg, "Pseudo")))
                visclass = PseudoColor;
            else if (!strcmp(arg, "StaticColor"))
                visclass = StaticColor;
            else if (!strcmp(arg, "GrayScale"))
                visclass = GrayScale;
            else if (!strcmp(arg, "StaticGray"))
                visclass = StaticGray;
            else {
                fprintf(stderr, "%s: Wrong argument %s for -class\n", ProgramName, arg);
                exit(EXIT_FAILURE);
            }
        } else if (!strcmp(arg, "-depth")) {
            arg = argv[++i];
            if (arg == NULL) {
                fprintf(stderr, "%s: Missing argument to -depth\n", ProgramName);
                exit(EXIT_FAILURE);
            }
            errno = 0; /* reset errno to catch |atoi()|-errors */
            depth = atoi(arg);
            if (errno != 0)
                usage();
        } else if (!strcmp(arg, "-help")) {
            usage();
        } else if (!strcmp(arg, "-list")) {
            listVis = True;
        } else if (!strcmp(arg, "-speed")) {
            if (++i >= argc)
                usage();
            errno = 0; /* reset errno to catch |atof()|-errors */
            speed = atof(argv[i]);
            if (errno != 0)
                usage();
        } else if (!strcmp(arg, "-spokes")) {
            arg = argv[++i];
            if (arg == NULL) {
                fprintf(stderr, "%s: Missing argument to -spokes\n", ProgramName);
                exit(EXIT_FAILURE);
            }
            errno = 0; /* reset errno to catch |atoi()|-errors */
            spokes = atoi(arg);
            if (errno != 0)
                usage();
        } else if (!strcmp(arg, "-nodb")) {
            do_db = False;
        } else if (!strcmp(arg, "-visid")) {
            arg = argv[++i];
            if (arg == NULL) {
                fprintf(stderr, "%s: Missing argument to -visid\n", ProgramName);
                exit(EXIT_FAILURE);
            }
            /* |atol()| only uses base10, |strtol(..., ..., 0)| takes any base */
            visid = (int) strtol(arg, (char **)NULL, 0);
        } else if (!strcmp(arg, "-verbose")) {
            verbose = True;
        } else if (!strcmp(arg, "-sync")) {
            synchronous = True;
        } else if (!strcmp(arg, "-debug_use_threadsafe_api")) {
            use_threadsafe_api = True;
        }
        else {
            fprintf(stderr, "%s: Unrecognized option %s\n", ProgramName, arg);
            usage();
        }
    }

#ifdef USE_XPRINT
    /* Display and printing at the same time not implemented */
    if (doPrint && display_name) {
        usage();
    }
#endif

    if (use_threadsafe_api) {
        if (!XInitThreads()) {
            fprintf(stderr, "%s: XInitThreads() failure.\n", ProgramName);
            exit(EXIT_FAILURE);
        }
    }

#ifdef USE_XPRINT
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
        
        if (synchronous) {
            Log(("Running in synchronous X mode.\n"));
            XSynchronize(dpy, True);
        }

        if (XpQueryExtension(dpy, &xp_event_base, &xp_error_base) == False) {
            fprintf(stderr, "%s: XpQueryExtension() failed.\n", ProgramName);
            XpuClosePrinterDisplay(dpy, pcontext);
            return EXIT_FAILURE;
        }

        /* Listen to XP(Start|End)(Job|Doc|Page)Notify events).
         * This is mandatory as Xp(Start|End)(Job|Doc|Page) functions are _not_ 
         * syncronous !!
         * Not waiting for such events may cause that subsequent data may be 
         * destroyed/corrupted!!
         */
        XpSelectInput(dpy, pcontext, XPPrintMask);

        /* Set job title */
        XpuSetJobTitle(dpy, pcontext, "xdbedizzy for Xprint");

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
        }
        else
        {
            Log(("starting job.\n"));
            XpuStartJobToSpooler(dpy);    
        }

        screen = XpGetScreenOfContext(dpy, pcontext);
        screennum = XScreenNumberOfScreen(screen);

        /* Obtain some info about page geometry */
        XpGetPageDimensions(dpy, pcontext, &dummy, &dummy, &winrect);
    }
#endif
    else
    {
        dpy = XOpenDisplay(display_name);
        if (dpy == NULL) {
            fprintf(stderr, "%s: Cannot open display %s\n",
                    ProgramName, XDisplayName(display_name));
            exit(EXIT_FAILURE);
        }

        if (synchronous) {
            Log(("Running in synchronous X mode.\n"));
            XSynchronize(dpy, True);
        }

        screen = XDefaultScreenOfDisplay(dpy);
        screennum = XScreenNumberOfScreen(screen);
#ifdef USE_XPRINT
        pcontext = None;
#endif

        winrect.x      = 10;
        winrect.y      = 10;
        winrect.width  = 400;
        winrect.height = 400;

#ifdef USE_XPRINT
        dpi_x = dpi_y = 100L; /* hack-style - but enougth for our needs */
#endif
    }
    
    if (do_db) {
        int dbeMajorVersion,
            dbeMinorVersion;

        if (!XdbeQueryExtension (dpy, &dbeMajorVersion, &dbeMinorVersion)) {
            fprintf(stderr, "%s: XdbeQueryExtension() failed.\n", ProgramName);
            exit(EXIT_FAILURE);
        }

        if ((visual = choose_DB_visual(dpy, listVis, visclass, &depth)) == NULL) {
            fprintf(stderr, "%s: Failed to find matching double buffer capable visual.\n", ProgramName);
            exit(EXIT_FAILURE);
        }
        fprintf(stdout, "%s: Chose visual ID: %#4x depth: %d\n\n",
                ProgramName, (int)visual->visualid, depth);
    }
    else {
        /* No double buffering: ignore class, depth; use default visual. */
        visual = XDefaultVisual(dpy, screennum);
        depth  = XDefaultDepth(dpy,  screennum);
    }

    cmap = XCreateColormap(dpy, XRootWindowOfScreen(screen), visual, AllocNone);
    c_black  = getColor(cmap, "black");
    c_pink   = getColor(cmap, "pink");
    c_green  = getColor(cmap, "green");
    c_orange = getColor(cmap, "orange");
    c_blue   = getColor(cmap, "blue");
    attrs.colormap         = cmap;
    attrs.background_pixel = c_black;
    attrs.border_pixel     = c_black;
    win = XCreateWindow(dpy, XRootWindowOfScreen(screen), 
                        winrect.x, winrect.y, winrect.width, winrect.height,
                        0, depth, InputOutput, visual,
                        CWBorderPixel | CWColormap | CWBackPixel, &attrs);
    if (win == None) {
        fprintf(stderr, "%s: Couldn't window.\n", ProgramName);
        exit(EXIT_FAILURE);
    }

    XSetStandardProperties(dpy, win, "DBE dizzy demo", ProgramName, None,
                           argv, argc, NULL);
    XSelectInput(dpy, win,
                 VisibilityChangeMask | ExposureMask | ButtonPressMask | KeyPressMask |
                 StructureNotifyMask);
    if (do_db) {
        swapInfo.swap_action = XdbeBackground;
        buf = XdbeAllocateBackBufferName (dpy, win, swapInfo.swap_action);
        if (buf == None) {
            fprintf(stderr, "%s: Couldn't create buffers\n", ProgramName);
            exit(EXIT_FAILURE);
        }
        else {
            swapInfo.swap_window = win;
        }
    }
    else {
        buf = win; /* No double buffering. */
    }

    /* Create GCs, one per color (to avoid pipeline flushing
     * when the GC is changed) */
#ifdef USE_XPRINT
    gcvals.line_width = (8L * ((dpi_x+dpi_y)/2L)) / 100L; /* scale line with DPI */
#else
    gcvals.line_width = 8L;
#endif
    
    gcvals.cap_style  = CapRound;
#define CREATECOLORGC(cl) (gcvals.foreground = (cl), \
                           XCreateGC(dpy, win, GCForeground | GCLineWidth | GCCapStyle, &gcvals))
    gc_black  = CREATECOLORGC(c_black);
    gc_pink   = CREATECOLORGC(c_pink);
    gc_green  = CREATECOLORGC(c_green);
    gc_orange = CREATECOLORGC(c_orange);
    gc_blue   = CREATECOLORGC(c_blue);
#undef CREATECOLORGC

    XMapWindow(dpy, win);

    main_loop();

#ifdef USE_XPRINT
    if (doPrint) {
        char *scr;
        
        /* End the print job - the final results are sent by the X print
         * server to the spooler sub system.
         */
        Log(("finishing print job.\n"));

        /* Job completed, check if there are any messages from the spooler command */
        scr = XpGetOneAttribute(dpy, pcontext, XPJobAttr, "xp-spooler-command-results");
        if( scr )
        {
          if( strlen(scr) > 0 )
          {
            const char *msg = XpuCompoundTextToXmb(dpy, scr);
            if( msg )
            {
              Msg(("Spooler command returned:\n%s", msg));
              XpuFreeXmbString(msg);
            }
            else
            {
              Msg(("Spooler command returned (unconverted):\n%s", scr));
            }
          }

          XFree((void *)scr);
        }

        if (toFile) {
           if (XpuWaitForPrintFileChild(printtofile_handle) != XPGetDocFinished) {
              fprintf(stderr, "%s: Error while printing to file.\n", ProgramName);
              XpuClosePrinterDisplay(dpy, pcontext);
              return EXIT_FAILURE;
           }
        }

        XDestroyWindow(dpy, win);
        XpuClosePrinterDisplay(dpy, pcontext);

        XpuFreePrinterList(plist);
    }
    else
#endif
    {
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
    }
    
    Log(("Done."));

    return EXIT_SUCCESS;
}


