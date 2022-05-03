/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.platform.h
*  PURPOSE:     Platform native include header file, for platform features
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_PLATFORM_INCLUDE_
#define _FILESYSTEM_PLATFORM_INCLUDE_

#ifdef __linux__
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#endif //__linux__

#include "../CFileSystem.Utils.hxx"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#pragma warning(disable: 4996)
#endif //_WIN32

/*===================================================
    CSystemCapabilities

    This class determines the system-dependant capabilities
    and exports methods that return them to the runtime.

    It does not depend on a properly initialized CFileSystem
    environment, but CFileSystem depends on it.

    Through-out the application runtime, this class stays
    immutable.
===================================================*/
class CSystemCapabilities
{
private:

#ifdef _WIN32
    struct diskInformation
    {
        // Immutable information of a hardware data storage.
        // Modelled on the basis of a hard disk.
        DWORD sectorsPerCluster;
        DWORD bytesPerSector;
        DWORD totalNumberOfClusters;
    };
#endif //_WIN32

public:
    CSystemCapabilities( void )
    {

    }

    inline size_t GetFailSafeSectorSize( void )
    {
        return 2048;
    }

    // Only applicable to raw-files.
    size_t GetSystemLocationSectorSize( char driveLetter )
    {
#ifdef _WIN32
        char systemDriveLocatorBuf[4];

        // systemLocation is assumed to be an absolute path to hardware.
        // Whether the input makes sense ultimatively lies on the shoulders of the callee.
        systemDriveLocatorBuf[0] = driveLetter;
        systemDriveLocatorBuf[1] = ':';
        systemDriveLocatorBuf[2] = '/';
        systemDriveLocatorBuf[3] = 0;

        // Attempt to get disk information.
        diskInformation diskInfo;
        DWORD freeSpace;

        BOOL success = GetDiskFreeSpaceA(
            systemDriveLocatorBuf,
            &diskInfo.sectorsPerCluster,
            &diskInfo.bytesPerSector,
            &freeSpace,
            &diskInfo.totalNumberOfClusters
        );

        // If the retrieval fails, we return a good value for anything.
        if ( success == FALSE )
            return GetFailSafeSectorSize();

        return diskInfo.bytesPerSector;
#elif defined(__linux__)
        // 2048 is always a good solution :)
        return GetFailSafeSectorSize();
#endif //PLATFORM DEPENDENT CODE
    }
};

// Export it from the main class file.
extern CSystemCapabilities systemCapabilities;

// *** PLATFORM HELPERS

template <typename ucpType>
AINLINE bool _File_IsCorrectDriveLetter( ucpType codepoint )
{
    switch( codepoint )
    {
    // Drive letters
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        return true;
    default:
        break;
    }

    return false;
}

// Common utilities.
template <typename charType>
inline bool _File_IsAbsolutePath( const charType *path, const charType*& pathAfterAbsolutorOut )
{
    try
    {
        typedef character_env <charType> char_env;

        // Need to iterate through.
        character_env_iterator_tozero <charType> path_iter( path );

        typename char_env::ucp_t codepoint = path_iter.ResolveAndIncrement();

#ifdef _WIN32
        if ( _File_IsCorrectDriveLetter( codepoint ) )
        {
            // Since we know that we use the to-zero iterator, we are safe to assume
            // that we can iterate until we get a 0.
            auto next_iter = path_iter;

            typename char_env::ucp_t doubleDotSep = next_iter.ResolveAndIncrement();

            // TODO: unify the definition of the slash sep.

            if ( doubleDotSep == ':' && next_iter.IsEnd() == false )
            {
                path_iter = std::move( next_iter );
                goto doSkipTheDesc;
            }
        }
#elif defined(__linux__)
        switch( codepoint )
        {
        case '/':
        case '\\':
            goto doSkipTheDesc;
        }
#endif //OS DEPENDANT CODE

        return false;

    doSkipTheDesc:
        pathAfterAbsolutorOut = path_iter.GetPointer();
        return true;
    }
    catch( eir::codepoint_exception& )
    {
        // Some horrible encoding error occurred.
        return false;
    }
}

#ifdef _WIN32

template <typename charType>
inline bool _File_IsUNCTerminator( charType c )
{
    // This function expects a UCP value as input.

    return ( c == '\\' );
}

template <typename charType>
AINLINE const charType* _File_ParseDirectoryToken( const charType *path, bool& hasNameOut, bool& hasTermSlashOut, const charType*& afterTermOut )
{
    character_env_iterator_tozero <charType> iter( path );

    const charType *offNameEnd = path;

    // Then we need a name of some sort. Every character is allowed.
    bool hasName = false;
    bool gotTerminator = false;

    while ( !iter.IsEnd() )
    {
        typename character_env <charType>::ucp_t curChar = iter.ResolveAndIncrement();

        if ( FileSystem::IsDirectorySeparator( curChar ) )
        {
            gotTerminator = true;
            break;
        }

        // We found some name character.
        hasName = true;

        // Since we count this character as UNC, we can advance.
        offNameEnd = iter.GetPointer();
    }

    hasNameOut = hasName;
    hasTermSlashOut = gotTerminator;
    afterTermOut = iter.GetPointer();

    return offNameEnd;
}

template <typename charType>
inline bool _File_IsUNCPath( const charType *path, const charType*& uncEndOut, filePath& uncOut )
{
    typedef character_env <charType> char_env;

    character_env_iterator_tozero <charType> iter( path );

    // We must start with two backslashes.
    if ( _File_IsUNCTerminator( iter.ResolveAndIncrement() ) && _File_IsUNCTerminator( iter.ResolveAndIncrement() ) )
    {
        bool hasName, gotUNCTerminator;

        const charType *uncStart = iter.GetPointer();
        const charType *uncAfterEnd;
        const charType *uncEnd = _File_ParseDirectoryToken( uncStart, hasName, gotUNCTerminator, uncAfterEnd );

        // Verify that it is properly terminated and stuff.
        if ( hasName && gotUNCTerminator )
        {
            // We got a valid UNC path. Output the UNC name into us.
            uncOut = filePath( uncStart, ( uncEnd - uncStart ) );

            // Tell the runtime where the UNC path ended.
            uncEndOut = uncAfterEnd;

            // Yay!
            return true;
        }
    }

    return false;
}

template <typename charType>
AINLINE bool _File_IsExtendedPath( const charType *path, const charType*& afterExtendedOut )
{
    character_env_iterator_tozero <charType> iter( path );

    // We can simply do this without IsEnd checking because we have a tozero iterator.
    if ( iter.ResolveAndIncrement() == '\\' &&
         iter.ResolveAndIncrement() == '\\' &&
         iter.ResolveAndIncrement() == '?' &&
         iter.ResolveAndIncrement() == '\\' )
    {
        afterExtendedOut = iter.GetPointer();
        return true;
    }

    return false;
}

#endif //_WIN32

// *** IMPLEMENTATION OF PATH CHARACTER CHECKING

#ifdef _WIN32

struct pathcheck_win32
{
    template <typename ucpType>
    AINLINE bool IsInvalidCharacter( ucpType ucp ) const
    {
        switch( ucp )
        {
        case ':':
        case '<':
        case '>':
        case '"':
        case '|':
        case '?':
        case '*':
            return true;
        }

        return false;
    }
};

typedef pathcheck_win32 platform_pathCheckerType;

#elif defined(__linux__)

struct pathcheck_linux
{
    template <typename ucpType>
    AINLINE bool IsInvalidCharacter( ucpType ucp ) const
    {
        switch( ucp )
        {
        case ':':
            return true;
        }

        return false;
    }
};

typedef pathcheck_linux platform_pathCheckerType;

#else
#error No implementation for platform pathCheckerType
#endif //CROSS PLATFORM CODE

// *** IMPLEMENTATION OF TRANSLATOR ROOT PATHS

#ifdef _WIN32

enum class eRootPathType
{
    UNKNOWN,
    UNC,
    DISK
};

struct platform_rootPathType
{
    template <typename charType>
    AINLINE bool buildFromSystemPath( const charType *systemPath, bool allowFile )
    {
        // Try fetching extended path.
        {
            const charType *afterExtended;

            if ( _File_IsExtendedPath( systemPath, afterExtended ) )
            {
                character_env_iterator_tozero <charType> iter( afterExtended );

                auto firstChar = iter.ResolveAndIncrement();

                if ( firstChar != 0 )
                {
                    auto secondChar = iter.ResolveAndIncrement();

                    if ( _File_IsCorrectDriveLetter( firstChar ) && secondChar == ':' )
                    {
                        // Skip a root slash.
                        if ( FileSystem::IsDirectorySeparator( iter.Resolve() ) )
                        {
                            iter.Increment();
                        }

                        normalNodePath nodePath = _File_NormalizeRelativePath( iter.GetPointer(), pathcheck_win32(), allowFile );

                        if ( nodePath.backCount > 0 )
                        {
                            return false;
                        }

                        this->pathType = eRootPathType::DISK;
                        this->driveLetter = firstChar;
                        
                        this->rootNodes = std::move( nodePath.travelNodes );
                        this->isFilePath = nodePath.isFilePath;
                        this->isExtendedPath = true;

                        return true;
                    }

                    // Check for "UNC" descriptor.
                    const auto& classic_locale = std::locale::classic();

                    toupper_lookup <decltype(firstChar)> lookup( classic_locale );
                    toupper_lookup <char> char_lookup( classic_locale );

                    if ( IsCharacterEqualEx( firstChar, 'U', lookup, char_lookup, false ) &&
                         IsCharacterEqualEx( secondChar, 'N', lookup, char_lookup, false ) &&
                         IsCharacterEqualEx( iter.ResolveAndIncrement(), 'C', lookup, char_lookup, false ) &&
                         FileSystem::IsDirectorySeparator( iter.ResolveAndIncrement() ) )
                    {
                        bool gotUNCName, gotUNCTerm;

                        const charType *uncStart = iter.GetPointer();
                        const charType *uncAfterEnd;
                        const charType *uncEnd = _File_ParseDirectoryToken( uncStart, gotUNCName, gotUNCTerm, uncAfterEnd );

                        if ( gotUNCName && gotUNCTerm )
                        {
                            normalNodePath nodePath = _File_NormalizeRelativePath( uncAfterEnd, pathcheck_win32(), allowFile );

                            if ( nodePath.backCount > 0 )
                            {
                                return false;
                            }

                            this->pathType = eRootPathType::UNC;
                            this->unc = filePath( uncStart, uncEnd - uncStart );

                            this->rootNodes = std::move( nodePath.travelNodes );
                            this->isFilePath = nodePath.isFilePath;
                            this->isExtendedPath = true;

                            return true;
                        }
                    }
                }

                // Not a valid extended path.
                return false;
            }
        }

        // Try fetching the root descriptor as well as the directory nodes.
        const charType *uncEnd;
        filePath uncPart;

        const charType *pathAfterAbsolutor;

        if ( _File_IsAbsolutePath( systemPath, pathAfterAbsolutor ) )
        {
            normalNodePath nodePath = _File_NormalizeRelativePath( pathAfterAbsolutor, pathcheck_win32(), allowFile );

            if ( nodePath.backCount > 0 )
                return false;

            this->pathType = eRootPathType::DISK;
            this->driveLetter = character_env <charType>::ResolveStreamCodepoint( 1, systemPath );

            this->rootNodes = std::move( nodePath.travelNodes );
            this->isFilePath = nodePath.isFilePath;
            this->isExtendedPath = false;

            return true;
        }
        else if ( _File_IsUNCPath( systemPath, uncEnd, uncPart ) )
        {
            normalNodePath nodePath = _File_NormalizeRelativePath( uncEnd, pathcheck_win32(), allowFile );

            if ( nodePath.backCount > 0 )
                return false;

            // TODO: actually the slash direction should be reversed here from the default, to '\'.
            //  but we currently have no way to specify the slash direction for the translator.
            //  fix this.

            this->pathType = eRootPathType::UNC;
            this->unc = std::move( uncPart );

            this->rootNodes = std::move( nodePath.travelNodes );
            this->isFilePath = nodePath.isFilePath;
            this->isExtendedPath = false;

            return true;
        }

        return false;
    }

    AINLINE void buildFromParams( const platform_rootPathType& rootDesc, dirNames&& nodes, bool isFilePath )
    {
        this->pathType = rootDesc.pathType;
        this->driveLetter = rootDesc.driveLetter;
        this->unc = rootDesc.unc;
        this->isExtendedPath = rootDesc.isExtendedPath;

        this->rootNodes = std::move( nodes );
        this->isFilePath = isFilePath;
    }

    AINLINE bool doesRootDescriptorMatch( const platform_rootPathType& right ) const
    {
        eRootPathType leftPathType = this->pathType;
        eRootPathType rightPathType = right.pathType;

        if ( leftPathType != rightPathType )
            return false;

        // We actually match no matter if we are extended notation or not.

        if ( leftPathType == eRootPathType::DISK )
        {
            return ( UniversalCompareStrings( &this->driveLetter, 1, &right.driveLetter, 1, false ) );
        }
        else if ( leftPathType == eRootPathType::UNC )
        {
            return ( this->unc.equals( right.unc, false ) );
        }
        else
        {
            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
        }
    }

private:
    AINLINE filePath RootDescriptorCustom( bool isExtendedPath ) const
    {
        eRootPathType pathType = this->pathType;

        filePath desc;

        if ( isExtendedPath )
        {
            desc += FileSystem::GetDirectorySeparator <char> ( false );
            desc += FileSystem::GetDirectorySeparator <char> ( false );
            desc += "?";
            desc += FileSystem::GetDirectorySeparator <char> ( false );
        }

        if ( pathType == eRootPathType::DISK )
        {
            desc += this->driveLetter;
            desc += ':';
            desc += FileSystem::GetDirectorySeparator <char> ( isExtendedPath == false );
        }
        else if ( pathType == eRootPathType::UNC )
        {
            if ( isExtendedPath )
            {
                desc += "UNC";
            }
            else
            {
                desc += FileSystem::GetDirectorySeparator <char> ( false );
            }

            desc += FileSystem::GetDirectorySeparator <char> ( false );
            desc += this->unc;
            desc += FileSystem::GetDirectorySeparator <char> ( false );
        }
        else
        {
            throw FileSystem::filesystem_exception( FileSystem::eGenExceptCode::INTERNAL_ERROR );
        }

        return desc;
    }

public:
    AINLINE filePath RootDescriptor( void ) const
    {
        return RootDescriptorCustom( this->isExtendedPath );
    }

    // Special version of RootDescriptor that returns fullpaths according to the \\?\ prefix (Win32 only).
    AINLINE filePath RootDescriptorExtended( bool shouldBeExtended ) const
    {
        return RootDescriptorCustom( this->isExtendedPath || shouldBeExtended );
    }

    AINLINE dirNames& RootNodes( void )                 { return this->rootNodes; }
    AINLINE const dirNames& RootNodes( void ) const     { return this->rootNodes; }
    AINLINE bool IsFilePath( void ) const               { return this->isFilePath; }

private:
    AINLINE bool DecideSlashDirectionCustom( bool isExtendedPath ) const
    {
        // True means forward, false means backward.

        if ( isExtendedPath )
        {
            return false;
        }

        if ( this->pathType == eRootPathType::UNC )
        {
            return false;
        }

        return true;
    }

public:
    AINLINE bool DecideSlashDirection( void ) const
    {
        return DecideSlashDirectionCustom( this->isExtendedPath );
    }

    AINLINE bool DecideSlashDirectionExtended( bool shouldBeExtended ) const
    {
        return DecideSlashDirectionCustom( this->isExtendedPath || shouldBeExtended );
    }

    dirNames        rootNodes;
    bool            isFilePath;
    bool            isExtendedPath;
    eRootPathType   pathType = eRootPathType::UNKNOWN;
    char32_t        driveLetter;    // valid if pathType == eRootPathType::DISK
    filePath        unc;            // valid if pathType == eRootPathType::UNC
};

#elif defined(__linux__)

struct platform_rootPathType
{
    template <typename charType>
    AINLINE bool buildFromSystemPath( const charType *systemPath, bool allowFile )
    {
        const charType *pathAfterAbsolutor;

        if ( _File_IsAbsolutePath( systemPath, pathAfterAbsolutor ) )
        {
            normalNodePath normalPath = _File_NormalizeRelativePath( pathAfterAbsolutor, pathcheck_linux() );

            if ( normalPath.backCount > 0 )
                return false;

            this->rootNodes = std::move( normalPath.travelNodes );
            this->isFilePath = normalPath.isFilePath;

            return true;
        }

        return false;
    }

    AINLINE void buildFromParams( const platform_rootPathType& rootDesc, dirNames&& nodes, bool isFilePath )
    {
        this->rootNodes = std::move( nodes );
        this->isFilePath = isFilePath;
    }

    AINLINE bool doesRootDescriptorMatch( const platform_rootPathType& right ) const
    {
        // There are no special root descriptors so do not worry.
        return true;
    }
    AINLINE bool IsFilePath( void ) const
    {
        return this->isFilePath;
    }
    AINLINE filePath RootDescriptor( void ) const
    {
        // Meow, pretty simple.
        return "/";
    }
    AINLINE bool DecideSlashDirection( void ) const
    {
        // Always the forward slash.
        return true;
    }

    AINLINE dirNames& RootNodes( void )                 { return this->rootNodes; }
    AINLINE const dirNames& RootNodes( void ) const     { return this->rootNodes; }

    dirNames        rootNodes;
    bool            isFilePath;
};

#else
#error No implementation for the rootPath type of CSystemFileTranslator
#endif //CROSS PLATFORM CODE

#endif //_FILESYSTEM_PLATFORM_INCLUDE_
