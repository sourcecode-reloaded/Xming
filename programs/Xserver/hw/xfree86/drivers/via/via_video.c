/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * I N C L U D E S
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"
#include "via_driver.h"
#include "via_video.h"

#include "via.h"

#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "via_xvpriv.h"
#include "via_swov.h"
#include "via_memcpy.h"
#include "via_id.h"
#include "fourcc.h"

/*
 * D E F I N E
 */
#define OFF_DELAY       200  /* milliseconds */
#define FREE_DELAY      60000
#define PARAMSIZE       1024
#define SLICESIZE       65536       
#define OFF_TIMER       0x01
#define FREE_TIMER      0x02
#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)
#define VIA_MAX_XVIMAGE_X 1920
#define VIA_MAX_XVIMAGE_Y 1200

#define LOW_BAND 0x0CB0
#define MID_BAND 0x1f10

#define  XV_IMAGE          0
#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

#ifndef XvExtension
void viaInitVideo(ScreenPtr pScreen) {}
void viaExitVideo(ScrnInfoPtr pScrn) {}
void viaSaveVideo(ScrnInfoPtr pScrn) {}
void viaRestoreVideo(ScrnInfoPtr pScrn) {}
void VIAVidAdjustFrame(ScrnInfoPtr pScrn, int x, int y) {}
#else

static vidCopyFunc viaFastVidCpy = NULL;


/*
 *  F U N C T I O N   D E C L A R A T I O N
 */
static unsigned viaSetupAdaptors(ScreenPtr pScreen, XF86VideoAdaptorPtr **adaptors);
static void viaStopVideo(ScrnInfoPtr, pointer, Bool);
static void viaQueryBestSize(ScrnInfoPtr, Bool,
        short, short, short, short, unsigned int *, unsigned int *, pointer);
static int viaQueryImageAttributes(ScrnInfoPtr, 
        int, unsigned short *, unsigned short *,  int *, int *);
static int viaGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static int viaSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int viaPutImage(ScrnInfoPtr, short, short, short, short, short, short, 
		       short, short,int, unsigned char*, short, short, Bool, 
		       RegionPtr, pointer);

static Atom xvBrightness, xvContrast, xvColorKey, xvHue, xvSaturation, xvAutoPaint;

/*
 *  S T R U C T S
 */
/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] =
{
  { XV_IMAGE        , "XV_IMAGE",VIA_MAX_XVIMAGE_X,VIA_MAX_XVIMAGE_Y,{1, 1}},
};

#define NUM_FORMATS_G 9

static XF86VideoFormatRec FormatsG[NUM_FORMATS_G] = 
{
  { 8, TrueColor }, /* Dithered */
  { 8, PseudoColor }, /* Using .. */
  { 8, StaticColor },
  { 8, GrayScale },
  { 8, StaticGray }, /* .. TexelLUT */
  {16, TrueColor},
  {24, TrueColor},
  {16, DirectColor},
  {24, DirectColor}
};

#define NUM_ATTRIBUTES_G 6

static XF86AttributeRec AttributesG[NUM_ATTRIBUTES_G] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, 0, 10000, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 20000, "XV_CONTRAST"},
   {XvSettable | XvGettable, 0, 20000,"XV_SATURATION"},
   {XvSettable | XvGettable,-180,180,"XV_HUE"},
   {XvSettable | XvGettable,0,1,"XV_AUTOPAINT_COLORKEY"}
};

#define NUM_IMAGES_G 5

static XF86ImageRec ImagesG[NUM_IMAGES_G] =
{
    XVIMAGE_YUY2,
    XVIMAGE_YV12,
    {
	/*
	 * Below, a dummy picture type that is used in XvPutImage only to do
	 * an overlay update. Introduced for the XvMC client lib.
	 * Defined to have a zero data size.
	 */
	
	FOURCC_XVMC,
        XvYUV,
        LSBFirst,
        {'V','I','A',0x00,
          0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
        12,
        XvPlanar,
        1,
        0, 0, 0, 0 ,
        8, 8, 8,
        1, 2, 2,
        1, 2, 2,
        {'Y','V','U',
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
    },
    { /* RGB 555 */
      FOURCC_RV15,
      XvRGB,
      LSBFirst,
      {'R','V','1','5',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
      15, 0x7C00, 0x03E0, 0x001F,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    },
    { /* RGB 565 */
      FOURCC_RV16,
      XvRGB,
      LSBFirst,
      {'R','V','1','6',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
      16, 0xF800, 0x07E0, 0x001F,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    }
};

static char * XvAdaptorName[XV_ADAPT_NUM] =
{
   "XV_SWOV"
};

static XF86VideoAdaptorPtr viaAdaptPtr[XV_ADAPT_NUM];
static XF86VideoAdaptorPtr *allAdaptors;
static unsigned numAdaptPort[XV_ADAPT_NUM] = 
  {1};

/*
 *  F U N C T I O N
 */

/*
 *   Decide if the mode support video overlay. This depends on the bandwidth
 *   of the mode and the type of RAM available.
 */
static Bool DecideOverlaySupport(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    DisplayModePtr mode = pScrn->currentMode;

    /* Small trick here. We keep the height in 16's of lines and width in 32's 
       to avoid numeric overflow */

    if ( pVia->ChipId != PCI_CHIP_VT3205 && 
	 pVia->ChipId != PCI_CHIP_VT3204 &&
	 pVia->ChipId != PCI_CHIP_VT3259) {
	CARD32 bandwidth = (mode->HDisplay >> 4) * (mode->VDisplay >> 5) *
	    pScrn->bitsPerPixel * mode->VRefresh;
	
	switch (pVia->MemClk) {
	case VIA_MEM_SDR100: /* No overlay without DDR */
	case VIA_MEM_SDR133:
	    return FALSE;
	case VIA_MEM_DDR200:
	    /* Basic limit for DDR200 is about this */
	    if(bandwidth > 1800000)
		return FALSE;
	    /* But we have constraints at higher than 800x600 */
	    if (mode->HDisplay > 800) {
		if(pScrn->bitsPerPixel != 8)
		    return FALSE;
		if(mode->VDisplay > 768)
		    return FALSE;
		if(mode->VRefresh > 60)
		    return FALSE;
	    }
	    return TRUE;
	case 0: /*	FIXME: Why does my CLE266 report 0? */
	case VIA_MEM_DDR266:
	    if(bandwidth > 7901250)
		return FALSE;
	    return TRUE;
	}
	return FALSE;

    } else {
	VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
	unsigned width,height,refresh,dClock;
	float mClock,memEfficiency,needBandWidth,totalBandWidth;
	int bTV = 0;

	switch(pVia->MemClk) {
	case VIA_MEM_SDR100:
	    mClock = 50; /*HW base on 128 bit*/
	    break;
	case VIA_MEM_SDR133:
	    mClock = 66.5 ;
	    break;
	case VIA_MEM_DDR200:
	    mClock = 100;    
	    break;
	case VIA_MEM_DDR266:
	    mClock = 133;
	    break;
	case VIA_MEM_DDR333:
	    mClock = 166;
	    break;
	default:
	    /*Unknow DRAM Type*/
	    DBG_DD(ErrorF("Unknow DRAM Type!\n"));
	    mClock = 166;
	    break;
	}
        
	switch(pVia->MemClk) {
	case VIA_MEM_SDR100:
	case VIA_MEM_SDR133:
	case VIA_MEM_DDR200:
	    memEfficiency = (float)SINGLE_3205_100;
	    break;
	case VIA_MEM_DDR266:
	case VIA_MEM_DDR333:
	    memEfficiency = (float)SINGLE_3205_133;
	    break;
	default:
	    /*Unknow DRAM Type .*/
	    DBG_DD(ErrorF("Unknow DRAM Type!\n"));
	    memEfficiency = (float)SINGLE_3205_133;
	    break;
	}
      
	width = mode->HDisplay;
	height = mode->VDisplay;
	refresh = mode->VRefresh;

	/* 
	 * FIXME: If VBE modes assume a high refresh (100) for now 
	 */

	if (pVia->pVbe) {
	    refresh = 100;
	    if (pBIOSInfo->PanelActive) 
		refresh = 70;
	    if (pBIOSInfo->TVActive)
		refresh = 60;
	} else {
	    if (pBIOSInfo->PanelActive) {
		width = pBIOSInfo->panelX;
		height = pBIOSInfo->panelY;
		if ((width == 1400) && (height == 1050)) {
		    width = 1280;
		    height = 1024;
		    refresh = 60;
		}
	    } else if (pBIOSInfo->TVActive) {
		bTV = 1;
	    }
        }
	if (bTV) { 

	    /* 
	     * Approximative, VERY conservative formula in some cases.
	     * This formula and the one below are derived analyzing the
	     * tables present in VIA's own drivers. They may reject the over-
	     * lay in some cases where VIA's driver don't.
	     */

	    dClock = (width * height * 60) / 580000;   

	} else {

	    /*
	     * Approximative, slightly conservative formula. See above.
	     */
       
	    dClock = (width * height * refresh) / 680000;
	}

	if (dClock) {
	    needBandWidth = (float)(((pScrn->bitsPerPixel >> 3) + VIDEO_BPP)*dClock);
	    totalBandWidth = (float)(mClock*16.*memEfficiency);
      
	    DBG_DD(ErrorF(" via_video.c : cBitsPerPel= %d : \n",pScrn->bitsPerPixel));
	    DBG_DD(ErrorF(" via_video.c : Video_Bpp= %d : \n",VIDEO_BPP));
	    DBG_DD(ErrorF(" via_video.c : refresh = %d : \n",refresh));
	    DBG_DD(ErrorF(" via_video.c : dClock= %d : \n",dClock));
	    DBG_DD(ErrorF(" via_video.c : mClk= %f : \n",mClock));
	    DBG_DD(ErrorF(" via_video.c : memEfficiency= %f : \n",memEfficiency));
	    DBG_DD(ErrorF(" via_video.c : needBandwidth= %f : \n",needBandWidth));
	    DBG_DD(ErrorF(" via_video.c : totalBandwidth= %f : \n",totalBandWidth));
	    if (needBandWidth < totalBandWidth)
		return TRUE;
	}          
	return FALSE;
    }
    return FALSE;
}

static void
viaResetVideo(ScrnInfoPtr pScrn) 
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    

    DBG_DD(ErrorF(" via_video.c : viaResetVideo: \n"));

    viaVidEng->video1_ctl = 0;
    viaVidEng->video3_ctl = 0;
    viaVidEng->compose    = 0x80000000;
    viaVidEng->compose    = 0x40000000;
    viaVidEng->color_key = 0x821;
    viaVidEng->snd_color_key = 0x821;

}

void viaSaveVideo(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    

    pVia->dwV1 = ((vmmtr)viaVidEng)->video1_ctl;
    pVia->dwV3 = ((vmmtr)viaVidEng)->video3_ctl;
    viaVidEng->video1_ctl = 0;
    viaVidEng->video3_ctl = 0;
    viaVidEng->compose    = 0x80000000;
    viaVidEng->compose    = 0x40000000;
}

void viaRestoreVideo(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    

    viaVidEng->video1_ctl = pVia->dwV1 ;
    viaVidEng->video3_ctl = pVia->dwV3 ;
    viaVidEng->compose    = 0x80000000;
    viaVidEng->compose    = 0x40000000;

}



void viaExitVideo(ScrnInfoPtr pScrn)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    
    XF86VideoAdaptorPtr curAdapt;
    int i, j, numPorts;

    DBG_DD(ErrorF(" via_video.c : viaExitVideo : \n"));

#ifdef XF86DRI
    ViaCleanupXVMC(pScrn, viaAdaptPtr, XV_ADAPT_NUM);
#endif

    viaVidEng->video1_ctl = 0;
    viaVidEng->video3_ctl = 0;
    viaVidEng->compose    = 0x80000000;
    viaVidEng->compose    = 0x40000000;

    /*
     * Free all adaptor info allocated in viaInitVideo.
     */

    for (i=0; i<XV_ADAPT_NUM; ++i) {
	curAdapt = viaAdaptPtr[i];
	if (curAdapt) {
	    if (curAdapt->pPortPrivates) {
		if (curAdapt->pPortPrivates->ptr) {
		    numPorts = numAdaptPort[i];
		    for (j=0; j<numPorts; ++j) {
		      viaStopVideo(pScrn, (viaPortPrivPtr)curAdapt->pPortPrivates->ptr + j, TRUE);
		    }
		    xfree(curAdapt->pPortPrivates->ptr);
		}
		xfree(curAdapt->pPortPrivates);
	    }
	    xfree(curAdapt);
	}
    }
    if (allAdaptors) 
	xfree(allAdaptors);
}   

void viaInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr  pVia = VIAPTR(pScrn); 
    XF86VideoAdaptorPtr *adaptors, *newAdaptors;
    int num_adaptors, num_new;


    DBG_DD(ErrorF(" via_video.c : viaInitVideo : \n"));

    allAdaptors = NULL;
    newAdaptors = NULL;
    num_new = 0;

    if (!viaFastVidCpy)
	viaFastVidCpy = viaVidCopyInit("video", pScreen);

    if ( (pVia->Chipset == VIA_CLE266) || (pVia->Chipset == VIA_KM400) ||
	 (pVia->Chipset == VIA_K8M800) || (pVia->Chipset == VIA_PM800)) {
	num_new = viaSetupAdaptors(pScreen, &newAdaptors);
	num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
		   "[Xv] Unsupported Chipset. X video functionality disabled.\n");
	num_adaptors=0;
    }

    DBG_DD(ErrorF(" via_video.c : num_adaptors : %d\n",num_adaptors));
    if(newAdaptors) {
	allAdaptors =  xalloc((num_adaptors + num_new) * 
			      sizeof(XF86VideoAdaptorPtr*));
	if(allAdaptors) {
	    if (num_adaptors)
		memcpy(allAdaptors, adaptors, num_adaptors * sizeof(XF86VideoAdaptorPtr));
	    memcpy(allAdaptors + num_adaptors, newAdaptors, 
		   num_new * sizeof(XF86VideoAdaptorPtr));
	    num_adaptors += num_new;
        }
    }

    if (num_adaptors) {
	xf86XVScreenInit(pScreen, allAdaptors, num_adaptors);
#ifdef XF86DRI
	ViaInitXVMC(pScreen);
#endif
	viaSetColorSpace(pVia,0,0,0,0,TRUE);
	pVia->swov.panning_x = 0;
	pVia->swov.panning_y = 0;
	pVia->swov.oldPanningX = 0;
	pVia->swov.oldPanningY = 0;
    }
}

static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
    int *dataA, *dataB;
    int num;

    num = REGION_NUM_RECTS(A);
    if(num != REGION_NUM_RECTS(B))
        return FALSE;

    if((A->extents.x1 != B->extents.x1) ||
       (A->extents.x2 != B->extents.x2) ||
       (A->extents.y1 != B->extents.y1) ||
       (A->extents.y2 != B->extents.y2))
        return FALSE;

    dataA = (int*)REGION_RECTS(A);
    dataB = (int*)REGION_RECTS(B);

    while(num--) {
        if((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
           return FALSE;
        dataA += 2; 
        dataB += 2;
    }
 
    return TRUE;
}


/*
 * This one gets called, for example, on panning.
 */

static int
viaReputImage(ScrnInfoPtr pScrn,
	      short drw_x,
	      short drw_y,
	      RegionPtr clipBoxes,
	      pointer data)
{

    DDUPDATEOVERLAY      UpdateOverlay_Video;
    LPDDUPDATEOVERLAY    lpUpdateOverlay = &UpdateOverlay_Video;
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;
    VIAPtr  pVia = VIAPTR(pScrn);

    if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
	REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);    
	if (pPriv->autoPaint) 
	    xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes); 
    } 

    if (drw_x == pPriv->old_drw_x &&
	drw_y == pPriv->old_drw_y &&
	pVia->swov.oldPanningX == pVia->swov.panning_x &&
	pVia->swov.oldPanningY == pVia->swov.panning_y)
      
	return Success;
	
    lpUpdateOverlay->SrcLeft = pPriv->old_src_x;
    lpUpdateOverlay->SrcTop = pPriv->old_src_y;
    lpUpdateOverlay->SrcRight = pPriv->old_src_x + pPriv->old_src_w;
    lpUpdateOverlay->SrcBottom = pPriv->old_src_y + pPriv->old_src_h;

    lpUpdateOverlay->DstLeft = drw_x;
    lpUpdateOverlay->DstTop = drw_y;
    lpUpdateOverlay->DstRight = drw_x + pPriv->old_drw_w;
    lpUpdateOverlay->DstBottom = drw_y + pPriv->old_drw_h;
    pPriv->old_drw_x = drw_x;
    pPriv->old_drw_y = drw_y;

    lpUpdateOverlay->dwFlags = DDOVER_KEYDEST;

    if (pScrn->bitsPerPixel == 8)
	lpUpdateOverlay->dwColorSpaceLowValue = pPriv->colorKey & 0xff;
    else
	lpUpdateOverlay->dwColorSpaceLowValue = pPriv->colorKey;
    
    if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
	REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);    
	if (pPriv->autoPaint) 
	    xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes); 
    } 

    VIAVidUpdateOverlay(pScrn, lpUpdateOverlay);

    return Success;
}




static unsigned
viaSetupAdaptors(ScreenPtr pScreen, XF86VideoAdaptorPtr **adaptors)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    viaPortPrivRec *viaPortPriv;
    DevUnion  *pdevUnion;
    int  i,j, usedPorts, numPorts;
    
    DBG_DD(ErrorF(" via_video.c : viaSetupImageVideo: \n"));

    xvBrightness      = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast        = MAKE_ATOM("XV_CONTRAST");
    xvColorKey        = MAKE_ATOM("XV_COLORKEY");
    xvHue             = MAKE_ATOM("XV_HUE");
    xvSaturation      = MAKE_ATOM("XV_SATURATION");
    xvAutoPaint       = MAKE_ATOM("XV_AUTOPAINT_COLORKEY");

    *adaptors = NULL;
    usedPorts = 0;


    for ( i = 0; i < XV_ADAPT_NUM; i ++ ) {
        if(!(viaAdaptPtr[i] = xf86XVAllocateVideoAdaptorRec(pScrn)))
            return 0;
	numPorts = numAdaptPort[i];
	
        viaPortPriv = (viaPortPrivPtr)xnfcalloc(numPorts, sizeof(viaPortPrivRec) );
        pdevUnion = (DevUnion  *)xnfcalloc(numPorts, sizeof(DevUnion));

        if(i == XV_ADAPT_SWOV) /* Overlay engine */
        {
            viaAdaptPtr[i]->type = XvInputMask | XvWindowMask | XvImageMask |
		XvVideoMask | XvStillMask;
            viaAdaptPtr[i]->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
        }
        else
        {
            viaAdaptPtr[i]->type = XvInputMask | XvWindowMask | XvVideoMask;
            viaAdaptPtr[i]->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
        }
        viaAdaptPtr[i]->name = XvAdaptorName[i];
        viaAdaptPtr[i]->nEncodings = 1;
        viaAdaptPtr[i]->pEncodings = DummyEncoding;
        viaAdaptPtr[i]->nFormats = sizeof(FormatsG) / sizeof(FormatsG[0]);
        viaAdaptPtr[i]->pFormats = FormatsG;

        /* The adapter can handle 1 port simultaneously */
        viaAdaptPtr[i]->nPorts = numPorts; 
        viaAdaptPtr[i]->pPortPrivates = pdevUnion;
        viaAdaptPtr[i]->pPortPrivates->ptr = (pointer) viaPortPriv;
	viaAdaptPtr[i]->nAttributes = NUM_ATTRIBUTES_G;
	viaAdaptPtr[i]->pAttributes = AttributesG;

        viaAdaptPtr[i]->nImages = NUM_IMAGES_G;
        viaAdaptPtr[i]->pImages = ImagesG;
        viaAdaptPtr[i]->PutVideo = NULL;
        viaAdaptPtr[i]->StopVideo = viaStopVideo;
        viaAdaptPtr[i]->QueryBestSize = viaQueryBestSize;
        viaAdaptPtr[i]->GetPortAttribute = viaGetPortAttribute;
        viaAdaptPtr[i]->SetPortAttribute = viaSetPortAttribute;
        viaAdaptPtr[i]->PutImage = viaPutImage;
        viaAdaptPtr[i]->ReputImage = viaReputImage;
        viaAdaptPtr[i]->QueryImageAttributes = viaQueryImageAttributes;
 	for (j=0; j<numPorts; ++j) {
 	    viaPortPriv[j].colorKey = 0x0821;
 	    viaPortPriv[j].autoPaint = TRUE;
 	    viaPortPriv[j].brightness = 5000.;
 	    viaPortPriv[j].saturation = 10000;
 	    viaPortPriv[j].contrast = 10000;
 	    viaPortPriv[j].hue = 0;
	    viaPortPriv[j].FourCC = 0;
 	    viaPortPriv[j].xv_portnum = j + usedPorts;
 	    REGION_NULL(pScreen, &viaPortPriv[j].clip);
 	}
 	usedPorts += j;

 #ifdef XF86DRI
 	viaXvMCInitXv(pScrn, viaAdaptPtr[i]); 
 #endif


    } /* End of for */
    viaResetVideo(pScrn);
    *adaptors = viaAdaptPtr;
    return XV_ADAPT_NUM;
}


static void 
viaStopVideo(ScrnInfoPtr pScrn, pointer data, Bool exit)
{
    VIAPtr  pVia = VIAPTR(pScrn);
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;

    DBG_DD(ErrorF(" via_video.c : viaStopVideo: exit=%d\n", exit));

    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
    if (exit) {
	ViaOverlayHide(pScrn);
	ViaSwovSurfaceDestroy(pScrn, pPriv);
	pVia->dwFrameNum = 0;    
	pPriv->old_drw_x= 0;
	pPriv->old_drw_y= 0;
	pPriv->old_drw_w= 0;
	pPriv->old_drw_h= 0;
    } 
}

static int 
viaSetPortAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value,
    pointer data
    ){
    VIAPtr  pVia = VIAPTR(pScrn);
    vmmtr   viaVidEng = (vmmtr) pVia->VidMapBase;    
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;
    int attr, avalue;

    DBG_DD(ErrorF(" via_video.c : viaSetPortAttribute : \n"));


    /* Color Key */
    if(attribute == xvColorKey) {
	DBG_DD(ErrorF("  V4L Disable  xvColorKey = %08lx\n",value));

	pPriv->colorKey = value;
	/* All assume color depth is 16 */
	value &= 0x00FFFFFF;
	viaVidEng->color_key = value;
	viaVidEng->snd_color_key = value;
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
	DBG_DD(ErrorF("  V4L Disable done  xvColorKey = %08lx\n",value));

    } else if (attribute == xvAutoPaint) {
	pPriv->autoPaint = value;
	DBG_DD(ErrorF("       xvAutoPaint = %08lx\n",value));
	/* Color Control */
    } else if (attribute == xvBrightness ||
               attribute == xvContrast   ||
               attribute == xvSaturation ||
               attribute == xvHue) {
        if (attribute == xvBrightness)
	    {
		DBG_DD(ErrorF("     xvBrightness = %08ld\n",value));
		pPriv->brightness = value;
	    }
        if (attribute == xvContrast)
	    {
		DBG_DD(ErrorF("     xvContrast = %08ld\n",value));
		pPriv->contrast   = value;
	    }
        if (attribute == xvSaturation)
	    {
		DBG_DD(ErrorF("     xvSaturation = %08ld\n",value));
		pPriv->saturation     = value;
	    }
        if (attribute == xvHue)
	    {
		DBG_DD(ErrorF("     xvHue = %08ld\n",value));
		pPriv->hue        = value;
	    }
	viaSetColorSpace(pVia, pPriv->hue, pPriv->saturation,
			 pPriv->brightness, pPriv->contrast, FALSE); 
    }else{
	DBG_DD(ErrorF(" via_video.c : viaSetPortAttribute : is not supported the attribute"));
	return BadMatch;
    }
    
    /* attr,avalue hardware processing goes here */
    (void)attr;
    (void)avalue;
    
    return Success;
}

static int 
viaGetPortAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value,
    pointer data
){
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;

    DBG_DD(ErrorF(" via_video.c : viaGetPortAttribute : port %d %ld\n",
		  pPriv->xv_portnum, attribute));

    *value = 0;
    if (attribute == xvColorKey ) {
           *value =(INT32) pPriv->colorKey;
           DBG_DD(ErrorF(" via_video.c :    ColorKey 0x%lx\n",pPriv->colorKey));
    } else if (attribute == xvAutoPaint ) {
      *value = (INT32) pPriv->autoPaint;
      DBG_DD(ErrorF("    AutoPaint = %08ld\n", *value));
    /* Color Control */
    } else if (attribute == xvBrightness ||
               attribute == xvContrast   ||
               attribute == xvSaturation ||
               attribute == xvHue) {
        if (attribute == xvBrightness)
        {
	  *value = pPriv->brightness;
            DBG_DD(ErrorF("    xvBrightness = %08ld\n", *value));
        }
        if (attribute == xvContrast)
        {
	  *value = pPriv->contrast;
            DBG_DD(ErrorF("    xvContrast = %08ld\n", *value));
        }
        if (attribute == xvSaturation)
        {
	  *value = pPriv->saturation;
            DBG_DD(ErrorF("    xvSaturation = %08ld\n", *value));
        }
        if (attribute == xvHue)
        {
	  *value = pPriv->hue;
            DBG_DD(ErrorF("    xvHue = %08ld\n", *value));
        }

     }else {
           /*return BadMatch*/ ;
     }
    return Success;
}

static void 
viaQueryBestSize(
    ScrnInfoPtr pScrn,
    Bool motion,
    short vid_w, short vid_h,
    short drw_w, short drw_h,
    unsigned int *p_w, unsigned int *p_h,
    pointer data
){
    DBG_DD(ErrorF(" via_video.c : viaQueryBestSize :\n"));
    *p_w = drw_w;
    *p_h = drw_h;

    if(*p_w > 2048 )
           *p_w = 2048;
}

/*
 *  To do SW Flip
 */
static void Flip(VIAPtr pVia, viaPortPrivPtr pPriv, int fourcc, unsigned long DisplayBufferIndex)
{
    unsigned long proReg=0;
    if ((pVia->ChipId == PCI_CHIP_VT3259) &&
        !(pVia->swov.gdwVideoFlagSW & VIDEO_1_INUSE))
	proReg = PRO_HQV1_OFFSET;
    
    switch(fourcc)
    {
        case FOURCC_UYVY:
        case FOURCC_YUY2:
        case FOURCC_RV15:
        case FOURCC_RV16:
            while ((VIDInD(HQV_CONTROL) & HQV_SW_FLIP) );
            VIDOutD(HQV_SRC_STARTADDR_Y + proReg, 
		    pVia->swov.SWDevice.dwSWPhysicalAddr[DisplayBufferIndex]);
            VIDOutD(HQV_CONTROL,( VIDInD(HQV_CONTROL)&~HQV_FLIP_ODD) |HQV_SW_FLIP|HQV_FLIP_STATUS);
            break;

        case FOURCC_YV12:
        default:
            while ((VIDInD(HQV_CONTROL + proReg) & HQV_SW_FLIP) );
            VIDOutD(HQV_SRC_STARTADDR_Y + proReg, 
		    pVia->swov.SWDevice.dwSWPhysicalAddr[DisplayBufferIndex]);
	    if (pVia->ChipId == PCI_CHIP_VT3259) {
		VIDOutD(HQV_SRC_STARTADDR_U + proReg, 
			pVia->swov.SWDevice.dwSWCrPhysicalAddr[DisplayBufferIndex]);
	    } else {
		VIDOutD(HQV_SRC_STARTADDR_U, 
			pVia->swov.SWDevice.dwSWCbPhysicalAddr[DisplayBufferIndex]);
		VIDOutD(HQV_SRC_STARTADDR_V, 
			pVia->swov.SWDevice.dwSWCrPhysicalAddr[DisplayBufferIndex]);
	    }
            VIDOutD(HQV_CONTROL + proReg,
		    ( VIDInD(HQV_CONTROL + proReg)&~HQV_FLIP_ODD) |HQV_SW_FLIP|HQV_FLIP_STATUS);
            break;
    }
}

/*
 * Slow and dirty. NV12 blit.
 */

static void 
nv12cp(unsigned char *dst,
       const unsigned char *src,
       int dstPitch,
       int w,
       int h, int yuv422)
{
    int count;
    int x, dstAdd;
    const unsigned char* src2;

    /* Blit luma component as a fake YUY2 assembler blit. */

    (*viaFastVidCpy)(dst, src, dstPitch, w >> 1, h, TRUE);

    src += w*h;
    dst += dstPitch*h;
    dstAdd = dstPitch - w;

    /* UV component is 1/2 of Y */
    w >>= 1;

   /* copy V(Cr),U(Cb) components to video memory */
    count = h/2;

    src2 = src + w*count;
    while(count--) {
	x=w;
	while(x > 3) {
	    register CARD32 
		dst32,
		src32 = *((CARD32 *) src),
		src32_2 = *((CARD32 *) src2);
	    dst32 = 
		(src32_2 & 0xff) | ((src32 & 0xff) << 8) |
		((src32_2 & 0x0000ff00) << 8) | ((src32 & 0x0000ff00) << 16);
	    *((CARD32 *) dst) = dst32;
	    dst +=4;
	    dst32 = 
		((src32_2 & 0x00ff0000) >> 16) | ((src32 & 0x00ff0000) >> 8) |
		((src32_2 & 0xff000000) >> 8) | (src32 & 0xff000000);
	    *((CARD32 *) dst) = dst32;
	    dst +=4;
	    x -= 4;
	    src += 4;
	    src2 += 4;
	}
        while(x--) {
	    *dst++ = *src2++;
	    *dst++ = *src++;
        }   
	dst += dstAdd;
    }
}



static int
viaPutImage( 
    ScrnInfoPtr pScrn,
    short src_x, short src_y,
    short drw_x, short drw_y,
    short src_w, short src_h,
    short drw_w, short drw_h,
    int id, unsigned char* buf,
    short width, short height,
    Bool sync,
    RegionPtr clipBoxes, pointer data
    ){
    VIAPtr  pVia = VIAPTR(pScrn);
    viaPortPrivPtr pPriv = (viaPortPrivPtr)data;
    unsigned long retCode;

# ifdef XV_DEBUG
    ErrorF(" via_video.c : viaPutImage : called\n");
    ErrorF(" via_video.c : FourCC=0x%x width=%d height=%d sync=%d\n",id,width,height,sync);
    ErrorF(" via_video.c : src_x=%d src_y=%d src_w=%d src_h=%d colorkey=0x%lx\n",src_x, src_y, src_w, src_h, pPriv->colorKey);
    ErrorF(" via_video.c : drw_x=%d drw_y=%d drw_w=%d drw_h=%d\n",drw_x,drw_y,drw_w,drw_h);
# endif

    switch ( pPriv->xv_portnum )
	{
        case XV_ADAPT_SWOV:
	{
	    DDUPDATEOVERLAY      UpdateOverlay_Video;
	    LPDDUPDATEOVERLAY    lpUpdateOverlay = &UpdateOverlay_Video;

	    int dstPitch;
	    unsigned long dwUseExtendedFIFO=0;

	    DBG_DD(ErrorF(" via_video.c :              : S/W Overlay! \n"));
	    /*  Allocate video memory(CreateSurface),
	     *  add codes to judge if need to re-create surface
	     */
	    if ( (pPriv->old_src_w != src_w) || (pPriv->old_src_h != src_h) )
		ViaSwovSurfaceDestroy(pScrn, pPriv);

	    if (Success != ( retCode = ViaSwovSurfaceCreate(pScrn, pPriv, id, width, height) ))
                {
		    DBG_DD(ErrorF("             : Fail to Create SW Video Surface\n"));
		    return retCode;
                }

		    
	    /*  Copy image data from system memory to video memory
	     *  TODO: use DRM's DMA feature to accelerate data copy
	     */
	    if (FOURCC_XVMC != id) {
		dstPitch = pVia->swov.SWDevice.dwPitch;

		switch(id) {
		case FOURCC_YV12:
		    if (pVia->ChipId == PCI_CHIP_VT3259) {
			nv12cp(pVia->swov.SWDevice.lpSWOverlaySurface[pVia->dwFrameNum&1],
			       buf,dstPitch,width,height,0);
		    } else {
			(*viaFastVidCpy)(pVia->swov.SWDevice.lpSWOverlaySurface[pVia->dwFrameNum&1],
					 buf,dstPitch,width,height,0);
		    }
		    break;
		case FOURCC_UYVY:
		case FOURCC_YUY2:
		case FOURCC_RV15:
		case FOURCC_RV16:
		default:
		    (*viaFastVidCpy)(pVia->swov.SWDevice.lpSWOverlaySurface[pVia->dwFrameNum&1],
				     buf,dstPitch,width,height,1);
		    break;
		}
	    } 

	    /* If there is bandwidth issue, block the H/W overlay */

	    if (!pVia->OverlaySupported && 
		!(pVia->OverlaySupported = DecideOverlaySupport(pScrn))) {
		DBG_DD(ErrorF(" via_video.c : Xv Overlay rejected due to insufficient "
			      "memory bandwidth.\n"));
		return BadAlloc;
	    }

	    /* 
	     *  fill video overlay parameter
	     */
	    lpUpdateOverlay->SrcLeft = src_x;
	    lpUpdateOverlay->SrcTop = src_y;
	    lpUpdateOverlay->SrcRight = src_x + src_w;
	    lpUpdateOverlay->SrcBottom = src_y + src_h;

	    lpUpdateOverlay->DstLeft = drw_x;
	    lpUpdateOverlay->DstTop = drw_y;
	    lpUpdateOverlay->DstRight = drw_x + drw_w;
	    lpUpdateOverlay->DstBottom = drw_y + drw_h;

	    lpUpdateOverlay->dwFlags = DDOVER_KEYDEST;

	    if (pScrn->bitsPerPixel == 8)
		lpUpdateOverlay->dwColorSpaceLowValue = pPriv->colorKey & 0xff;
	    else
		lpUpdateOverlay->dwColorSpaceLowValue = pPriv->colorKey;

	    /* If use extend FIFO mode */
	    if (pScrn->currentMode->HDisplay > 1024)
                {
                    dwUseExtendedFIFO = 1;
                }

	    if (FOURCC_XVMC != id) {

		/*
		 * XvMC flipping is done in the client lib.
		 */

		DBG_DD(ErrorF("             : Flip\n"));
		Flip(pVia, pPriv, id, pVia->dwFrameNum&1);
	    }
		
	    pVia->dwFrameNum ++;
    
	    /* If the dest rec. & extendFIFO doesn't change, don't do UpdateOverlay 
	       unless the surface clipping has changed */
	    if ( (pPriv->old_drw_x == drw_x) && (pPriv->old_drw_y == drw_y)
		 && (pPriv->old_drw_w == drw_w) && (pPriv->old_drw_h == drw_h)
		 && (pPriv->old_src_x == src_x) && (pPriv->old_src_y == src_y)
		 && (pPriv->old_src_w == src_w) && (pPriv->old_src_h == src_h)
		 && (pVia->old_dwUseExtendedFIFO == dwUseExtendedFIFO)
		 && (pVia->VideoStatus & VIDEO_SWOV_ON) &&
		 RegionsEqual(&pPriv->clip, clipBoxes))
                {
                    return Success;
                }

	    pPriv->old_src_x = src_x;
	    pPriv->old_src_y = src_y;
	    pPriv->old_src_w = src_w;
	    pPriv->old_src_h = src_h;

	    pPriv->old_drw_x = drw_x;
	    pPriv->old_drw_y = drw_y;
	    pPriv->old_drw_w = drw_w;
	    pPriv->old_drw_h = drw_h;
	    pVia->old_dwUseExtendedFIFO = dwUseExtendedFIFO;
	    pVia->VideoStatus |= VIDEO_SWOV_ON;

	    /*  BitBlt: Draw the colorkey rectangle */
	    if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
		REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);    
		if (pPriv->autoPaint) 
		    xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes); 
	    }		

	    /*
	     *  Update video overlay
	     */
	    if (!VIAVidUpdateOverlay(pScrn, lpUpdateOverlay)) {
		DBG_DD(ErrorF(" via_video.c : call v4l updateoverlay fail. \n"));
	    } else {
		DBG_DD(ErrorF(" via_video.c : PutImage done OK\n"));
		return Success;
	    }
            break;
	}
        default:
            DBG_DD(ErrorF(" via_video.c : XVPort not supported\n"));
            break;
	}
    DBG_DD(ErrorF(" via_video.c : PutImage done OK\n"));
    return Success;
}


static int 
viaQueryImageAttributes(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short *w, unsigned short *h,
    int *pitches, int *offsets
){
    int size, tmp;

    DBG_DD(ErrorF(" via_video.c : viaQueryImageAttributes : FourCC=0x%x, ", id));

    if ( (!w) || (!h) )
       return 0;

    if(*w > VIA_MAX_XVIMAGE_X) *w = VIA_MAX_XVIMAGE_X;
    if(*h > VIA_MAX_XVIMAGE_Y) *h = VIA_MAX_XVIMAGE_Y;

    *w = (*w + 1) & ~1; 
    if(offsets) 
           offsets[0] = 0;

    switch(id) {
    case FOURCC_YV12:  /*Planar format : YV12 -4:2:0*/
      *h = (*h + 1) & ~1;
      size = *w;
      if(pitches) pitches[0] = size;
      size *= *h;
      if(offsets) offsets[1] = size;
      tmp = (*w >> 1);
      if(pitches) pitches[1] = pitches[2] = tmp;
      tmp *= (*h >> 1);
      size += tmp;
      if(offsets) offsets[2] = size;
      size += tmp;
      break;
    case FOURCC_XVMC:
        *h = (*h + 1) & ~1;
#ifdef XF86DRI
	size = viaXvMCPutImageSize(pScrn);
#else
	size = 0;
#endif
	if (pitches) pitches[0] = size;
	break;
    case FOURCC_AI44:
    case FOURCC_IA44:
      size = *w * *h;
      if (pitches) pitches[0] = *w;
      if (offsets) offsets[0] = 0;
      break;
    case FOURCC_UYVY:  /*Packed format : UYVY -4:2:2*/
    case FOURCC_YUY2:  /*Packed format : YUY2 -4:2:2*/
    case FOURCC_RV15:
    case FOURCC_RV16:
    default:
        size = *w << 1;
        if(pitches) 
             pitches[0] = size;
        size *= *h;
        break;
    }

    if ( pitches )
       DBG_DD(ErrorF(" pitches[0]=%d, pitches[1]=%d, pitches[2]=%d, ", pitches[0], pitches[1], pitches[2]));
    if ( offsets )
       DBG_DD(ErrorF(" offsets[0]=%d, offsets[1]=%d, offsets[2]=%d, ", offsets[0], offsets[1], offsets[2]));
    DBG_DD(ErrorF(" width=%d, height=%d \n", *w, *h));

    return size;
}


/*
 *
 */
void 
VIAVidAdjustFrame(ScrnInfoPtr pScrn, int x, int y)
{
    VIAPtr pVia = VIAPTR(pScrn);

    pVia->swov.panning_x = x;
    pVia->swov.panning_y = y;
}

#endif  /* !XvExtension */
