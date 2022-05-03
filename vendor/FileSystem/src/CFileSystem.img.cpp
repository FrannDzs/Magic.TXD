/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.cpp
*  PURPOSE:     IMG R* Games archive management
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>

// Include internal (private) definitions.
#include "fsinternal/CFileSystem.internal.h"
#include "fsinternal/CFileSystem.img.internal.h"

#include "fsinternal/CFileSystem.img.serialize.hxx"

extern CFileSystem *fileSystem;

#include "CFileSystem.Utils.hxx"


void imgExtension::Initialize( CFileSystemNative *sys )
{
    return;
}

void imgExtension::Shutdown( CFileSystemNative *sys )
{
    return;
}

template <eir::CharacterType charType>
inline const charType* GetReadWriteMode( bool isNew )
{
    if constexpr ( std::same_as <charType, char> )
    {
        return ( isNew ? "wb" : "rb+" );
    }
    else if constexpr ( std::same_as <charType, wchar_t> )
    {
        return ( isNew ? L"wb" : L"rb+" );
    }
    else if constexpr ( std::same_as <charType, char8_t> )
    {
        return ( isNew ? u8"wb" : u8"rb+" );
    }
    else if constexpr ( std::same_as <charType, char16_t> )
    {
        return ( isNew ? u"wb" : u"rb+" );
    }
    else if constexpr ( std::same_as <charType, char32_t> )
    {
        return ( isNew ? U"wb" : U"rb+" );
    }
}

static AINLINE CIMGArchiveTranslator* _regularIMGConstructor( imgExtension *env, CFile *registryFile, CFile *contentFile, eIMGArchiveVersion version, bool isLiveMode )
{
    return new CIMGArchiveTranslator( *env, contentFile, registryFile, version, isLiveMode );
}

CIMGArchiveTranslatorHandle* imgExtension::NewArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion version, bool isLiveMode )
{
    return _regularIMGConstructor( this, registryFile, contentFile, version, isLiveMode );
}

CIMGArchiveTranslatorHandle* CFileSystem::CreateIMGArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion version, bool isLiveMode )
{
    if ( imgExtension *env = imgExtension::Get( this ) )
    {
        return env->NewArchiveDirect( contentFile, registryFile, version, isLiveMode );
    }

    return nullptr;
}

template <typename charType>
inline CFile* OpenSeperateIMGRegistryFile( CFileTranslator *srcRoot, const charType *imgFilePath, bool isNew )
{
    CFile *registryFile = nullptr;

    filePath dirOfArchive;
    filePath extention;

    filePath nameItem = FileSystem::GetFileNameItem <FileSysCommonAllocator> ( imgFilePath, false, &dirOfArchive, &extention );

    if ( nameItem.size() != 0 )
    {
        filePath regFilePath = dirOfArchive + nameItem + ".DIR";

        // Open a seperate registry file.
        registryFile = srcRoot->Open( regFilePath, GetReadWriteMode <wchar_t> ( isNew ), FILE_FLAG_WRITESHARE );
    }

    return registryFile;
}

template <typename charType, typename handlerType, typename... extraParams>
static AINLINE CIMGArchiveTranslatorHandle* GenNewArchiveTemplate( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version, handlerType handler, extraParams... theParams )
{
    // Create an archive depending on version.
    CFile *contentFile = nullptr;
    CFile *registryFile = nullptr;

    if ( version == IMG_VERSION_1 )
    {
        // Just open the content file.
        contentFile = srcRoot->Open( srcPath, GetReadWriteMode <charType> ( true ), FILE_FLAG_WRITESHARE );

        // We need to create a seperate registry file.
        registryFile = OpenSeperateIMGRegistryFile( srcRoot, srcPath, true );
    }
    else if ( version == IMG_VERSION_2 )
    {
        // Just create a content file.
        contentFile = srcRoot->Open( srcPath, GetReadWriteMode <charType> ( true ), FILE_FLAG_WRITESHARE );

        registryFile = contentFile;
    }

    if ( contentFile == nullptr || registryFile == nullptr )
    {
        if ( contentFile )
        {
            delete contentFile;
        }

        if ( registryFile && registryFile != contentFile )
        {
            delete registryFile;
        }

        return nullptr;
    }

    try
    {
        return handler( env, registryFile, contentFile, version, std::forward <extraParams> ( theParams )... );
    }
    catch( ... )
    {
        // Do not forget to clean up.
        delete contentFile;

        if ( registryFile != contentFile )
        {
            delete registryFile;
        }

        throw;
    }
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenNewArchive( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{
    return GenNewArchiveTemplate( env, srcRoot, srcPath, version, _regularIMGConstructor, isLiveMode );
}

CIMGArchiveTranslatorHandle* imgExtension::NewArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenNewArchive( this, srcRoot, srcPath, version, isLiveMode ); }
CIMGArchiveTranslatorHandle* imgExtension::NewArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenNewArchive( this, srcRoot, srcPath, version, isLiveMode ); }
CIMGArchiveTranslatorHandle* imgExtension::NewArchive( CFileTranslator *srcRoot, const char8_t *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenNewArchive( this, srcRoot, srcPath, version, isLiveMode ); }

template <eir::CharacterType charType>
inline const charType* GetOpenArchiveFileMode( bool writeAccess )
{
    if constexpr ( std::same_as <charType, char> )
    {
        return ( writeAccess ? "rb+" : "rb" );
    }
    else if constexpr ( std::same_as <charType, wchar_t> )
    {
        return ( writeAccess ? L"rb+" : L"rb" );
    }
    else if constexpr ( std::same_as <charType, char8_t> )
    {
        return ( writeAccess ? u8"rb+" : u8"rb" );
    }
    else if constexpr ( std::same_as <charType, char16_t> )
    {
        return ( writeAccess ? u"rb+" : u"rb" );
    }
    else if constexpr ( std::same_as <charType, char32_t> )
    {
        return ( writeAccess ? U"rb+" : U"rb" );
    }
}

template <typename constructionHandler, typename... extraParamsType>
static inline CIMGArchiveTranslatorHandle* GenOpenArchiveDirectTemplate( imgExtension *env, CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, constructionHandler constr, extraParamsType... extraParams )
{
    CIMGArchiveTranslator *translator = constr( env, registryFile, contentFile, imgVersion, std::forward <extraParamsType> ( extraParams )... );

    try
    {
        bool loadingSuccess = translator->ReadArchive();

        if ( loadingSuccess == false )
        {
            delete translator;

            return nullptr;
        }

        return translator;
    }
    catch( ... )
    {
        delete translator;

        throw;
    }
}

CIMGArchiveTranslatorHandle* imgExtension::OpenArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode )
{
    return GenOpenArchiveDirectTemplate( this, contentFile, registryFile, imgVersion, _regularIMGConstructor, isLiveMode );
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenIMGArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode )
{
    if ( imgExtension *env = imgExtension::Get( this ) )
    {
        return env->OpenArchiveDirect( contentFile, registryFile, imgVersion, isLiveMode );
    }

    return nullptr;
}

template <typename charType, typename constructionHandler, typename... extraParamsType>
static inline CIMGArchiveTranslatorHandle* GenOpenArchiveTemplate( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath, bool writeAccess, constructionHandler constr, extraParamsType... extraParams )
{
    bool hasValidArchive = false;
    eIMGArchiveVersion theVersion;

    CFile *contentFile = srcRoot->Open( srcPath, GetOpenArchiveFileMode <charType> ( false ), FILE_FLAG_WRITESHARE );

    if ( !contentFile )
    {
        return nullptr;
    }

    bool hasUniqueRegistryFile = false;
    CFile *registryFile = nullptr;

    // Check for version 2.
    imgMainHeaderVer2 imgHeader;

    bool hasReadMainHeader = contentFile->ReadStruct( imgHeader );

    if ( hasReadMainHeader &&
        imgHeader.checksum[0] == 'V' &&
        imgHeader.checksum[1] == 'E' &&
        imgHeader.checksum[2] == 'R' &&
        imgHeader.checksum[3] == '2' )
    {
        hasValidArchive = true;
        theVersion = IMG_VERSION_2;

        registryFile = contentFile;
    }

    // Reset the seek.
    contentFile->SeekNative( 0, SEEK_SET );

    if ( !hasValidArchive )
    {
        // Check for version 1.
        hasUniqueRegistryFile = true;

        registryFile = OpenSeperateIMGRegistryFile( srcRoot, srcPath, false );

        if ( registryFile )
        {
            hasValidArchive = true;
            theVersion = IMG_VERSION_1;
        }
    }

    if ( !hasValidArchive )
    {
        delete contentFile;

        if ( hasUniqueRegistryFile )
        {
            delete registryFile;
        }

        return nullptr;
    }

    try
    {
        return GenOpenArchiveDirectTemplate( env, contentFile, registryFile, theVersion, constr, std::forward <extraParamsType> ( extraParams )... );
    }
    catch( ... )
    {
        // Need to clean up.
        delete contentFile;

        if ( hasUniqueRegistryFile )
        {
            delete registryFile;
        }

        throw;
    }
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenOpenArchive( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath, bool writeAccess, bool isLiveMode )
{
    return GenOpenArchiveTemplate( env, srcRoot, srcPath, writeAccess, _regularIMGConstructor, isLiveMode );
}

CIMGArchiveTranslatorHandle* imgExtension::OpenArchive( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }
CIMGArchiveTranslatorHandle* imgExtension::OpenArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }
CIMGArchiveTranslatorHandle* imgExtension::OpenArchive( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenOpenIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath, bool writeAccess, bool isLiveMode )
{
    imgExtension *imgExt = imgExtension::Get( sys );

    if ( imgExt )
    {
        return imgExt->OpenArchive( srcRoot, srcPath, writeAccess, isLiveMode );
    }
    return nullptr;
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenIMGArchive( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenIMGArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::OpenIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenIMGArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::OpenIMGArchive( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenIMGArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenCreateIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{
    imgExtension *imgExt = imgExtension::Get( sys );

    if ( imgExt )
    {
        return imgExt->NewArchive( srcRoot, srcPath, version, isLiveMode );
    }
    return nullptr;
}

CIMGArchiveTranslatorHandle* CFileSystem::CreateIMGArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenCreateIMGArchive( this, srcRoot, srcPath, version, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::CreateIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenCreateIMGArchive( this, srcRoot, srcPath, version, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::CreateIMGArchive( CFileTranslator *srcRoot, const char8_t *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenCreateIMGArchive( this, srcRoot, srcPath, version, isLiveMode ); }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif //_MSC_VER

#ifdef FILESYS_ENABLE_LZO

struct CIMGArchiveTranslator_lzo : public CIMGArchiveTranslator
{
    inline CIMGArchiveTranslator_lzo( imgExtension& imgExt, CFile *contentFile, CFile *registryFile, eIMGArchiveVersion theVersion, bool isLiveMode )
        : CIMGArchiveTranslator( imgExt, contentFile, registryFile, theVersion, isLiveMode )
    {
        // Set the compression provider.
        this->SetCompressionHandler( &compression );
    }

    inline ~CIMGArchiveTranslator_lzo( void )
    {
        // We must unset the compression handler.
        this->SetCompressionHandler( nullptr );
    }

    // We need a compressor per translator, so we can compress simultaneously on multiple threads.
    xboxIMGCompression compression;
};

#endif //FILESYS_ENABLE_LZO

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

static AINLINE CIMGArchiveTranslator* _lzoCompressedIMGConstructor( imgExtension *env, CFile *registryFile, CFile *contentFile, eIMGArchiveVersion version, bool isLiveMode )
{
#ifdef FILESYS_ENABLE_LZO
    return new CIMGArchiveTranslator_lzo( *env, contentFile, registryFile, version, isLiveMode );
#else
    throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
#endif
}

static inline CIMGArchiveTranslatorHandle* GenOpenDirectCompressedIMGArchive( CFileSystem *sys, CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode )
{
    CIMGArchiveTranslatorHandle *archiveHandle = nullptr;
    {
        imgExtension *imgExt = imgExtension::Get( sys );

        if ( imgExt )
        {
            // Create a translator specifically with the LZO compression algorithm.
            archiveHandle = GenOpenArchiveDirectTemplate( imgExt, contentFile, registryFile, imgVersion, _lzoCompressedIMGConstructor, isLiveMode );
        }
    }

    return archiveHandle;
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenCompressedIMGArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion imgVersion, bool isLiveMode )
{
    return GenOpenDirectCompressedIMGArchive( this, contentFile, registryFile, imgVersion, isLiveMode );
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenOpenCompressedIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath, bool writeAccess, bool isLiveMode )
{
    CIMGArchiveTranslatorHandle *archiveHandle = nullptr;
    {
        imgExtension *imgExt = imgExtension::Get( sys );

        if ( imgExt )
        {
            // Create a translator specifically with the LZO compression algorithm.
            archiveHandle = GenOpenArchiveTemplate( imgExt, srcRoot, srcPath, writeAccess, _lzoCompressedIMGConstructor, isLiveMode );
        }
    }

    return archiveHandle;
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenCompressedIMGArchive( CFileTranslator *srcRoot, const char *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenCompressedIMGArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::OpenCompressedIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenCompressedIMGArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::OpenCompressedIMGArchive( CFileTranslator *srcRoot, const char8_t *srcPath, bool writeAccess, bool isLiveMode )
{ return GenOpenCompressedIMGArchive( this, srcRoot, srcPath, writeAccess, isLiveMode ); }

CIMGArchiveTranslatorHandle* CFileSystem::CreateCompressedIMGArchiveDirect( CFile *contentFile, CFile *registryFile, eIMGArchiveVersion version, bool isLiveMode )
{
    if ( imgExtension *env = imgExtension::Get( this ) )
    {
        return _lzoCompressedIMGConstructor( env, registryFile, contentFile, version, isLiveMode );
    }

    return nullptr;
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenCreateCompressedIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{
    CIMGArchiveTranslatorHandle *archiveHandle = nullptr;
    {
        imgExtension *imgExt = imgExtension::Get( sys );

        if ( imgExt )
        {
            // Create a translator specifically with the LZO compression algorithm.
            archiveHandle = GenNewArchiveTemplate( imgExt, srcRoot, srcPath, version, _lzoCompressedIMGConstructor, isLiveMode );
        }
    }

    return archiveHandle;
}

CIMGArchiveTranslatorHandle* CFileSystem::CreateCompressedIMGArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenCreateCompressedIMGArchive( this, srcRoot, srcPath, version, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::CreateCompressedIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenCreateCompressedIMGArchive( this, srcRoot, srcPath, version, isLiveMode ); }
CIMGArchiveTranslatorHandle* CFileSystem::CreateCompressedIMGArchive( CFileTranslator *srcRoot, const char8_t *srcPath, eIMGArchiveVersion version, bool isLiveMode )
{ return GenCreateCompressedIMGArchive( this, srcRoot, srcPath, version, isLiveMode ); }

// Sub modules.
#ifdef FILESYS_ENABLE_LZO
extern void InitializeXBOXIMGCompressionEnvironment( const fs_construction_params& params );
extern void ShutdownXBOXIMGCompressionEnvironment( void );
#endif //FILESYS_ENABLE_LZO

fileSystemFactory_t::pluginOffset_t imgExtension::_imgPluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;

void CFileSystemNative::RegisterIMGDriver( const fs_construction_params& params )
{
    imgExtension::_imgPluginOffset =
        _fileSysFactory.RegisterDependantStructPlugin <imgExtension> ( fileSystemFactory_t::ANONYMOUS_PLUGIN_ID );

    // Register sub modules.
#ifdef FILESYS_ENABLE_LZO
    InitializeXBOXIMGCompressionEnvironment( params );
#endif //FILESYS_ENABLE_LZO
}

void CFileSystemNative::UnregisterIMGDriver( void )
{
    // Unregister sub modules.
#ifdef FILESYS_ENABLE_LZO
    ShutdownXBOXIMGCompressionEnvironment();
#endif //FILESYS_ENABLE_LZO

    if ( imgExtension::_imgPluginOffset != fileSystemFactory_t::INVALID_PLUGIN_OFFSET )
    {
        _fileSysFactory.UnregisterPlugin( imgExtension::_imgPluginOffset );
    }
}
