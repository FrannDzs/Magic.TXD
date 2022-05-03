/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/include/CFileSystem.common.stl.h
*  PURPOSE:     Definitions for interoperability with the STL library
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_STL_COMPAT_
#define _FILESYSTEM_STL_COMPAT_

// This include is actually a bad idea because the header already includes this file.
// We do this anyway because this header is sometimes included stand-alone.
#include "CFileSystemInterface.h"

#include <string>
#include <algorithm>

namespace FileSystem
{
    // CFile to std::streambuf wrapper.
    struct fileStreamBuf : public std::streambuf
    {
        fileStreamBuf( CFile *theStream )
        {
            this->underlyingStream = theStream;
        }

    protected:
        std::streampos seekoff( std::streamoff offset, std::ios_base::seekdir way, std::ios_base::openmode openmode ) override
        {
            fsOffsetNumber_t baseOff;

            CFile *underlyingStream = this->underlyingStream;

            if ( way == std::ios_base::beg )
            {
                baseOff = 0;
            }
            else if ( way == std::ios_base::cur )
            {
                baseOff = underlyingStream->TellNative();
            }
            else if ( way == std::ios_base::end )
            {
                baseOff = underlyingStream->GetSizeNative();
            }
            else
            {
                throw filesystem_exception( eGenExceptCode::INVALID_PARAM );
            }

            fsOffsetNumber_t streamOffsetANSI = ( baseOff + (fsOffsetNumber_t)offset );

            underlyingStream->SeekNative( streamOffsetANSI, SEEK_SET );

            // The documentation of this is pretty weird... Why would you say that the default documentation returns something but it aint good?
            return std::streampos( streamOffsetANSI );
        }

        std::streampos seekpos( std::streampos offset, std::ios_base::openmode openmode ) override
        {
            if ( offset < 0 )
            {
                return std::streampos( -1 );
            }
            
            // Basically an absolute offset.
            fsOffsetNumber_t streamOffsetANSI = (fsOffsetNumber_t)offset;

            this->underlyingStream->SeekNative( streamOffsetANSI, SEEK_SET );

            // We have to return the new position into the stream.
            return std::streampos( streamOffsetANSI );
        }

        int sync( void ) override
        {
            // For the heck of it flush the buffers.
            this->underlyingStream->Flush();

            return 0;
        }

        std::streamsize showmanyc( void ) override
        {
            CFile *underlyingStream = this->underlyingStream;

            fsOffsetNumber_t curPos = ( underlyingStream->TellNative() );
            fsOffsetNumber_t fileSize = underlyingStream->GetSizeNative();

            return (std::streamsize)( std::max( (fsOffsetNumber_t)0, fileSize - curPos ) );
        }

    private:
        // Need to keep the file ptr same sometimes.
        struct guard
        {
            inline guard( CFile *underlyingStream )
            {
                this->underlyingStream = underlyingStream;
                this->stored_pos = underlyingStream->TellNative();
            }

            inline ~guard( void )
            {
                // Need to reset pointer.
                underlyingStream->SeekNative( this->stored_pos, SEEK_SET );
            }
           
        private:
            CFile *underlyingStream;
            fsOffsetNumber_t stored_pos;
        };

    protected:
        std::streamsize xsgetn( char *outBuffer, std::streamsize n ) override
        {
            if ( n < 0 )
                return -1;

            size_t realSize = (size_t)n;

            if ( realSize > std::numeric_limits <size_t>::max() )
                return -1;

            // We read from the real input offset.
            CFile *underlyingStream = this->underlyingStream;

            size_t readCount = underlyingStream->Read( outBuffer, realSize );

            return (std::streamsize)readCount;
        }

        std::streamsize xsputn( const char *inputBuffer, std::streamsize n ) override
        {
            if ( n < 0 )
                return -1;

            size_t realSize = (size_t)n;

            if ( realSize > std::numeric_limits <size_t>::max() )
                return -1;

            // Now with the write offset.
            CFile *underlyingStream = this->underlyingStream;

            size_t writeCount = underlyingStream->Write( inputBuffer, realSize );

            return (std::streamsize)writeCount;
        }

        int overflow( int c_wide ) override
        {
            char narrow_c = (char)c_wide;

            // Gotta write at the real write offset, but not remember it.
            CFile *underlyingStream = this->underlyingStream;

            // I don't give a fuck.
            //guard write_guard( underlyingStream );

            size_t writeCount = underlyingStream->Write( &narrow_c, 1 );

            if ( writeCount == 0 )
                return EOF;

            return narrow_c;
        }

        int uflow( void ) override
        {
            unsigned char theChar;

            // Seek to the real input offset, and do remember it!
            CFile *underlyingStream = this->underlyingStream;

            size_t readCount = underlyingStream->Read( &theChar, 1 );

            if ( readCount == 0 )
                return EOF;

            return theChar;
        }

        int underflow( void ) override
        {
            unsigned char theChar;

            // Read without remembering.
            CFile *underlyingStream = this->underlyingStream;

            // I don't give a fuck.
            //guard read_guard( underlyingStream );

            size_t readCount = underlyingStream->Read( &theChar, 1 );

            if ( readCount == 0 )
                return EOF;

            return theChar;
        }

    private:
        CFile *underlyingStream;
    };
};

#endif //_FILESYSTEM_STL_COMPAT_