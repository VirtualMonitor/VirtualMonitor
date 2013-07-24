#if defined(WIN32) && defined(_MSC_VER)
#include <stdlib.h>
#include <malloc.h>
#include "winapi.h"
/*
Version Number    Description
6.1               Windows 7     / Windows 2008 R2
6.0               Windows Vista / Windows 2008
5.2               Windows 2003 
5.1               Windows XP
5.0               Windows 2000
*/
BOOL IsWinVistaOrLater() {
    DWORD version = GetVersion();
    DWORD major = (DWORD) (LOBYTE(LOWORD(version)));
    DWORD minor = (DWORD) (HIBYTE(LOWORD(version)));

    return (major > 6);
}


DIR *opendir(const char *name)
{
    char *path;
    HANDLE h;
    WIN32_FIND_DATAA *fd;
    DIR *dir;
    int namlen;
    
    if (name == NULL) {
        return NULL;
    }
    
    if ((namlen = strlen(name)) == -1) {
        return NULL;
    }
    
    if (name[namlen -1] == '\\' || name[namlen -1] == '/') {
        path = _alloca(namlen + 2);
        strcpy_s(path, namlen + 2, name);
        path[namlen] = '*';
        path[namlen + 1] = '\0';
    } else {
        path = _alloca(namlen + 3);
        strcpy_s(path, namlen + 3, name);
        
        path[namlen] = (strchr(name, '/') != NULL) ? '/' : '\\';
        path[namlen + 1] = '*';
        path[namlen + 2] = '\0';
    }
    
    if ((fd = malloc(sizeof(WIN32_FIND_DATA))) == NULL) {
        return NULL;
    }
    
    if ((h = FindFirstFileA(path, fd)) == INVALID_HANDLE_VALUE) {
        free(fd);
        return NULL;
    }
    
    if ((dir = malloc(sizeof(DIR))) == NULL) {
        FindClose(h);
        free(fd);
        return NULL;
    }
    
    dir->h = h;
    dir->fd = fd;
    dir->has_next = TRUE;
    
    return dir;
}

struct dirent *readdir(DIR *dir) 
{
    char *cFileName;
    char *d_name;
    
    if (dir == NULL) {
        return NULL;
    }
    if (dir->fd == NULL) {
        return NULL;
    }
    
    if (!dir->has_next) {
        return NULL;
    }
    cFileName = dir->fd->cFileName;
    d_name = dir->entry.d_name;
    strcpy_s(d_name, _MAX_PATH, cFileName);
    dir->has_next = FindNextFileA(dir->h, dir->fd);
    
    return &dir->entry;
}

int closedir(DIR *dir)
{
    if (dir == NULL) {
        return -1;
    }

    if (dir->h && dir->h != INVALID_HANDLE_VALUE) {
        FindClose(dir->h);
    }

    if (dir->fd) {
        free(dir->fd);
    }

    free(dir);
    return 0;
}
#endif


