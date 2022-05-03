/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.config.cpp
*  PURPOSE:     Classes to write binary condiguration to disc
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

// TODO: finish this implementation.

// We need internal stuff.
#include "CFileSystem.internal.h"

// The header of every block.
struct blockHeader
{
    char fourcc[4];
    fsUInt_t blockSize;
};

ConfigContainer::ConfigContainer( CFile *stream )
{
    this->underlyingStream = stream;
    this->parentBlock = nullptr;

    this->EnterContext();
}

ConfigContainer::ConfigContainer( ConfigContainer *parent )
{
    this->underlyingStream = nullptr;
    this->parentBlock = parent;

    this->EnterContext();
}

ConfigContainer::~ConfigContainer( void )
{
    this->LeaveContext();
}

void ConfigContainer::EnterContext( void )
{

}

void ConfigContainer::LeaveContext( void )
{

}

void ConfigContainer::write( const void *buf, size_t writecount )
{

}

void ConfigContainer::read( void *buf, size_t readcount )
{

}

fsOffsetNumber_t ConfigContainer::tell( void ) const
{
    return 0;
}

void ConfigContainer::seek( fsOffsetNumber_t offset, eSeekMode mode )
{

}