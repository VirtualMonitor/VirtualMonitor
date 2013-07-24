/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of SDLFramebuffer class
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"
#else
# include <VBox/com/com.h>
# include <VBox/com/string.h>
# include <VBox/com/Guid.h>
#endif

#define LOG_GROUP LOG_GROUP_GUI
#include <VBox/err.h>
#include <VBox/log.h>

#include <signal.h>

#include "SDLFramebuffer.h"

//
// Constructor / destructor
//

/**
 * SDL framebuffer constructor. It is called from the main
 * (i.e. SDL) thread. Therefore it is safe to use SDL calls
 * here.
 */
SDLFramebuffer::SDLFramebuffer()
{
    int rc;
    LogFlow(("SDLFramebuffer::SDLFramebuffer\n"));

#if defined (RT_OS_WINDOWS)
    refcnt = 0;
#endif

    mScreen = NULL;
    mfFullscreen = false;
    mTopOffset = 0;

    /* memorize the thread that inited us, that's the SDL thread */
    mSdlNativeThread = RTThreadNativeSelf();

    rc = RTCritSectInit(&mUpdateLock);
    AssertMsg(rc == VINF_SUCCESS, ("Error from RTCritSectInit!\n"));

#ifdef VBOX_SECURELABEL
    mLabelFont = NULL;
    mLabelHeight = 0;
#endif

#ifdef RT_OS_LINUX
    /* NOTE: we still want Ctrl-C to work, so we undo the SDL redirections */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
#endif

    /*
     * Start with standard screen dimensions.
     */
#if 0
    mWidth  = 640;
    mHeight = 480;
#endif

    mWidth  = 800;
    mHeight = 600;
    resize();
    Assert(mScreen);
}

SDLFramebuffer::~SDLFramebuffer()
{
    LogFlow(("SDLFramebuffer::~SDLFramebuffer\n"));
    RTCritSectDelete(&mUpdateLock);

    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
#ifdef VBOX_SECURELABEL
    if (mLabelFont)
        TTF_CloseFont(mLabelFont);
    TTF_Quit();
#endif
    mScreen = NULL;
}


/**
 * Returns the current framebuffer width in pixels.
 *
 * @returns COM status code
 * @param   width Address of result buffer.
 */
HRESULT SDLFramebuffer::getWidth(ULONG *width)
{
    LogFlow(("SDLFramebuffer::GetWidth\n"));
    if (!width)
        return E_INVALIDARG;
    *width = mWidth;
    return S_OK;
}

/**
 * Returns the current framebuffer height in pixels.
 *
 * @returns COM status code
 * @param   height Address of result buffer.
 */
HRESULT SDLFramebuffer::getHeight(ULONG *height)
{
    LogFlow(("SDLFramebuffer::GetHeight\n"));
    if (!height)
        return E_INVALIDARG;
    *height = mHeight;
    return S_OK;
}

/**
 * Lock the framebuffer (make its address immutable).
 *
 * @returns COM status code
 */
HRESULT SDLFramebuffer::Lock()
{
    LogFlow(("SDLFramebuffer::Lock\n"));
    RTCritSectEnter(&mUpdateLock);
    return S_OK;
}

/**
 * Unlock the framebuffer.
 *
 * @returns COM status code
 */
HRESULT SDLFramebuffer::Unlock()
{
    LogFlow(("SDLFramebuffer::Unlock\n"));
    RTCritSectLeave(&mUpdateLock);
    return S_OK;
}

/**
 * Return the framebuffer start address.
 *
 * @returns COM status code.
 * @param   address Pointer to result variable.
 */
HRESULT SDLFramebuffer::getAddress(uintptr_t *address)
{
    LogFlow(("SDLFramebuffer::GetAddress\n"));
    if (!address)
        return E_INVALIDARG;

    /* subtract the reserved extra area */
    *address = mScreen
             ? (uintptr_t)mScreen->pixels
#ifdef RT_OS_OS2 /* Play safe for now - this is vital when we get a larger surface than requested. */
               + mScreen->offset
#endif
               + (mScreen->pitch * mTopOffset)
             : 0;

    LogFlow(("VBoxSDL::GetAddress returning %p\n", *address));
    return S_OK;
}

/**
 * Return the current framebuffer color depth.
 *
 * @returns COM status code
 * @param   bitsPerPixel Address of result variable
 */
HRESULT SDLFramebuffer::getBitsPerPixel(ULONG *bitsPerPixel)
{
    LogFlow(("SDLFramebuffer::GetBitsPerPixel\n"));

    if (!bitsPerPixel)
        return E_INVALIDARG;
    *bitsPerPixel = (ULONG)(mScreen ? mScreen->format->BitsPerPixel : 0);
    return S_OK;
}

/**
 * Return the current framebuffer line size in bytes.
 *
 * @returns COM status code.
 * @param   lineSize Address of result variable.
 */
HRESULT SDLFramebuffer::getLineSize(ULONG *lineSize)
{
    LogFlow(("SDLFramebuffer::GetLineSize\n"));
    if (!lineSize)
        return E_INVALIDARG;
    *lineSize = (ULONG)(mScreen ? mScreen->pitch : 0);

    return S_OK;
}

/**
 * Notify framebuffer of an update.
 *
 * @returns COM status code
 * @param   x        Update region upper left corner x value.
 * @param   y        Update region upper left corner y value.
 * @param   w        Update region width in pixels.
 * @param   h        Update region height in pixels.
 * @param   finished Address of output flag whether the update
 *                   could be fully processed in this call (which
 *                   has to return immediately) or VBox should wait
 *                   for a call to the update complete API before
 *                   continuing with display updates.
 */
HRESULT SDLFramebuffer::NotifyUpdate(ULONG x, ULONG y,
                                     ULONG w, ULONG h)
{
    LogFlow(("SDLFramebuffer::NotifyUpdate: x = %d, y = %d, w = %d, h = %d\n",
             x, y, w, h));

#ifdef VBOXBFE_WITH_X11
    /*
     * SDL does not allow us to make this call from any other
     * thread. So we have to send an event to the main SDL
     * thread and process it there. For sake of simplicity, we encode
     * all information in the event parameters.
     */
    SDL_Event event;
    event.type = SDL_USEREVENT;
    event.user.type  = SDL_USER_EVENT_UPDATERECT;
    // 16 bit is enough for coordinates
    event.user.data1 = (void*)(x << 16 | (y + mTopOffset));
    event.user.data2 = (void*)(w << 16 | h);

    int rc = SDL_PushEvent(&event);
    //    printf("%s:%d event=%p\n",__FILE__,__LINE__,&event);
    NOREF(rc);
    AssertMsg(!rc, ("Error: SDL_PushEvent was not successful! SDL error: '%s'\n",
              SDL_GetError()));
    /* in order to not flood the SDL event queue, yield the CPU */
    RTThreadYield();
#else /* !VBOXBFE_WITH_X11 */
    update(x, y + mTopOffset, w, h);
#endif /* !VBOXBFE_WITH_X11 */

    return S_OK;
}

/**
 * Request a display resize from the framebuffer.
 *
 * @returns COM status code.
 * @param   w        New display width in pixels.
 * @param   h        New display height in pixels.
 * @param   finished Address of output flag whether the update
 *                   could be fully processed in this call (which
 *                   has to return immediately) or VBox should wait
 *                   for all call to the resize complete API before
 *                   continuing with display updates.
 */
HRESULT SDLFramebuffer::RequestResize(ULONG w, ULONG h, BOOL *finished)
{
    LogFlow(("SDLFramebuffer::RequestResize: w = %d, h = %d\n", w, h));

    /*
     * SDL does not allow us to make this call from any other
     * thread. So we have to send an event to the main SDL
     * thread and tell VBox to wait.
     */
    if (!finished)
    {
        AssertMsgFailed(("RequestResize requires the finished flag!\n"));
        return E_FAIL;
    }
    mWidth = w;
    mHeight = h;

    SDL_Event event;
    event.type = SDL_USEREVENT;
    event.user.type  = SDL_USER_EVENT_RESIZE;

    int rc = SDL_PushEvent(&event);
    NOREF(rc);
    AssertMsg(!rc, ("Error: SDL_PushEvent was not successful!\n"));

    /* we want this request to be processed quickly, so yield the CPU */
    RTThreadYield();

    *finished = false;

    return S_OK;
}

HRESULT SDLFramebuffer::GetVisibleRegion(BYTE *aRectangles, ULONG aCount,
                                         ULONG *aCountCopied)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /// @todo

    NOREF(aCount);
    NOREF(aCountCopied);

    return S_OK;
}

HRESULT SDLFramebuffer::SetVisibleRegion(BYTE *aRectangles, ULONG aCount)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /// @todo

    NOREF(aCount);

    return S_OK;
}

HRESULT SDLFramebuffer::ProcessVHWACommand(BYTE *pCommand)
{
    return E_NOTIMPL;
}

HRESULT SDLFramebuffer::Release()
{
    return E_NOTIMPL;
}

//
// Internal public methods
//

/**
 * Method that does the actual resize.
 *
 * @remarks Must be called from the SDL thread!
 */
void SDLFramebuffer::resize()
{
    LogFlow(("VBoxSDL::resize() mWidth: %d, mHeight: %d\n", mWidth, mHeight));
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));

    uint32_t newHeight = mHeight;
    int sdlFlags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
    if (mfFullscreen)
    {
        sdlFlags |= SDL_FULLSCREEN;
#ifdef RT_OS_WINDOWS
        /* this flag causes a crash on Windows, mScreen->pixels is NULL */
        sdlFlags &= ~SDL_HWSURFACE;
        sdlFlags |= SDL_SWSURFACE;
#endif
    }

#ifdef VBOX_SECURELABEL
    /* try to add the label size */
    newHeight = mHeight + mLabelHeight;
#endif

    mScreen = SDL_SetVideoMode(mWidth, newHeight, 0, sdlFlags);
#ifdef VBOX_SECURELABEL
    /* if it didn't work, then we have to go for the original resolution and paint over the guest */
    if (!mScreen)
    {
        mScreen = SDL_SetVideoMode(mWidth, mHeight, 0, sdlFlags);
        /* we don't have any extra space */
        mTopOffset = 0;
    }
    else
    {
        /* we now have some extra space */
        mTopOffset = mLabelHeight;
    }
#endif

    AssertMsg(mScreen, ("Error: SDL_SetVideoMode failed!\n"));
    if (mScreen)
        AssertMsg(mScreen->pixels, ("Error: SDL_SetVideoMode returned NULL framebuffer!\n"));
    repaint();
}

/**
 * Update specified framebuffer area.
 *
 * @remarks Must be called from the SDL thread on Linux! Update region
 *          on the whole framebuffer, including y offset!
 * @param   x left column
 * @param   y top row
 * @param   w width in pixels
 * @param   h height in pixels
 */
void SDLFramebuffer::update(int x, int y, int w, int h)
{
#ifdef VBOXBFE_WITH_X11
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
#endif

    Assert(mScreen);

    uint32_t safeY = y;
    uint32_t safeH = h;

#ifdef VBOX_SECURELABEL
    /*
     * Cut down the update area to the untrusted portion
     */
    if (safeY < mLabelHeight)
        safeY = mLabelHeight;
    if ((safeH + mLabelHeight) > (mHeight + mTopOffset))
        safeH = mHeight + mTopOffset - mLabelHeight;
#endif

    SDL_UpdateRect(mScreen, x, safeY, w, safeH);

#ifdef VBOX_SECURELABEL
    paintSecureLabel(x, y, w, h, false);
#endif
}

/**
 * Repaint the whole framebuffer
 *
 * @remarks Must be called from the SDL thread!
 */
void SDLFramebuffer::repaint()
{
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
    LogFlow(("SDLFramebuffer::repaint\n"));
    update(0, 0, mWidth, mHeight);
}

bool SDLFramebuffer::getFullscreen()
{
    LogFlow(("SDLFramebuffer::getFullscreen\n"));
    return mfFullscreen;
}

/**
 * Toggle fullscreen mode
 *
 * @remarks Must be called from the SDL thread!
 */
void SDLFramebuffer::setFullscreen(bool fFullscreen)
{
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
    LogFlow(("SDLFramebuffer::SetFullscreen: fullscreen: %d\n", fFullscreen));
    mfFullscreen = fFullscreen;
    resize();
}

/**
 * Returns the current y offset of the start of the guest screen
 *
 * @returns current y offset in pixels
 */
int SDLFramebuffer::getYOffset()
{
    return mTopOffset;
}

/**
 * Returns the number of horizontal pixels of the host console
 *
 * @return  X resolution
 * @remarks currently not used in SDL mode
 */
int SDLFramebuffer::getHostXres()
{
    return 0;
}

/**
 * Returns the number of vertical pixels of the host console
 *
 * @return  Y resolution
 * @remarks currently not used in SDL mode
 */
int SDLFramebuffer::getHostYres()
{
    return 0;
}

/**
 * Returns the number of bits per pixels of the host console
 *
 * @return  bits per pixel
 * @remarks currently not used in SDL mode
 */
int SDLFramebuffer::getHostBitsPerPixel()
{
    return 0;
}


#ifdef VBOX_SECURELABEL
/**
 * Setup the secure labeling parameters
 *
 * @returns         VBox status code
 * @param height    height of the secure label area in pixels
 * @param font      file path fo the TrueType font file
 * @param pointsize font size in points
 */
int SDLFramebuffer::initSecureLabel(uint32_t height, char *font, uint32_t pointsize)
{
    LogFlow(("SDLFramebuffer:initSecureLabel: new offset: %d pixels, new font: %s, new pointsize: %d\n",
              height, font, pointsize));
    mLabelHeight = height;
    Assert(font);
    TTF_Init();
    mLabelFont = TTF_OpenFont(font, pointsize);
    if (!mLabelFont)
    {
        AssertMsgFailed(("Failed to open TTF font file %s\n", font));
        return VERR_OPEN_FAILED;
    }
    repaint();
    return VINF_SUCCESS;
}

/**
 * Set the secure label text and repaint the label
 *
 * @param   text UTF-8 string of new label
 * @remarks must be called from the SDL thread!
 */
void SDLFramebuffer::setSecureLabelText(const char *text)
{
    mSecureLabelText = text;
    paintSecureLabel(0, 0, 0, 0, true);
}

/**
 * Paint the secure label if required
 *
 * @param   fForce Force the repaint
 * @remarks must be called from the SDL thread!
 */
void SDLFramebuffer::paintSecureLabel(int x, int y, int w, int h, bool fForce)
{
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
    /* check if we can skip the paint */
    if (!fForce && ((uint32_t)y > mLabelHeight))
    {
        return;
    }
    /* first fill the background */
    SDL_Rect rect = {0, 0, mWidth, mLabelHeight};
    SDL_FillRect(mScreen, &rect, SDL_MapRGB(mScreen->format, 255, 255, 0));
    /* now the text */
    if (mLabelFont && mSecureLabelText)
    {
        SDL_Color clrFg = {0, 0, 255, 0};
        SDL_Surface *sText = TTF_RenderUTF8_Solid(mLabelFont, mSecureLabelText, clrFg);
        rect.x = 10;
        SDL_BlitSurface(sText, NULL, mScreen, &rect);
        SDL_FreeSurface(sText);
    }
    /* make sure to update the screen */
    SDL_UpdateRect(mScreen, 0, 0, mWidth, mLabelHeight);
}
#endif /* VBOX_SECURELABEL */

#if 0
void SDL_DrawCursorSlow(SDL_Surface *screen, SDL_Cursor *SDL_cursor, SDL_Rect *area)
{
	const Uint32 pixels[2] = { 0xFFFFFF, 0x000000 };
	int h;
	int x, minx, maxx;
	Uint8 *data, datab = 0;
	Uint8 *mask, maskb = 0;
	Uint8 *dst;
	int dstbpp, dstskip;
	Uint8 pixels8[2];

	data = SDL_cursor->data + area->y * SDL_cursor->area.w/8;
	mask = SDL_cursor->mask + area->y * SDL_cursor->area.w/8;
	dstbpp = screen->format->BytesPerPixel;
	dst = (Uint8 *)screen->pixels +
                       (SDL_cursor->area.y+area->y)*screen->pitch +
                       SDL_cursor->area.x*dstbpp;
	dstskip = screen->pitch-SDL_cursor->area.w*dstbpp;

	minx = area->x;
	maxx = area->x+area->w;
#if 0
	if ( screen->format->BytesPerPixel == 1 ) {
		//if ( palette_changed ) {
			pixels8[1] = (Uint8)SDL_MapRGB(screen->format, 255, 255, 255);
			pixels8[0] = (Uint8)SDL_MapRGB(screen->format, 0, 0, 0);
		//	palette_changed = 0;
		//}
		for ( h=area->h; h; h-- ) {
			for ( x=0; x<SDL_cursor->area.w; ++x ) {
				if ( (x%8) == 0 ) {
					maskb = *mask++;
					datab = *data++;
				}
				if ( (x >= minx) && (x < maxx) ) {
					if ( maskb & 0x80 ) {
						SDL_memset(dst, pixels8[datab>>7], dstbpp);
					}
				}
				maskb <<= 1;
				datab <<= 1;
				dst += dstbpp;
			}
			dst += dstskip;
		}
	} else 
#endif
{
		for ( h=area->h; h; h-- ) {
			for ( x=0; x<SDL_cursor->area.w; ++x ) {
				if ( (x%8) == 0 ) {
					maskb = *mask++;
					datab = *data++;
				}
				if ( (x >= minx) && (x < maxx) ) {
					if ( maskb & 0x80 ) {
						SDL_memset(dst, pixels[datab>>7], dstbpp);
					}
				}
				maskb <<= 1;
				datab <<= 1;
				dst += dstbpp;
			}
			dst += dstskip;
		}
	}
}

static /* Software cursor drawing support */
SDL_Cursor * iSDL_CreateCursor (Uint8 *data, Uint8 *mask, 
					int w, int h, int hot_x, int hot_y)
{
	// SDL_VideoDevice *video = current_video;
	int savelen;
	int i;
	SDL_Cursor *cursor;

	/* Make sure the width is a multiple of 8 */
	w = ((w+7)&~7);

	/* Sanity check the hot spot */
	if ( (hot_x < 0) || (hot_y < 0) || (hot_x >= w) || (hot_y >= h) ) {
		//SDL_SetError("Cursor hot spot doesn't lie within cursor");
		
				printf("%s: %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
		 __func__, __LINE__, (hot_x < 0) , (hot_y < 0) , (hot_x >= w) , (hot_y >= h) , hot_x, hot_y, w, h);
		return(NULL);
	}

	/* Allocate memory for the cursor */
	cursor = (SDL_Cursor *)SDL_malloc(sizeof *cursor);
	if ( cursor == NULL ) {
		SDL_OutOfMemory();
		return(NULL);
	}
	savelen = (w*4)*h;
	cursor->area.x = 0;
	cursor->area.y = 0;
	cursor->area.w = w;
	cursor->area.h = h;
	cursor->hot_x = hot_x;
	cursor->hot_y = hot_y;
	cursor->data = (Uint8 *)SDL_malloc((w/8)*h*2);
	cursor->mask = cursor->data+((w/8)*h);
	cursor->save[0] = (Uint8 *)SDL_malloc(savelen*2);
	cursor->save[1] = cursor->save[0] + savelen;
	cursor->wm_cursor = NULL;
	if ( ! cursor->data || ! cursor->save[0] ) {
		SDL_FreeCursor(cursor);
		SDL_OutOfMemory();
		return(NULL);
	}
	for ( i=((w/8)*h)-1; i>=0; --i ) {
		cursor->data[i] = data[i];
		cursor->mask[i] = mask[i] | data[i];
	}
	SDL_memset(cursor->save[0], 0, savelen*2);

	/* If the window manager gives us a good cursor, we're done! */
#if 0
	if ( video->CreateWMCursor ) {
		cursor->wm_cursor = video->CreateWMCursor(video, data, mask,
							w, h, hot_x, hot_y);
	} else {
		cursor->wm_cursor = NULL;
	}
#endif
	return(cursor);
}
#endif

HRESULT SDLFramebuffer::DrawCursor(bool fVisible,
                                   bool fAlpha, uint32_t xHot,
                                   uint32_t yHot, uint32_t width,
                                   uint32_t height, void *pShape)
{
#if 0
    SDL_Cursor *c;
    uint8_t *p = (uint8_t*)pShape;
    uint32_t andMaskSize = (width + 7) / 8 * height;
    uint32_t srcShapePtrScan = width * 4;

    uint8_t *srcAndMaskPtr = p;
    uint8_t *srcShapePtr = p+ ((andMaskSize + 3) & ~3);
    int i;

    for (i = 0; i < andMaskSize; i++) {
       srcAndMaskPtr[i] = ~srcAndMaskPtr[i];
    }
    printf("and %d\n", ((andMaskSize + 3) & ~3));
    c = iSDL_CreateCursor(srcShapePtr, srcAndMaskPtr, width, height, xHot, yHot);
    if (!c) {
				printf("%s: %d, %d, %d, %d, %d\n", __func__, __LINE__, width, height, xHot, yHot);
        return E_INVALIDARG;
    }

    SDL_Rect area;
    area.x = 0;
    area.y = 0;
    area.w = width;
    area.h = height;

				printf("%s: %d\n", __func__, __LINE__);
    /* Get the mouse rectangle, clipped to the screen */
    if ( (area.w == 0) || (area.h == 0) ) {
        return S_OK;
    }
				printf("%s: %d\n", __func__, __LINE__);

	/* Copy mouse background */
	{ int w, h, screenbpp;
	  Uint8 *src, *dst;

	  /* Set up the copy pointers */
	  screenbpp = mScreen->format->BytesPerPixel;
#if 0
	  if ( (mScreen == SDL_VideoSurface) ||
	          FORMAT_EQUAL(mScreen->format, SDL_VideoSurface->format) ) {
		dst = c->save[0];
	  } else {
		dst = c->save[1];
	  }
#endif
	  dst = c->save[0];
	  src = (Uint8 *)mScreen->pixels + area.y * mScreen->pitch +
                                          area.x * screenbpp;

	  /* Perform the copy */
	  w = area.w*screenbpp;
	  h = area.h;
	  while ( h-- ) {
		  SDL_memcpy(dst, src, w);
		  dst += w;
		  src += mScreen->pitch;
	  }
	}

	/* Draw the mouse cursor */
	// area.x -= c->area.x;
	// area.y -= c->area.y;
	if ( (area.x == 0) && (area.w == c->area.w) ) {
//		SDL_DrawCursorFast(mScreen, &area);
	} else {
//		SDL_DrawCursorSlow(mScreen, &area);
	}
	// SDL_BlitSurface(c, NULL, mScreen, &area);
	SDL_DrawCursorSlow(mScreen, c, &area);

    SDL_FreeCursor(c);
#endif
    return S_OK;
}
