/* sim_fio.c: simulator file I/O library */
// SPDX-FileCopyrightText: 1993-2008 Robert M Supnik
// SPDX-License-Identifier: X11

/*
   03-Jun-11    MP      Simplified VMS 64b support and made more portable
   02-Feb-11    MP      Added sim_fsize_ex and sim_fsize_name_ex returning t_addr
                        Added export of sim_buf_copy_swapped and sim_buf_swap_data
   28-Jun-07    RMS     Added VMS IA64 support (from Norm Lastovica)
   10-Jul-06    RMS     Fixed linux conditionalization (from Chaskiel Grundman)
   15-May-06    RMS     Added sim_fsize_name
   21-Apr-06    RMS     Added FreeBSD large file support (from Mark Martinec)
   19-Nov-05    RMS     Added OS/X large file support (from Peter Schorn)
   16-Aug-05    RMS     Fixed C++ declaration and cast problems
   17-Jul-04    RMS     Fixed bug in optimized sim_fread (reported by Scott Bailey)
   26-May-04    RMS     Optimized sim_fread (suggested by John Dundas)
   02-Jan-04    RMS     Split out from SCP

   This library includes:

   sim_finit         -       initialize package
   sim_fopen         -       open file
   sim_fread         -       endian independent read (formerly fxread)
   sim_fwrite        -       endian independent write (formerly fxwrite)
   sim_fseek         -       conditionally extended (>32b) seek (
   sim_fseeko        -       extended seek (>32b if available)
   sim_can_seek      -       test for seekable (regular file)
   sim_fsize         -       get file size
   sim_fsize_name    -       get file size of named file
   sim_fsize_ex      -       get file size as a t_offset
   sim_fsize_name_ex -       get file size as a t_offset of named file
   sim_buf_copy_swapped -    copy data swapping elements along the way
   sim_buf_swap_data -       swap data elements inplace in buffer if needed
   sim_byte_swap_data -      swap data elements inplace in buffer
   sim_shmem_open            create or attach to a shared memory region
   sim_shmem_close           close a shared memory region
   sim_chdir                 change working directory
   sim_mkdir                 create a directory
   sim_rmdir                 remove a directory
   sim_getcwd                get the current working directory
   sim_copyfile              copy a file
   sim_filepath_parts        expand and extract filename/path parts
   sim_dirscan               scan for a filename pattern
   sim_get_filelist          get a list of files matching a pattern
   sim_free_filelist         free a filelist
   sim_print_filelist        print the elements of a filelist

   sim_fopen and sim_fseek are OS-dependent.  The other routines are not.
   sim_fsize is always a 32b routine (it is used only with small capacity random
   access devices like fixed head disks and DECtapes).
*/

#define IN_SIM_FIO_C 1              /* Include from sim_fio.c */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"
#include "sim_host_path.h"
#include "sim_types.h"

bool sim_end;                       /* true = little endian, false = big endian */
bool sim_taddr_64;                  /* t_addr is > 32b and Large File Support available */
bool sim_toffset_64;                /* Large File (>2GB) file I/O Support available */

#if defined(fprintf)                /* Make sure to only use the C rtl stream I/O routines */
#undef fprintf
#undef fputs
#undef fputc
#endif

/* OS-independent, endian independent binary I/O package

   For consistency, all binary data read and written by the simulator
   is stored in little endian data order.  That is, in a multi-byte
   data item, the bytes are written out right to left, low order byte
   to high order byte.  On a big endian host, data is read and written
   from high byte to low byte.  Consequently, data written on a little
   endian system must be byte reversed to be usable on a big endian
   system, and vice versa.

   These routines are analogs of the standard C runtime routines
   fread and fwrite.  If the host is little endian, or the data items
   are size char, then the calls are passed directly to fread or
   fwrite.  Otherwise, these routines perform the necessary byte swaps.
   Sim_fread swaps in place, sim_fwrite uses an intermediate buffer.
*/

int32_t sim_finit (void)
{
union {int32_t i; char c[sizeof (int32_t)]; } end_test;

end_test.i = 1;                                         /* test endian-ness */
sim_end = (end_test.c[0] != 0);
sim_toffset_64 = (sizeof(t_offset) > sizeof(int32_t));  /* Large File (>2GB) support */
sim_taddr_64 = sim_toffset_64 && (sizeof(t_addr) > sizeof(int32_t));
return sim_end;
}

/* Copy little endian data to local buffer swapping if needed */
void sim_buf_swap_data (void *bptr, size_t size, size_t count)
{
if (sim_end || (count == 0) || (size == sizeof (char)))
    return;
sim_byte_swap_data (bptr, size, count);
}

#if defined(__GNUC__) || defined(__clang__)
#define sim_bswap16 __builtin_bswap16
#define sim_bswap32 __builtin_bswap32
#define sim_bswap64 __builtin_bswap64

#define USE_BSWAP_INTRINSIC
#elif defined(_MSC_VER)
#define sim_bswap16 _byteswap_ushort
#define sim_bswap32 _byteswap_ulong
#define sim_bswap64 _byteswap_uint64

#define USE_BSWAP_INTRINSIC
#endif

void sim_byte_swap_data (void *bptr, size_t size, size_t count)
{
    size_t j;
    uint8_t *sptr = (uint8_t *) bptr;

    if (sim_end || (count == 0) || (size == sizeof (char)))
        return;

    /* Note: Restructured this code so that GCC Link Time Optimization doesn't generate
     * spurious buffer overwrite messages.
     *
     * LTO tries to inline this function where it's used and ends up evaluating the loop.
     * It's clearly a LTO bug that needs to be worked around as opposed to waiting for a
     * compiler update (and SIMH can't guarantee that users will maintain updated platforms).
     *
     * Output from the compiler looks like:
     *
     * 363 | int32_t f, u, comp, cyl, sect, surf;
     *     |             ^
     * PDP18B/pdp18b_rp.c:363:13: note: at offset [16, 48] into destination object ‘comp’ of size 4
     * PDP18B/pdp18b_rp.c:363:13: note: at offset [80, 17179869168] into destination object ‘comp’ of size 4
     * In function ‘sim_byte_swap_data’,
     * inlined from ‘sim_byte_swap_data’ at sim_fio.c:130:6,
     * inlined from ‘sim_buf_swap_data’ at sim_fio.c:127:1,
     * inlined from ‘sim_fread’ at sim_fio.c:158:1,
     * inlined from ‘rp_svc’ at PDP18B/pdp18b_rp.c:442:15:
     * sim_fio.c:142:17: error: writing 4 bytes into a region of size 0 [-Werror=stringop-overflow=]
     * 142 |         *sptr++ = *(dptr + k);
     */

    for (j = 0; j < count; j++) {                           /* loop on items */
#if defined(USE_BSWAP_INTRINSIC)
        switch (size) {
        case sizeof(uint16_t):
            *((uint16_t *) sptr) = sim_bswap16 (*((uint16_t *) sptr));
            break;

        case sizeof(uint32_t):
            *((uint32_t *) sptr) = sim_bswap32 (*((uint32_t *) sptr));
            break;

        case sizeof(uint64_t):
            *((uint64_t *) sptr) = sim_bswap64 (*((uint64_t *) sptr));
            break;

        default:
#endif

            {
                /* Either there aren't any intrinsics that do byte swapping or
                 * it's not a well known size. */
                uint8_t *dptr;
                size_t k;
                const size_t midpoint = (size + 1) / 2;

                dptr = sptr + size - 1;
                for (k = size - 1; k >= midpoint; k--) {
                    uint8_t by = *sptr;                     /* swap end-for-end */
                    *sptr++ = *dptr;
                    *dptr-- = by;
                }
            }

#if defined(USE_BSWAP_INTRINSIC)
            break;
        }
#endif

        sptr += size;                                       /* next item */
    }
}

size_t sim_fread (void *bptr, size_t size, size_t count, FILE *fptr)
{
size_t c;

if ((size == 0) || (count == 0))                        /* check arguments */
    return 0;
c = fread (bptr, size, count, fptr);                    /* read buffer */
if (sim_end || (size == sizeof (char)) || (c == 0))     /* le, byte, or err? */
    return c;                                           /* done */
sim_buf_swap_data (bptr, size, c);
return c;
}

void sim_buf_copy_swapped (void *dbuf, const void *sbuf, size_t size, size_t count)
{
size_t j, k;
const uchar_t *sptr = (const uchar_t *)sbuf;
uchar_t *dptr = (uchar_t *)dbuf;

if (sim_end || (size == sizeof (char))) {
    memcpy (dptr, sptr, size * count);
    return;
    }
for (j = 0; j < count; j++) {                           /* loop on items */
    /* Unsigned countdown loop. Predecrement k before it's used inside the
       loop so that k == 0 in the loop body to process the last item, then
       terminate. Initialize k to size for the same reason: the predecrement
       gives us size - 1 in the loop body. */
    for (k = size; k > 0; /* empty */)
        *(dptr + --k) = *sptr++;
    dptr = dptr + size;
    }
}

size_t sim_fwrite (const void *bptr, size_t size, size_t count, FILE *fptr)
{
size_t c, nelem, nbuf, lcnt, total;
int32_t i;
const uchar_t *sptr;
uchar_t *sim_flip;

if ((size == 0) || (count == 0))                        /* check arguments */
    return 0;
if (sim_end || (size == sizeof (char)))                 /* le or byte? */
    return fwrite (bptr, size, count, fptr);            /* done */
sim_flip = (uchar_t *)malloc(FLIP_SIZE);
if (!sim_flip)
    return 0;
nelem = FLIP_SIZE / size;                               /* elements in buffer */
nbuf = count / nelem;                                   /* number buffers */
lcnt = count % nelem;                                   /* count in last buf */
if (lcnt) nbuf = nbuf + 1;
else lcnt = nelem;
total = 0;
sptr = (const uchar_t *) bptr;                          /* init input ptr */
for (i = (int32_t)nbuf; i > 0; i--) {                   /* loop on buffers */
    c = (i == 1)? lcnt: nelem;
    sim_buf_copy_swapped (sim_flip, sptr, size, c);
    sptr = sptr + size * c;
    c = fwrite (sim_flip, size, c, fptr);
    if (c == 0) {
        free(sim_flip);
        return total;
        }
    total = total + c;
    }
free(sim_flip);
return total;
}

/* Forward Declaration */

t_offset sim_ftell (FILE *st);

/* Get file size */

t_offset sim_fsize_ex (FILE *fp)
{
t_offset pos, sz;

if (fp == NULL)
    return 0;
pos = sim_ftell (fp);
if (sim_fseeko (fp, 0, SEEK_END))
    return 0;
sz = sim_ftell (fp);
if (sim_fseeko (fp, pos, SEEK_SET))
    return 0;
return sz;
}

t_offset sim_fsize_name_ex (const char *fname)
{
FILE *fp;
t_offset sz;

if ((fp = sim_fopen (fname, "rb")) == NULL)
    return 0;
sz = sim_fsize_ex (fp);
fclose (fp);
return sz;
}

uint32_t sim_fsize_name (const char *fname)
{
return (uint32_t)(sim_fsize_name_ex (fname));
}

uint32_t sim_fsize (FILE *fp)
{
return (uint32_t)(sim_fsize_ex (fp));
}

bool sim_can_seek (FILE *fp)
{
struct stat statb;

if ((0 != fstat (fileno (fp), &statb)) ||
    (0 == (statb.st_mode & S_IFREG)))
    return false;
return true;
}

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

int sim_stat (const char *fname, struct stat *stat_str)
{
char namebuf[PATH_MAX + 1];

if (NULL == sim_normalize_host_path (fname, namebuf, sizeof (namebuf)))
    return -1;
return stat (namebuf, stat_str);
}

int sim_chdir(const char *path)
{
char pathbuf[PATH_MAX + 1];

if (NULL == sim_normalize_host_path (path, pathbuf, sizeof (pathbuf)))
    return -1;
return chdir (pathbuf);
}

int sim_mkdir(const char *path)
{
char pathbuf[PATH_MAX + 1];

if (NULL == sim_normalize_host_path (path, pathbuf, sizeof (pathbuf)))
    return -1;
#if defined(_WIN32)
return mkdir (pathbuf);
#else
return mkdir (pathbuf, 0777);
#endif
}

int sim_rmdir(const char *path)
{
char pathbuf[PATH_MAX + 1];

if (NULL == sim_normalize_host_path (path, pathbuf, sizeof (pathbuf)))
    return -1;
return rmdir (pathbuf);
}

static void _sim_filelist_entry (const char *directory,
                                 const char *filename,
                                 t_offset FileSize,
                                 const struct stat *filestat,
                                 void *context)
{
char **filelist = *(char ***)context;
char FullPath[PATH_MAX + 1];
int listcount = 0;

/* Generic directory-scan callback signature.
   This implementation does not use every parameter. */
(void) FileSize;
(void) filestat;

snprintf (FullPath, sizeof (FullPath), "%s%s", directory, filename);
if (filelist != NULL) {
    while (filelist[listcount++] != NULL);
    --listcount;
    }
filelist = (char **)realloc (filelist, (listcount + 2) * sizeof (*filelist));
filelist[listcount] = strdup (FullPath);
filelist[listcount + 1] = NULL;
*(char ***)context = filelist;
}

char **sim_get_filelist (const char *filename)
{
t_stat r;
char **filelist = NULL;

r = sim_dir_scan (filename, _sim_filelist_entry, &filelist);
if (r == SCPE_OK)
    return filelist;
return NULL;
}

void sim_free_filelist (char ***pfilelist)
{
char **listp = *pfilelist;

if (listp == NULL)
    return;
while (*listp != NULL)
    free (*listp++);
free (*pfilelist);
*pfilelist = NULL;
}

void sim_print_filelist (char **filelist)
{
if (filelist == NULL)
    return;
while (*filelist != NULL)
    sim_printf ("%s\n", *filelist++);
}


/* OS-dependent routines */

/* Optimized file open */
FILE* sim_fopen (const char *file, const char *mode)
{
FILE *f;
char namebuf[PATH_MAX + 1];

if (NULL == sim_normalize_host_path (file, namebuf, sizeof (namebuf)))
    return NULL;
#if (defined (__linux) || defined (__linux__)) && !defined (DONT_DO_LARGEFILE)
f = fopen64 (namebuf, mode);
#else
f = fopen (namebuf, mode);
#endif
return f;
}

#if !defined (DONT_DO_LARGEFILE)
/* Windows */

#if defined (_WIN32)
#define S_SIM_IO_FSEEK_EXT_ 1
#include <sys/stat.h>

int sim_fseeko (FILE *st, t_offset offset, int whence)
{
return _fseeki64 (st, (__int64)offset, whence);
}

t_offset sim_ftell (FILE *st)
{
return (t_offset)_ftelli64 (st);
}

#endif                                                  /* end Windows */

/* Linux */

#if defined (__linux) || defined (__linux__)
#define S_SIM_IO_FSEEK_EXT_ 1
int sim_fseeko (FILE *st, t_offset xpos, int origin)
{
return fseeko64 (st, (off64_t)xpos, origin);
}

t_offset sim_ftell (FILE *st)
{
return (t_offset)(ftello64 (st));
}

#endif                                                  /* end Linux with LFS */

/* Apple OS/X */

#if defined (__APPLE__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined (__OpenBSD__)
#define S_SIM_IO_FSEEK_EXT_ 1
int sim_fseeko (FILE *st, t_offset xpos, int origin)
{
return fseeko (st, (off_t)xpos, origin);
}

t_offset sim_ftell (FILE *st)
{
return (t_offset)(ftello (st));
}

#endif  /* end Apple OS/X */
#endif /* !DONT_DO_LARGEFILE */

/* Default: no OS-specific routine has been defined */

#if !defined (S_SIM_IO_FSEEK_EXT_)
int sim_fseeko (FILE *st, t_offset xpos, int origin)
{
return fseek (st, (long) xpos, origin);
}

t_offset sim_ftell (FILE *st)
{
return (t_offset)(ftell (st));
}
#endif

int sim_fseek (FILE *st, t_addr offset, int whence)
{
return sim_fseeko (st, (t_offset)offset, whence);
}

#if defined(_WIN32)
const char *
sim_get_os_error_text (int Error)
{
static char szMsgBuffer[2048];
DWORD dwStatus;

dwStatus = FormatMessageA (FORMAT_MESSAGE_FROM_SYSTEM|
                           FORMAT_MESSAGE_IGNORE_INSERTS,     //  __in      DWORD dwFlags,
                           NULL,                              //  __in_opt  LPCVOID lpSource,
                           Error,                             //  __in      DWORD dwMessageId,
                           0,                                 //  __in      DWORD dwLanguageId,
                           szMsgBuffer,                       //  __out     LPTSTR lpBuffer,
                           sizeof (szMsgBuffer) - 1,          //  __in      DWORD nSize,
                           NULL);                             //  __in_opt  va_list *Arguments
if (0 == dwStatus)
    snprintf(szMsgBuffer, sizeof(szMsgBuffer) - 1, "Error Code: 0x%X", Error);
while (sim_isspace (szMsgBuffer[strlen (szMsgBuffer)-1]))
    szMsgBuffer[strlen (szMsgBuffer) - 1] = '\0';
return szMsgBuffer;
}

t_stat sim_copyfile (const char *source_file, const char *dest_file, bool overwrite_existing)
{
char sourcename[PATH_MAX + 1], destname[PATH_MAX + 1];

if (NULL == sim_normalize_host_path (source_file, sourcename, sizeof (sourcename)))
    return sim_messagef (SCPE_ARG, "Error Copying - Problem Parsing Source Filename '%s'\n", source_file);
if (NULL == sim_normalize_host_path (dest_file, destname, sizeof (destname)))
    return sim_messagef (SCPE_ARG, "Error Copying - Problem Parsing Destination Filename '%s'\n", dest_file);
if (CopyFileA (sourcename, destname, !overwrite_existing))
    return SCPE_OK;
return sim_messagef (SCPE_ARG, "Error Copying '%s' to '%s': %s\n", source_file, dest_file, sim_get_os_error_text (GetLastError ()));
}

static void _time_t_to_filetime (time_t ttime, FILETIME *filetime)
{
uint64_t time64;

time64 = 134774;                /* Days between Jan 1, 1601 and Jan 1, 1970 */
time64 *= 24;                   /* Hours */
time64 *= 3600;                 /* Seconds */
time64 += (uint64_t)ttime;      /* include time_t seconds */

time64 *= 10000000;             /* Convert seconds to 100ns units */
filetime->dwLowDateTime = (DWORD)time64;
filetime->dwHighDateTime = (DWORD)(time64 >> 32);
}

t_stat sim_set_file_times (const char *file_name, time_t access_time, time_t write_time)
{
char filename[PATH_MAX + 1];
FILETIME accesstime, writetime;
HANDLE hFile;
BOOL bStat;

_time_t_to_filetime (access_time, &accesstime);
_time_t_to_filetime (write_time, &writetime);
if (NULL == sim_normalize_host_path (file_name, filename, sizeof (filename)))
    return sim_messagef (SCPE_ARG, "Error Setting File Times - Problem Source Filename '%s'\n", filename);
hFile = CreateFileA (filename, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
if (hFile == INVALID_HANDLE_VALUE)
    return sim_messagef (SCPE_IOERR, "Can't open file '%s' to set it's times: %s\n", filename, sim_get_os_error_text (GetLastError ()));
bStat = SetFileTime (hFile, NULL, &accesstime, &writetime);
CloseHandle (hFile);
return bStat ? SCPE_OK : sim_messagef (SCPE_IOERR, "Error setting file '%s' times: %s\n", filename, sim_get_os_error_text (GetLastError ()));
}


#include <io.h>
#include <direct.h>
int sim_set_fsize (FILE *fptr, t_addr size)
{
return _chsize(_fileno(fptr), (long)size);
}

int sim_set_fifo_nonblock (FILE *fptr)
{
return -1;
}

struct SHMEM {
    HANDLE hMapping;
    size_t shm_size;
    void *shm_base;
    char *shm_name;
    };

t_stat sim_shmem_open (const char *name, size_t size, SHMEM **shmem, void **addr)
{
SYSTEM_INFO SysInfo;
bool AlreadyExists;

GetSystemInfo (&SysInfo);
*shmem = (SHMEM *)calloc (1, sizeof(**shmem));
if (*shmem == NULL)
    return SCPE_MEM;
(*shmem)->shm_name = (char *)calloc (1, 1 + strlen (name));
if ((*shmem)->shm_name == NULL) {
    free (*shmem);
    *shmem = NULL;
    return SCPE_MEM;
    }
strlcpy ((*shmem)->shm_name, name, 1 + strlen (name));
(*shmem)->hMapping = INVALID_HANDLE_VALUE;
(*shmem)->shm_size = size;
(*shmem)->shm_base = NULL;
(*shmem)->hMapping = CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE|SEC_COMMIT, 0, (DWORD)(size+SysInfo.dwPageSize), name);
if ((*shmem)->hMapping == INVALID_HANDLE_VALUE) {
    DWORD LastError = GetLastError();

    sim_shmem_close (*shmem);
    *shmem = NULL;
    return sim_messagef (SCPE_OPENERR, "Can't CreateFileMapping of a %u byte shared memory segment '%s' - LastError=0x%X\n", (uint_t)size, name, (uint_t)LastError);
    }
AlreadyExists = (GetLastError () == ERROR_ALREADY_EXISTS);
(*shmem)->shm_base = MapViewOfFile ((*shmem)->hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
if ((*shmem)->shm_base == NULL) {
    DWORD LastError = GetLastError();

    sim_shmem_close (*shmem);
    *shmem = NULL;
    return sim_messagef (SCPE_OPENERR, "Can't MapViewOfFile() of a %u byte shared memory segment '%s' - LastError=0x%X\n", (uint_t)size, name, (uint_t)LastError);
    }
if (AlreadyExists) {
    if (*((DWORD *)((*shmem)->shm_base)) == 0)
        Sleep (50);
    if (*((DWORD *)((*shmem)->shm_base)) != (DWORD)size) {
        DWORD SizeFound = *((DWORD *)((*shmem)->shm_base));
        sim_shmem_close (*shmem);
        *shmem = NULL;
        return sim_messagef (SCPE_OPENERR, "Shared Memory segment '%s' is %u bytes instead of %d\n", name, (uint_t)SizeFound, (int)size);
        }
    }
else
    *((DWORD *)((*shmem)->shm_base)) = (DWORD)size;     /* Save Size in first page */

*addr = ((char *)(*shmem)->shm_base + SysInfo.dwPageSize);      /* Point to the second page for data */
return SCPE_OK;
}

void sim_shmem_close (SHMEM *shmem)
{
if (shmem == NULL)
    return;
if (shmem->shm_base != NULL)
    UnmapViewOfFile (shmem->shm_base);
if (shmem->hMapping != INVALID_HANDLE_VALUE)
    CloseHandle (shmem->hMapping);
free (shmem->shm_name);
free (shmem);
}

int32_t sim_shmem_atomic_add (int32_t *p, int32_t v)
{
return InterlockedExchangeAdd ((volatile long *) p,v) + (v);
}

bool sim_shmem_atomic_cas (int32_t *ptr, int32_t oldv, int32_t newv)
{
return (InterlockedCompareExchange ((LONG volatile *) ptr, newv, oldv) == oldv);
}

#else /* !defined(_WIN32) */
#include <unistd.h>
int sim_set_fsize (FILE *fptr, t_addr size)
{
return ftruncate(fileno(fptr), (off_t)size);
}

#include <sys/stat.h>
#include <fcntl.h>
#if defined (HAVE_UTIME)
#include <utime.h>
#endif

const char *
sim_get_os_error_text (int Error)
{
return strerror (Error);
}

t_stat sim_copyfile (const char *source_file, const char *dest_file, bool overwrite_existing)
{
FILE *fIn = NULL, *fOut = NULL;
t_stat st = SCPE_OK;
char *buf = NULL;
size_t bytes;
#if defined(SIMH_NO_FOPEN_X)
struct stat statb;
#endif

fIn = sim_fopen (source_file, "rb");
if (!fIn) {
    st = sim_messagef (SCPE_ARG, "Can't open '%s' for input: %s\n", source_file, strerror (errno));
    goto Cleanup_Return;
    }
#if !defined(SIMH_NO_FOPEN_X)
fOut = sim_fopen (dest_file, overwrite_existing ? "wb" : "xb");
#else
    // TODO: Replace this fallback with an atomic no-overwrite open on
    // platforms where CMake did not confirm fopen exclusive-create
    // mode. In theory this sim_stat/open sequence can race with
    // another creator.
if (!overwrite_existing && (sim_stat (dest_file, &statb) == 0)) {
    st = sim_messagef (SCPE_ARG, "Can't open '%s' for output: %s\n", dest_file, strerror (EEXIST));
    goto Cleanup_Return;
    }
fOut = sim_fopen (dest_file, "wb");
#endif
if (!fOut) {
    st = sim_messagef (SCPE_ARG, "Can't open '%s' for output: %s\n", dest_file, strerror (errno));
    goto Cleanup_Return;
    }
buf = (char *)malloc (BUFSIZ);
while ((bytes = fread (buf, 1, BUFSIZ, fIn)))
    fwrite (buf, 1, bytes, fOut);
Cleanup_Return:
free (buf);
if (fIn)
    fclose (fIn);
if (fOut)
    fclose (fOut);
#if defined(HAVE_UTIME)
if (st == SCPE_OK) {
    struct stat statb;

    if (!sim_stat (source_file, &statb)) {
        struct utimbuf utim;

        utim.actime = statb.st_atime;
        utim.modtime = statb.st_mtime;
        if (utime (dest_file, &utim))
            st = SCPE_IOERR;
        }
    else
        st = SCPE_IOERR;
    }
#endif
return st;
}

t_stat sim_set_file_times (const char *file_name, time_t access_time, time_t write_time)
{
t_stat st = SCPE_IOERR;
#if defined (HAVE_UTIME)
struct utimbuf utim;

utim.actime = access_time;
utim.modtime = write_time;
if (!utime (file_name, &utim))
    st = SCPE_OK;
#else
st = SCPE_NOFNC;
#endif
return st;
}

int sim_set_fifo_nonblock (FILE *fptr)
{
struct stat stbuf;

if (!fptr || fstat (fileno(fptr), &stbuf))
    return -1;
#if defined(S_IFIFO) && defined(O_NONBLOCK)
if ((stbuf.st_mode & S_IFIFO)) {
    int flags = fcntl(fileno(fptr), F_GETFL, 0);
    return fcntl(fileno(fptr), F_SETFL, flags | O_NONBLOCK);
    }
#endif
return -1;
}

#if defined (__linux__) || defined (__APPLE__) || defined (__FreeBSD__) || defined(__NetBSD__) || defined (__OpenBSD__)

#if defined (HAVE_SHM_OPEN)
#include <sys/mman.h>
#endif

struct SHMEM {
    int shm_fd;
    size_t shm_size;
    void *shm_base;
    char *shm_name;
    };

t_stat sim_shmem_open (const char *name, size_t size, SHMEM **shmem, void **addr)
{
#if defined (HAVE_SHM_OPEN) && defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
*shmem = (SHMEM *)calloc (1, sizeof(**shmem));
mode_t orig_mask;

*addr = NULL;
if (*shmem == NULL)
    return SCPE_MEM;
(*shmem)->shm_name = (char *)calloc (1, 1 + strlen (name) + ((*name != '/') ? 1 : 0));
if ((*shmem)->shm_name == NULL) {
    free (*shmem);
    *shmem = NULL;
    return SCPE_MEM;
    }

snprintf ((*shmem)->shm_name,
          2 + strlen (name),
          "%s%s",
          ((*name != '/') ? "/" : ""),
          name);
(*shmem)->shm_base = MAP_FAILED;
(*shmem)->shm_size = size;
(*shmem)->shm_fd = shm_open ((*shmem)->shm_name, O_RDWR, 0);
if ((*shmem)->shm_fd == -1) {
    int last_errno;

    orig_mask = umask (0000);
    (*shmem)->shm_fd = shm_open ((*shmem)->shm_name, O_CREAT | O_RDWR, 0660);
    last_errno = errno;
    umask (orig_mask);                  /* Restore original mask */
    if ((*shmem)->shm_fd == -1) {
        sim_shmem_close (*shmem);
        *shmem = NULL;
        return sim_messagef (SCPE_OPENERR, "Can't shm_open() a %d byte shared memory segment '%s' - errno=%d - %s\n", (int)size, name, last_errno, strerror (last_errno));
        }
    if (ftruncate((*shmem)->shm_fd, size)) {
        sim_shmem_close (*shmem);
        *shmem = NULL;
        return SCPE_OPENERR;
        }
    }
else {
    struct stat statb;

    if ((fstat ((*shmem)->shm_fd, &statb)) ||
        ((size_t)statb.st_size != (*shmem)->shm_size)) {
        sim_shmem_close (*shmem);
        *shmem = NULL;
        return sim_messagef (SCPE_OPENERR, "Shared Memory segment '%s' is %d bytes instead of %d\n", name, (int)(statb.st_size), (int)size);
        }
    }
(*shmem)->shm_base = mmap(NULL, (*shmem)->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, (*shmem)->shm_fd, 0);
if ((*shmem)->shm_base == MAP_FAILED) {
    int last_errno = errno;

    sim_shmem_close (*shmem);
    *shmem = NULL;
    return sim_messagef (SCPE_OPENERR, "Shared Memory '%s' mmap() failed. errno=%d - %s\n", name, last_errno, strerror (last_errno));
    }
*addr = (*shmem)->shm_base;
return SCPE_OK;
#else
*shmem = NULL;
return sim_messagef (SCPE_NOFNC, "Shared memory not available - Missing shm_open() API\n");
#endif
}

void sim_shmem_close (SHMEM *shmem)
{
#if defined (HAVE_SHM_OPEN)
if (shmem == NULL)
    return;
if (shmem->shm_base != MAP_FAILED)
    munmap (shmem->shm_base, shmem->shm_size);
if (shmem->shm_fd != -1) {
    shm_unlink (shmem->shm_name);
    close (shmem->shm_fd);
    }
free (shmem->shm_name);
free (shmem);
#endif
}

int32_t sim_shmem_atomic_add (int32_t *p, int32_t v)
{
#if defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
return __sync_add_and_fetch ((int *) p, v);
#else
return *p + v;
#endif
}

bool sim_shmem_atomic_cas (int32_t *ptr, int32_t oldv, int32_t newv)
{
#if defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
return __sync_bool_compare_and_swap (ptr, oldv, newv);
#else
if (*ptr == oldv) {
    *ptr = newv;
    return 1;
    }
else
    return 0;
#endif
}

#else /* !(defined (__linux__) || defined (__APPLE__)) */

t_stat sim_shmem_open (const char *name, size_t size, SHMEM **shmem, void **addr)
{
return SCPE_NOFNC;
}

void sim_shmem_close (SHMEM *shmem)
{
}

int32_t sim_shmem_atomic_add (int32_t *p, int32_t v)
{
return -1;
}

bool sim_shmem_atomic_cas (int32_t *ptr, int32_t oldv, int32_t newv)
{
return false;
}

#endif /* defined (__linux__) || defined (__APPLE__) */
#endif /* defined (_WIN32) */

char *sim_getcwd (char *buf, size_t buf_size)
{
#if defined(__MINGW64__) ||defined(_MSC_VER) || defined(__MINGW32__)
return _getcwd (buf, (int) buf_size);
#else
return getcwd (buf, buf_size);
#endif
}

/*
 * Parsing and expansion of file names.
 *
 *    %~I%        - expands filepath value removing any surrounding quotes (" or ')
 *    %~fI%       - expands filepath value to a fully qualified path name
 *    %~pI%       - expands filepath value to a path only
 *    %~nI%       - expands filepath value to a file name only
 *    %~xI%       - expands filepath value to a file extension only
 *    %~tI%       - expands filepath value to the file timestamp
 *    %~zI%       - expands filepath value to the file size
 *
 * The modifiers can be combined to get compound results:
 *
 *    %~pnI%      - expands filepath value to a path and name only
 *    %~nxI%      - expands filepath value to a file name and extension only
 *
 * In the above example above %I% can be replaced by other
 * environment variables or numeric parameters to a DO command
 * invocation.
 */

/* Records the expanded path information used by sim_filepath_parts().
   fullpath is allocated for this struct and freed with it; name and ext
   point into fullpath.  Optional file size and timestamp text are cached
   here when requested. */
struct sim_filepath_info {
    char *fullpath;
    char *name;
    char *ext;
    char filesize[32];
    /* GCC cannot infer localtime field ranges, so this covers the
       worst case so GCC can prove the timestamp formatting fits. */
    char datetime[80];
};

/* Detect paths that do not need the current directory prepended.
   For example, /tmp/a and C:\tmp\a are already full paths. */
static bool sim_filepath_is_fullpath(const char *filepath)
{
    return (((filepath[0] != '\0') && (filepath[1] == ':')) ||
            (filepath[0] == '/') || (filepath[0] == '\\'));
}

/* Return an allocated full path string.  For example, report.bin becomes
   <current-directory>/report.bin. */
static char *sim_filepath_make_fullpath(const char *filepath)
{
    size_t tot_len;
    char *fullpath;

    if (sim_filepath_is_fullpath(filepath)) {
        tot_len = 1 + strlen(filepath);
        fullpath = (char *)malloc(tot_len);
        if (fullpath == NULL)
            return NULL;
        strlcpy(fullpath, filepath, tot_len);
        return fullpath;
    }

    {
        char dir[PATH_MAX + 1] = "";
        char *wd = sim_getcwd(dir, sizeof(dir));
        size_t dir_len;

        if (wd == NULL)
            return NULL;

        dir_len = strlen(dir);
        tot_len = 1 + strlen(filepath) + 1 + dir_len;
        fullpath = (char *)malloc(tot_len);
        if (fullpath == NULL)
            return NULL;

        strlcpy(fullpath, dir, tot_len);
        if ((dir_len != 0) &&
            (dir[dir_len - 1] != '/') &&
            (dir[dir_len - 1] != '\\'))
            strlcat(fullpath, "/", tot_len);
        strlcat(fullpath, filepath, tot_len);
        return fullpath;
    }
}

/* Preserve the lexical path cleanup rules for filepath parts.  For example,
   /tmp//a/./b/../c becomes /tmp/a/c. */
static void sim_filepath_normalize_fullpath(char *fullpath)
{
    char *c;

    while ((c = strchr(fullpath, '\\')) != NULL)
        *c = '/';
    if ((fullpath[0] != '\0') && (fullpath[1] == ':') &&
        islower(fullpath[0]))
        fullpath[0] = toupper(fullpath[0]);
    while ((c = strstr(fullpath + 1, "//")) != NULL)
        memmove(c, c + 1, 1 + strlen(c + 1));
    while ((c = strstr(fullpath, "/./")) != NULL)
        memmove(c, c + 2, 1 + strlen(c + 2));
    while ((c = strstr(fullpath, "/../")) != NULL) {
        char *cl = c - 1;

        while ((*cl != '/') && (cl > fullpath))
            --cl;
        if ((cl <= fullpath) ||
            ((fullpath[1] == ':') && (c == fullpath + 2)))
            memmove(c, c + 3, 1 + strlen(c + 3));
        else if (*cl == '/')
            memmove(cl, c + 3, 1 + strlen(c + 3));
        else
            break;
    }
}

/* Point the name and extension fields into the normalized fullpath.
   For example, /tmp/report.bin records report.bin and .bin. */
static void sim_filepath_find_parts(struct sim_filepath_info *info)
{
    char *name;

    name = strrchr(info->fullpath, '/');
    if (name == NULL)
        info->name = info->fullpath + strlen(info->fullpath);
    else
        info->name = name + 1;

    info->ext = strrchr(info->name, '.');
    if (info->ext == NULL)
        info->ext = info->name + strlen(info->name);
}

/* Format the %~z file-size substitution.  The result includes the
   trailing space that existing command substitutions expect. */
static void sim_filepath_format_size(char *buf, size_t size,
                                     const struct stat *filestat)
{
    if (sizeof(filestat->st_size) == 4)
        snprintf(buf, size, "%ld ", (long)filestat->st_size);
    else
        snprintf(buf, size, "%" PRIdMAX " ", (intmax_t)filestat->st_size);
}

/* Format the MM/DD/YYYY hh:mm AM/PM timestamp substitution. */
static bool sim_filepath_format_datetime(char *buf, size_t size,
                                           time_t mtime)
{
    struct tm *tm = localtime(&mtime);
    int written;

    if (tm == NULL)
        return false;

    written = snprintf(buf, size, "%02d/%02d/%04d %02d:%02d %cM ",
                       1 + tm->tm_mon, tm->tm_mday, 1900 + tm->tm_year,
                       tm->tm_hour % 12, tm->tm_min,
                       (0 == (tm->tm_hour % 12)) ? 'A' : 'P');
    return ((written >= 0) && ((size_t)written < size));
}

/* Missing or unreadable files still get deterministic placeholder metadata:
   size 0 and the timestamp produced from time_t 0. */
static void sim_filepath_read_metadata(struct sim_filepath_info *info)
{
    struct stat filestat;

    memset(&filestat, 0, sizeof(filestat));
    (void)stat(info->fullpath, &filestat);
    sim_filepath_format_size(info->filesize, sizeof(info->filesize),
                             &filestat);
    (void)sim_filepath_format_datetime(info->datetime, sizeof(info->datetime),
                                       filestat.st_mtime);
}

/* Allocate and normalize fullpath, then record name and ext inside it.
   For example, /tmp//alpha/./report.bin becomes /tmp/alpha/report.bin,
   with name pointing at report.bin and ext pointing at .bin. */
static bool sim_filepath_info_init(struct sim_filepath_info *info,
                                     const char *filepath)
{
    memset(info, 0, sizeof(*info));
    info->fullpath = sim_filepath_make_fullpath(filepath);
    if (info->fullpath == NULL)
        return false;

    sim_filepath_normalize_fullpath(info->fullpath);
    sim_filepath_find_parts(info);
    return true;
}

/* Release owned storage and clear borrowed pointers into it. */
static void sim_filepath_info_free(struct sim_filepath_info *info)
{
    free(info->fullpath);
    memset(info, 0, sizeof(*info));
}

/* Return the number of bytes one requested part will add to the result.
   For example, 'n' contributes the filename length without the extension,
   and 'p' contributes the directory prefix length. */
static size_t sim_filepath_part_size(const struct sim_filepath_info *info,
                                     char part)
{
    switch (part) {
    case 'f':
        return strlen(info->fullpath);
    case 'p':
        return (size_t)(info->name - info->fullpath);
    case 'n':
        return (size_t)(info->ext - info->name);
    case 'x':
        return strlen(info->ext);
    case 't':
        return strlen(info->datetime);
    case 'z':
        return strlen(info->filesize);
    default:
        return 0;
    }
}

/* Measure the exact allocation needed for the requested part list. */
static size_t sim_filepath_parts_size(const struct sim_filepath_info *info,
                                      const char *filepath,
                                      const char *parts)
{
    size_t size = 0;
    const char *p;

    if (*parts == '\0')
        return strlen(filepath);

    for (p = parts; *p; ++p)
        size += sim_filepath_part_size(info, *p);

    return size;
}

/* Append a byte span and return the next output position. */
static char *sim_filepath_append(char *dst, const char *src, size_t len)
{
    memcpy(dst, src, len);
    return dst + len;
}

/* Append the text for one part character, such as 'p' for the path or
   'n' for the filename without extension. */
static char *sim_filepath_append_part(char *dst,
                                      const struct sim_filepath_info *info,
                                      char part)
{
    switch (part) {
    case 'f':
        return sim_filepath_append(dst, info->fullpath, strlen(info->fullpath));
    case 'p':
        return sim_filepath_append(dst, info->fullpath,
                                   (size_t)(info->name - info->fullpath));
    case 'n':
        return sim_filepath_append(dst, info->name,
                                   (size_t)(info->ext - info->name));
    case 'x':
        return sim_filepath_append(dst, info->ext, strlen(info->ext));
    case 't':
        return sim_filepath_append(dst, info->datetime, strlen(info->datetime));
    case 'z':
        return sim_filepath_append(dst, info->filesize, strlen(info->filesize));
    default:
        return dst;
    }
}

/* Allocate and assemble the final substitution result. */
static char *sim_filepath_parts_build(const struct sim_filepath_info *info,
                                      const char *filepath, const char *parts)
{
    size_t size = sim_filepath_parts_size(info, filepath, parts);
    char *result = (char *)malloc(size + 1);
    char *dst = result;
    const char *p;

    if (result == NULL)
        return NULL;

    if (*parts == '\0')
        dst = sim_filepath_append(dst, filepath, strlen(filepath));
    else
        for (p = parts; *p; ++p)
            dst = sim_filepath_append_part(dst, info, *p);

    *dst = '\0';
    return result;
}

/* Expand filepath, then return the requested path, name, metadata, or
   combined parts described by the part-character string. */
char *sim_filepath_parts(const char *filepath, const char *parts)
{
    struct sim_filepath_info info;
    char *result;
    char namebuf[PATH_MAX + 1];

    if (NULL == sim_normalize_host_path(filepath, namebuf, sizeof(namebuf)))
        return NULL;
    filepath = namebuf;

    if (!sim_filepath_info_init(&info, filepath))
        return NULL;
    if (strchr(parts, 't') || strchr(parts, 'z'))
        sim_filepath_read_metadata(&info);

    result = sim_filepath_parts_build(&info, filepath, parts);
    sim_filepath_info_free(&info);
    return result;
}

#if defined (_WIN32)

t_stat sim_dir_scan (const char *cptr, DIR_ENTRY_CALLBACK entry, void *context)
{
HANDLE hFind;
WIN32_FIND_DATAA File;
struct stat filestat;
char WildName[PATH_MAX + 1];

if (NULL == sim_normalize_host_path (cptr, WildName, sizeof (WildName)))
    return SCPE_ARG;
cptr = WildName;
sim_trim_endspc (WildName);
if ((hFind =  FindFirstFileA (cptr, &File)) != INVALID_HANDLE_VALUE) {
    int64_t FileSize;
    char DirName[PATH_MAX + 1], FileName[PATH_MAX + 1];
    char *c;
    size_t dir_len;
    const char *backslash = strchr (cptr, '\\');
    const char *slash = strchr (cptr, '/');
    const char *pathsep = (backslash && slash) ? MIN (backslash, slash) : (backslash ? backslash : slash);

    GetFullPathNameA(cptr, sizeof(DirName), DirName, (char **)&c);
    c = strrchr (DirName, '\\');
    *c = '\0';                                  /* Truncate to just directory path */
    if (!pathsep ||                             /* Separator wasn't mentioned? */
        (slash && (0 == strcmp (slash, "/*"))))
        pathsep = "\\";                         /* Default to Windows backslash */
    if (*pathsep == '/') {                      /* If slash separator? */
        while ((c = strchr (DirName, '\\')))
            *c = '/';                           /* Convert backslash to slash */
        }
    dir_len = strlen (DirName);
    DirName[dir_len] = *pathsep;
    DirName[dir_len + 1] = '\0';
    do {
        if ((strcmp (File.cFileName, ".") == 0) ||
            (strcmp (File.cFileName, "..") == 0))
            continue;
        FileSize = (((int64_t)(File.nFileSizeHigh)) << 32) | File.nFileSizeLow;
        strlcpy (FileName, DirName, sizeof (FileName));
        strlcat (FileName, File.cFileName, sizeof (FileName));
        stat (FileName, &filestat);
        entry (DirName, File.cFileName, FileSize, &filestat, context);
        } while (FindNextFile (hFind, &File));
    FindClose (hFind);
    }
else
    return SCPE_ARG;
return SCPE_OK;
}

#else /* !defined (_WIN32) */

#include <sys/stat.h>
#if defined (HAVE_GLOB)
#include <glob.h>
#else /* !defined (HAVE_GLOB) */
#include <dirent.h>
#if defined (HAVE_FNMATCH)
#include <fnmatch.h>
#endif
#endif /* defined (HAVE_GLOB) */

t_stat sim_dir_scan (const char *cptr, DIR_ENTRY_CALLBACK entry, void *context)
{
#if defined (HAVE_GLOB)
glob_t  paths;
#else
DIR *dir;
#endif
int found_count = 0;
struct stat filestat;
char *c;
char DirName[PATH_MAX + 1], WholeName[PATH_MAX + 1], WildName[PATH_MAX + 1], MatchName[PATH_MAX + 1];

memset (DirName, 0, sizeof(DirName));
memset (WholeName, 0, sizeof(WholeName));
memset (MatchName, 0, sizeof(MatchName));
if (NULL == sim_normalize_host_path (cptr, WildName, sizeof (WildName)))
    return SCPE_ARG;
cptr = WildName;
sim_trim_endspc (WildName);
c = sim_filepath_parts (cptr, "f");
strlcpy (WholeName, c, sizeof (WholeName));
free (c);
c = sim_filepath_parts (cptr, "nx");
strlcpy (MatchName, c, sizeof (MatchName));
free (c);
c = strrchr (WholeName, '/');
if (c) {
    memmove (DirName, WholeName, 1+c-WholeName);
    DirName[1+c-WholeName] = '\0';
    }
else
    DirName[0] = '\0';
cptr = WholeName;
#if defined (HAVE_GLOB)
memset (&paths, 0, sizeof (paths));
if (0 == glob (cptr, 0, NULL, &paths)) {
#else
dir = opendir(DirName[0] ? DirName : "/.");
if (dir) {
    struct dirent *ent;
#endif
    t_offset FileSize;
    char *FileName;
     char *p_name;
#if defined (HAVE_GLOB)
    size_t i;
#endif

#if defined (HAVE_GLOB)
    for (i=0; i<paths.gl_pathc; i++) {
        FileName = (char *)malloc (1 + strlen (paths.gl_pathv[i]));
        snprintf (FileName, 1 + strlen (paths.gl_pathv[i]), "%s",
                  paths.gl_pathv[i]);
#else /* !defined (HAVE_GLOB) */
    while ((ent = readdir (dir))) {
#if defined (HAVE_FNMATCH)
        if (fnmatch(MatchName, ent->d_name, 0))
            continue;
#else /* !defined (HAVE_FNMATCH) */
        /* only match all names or exact name without fnmatch support */
        if ((strcmp(MatchName, "*") != 0) &&
            (strcmp(MatchName, ent->d_name) != 0))
            continue;
#endif /* defined (HAVE_FNMATCH) */
        FileName = (char *)malloc (1 + strlen (DirName) +
                                   strlen (ent->d_name));
        snprintf (FileName, 1 + strlen (DirName) + strlen (ent->d_name),
                  "%s%s", DirName, ent->d_name);
#endif /* defined (HAVE_GLOB) */
        p_name = FileName + strlen (DirName);
        memset (&filestat, 0, sizeof (filestat));
        (void)stat (FileName, &filestat);
        FileSize = (t_offset)((filestat.st_mode & S_IFDIR) ? 0 : sim_fsize_name_ex (FileName));
        entry (DirName, p_name, FileSize, &filestat, context);
        free (FileName);
        ++found_count;
        }
#if defined (HAVE_GLOB)
    globfree (&paths);
#else
    closedir (dir);
#endif
    }
else
    return SCPE_ARG;
if (found_count)
    return SCPE_OK;
else
    return SCPE_ARG;
}
#endif /* !defined(_WIN32) */

/* Trim trailing spaces from a string

    Inputs:
        cptr    =       pointer to string
    Outputs:
        cptr    =       pointer to string
*/

char *sim_trim_endspc (char *cptr)
{
char *tptr;

tptr = cptr + strlen (cptr);
while ((--tptr >= cptr) && sim_isspace (*tptr))
    *tptr = 0;
return cptr;
}

int sim_isspace (int c)
{
return ((c < 0) || (c >= 128)) ? 0 : isspace (c);
}

int sim_islower (int c)
{
return (c >= 'a') && (c <= 'z');
}

int sim_isupper (int c)
{
return (c >= 'A') && (c <= 'Z');
}

int sim_toupper (int c)
{
return ((c >= 'a') && (c <= 'z')) ? ((c - 'a') + 'A') : c;
}

int sim_tolower (int c)
{
return ((c >= 'A') && (c <= 'Z')) ? ((c - 'A') + 'a') : c;
}

int sim_isalpha (int c)
{
return ((c < 0) || (c >= 128)) ? 0 : isalpha (c);
}

int sim_isprint (int c)
{
return ((c < 0) || (c >= 128)) ? 0 : isprint (c);
}

int sim_isdigit (int c)
{
return ((c >= '0') && (c <= '9'));
}

int sim_isgraph (int c)
{
return ((c < 0) || (c >= 128)) ? 0 : isgraph (c);
}

int sim_isalnum (int c)
{
return ((c < 0) || (c >= 128)) ? 0 : isalnum (c);
}

/* strncasecmp() is not available on all platforms */
int sim_strncasecmp (const char* string1, const char* string2, size_t len)
{
size_t i;
uchar_t s1, s2;

for (i=0; i<len; i++) {
    s1 = (uchar_t)string1[i];
    s2 = (uchar_t)string2[i];
    s1 = (uchar_t)sim_toupper (s1);
    s2 = (uchar_t)sim_toupper (s2);
    if (s1 < s2)
        return -1;
    if (s1 > s2)
        return 1;
    if (s1 == 0)
        return 0;
    }
return 0;
}

/* strcasecmp() is not available on all platforms */
int sim_strcasecmp (const char *string1, const char *string2)
{
size_t i = 0;
uchar_t s1, s2;

while (1) {
    s1 = (uchar_t)string1[i];
    s2 = (uchar_t)string2[i];
    s1 = (uchar_t)sim_toupper (s1);
    s2 = (uchar_t)sim_toupper (s2);
    if (s1 == s2) {
        if (s1 == 0)
            return 0;
        i++;
        continue;
        }
    if (s1 < s2)
        return -1;
    if (s1 > s2)
        return 1;
    }
return 0;
}

int sim_strwhitecasecmp (const char *string1, const char *string2, bool casecmp)
{
uchar_t s1 = 1, s2 = 1;         /* start with equal, but not space */

while ((s1 == s2) && (s1 != '\0')) {
    if (s1 == ' ') {            /* last character was space? */
        while (s1 == ' ') {     /* read until not a space */
            s1 = *string1++;
            if (sim_isspace (s1))
                s1 = ' ';       /* all whitespace is a space */
            else {
                if (casecmp)
                    s1 = (uchar_t)sim_toupper (s1);
                }
            }
        }
    else {                      /* get new character */
        s1 = *string1++;
        if (sim_isspace (s1))
            s1 = ' ';           /* all whitespace is a space */
        else {
            if (casecmp)
                s1 = (uchar_t)sim_toupper (s1);
            }
        }
    if (s2 == ' ') {            /* last character was space? */
        while (s2 == ' ') {     /* read until not a space */
            s2 = *string2++;
            if (sim_isspace (s2))
                s2 = ' ';       /* all whitespace is a space */
            else {
                if (casecmp)
                    s2 = (uchar_t)sim_toupper (s2);
                }
            }
        }
    else {                      /* get new character */
        s2 = *string2++;
        if (sim_isspace (s2))
            s2 = ' ';           /* all whitespace is a space */
        else {
            if (casecmp)
                s2 = (uchar_t)sim_toupper (s2);
            }
        }
    if (s1 == s2) {
        if (s1 == 0)
            return 0;
        continue;
        }
    if (s1 < s2)
        return -1;
    if (s1 > s2)
        return 1;
    }
return 0;
}
