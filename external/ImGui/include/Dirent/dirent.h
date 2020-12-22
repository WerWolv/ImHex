/*
 * Dirent interface for Microsoft Visual Studio
 * Version 1.23.1
 *
 * Copyright (C) 2006-2012 Toni Ronkko
 * This file is part of dirent.  Dirent may be freely distributed
 * under the MIT license.  For all details and documentation, see
 * https://github.com/tronkko/dirent
 */
#ifndef DIRENT_H
#define DIRENT_H

/*
 * Include windows.h without Windows Sockets 1.1 to prevent conflicts with
 * Windows Sockets 2.0.
 */
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* Indicates that d_type field is available in dirent structure */
#define _DIRENT_HAVE_D_TYPE

/* Indicates that d_namlen field is available in dirent structure */
#define _DIRENT_HAVE_D_NAMLEN

/* Entries missing from MSVC 6.0 */
#if !defined(FILE_ATTRIBUTE_DEVICE)
#   define FILE_ATTRIBUTE_DEVICE 0x40
#endif

/* File type and permission flags for stat(), general mask */
#if !defined(S_IFMT)
#   define S_IFMT _S_IFMT
#endif

/* Directory bit */
#if !defined(S_IFDIR)
#   define S_IFDIR _S_IFDIR
#endif

/* Character device bit */
#if !defined(S_IFCHR)
#   define S_IFCHR _S_IFCHR
#endif

/* Pipe bit */
#if !defined(S_IFFIFO)
#   define S_IFFIFO _S_IFFIFO
#endif

/* Regular file bit */
#if !defined(S_IFREG)
#   define S_IFREG _S_IFREG
#endif

/* Read permission */
#if !defined(S_IREAD)
#   define S_IREAD _S_IREAD
#endif

/* Write permission */
#if !defined(S_IWRITE)
#   define S_IWRITE _S_IWRITE
#endif

/* Execute permission */
#if !defined(S_IEXEC)
#   define S_IEXEC _S_IEXEC
#endif

/* Pipe */
#if !defined(S_IFIFO)
#   define S_IFIFO _S_IFIFO
#endif

/* Block device */
#if !defined(S_IFBLK)
#   define S_IFBLK 0
#endif

/* Link */
#if !defined(S_IFLNK)
#   define S_IFLNK 0
#endif

/* Socket */
#if !defined(S_IFSOCK)
#   define S_IFSOCK 0
#endif

/* Read user permission */
#if !defined(S_IRUSR)
#   define S_IRUSR S_IREAD
#endif

/* Write user permission */
#if !defined(S_IWUSR)
#   define S_IWUSR S_IWRITE
#endif

/* Execute user permission */
#if !defined(S_IXUSR)
#   define S_IXUSR 0
#endif

/* Read group permission */
#if !defined(S_IRGRP)
#   define S_IRGRP 0
#endif

/* Write group permission */
#if !defined(S_IWGRP)
#   define S_IWGRP 0
#endif

/* Execute group permission */
#if !defined(S_IXGRP)
#   define S_IXGRP 0
#endif

/* Read others permission */
#if !defined(S_IROTH)
#   define S_IROTH 0
#endif

/* Write others permission */
#if !defined(S_IWOTH)
#   define S_IWOTH 0
#endif

/* Execute others permission */
#if !defined(S_IXOTH)
#   define S_IXOTH 0
#endif

/* Maximum length of file name */
#if !defined(PATH_MAX)
#   define PATH_MAX MAX_PATH
#endif
#if !defined(FILENAME_MAX)
#   define FILENAME_MAX MAX_PATH
#endif
#if !defined(NAME_MAX)
#   define NAME_MAX FILENAME_MAX
#endif

/* File type flags for d_type */
#define DT_UNKNOWN 0
#define DT_REG S_IFREG
#define DT_DIR S_IFDIR
#define DT_FIFO S_IFIFO
#define DT_SOCK S_IFSOCK
#define DT_CHR S_IFCHR
#define DT_BLK S_IFBLK
#define DT_LNK S_IFLNK

/* Macros for converting between st_mode and d_type */
#define IFTODT(mode) ((mode) & S_IFMT)
#define DTTOIF(type) (type)

/*
 * File type macros.  Note that block devices, sockets and links cannot be
 * distinguished on Windows and the macros S_ISBLK, S_ISSOCK and S_ISLNK are
 * only defined for compatibility.  These macros should always return false
 * on Windows.
 */
#if !defined(S_ISFIFO)
#   define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#endif
#if !defined(S_ISDIR)
#   define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG)
#   define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISLNK)
#   define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK)
#   define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISCHR)
#   define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISBLK)
#   define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#endif

/* Return the exact length of the file name without zero terminator */
#define _D_EXACT_NAMLEN(p) ((p)->d_namlen)

/* Return the maximum size of a file name */
#define _D_ALLOC_NAMLEN(p) ((PATH_MAX)+1)


#ifdef __cplusplus
extern "C" {
#endif


/* Wide-character version */
struct _wdirent {
    /* Always zero */
    long d_ino;

    /* File position within stream */
    long d_off;

    /* Structure size */
    unsigned short d_reclen;

    /* Length of name without \0 */
    size_t d_namlen;

    /* File type */
    int d_type;

    /* File name */
    wchar_t d_name[PATH_MAX+1];
};
typedef struct _wdirent _wdirent;

struct _WDIR {
    /* Current directory entry */
    struct _wdirent ent;

    /* Private file data */
    WIN32_FIND_DATAW data;

    /* True if data is valid */
    int cached;

    /* Win32 search handle */
    HANDLE handle;

    /* Initial directory name */
    wchar_t *patt;
};
typedef struct _WDIR _WDIR;

/* Multi-byte character version */
struct dirent {
    /* Always zero */
    long d_ino;

    /* File position within stream */
    long d_off;

    /* Structure size */
    unsigned short d_reclen;

    /* Length of name without \0 */
    size_t d_namlen;

    /* File type */
    int d_type;

    /* File name */
    char d_name[PATH_MAX+1];
};
typedef struct dirent dirent;

struct DIR {
    struct dirent ent;
    struct _WDIR *wdirp;
};
typedef struct DIR DIR;


/* Dirent functions */
static DIR *opendir (const char *dirname);
static _WDIR *_wopendir (const wchar_t *dirname);

static struct dirent *readdir (DIR *dirp);
static struct _wdirent *_wreaddir (_WDIR *dirp);

static int readdir_r(
    DIR *dirp, struct dirent *entry, struct dirent **result);
static int _wreaddir_r(
    _WDIR *dirp, struct _wdirent *entry, struct _wdirent **result);

static int closedir (DIR *dirp);
static int _wclosedir (_WDIR *dirp);

static void rewinddir (DIR* dirp);
static void _wrewinddir (_WDIR* dirp);

static int scandir (const char *dirname, struct dirent ***namelist,
    int (*filter)(const struct dirent*),
    int (*compare)(const struct dirent**, const struct dirent**));

static int alphasort (const struct dirent **a, const struct dirent **b);

static int versionsort (const struct dirent **a, const struct dirent **b);


/* For compatibility with Symbian */
#define wdirent _wdirent
#define WDIR _WDIR
#define wopendir _wopendir
#define wreaddir _wreaddir
#define wclosedir _wclosedir
#define wrewinddir _wrewinddir


/* Internal utility functions */
static WIN32_FIND_DATAW *dirent_first (_WDIR *dirp);
static WIN32_FIND_DATAW *dirent_next (_WDIR *dirp);

static int dirent_mbstowcs_s(
    size_t *pReturnValue,
    wchar_t *wcstr,
    size_t sizeInWords,
    const char *mbstr,
    size_t count);

static int dirent_wcstombs_s(
    size_t *pReturnValue,
    char *mbstr,
    size_t sizeInBytes,
    const wchar_t *wcstr,
    size_t count);

static void dirent_set_errno (int error);


/*
 * Open directory stream DIRNAME for read and return a pointer to the
 * internal working area that is used to retrieve individual directory
 * entries.
 */
static _WDIR*
_wopendir(
    const wchar_t *dirname)
{
    _WDIR *dirp = NULL;
    int error;

    /* Must have directory name */
    if (dirname == NULL  ||  dirname[0] == '\0') {
        dirent_set_errno (ENOENT);
        return NULL;
    }

    /* Allocate new _WDIR structure */
    dirp = (_WDIR*) malloc (sizeof (struct _WDIR));
    if (dirp != NULL) {
        DWORD n;

        /* Reset _WDIR structure */
        dirp->handle = INVALID_HANDLE_VALUE;
        dirp->patt = NULL;
        dirp->cached = 0;

        /* Compute the length of full path plus zero terminator
         *
         * Note that on WinRT there's no way to convert relative paths
         * into absolute paths, so just assume it is an absolute path.
         */
#       if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
            n = wcslen(dirname);
#       else
            n = GetFullPathNameW (dirname, 0, NULL, NULL);
#       endif

        /* Allocate room for absolute directory name and search pattern */
        dirp->patt = (wchar_t*) malloc (sizeof (wchar_t) * n + 16);
        if (dirp->patt) {

            /*
             * Convert relative directory name to an absolute one.  This
             * allows rewinddir() to function correctly even when current
             * working directory is changed between opendir() and rewinddir().
             *
             * Note that on WinRT there's no way to convert relative paths
             * into absolute paths, so just assume it is an absolute path.
             */
#           if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
                wcsncpy_s(dirp->patt, n+1, dirname, n);
#           else
                n = GetFullPathNameW (dirname, n, dirp->patt, NULL);
#           endif
            if (n > 0) {
                wchar_t *p;

                /* Append search pattern \* to the directory name */
                p = dirp->patt + n;
                if (dirp->patt < p) {
                    switch (p[-1]) {
                    case '\\':
                    case '/':
                    case ':':
                        /* Directory ends in path separator, e.g. c:\temp\ */
                        /*NOP*/;
                        break;

                    default:
                        /* Directory name doesn't end in path separator */
                        *p++ = '\\';
                    }
                }
                *p++ = '*';
                *p = '\0';

                /* Open directory stream and retrieve the first entry */
                if (dirent_first (dirp)) {
                    /* Directory stream opened successfully */
                    error = 0;
                } else {
                    /* Cannot retrieve first entry */
                    error = 1;
                    dirent_set_errno (ENOENT);
                }

            } else {
                /* Cannot retrieve full path name */
                dirent_set_errno (ENOENT);
                error = 1;
            }

        } else {
            /* Cannot allocate memory for search pattern */
            error = 1;
        }

    } else {
        /* Cannot allocate _WDIR structure */
        error = 1;
    }

    /* Clean up in case of error */
    if (error  &&  dirp) {
        _wclosedir (dirp);
        dirp = NULL;
    }

    return dirp;
}

/*
 * Read next directory entry.
 *
 * Returns pointer to static directory entry which may be overwritten by
 * subsequent calls to _wreaddir().
 */
static struct _wdirent*
_wreaddir(
    _WDIR *dirp)
{
    struct _wdirent *entry;

    /*
     * Read directory entry to buffer.  We can safely ignore the return value
     * as entry will be set to NULL in case of error.
     */
    (void) _wreaddir_r (dirp, &dirp->ent, &entry);

    /* Return pointer to statically allocated directory entry */
    return entry;
}

/*
 * Read next directory entry.
 *
 * Returns zero on success.  If end of directory stream is reached, then sets
 * result to NULL and returns zero.
 */
static int
_wreaddir_r(
    _WDIR *dirp,
    struct _wdirent *entry,
    struct _wdirent **result)
{
    WIN32_FIND_DATAW *datap;

    /* Read next directory entry */
    datap = dirent_next (dirp);
    if (datap) {
        size_t n;
        DWORD attr;

        /*
         * Copy file name as wide-character string.  If the file name is too
         * long to fit in to the destination buffer, then truncate file name
         * to PATH_MAX characters and zero-terminate the buffer.
         */
        n = 0;
        while (n < PATH_MAX  &&  datap->cFileName[n] != 0) {
            entry->d_name[n] = datap->cFileName[n];
            n++;
        }
        entry->d_name[n] = 0;

        /* Length of file name excluding zero terminator */
        entry->d_namlen = n;

        /* File type */
        attr = datap->dwFileAttributes;
        if ((attr & FILE_ATTRIBUTE_DEVICE) != 0) {
            entry->d_type = DT_CHR;
        } else if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            entry->d_type = DT_DIR;
        } else {
            entry->d_type = DT_REG;
        }

        /* Reset dummy fields */
        entry->d_ino = 0;
        entry->d_off = 0;
        entry->d_reclen = sizeof (struct _wdirent);

        /* Set result address */
        *result = entry;

    } else {

        /* Return NULL to indicate end of directory */
        *result = NULL;

    }

    return /*OK*/0;
}

/*
 * Close directory stream opened by opendir() function.  This invalidates the
 * DIR structure as well as any directory entry read previously by
 * _wreaddir().
 */
static int
_wclosedir(
    _WDIR *dirp)
{
    int ok;
    if (dirp) {

        /* Release search handle */
        if (dirp->handle != INVALID_HANDLE_VALUE) {
            FindClose (dirp->handle);
            dirp->handle = INVALID_HANDLE_VALUE;
        }

        /* Release search pattern */
        if (dirp->patt) {
            free (dirp->patt);
            dirp->patt = NULL;
        }

        /* Release directory structure */
        free (dirp);
        ok = /*success*/0;

    } else {

        /* Invalid directory stream */
        dirent_set_errno (EBADF);
        ok = /*failure*/-1;

    }
    return ok;
}

/*
 * Rewind directory stream such that _wreaddir() returns the very first
 * file name again.
 */
static void
_wrewinddir(
    _WDIR* dirp)
{
    if (dirp) {
        /* Release existing search handle */
        if (dirp->handle != INVALID_HANDLE_VALUE) {
            FindClose (dirp->handle);
        }

        /* Open new search handle */
        dirent_first (dirp);
    }
}

/* Get first directory entry (internal) */
static WIN32_FIND_DATAW*
dirent_first(
    _WDIR *dirp)
{
    WIN32_FIND_DATAW *datap;

    /* Open directory and retrieve the first entry */
    dirp->handle = FindFirstFileExW(
        dirp->patt, FindExInfoStandard, &dirp->data,
        FindExSearchNameMatch, NULL, 0);
    if (dirp->handle != INVALID_HANDLE_VALUE) {

        /* a directory entry is now waiting in memory */
        datap = &dirp->data;
        dirp->cached = 1;

    } else {

        /* Failed to re-open directory: no directory entry in memory */
        dirp->cached = 0;
        datap = NULL;

    }
    return datap;
}

/*
 * Get next directory entry (internal).
 *
 * Returns 
 */
static WIN32_FIND_DATAW*
dirent_next(
    _WDIR *dirp)
{
    WIN32_FIND_DATAW *p;

    /* Get next directory entry */
    if (dirp->cached != 0) {

        /* A valid directory entry already in memory */
        p = &dirp->data;
        dirp->cached = 0;

    } else if (dirp->handle != INVALID_HANDLE_VALUE) {

        /* Get the next directory entry from stream */
        if (FindNextFileW (dirp->handle, &dirp->data) != FALSE) {
            /* Got a file */
            p = &dirp->data;
        } else {
            /* The very last entry has been processed or an error occurred */
            FindClose (dirp->handle);
            dirp->handle = INVALID_HANDLE_VALUE;
            p = NULL;
        }

    } else {

        /* End of directory stream reached */
        p = NULL;

    }

    return p;
}

/*
 * Open directory stream using plain old C-string.
 */
static DIR*
opendir(
    const char *dirname) 
{
    struct DIR *dirp;
    int error;

    /* Must have directory name */
    if (dirname == NULL  ||  dirname[0] == '\0') {
        dirent_set_errno (ENOENT);
        return NULL;
    }

    /* Allocate memory for DIR structure */
    dirp = (DIR*) malloc (sizeof (struct DIR));
    if (dirp) {
        wchar_t wname[PATH_MAX + 1];
        size_t n;

        /* Convert directory name to wide-character string */
        error = dirent_mbstowcs_s(
            &n, wname, PATH_MAX + 1, dirname, PATH_MAX + 1);
        if (!error) {

            /* Open directory stream using wide-character name */
            dirp->wdirp = _wopendir (wname);
            if (dirp->wdirp) {
                /* Directory stream opened */
                error = 0;
            } else {
                /* Failed to open directory stream */
                error = 1;
            }

        } else {
            /*
             * Cannot convert file name to wide-character string.  This
             * occurs if the string contains invalid multi-byte sequences or
             * the output buffer is too small to contain the resulting
             * string.
             */
            error = 1;
        }

    } else {
        /* Cannot allocate DIR structure */
        error = 1;
    }

    /* Clean up in case of error */
    if (error  &&  dirp) {
        free (dirp);
        dirp = NULL;
    }

    return dirp;
}

/*
 * Read next directory entry.
 */
static struct dirent*
readdir(
    DIR *dirp)
{
    struct dirent *entry;

    /*
     * Read directory entry to buffer.  We can safely ignore the return value
     * as entry will be set to NULL in case of error.
     */
    (void) readdir_r (dirp, &dirp->ent, &entry);

    /* Return pointer to statically allocated directory entry */
    return entry;
}

/*
 * Read next directory entry into called-allocated buffer.
 *
 * Returns zero on success.  If the end of directory stream is reached, then
 * sets result to NULL and returns zero.
 */
static int
readdir_r(
    DIR *dirp,
    struct dirent *entry,
    struct dirent **result)
{
    WIN32_FIND_DATAW *datap;

    /* Read next directory entry */
    datap = dirent_next (dirp->wdirp);
    if (datap) {
        size_t n;
        int error;

        /* Attempt to convert file name to multi-byte string */
        error = dirent_wcstombs_s(
            &n, entry->d_name, PATH_MAX + 1, datap->cFileName, PATH_MAX + 1);

        /*
         * If the file name cannot be represented by a multi-byte string,
         * then attempt to use old 8+3 file name.  This allows traditional
         * Unix-code to access some file names despite of unicode
         * characters, although file names may seem unfamiliar to the user.
         *
         * Be ware that the code below cannot come up with a short file
         * name unless the file system provides one.  At least
         * VirtualBox shared folders fail to do this.
         */
        if (error  &&  datap->cAlternateFileName[0] != '\0') {
            error = dirent_wcstombs_s(
                &n, entry->d_name, PATH_MAX + 1,
                datap->cAlternateFileName, PATH_MAX + 1);
        }

        if (!error) {
            DWORD attr;

            /* Length of file name excluding zero terminator */
            entry->d_namlen = n - 1;

            /* File attributes */
            attr = datap->dwFileAttributes;
            if ((attr & FILE_ATTRIBUTE_DEVICE) != 0) {
                entry->d_type = DT_CHR;
            } else if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                entry->d_type = DT_DIR;
            } else {
                entry->d_type = DT_REG;
            }

            /* Reset dummy fields */
            entry->d_ino = 0;
            entry->d_off = 0;
            entry->d_reclen = sizeof (struct dirent);

        } else {

            /*
             * Cannot convert file name to multi-byte string so construct
             * an erroneous directory entry and return that.  Note that
             * we cannot return NULL as that would stop the processing
             * of directory entries completely.
             */
            entry->d_name[0] = '?';
            entry->d_name[1] = '\0';
            entry->d_namlen = 1;
            entry->d_type = DT_UNKNOWN;
            entry->d_ino = 0;
            entry->d_off = -1;
            entry->d_reclen = 0;

        }

        /* Return pointer to directory entry */
        *result = entry;

    } else {

        /* No more directory entries */
        *result = NULL;

    }

    return /*OK*/0;
}

/*
 * Close directory stream.
 */
static int
closedir(
    DIR *dirp)
{
    int ok;
    if (dirp) {

        /* Close wide-character directory stream */
        ok = _wclosedir (dirp->wdirp);
        dirp->wdirp = NULL;

        /* Release multi-byte character version */
        free (dirp);

    } else {

        /* Invalid directory stream */
        dirent_set_errno (EBADF);
        ok = /*failure*/-1;

    }
    return ok;
}

/*
 * Rewind directory stream to beginning.
 */
static void
rewinddir(
    DIR* dirp)
{
    /* Rewind wide-character string directory stream */
    _wrewinddir (dirp->wdirp);
}

/*
 * Scan directory for entries.
 */
static int
scandir(
    const char *dirname,
    struct dirent ***namelist,
    int (*filter)(const struct dirent*),
    int (*compare)(const struct dirent**, const struct dirent**))
{
    struct dirent **files = NULL;
    size_t size = 0;
    size_t allocated = 0;
    const size_t init_size = 1;
    DIR *dir = NULL;
    struct dirent *entry;
    struct dirent *tmp = NULL;
    size_t i;
    int result = 0;

    /* Open directory stream */
    dir = opendir (dirname);
    if (dir) {

        /* Read directory entries to memory */
        while (1) {

            /* Enlarge pointer table to make room for another pointer */
            if (size >= allocated) {
                void *p;
                size_t num_entries;

                /* Compute number of entries in the enlarged pointer table */
                if (size < init_size) {
                    /* Allocate initial pointer table */
                    num_entries = init_size;
                } else {
                    /* Double the size */
                    num_entries = size * 2;
                }

                /* Allocate first pointer table or enlarge existing table */
                p = realloc (files, sizeof (void*) * num_entries);
                if (p != NULL) {
                    /* Got the memory */
                    files = (dirent**) p;
                    allocated = num_entries;
                } else {
                    /* Out of memory */
                    result = -1;
                    break;
                }

            }

            /* Allocate room for temporary directory entry */
            if (tmp == NULL) {
                tmp = (struct dirent*) malloc (sizeof (struct dirent));
                if (tmp == NULL) {
                    /* Cannot allocate temporary directory entry */
                    result = -1;
                    break;
                }
            }

            /* Read directory entry to temporary area */
            if (readdir_r (dir, tmp, &entry) == /*OK*/0) {

                /* Did we get an entry? */
                if (entry != NULL) {
                    int pass;

                    /* Determine whether to include the entry in result */
                    if (filter) {
                        /* Let the filter function decide */
                        pass = filter (tmp);
                    } else {
                        /* No filter function, include everything */
                        pass = 1;
                    }

                    if (pass) {
                        /* Store the temporary entry to pointer table */
                        files[size++] = tmp;
                        tmp = NULL;

                        /* Keep up with the number of files */
                        result++;
                    }

                } else {

                    /*
                     * End of directory stream reached => sort entries and
                     * exit.
                     */
                    qsort (files, size, sizeof (void*),
                        (int (*) (const void*, const void*)) compare);
                    break;

                }

            } else {
                /* Error reading directory entry */
                result = /*Error*/ -1;
                break;
            }

        }

    } else {
        /* Cannot open directory */
        result = /*Error*/ -1;
    }

    /* Release temporary directory entry */
    if (tmp) {
        free (tmp);
    }

    /* Release allocated memory on error */
    if (result < 0) {
        for (i = 0; i < size; i++) {
            free (files[i]);
        }
        free (files);
        files = NULL;
    }

    /* Close directory stream */
    if (dir) {
        closedir (dir);
    }

    /* Pass pointer table to caller */
    if (namelist) {
        *namelist = files;
    }
    return result;
}

/* Alphabetical sorting */
static int
alphasort(
    const struct dirent **a, const struct dirent **b)
{
    return strcoll ((*a)->d_name, (*b)->d_name);
}

/* Sort versions */
static int
versionsort(
    const struct dirent **a, const struct dirent **b)
{
    /* FIXME: implement strverscmp and use that */
    return alphasort (a, b);
}


/* Convert multi-byte string to wide character string */
static int
dirent_mbstowcs_s(
    size_t *pReturnValue,
    wchar_t *wcstr,
    size_t sizeInWords,
    const char *mbstr,
    size_t count)
{
    int error;

#if defined(_MSC_VER)  &&  _MSC_VER >= 1400

    /* Microsoft Visual Studio 2005 or later */
    error = mbstowcs_s (pReturnValue, wcstr, sizeInWords, mbstr, count);

#else

    /* Older Visual Studio or non-Microsoft compiler */
    size_t n;

    /* Convert to wide-character string (or count characters) */
    n = mbstowcs (wcstr, mbstr, sizeInWords);
    if (!wcstr  ||  n < count) {

        /* Zero-terminate output buffer */
        if (wcstr  &&  sizeInWords) {
            if (n >= sizeInWords) {
                n = sizeInWords - 1;
            }
            wcstr[n] = 0;
        }

        /* Length of resulting multi-byte string WITH zero terminator */
        if (pReturnValue) {
            *pReturnValue = n + 1;
        }

        /* Success */
        error = 0;

    } else {

        /* Could not convert string */
        error = 1;

    }

#endif

    return error;
}

/* Convert wide-character string to multi-byte string */
static int
dirent_wcstombs_s(
    size_t *pReturnValue,
    char *mbstr,
    size_t sizeInBytes, /* max size of mbstr */
    const wchar_t *wcstr,
    size_t count)
{
    int error;

#if defined(_MSC_VER)  &&  _MSC_VER >= 1400

    /* Microsoft Visual Studio 2005 or later */
    error = wcstombs_s (pReturnValue, mbstr, sizeInBytes, wcstr, count);

#else

    /* Older Visual Studio or non-Microsoft compiler */
    size_t n;

    /* Convert to multi-byte string (or count the number of bytes needed) */
    n = wcstombs (mbstr, wcstr, sizeInBytes);
    if (!mbstr  ||  n < count) {

        /* Zero-terminate output buffer */
        if (mbstr  &&  sizeInBytes) {
            if (n >= sizeInBytes) {
                n = sizeInBytes - 1;
            }
            mbstr[n] = '\0';
        }

        /* Length of resulting multi-bytes string WITH zero-terminator */
        if (pReturnValue) {
            *pReturnValue = n + 1;
        }

        /* Success */
        error = 0;

    } else {

        /* Cannot convert string */
        error = 1;

    }

#endif

    return error;
}

/* Set errno variable */
static void
dirent_set_errno(
    int error)
{
#if defined(_MSC_VER)  &&  _MSC_VER >= 1400

    /* Microsoft Visual Studio 2005 and later */
    _set_errno (error);

#else

    /* Non-Microsoft compiler or older Microsoft compiler */
    errno = error;

#endif
}


#ifdef __cplusplus
}
#endif
#endif /*DIRENT_H*/

