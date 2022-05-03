/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.system.h
*  PURPOSE:     FileSystem translator that represents directory links
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_TRANSLATOR_SYSTEM_
#define _FILESYSTEM_TRANSLATOR_SYSTEM_

#include "CFileSystem.platform.h"

class CSystemFileTranslator : public CSystemPathTranslator <platform_pathCheckerType, platform_rootPathType>
{
public:
    // We need to distinguish between Windows and other OSes here.
    // Windows uses driveletters (C:/) and Unix + Mac do not.
    // This way, on Windows we can use the '/' or '\\' character
    // to trace paths from the translator root.
    // On Linux and Mac, these characters are for addressing
    // absolute paths.
    CSystemFileTranslator( platform_rootPathType rootPath ) :
        CSystemPathTranslator(
            std::move( rootPath ),
#ifdef _WIN32
            false, false
#else
            true, true
#endif //OS DEPENDANT CODE
        )
    {
    }

    ~CSystemFileTranslator( void );

private:
    template <typename charType>
    bool            GenCreateDir                    ( const charType *path, filesysPathOperatingMode opMode, bool doCreateParentDirs );
    template <typename charType>
    CFile*          GenOpen                         ( const charType *path, const filesysOpenMode& mode, eFileOpenFlags flags );
    template <typename charType>
    bool            GenExists                       ( const charType *path ) const;
    template <typename charType>
    bool            GenDelete                       ( const charType *path, filesysPathOperatingMode opMode );
    template <typename charType>
    bool            GenCopy                         ( const charType *src, const charType *dst );
    template <typename charType>
    bool            GenRename                       ( const charType *src, const charType *dst );
    template <typename charType>
    size_t          GenSize                         ( const charType *path ) const;
    template <typename charType>
    bool            GenQueryStats                   ( const charType *path, filesysStats& statsOut ) const;

public:
    bool            CreateDir                       ( const char *path, filesysPathOperatingMode opMode, bool createParentDirs ) override;
    CFile*          Open                            ( const char *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override;
    bool            Exists                          ( const char *path ) const override;
    bool            Delete                          ( const char *path, filesysPathOperatingMode opMode ) override;
    bool            Copy                            ( const char *src, const char *dst ) override;
    bool            Rename                          ( const char *src, const char *dst ) override;
    size_t          Size                            ( const char *path ) const override;
    bool            QueryStats                      ( const char *path, filesysStats& statsOut ) const override;

    bool            CreateDir                       ( const wchar_t *path, filesysPathOperatingMode opMode, bool createParentDirs ) override;
    CFile*          Open                            ( const wchar_t *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override;
    bool            Exists                          ( const wchar_t *path ) const override;
    bool            Delete                          ( const wchar_t *path, filesysPathOperatingMode opMode ) override;
    bool            Copy                            ( const wchar_t *src, const wchar_t *dst ) override;
    bool            Rename                          ( const wchar_t *src, const wchar_t *dst ) override;
    size_t          Size                            ( const wchar_t *path ) const override;
    bool            QueryStats                      ( const wchar_t *path, filesysStats& statsOut ) const override;

    bool            CreateDir                       ( const char8_t *path, filesysPathOperatingMode opMode, bool createParentDirs ) override;
    CFile*          Open                            ( const char8_t *path, const filesysOpenMode& mode, eFileOpenFlags flags ) override;
    bool            Exists                          ( const char8_t *path ) const override;
    bool            Delete                          ( const char8_t *path, filesysPathOperatingMode opMode ) override;
    bool            Copy                            ( const char8_t *src, const char8_t *dst ) override;
    bool            Rename                          ( const char8_t *src, const char8_t *dst ) override;
    size_t          Size                            ( const char8_t *path ) const override;
    bool            QueryStats                      ( const char8_t *path, filesysStats& statsOut ) const override;

protected:
    bool            OnConfirmDirectoryChange        ( const translatorPathResult& path ) override final;

private:
    // Helper.
    template <typename charType>
    AINLINE bool    DirPathTransformQualifiedInputPath( const charType *path, bool& slashDirOut, filePath& pathOut ) const;

    template <typename charType>
    void            GenScanDirectory( const charType *directory, const charType *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const;

public:
    void            ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const override;
    void            ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const override;
    void            ScanDirectory( const char8_t *directory, const char8_t *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const override;

private:
    template <typename charType>
    void            GenGetDirectories               ( const charType *path, const charType *wildcard, bool recurse, dirNames& output ) const;
    template <typename charType>
    void            GenGetFiles                     ( const charType *path, const charType *wildcard, bool recurse, dirNames& output ) const;

public:
    void            GetDirectories                  ( const char *path, const char *wildcard, bool recurse, dirNames& output ) const override;
    void            GetFiles                        ( const char *path, const char *wildcard, bool recurse, dirNames& output ) const override;

    void            GetDirectories                  ( const wchar_t *path, const wchar_t *wildcard, bool recurse, dirNames& output ) const override;
    void            GetFiles                        ( const wchar_t *path, const wchar_t *wildcard, bool recurse, dirNames& output ) const override;

    void            GetDirectories                  ( const char8_t *path, const char8_t *wildcard, bool recurse, dirNames& output ) const override;
    void            GetFiles                        ( const char8_t *path, const char8_t *wildcard, bool recurse, dirNames& output ) const override;

private:
    template <typename charType>
    CDirectoryIterator* GenBeginDirectoryListing    ( const charType *path, const charType *wildcard, const scanFilteringFlags& flags ) const;

public:
    // Iterator-based directory listing.
    CDirectoryIterator* BeginDirectoryListing       ( const char *path, const char *wildcard, const scanFilteringFlags& filter_flags ) const override;
    CDirectoryIterator* BeginDirectoryListing       ( const wchar_t *path, const wchar_t *wildcard, const scanFilteringFlags& filter_flags ) const override;
    CDirectoryIterator* BeginDirectoryListing       ( const char8_t *path, const char8_t *wildcard, const scanFilteringFlags& filter_flags ) const override;

private:
    friend class CFileSystem;
    friend struct CFileSystemNative;

    template <typename charType>
    bool            ParseSystemPath                 ( const charType *path, bool allowFile, translatorPathResult& transPathOut ) const;
    filePath        GetFullRootDirPath              ( const translatorPathResult& path, bool& slashDirOut ) const;

    void            CheckForFaultyTranslator        ( void ) const;

    bool            _CreateDirTree                  ( const translatorPathResult& dirPath, bool shouldRemoveFileAtEnd, bool doCreateParentDirs, bool doCreateActualDir );

#ifdef _WIN32
    HANDLE          m_rootHandle;
    HANDLE          m_curDirHandle;
#elif defined(__linux__)
    DIR*            m_rootHandle;
    DIR*            m_curDirHandle;
#endif //OS DEPENDANT CODE
};

// Important raw system exports.
bool File_IsDirectoryAbsolute( const char *pPath );

#endif //_FILESYSTEM_TRANSLATOR_SYSTEM_
