#ifndef RFB_PRIVATE_H
#define RFB_PRIVATE_H

#ifdef _MSC_VER
#undef SOCKET
#include <winsock2.h>
#ifdef LIBVNCSERVER_HAVE_WS2TCPIP_H
#undef socklen_t
#include <ws2tcpip.h>
#endif
#endif

/* from cursor.c */

void rfbShowCursor(rfbClientPtr cl);
void rfbHideCursor(rfbClientPtr cl);
void rfbRedrawAfterHideCursor(rfbClientPtr cl,sraRegionPtr updateRegion);

/* from main.c */

rfbClientPtr rfbClientIteratorHead(rfbClientIteratorPtr i);

/* from tight.c */

#ifdef LIBVNCSERVER_HAVE_LIBZ
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
extern void rfbTightCleanup(rfbScreenInfoPtr screen);
#endif

/* from zlib.c */
extern void rfbZlibCleanup(rfbScreenInfoPtr screen);

/* from zrle.c */
void rfbFreeZrleData(rfbClientPtr cl);

#endif


/* from ultra.c */

extern void rfbFreeUltraData(rfbClientPtr cl);

#endif

