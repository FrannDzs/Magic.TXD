/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/fsinternal/CFileSystem.translator.widewrap.h
*  PURPOSE:     File Translator wrapper for ANSI-only filesystems.
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_TRANSLATOR_UNICODE_WRAPPER_
#define _FILESYSTEM_TRANSLATOR_UNICODE_WRAPPER_

struct CFileTranslatorWideWrap : virtual public CFileTranslator
{
    using CFileTranslator::CreateDir;
    using CFileTranslator::Open;
    using CFileTranslator::Exists;
    using CFileTranslator::Delete;
    using CFileTranslator::Copy;
    using CFileTranslator::Rename;
    using CFileTranslator::Size;
    using CFileTranslator::QueryStats;

    using CFileTranslator::ScanDirectory;
    using CFileTranslator::GetDirectories;
    using CFileTranslator::GetFiles;
    using CFileTranslator::BeginDirectoryListing;

    // We implement all unicode methods here and redirect them to ANSI methods.
    bool            CreateDir( const wchar_t *path, filesysPathOperatingMode opMode, bool createParentDirs ) override;
    CFile*          Open( const wchar_t *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override;
    bool            Exists( const wchar_t *path ) const override;
    bool            Delete( const wchar_t *path, filesysPathOperatingMode opMode ) override;
    bool            Copy( const wchar_t *src, const wchar_t *dst ) override;
    bool            Rename( const wchar_t *src, const wchar_t *dst ) override;
    size_t          Size( const wchar_t *path ) const override;
    bool            QueryStats( const wchar_t *path, filesysStats& statsOut ) const override;
    
    void            ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                                   pathCallback_t dirCallback,
                                   pathCallback_t fileCallback,
                                   void *userdata ) const override;
    void            GetDirectories( const wchar_t *directory, const wchar_t *wildcard, bool recurse, dirNames& output ) const override;
    void            GetFiles( const wchar_t *directory, const wchar_t *wildcard, bool recurse, dirNames& output ) const override;
    CDirectoryIterator* BeginDirectoryListing( const wchar_t *directory, const wchar_t *wildcard, const scanFilteringFlags& filter_flags ) const override;

    // UTF8 support.
    bool            CreateDir( const char8_t *path, filesysPathOperatingMode opMode, bool createParentDirs ) override;
    CFile*          Open( const char8_t *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override;
    bool            Exists( const char8_t *path ) const override;
    bool            Delete( const char8_t *path, filesysPathOperatingMode opMode ) override;
    bool            Copy( const char8_t *src, const char8_t *dst ) override;
    bool            Rename( const char8_t *src, const char8_t *dst ) override;
    size_t          Size( const char8_t *path ) const override;
    bool            QueryStats( const char8_t *path, filesysStats& statsOut ) const override;
    
    void            ScanDirectory( const char8_t *directory, const char8_t *wildcard, bool recurse,
                                   pathCallback_t dirCallback,
                                   pathCallback_t fileCallback,
                                   void *userdata ) const override;
    void            GetDirectories( const char8_t *directory, const char8_t *wildcard, bool recurse, dirNames& output ) const override;
    void            GetFiles( const char8_t *directory, const char8_t *wildcard, bool recurse, dirNames& output ) const override;
    CDirectoryIterator* BeginDirectoryListing( const char8_t *directory, const char8_t *wildcard, const scanFilteringFlags& filter_flags ) const override;
};

#endif //_FILESYSTEM_TRANSLATOR_UNICODE_WRAPPER_