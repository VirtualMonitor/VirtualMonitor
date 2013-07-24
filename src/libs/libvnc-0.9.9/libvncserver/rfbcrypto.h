#ifndef _RFB_CRYPTO_H
#define _RFB_CRYPTO_H 1

#if 1
#ifdef _MSC_VER
#define __func__ __FUNCTION__
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_USE_INCLUDES
#define LWIP_PROVIDE_ERRNO
#define PACK_STRUCT_BEGIN #pragma pack(push,1)
#define PACK_STRUCT_END  #pragma pack(pop)
#elif defined(__GNUC__)
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((__packed__))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#include <errno.h>
#else
#error This header file has not been ported yet for this compiler.
#endif
#endif


#ifndef _MSC_VER
#include <sys/uio.h>
#else
struct iovec
{
    char *iov_base;
    size_t iov_len;
};
#endif

#define SHA1_HASH_SIZE 20
#define MD5_HASH_SIZE 16

void digestmd5(const struct iovec *iov, int iovcnt, void *dest);
void digestsha1(const struct iovec *iov, int iovcnt, void *dest);

#endif
