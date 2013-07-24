/* $Id: kHlpAssert.h 46 2012-03-17 01:39:33Z bird $ */
/** @file
 * kHlpAssert - Assertion Macros.
 */

/*
 * Copyright (c) 2006-2007 Knut St. Osmundsen <bird-kStuff-spamix@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ___kHlpAssert_h___
#define ___kHlpAssert_h___

#include <k/kHlpDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup   grp_kHlpAssert - Assertion Macros
 * @addtogroup grp_kHlp
 * @{ */

/** @def K_STRICT
 * Assertions are enabled when K_STRICT is \#defined. */

/** @def kHlpAssertBreakpoint
 * Emits a breakpoint instruction or somehow triggers a debugger breakpoint.
 */
#ifdef _MSC_VER
# define kHlpAssertBreakpoint() do { __debugbreak(); } while (0)
#elif defined(__GNUC__) && K_OS == K_OS_SOLARIS && (K_ARCH == K_ARCH_AMD64 || K_ARCH == K_ARCH_X86_32)
# define kHlpAssertBreakpoint() do { __asm__ __volatile__ ("int $3"); } while (0)
#elif defined(__GNUC__) && (K_ARCH == K_ARCH_AMD64 || K_ARCH == K_ARCH_X86_32 || K_ARCH == K_ARCH_X86_16)
# define kHlpAssertBreakpoint() do { __asm__ __volatile__ ("int3"); } while (0)
#else
# error "Port Me"
#endif

#ifdef K_STRICT

# define kHlpAssert(expr) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertBreakpoint(); \
        } \
    } while (0)

# define kHlpAssertStmt(expr, stmt) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertBreakpoint(); \
            stmt; \
        } \
    } while (0)

# define kHlpAssertReturn(expr, rcRet) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertBreakpoint(); \
            return (rcRet); \
        } \
    } while (0)

# define kHlpAssertStmtReturn(expr, stmt, rcRet) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertBreakpoint(); \
            stmt; \
            return (rcRet); \
        } \
    } while (0)

# define kHlpAssertReturnVoid(expr) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertBreakpoint(); \
            return; \
        } \
    } while (0)

# define kHlpAssertStmtReturnVoid(expr, stmt) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertBreakpoint(); \
            stmt; \
            return; \
        } \
    } while (0)

# define kHlpAssertMsg(expr, msg) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertMsg2 msg; \
            kHlpAssertBreakpoint(); \
        } \
    } while (0)

# define kHlpAssertMsgStmt(expr, msg, stmt) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertMsg2 msg; \
            kHlpAssertBreakpoint(); \
            stmt; \
        } \
    } while (0)

# define kHlpAssertMsgReturn(expr, msg, rcRet) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertMsg2 msg; \
            kHlpAssertBreakpoint(); \
            return (rcRet); \
        } \
    } while (0)

# define kHlpAssertMsgStmtReturn(expr, msg, stmt, rcRet) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertMsg2 msg; \
            kHlpAssertBreakpoint(); \
            stmt; \
            return (rcRet); \
        } \
    } while (0)

# define kHlpAssertMsgReturnVoid(expr, msg) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertMsg2 msg; \
            kHlpAssertBreakpoint(); \
            return; \
        } \
    } while (0)

# define kHlpAssertMsgStmtReturnVoid(expr, msg, stmt) \
    do { \
        if (!(expr)) \
        { \
            kHlpAssertMsg1(#expr, __FILE__, __LINE__, __FUNCTION__); \
            kHlpAssertMsg2 msg; \
            kHlpAssertBreakpoint(); \
            stmt; \
            return; \
        } \
    } while (0)

#else   /* !K_STRICT */
# define kHlpAssert(expr)                                   do { } while (0)
# define kHlpAssertStmt(expr, stmt)                         do { if (!(expr)) { stmt; }  } while (0)
# define kHlpAssertReturn(expr, rcRet)                      do { if (!(expr)) return (rcRet); } while (0)
# define kHlpAssertStmtReturn(expr, stmt, rcRet)            do { if (!(expr)) { stmt; return (rcRet); } } while (0)
# define kHlpAssertReturnVoid(expr)                         do { if (!(expr)) return; } while (0)
# define kHlpAssertStmtReturnVoid(expr, stmt)               do { if (!(expr)) { stmt; return; } } while (0)
# define kHlpAssertMsg(expr, msg)                           do { } while (0)
# define kHlpAssertMsgStmt(expr, msg, stmt)                 do { if (!(expr)) { stmt; } } while (0)
# define kHlpAssertMsgReturn(expr, msg, rcRet)              do { if (!(expr)) return (rcRet); } while (0)
# define kHlpAssertMsgStmtReturn(expr, msg, stmt, rcRet)    do { if (!(expr)) { stmt; return (rcRet); } } while (0)
# define kHlpAssertMsgReturnVoid(expr, msg)                 do { if (!(expr)) return; } while (0)
# define kHlpAssertMsgStmtReturnVoid(expr, msg, stmt)       do { if (!(expr)) { stmt; return; } } while (0)
#endif  /* !K_STRICT */

#define kHlpAssertPtr(ptr)                      kHlpAssertMsg(K_VALID_PTR(ptr), ("%s = %p\n", #ptr, (ptr)))
#define kHlpAssertPtrReturn(ptr, rcRet)         kHlpAssertMsgReturn(K_VALID_PTR(ptr), ("%s = %p -> %d\n", #ptr, (ptr), (rcRet)), (rcRet))
#define kHlpAssertPtrReturn(ptr, rcRet)         kHlpAssertMsgReturn(K_VALID_PTR(ptr), ("%s = %p -> %d\n", #ptr, (ptr), (rcRet)), (rcRet))
#define kHlpAssertPtrReturnVoid(ptr)            kHlpAssertMsgReturnVoid(K_VALID_PTR(ptr), ("%s = %p -> %d\n", #ptr, (ptr), (rcRet)))
#define kHlpAssertPtrNull(ptr)                  kHlpAssertMsg(!(ptr) || K_VALID_PTR(ptr), ("%s = %p\n", #ptr, (ptr)))
#define kHlpAssertPtrNullReturn(ptr, rcRet)     kHlpAssertMsgReturn(!(ptr) || K_VALID_PTR(ptr), ("%s = %p -> %d\n", #ptr, (ptr), (rcRet)), (rcRet))
#define kHlpAssertPtrNullReturnVoid(ptr)        kHlpAssertMsgReturnVoid(!(ptr) || K_VALID_PTR(ptr), ("%s = %p -> %d\n", #ptr, (ptr), (rcRet)))
#define kHlpAssertRC(rc)                        kHlpAssertMsg((rc) == 0, ("%s = %d\n", #rc, (rc)))
#define kHlpAssertRCReturn(rc, rcRet)           kHlpAssertMsgReturn((rc) == 0, ("%s = %d -> %d\n", #rc, (rc), (rcRet)), (rcRet))
#define kHlpAssertRCReturnVoid(rc)              kHlpAssertMsgReturnVoid((rc) == 0, ("%s = %d -> %d\n", #rc, (rc), (rcRet)))
#define kHlpAssertFailed()                      kHlpAssert(0)
#define kHlpAssertFailedReturn(rcRet)           kHlpAssertReturn(0, (rcRet))
#define kHlpAssertFailedReturnVoid()            kHlpAssertReturnVoid(0)
#define kHlpAssertMsgFailed(msg)                kHlpAssertMsg(0, msg)
#define kHlpAssertMsgFailedReturn(msg, rcRet)   kHlpAssertMsgReturn(0, msg, (rcRet))
#define kHlpAssertMsgFailedReturnVoid(msg)      kHlpAssertMsgReturnVoid(0, msg))

/**
 * Helper function that displays the first part of the assertion message.
 *
 * @param   pszExpr         The expression.
 * @param   pszFile         The file name.
 * @param   iLine           The line number is the file.
 * @param   pszFunction     The function name.
 * @internal
 */
KHLP_DECL(void) kHlpAssertMsg1(const char *pszExpr, const char *pszFile, unsigned iLine, const char *pszFunction);

/**
 * Helper function that displays custom assert message.
 *
 * @param   pszFormat       Format string that get passed to vprintf.
 * @param   ...             Format arguments.
 * @internal
 */
KHLP_DECL(void) kHlpAssertMsg2(const char *pszFormat, ...);


/** @} */

#ifdef __cplusplus
}
#endif

#endif
