/*
 * Copyright 2005 Eric Anholt
 * Copyright 2005 Benjamin Herrenschmidt
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <anholt@FreeBSD.org>
 *    Zack Rusin <zrusin@trolltech.com>
 *    Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *
 */

#if defined(ACCEL_MMIO) && defined(ACCEL_CP)
#error Cannot define both MMIO and CP acceleration!
#endif

#if !defined(UNIXCPP) || defined(ANSICPP)
#define FUNC_NAME_CAT(prefix,suffix) prefix##suffix
#else
#define FUNC_NAME_CAT(prefix,suffix) prefix/**/suffix
#endif

#ifdef ACCEL_MMIO
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,MMIO)
#else
#ifdef ACCEL_CP
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,CP)
#else
#error No accel type defined!
#endif
#endif

#include "radeon.h"
#include "atidri.h"

#include "exa.h"

#include "fbdevhw.h"

static void
FUNC_NAME(RADEONSync)(ScreenPtr pScreen, int marker)
{
    TRACE;

    FUNC_NAME(RADEONWaitForIdle)(xf86Screens[pScreen->myNum]);
}

static Bool
FUNC_NAME(RADEONPrepareSolid)(PixmapPtr pPix, int alu, Pixel pm, Pixel fg)
{
    RINFO_FROM_SCREEN(pPix->drawable.pScreen);
    CARD32 datatype, dst_pitch_offset;
    ACCEL_PREAMBLE();

    TRACE;

    if (pPix->drawable.bitsPerPixel == 24)
	RADEON_FALLBACK(("24bpp unsupported\n"));
    if (!RADEONGetDatatypeBpp(pPix->drawable.bitsPerPixel, &datatype))
	RADEON_FALLBACK(("RADEONGetDatatypeBpp failed\n"));
    if (!RADEONGetPixmapOffsetPitch(pPix, &dst_pitch_offset))
	RADEON_FALLBACK(("RADEONGetPixmapOffsetPitch failed\n"));

    RADEON_SWITCH_TO_2D();

    BEGIN_ACCEL(5);
    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL,
	    RADEON_GMC_DST_PITCH_OFFSET_CNTL |
	    RADEON_GMC_BRUSH_SOLID_COLOR |
	    (datatype << 8) |
	    RADEON_GMC_SRC_DATATYPE_COLOR |
	    RADEON_ROP[alu].pattern |
	    RADEON_GMC_CLR_CMP_CNTL_DIS);
    OUT_ACCEL_REG(RADEON_DP_BRUSH_FRGD_CLR, fg);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK, pm);
    OUT_ACCEL_REG(RADEON_DP_CNTL,
	(RADEON_DST_X_LEFT_TO_RIGHT | RADEON_DST_Y_TOP_TO_BOTTOM));
    OUT_ACCEL_REG(RADEON_DST_PITCH_OFFSET, dst_pitch_offset);
    FINISH_ACCEL();

    return TRUE;
}


static void
FUNC_NAME(RADEONSolid)(PixmapPtr pPix, int x1, int y1, int x2, int y2)
{

    RINFO_FROM_SCREEN(pPix->drawable.pScreen);
    ACCEL_PREAMBLE();

    TRACE;

    BEGIN_ACCEL(2);
    OUT_ACCEL_REG(RADEON_DST_Y_X, (y1 << 16) | x1);
    OUT_ACCEL_REG(RADEON_DST_HEIGHT_WIDTH, ((y2 - y1) << 16) | (x2 - x1));
    FINISH_ACCEL();
}

static void
FUNC_NAME(RADEONDoneSolid)(PixmapPtr pPix)
{
    TRACE;
}

static Bool
FUNC_NAME(RADEONPrepareCopy)(PixmapPtr pSrc,   PixmapPtr pDst,
			     int xdir, int ydir,
			     int rop,
			     Pixel planemask)
{
    RINFO_FROM_SCREEN(pDst->drawable.pScreen);
    CARD32 datatype, src_pitch_offset, dst_pitch_offset;
    ACCEL_PREAMBLE();

    TRACE;

    info->xdir = xdir;
    info->ydir = ydir;

    if (pDst->drawable.bitsPerPixel == 24)
	RADEON_FALLBACK(("24bpp unsupported"));
    if (!RADEONGetDatatypeBpp(pDst->drawable.bitsPerPixel, &datatype))
	RADEON_FALLBACK(("RADEONGetDatatypeBpp failed\n"));
    if (!RADEONGetPixmapOffsetPitch(pSrc, &src_pitch_offset))
	RADEON_FALLBACK(("RADEONGetPixmapOffsetPitch source failed\n"));
    if (!RADEONGetPixmapOffsetPitch(pDst, &dst_pitch_offset))
	RADEON_FALLBACK(("RADEONGetPixmapOffsetPitch dest failed\n"));

    RADEON_SWITCH_TO_2D();

    BEGIN_ACCEL(5);
    OUT_ACCEL_REG(RADEON_DP_GUI_MASTER_CNTL,
	RADEON_GMC_DST_PITCH_OFFSET_CNTL |
	RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
	RADEON_GMC_BRUSH_NONE |
	(datatype << 8) |
	RADEON_GMC_SRC_DATATYPE_COLOR |
	RADEON_ROP[rop].rop |
	RADEON_DP_SRC_SOURCE_MEMORY |
	RADEON_GMC_CLR_CMP_CNTL_DIS);
    OUT_ACCEL_REG(RADEON_DP_WRITE_MASK, planemask);
    OUT_ACCEL_REG(RADEON_DP_CNTL,
	((xdir >= 0 ? RADEON_DST_X_LEFT_TO_RIGHT : 0) |
	 (ydir >= 0 ? RADEON_DST_Y_TOP_TO_BOTTOM : 0)));
    OUT_ACCEL_REG(RADEON_DST_PITCH_OFFSET, dst_pitch_offset);
    OUT_ACCEL_REG(RADEON_SRC_PITCH_OFFSET, src_pitch_offset);
    FINISH_ACCEL();

    return TRUE;
}

static void
FUNC_NAME(RADEONCopy)(PixmapPtr pDst,
		      int srcX, int srcY,
		      int dstX, int dstY,
		      int w, int h)
{

    RINFO_FROM_SCREEN(pDst->drawable.pScreen);
    ACCEL_PREAMBLE();

    TRACE;

    if (info->xdir < 0) {
	srcX += w - 1;
	dstX += w - 1;
    }
    if (info->ydir < 0) {
	srcY += h - 1;
	dstY += h - 1;
    }

    BEGIN_ACCEL(3);

    OUT_ACCEL_REG(RADEON_SRC_Y_X,	   (srcY << 16) | srcX);
    OUT_ACCEL_REG(RADEON_DST_Y_X,	   (dstY << 16) | dstX);
    OUT_ACCEL_REG(RADEON_DST_HEIGHT_WIDTH, (h  << 16) | w);

    FINISH_ACCEL();
}

static void
FUNC_NAME(RADEONDoneCopy)(PixmapPtr pDst)
{
    TRACE;
}

static Bool
FUNC_NAME(RADEONUploadToScreen)(PixmapPtr pDst, int x, int y, int w, int h,
				char *src, int src_pitch)
{
#if X_BYTE_ORDER == X_BIG_ENDIAN || defined(ACCEL_CP)
    RINFO_FROM_SCREEN(pDst->drawable.pScreen);
#endif
    CARD8	   *dst	     = pDst->devPrivate.ptr;
    unsigned int   dst_pitch = exaGetPixmapPitch(pDst);
    unsigned int   bpp	     = pDst->drawable.bitsPerPixel;
#ifdef ACCEL_CP
    unsigned int   hpass;
    CARD32	   buf_pitch;
#endif
#if X_BYTE_ORDER == X_BIG_ENDIAN 
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned int swapper = info->ModeReg.surface_cntl &
	    ~(RADEON_NONSURF_AP0_SWP_32BPP | RADEON_NONSURF_AP1_SWP_32BPP |
	      RADEON_NONSURF_AP0_SWP_16BPP | RADEON_NONSURF_AP1_SWP_16BPP);
#endif

    TRACE;

    if (bpp < 8)
	return FALSE;

#ifdef ACCEL_CP
    if (info->directRenderingEnabled) {
	CARD8 *buf;
	int cpp = bpp / 8;
	ACCEL_PREAMBLE();

	dst += (x * cpp) + (y * dst_pitch);
	RADEON_SWITCH_TO_2D();
	while ((buf = RADEONHostDataBlit(pScrn,
					cpp, w, dst_pitch, &buf_pitch,
					&dst, &h, &hpass)) != 0) {
	    RADEONHostDataBlitCopyPass(pScrn, cpp, buf, (unsigned char *)src,
				       hpass, buf_pitch, src_pitch);
	    src += hpass * src_pitch;
	}
	
	exaMarkSync(pDst->drawable.pScreen);
	return TRUE;
  }
#endif

    /* Do we need that sync here ? probably not .... */
    exaWaitSync(pDst->drawable.pScreen);

#if X_BYTE_ORDER == X_BIG_ENDIAN
    switch(bpp) {
    case 15:
    case 16:
	swapper |= RADEON_NONSURF_AP0_SWP_16BPP
		|  RADEON_NONSURF_AP1_SWP_16BPP;
	break;
    case 24:
    case 32:
	swapper |= RADEON_NONSURF_AP0_SWP_32BPP
		|  RADEON_NONSURF_AP1_SWP_32BPP;
	break;
    }
    OUTREG(RADEON_SURFACE_CNTL, swapper);
#endif
    w *= bpp / 8;
    dst += (x * bpp / 8) + (y * dst_pitch);

    while (h--) {
	memcpy(dst, src, w);
	src += src_pitch;
	dst += dst_pitch;
    }

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* restore byte swapping */
    OUTREG(RADEON_SURFACE_CNTL, info->ModeReg.surface_cntl);
#endif

    return TRUE;
}

static Bool
FUNC_NAME(RADEONDownloadFromScreen)(PixmapPtr pSrc, int x, int y, int w, int h,
				    char *dst, int dst_pitch)
{
#if X_BYTE_ORDER == X_BIG_ENDIAN
    RINFO_FROM_SCREEN(pSrc->drawable.pScreen);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned int swapper = info->ModeReg.surface_cntl &
	    ~(RADEON_NONSURF_AP0_SWP_32BPP | RADEON_NONSURF_AP1_SWP_32BPP |
	      RADEON_NONSURF_AP0_SWP_16BPP | RADEON_NONSURF_AP1_SWP_16BPP);
#endif
    unsigned char *src	     = pSrc->devPrivate.ptr;
    int		   src_pitch = exaGetPixmapPitch(pSrc);
    int		   bpp	     = pSrc->drawable.bitsPerPixel;

    TRACE;

    /*
     * This is currently done without DMA until I have ironed out the
     * various endian issues with R300 among others
     */
    exaWaitSync(pSrc->drawable.pScreen);

#if X_BYTE_ORDER == X_BIG_ENDIAN
    switch(bpp) {
    case 15:
    case 16:
	swapper |= RADEON_NONSURF_AP0_SWP_16BPP
		|  RADEON_NONSURF_AP1_SWP_16BPP;
	break;
    case 24:
    case 32:
	swapper |= RADEON_NONSURF_AP0_SWP_32BPP
		|  RADEON_NONSURF_AP1_SWP_32BPP;
	break;
    }
    OUTREG(RADEON_SURFACE_CNTL, swapper);
#endif

    src += (x * bpp / 8) + (y * src_pitch);
    w *= bpp / 8;

    while (h--) {
	memcpy(dst, src, w);
	src += src_pitch;
	dst += dst_pitch;
    }

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* restore byte swapping */
    OUTREG(RADEON_SURFACE_CNTL, info->ModeReg.surface_cntl);
#endif

    return TRUE;
}

Bool FUNC_NAME(RADEONDrawInit)(ScreenPtr pScreen)
{
    RINFO_FROM_SCREEN(pScreen);

    memset(&info->exa.accel, 0, sizeof(ExaAccelInfoRec));

    info->exa.accel.PrepareSolid = FUNC_NAME(RADEONPrepareSolid);
    info->exa.accel.Solid = FUNC_NAME(RADEONSolid);
    info->exa.accel.DoneSolid = FUNC_NAME(RADEONDoneSolid);

    info->exa.accel.PrepareCopy = FUNC_NAME(RADEONPrepareCopy);
    info->exa.accel.Copy = FUNC_NAME(RADEONCopy);
    info->exa.accel.DoneCopy = FUNC_NAME(RADEONDoneCopy);

    info->exa.accel.WaitMarker = FUNC_NAME(RADEONSync);
    info->exa.accel.UploadToScreen = FUNC_NAME(RADEONUploadToScreen);
    info->exa.accel.DownloadFromScreen = FUNC_NAME(RADEONDownloadFromScreen);

#if X_BYTE_ORDER == X_BIG_ENDIAN
    info->exa.accel.PrepareAccess = RADEONPrepareAccess;
    info->exa.accel.FinishAccess = RADEONFinishAccess;
#endif /* X_BYTE_ORDER == X_BIG_ENDIAN */

    info->exa.card.flags = EXA_OFFSCREEN_PIXMAPS;
    info->exa.card.pixmapOffsetAlign = RADEON_BUFFER_ALIGN + 1;
    info->exa.card.pixmapPitchAlign = 64;

    info->exa.card.maxX = 2047;
    info->exa.card.maxY = 2047;

#ifdef RENDER
    if (info->RenderAccel) {
	if (info->ChipFamily >= CHIP_FAMILY_R300) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Render acceleration "
			       "unsupported on R300 type cards and newer.\n");
	} else if ((info->ChipFamily == CHIP_FAMILY_RV250) || 
		   (info->ChipFamily == CHIP_FAMILY_RV280) || 
		   (info->ChipFamily == CHIP_FAMILY_RS300) || 
		   (info->ChipFamily == CHIP_FAMILY_R200)) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Render acceleration "
			       "enabled for R200 type cards.\n");
		info->exa.accel.CheckComposite = R200CheckComposite;
		info->exa.accel.PrepareComposite =
		    FUNC_NAME(R200PrepareComposite);
		info->exa.accel.Composite = FUNC_NAME(RadeonComposite);
		info->exa.accel.DoneComposite = RadeonDoneComposite;
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Render acceleration "
			       "enabled for R100 type cards.\n");
		info->exa.accel.CheckComposite = R100CheckComposite;
		info->exa.accel.PrepareComposite =
		    FUNC_NAME(R100PrepareComposite);
		info->exa.accel.Composite = FUNC_NAME(RadeonComposite);
		info->exa.accel.DoneComposite = RadeonDoneComposite;
	}
    }
#endif

    RADEONEngineInit(pScrn);

    if (!exaDriverInit(pScreen, &info->exa)) {
	return FALSE;
    }
    exaMarkSync(pScreen);

    return TRUE;
}

#undef FUNC_NAME
