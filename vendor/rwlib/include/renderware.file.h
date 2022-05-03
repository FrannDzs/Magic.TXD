/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.file.h
*  PURPOSE:     File stream interface header
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include <sdk/StringUtils.h>

namespace rw
{

// File interface for custom file pipelining.
struct FileInterface abstract
{
    typedef void* filePtr_t;

    virtual filePtr_t       OpenStream          ( const char *streamPath, const char *streamMode ) = 0;
    virtual void            CloseStream         ( filePtr_t ptr ) = 0;

    virtual filePtr_t       OpenStreamW         ( const wchar_t *streamPath, const wchar_t *streamMode ) = 0;

    virtual size_t          ReadStream          ( filePtr_t ptr, void *outBuf, size_t readCount ) = 0;
    virtual size_t          WriteStream         ( filePtr_t ptr, const void *outBuf, size_t writeCount ) = 0;

    virtual bool            SeekStream          ( filePtr_t ptr, long streamOffset, int type ) = 0;
    virtual long            TellStream          ( filePtr_t ptr ) = 0;

    virtual bool            IsEOFStream         ( filePtr_t ptr ) = 0;

    virtual long            SizeStream          ( filePtr_t ptr ) = 0;

    virtual void            FlushStream         ( filePtr_t ptr ) = 0;
};

// Translator interface for basing file activity somewhere.
struct FileTranslator abstract
{
    typedef rwStaticSet <rwStaticString <wchar_t>, lexical_string_comparator <true>> fileList_t;

    virtual bool            GetBasedDirectory   ( const wchar_t *streamPath, rwStaticString <wchar_t>& fullPathOut ) const = 0;
    virtual fileList_t      GetDirectoryListing ( const wchar_t *listingDir, const wchar_t *pattern, bool recursive ) const = 0;
};

// Ability to set the current directory translator.
void SetDataRepositoryTranslator( Interface *engineInterface, FileTranslator *translator );

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif //_MSC_VER

// filePtr_t to std::streambuf wrapper.
struct fileStreamBuf : public std::streambuf
{
    fileStreamBuf( FileInterface *fileIntf, FileInterface::filePtr_t fptr )
    {
        this->fileInterface = fileIntf;
        this->fptr = fptr;
    }

protected:
    FileInterface *fileInterface;
    FileInterface::filePtr_t fptr;

    std::streampos seekoff( std::streamoff offset, std::ios_base::seekdir way, std::ios_base::openmode openmode )
    {
        // We don't care about openmode.

        int offsetANSI;

        if ( way == std::ios_base::beg )
        {
            offsetANSI = SEEK_SET;
        }
        else if ( way == std::ios_base::cur )
        {
            offsetANSI = SEEK_CUR;
        }
        else if ( way == std::ios_base::end )
        {
            offsetANSI = SEEK_END;
        }
        else
        {
            assert( 0 );
        }

        long streamOffsetANSI = (long)offset;

        int seekSuccess = this->fileInterface->SeekStream( this->fptr, streamOffsetANSI, offsetANSI );

        if ( seekSuccess == -1 )
            return -1;

        return this->fileInterface->TellStream( this->fptr );
    }

    std::streampos seekpos( std::streamoff offset, std::ios_base::openmode openmode )
    {
        long streamOffsetANSI = (long)offset;

        int seekSuccess = this->fileInterface->SeekStream( this->fptr, streamOffsetANSI, SEEK_SET );

        if ( seekSuccess == -1 )
            return -1;

        return this->fileInterface->TellStream( this->fptr );
    }

    int sync( void )
    {
        this->fileInterface->FlushStream( this->fptr );

        return 0;
    }

    std::streamsize showmanyc( void )
    {
        long curPos = this->fileInterface->TellStream( this->fptr );
        long fileSize = this->fileInterface->SizeStream( this->fptr );

        return (std::streamsize)( std::max( (long)0, fileSize - curPos ) );
    }

    std::streamsize xsgetn( char *outBuffer, std::streamsize n )
    {
        if ( n < 0 )
            return -1;

        size_t readCount = this->fileInterface->ReadStream( this->fptr, outBuffer, (long)n );

        return (std::streamsize)readCount;
    }

    std::streamsize xsputn( const char *inputBuffer, std::streamsize n )
    {
        if ( n < 0 )
            return -1;

        size_t writeCount = this->fileInterface->WriteStream( this->fptr, inputBuffer, (long)n );

        return (std::streamsize)writeCount;
    }

    int overflow( int c )
    {
        size_t writeCount = this->fileInterface->WriteStream( this->fptr, &c, 1 );

        if ( writeCount == 0 )
            return EOF;

        this->fileInterface->SeekStream( this->fptr, -(long)writeCount, SEEK_CUR );

        return c;
    }

    int uflow( void )
    {
        unsigned char theChar;

        size_t readCount = this->fileInterface->ReadStream( this->fptr, &theChar, 1 );

        if ( readCount == 0 )
            return EOF;

        return theChar;
    }

    int underflow( void )
    {
        unsigned char theChar;

        size_t readCount = this->fileInterface->ReadStream( this->fptr, &theChar, 1 );

        if ( readCount == 0 )
            return EOF;

        this->fileInterface->SeekStream( this->fptr, -(long)readCount, SEEK_CUR );

        return theChar;
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

} // namespace rw