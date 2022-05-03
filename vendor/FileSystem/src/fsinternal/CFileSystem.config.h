/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.config.h
*  PURPOSE:     Classes to write binary condiguration to disc
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_CONFIGURATION_PROVIDER_
#define _FILESYSTEM_CONFIGURATION_PROVIDER_

// Recursive block format to serialize binary versioned configuration into a stream.
struct ConfigContainer
{
    enum eSeekMode
    {
        CSEEK_BEG,
        CSEEK_CUR,
        CSEEK_END
    };

    ConfigContainer( CFile *stream );
    ConfigContainer( ConfigContainer *parent );
    ~ConfigContainer( void );

    // We enter the context by issuing the constructor.
    void write( const void *buf, size_t bufsize );
    void read( void *dstbuf, size_t readcount );

    fsOffsetNumber_t tell( void ) const;
    void seek( fsOffsetNumber_t offset, eSeekMode mode );

private:
    void EnterContext( void );
    void LeaveContext( void );

    CFile *underlyingStream;
    ConfigContainer *parentBlock;
};

#endif //_FILESYSTEM_CONFIGURATION_PROVIDER_