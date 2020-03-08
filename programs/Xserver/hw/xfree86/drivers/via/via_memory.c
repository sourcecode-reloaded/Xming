/*
 * Copyright 2003 Red Hat, Inc. All Rights Reserved.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86fbman.h"

#include "via.h"
#ifdef XF86DRI
#include "xf86drm.h"
#endif

#include "via_driver.h"
#include "via_priv.h"
#include "via_swov.h"
#ifdef XF86DRI
#include "via_drm.h"
#endif

/*
 *	Isolate the wonders of X memory allocation, DRI memory allocation
 *	and 4.3 or 4.4 differences in once abstraction
 *
 *	The pool code indicates who provided the memory
 *	0	-	nobody
 *	1	-	xf86 linear (Used in 4.4 only - current)
 *	2	-	DRM
 *	3	-	Preallocated buffer (Used in 4.3 only - removed)
 */
 
void VIAFreeLinear(VIAMemPtr mem)
{
    DEBUG(ErrorF("Freed %lu (pool %d)\n", mem->base, mem->pool));
    switch(mem->pool)
	{
	case 1:
	    xf86FreeOffscreenLinear(mem->linear);
	    mem->linear = NULL;
	    mem->pool = 0;
	    return;
	case 2:
#ifdef XF86DRI
	    if(drmCommandWrite(mem->drm_fd, DRM_VIA_FREEMEM,
			       &mem->drm, sizeof(drm_via_mem_t)) < 0)
		ErrorF("DRM module failed free.\n");
#endif
	    mem->pool = 0;
	    return;
	case 0:
	case 3: /* deprecated */
	default:
	    return;
	}
}


static unsigned long offScreenLinear(VIAMemPtr mem, ScrnInfoPtr pScrn,
				     unsigned long size) {

    int depth = (pScrn->bitsPerPixel + 7) >> 3;
    /* Make sure we don't truncate requested size */
    mem->linear = xf86AllocateOffscreenLinear(pScrn->pScreen, 
					      ( size + depth - 1 ) / depth,
					      32, NULL, NULL, NULL);
    if(mem->linear == NULL)
	return BadAlloc;
    mem->base = mem->linear->offset * depth;
    mem->pool = 1;
    return Success;

}


unsigned long VIAAllocLinear(VIAMemPtr mem, ScrnInfoPtr pScrn, unsigned long size)
{
    if(mem->pool)
	ErrorF("VIA Double Alloc.\n");

#ifdef XF86DRI
    {
	VIAPtr  pVia = VIAPTR(pScrn);
	int ret;		

	if(pVia->directRenderingEnabled) {
	    mem->drm_fd = pVia->drmFD;
	    mem->drm.context = 1;
	    mem->drm.size = size;
	    mem->drm.type = VIA_MEM_VIDEO;
	    ret = drmCommandWriteRead(mem->drm_fd, DRM_VIA_ALLOCMEM, &mem->drm, 
				      sizeof(drm_via_mem_t));
	    if (ret || (size != mem->drm.size)) {
		/*
		 * Try XY Fallback before failing.
		 */ 
		
		if (Success == offScreenLinear(mem, pScrn, size))
		    return Success;
		ErrorF("DRM memory allocation failed\n");
		return BadAlloc;
	    }
	    
	    mem->base = mem->drm.offset;
	    mem->pool = 2;
	    DEBUG(ErrorF("Fulfilled via DRI at %lu\n", mem->base));
	    return 0;
	}
    }
#endif /* XF86DRI */

    if (Success == offScreenLinear(mem, pScrn, size))
	return Success;
    ErrorF("Linear memory allocation failed\n");
    return BadAlloc;
}

void VIAInitLinear(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    /*
     * In the 44 path we must take care not to truncate offset and size so
     * that we get overlaps. If there is available memory below line 2048
     * we use it.
     */ 
    unsigned long offset = (pVia->FBFreeStart + pVia->Bpp - 1 ) / pVia->Bpp; 
    unsigned long size = pVia->FBFreeEnd / pVia->Bpp - offset;
    if (size > 0) xf86InitFBManagerLinear(pScreen, offset, size);
}
    
