/*-------------------------------------------------------------------------*
 * File:  casefile.c
 *-------------------------------------------------------------------------*/
/**
 * Case-insensitive file lookup for case-sensitive filesystems.
 *
 * The game was written for DOS/Windows, where "sounds.res" and "SOUNDS.RES"
 * are the same file.  The data files ship in upper case while the code asks
 * for them in a mix of cases.  Linux (and Emscripten's virtual filesystem) are
 * case sensitive, so open()/fopen() are wrapped at link time
 * (-Wl,--wrap=open -Wl,--wrap=fopen).  If the path exists as written it is
 * used untouched; otherwise each path component is matched against the
 * directory contents ignoring case, and backslashes are treated as '/'.
 *
 *<!-----------------------------------------------------------------------*/
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define CASEFILE_PATH_MAX 1024

int __real_open(const char *path, int flags, ...);
FILE *__real_fopen(const char *path, const char *mode);

static int IExists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* Look for a directory entry in dir that matches name ignoring case. */
static int IFindEntry(const char *dir, const char *name, char *found, size_t foundSize)
{
    DIR *d = opendir(dir[0] ? dir : ".");
    struct dirent *e;
    int ok = 0;

    if (d == NULL)
        return 0;
    while ((e = readdir(d)) != NULL) {
        if (strcasecmp(e->d_name, name) == 0) {
            strncpy(found, e->d_name, foundSize - 1);
            found[foundSize - 1] = '\0';
            ok = 1;
            break;
        }
    }
    closedir(d);
    return ok;
}

/* Fill out with the best case-corrected spelling of path. */
static const char *IResolve(const char *path, char *out, size_t outSize)
{
    char work[CASEFILE_PATH_MAX];
    char *comp;
    size_t len = 0;
    int i;

    if (path == NULL || IExists(path))
        return path;
    if (strlen(path) >= sizeof(work))
        return path;

    strcpy(work, path);
    for (i = 0; work[i]; i++)
        if (work[i] == '\\')
            work[i] = '/';
    if (IExists(work)) {
        strcpy(out, work);
        return out;
    }

    out[0] = '\0';
    comp = work;
    if (*comp == '/') {
        strcpy(out, "/");
        len = 1;
        comp++;
    }
    while (*comp) {
        char *slash = strchr(comp, '/');
        char name[CASEFILE_PATH_MAX];
        char real[CASEFILE_PATH_MAX];
        size_t nameLen = slash ? (size_t)(slash - comp) : strlen(comp);

        if (nameLen == 0) {                     /* "//" */
            comp++;
            continue;
        }
        memcpy(name, comp, nameLen);
        name[nameLen] = '\0';

        if (len + nameLen + 2 >= outSize)
            return path;
        strcpy(real, name);
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            char probe[CASEFILE_PATH_MAX];
            snprintf(probe, sizeof(probe), "%s%s", out, name);
            if (!IExists(probe))
                IFindEntry(out, name, real, sizeof(real));
        }
        strcpy(out + len, real);
        len += strlen(real);
        comp += nameLen;
        if (*comp == '/') {
            out[len++] = '/';
            out[len] = '\0';
            comp++;
        }
    }
    return out;
}

int __wrap_open(const char *path, int flags, ...)
{
    char fixed[CASEFILE_PATH_MAX];
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return __real_open(IResolve(path, fixed, sizeof(fixed)), flags, mode);
}

FILE *__wrap_fopen(const char *path, const char *mode)
{
    char fixed[CASEFILE_PATH_MAX];

    return __real_fopen(IResolve(path, fixed, sizeof(fixed)), mode);
}
