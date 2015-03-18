/* -*- mode: c; indent-width: 4; -*- */
/*
 * win32 system io functionality
 *
 *      stat, lstat, fstat, readlink, symlink, open     
 *
 * Copyright (c) 2007, 2012 - 2015 Adam Young.
 *
 * This file is part of the Midnight Commander.
 *
 * The Midnight Commander is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * The Midnight Commander is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Notice: Portions of this text are reprinted and reproduced in electronic form. from
 * IEEE Portable Operating System Interface (POSIX), for reference only. Copyright (C)
 * 2001-2003 by the Institute of. Electrical and Electronics Engineers, Inc and The Open
 * Group. Copyright remains with the authors and the original Standard can be obtained 
 * online at http://www.opengroup.org/unix/online.html.
 * ==end==
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT        0x0501              /* enable xp+ features */
#endif

#include "win32_internal.h"
#include "win32_misc.h"
#include "win32_io.h"

#include <winioctl.h>                           /* DeviceIoControls */
#if defined(HAVE_NTIFS_H)
#include <ntifs.h>
#endif
#if defined(__WATCOMC__)
#include <shellapi.h>                           /* SHSTDAPI */
#endif
#include <shlobj.h>                             /* shell interface */
#define PSAPI_VERSION       1                   /* GetMappedFileName and psapi.dll */
#include <psapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#include <unistd.h>


static BOOL                 xGetFinalPathNameByHandle(HANDLE handle, char *name, int length);

static void                 ApplyAttributes(struct stat *sb, const DWORD dwAttr, const char *name);
static void                 ApplyTimes(struct stat *sb, const FILETIME *ftCreationTime,
                                            const FILETIME *ftLastAccessTime, const FILETIME *ftLastWriteTime);
static void                 ApplySize(struct stat *sb, const DWORD nFileSizeLow, const DWORD nFileSizeHigh);
static time_t               ConvertTime(const FILETIME *ft);

static BOOL                 IsExec(const char *name);
static BOOL                 IsExtension(const char *name, const char *ext);

static int                  Readlink(const char *path, const char **suffixes, char *buf, int maxlen);
static int                  ReadShortcut(const char *name, char *buf, int maxlen);
static int                  CreateShortcut(const char *link, const char *name,
                                    const char *working, const char *desc);
static int                  ReadReparse(const char *name, char *buf, int maxlen);
static int                  Stat(const char *name, struct stat *sb);

static const char *         suffixes_null[] = {
    "", NULL
    };
static const char *         suffixes_default[] = {
    "", ".lnk", NULL
    };


/*
//  NAME
//      stat - get file status
//
//  SYNOPSIS
//      #include <sys/stat.h>
//
//      int stat(const char *restrict path, struct stat *restrict buf);
//
//  DESCRIPTION
//
//      The stat() function shall obtain information about the named file and write it to
//      the area pointed to by the buf argument. The path argument points to a pathname
//      naming a file. Read, write, or execute permission of the named file is not
//      required. An implementation that provides additional or alternate file access
//      control mechanisms may, under implementation-defined conditions, cause stat() to
//      fail. In particular, the system may deny the existence of the file specified by path.
//
//      If the named file is a symbolic link, the stat() function shall continue pathname
//      resolution using the contents of the symbolic link, and shall return information
//      pertaining to the resulting file if the file exists.
//
//      The buf argument is a pointer to a stat structure, as defined in the <sys/stat.h>
//      header, into which information is placed concerning the file.
//
//      The stat() function shall update any time-related fields (as described in the Base
//      Definitions volume of IEEE Std 1003.1-2001, Section 4.7, File Times Update), before
//      writing into the stat structure.
//
//      Unless otherwise specified, the structure members st_mode, st_ino, st_dev, st_uid,
//      st_gid, st_atime, st_ctime, and st_mtime shall have meaningful values for all file
//      types defined in this volume of IEEE Std 1003.1-2001. The value of the member
//      st_nlink shall be set to the number of links to the file.
//
//  RETURN VALUE
//      Upon successful completion, 0 shall be returned. Otherwise, -1 shall be returned
//      and errno set to indicate the error.
//
//  ERRORS
//
//      The stat() function shall fail if:
//
//      [EACCES]
//          Search permission is denied for a component of the path prefix.
//
//      [EIO]
//          An error occurred while reading from the file system.
//
//      [ELOOP]
//          A loop exists in symbolic links encountered during resolution of the path
//          argument.
//
//      [ENAMETOOLONG]
//          The length of the path argument exceeds {PATH_MAX} or a pathname component is
//          longer than {NAME_MAX}.
//
//      [ENOENT]
//          A component of path does not name an existing file or path is an empty string.
//
//      [ENOTDIR]
//          A component of the path prefix is not a directory.
//
//      [EOVERFLOW]
//          The file size in bytes or the number of blocks allocated to the file or the
//          file serial number cannot be represented correctly in the structure pointed to
//          by buf.
//
//
//      The stat() function may fail if:
//
//      [ELOOP]
//          More than {SYMLOOP_MAX} symbolic links were encountered during resolution of
//          the path argument.
//
//      [ENAMETOOLONG]
//          As a result of encountering a symbolic link in resolution of the path argument,
//          the length of the substituted pathname string exceeded {PATH_MAX}.
//
//      [EOVERFLOW]
//          A value to be stored would overflow one of the members of the stat structure.
*/
int
w32_stat(const char *path, struct stat *sb)
{
    char symbuf[WIN32_PATH_MAX];
    int ret = 0;

    if (NULL == path || NULL == sb) {
        ret = -EFAULT;

    } else {
        memset(sb, 0, sizeof(struct stat));
        if ((ret = Readlink(path, (void *)-1, symbuf, sizeof(symbuf))) > 0) {
            path = symbuf;
        }
    }
    if (ret < 0 || (ret = Stat(path, sb)) < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}



/*
//  NAME
//      lstat - get symbolic link status
//
//  SYNOPSIS
//
//      #include <sys/stat.h>
//
//      int lstat(const char *restrict path, struct stat *restrict buf);
//
//  DESCRIPTION
//      The lstat() function shall be equivalent to stat(), except when path refers to a
//      symbolic link. In that case lstat() shall return information about the link, while
//      stat() shall return information about the file the link references.
//
//      For symbolic links, the st_mode member shall contain meaningful information when
//      used with the file type macros, and the st_size member shall contain the length of
//      the pathname contained in the symbolic link. File mode bits and the contents of the
//      remaining members of the stat structure are unspecified. The value returned in the
//      st_size member is the length of the contents of the symbolic link, and does not
//      count any trailing null.
//
//  RETURN VALUE
//      Upon successful completion, lstat() shall return 0. Otherwise, it shall return -1
//      and set errno to indicate the error.
//
//  ERRORS
//
//      The lstat() function shall fail if:
//
//      [EACCES]
//          A component of the path prefix denies search permission.
//
//      [EIO]
//          An error occurred while reading from the file system.
//
//      [ELOOP]
//          A loop exists in symbolic links encountered during resolution of the path
//          argument.
//
//      [ENAMETOOLONG]
//          The length of a pathname exceeds {PATH_MAX} or a pathname component is longer
//          than {NAME_MAX}.
//
//      [ENOTDIR]
//          A component of the path prefix is not a directory.
//
//      [ENOENT]
//          A component of path does not name an existing file or path is an empty string.
//
//      [EOVERFLOW]
//          The file size in bytes or the number of blocks allocated to the file or the
//          file serial number cannot be represented correctly in the structure pointed to
//          by buf.
//
//      The lstat() function may fail if:
//
//      [ELOOP]
//          More than {SYMLOOP_MAX} symbolic links were encountered during resolution of
//          the path argument.
//
//      [ENAMETOOLONG]
//          As a result of encountering a symbolic link in resolution of the path argument,
//          the length of the substituted pathname string exceeded {PATH_MAX}.
//
//      [EOVERFLOW]
//          One of the members is too large to store into the structure pointed to by the
//          buf argument.
*/
int
w32_lstat(const char *path, struct stat *sb)
{
    int ret = 0;

    if (path == NULL || sb == NULL) {
        ret = -EFAULT;
    }

    if (ret < 0 || (ret = Stat(path, sb)) < 0) {
        errno = -ret;
        return (-1);
    }
    return (0);
}


/*
//  NAME
//      fstat - get file status
//
//  SYNOPSIS
//      #include <sys/stat.h>
//
//      int fstat(int fildes, struct stat *buf);
//
//  DESCRIPTION
//      The fstat() function shall obtain information about an open file associated with
//      the file descriptor fildes, and shall write it to the area pointed to by buf.
//
//      If fildes references a shared memory object, the implementation shall update in the
//      stat structure pointed to by the buf argument only the st_uid, st_gid, st_size, and
//      st_mode fields, and only the S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH, and
//      S_IWOTH file permission bits need be valid. The implementation may update other
//      fields and flags. [Optional]
//
//      If fildes references a typed memory object, the implementation shall update in the
//      stat structure pointed to by the buf argument only the st_uid, st_gid, st_size, and
//      st_mode fields, and only the S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH, and
//      S_IWOTH file permission bits need be valid. The implementation may update other
//      fields and flags. [Optional]
//
//      The buf argument is a pointer to a stat structure, as defined in <sys/stat.h>, into
//      which information is placed concerning the file.
//
//      The structure members st_mode, st_ino, st_dev, st_uid, st_gid, st_atime, st_ctime,
//      and st_mtime shall have meaningful values for all other file types defined in this
//      volume of IEEE Std 1003.1-2001. The value of the member st_nlink shall be set to
//      the number of links to the file.
//
//      An implementation that provides additional or alternative file access control
//      mechanisms may, under implementation-defined conditions, cause fstat() to fail.
//
//      The fstat() function shall update any time-related fields as described in the Base
//      Definitions volume of IEEE Std 1003.1-2001, Section 4.7, File Times Update, before
//      writing into the stat structure.
//
//  RETURN VALUE
//      Upon successful completion, 0 shall be returned. Otherwise, -1 shall be returned
//      and errno set to indicate the error.
//
//  ERRORS
//      The fstat() function shall fail if:
//
//      [EBADF]
//          The fildes argument is not a valid file descriptor.
//
//      [EIO]
//          An I/O error occurred while reading from the file system.
//
//      [EOVERFLOW]
//          The file size in bytes or the number of blocks allocated to the file or the
//          file serial number cannot be represented correctly in the structure pointed to
//          by buf.
//
//      The fstat() function may fail if:
//
//      [EOVERFLOW]
//          One of the values is too large to store into the structure pointed to by
//          the buf argument.
*/
int
w32_fstat(int fd, struct stat *sb)
{
    HANDLE handle;
    int ret = 0;

    if (NULL == sb) {
        ret = -EFAULT;

    } else {
        memset(sb, 0, sizeof(struct stat));

        if (fd < 0) {
            ret = -EBADF;

        } else if ((handle = ((HANDLE) _get_osfhandle(fd))) == INVALID_HANDLE_VALUE) {
                                                // socket
            if (fd > WIN32_FILDES_MAX && FILE_TYPE_PIPE == GetFileType((HANDLE) fd)) {
                sb->st_mode |= S_IRUSR | S_IRGRP | S_IROTH;
                sb->st_mode |= S_IFIFO;
                sb->st_dev = sb->st_rdev = 1;

            } else {
                ret = -EBADF;
            }

        } else {
            const DWORD ftype = GetFileType(handle);

            switch (ftype) {
            case FILE_TYPE_DISK: {      // disk file.
                    BY_HANDLE_FILE_INFORMATION fi = {0};

                    if (GetFileInformationByHandle(handle, &fi)) {
                        char t_name[MAX_PATH], *name = NULL;

                        if (xGetFinalPathNameByHandle(handle, t_name, sizeof(t_name))) {
                            name = t_name;      // resolved filename.
                        }
                        ApplyAttributes(sb, fi.dwFileAttributes, name);
                        ApplyTimes(sb, &fi.ftCreationTime, &fi.ftLastAccessTime, &fi.ftLastWriteTime);
                        ApplySize(sb, fi.nFileSizeLow, fi.nFileSizeHigh);
                        if (fi.nNumberOfLinks > 0) {
                            sb->st_nlink = (int)fi.nNumberOfLinks;
                        }
                    }
                }
                break;

            case FILE_TYPE_CHAR:        // character file, typically an LPT device or a console.
            case FILE_TYPE_PIPE:        // socket, a named pipe, or an anonymous pipe.
                sb->st_mode |= S_IRUSR | S_IRGRP | S_IROTH;
                if (FILE_TYPE_PIPE == ftype) {
                    sb->st_mode |= S_IFIFO;
                } else {
                    sb->st_mode |= S_IFCHR;
                }
                sb->st_dev = sb->st_rdev = 1;
                break;

            case FILE_TYPE_REMOTE:      
            case FILE_TYPE_UNKNOWN:             // others
            default:
                ret = -EBADF;
                break;
            }
        }
    }

    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}


typedef DWORD (WINAPI *GetFinalPathNameByHandleA_t)(
                    HANDLE hFile, char *lpszFilePath, DWORD length, DWORD dwFlags);


static DWORD WINAPI
my_GetFinalPathNameByHandleA(HANDLE handle, char *path, DWORD length, DWORD flags)
{                                               // Determine underlying file-name, XP+
    HANDLE map;
    DWORD ret;

    if (0 == GetFileSize(handle, &ret) && 0 == ret) {
        return 0;                               // Cannot map a file with a length of zero
    }

    ret = 0;

    if (0 != (map = CreateFileMapping(handle, NULL, PAGE_READONLY, 0, 1, NULL))) {
        LPVOID pmem = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 1);
        
        if (pmem) {                             // XP+
            if (GetMappedFileNameA(GetCurrentProcess(), pmem, path, length)) {
                //
                //  Translate path with device name to drive letters, for example:
                //
                //      \Device\Volume4\<path>
                //
                //      => F:\<path>
                //
                char t_drives[512] = {0};       // 27*4 ...
                
                if (GetLogicalDriveStringsA(sizeof(t_drives) - 1, t_drives)) {
                    
                    BOOL found = FALSE;
                    const char *p = t_drives;
                    char t_name[MAX_PATH];
                    char t_drive[3] = " :";

                    do {                        // Look up each device name
                        t_drive[0] = *p;

                        if (QueryDosDevice(t_drive, t_name, sizeof(t_name) - 1)) {
                            const size_t namelen = strlen(t_name);

                            if (namelen < MAX_PATH) {
                                found = (0 == _strnicmp(path, t_name, namelen) && 
                                                path[namelen] == '\\');

                                if (found) {
                                    // 
                                    //  Reconstruct path, replacing device path with DOS path
                                    //
                                    char t_path[MAX_PATH];
                                    size_t len;

                                    len = _snprintf(t_path, sizeof(t_path), "%s%s", t_drive, path + namelen);
                                    t_path[sizeof(t_path) - 1] = 0;
                                    memcpy(path, (const char *)t_path, (len < length ? len + 1 : length));
                                    path[length - 1] = 0;
                                    ret = 1;
                                }
                            }
                        } 

                        while (*p++);           // Go to the next NULL character.
                    
                    } while (!found && *p);     // end of string
                }
                ret = 1;
            }
            UnmapViewOfFile(pmem);
        }
        CloseHandle(map);
    }
    return ret;
}


static BOOL
xGetFinalPathNameByHandle(HANDLE handle, char *path, int length)
{
    static GetFinalPathNameByHandleA_t xGetFinalPathNameByHandleA = NULL;

    if (NULL == xGetFinalPathNameByHandleA) {
        HINSTANCE hinst;                        // Vista+

        if (0 == (hinst = LoadLibrary("Kernel32")) ||
                0 == (xGetFinalPathNameByHandleA = 
                        (GetFinalPathNameByHandleA_t)GetProcAddress(hinst, "GetFinalPathNameByHandleA"))) {
                                                // XP+
            xGetFinalPathNameByHandleA = my_GetFinalPathNameByHandleA;
            FreeLibrary(hinst);
        }
    }

#ifndef FILE_NAME_NORMALIZED
#define FILE_NAME_NORMALIZED    0
#define VOLUME_NAME_DOS         0
#endif

    return xGetFinalPathNameByHandleA(handle, path, length, FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
}


/*
//  NAME
//      readlink - read the contents of a symbolic link
//
//  SYNOPSIS
//
//      #include <unistd.h>
//
//      ssize_t readlink(const char *restrict path, char *restrict buf, size_t bufsize);
//
//  DESCRIPTION
//      The readlink() function shall place the contents of the symbolic link referred to
//      by path in the buffer buf which has size bufsize. If the number of bytes in the
//      symbolic link is less than bufsize, the contents of the remainder of buf are
//      unspecified. If the buf argument is not large enough to contain the link content,
//      the first bufsize bytes shall be placed in buf.
//
//      If the value of bufsize is greater than {SSIZE_MAX}, the result is
//      implementation-defined.
//
//  RETURN VALUE
//      Upon successful completion, readlink() shall return the count of bytes placed in
//      the buffer. Otherwise, it shall return a value of -1, leave the buffer unchanged,
//      and set errno to indicate the error.
//
//  ERRORS
//
//      The readlink() function shall fail if:
//
//      [EACCES]
//          Search permission is denied for a component of the path prefix of path.
//
//      [EINVAL]
//          The path argument names a file that is not a symbolic link.
//
//      [EIO]
//          An I/O error occurred while reading from the file system.
//
//      [ELOOP]
//          A loop exists in symbolic links encountered during resolution of the path
//          argument.
//
//      [ENAMETOOLONG]
//          The length of the path argument exceeds {PATH_MAX} or a pathname component is
//          longer than {NAME_MAX}.
//
//      [ENOENT]
//          A component of path does not name an existing file or path is an empty string.
//
//      [ENOTDIR]
//          A component of the path prefix is not a directory.
//
//      The readlink() function may fail if:
//
//      [EACCES]
//          Read permission is denied for the directory.
//
//      [ELOOP]
//          More than {SYMLOOP_MAX} symbolic links were encountered during resolution of
//          the path argument.
//
//      [ENAMETOOLONG]
//          As a result of encountering a symbolic link in resolution of the path argument,
//          the length of the substituted pathname string exceeded {PATH_MAX}.
//
//
//  NOTES:
//      Portable applications should not assume that the returned contents of
//      the symblic link are null-terminated.
*/
int
w32_readlink(const char *path, char *buf, int maxlen)
{
    int ret = 0;

    if (path == NULL || buf == NULL) {
        ret = -EFAULT;

    } else if (0 == (ret = Readlink(path, (void *)-1, buf, maxlen))) {
        ret = -EINVAL;                          /* not a symlink */
    }

    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}


/*
//  NAME
//      symlink - make a symbolic link to a file
//  
//  SYNOPSIS
//  
//      #include <unistd.h>
//  
//      int symlink(const char *path1, const char *path2);
//  
//  DESCRIPTION
//      The symlink() function shall create a symbolic link called path2 that contains the
//      string pointed to by path1 ( path2 is the name of the symbolic link created, path1
//      is the string contained in the symbolic link).
//  
//      The string pointed to by path1 shall be treated only as a character string and
//      shall not be validated as a pathname.
//  
//      If the symlink() function fails for any reason other than [EIO], any file named by
//      path2 shall be unaffected.
//  
//  RETURN VALUE
//      Upon successful completion, symlink() shall return 0; otherwise, it shall return -1
//      and set errno to indicate the error.
//  
//  ERRORS
//      The symlink() function shall fail if:
//  
//      [EACCES]
//          Write permission is denied in the directory where the symbolic link is being
//          created, or search permission is denied for a component of the path prefix of
//          path2.
//  
//      [EEXIST]
//          The path2 argument names an existing file or symbolic link.
//  
//      [EIO]
//          An I/O error occurs while reading from or writing to the file system.
//  
//      [ELOOP]
//          A loop exists in symbolic links encountered during resolution of the path2
//          argument.
//  
//      [ENAMETOOLONG]
//          The length of the path2 argument exceeds {PATH_MAX} or a pathname component is
//          longer than {NAME_MAX} or the length of the path1 argument is longer than
//          {SYMLINK_MAX}.
//  
//      [ENOENT]
//          A component of path2 does not name an existing file or path2 is an empty string.
//  
//      [ENOSPC]
//          The directory in which the entry for the new symbolic link is being placed
//          cannot be extended because no space is left on the file system containing the
//          directory, or the new symbolic link cannot be created because no space is left
//          on the file system which shall contain the link, or the file system is out of
//          file-allocation resources.
//  
//      [ENOTDIR]
//          A component of the path prefix of path2 is not a directory.
//  
//      [EROFS]
//          The new symbolic link would reside on a read-only file system.
//  
//      The symlink() function may fail if:
//  
//      [ELOOP]
//          More than {SYMLOOP_MAX} symbolic links were encountered during resolution of
//          the path2 argument.
//  
//      [ENAMETOOLONG]
//          As a result of encountering a symbolic link in resolution of the path2 argument, 
//          the length of the substituted pathname string exceeded {PATH_MAX} bytes
//          (including the terminating null byte), or the length of the string pointed to
//          by path1 exceeded {SYMLINK_MAX}.
*/
int
w32_symlink(const char *name1, const char *name2)
{
    int ret = -1;

    if (name1 == NULL || name2 == NULL)
        errno = ENAMETOOLONG;

    else if (strlen(name1) > MAX_PATH || strlen(name2) > MAX_PATH)
        errno = ENAMETOOLONG;

    else if (GetFileAttributes(name2) != 0xffffffff)
        errno = EEXIST; /*EACCES*/

    else if (CreateShortcut(name2, name1, "", name1) == FALSE)
        errno = EIO;

    else {
        ret = 0;
    }

    return ret;
}


/*
//  NAME
//      open - open a file
//  
//  SYNOPSIS
//  
//      #include <sys/stat.h>
//      #include <fcntl.h>
//  
//      int open(const char *path, int oflag, ... );
//  
//  DESCRIPTION
//      The open() function shall establish the connection between a file and a file
//      descriptor. It shall create an open file description that refers to a file and a
//      file descriptor that refers to that open file description. The file descriptor is
//      used by other I/O functions to refer to that file. The path argument points to a
//      pathname naming the file.
//  
//      The open() function shall return a file descriptor for the named file that is the
//      lowest file descriptor not currently open for that process. The open file
//      description is new, and therefore the file descriptor shall not share it with any
//      other process in the system. The FD_CLOEXEC file descriptor flag associated with
//      the new file descriptor shall be cleared.
//  
//      The file offset used to mark the current position within the file shall be set to
//      the beginning of the file.
//  
//      The file status flags and file access modes of the open file description shall be
//      set according to the value of oflag.
//  
//      Values for oflag are constructed by a bitwise-inclusive OR of flags from the
//      following list, defined in <fcntl.h>. Applications shall specify exactly one of the
//      first three values (file access modes) below in the value of oflag:
//  
//          O_RDONLY        
//              Open for reading only.
//  
//          O_WRONLY        
//              Open for writing only.
//  
//          O_RDWR          
//              Open for reading and writing. The result is undefined if this flag is
//              applied to a FIFO.
//  
//      Any combination of the following may be used:
//  
//          O_APPEND
//              If set, the file offset shall be set to the end of the file prior to each
//              write.
//  
//          O_CREAT
//              If the file exists, this flag has no effect except as noted under O_EXCL
//              below. Otherwise, the file shall be created; the user ID of the file shall
//              be set to the effective user ID of the process; the group ID of the file
//              shall be set to the group ID of the file's parent directory or to the
//              effective group ID of the process; and the access permission bits (see
//              <sys/stat.h>) of the file mode shall be set to the value of the third
//              argument taken as type mode_t modified as follows: a bitwise AND is
//              performed on the file-mode bits and the corresponding bits in the
//              complement of the process' file mode creation mask. Thus, all bits in the
//              file mode whose corresponding bit in the file mode creation mask is set are
//              cleared. When bits other than the file permission bits are set, the effect
//              is unspecified. The third argument does not affect whether the file is open
//              for reading, writing, or for both. Implementations shall provide a way to
//              initialize the file's group ID to the group ID of the parent directory.
//              Implementations may, but need not, provide an implementation-defined way to
//              initialize the file's group ID to the effective group ID of the calling
//              process.
//  
//          O_DSYNC
//              Write I/O operations on the file descriptor shall complete as defined by
//              synchronized I/O data integrity completion. [Option End]
//  
//          O_EXCL
//              If O_CREAT and O_EXCL are set, open() shall fail if the file exists. The
//              check for the existence of the file and the creation of the file if it does
//              not exist shall be atomic with respect to other threads executing open()
//              naming the same filename in the same directory with O_EXCL and O_CREAT set.
//              If O_EXCL and O_CREAT are set, and path names a symbolic link, open() shall
//              fail and set errno to [EEXIST], regardless of the contents of the symbolic
//              link. If O_EXCL is set and O_CREAT is not set, the result is undefined.
//  
//          O_NOCTTY
//              If set and path identifies a terminal device, open() shall not cause the
//              terminal device to become the controlling terminal for the process.
//  
//          O_NONBLOCK
//              When opening a FIFO with O_RDONLY or O_WRONLY set:
//  
//                  If O_NONBLOCK is set, an open() for reading-only shall return without
//                  delay. An open() for writing-only shall return an error if no process
//                  currently has the file open for reading.
//  
//                  If O_NONBLOCK is clear, an open() for reading-only shall block the
//                  calling thread until a thread opens the file for writing. An open() for
//                  writing-only shall block the calling thread until a thread opens the
//                  file for reading.
//  
//              When opening a block special or character special file that supports
//              non-blocking opens:
//  
//                  If O_NONBLOCK is set, the open() function shall return without blocking
//                  for the device to be ready or available. Subsequent behavior of the
//                  device is device-specific.
//  
//                  If O_NONBLOCK is clear, the open() function shall block the calling
//                  thread until the device is ready or available before returning.
//  
//              Otherwise, the behavior of O_NONBLOCK is unspecified.
//  
//          O_RSYNC
//              Read I/O operations on the file descriptor shall complete at the same level
//              of integrity as specified by the O_DSYNC and O_SYNC flags. If both O_DSYNC
//              and O_RSYNC are set in oflag, all I/O operations on the file descriptor
//              shall complete as defined by synchronized I/O data integrity completion. If
//              both O_SYNC and O_RSYNC are set in flags, all I/O operations on the file
//              descriptor shall complete as defined by synchronized I/O file integrity
//              completion. [Option End]
//  
//          O_SYNC
//              Write I/O operations on the file descriptor shall complete as defined by
//              synchronized I/O file integrity completion. [Option End]
//  
//          O_TRUNC
//              If the file exists and is a regular file, and the file is successfully
//              opened O_RDWR or O_WRONLY, its length shall be truncated to 0, and the
//              mode and owner shall be unchanged. It shall have no effect on FIFO
//              special files or terminal device files. Its effect on other file types
//              is implementation-defined. The result of using O_TRUNC with O_RDONLY is
//              undefined.
//
//      If O_CREAT is set and the file did not previously exist, upon successful completion, 
//      open() shall mark for update the st_atime, st_ctime, and st_mtime fields of the
//      file and the st_ctime and st_mtime fields of the parent directory.
//  
//      If O_TRUNC is set and the file did previously exist, upon successful completion, 
//      open() shall mark for update the st_ctime and st_mtime fields of the file.
//  
//      If both the O_SYNC and O_DSYNC flags are set, the effect is as if only the O_SYNC
//      flag was set. [Option End]
//  
//      If path refers to a STREAMS file, oflag may be constructed from O_NONBLOCK OR'ed
//      with either O_RDONLY, O_WRONLY, or O_RDWR. Other flag values are not applicable to
//      STREAMS devices and shall have no effect on them. The value O_NONBLOCK affects the
//      operation of STREAMS drivers and certain functions applied to file descriptors
//      associated with STREAMS files. For STREAMS drivers, the implementation of
//      O_NONBLOCK is device-specific. [Option End]
//  
//      If path names the master side of a pseudo-terminal device, then it is unspecified
//      whether open() locks the slave side so that it cannot be opened. Conforming
//      applications shall call unlockpt() before opening the slave side. [Option End]
//  
//      The largest value that can be represented correctly in an object of type off_t
//      shall be established as the offset maximum in the open file description.
//  
//  RETURN VALUE
//  
//      Upon successful completion, the function shall open the file and return a
//      non-negative integer representing the lowest numbered unused file descriptor.
//      Otherwise, -1 shall be returned and errno set to indicate the error. No files shall
//      be created or modified if the function returns -1.
//
//ERRORS
//
//    The open() function shall fail if:
//
//    [EACCES]
//        Search permission is denied on a component of the path prefix, or the file
//        exists and the permissions specified by oflag are denied, or the file does not
//        exist and write permission is denied for the parent directory of the file to be
//        created, or O_TRUNC is specified and write permission is denied.
//
//    [EEXIST]
//        O_CREAT and O_EXCL are set, and the named file exists.
//
//    [EINTR]
//        A signal was caught during open().
//
//    [EINVAL]
//        The implementation does not support synchronized I/O for this file. [Option End]
//
//    [EIO]
//        The path argument names a STREAMS file and a hangup or error occurred during
//        the open(). [Option End]
//
//    [EISDIR]
//        The named file is a directory and oflag includes O_WRONLY or O_RDWR.
//
//    [ELOOP]
//        A loop exists in symbolic links encountered during resolution of the path argument.
//
//    [EMFILE]
//        {OPEN_MAX} file descriptors are currently open in the calling process.
//
//    [ENAMETOOLONG]
//        The length of the path argument exceeds {PATH_MAX} or a pathname component is
//        longer than {NAME_MAX}.
//
//    [ENFILE]
//        The maximum allowable number of files is currently open in the system.
//
//    [ENOENT]
//        O_CREAT is not set and the named file does not exist; or O_CREAT is set and
//        either the path prefix does not exist or the path argument points to an empty
//        string.
//
//    [ENOSR]
//        [XSR] [Option Start] The path argument names a STREAMS-based file and the
//        system is unable to allocate a STREAM. [Option End]
//
//    [ENOSPC]
//        The directory or file system that would contain the new file cannot be expanded, 
//        the file does not exist, and O_CREAT is specified.
//
//    [ENOTDIR]
//        A component of the path prefix is not a directory.
//
//    [ENXIO]
//        O_NONBLOCK is set, the named file is a FIFO, O_WRONLY is set, and no process
//        has the file open for reading.
//
//    [ENXIO]
//        The named file is a character special or block special file, and the device
//        associated with this special file does not exist.
//
//    [EOVERFLOW]
//        The named file is a regular file and the size of the file cannot be represented
//        correctly in an object of type off_t.
//
//    [EROFS]
//        The named file resides on a read-only file system and either O_WRONLY, O_RDWR, 
//        O_CREAT (if the file does not exist), or O_TRUNC is set in the oflag argument.
//
//    The open() function may fail if:
//
//    [EAGAIN]
//        The path argument names the slave side of a pseudo-terminal device that is
//        locked. [Option End]
//
//    [EINVAL]
//        The value of the oflag argument is not valid.
//
//    [ELOOP]
//        More than {SYMLOOP_MAX} symbolic links were encountered during resolution of
//        the path argument.
//
//    [ENAMETOOLONG]
//        As a result of encountering a symbolic link in resolution of the path argument, 
//        the length of the substituted pathname string exceeded {PATH_MAX}.
//
//    [ENOMEM]
//        The path argument names a STREAMS file and the system is unable to allocate
//        resources. [Option End]
//
//    [ETXTBSY]
//        The file is a pure procedure (shared text) file that is being executed and
//        oflag is O_WRONLY or O_RDWR.
//
*/
int
w32_open(const char *path, int oflag, ...)
{
    char symbuf[WIN32_PATH_MAX];
    int mode, ret = 0;

    if (O_CREAT & oflag) {
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, int);
        va_end(ap);
    } else {
        mode = 0;
    }

    if ((ret = Readlink(path, (void *)-1, symbuf, sizeof(symbuf))) < 0) {
        /*
         *  If O_CREAT create the file if it does not exist, in which case the
         *  file is created with mode mode as described in chmod(2) and modified
         *  by the process' umask value (see umask(2)).
         */
        if ((oflag & O_CREAT) && (ret == -ENOTDIR || ret == -ENOENT)) {
            ret = 0;
        }

    } else if (ret > 0) {
        /*
         *  If O_NOFOLLOW and pathname is a symbolic link, then the
         *  open fails with ELOOP.
         */
#if defined(O_NOFOLLOW) && (O_NOFOLLOW)         // extension
        if (oflag & O_NOFOLLOW) {
            ret = -ELOOP;
        }
#endif

        /*
         *  If O_EXCL is set and the last component of the pathname is a symbolic
         *  link, open() will fail even if the symbolic link points to a
         *  non-existent name.
         */
        if ((oflag & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL)) {
            oflag &= ~(O_CREAT|O_EXCL);         // link must exist !
        }
        path = symbuf;
    }

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return _open(path, oflag, mode);            // true open
}


/*
 *  Convert WIN attributes to thier Unix counterparts.
 */
static void
ApplyAttributes(struct stat *sb,
        const DWORD dwAttributes, const char *name)
{
    const char *p;
    char symbuf[WIN32_PATH_MAX];
    mode_t mode = 0;

    /*
     *  mode
     */

    /* S_IFxxx */
    if (NULL != (p = name) && p[0] && p[1] == ':') {
        p += 2;                                 /* remove drive */
    }

    if (p && (!p[0] || (ISSLASH(p[0]) && !p[1]))) {
        mode |= S_IFDIR|S_IEXEC;                /* handle root directory explicity */

    } else if (FILE_ATTRIBUTE_DIRECTORY & dwAttributes) {
//      if (FILE_ATTRIBUTE_REPARSE_POINT & dwAttributes) {
//          mode |= S_IFLNK;
//      }
        mode |= S_IFDIR|S_IEXEC;                /* directory */

    } else if ((FILE_ATTRIBUTE_REPARSE_POINT & dwAttributes) ||
                   (name && Readlink(name, NULL, symbuf, sizeof(symbuf)) > 0)) {
        mode |= S_IFLNK;                        /* symbolic link */

    } else {
        mode |= S_IFREG;                        /* normal file */
    }

    /* rw */
    mode |= (dwAttributes & FILE_ATTRIBUTE_READONLY) ?
                S_IREAD : (S_IREAD|S_IWRITE);

    /* x */
    if (name && IsExec(name)) {
        mode |= S_IEXEC;                        /* known exec type */
    }

    /* group/other */
    if (0 == (dwAttributes & FILE_ATTRIBUTE_SYSTEM)) {
        mode |= (mode & 0700) >> 3;             /* group */
        if (0 == (dwAttributes & FILE_ATTRIBUTE_HIDDEN)) {
            mode |= (mode & 0700) >> 6;         /* other */
        }
    }

    /*
     *  apply
     */
    sb->st_mode = mode;
    sb->st_nlink = 1;

    if (dwAttributes & FILE_ATTRIBUTE_SYSTEM) {
        sb->st_uid = sb->st_gid = 0;            /* root */
    } else {                                    /* current user */
        sb->st_uid = w32_getuid();
        sb->st_gid = w32_getgid();
    }
}


static void
ApplyTimes(struct stat *sb,
        const FILETIME *ftCreationTime, const FILETIME *ftLastAccessTime, const FILETIME *ftLastWriteTime)
{
    sb->st_mtime = ConvertTime(ftLastWriteTime);

    if (0 == (sb->st_atime = ConvertTime(ftLastAccessTime))) {
        sb->st_atime = sb->st_mtime;
    }

    if (0 == (sb->st_ctime = ConvertTime(ftCreationTime))) {
        sb->st_ctime = sb->st_mtime;
    }
}


/*  Function:       ConvertTime
 *      Convert a FILETIME structure into a UTC time.
 *
 *  Notes:
 *      Not all file systems can record creation and last access time and not all file
 *      systems record them in the same manner. For example, on Windows NT FAT, create
 *      time has a resolution of 10 milliseconds, write time has a resolution of 2
 *      seconds, and access time has a resolution of 1 day (really, the access date).
 *      On NTFS, access time has a resolution of 1 hour. Therefore, the GetFileTime
 *      function may not return the same file time information set using the
 *      SetFileTime function. Furthermore, FAT records times on disk in local time.
 *      However, NTFS records times on disk in UTC, so it is not affected by changes in
 *      time zone or daylight saving time.
 *
 */
static time_t
ConvertTime(const FILETIME *ft)
{
    SYSTEMTIME SystemTime;
    FILETIME LocalFTime;
    struct tm tm = {0};

    if (! ft->dwLowDateTime && !ft->dwHighDateTime)  {
        return 0;                               /* time unknown */
    }

    if (! FileTimeToLocalFileTime(ft, &LocalFTime) ||
            ! FileTimeToSystemTime(&LocalFTime, &SystemTime)) {
        return -1;
    }

    tm.tm_year = SystemTime.wYear - 1900;       /* Year (current year minus 1900) */
    tm.tm_mon  = SystemTime.wMonth - 1;         /* Month (0  11; January = 0) */
    tm.tm_mday = SystemTime.wDay;               /* Day of month (1  31) */
    tm.tm_hour = SystemTime.wHour;              /* Hours after midnight (0  23) */
    tm.tm_min  = SystemTime.wMinute;            /* Minutes after hour (0  59) */
    tm.tm_sec  = SystemTime.wSecond;            /* Seconds after minute (0  59) */
    tm.tm_isdst = -1;

    return mktime(&tm);
}



static void
ApplySize(struct stat *sb, const DWORD nFileSizeLow, const DWORD nFileSizeHigh)
{
    sb->st_size = nFileSizeLow;
/*TODO
 *  sb->st_size =
 *      (((__int64)nFileSizeHigh) << 32) + nFileSizeLow;
 */
}



/*
 *  Is the file an executFileType
 */
#define EXEC_ASSUME     \
    (sizeof(exec_assume)/sizeof(exec_assume[0]))

#define EXEC_EXCLUDE    \
    (sizeof(exec_exclude)/sizeof(exec_exclude[0]))

static const char *     exec_assume[]   = {
    ".exe", ".com", ".cmd", ".bat"
    };

static const char *     exec_exclude[]  = {
    ".o",   ".obj",                             /* objects */
    ".h",   ".hpp", ".inc",                     /* header files */
    ".c",   ".cc",  ".cpp", ".cs",              /* source files */
    ".a",   ".lib", ".dll",                     /* libraries */
                                                /* archives */
    ".zip", ".gz",  ".tar", ".tgz", ".bz2", ".rar",
    ".doc", ".txt",                             /* documents */
    ".hlp", ".chm",                             /* help */
    ".dat"                                      /* data files */
    };


static int
IsExec(const char *name)
{
    DWORD driveType;
    const char *dot;
    int idx = -1;

    if ((dot = strrchr(name, '.')) != NULL) {
        for (idx = EXEC_ASSUME-1; idx >= 0; idx--)
            if (WIN32_STRICMP(dot, exec_assume[idx]) == 0) {
                return TRUE;
            }

        for (idx = EXEC_EXCLUDE-1; idx >= 0; idx--)
            if (WIN32_STRICMP(dot, exec_exclude[idx]) == 0) {
                break;
            }
    }

    if (-1 == idx) {                            /* only local drives */
        if ((driveType = GetDriveType(name)) == DRIVE_FIXED) {
            DWORD binaryType = 0;

            if (GetBinaryType(name, &binaryType)) {
                /*
                switch(binaryType) {
                case SCS_32BIT_BINARY:  // 32-bit Windows-based application
                case SCS_64BIT_BINARY:  // 64-bit Windows-based application
                case SCS_DOS_BINARY:    // MS-DOS � based application
                case SCS_OS216_BINARY:  // 16-bit OS/2-based application
                case SCS_PIF_BINARY:    // PIF file that executes an MS-DOS based application
                case SCS_POSIX_BINARY:  // A POSIX based application
                case SCS_WOW_BINARY:    // A 16-bit Windows-based application
                }*/
                return TRUE;
            }
        }
    }

    return FALSE;
}


static BOOL
IsExtension(const char *name, const char *ext)
{
    const char *dot;

    if (ext && (dot = strrchr(name, '.')) != NULL &&
            WIN32_STRICMP(dot, ext) == 0) {
        return TRUE;
    }
    return FALSE;
}


static int
Readlink(const char *path, const char **suffixes, char *buf, int maxlen)
{
    DWORD attrs;
    const char *suffix;
    int length;
    int ret = -ENOENT;

    (void) strncpy( buf, path, maxlen );        // prime working buffer
    buf[ maxlen-1 ] = '\0';
    length = strlen(buf);

    if (suffixes == (void *)NULL) {
        suffixes = suffixes_null;
    } else if (suffixes == (void *)-1) {
        suffixes = suffixes_default;
    }

    while ((suffix = *suffixes++) != NULL) {
        /* Concat suffix */
        if (length + (int)strlen(suffix) >= maxlen) {
            ret = -ENAMETOOLONG;
            continue;
        }
        strcpy(buf + length, suffix);           // concat suffix

        /* File attributes */
        if ((attrs = GetFileAttributes(buf)) == 0xffffffff) {
            DWORD rc;

            if ((rc = GetLastError()) == ERROR_ACCESS_DENIED ||
                        rc == ERROR_SHARING_VIOLATION) {
                ret = -EACCES;                  // true error ???
            } else if (rc == ERROR_PATH_NOT_FOUND) {
                ret = -ENOTDIR;
            } else if (rc == ERROR_FILE_NOT_FOUND) {
                ret = -ENOENT;
            } else {
                ret = -EIO;
            }
            continue;                           // next suffix
        }

        /* Parse attributes */
        if ((attrs & (FILE_ATTRIBUTE_DIRECTORY)) ||
#ifdef FILE_ATTRIBUTE_COMPRESSED
                (attrs & (FILE_ATTRIBUTE_COMPRESSED)) ||
#endif
#ifdef FILE_ATTRIBUTE_DEVICE
                (attrs & (FILE_ATTRIBUTE_DEVICE)) ||
#endif
#ifdef FILE_ATTRIBUTE_ENCRYPTED
                (attrs & (FILE_ATTRIBUTE_ENCRYPTED))
#endif
            ) {
            ret = 0;                            // not/(cannot be) a symlink

        /* .. win32 readparse point */
        } else if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
            if ((ret = ReadReparse(path, buf, maxlen)) < 0) {
                ret = -EIO;
            } else {
                ret = strlen(buf);
            }

        /* .. win32 shortcut */
        } else if (attrs & (FILE_ATTRIBUTE_OFFLINE)) {
            ret = -EACCES;                      // wont be able to access

#define SHORTCUT_COOKIE         "L\0\0\0"       // shortcut magic

                                                // cygwin shortcut also syste/rdonly
#define CYGWIN_ATTRS            FILE_ATTRIBUTE_SYSTEM
#define CYGWIN_COOKIE           "!<symlink>"    // old style shortcut

        } else if ( IsExtension( buf, ".lnk" ) ||
                        (attrs & (FILE_ATTRIBUTE_HIDDEN|CYGWIN_ATTRS)) == CYGWIN_ATTRS ) {

            SECURITY_ATTRIBUTES sa;
            char cookie[sizeof(CYGWIN_COOKIE)-1];
            HANDLE fh;
            DWORD got;

            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = NULL;
            sa.bInheritHandle = FALSE;

            if ((fh = CreateFile (buf, GENERIC_READ, FILE_SHARE_READ,
                        &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) != INVALID_HANDLE_VALUE) {

                // read header
                if (! ReadFile(fh, cookie, sizeof (cookie), &got, 0)) {
                    ret = -EIO;

                // win32 shortcut (will also read cygwin shortcuts)
                } else if (got >= 4 && 0 == memcmp(cookie, SHORTCUT_COOKIE, 4)) {
                    if ((ret = ReadShortcut (buf, buf, maxlen)) < 0) {
                        ret = -EIO;
                    } else {
                        ret = strlen(buf);
                    }

                // cygwin symlink (old style)
                } else if ((attrs & CYGWIN_ATTRS) && got == sizeof(cookie) &&
                                0 == memcmp(cookie, CYGWIN_COOKIE, sizeof(cookie))) {

                    if (! ReadFile(fh, buf, maxlen, &got, 0)) {
                        ret = -EIO;
                    } else {
                        char *end;

                        if ((end = (char *)memchr(buf, 0, got)) != NULL) {
                            ret = end - buf;    // find the NUL terminator
                        } else {
                            ret = (int)got;
                        }
                        if (ret == 0) {
                            ret = -EIO;         // hmmm .. empty link specification
                        }
                    }
                } else {
                    ret = 0;                    // not a symlink
                }
                CloseHandle(fh);
            }
        } else {
            ret = 0;                            // not a symlink
        }
        break;
    }

    if (ret > 0) {
        w32_dos2unix(buf);
    }
    return ret;
}


/*  Function:       ReadShortcut
 *      Fills the filename and path buffer with relevant information.
 *
 *  Parameters:
 *      name -          name of the link file passed into the function.
 *
 *      path -          the buffer that receives the file's path name.
 *
 *      maxlen -        max length of the 'path' buffer.
 *
 *  Notes:
 *      The shortcuts used in Microsoft Windows 95 provide applications and users a way
 *      to create shortcuts or links to objects in the shell's namespace. The
 *      IShellLink OLE Interface can be used to obtain the path and filename from the
 *      shortcut, among other things.
 *
 *      A shortcut allows the user or an application to access an object from anywhere
 *      in the namespace. Shortcuts to objects are stored as binary files. These files
 *      contain information such as the path to the object, working directory, the path
 *      of the icon used to display the object, the description string, and so on.
 *
 *      Given a shortcut, applications can use the IShellLink interface and its
 *      functions to obtain all the pertinent information about that object. The
 *      IShellLink interface supports functions such as GetPath(), GetDescription(),
 *      Resolve(), GetWorkingDirectory(), and so on.
 */
static int
ReadShortcut(const char *name, char *buf, int maxlen)
{
    HRESULT hres = FALSE;
    WIN32_FIND_DATA wfd;
    IShellLink *pShLink;

    CoInitialize(NULL);

    hres = CoCreateInstance(&CLSID_ShellLink, NULL,
                CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *)&pShLink);

    if (SUCCEEDED(hres)) {
        WORD wsz[ MAX_PATH ];
        IPersistFile *ppf;

        hres = pShLink->lpVtbl->QueryInterface( pShLink, &IID_IPersistFile, (LPVOID *)&ppf );

        if (SUCCEEDED(hres)) {
            MultiByteToWideChar( CP_ACP, 0, name, -1, wsz, sizeof(wsz) );

            hres = ppf->lpVtbl->Load( ppf, wsz, STGM_READ );
            if (SUCCEEDED(hres)) {
/*              if (SUCCEEDED(hres)) {
 *                  hres = pShLink->lpVtbl->Resolve(
 *                              pShLink, 0, SLR_NOUPDATE | SLR_ANY_MATCH | SLR_NO_UI );
 *              }
 */

                hres = pShLink->lpVtbl->GetPath( pShLink, buf, maxlen, &wfd, 0 );
                if (!SUCCEEDED(hres) || buf[0] == '\0') {
                    /*
                    *  A document shortcut may only have a description ...
                    *  Also CYGWIN generates this style of link.
                    */
                    hres = pShLink->lpVtbl->GetDescription( pShLink, buf, maxlen );
                    if (buf[0] == '\0')
                        hres = !S_OK;
                }
                ppf->lpVtbl->Release( ppf );
            }
        }
        pShLink->lpVtbl->Release( pShLink );
    }

    CoUninitialize();

    return (SUCCEEDED(hres) ? 0 : -1);
}


static int
CreateShortcut(const char *link, const char *name, const char *working, const char *desc )
{
    IShellLink *pShLink;
    HRESULT hres;

    CoInitialize(NULL);

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance( &CLSID_ShellLink, NULL,
                CLSCTX_INPROC_SERVER, &IID_IShellLink, (PVOID *) &pShLink );

    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;
        WORD wsz[ MAX_PATH ];

        // Set the path to the shortcut target and add the
        // description.
        if (name)
            pShLink->lpVtbl->SetPath(pShLink, (LPCSTR) name);
        if (working)
            pShLink->lpVtbl->SetWorkingDirectory(pShLink, (LPCSTR) working);
        if (desc)
            pShLink->lpVtbl->SetDescription(pShLink, (LPCSTR) desc);

        // Query IShellLink for the IPersistFile interface for saving the
        // shortcut in persistent storage.
        hres = pShLink->lpVtbl->QueryInterface( pShLink, &IID_IPersistFile, (PVOID *) &ppf );

        if (SUCCEEDED(hres)) {
            // Ensure that the string is ANSI.
            MultiByteToWideChar( CP_ACP, 0, (LPCSTR) link, -1, wsz, MAX_PATH );

            // Save the link by calling IPersistFile::Save.
            hres = ppf->lpVtbl->Save( ppf, wsz, TRUE );
            ppf->lpVtbl->Release( ppf );
        }

        pShLink->lpVtbl->Release( pShLink );
    }

    CoUninitialize();

    return (SUCCEEDED(hres) ? TRUE : FALSE);
}



#if !defined(HAVE_NTIFS_H)
typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
	struct {
    	    USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG  Flags;
    	    WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
    	    USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
    	    WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
    	    UCHAR DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
#endif

static int
ReadReparse(const char *name, char *buf, int maxlen)
{
#define MAX_REPARSE_SIZE	(512+(16*1024))	/* Header + 16k */
    HANDLE fileHandle;
    BYTE reparseBuffer[ MAX_REPARSE_SIZE ];
    PREPARSE_GUID_DATA_BUFFER reparseInfo = (PREPARSE_GUID_DATA_BUFFER) reparseBuffer;
    PREPARSE_DATA_BUFFER rdb = (PREPARSE_DATA_BUFFER) reparseBuffer;
    DWORD returnedLength;
    int ret = 0;
                                                /* open the file image */
    if ((fileHandle = CreateFile(name, 0,
    		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 
		FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS, NULL)) == INVALID_HANDLE_VALUE) {
	ret = GetLastError();
	return -1;
    }

                                                /* retrieve reparse details */
    if (DeviceIoControl(fileHandle, FSCTL_GET_REPARSE_POINT,
        	NULL, 0, reparseInfo, sizeof(reparseBuffer), &returnedLength, NULL)) {
//      if (IsReparseTagMicrosoft(reparseInfo->ReparseTag)) {
            int length;

            switch (reparseInfo->ReparseTag) {
            case IO_REPARSE_TAG_SYMLINK:
                //
                //  Symbolic links
                //
                if ((length = rdb->SymbolicLinkReparseBuffer.SubstituteNameLength) >= 4) {
                    const size_t offset = rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(wchar_t);
                    const wchar_t* symlink = rdb->SymbolicLinkReparseBuffer.PathBuffer + offset;

                    wcstombs(buf, symlink, maxlen);
                    return 0;
                }
                break;

            case IO_REPARSE_TAG_MOUNT_POINT:
                //
                //  Mount points and junctions
                //
                if ((length = rdb->MountPointReparseBuffer.SubstituteNameLength) > 0) {
                    const size_t offset = rdb->MountPointReparseBuffer.SubstituteNameOffset / sizeof(wchar_t);
                    const wchar_t* mount = rdb->MountPointReparseBuffer.PathBuffer + offset;

                    wcstombs(buf, mount, maxlen);
                    return 0;
                }
                break;
            }
//      }
    }
    CloseHandle(fileHandle);
    return -1;
}


/*
 *  Stat() system call
 */
static int
Stat(const char *name, struct stat *sb)
{
    char fullname[WIN32_PATH_MAX], *pfname;
    int flength, ret = -1;

    if (name == NULL || sb == NULL) {
        ret = -EFAULT;                          /* basic checks */

    } else if (name[0] == '\0' ||
                    (name[1] == ':' && name[2] == '\0')) {
        ret = -ENOENT;                          /* missing directory ??? */

    } else if (strchr(name, '?') || strchr(name, '*')) {
        ret = -ENOENT;                          /* wildcards -- breaks FindFirstFile() */

    } else if ((flength = GetFullPathName(name, sizeof(fullname), fullname, &pfname)) == 0) {
        ret = -ENOENT;

    } else if (flength >= sizeof(fullname)) {
        ret = -ENAMETOOLONG;

    } else {
        HANDLE h;
        WIN32_FIND_DATA fb;
        int root = FALSE, drive = 0;

        /*
         *  determine the drive .. used as st_dev
         */
        if (! ISSLASH(fullname[0])) {           /* A=1 .. */
            drive = toupper(fullname[0]) - 'A' + 1;
        }

        /*
         *  retrieve the file details
         */
        if ((h = FindFirstFile(fullname, &fb)) == INVALID_HANDLE_VALUE) {

            if (((fullname[0] && fullname[1] == ':' &&
                        ISSLASH(fullname[2]) && fullname[3] == '\0')) &&
                    GetDriveType(fullname) > 1) {
                                                /* "x:\" */
                /*
                 *  root directories
                 */
                root = 1;
                ret = 0;

            } else if (ISSLASH(fullname[0]) && ISSLASH(fullname[1])) {
                /*
                 *  root UNC (//servername/xxx)
                 */
                const char *slash = w32_strslash(fullname + 2);
                const char *nextslash = w32_strslash(slash ? slash+1 : NULL);

                ret = -ENOENT;
                if (NULL != slash && 
                        (NULL == nextslash || 0 == nextslash[1])) {
                    root = 2;
                    ret = 0;
                }

            } else {
                ret = -ENOENT;
            }

        } else {
            (void) FindClose(h);                /* release find session */
            ret = 0;
        }

        /*
         *  assign results
         */
        if (0 == ret) {
            HANDLE handle;

            if (INVALID_HANDLE_VALUE != (handle = 
                        CreateFile(fullname, 0, 0, NULL, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_READONLY, NULL))) {
                BY_HANDLE_FILE_INFORMATION fi = {0};

                if (GetFileInformationByHandle(handle, &fi)) {
                    if (fi.nNumberOfLinks > 0) {
#if defined(_MSC_VER)                           /* XXX/NOTE */
                        sb->st_nlink = (short)fi.nNumberOfLinks;
#else
                        sb->st_nlink = fi.nNumberOfLinks;
#endif
                    }
                    fb.nFileSizeHigh    = fi.nFileSizeHigh;
                    fb.nFileSizeLow     = fi.nFileSizeLow;
                    fb.ftCreationTime   = fi.ftCreationTime;  
                    fb.ftLastAccessTime = fi.ftLastAccessTime;
                    fb.ftLastWriteTime  = fi.ftLastWriteTime;
                    sb->st_ino = w32_ino_gen(fi.nFileIndexLow, fi.nFileIndexHigh);
                }

                if (0 == sb->st_ino) {
                    sb->st_ino = w32_ino_hash(fullname);
                }

                CloseHandle(handle);

            } else {
                if (root) {
                    ret = -ENOENT;
                } else if (0 == (sb->st_ino = w32_ino_file(fullname))) {
                    sb->st_ino = w32_ino_hash(fullname);
                }
            }

            if (root) {
                fb.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
                ApplyTimes(sb, &fb.ftCreationTime, &fb.ftLastAccessTime, &fb.ftLastWriteTime);
            }
            ApplyAttributes(sb, fb.dwFileAttributes, fullname);
            ApplyTimes(sb, &fb.ftCreationTime, &fb.ftLastAccessTime, &fb.ftLastWriteTime);

            ApplySize(sb, fb.nFileSizeLow, fb.nFileSizeHigh);

            sb->st_rdev = (dev_t)(drive - 1);   /* A=0 ... */
            sb->st_dev = sb->st_rdev;
        }
    }
    return ret;
}
