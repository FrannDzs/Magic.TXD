/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/fsinternal/CFileSystem.translator.widewrap.cpp
*  PURPOSE:     File Translator wrapper for ANSI-only filesystems.
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

// Include the internal things.
#include "CFileSystem.internal.h"

bool CFileTranslatorWideWrap::CreateDir( const wchar_t *path, filesysPathOperatingMode opMode, bool createParentDirs )
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return CreateDir( widePath.c_str(), opMode, createParentDirs );
}

CFile* CFileTranslatorWideWrap::Open( const wchar_t *path, const filesysOpenMode& mode, eFileOpenFlags flags )
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Open( widePath.c_str(), mode, flags );
}

bool CFileTranslatorWideWrap::Exists( const wchar_t *path ) const
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Exists( widePath.c_str() );
}

bool CFileTranslatorWideWrap::Delete( const wchar_t *path, filesysPathOperatingMode opMode )
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Delete( widePath.c_str(), opMode );
}

bool CFileTranslatorWideWrap::Copy( const wchar_t *src, const wchar_t *dst )
{
    filePath wideSrc( src );
    wideSrc.transform_to <char> ();

    filePath wideDst( dst );
    wideDst.transform_to <char> ();

    return Copy( wideSrc.c_str(), wideDst.c_str() );
}

bool CFileTranslatorWideWrap::Rename( const wchar_t *src, const wchar_t *dst )
{
    filePath wideSrc( src );
    wideSrc.transform_to <char> ();

    filePath wideDst( dst );
    wideDst.transform_to <char> ();

    return Rename( wideSrc.c_str(), wideDst.c_str() );
}

size_t CFileTranslatorWideWrap::Size( const wchar_t *path ) const
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Size( widePath.c_str() );
}

bool CFileTranslatorWideWrap::QueryStats( const wchar_t *path, filesysStats& statsOut ) const
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return QueryStats( widePath.c_str(), statsOut );
}

void CFileTranslatorWideWrap::ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return ScanDirectory( wideDir.c_str(), wideWildcard.c_str(), recurse, dirCallback, fileCallback, userdata );
}

void CFileTranslatorWideWrap::GetDirectories( const wchar_t *directory, const wchar_t *wildcard, bool recurse, dirNames& output ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return GetDirectories( wideDir.c_str(), wideWildcard.c_str(), recurse, output );
}

void CFileTranslatorWideWrap::GetFiles( const wchar_t *directory, const wchar_t *wildcard, bool recurse, dirNames& output ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return GetFiles( wideDir.c_str(), wideWildcard.c_str(), recurse, output );
}

CDirectoryIterator* CFileTranslatorWideWrap::BeginDirectoryListing( const wchar_t *directory, const wchar_t *wildcard, const scanFilteringFlags& filter_flags ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return BeginDirectoryListing( wideDir.c_str(), wideWildcard.c_str(), filter_flags );
}

// UTF-8 support.
bool CFileTranslatorWideWrap::CreateDir( const char8_t *path, filesysPathOperatingMode opMode, bool createParentDirs )
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return CreateDir( widePath.c_str(), opMode, createParentDirs );
}

CFile* CFileTranslatorWideWrap::Open( const char8_t *path, const filesysOpenMode& mode, eFileOpenFlags flags )
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Open( widePath.c_str(), mode, flags );
}

bool CFileTranslatorWideWrap::Exists( const char8_t *path ) const
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Exists( widePath.c_str() );
}

bool CFileTranslatorWideWrap::Delete( const char8_t *path, filesysPathOperatingMode opMode )
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Delete( widePath.c_str(), opMode );
}

bool CFileTranslatorWideWrap::Copy( const char8_t *src, const char8_t *dst )
{
    filePath wideSrc( src );
    wideSrc.transform_to <char> ();

    filePath wideDst( dst );
    wideDst.transform_to <char> ();

    return Copy( wideSrc.c_str(), wideDst.c_str() );
}

bool CFileTranslatorWideWrap::Rename( const char8_t *src, const char8_t *dst )
{
    filePath wideSrc( src );
    wideSrc.transform_to <char> ();

    filePath wideDst( dst );
    wideDst.transform_to <char> ();

    return Rename( wideSrc.c_str(), wideDst.c_str() );
}

size_t CFileTranslatorWideWrap::Size( const char8_t *path ) const
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return Size( widePath.c_str() );
}

bool CFileTranslatorWideWrap::QueryStats( const char8_t *path, filesysStats& statsOut ) const
{
    filePath widePath( path );
    widePath.transform_to <char> ();

    return QueryStats( widePath.c_str(), statsOut );
}

void CFileTranslatorWideWrap::ScanDirectory( const char8_t *directory, const char8_t *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return ScanDirectory( wideDir.c_str(), wideWildcard.c_str(), recurse, dirCallback, fileCallback, userdata );
}

void CFileTranslatorWideWrap::GetDirectories( const char8_t *directory, const char8_t *wildcard, bool recurse, dirNames& output ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return GetDirectories( wideDir.c_str(), wideWildcard.c_str(), recurse, output );
}

void CFileTranslatorWideWrap::GetFiles( const char8_t *directory, const char8_t *wildcard, bool recurse, dirNames& output ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return GetFiles( wideDir.c_str(), wideWildcard.c_str(), recurse, output );
}

CDirectoryIterator* CFileTranslatorWideWrap::BeginDirectoryListing( const char8_t *directory, const char8_t *wildcard, const scanFilteringFlags& filter_flags ) const
{
    filePath wideDir( directory );
    wideDir.transform_to <char> ();

    filePath wideWildcard( wildcard );
    wideWildcard.transform_to <char> ();

    return BeginDirectoryListing( wideDir.c_str(), wideWildcard.c_str(), filter_flags );
}