static const char CVSID[] = "$Id: help.c,v 1.86 2002/11/12 10:04:29 ajhood Exp $";
/*******************************************************************************
*									       *
* help.c -- Nirvana Editor help display					       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version.							               *
* 									       *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.							       *
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* September 10, 1991							       *
*									       *
* Written by Mark Edel, mostly rewritten by Steve Haehn for new help system,   *
* December, 2001   	    						       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "help.h"
#include "textBuf.h"
#include "text.h"
#include "textDisp.h"
#include "textP.h"
#include "textSel.h"
#include "nedit.h"
#include "search.h"
#include "window.h"
#include "preferences.h"
#include "help_data.h"
#include "file.h"
#include "highlight.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/system.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#include <Xm/Xm.h>
#include <Xm/XmP.h>         /* These are for applying style info to help text */
#include <Xm/Form.h>
#include <Xm/PrimitiveP.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/PushB.h>
#ifdef EDITRES
#include <X11/Xmu/Editres.h>
/* extern void _XEditResCheckMessages(); */
#endif /* EDITRES */

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/*============================================================================*/
/*                              SYMBOL DEFINITIONS                            */
/*============================================================================*/

#define EOS '\0'              /* end-of-string character                  */

#define CLICK_THRESHOLD 5     /* number of pixels mouse may move from its */
                              /* pressed location for mouse-up to be      */
                              /* considered a valid click (as opposed to  */
                              /* a drag or mouse-pick error)              */

/*============================================================================*/
/*                             VARIABLE DECLARATIONS                          */
/*============================================================================*/

static Widget HelpWindows[NUM_TOPICS] = {NULL}; 
static Widget HelpTextPanes[NUM_TOPICS] = {NULL};
static textBuffer *HelpStyleBuffers[NUM_TOPICS] = {NULL};
static int navHistForw[NUM_TOPICS];
static int navHistBack[NUM_TOPICS];

/* Information on the last search for search-again */
static char LastSearchString[DF_MAX_PROMPT_LENGTH] = "";
static int LastSearchTopic = -1;
static int LastSearchPos = 0;
static int LastSearchWasAllTopics = False;

/* Fonts for each help text style generated by the help generator (setext).
   The NEdit text widget uses the first help style, 'A', to calculate window
   width, so making 'A' a fixed font will yield window widths calibrated to
   match width-dependent fixed font layouts in the help text. */
static enum helpFonts StyleFonts[] =
{
    /* Fixed fonts, styles: 'A', 'B', 'C', 'D' */
    FIXED_HELP_FONT, BOLD_FIXED_HELP_FONT, BOLD_FIXED_HELP_FONT,
    BOLD_ITALIC_FIXED_HELP_FONT,

    /* Underlined fixed fonts, styles: 'E', 'F', 'G', 'H' */
    FIXED_HELP_FONT, BOLD_FIXED_HELP_FONT, BOLD_FIXED_HELP_FONT,
    BOLD_ITALIC_FIXED_HELP_FONT,

    /* Normal (proportional) fonts, styles: 'I', 'J', 'K', 'L' */
    HELP_FONT, BOLD_HELP_FONT, ITALIC_HELP_FONT, BOLD_ITALIC_HELP_FONT,

    /* Underlined fonts, styles: 'M', 'N', 'O', 'P' */
    HELP_FONT, BOLD_HELP_FONT, ITALIC_HELP_FONT, BOLD_ITALIC_HELP_FONT,

    /* Link font, style: 'Q' */
    HELP_FONT,

    /* Heading fonts, styles: 'R', 'S', 'T' */
    H1_HELP_FONT, H2_HELP_FONT,  H3_HELP_FONT
};

static int StyleUnderlines[] =
{
    /* Fixed fonts, styles: 'A', 'B', 'C', 'D' */
    False, False, False, False,

    /* Underlined fixed fonts, styles: 'E', 'F', 'G', 'H' */
    True, True, True, True,

    /* Normal (proportional) fonts, styles: 'I', 'J', 'K', 'L' */
    False, False, False, False,

    /* Underlined fonts, styles: 'M', 'N', 'O', 'P' */
    True, True, True, True,

    /* Link font, style: 'Q' */
    True,

    /* Heading fonts, styles: 'R', 'S', 'T' */
    False, False, False
};

#define N_STYLES (XtNumber(StyleFonts))

static styleTableEntry HelpStyleInfo[ N_STYLES ];

/*============================================================================*/
/*                             PROGRAM PROTOTYPES                             */
/*============================================================================*/

static Widget createHelpPanel(Widget parent, int topic);
static void dismissCB(Widget w, XtPointer clientData, XtPointer callData);
static void prevTopicCB(Widget w, XtPointer clientData, XtPointer callData);
static void nextTopicCB(Widget w, XtPointer clientData, XtPointer callData);
static void bwHistoryCB(Widget w, XtPointer clientData, XtPointer callData);
static void fwHistoryCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchHelpCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchHelpAgainCB(Widget w, XtPointer clientData,
	XtPointer callData);
static void printCB(Widget w, XtPointer clientData, XtPointer callData);
static char *stitch(Widget  parent, char **string_list,char **styleMap);
static void searchHelpText(Widget parent, int parentTopic, 
      	const char *searchFor, int allSections, int startPos, int startTopic);
static void changeWindowTopic(int existingTopic, int newTopic);
static int findTopicFromShellWidget(Widget shellWidget);
static void loadFontsAndColors(Widget parent, int style);
static void initNavigationHistory(void);

#ifdef HAVE__XMVERSIONSTRING
extern char _XmVersionString[];
#endif

/*============================================================================*/
/*================================= PROGRAMS =================================*/
/*============================================================================*/

/*
** Create a string containing information on the build environment.  Returned
** string must NOT be freed by caller.
*/
static const char *getBuildInfo(void)
{
    const char * bldFormat =
        "%s\n"
        "     Built on: %s, %s, %s\n"
        "     Built at: %s, %s\n"
        "   With Motif: %d.%d.%d [%s]\n"
#ifdef HAVE__XMVERSIONSTRING
        "Running Motif: %d.%d [%s]\n"
#else
        "Running Motif: %d.%d\n"
#endif
        "       Server: %s %d\n"
        "       Visual: %s\n"
       ;
    const char * visualClass[] = {"StaticGray",  "GrayScale",
                                  "StaticColor", "PseudoColor",
                                  "TrueColor",   "DirectColor"};
    static char * bldInfoString=NULL;
    
    if (bldInfoString==NULL)
    {
        char        visualStr[30]="";
        if (TheDisplay) {
            Visual     *visual;
            int         depth;
            Colormap    map;
            Boolean usingDefaultVisual = FindBestVisual(TheDisplay, APP_NAME,
                                                        APP_CLASS, &visual,
                                                        &depth, &map);
            sprintf(visualStr,"Id %#lx %s %d bit%s", visual->visualid,
                    visualClass[visual->class], depth,
                    usingDefaultVisual ?" (Default)":"");
        }
       bldInfoString = XtMalloc (strlen (bldFormat)  + 1024);
       sprintf(bldInfoString, bldFormat,
            NEditVersion,
            COMPILE_OS, COMPILE_MACHINE, COMPILE_COMPILER,
            linkdate, linktime,
            XmVERSION, XmREVISION, XmUPDATE_LEVEL,
            XmVERSION_STRING,
            xmUseVersion/1000, xmUseVersion%1000,
#ifdef HAVE__XMVERSIONSTRING
            _XmVersionString,
#endif
            ServerVendor(TheDisplay), VendorRelease(TheDisplay),
            visualStr
            );
    }
    
    return bldInfoString;
}

/*
** Initialization for help system data, needs to be done only once.
*/
static void initHelpStyles (Widget parent) 
{
    static int styleTableInitialized = False;
    
    if (! styleTableInitialized) 
    {
        Pixel black = BlackPixelOfScreen(XtScreen(parent));
        int   styleIndex;
        char ** line;

        for (styleIndex = 0; styleIndex < STL_HD + MAX_HEADING; styleIndex++) 
        {
            HelpStyleInfo[ styleIndex ].color     = black;
            HelpStyleInfo[ styleIndex ].underline = StyleUnderlines[styleIndex];
            HelpStyleInfo[ styleIndex ].font      = NULL;
        }

        styleTableInitialized  = True;

        /*-------------------------------------------------------
        * Only attempt to add build information to version text
        * when string formatting symbols are present in the text.
        * This special case is needed to incorporate this 
        * dynamically created information into the static help.
        *-------------------------------------------------------*/
        for (line = HelpText[ HELP_VERSION ]; *line != NULL; line++) 
        {
            /*--------------------------------------------------
            * If and when this printf format is found in the
            * version help text, replace that line with the
            * build information. Then stitching the help text
            * will have the final count of characters to use.
            *--------------------------------------------------*/
            if (strstr (*line, "%s")  != NULL) 
            {
                const char * bldInfo  = getBuildInfo();
                char * text     = XtMalloc (strlen (*line)  + strlen (bldInfo));
                sprintf (text, *line, bldInfo);
                *line = text;
                break;
            }
        }
    }
}

/*
** Help fonts are not loaded until they're actually needed.  This function
** checks if the style's font is loaded, and loads it if it's not.
*/
static void loadFontsAndColors(Widget parent, int style)
{
    XFontStruct *font;
    int r,g,b;
    if (HelpStyleInfo[style - STYLE_PLAIN].font == NULL) {
	font = XLoadQueryFont(XtDisplay(parent),
		GetPrefHelpFontName(StyleFonts[style - STYLE_PLAIN]));
	if (font == NULL) {
	    fprintf(stderr, "NEdit: help font, %s, not available\n",
		    GetPrefHelpFontName(StyleFonts[style - STYLE_PLAIN]));
	    font = XLoadQueryFont(XtDisplay(parent), "fixed");
	    if (font == NULL) {
		fprintf(stderr, "NEdit: fallback help font, \"fixed\", not "
			"available, cannot continue\n");
		exit(EXIT_FAILURE);
	    }
	}
	HelpStyleInfo[style - STYLE_PLAIN].font = font;
	if (style == STL_NM_LINK)
	    HelpStyleInfo[style - STYLE_PLAIN].color =
		    AllocColor(parent, GetPrefHelpLinkColor(), &r, &g, &b);
    }
}

static void adaptNavigationButtons(int topic) {
    Widget btn;
    
    if(HelpWindows[topic] == NULL)
      	return; /* Shouldn't happen */
    
    btn=XtNameToWidget(HelpWindows[topic], "helpForm.prevTopic");
    if(btn) {
      	if(topic > 0)
	  XtSetSensitive(btn, True);
	else
	  XtSetSensitive(btn, False);
    }

    btn=XtNameToWidget(HelpWindows[topic], "helpForm.nextTopic");
    if(btn) {
      	if(topic < (NUM_TOPICS - 1))
	  XtSetSensitive(btn, True);
	else
	  XtSetSensitive(btn, False);
    }

    btn=XtNameToWidget(HelpWindows[topic], "helpForm.histBack");
    if(btn) {
      	if(navHistBack[topic] != -1)
	  XtSetSensitive(btn, True);
	else
	  XtSetSensitive(btn, False);
    }

    btn=XtNameToWidget(HelpWindows[topic], "helpForm.histForw");
    if(btn) {
      	if(navHistForw[topic] != -1)
	  XtSetSensitive(btn, True);
	else
	  XtSetSensitive(btn, False);
    }

}

/*
** Put together stored help strings to create the text and optionally the style
** information for a given help topic.
*/
static char * stitch (

    Widget  parent, 	 /* used for dynamic font/color allocation */
    char ** string_list, /* given help strings to stitch together */
    char ** styleMap     /* NULL, or a place to store help styles */
)
{
    char  *  cp;
    char  *  section, * sp;       /* resulting help text section            */
    char  *  styleData, * sdp;    /* resulting style data for text          */
    char     style = STYLE_PLAIN; /* start off each section with this style */
    int      total_size = 0;      /* help text section size                 */
    char  ** crnt_line;

    /*----------------------------------------------------
    * How many characters are there going to be displayed?
    *----------------------------------------------------*/
    for (crnt_line = string_list; *crnt_line != NULL; crnt_line++) 
    {
        for (cp = *crnt_line; *cp != EOS; cp++) 
        {
            /*---------------------------------------------
            * The help text has embedded style information
            * consisting of the style marker and a single
            * character style, for a total of 2 characters.
            * This style information is not to be included
            * in the character counting below.
            *---------------------------------------------*/
            if (*cp != STYLE_MARKER)  {
                total_size++;
            }
            else {
                cp++;  /* skipping style marker, loop will handle style */
            }
        }
    }
    
    /*--------------------------------------------------------
    * Get the needed space, one area for the help text being
    * stitched together, another for the styles to be applied.
    *--------------------------------------------------------*/
    sp  = section   = XtMalloc (total_size +1);
    sdp = styleData = (styleMap) ? XtMalloc (total_size +1)  : NULL;
    *sp = EOS;
    
    /*--------------------------------------------
    * Fill in the newly acquired contiguous space
    * with help text and style information.
    *--------------------------------------------*/
    for (crnt_line = string_list; *crnt_line != NULL; crnt_line++) 
    {
        for (cp = *crnt_line; *cp != EOS; cp++) 
        {
            if (*cp == STYLE_MARKER)  {
                style = *(++cp);
		loadFontsAndColors(parent, style);
            } 
            else {
                *(sp++)  = *cp;
                
                if (styleMap) 
                    *(sdp++) = style;
            }
        }
    }
    
    *sp = EOS;
    
    /*-----------------------------------------
    * Only deal with style map, when available.
    *-----------------------------------------*/
    if (styleMap)  {
        *styleMap = styleData;
        *sdp      = EOS;
    }

    return section;
}

/*
** Display help for subject "topic".  "parent" is a widget over which the help
** dialog may be posted.  Help dialogs are preserved when popped down by the
** user, and may appear posted over a previous parent, regardless of the parent
** argument.
*/
void Help(Widget parent, enum HelpTopic topic)
{
    if (HelpWindows[topic] != NULL)
    	RaiseShellWindow(HelpWindows[topic]);
    else
    	HelpWindows[topic] = createHelpPanel(parent, topic);
    adaptNavigationButtons(topic);
}


/* Setup Window/Icon title for the help window. */
static void setHelpWinTitle(Widget win, enum HelpTopic topic) 
{
    char * buf, *topStr=HelpTitles[topic];
    
    buf=malloc(strlen(topStr) + 24);
    topic++; 
    
    sprintf(buf, "NEdit Help (%d)", (int)topic);
    XtVaSetValues(win, XmNiconName, buf, NULL);
    
    sprintf(buf, "NEdit Help: %s (%d)", topStr, (int)topic);
    XtVaSetValues(win, XmNtitle, buf, NULL);
  
    free(buf);
}
	
/*
** Create a new help window displaying a given subject, "topic"
**
** Importand hint: If this widget is restructured or the name of the text
** subwidget is changed don't forget to adapt the default translations of the
** help text. They are located in nedit.c, look for 
**   static char *fallbackResources 
**   (currently:  nedit.helpForm.sw.helpText*translations...)
*/
static Widget createHelpPanel(Widget parent, int topic)
{
    Arg al[50];
    int ac;
    Widget appShell, btn, dismissBtn, form, btnFW;
    Widget sw, hScrollBar, vScrollBar;
    XmString st1;
    char * helpText  = NULL;
    char * styleData = NULL;

    ac = 0;
    XtSetArg(al[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
    appShell = CreateShellWithBestVis(APP_NAME, APP_CLASS,
	    applicationShellWidgetClass, TheDisplay, al, ac);
    AddSmallIcon(appShell);
    /* With openmotif 2.1.30, a crash may occur when the text widget of the
       help window is (slowly) resized to a zero width. By imposing a 
       minimum _window_ width, we can work around this problem. The minimum 
       width should be larger than the width of the scrollbar. 50 is probably 
       a safe value; this leaves room for a few characters */
    XtVaSetValues(appShell, XtNminWidth, 50, NULL);
    form = XtVaCreateManagedWidget("helpForm", xmFormWidgetClass, appShell, 
      	    NULL);
    XtVaSetValues(form, XmNshadowThickness, 0, NULL);
    
    /* Create the bottom row of buttons */
    btn = XtVaCreateManagedWidget("find", xmPushButtonWidgetClass, form,
    	    XmNlabelString, st1=XmStringCreateSimple("Find..."),
    	    XmNmnemonic, 'F',
    	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 3,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 25, NULL);
    XtAddCallback(btn, XmNactivateCallback, searchHelpCB, appShell);
    XmStringFree(st1);

    btn = XtVaCreateManagedWidget("findAgain", xmPushButtonWidgetClass, form,
    	    XmNlabelString, st1=XmStringCreateSimple("Find Again"),
    	    XmNmnemonic, 'A',
   	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 27,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 49, NULL);
    XtAddCallback(btn, XmNactivateCallback, searchHelpAgainCB, appShell);
    XmStringFree(st1);

    btn = XtVaCreateManagedWidget("print", xmPushButtonWidgetClass, form,
    	    XmNlabelString, st1=XmStringCreateSimple("Print..."),
    	    XmNmnemonic, 'P',
   	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 51,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 73, NULL);
    XtAddCallback(btn, XmNactivateCallback, printCB, appShell);
    XmStringFree(st1);

    dismissBtn = XtVaCreateManagedWidget("dismiss", xmPushButtonWidgetClass,
	    form, XmNlabelString, st1=XmStringCreateSimple("Dismiss"),
   	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 75,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 97, NULL);
    XtAddCallback(dismissBtn, XmNactivateCallback, dismissCB, appShell);
    XmStringFree(st1);
            
    /* Create the next row of buttons (for navigation) */
    btn = XtVaCreateManagedWidget("prevTopic", xmPushButtonWidgetClass, 
      	    form, XmNlabelString, st1=XmStringCreateSimple("<< Browse"),
    	    XmNmnemonic, 'o', 
    	    XmNbottomAttachment, XmATTACH_WIDGET,
      	    XmNbottomWidget, dismissBtn,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 51,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 73, NULL);
    XtAddCallback(btn, XmNactivateCallback, prevTopicCB, appShell);
    XmStringFree(st1);

    btn = XtVaCreateManagedWidget("nextTopic", xmPushButtonWidgetClass, form,
    	    XmNlabelString, st1=XmStringCreateSimple("Browse >>"),
    	    XmNmnemonic, 'e', 
    	    XmNbottomAttachment, XmATTACH_WIDGET,
      	    XmNbottomWidget, dismissBtn,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 75, 
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 97, NULL);
    XtAddCallback(btn, XmNactivateCallback, nextTopicCB, appShell);
    XmStringFree(st1);

    btn = XtVaCreateManagedWidget("histBack", xmPushButtonWidgetClass, form,
    	    XmNlabelString, st1=XmStringCreateSimple("Back"),
    	    XmNmnemonic, 'B', 
    	    XmNbottomAttachment, XmATTACH_WIDGET,
      	    XmNbottomWidget, dismissBtn,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 3,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 25, NULL);
    XtAddCallback(btn, XmNactivateCallback, bwHistoryCB, appShell);
    XmStringFree(st1);

    btnFW = XtVaCreateManagedWidget("histForw", xmPushButtonWidgetClass, form,
    	    XmNlabelString, st1=XmStringCreateSimple("Forward"),
      	    XmNmnemonic, 'w', 
    	    XmNbottomAttachment, XmATTACH_WIDGET,
      	    XmNbottomWidget, dismissBtn,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 27,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 49, NULL);
    XtAddCallback(btnFW, XmNactivateCallback, fwHistoryCB, appShell);
    XmStringFree(st1);
    
    /* Create a text widget inside of a scrolled window widget */
    sw = XtVaCreateManagedWidget("sw", xmScrolledWindowWidgetClass, form,
 	    XmNshadowThickness, 2,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
    	    XmNrightAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, btnFW, NULL);
    hScrollBar = XtVaCreateManagedWidget("hScrollBar",
    	    xmScrollBarWidgetClass, sw, XmNorientation, XmHORIZONTAL, 
    	    XmNrepeatDelay, 10, NULL);
    vScrollBar = XtVaCreateManagedWidget("vScrollBar",
    	    xmScrollBarWidgetClass, sw, XmNorientation, XmVERTICAL,
    	    XmNrepeatDelay, 10, NULL);
    HelpTextPanes[topic] = XtVaCreateManagedWidget("helpText",
	    textWidgetClass, sw, textNrows, 30, textNcolumns, 65,
	    textNbacklightCharTypes, NULL,
    	    textNhScrollBar, hScrollBar, textNvScrollBar, vScrollBar,
	    textNreadOnly, True, textNcontinuousWrap, True,
	    textNautoShowInsertPos, True, NULL);
    XtVaSetValues(sw, XmNworkWindow, HelpTextPanes[topic],
	    XmNhorizontalScrollBar, hScrollBar,
    	    XmNverticalScrollBar, vScrollBar, NULL);
    
    /* Initialize help style information, if it hasn't already been init'd */
    initHelpStyles (parent);
    
    /* Put together the text to display and separate it into parallel text
       and style data for display by the widget */
    helpText = stitch (parent, HelpText[topic], &styleData);
    
    /* Stuff the text into the widget's text buffer */
    BufSetAll (TextGetBuffer (HelpTextPanes[topic]) , helpText);
    XtFree (helpText);
    
    /* Create a style buffer for the text widget and fill it with the style
       data which was generated along with the text content */
    HelpStyleBuffers[topic] = BufCreate(); 
    BufSetAll(HelpStyleBuffers[topic], styleData);
    XtFree (styleData);
    TextDAttachHighlightData(((TextWidget)HelpTextPanes[topic])->text.textD,
    	    HelpStyleBuffers[topic], HelpStyleInfo, N_STYLES, '\0', NULL, NULL);
    
    /* This shouldn't be necessary (what's wrong in text.c?) */
    HandleXSelections(HelpTextPanes[topic]);
    
    /* Process dialog mnemonic keys */
    AddDialogMnemonicHandler(form, FALSE);
    
    /* Set the default button */
    XtVaSetValues(form, XmNdefaultButton, dismissBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, dismissBtn, NULL);
    
    /* realize all of the widgets in the new window */
    RealizeWithoutForcingPosition(appShell);
    
    /* Give the text pane the initial focus */
    XmProcessTraversal(HelpTextPanes[topic], XmTRAVERSE_CURRENT);

    /* Make close command in window menu gracefully prompt for close */
    AddMotifCloseCallback(appShell, (XtCallbackProc)dismissCB, appShell);
    
    /* Initialize navigation information, if it hasn't already been init'd */
    initNavigationHistory();
    
    /* Set the window title */
    setHelpWinTitle(appShell, topic);
    
    
#ifdef EDITRES
    XtAddEventHandler (appShell, (EventMask)0, True,
		(XtEventHandler)_XEditResCheckMessages, NULL);
#endif /* EDITRES */
    
    return appShell;
}

static void changeTopicOrRaise(int existingTopic, int newTopic) {
    if(HelpWindows[newTopic] == NULL) {
      	changeWindowTopic(existingTopic, newTopic);
	adaptNavigationButtons(newTopic);
    } else {
      	RaiseShellWindow(HelpWindows[newTopic]);
    	adaptNavigationButtons(existingTopic);
	adaptNavigationButtons(newTopic);
    }

}

/*
** Callbacks for window controls
*/
static void dismissCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int topic;
    
    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return;
    
    /* I don't understand the mechanism by which this can be called with
       HelpWindows[topic] as NULL, but it has happened */
    XtDestroyWidget(HelpWindows[topic]);
    HelpWindows[topic] = NULL;
    if (HelpStyleBuffers[topic] != NULL) {
	BufFree(HelpStyleBuffers[topic]);
	HelpStyleBuffers[topic] = NULL;
    }        
}

static void prevTopicCB(Widget w, XtPointer clientData, XtPointer callData) 
{   int topic;

    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return; /* shouldn't happen */
    
    topic--;
    if(topic >= 0)
      	changeTopicOrRaise(topic+1, topic);
}

static void nextTopicCB(Widget w, XtPointer clientData, XtPointer callData) 
{   int topic;

    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return; /* shouldn't happen */
  
    topic++;
    if(topic < NUM_TOPICS)
      	changeTopicOrRaise(topic-1, topic);
}

static void bwHistoryCB(Widget w, XtPointer clientData, XtPointer callData) 
{   int topic, goTo;

    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return; /* shouldn't happen */
    
    goTo=navHistBack[topic];
    if(goTo >= 0 && goTo < NUM_TOPICS) {
      	navHistForw[goTo]=topic;
	changeTopicOrRaise(topic, goTo); 
    }
}

static void fwHistoryCB(Widget w, XtPointer clientData, XtPointer callData)
{   int topic, goTo;

    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return; /* shouldn't happen */
      
    goTo=navHistForw[topic];
    if(goTo >= 0 && goTo < NUM_TOPICS) {
      	navHistBack[goTo]=topic;
	changeTopicOrRaise(topic, goTo); 
    }
}

static void searchHelpCB(Widget w, XtPointer clientData, XtPointer callData)
{
    char promptText[DF_MAX_PROMPT_LENGTH];
    int response, topic;
    static char **searchHistory = NULL;
    static int nHistoryStrings = 0;
    
    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return; /* shouldn't happen */
    SetDialogFPromptHistory(searchHistory, nHistoryStrings);
    response = DialogF(DF_PROMPT, HelpWindows[topic], 3,
	    "Search for:    (use up arrow key to recall previous)",
    	    promptText, "This Section", "All Sections", "Cancel");
    if (response == 3)
    	return;
    AddToHistoryList(promptText, &searchHistory, &nHistoryStrings);
    searchHelpText(HelpWindows[topic], topic, promptText, response == 2, 0, 0);
}

static void searchHelpAgainCB(Widget w, XtPointer clientData,
	XtPointer callData)
{
    int topic;
    
    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return; /* shouldn't happen */
    searchHelpText(HelpWindows[topic], topic, LastSearchString,
	    LastSearchWasAllTopics, LastSearchPos, LastSearchTopic);
}

static void printCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int topic, helpStringLen;
    char *helpString;
    
    if ((topic = findTopicFromShellWidget((Widget)clientData)) == -1)
    	return; /* shouldn't happen */
    helpString = TextGetWrapped(HelpTextPanes[topic], 0,
	    TextGetBuffer(HelpTextPanes[topic])->length, &helpStringLen);
    PrintString(helpString, helpStringLen, HelpWindows[topic],
	    HelpTitles[topic]);
    XtFree(helpString);
}


/*
** Find the topic and text position within that topic targeted by a hyperlink
** with name "link_name". Returns true if the link was successfully interpreted.
*/
static int is_known_link(char *link_name, int *topic, int *textPosition)
{
    Href * hypertext;
    
    /*------------------------------
    * Direct topic links found here.
    *------------------------------*/
    for (*topic=0; HelpTitles[*topic] != NULL; (*topic)++) 
    {
        if (strcmp (link_name, HelpTitles[*topic])  == 0) 
        {
            *textPosition = 0;
            return 1;
        }
    }
    
    /*------------------------------------
    * Links internal to topics found here.
    *------------------------------------*/
    for (hypertext = &H_R[0]; hypertext != NULL; hypertext = hypertext->next) 
    {
        if (strcmp (link_name, hypertext->source)  == 0) 
        {
            *topic  = hypertext->topic;
            *textPosition = hypertext->location;
            return 1;
        }
    }

   return 0;
}

/*
** Find the text of a hyperlink from a clicked character position somewhere
** within the hyperlink text, and display the help that it links to.
*/
static void follow_hyperlink(int topic, int charPosition, int newWindow)
{
    textDisp *textD = ((TextWidget)HelpTextPanes[topic])->text.textD;
    char * link_text;
    int    link_topic;
    int    link_pos;
    int end        = charPosition;
    int begin      = charPosition;
    char whatStyle = BufGetCharacter(textD->styleBuffer, end);
    
    /*--------------------------------------------------
    * Locate beginning and ending of current text style.
    *--------------------------------------------------*/
    while (whatStyle == BufGetCharacter(textD->styleBuffer, ++end));
    while (whatStyle == BufGetCharacter(textD->styleBuffer, begin-1))  begin--;

    link_text = BufGetRange (textD->buffer, begin, end);
    
    if (is_known_link (link_text, &link_topic, &link_pos) ) 
    {
      	if (HelpWindows[link_topic] != NULL) {
    	    RaiseShellWindow(HelpWindows[link_topic]);
	} else {
  	    if(newWindow) {
    	      	HelpWindows[link_topic] = createHelpPanel(HelpWindows[topic], 
	      	      	link_topic);
	      	
	    } else {
	      	changeWindowTopic(topic, link_topic);
	    }
	}
	navHistBack[link_topic] = topic;
	navHistForw[topic] = link_topic;
        TextSetCursorPos(HelpTextPanes[link_topic], link_pos);
	adaptNavigationButtons(link_topic);
	adaptNavigationButtons(topic);
    }
    XtFree (link_text);
}

static void helpFocusButtonsAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{   
    XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
}

/* 
 * handler for help-button-action(<button-name>)
 * Calls the activate callback for the named button widget of the help text win.
 */
static void helpButtonActionAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    char buf[80];
    int topic;
    Widget btn;
    
    if(*nArgs != 1) {
      	fprintf(stderr, "help-button-action: requires exactly one argument.\n");
	return;
    }
    
    /* Find the topic being displayed by this widget */
    for (topic = 0; topic < NUM_TOPICS; topic++)
	if (HelpTextPanes[topic] == w)
	    break;
    
    if(topic == NUM_TOPICS || HelpWindows[topic] == NULL)
      	return; /* Shouldn't happen */

    /* Compose the button widget name */    
    strcpy(&buf[0], "helpForm.");
    if (strlen(args[0]) <= 70) {
	strcat(&buf[0], args[0]);
    }
    else {
	fprintf(stderr, "help-button-action: argument too long");
	return;
    }
    
    btn=XtNameToWidget(HelpWindows[topic], buf);
    if(btn) {
      	XtCallCallbacks(btn, XmNactivateCallback, HelpWindows[topic]);
    } else {
      	fprintf(stderr, "help-button-action: invalid argument: %s\n", args[0]);
    }
}

/* 
 * Handler for action help-hyperlink()
 * Arguments: none - init: record event position 
 *            "current": if clicked on a link, follow link in same window
 *            "new":     if clicked on a link, follow link in new window
 *
 * With the 1st argument "current" or "new" this action can have two additional
 * arguments. These arguments must be valid names of XmText actions. 
 * In this case, the action named in argument #2 is called if the action
 * help-hyperlink is about to follow the hyperlink. The Action in argument #3
 * is called if no hyperlink has been recognized at the current event position.
 */
static void helpHyperlinkAP(Widget w, XEvent *event, String *args,
	Cardinal *nArgs)
{
    XButtonEvent *e = (XButtonEvent *)event;
    int topic;
    textDisp *textD = ((TextWidget)w)->text.textD;
    int clickedPos, newWin;
    static int pressX=0, pressY=0;
    
    /* called without arguments we just record coordinates */
    if(*nArgs == 0) {
	pressX = e->x;
	pressY = e->y;
      	return;
    }
    
    newWin = !strcmp(args[0], "new");
    
    if(!newWin && strcmp(args[0], "current")) {
	fprintf(stderr, "help-hyperlink: Unrecognized argument %s\n", args[0]);
	return;
    }
    
    /* 
     * If for any reason (pointer moved - drag!, no hyperlink found) 
     * this action can't follow a hyperlink then execute the the action 
     * named in arg #3 (if provided)
     */
    if (abs(pressX - e->x) > CLICK_THRESHOLD || 
      	abs(pressY - e->y) > CLICK_THRESHOLD) {
	if(*nArgs == 3)
      	    XtCallActionProc(w, args[2], event, NULL, 0);
	return;
    }
    
    clickedPos = TextDXYToCharPos(textD, e->x, e->y);
    if (BufGetCharacter(textD->styleBuffer, clickedPos) != STL_NM_LINK){
	if(*nArgs == 3)
      	    XtCallActionProc(w, args[2], event, NULL, 0);
	return;
    }

    /* Find the topic being displayed by this widget */
    for (topic = 0; topic < NUM_TOPICS; topic++)
	if (HelpTextPanes[topic] == w)
	    break;
    
    if (topic == NUM_TOPICS){
      	/* If we get here someone must have bound help-hyperlink to a non-help
	 * text widget (!) Or some other really strange thing happened.
	 */  	
	if(*nArgs == 3)
      	    XtCallActionProc(w, args[2], event, NULL, 0);
	return;
    }
    
    /* If the action help-hyperlink had 3 arguments execute the action
     * named in arg #2 before really following the link.
     */
    if(*nArgs == 3)
      	XtCallActionProc(w, args[1], event, NULL, 0);
    
    follow_hyperlink(topic, clickedPos, newWin);
}  
 
/*
** Install the action for following hyperlinks in the help window
*/
void InstallHelpLinkActions(XtAppContext context)
{   
    static XtActionsRec Actions[] = {
      	{"help-hyperlink", helpHyperlinkAP},
	{"help-focus-buttons", helpFocusButtonsAP},
	{"help-button-action", helpButtonActionAP}
    };

    XtAppAddActions(context, Actions, XtNumber(Actions));
}

/*
** Search the help text.  If allSections is true, searches all of the help
** text, otherwise searches only in parentTopic.
*/
static void searchHelpText(Widget parent, int parentTopic, 
      	const char *searchFor, int allSections, int startPos, int startTopic)
{    
    int topic, beginMatch, endMatch;
    int found = False;
    char * helpText  = NULL;
    
    /* Search for the string */
    for (topic=startTopic; topic<NUM_TOPICS; topic++) {
	if (!allSections && topic != parentTopic)
	    continue;
        helpText = stitch(parent, HelpText[topic], NULL);
	if (SearchString(helpText, searchFor, SEARCH_FORWARD,
		SEARCH_LITERAL, False, topic == startTopic ? startPos : 0,
		&beginMatch, &endMatch, NULL, NULL, GetPrefDelimiters())) {
	    found = True;
	    XtFree(helpText);
    	    break;
	}
        XtFree(helpText);
    }
    if (!found) {
	if (startPos != 0 || (allSections && startTopic != 0)) { 
	    /* Wrap search */
	    searchHelpText(parent, parentTopic, searchFor, allSections, 0, 0);
	    return;
    	}
	DialogF(DF_INF, parent, 1, "String Not Found", "Dismiss");
	return;
    }
    
    /* update navigation history */  
    if(parentTopic != topic) {
      	navHistForw[parentTopic]= topic;
	navHistBack[topic]= parentTopic;
    }
    
    /* If the appropriate window is already up, bring it to the top, if not,
       make the parent window become this topic */
    changeTopicOrRaise(parentTopic, topic);
    BufSelect(TextGetBuffer(HelpTextPanes[topic]), beginMatch, endMatch);
    TextSetCursorPos(HelpTextPanes[topic], endMatch);
    
    /* Save the search information for search-again */
    strcpy(LastSearchString, searchFor);
    LastSearchTopic = topic;
    LastSearchPos = endMatch;
    LastSearchWasAllTopics = allSections;
}

/*
** Change a help window to display a new topic.  (Help window data is stored
** and indexed by topic so if a given topic is already displayed or has been
** positioned by the user, it can be found and popped back up in the same
** place.)  To change the topic displayed, the stored data has to be relocated.
*/
static void changeWindowTopic(int existingTopic, int newTopic)
{
    char *helpText, *styleData;
    
    /* Relocate the window/widget/buffer information */
    if(newTopic != existingTopic) {
      	HelpWindows[newTopic] = HelpWindows[existingTopic];
      	HelpWindows[existingTopic] = NULL;
      	HelpStyleBuffers[newTopic] = HelpStyleBuffers[existingTopic];
      	HelpStyleBuffers[existingTopic] = NULL;
      	HelpTextPanes[newTopic] = HelpTextPanes[existingTopic];
      	HelpTextPanes[existingTopic] = NULL;
	setHelpWinTitle(HelpWindows[newTopic], newTopic);
    } 
    
    /* Set the existing text widget to display the new text.  Because it's
       highlighted, we have to turn off highlighting before changing the
       displayed text to prevent the text widget from trying to apply the
       old, mismatched, highlighting to the new text */
    helpText = stitch(HelpTextPanes[newTopic], HelpText[newTopic], &styleData);
    TextDAttachHighlightData(((TextWidget)HelpTextPanes[newTopic])->text.textD,
    	    NULL, NULL, 0, '\0', NULL, NULL);
    BufSetAll(TextGetBuffer(HelpTextPanes[newTopic]), helpText);
    XtFree(helpText);
    BufSetAll(HelpStyleBuffers[newTopic], styleData);
    XtFree(styleData);
    TextDAttachHighlightData(((TextWidget)HelpTextPanes[newTopic])->text.textD,
    	    HelpStyleBuffers[newTopic], HelpStyleInfo, N_STYLES, '\0',
	    NULL, NULL);
}

static int findTopicFromShellWidget(Widget shellWidget)
{
    int i;
    
    for (i=0; i<NUM_TOPICS; i++)
	if (shellWidget == HelpWindows[i])
	    return i;
    return -1;
}

static void initNavigationHistory(void) {
    static int doInitNavigationHistory = True;
    int i;
    
    if(doInitNavigationHistory) {
      	for(i=0; i<NUM_TOPICS; i++)
      	    navHistBack[i] = navHistForw[i] = -1;
	
	doInitNavigationHistory = False;
    }
}

#if XmVersion == 2000
/* amai: This function may be called before the Motif part
         is being initialized. The following, public interface
         is known to initialize at least xmUseVersion.
	 That interface is declared in <Xm/Xm.h> in Motif 1.2 only.
	 As for Motif 2.1 we don't need this call anymore.
	 This also holds for the Motif 2.1 version of LessTif
	 releases > 0.93.0. */
extern void XmRegisterConverters(void);
#endif


/* Print version info to stdout */
void PrintVersion(void)
{
    const char *text;
  
#if XmVersion < 2001
    XmRegisterConverters();  /* see comment above */
#endif
    text = getBuildInfo();
    puts (text);
}
