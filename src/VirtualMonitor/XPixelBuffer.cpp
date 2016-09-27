#include "XPixelBuffer.h"


XPixelBuffer::XPixelBuffer()
{

}

XPixelBuffer::~XPixelBuffer()
{    
    if (xim)
      XDestroyImage(xim);

    if (damage)
        XDamageDestroy(dpy, damage);

    if (pixmap)
        XFreePixmap(dpy,pixmap);

    if (src)
        XRenderFreePicture(dpy,src);

    if (dest)
        XRenderFreePicture(dpy,dest);

}

char* XPixelBuffer::getFrameBuffer()
{
    return xim->data;
}

void XPixelBuffer::refresh()
{
    XGetImage(dpy,root,0,0,xRes*xScale,yRes*yScale, AllPlanes, ZPixmap);
}

int XPixelBuffer::initialise(uint32_t xRes, uint32_t yRes, uint32_t bpp)
{
    if (!(dpy = XOpenDisplay(0))) {     
      fprintf(stderr,"Unable to open display \"%s\"\r\n",
               XDisplayName(0));
       return -1;
    }

    int rootxRes = DisplayWidth(dpy, DefaultScreen(dpy));
    int rootyRes = DisplayHeight(dpy, DefaultScreen(dpy));

    xScale = (double)rootxRes/(double)xRes;
    yScale = (double)rootyRes/(double)yRes;

    this->bpp=bpp;
    this->xRes=xRes;
    this->yRes=yRes;

    Visual* vis = DefaultVisual(dpy, DefaultScreen(dpy));

    root = DefaultRootWindow(dpy);

    if (xScale!=1.0 || yScale!=1.0)
    {
        XPixelBuffer::initScaling(vis);
    }

    xim = XCreateImage(dpy, vis, DefaultDepth(dpy, DefaultScreen(dpy)),
                       ZPixmap, 0, 0, xRes, yRes, BitmapPad(dpy), 0);

    xim->data = (char *)malloc(xim->bytes_per_line * xim->height);

    if (xim->data == NULL) {
      fprintf(stderr,"malloc() failed");\
      return 1;
    }

    XPixelBuffer::refresh();

    int rc= XPixelBuffer::subscribe();
    if (rc)
    {
        fprintf(stderr,"Subscribe to XDamage events failed");
        return rc;
    }

    return 0;

}

void XPixelBuffer::initScaling(Visual * vis)
{
    int rootDepth = DefaultDepth(dpy, DefaultScreen(dpy));
    int zero = 0;
    int one = 1;

    scale.matrix[0][0] = XDoubleToFixed (xScale);
    scale.matrix[0][1] = zero;
    scale.matrix[0][2] = zero;
    scale.matrix[1][0] = zero;
    scale.matrix[1][1] = XDoubleToFixed (yScale);
    scale.matrix[1][2] = zero;
    scale.matrix[2][0] = zero;
    scale.matrix[2][1] = zero;
    scale.matrix[2][2] = XDoubleToFixed (one);

    pixmap = XCreatePixmap(dpy, root, this->xRes, this->yRes, rootDepth);
    format = XRenderFindVisualFormat(dpy,vis);
    src = XRenderCreatePicture(dpy,root, format,0,0);
    dest= XRenderCreatePicture(dpy,pixmap,format,0,0);

    XRenderSetPictureFilter(dpy, src, FilterBest, NULL, 0);
    XRenderSetPictureTransform(dpy,src,&scale);
}

int XPixelBuffer::subscribe()
{    
   x11_fd = ConnectionNumber(dpy);

   XSelectInput(dpy, root, PropertyChangeMask);

   int xdamageErrorBase;

   if (XDamageQueryExtension(dpy, &xdamageEventBase, &xdamageErrorBase)) {
   } else {
       fprintf(stderr,"XDmamageQueryExtension call failed");
   }

   damage = XDamageCreate(dpy, root,XDamageReportRawRectangles);

   return 0;
}

int XPixelBuffer::getEvent(Event &evt)
{   
    FD_ZERO(&in_fds);
    FD_SET(x11_fd, &in_fds);

    // One second blocking each iteration
    tv.tv_usec = 0;
    tv.tv_sec = 1;

    // Wait for X Event or a Timer
    int num_ready_fds = select(x11_fd + 1, &in_fds, NULL, NULL, &tv);

    if (num_ready_fds>0)
    {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            int rc= makeEvt(&ev, evt);
            if (rc)
                continue;
            if (XPending(dpy)==0)
            {
                //notify on last
                return 0;
            }
        }
    }
    return 0;

}

int XPixelBuffer::makeEvt(XEvent* ev, Event& evt) {

    XDamageNotifyEvent* dev;

    if (ev->type != xdamageEventBase)
       return true;

    dev = (XDamageNotifyEvent*)ev;

    evt.code = EVENT_DITRY_AREA;
    evt.dirtyArea.left = dev->area.x * xScale;
    evt.dirtyArea.top = dev->area.y * yScale;
    evt.dirtyArea.right = dev->area.width * xScale;
    evt.dirtyArea.bottom = dev->area.height * yScale;

    return 0;
}





