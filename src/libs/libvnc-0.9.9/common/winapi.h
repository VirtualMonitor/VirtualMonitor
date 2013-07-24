#if defined(WIN32) && defined(_MSC_VER)
#ifndef _WIN_API_H_
#define _WIN_API_H_

#include <windows.h>

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#endif

typedef unsigned int lino_t;

struct dirent {
  lino_t          d_ino;
  char           d_name[_MAX_PATH];
};

typedef struct {
  HANDLE h;
  WIN32_FIND_DATAA *fd;
  BOOL has_next;
  struct dirent entry;
} DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

BOOL IsWinVistaOrLater();

#endif /*_WIN_API_H_*/
#endif

