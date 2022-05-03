#ifndef ZPREFIX_H
#define ZPREFIX_H

#define ZLIB_SYMBOLNAME( x ) rwlibz_##x

#define zlibVersion             ZLIB_SYMBOLNAME( zlibVersion )
#define deflate                 ZLIB_SYMBOLNAME( deflate )
#define deflateEnd              ZLIB_SYMBOLNAME( deflateEnd )
#define inflate                 ZLIB_SYMBOLNAME( inflate )
#define inflateEnd              ZLIB_SYMBOLNAME( inflateEnd )
#define deflateSetDirectory     ZLIB_SYMBOLNAME( deflateSetDirectory )
#define deflateCopy             ZLIB_SYMBOLNAME( deflateCopy )
#define deflateReset            ZLIB_SYMBOLNAME( deflateReset )
#define deflateParams           ZLIB_SYMBOLNAME( deflateParams )
#define deflateTune             ZLIB_SYMBOLNAME( deflateTune )
#define deflateBound            ZLIB_SYMBOLNAME( deflateBound )
#define deflatePrime            ZLIB_SYMBOLNAME( deflatePrime )
#define deflateSetHeader        ZLIB_SYMBOLNAME( deflateSetHeader )
#define inflateSetDirectory     ZLIB_SYMBOLNAME( inflateSetDirectory )
#define inflateSync             ZLIB_SYMBOLNAME( inflateSync )
#define inflateCopy             ZLIB_SYMBOLNAME( inflateCopy )
#define inflateReset            ZLIB_SYMBOLNAME( inflateReset )
#define inflatePrime            ZLIB_SYMBOLNAME( inflatePrime )
#define inflateGetHeader        ZLIB_SYMBOLNAME( inflateGetHeader )
#define inflateBack             ZLIB_SYMBOLNAME( inflateBack )
#define inflateBackEnd          ZLIB_SYMBOLNAME( inflateBackEnd )
#define zlibCompileFlags        ZLIB_SYMBOLNAME( zlibCompileFlags )
#define compress                ZLIB_SYMBOLNAME( compress )
#define compress2               ZLIB_SYMBOLNAME( compress2 )
#define compressBound           ZLIB_SYMBOLNAME( compressBound )
#define uncompress              ZLIB_SYMBOLNAME( uncompress )
#define gzopen                  ZLIB_SYMBOLNAME( gzopen )
#define gzdopen                 ZLIB_SYMBOLNAME( gzdopen )
#define gzsetparams             ZLIB_SYMBOLNAME( gzsetparams )
#define gzread                  ZLIB_SYMBOLNAME( gzread )
#define gzwrite                 ZLIB_SYMBOLNAME( gzwrite )
#define gzprintf                ZLIB_SYMBOLNAME( gzprintf )
#define gzputs                  ZLIB_SYMBOLNAME( gzputs )
#define gzgets                  ZLIB_SYMBOLNAME( gzgets )
#define gzputc                  ZLIB_SYMBOLNAME( gzputc )
#define gzgetc_                 ZLIB_SYMBOLNAME( gzgetc_ )
#define gzungetc                ZLIB_SYMBOLNAME( gzungetc )
#define gzflush                 ZLIB_SYMBOLNAME( gzflush )
#define gzseek                  ZLIB_SYMBOLNAME( gzseek )
#define gzrewind                ZLIB_SYMBOLNAME( gzrewind )
#define gztell                  ZLIB_SYMBOLNAME( gztell )
#define gzeof                   ZLIB_SYMBOLNAME( gzeof )
#define gzdirect                ZLIB_SYMBOLNAME( gzdirect )
#define gzclose                 ZLIB_SYMBOLNAME( gzclose )
#define gzerror                 ZLIB_SYMBOLNAME( gzerror )
#define gzclearerr              ZLIB_SYMBOLNAME( gzclearerr )
#define adler32                 ZLIB_SYMBOLNAME( adler32 )
#define adler32_combine         ZLIB_SYMBOLNAME( adler32_combine )
#define crc32                   ZLIB_SYMBOLNAME( crc32 )
#define crc32_combine           ZLIB_SYMBOLNAME( crc32_combine )
#define crc32_combine64         ZLIB_SYMBOLNAME( crc32_combine64 )
#define crc32_z                 ZLIB_SYMBOLNAME( crc32_z )
#define crc32_z_ex              ZLIB_SYMBOLNAME( crc32_z_ex )
#define deflateInit_            ZLIB_SYMBOLNAME( deflateInit_ )
#define inflateInit_            ZLIB_SYMBOLNAME( inflateInit_ )
#define deflateInit2_           ZLIB_SYMBOLNAME( deflateInit2_ )
#define inflateInit2_           ZLIB_SYMBOLNAME( inflateInit2_ )
#define inflateBackInit_        ZLIB_SYMBOLNAME( inflateBackInit_ )
#define zError                  ZLIB_SYMBOLNAME( zError )
#define inflateSyncPoint        ZLIB_SYMBOLNAME( inflateSyncPoint )
#define get_crc_table           ZLIB_SYMBOLNAME( get_crc_table )

#endif //ZPREFIX_H