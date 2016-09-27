#ifndef XPIXELBUFFER_H
#define XPIXELBUFFER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <memory>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <time.h>
#include "DrvIntf.h"


class XPixelBuffer
{
public:

    XPixelBuffer();     
    ~XPixelBuffer();

     int initialise(uint32_t xRes, uint32_t yRes, uint32_t bpp);
     int getEvent(Event &evt);
     char* getFrameBuffer();
     void refresh();

     inline void refresh(int dst_x, int dst_y)
     {
         Drawable getFrom;
         if (xScale!=1.0 || yScale!=1.0)
         {
             XRenderComposite(dpy,PictOpOver,src,None,dest,0,0,0,0,0,0,xRes,yRes);
             getFrom = pixmap;
         }
         else
         {
             getFrom = root;
         }
         XGetSubImage(dpy, getFrom, 0, 0, xRes, yRes, AllPlanes, ZPixmap, xim, 0, 0);
     }


private:

    int subscribe();

    void initScaling(Visual * vis);   
    int makeEvt(XEvent* ev, Event& evt);

    int x11_fd;
    fd_set in_fds;
    int xdamageEventBase;
    struct timeval tv;

    Display* dpy;
    XImage *xim;
    Drawable root;
    Damage damage;
    Pixmap pixmap;
    Picture src;
    Picture dest;
    XTransform scale;
    XRenderPictFormat* format;

    uint32_t xRes;
    uint32_t yRes;
    uint32_t bpp;

    double xScale;
    double yScale;

};

#endif // XPIXELBUFFER_H
