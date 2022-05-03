#ifndef _TIF_CONFIG_H_
#define _TIF_CONFIG_H_

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define as 0 or 1 according to the floating point format suported by the
   machine */
#define HAVE_IEEEFP 1

/* Define to 1 if you have the `jbg_newlen' function. */
#define HAVE_JBG_NEWLEN 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <io.h> header file. */
#ifdef _WIN32
#define HAVE_IO_H 1
#endif //_WIN32

#ifdef __linux__
#define HAVE_UNISTD_H
#endif //__linux__

/* Define to 1 if you have the <search.h> header file. */
#define HAVE_SEARCH_H 1

/* Define to 1 if you have the `setmode' function. */
#define HAVE_SETMODE 1

/* Define to 1 if you have the declaration of `optarg', and to 0 if you don't. */
#define HAVE_DECL_OPTARG 0

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT sizeof(int)

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG sizeof(long)

/* Signed 8-bit type */
#define TIFF_INT8_T char

/* Unsigned 8-bit type */
#define TIFF_UINT8_T unsigned char

/* Signed 16-bit type */
#define TIFF_INT16_T int16_t

/* Unsigned 16-bit type */
#define TIFF_UINT16_T uint16_t

/* Signed 32-bit type formatter */
#define TIFF_INT32_FORMAT "%" PRId32

/* Signed 32-bit type */
#define TIFF_INT32_T int32_t

/* Unsigned 32-bit type formatter */
#define TIFF_UINT32_FORMAT "%" PRIu32

/* Unsigned 32-bit type */
#define TIFF_UINT32_T uint32_t

/* Signed 64-bit type formatter */
#define TIFF_INT64_FORMAT "%" PRId64

/* Signed 64-bit type */
#define TIFF_INT64_T int64_t

/* Unsigned 64-bit type formatter */
#define TIFF_UINT64_FORMAT "%" PRIu64

/* Unsigned 64-bit type */
#define TIFF_UINT64_T uint64_t

#if defined(_WIN64) || defined(_M_AMD64) || defined(__x86_64__)

#define HAS_LONG_ARCH

#endif //64BIT SWITCH

/* Pointer difference type */
#  define TIFF_PTRDIFF_T ptrdiff_t

/* The size of `size_t', as computed by sizeof. */
#  define SIZEOF_SIZE_T (sizeof(size_t))

/* Size type formatter */
#  define TIFF_SIZE_FORMAT "%zu"

/* Unsigned size type */
#  define TIFF_SIZE_T size_t

/* Signed size type formatter */
#  define TIFF_SSIZE_FORMAT "%zd"

/* Signed size type */
#  define TIFF_SSIZE_T ptrdiff_t

/* Set the native cpu bit order */
#define HOST_FILLORDER FILLORDER_LSB2MSB

/* Visual Studio 2015 / VC 14 / MSVC 19.00 finally has snprintf() */
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#else
#define HAVE_SNPRINTF 1
#endif

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
# ifndef inline
#  define inline __inline
# endif
#endif

#define lfind _lfind

#ifdef _MSC_VER
#pragma warning(disable : 4996) /* function deprecation warnings */
#endif //_MSC_VER

#endif /* _TIF_CONFIG_H_ */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
